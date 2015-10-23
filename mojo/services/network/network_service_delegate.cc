// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/network_service_delegate.h"

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/services/network/cookie_store_impl.h"
#include "mojo/services/network/network_service_delegate_observer.h"
#include "mojo/services/network/network_service_impl.h"
#include "mojo/services/network/url_loader_factory_impl.h"
#include "mojo/services/network/web_socket_factory_impl.h"
#include "mojo/util/capture_util.h"
#include "sql/mojo/mojo_vfs.h"

namespace {

const char kSQLThreadName[] = "SQL_IO_Thread";
const char kUserDataDir[] = "user-data-dir";

// SQL blocks on the filesystem service, so perform all SQL functions on a
// separate thread.
class SQLThread : public base::Thread {
 public:
  SQLThread(filesystem::DirectoryPtr directory)
      : base::Thread(kSQLThreadName),
        directory_info_(directory.PassInterface().Pass()) {
    base::Thread::Options options;
    options.message_pump_factory =
        base::Bind(&mojo::common::MessagePumpMojo::Create);
    StartWithOptions(options);
  }
  ~SQLThread() override { Stop(); }

  void Init() override {
    filesystem::DirectoryPtr directory;
    directory.Bind(directory_info_.Pass());
    vfs_.reset(new sql::ScopedMojoFilesystemVFS(directory.Pass()));
  }

  void CleanUp() override {
    vfs_.reset();
  }

 private:
  // Our VFS which wraps sqlite so that we can reuse the current sqlite code.
  scoped_ptr<sql::ScopedMojoFilesystemVFS> vfs_;

  // This member is used to safely pass data from one thread to another. It is
  // set in the constructor and is consumed in Init().
  mojo::InterfacePtrInfo<filesystem::Directory> directory_info_;

  DISALLOW_COPY_AND_ASSIGN(SQLThread);
};

}  // namespace

namespace mojo {

NetworkServiceDelegate::NetworkServiceDelegate()
    : app_(nullptr),
      binding_(this) {
}

NetworkServiceDelegate::~NetworkServiceDelegate() {
}

void NetworkServiceDelegate::AddObserver(
    NetworkServiceDelegateObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkServiceDelegate::RemoveObserver(
    NetworkServiceDelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkServiceDelegate::Initialize(ApplicationImpl* app) {
  app_ = app;

#if !defined(OS_ANDROID)
  // TODO(erg): The following doesn't work when running the android
  // apptests. It works in the mandoline shell (on desktop and on android), and
  // in the apptests on desktop. However, on android, whenever we make the call
  // to OpenFileSystem, the entire mojo system hangs to the point where writes
  // to stderr that previously would have printed to our console aren't. The
  // apptests are also fairly resistant to being run under gdb on android.
  URLRequestPtr request(URLRequest::New());
  request->url = String::From("mojo:filesystem");
  app_->ConnectToService(request.Pass(), &files_);

  filesystem::FileSystemClientPtr client;
  binding_.Bind(GetProxy(&client));

  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  filesystem::DirectoryPtr directory;
  files_->OpenFileSystem("origin", GetProxy(&directory), client.Pass(),
                         Capture(&error));
  files_.WaitForIncomingResponse();

  io_worker_thread_.reset(new SQLThread(directory.Pass()));
#endif

  // TODO(erg): Find everything else that writes to the filesystem and
  // transition it to proxying mojo:filesystem. We shouldn't have any path
  // calculation code here, but sadly need it until the transition is done. In
  // the mean time, manually handle the user-data-dir switch (which gets set in
  // tests) so that tests are writing to a temp dir.
  base::FilePath base_path;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUserDataDir)) {
    base_path = command_line->GetSwitchValuePath(kUserDataDir);
  } else {
    CHECK(PathService::Get(base::DIR_TEMP, &base_path));
    base_path = base_path.Append(FILE_PATH_LITERAL("network_service"));
  }

  scoped_refptr<base::SequencedTaskRunner> worker_thread;
#if !defined(OS_ANDROID)
  worker_thread = io_worker_thread_->task_runner();
#endif
  context_.reset(new NetworkContext(base_path, worker_thread, this));
}

bool NetworkServiceDelegate::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  DCHECK(context_);
  connection->AddService<CookieStore>(this);
  connection->AddService<NetworkService>(this);
  connection->AddService<URLLoaderFactory>(this);
  connection->AddService<WebSocketFactory>(this);
  return true;
}

bool NetworkServiceDelegate::OnShellConnectionError() {
  EnsureIOThreadShutdown();
  return true;
}

void NetworkServiceDelegate::Quit() {
  EnsureIOThreadShutdown();

  // Destroy the NetworkContext now as it requires MessageLoop::current() upon
  // destruction and it is the last moment we know for sure that it is
  // running.
  context_.reset();
}

void NetworkServiceDelegate::Create(ApplicationConnection* connection,
                                    InterfaceRequest<NetworkService> request) {
  new NetworkServiceImpl(app_->app_lifetime_helper()->CreateAppRefCount(),
                         request.Pass());
}

void NetworkServiceDelegate::Create(ApplicationConnection* connection,
                                    InterfaceRequest<CookieStore> request) {
  new CookieStoreImpl(context_.get(),
                      GURL(connection->GetRemoteApplicationURL()).GetOrigin(),
                      app_->app_lifetime_helper()->CreateAppRefCount(),
                      request.Pass());
}

void NetworkServiceDelegate::Create(
    ApplicationConnection* connection,
    InterfaceRequest<WebSocketFactory> request) {
  new WebSocketFactoryImpl(context_.get(),
                           app_->app_lifetime_helper()->CreateAppRefCount(),
                           request.Pass());
}

void NetworkServiceDelegate::Create(
    ApplicationConnection* connection,
    InterfaceRequest<URLLoaderFactory> request) {
  new URLLoaderFactoryImpl(context_.get(),
                           app_->app_lifetime_helper()->CreateAppRefCount(),
                           request.Pass());
}

void NetworkServiceDelegate::OnFileSystemShutdown() {
  EnsureIOThreadShutdown();
}

void NetworkServiceDelegate::EnsureIOThreadShutdown() {
  if (io_worker_thread_) {
    // Broadcast to the entire system that we have to shut down anything
    // depending on the worker thread. Either we're shutting down or the
    // filesystem service is shutting down.
    FOR_EACH_OBSERVER(NetworkServiceDelegateObserver, observers_,
                      OnIOWorkerThreadShutdown());

    // Destroy the io worker thread here so that we can commit any pending
    // cookies here.
    io_worker_thread_.reset();
  }
}

}  // namespace mojo
