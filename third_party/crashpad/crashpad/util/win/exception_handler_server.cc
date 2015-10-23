// Copyright 2015 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/win/exception_handler_server.h"

#include <string.h>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/crashpad_info_client_options.h"
#include "snapshot/win/process_snapshot_win.h"
#include "util/file/file_writer.h"
#include "util/misc/tri_state.h"
#include "util/misc/uuid.h"
#include "util/win/registration_protocol_win.h"

namespace crashpad {

namespace {

decltype(GetNamedPipeClientProcessId)* GetNamedPipeClientProcessIdFunction() {
  static decltype(GetNamedPipeClientProcessId)* func =
      reinterpret_cast<decltype(GetNamedPipeClientProcessId)*>(GetProcAddress(
          GetModuleHandle(L"kernel32.dll"), "GetNamedPipeClientProcessId"));
  return func;
}

HANDLE DuplicateEvent(HANDLE process, HANDLE event) {
  HANDLE handle;
  if (DuplicateHandle(GetCurrentProcess(),
                      event,
                      process,
                      &handle,
                      SYNCHRONIZE | EVENT_MODIFY_STATE,
                      false,
                      0)) {
    return handle;
  }
  return nullptr;
}

}  // namespace

namespace internal {

//! \brief Context information for the named pipe handler threads.
class PipeServiceContext {
 public:
  PipeServiceContext(HANDLE port,
                     HANDLE pipe,
                     ExceptionHandlerServer::Delegate* delegate,
                     base::Lock* clients_lock,
                     std::set<internal::ClientData*>* clients,
                     uint64_t shutdown_token)
      : port_(port),
        pipe_(pipe),
        delegate_(delegate),
        clients_lock_(clients_lock),
        clients_(clients),
        shutdown_token_(shutdown_token) {}

  HANDLE port() const { return port_; }
  HANDLE pipe() const { return pipe_.get(); }
  ExceptionHandlerServer::Delegate* delegate() const { return delegate_; }
  base::Lock* clients_lock() const { return clients_lock_; }
  std::set<internal::ClientData*>* clients() const { return clients_; }
  uint64_t shutdown_token() const { return shutdown_token_; }

 private:
  HANDLE port_;  // weak
  ScopedKernelHANDLE pipe_;
  ExceptionHandlerServer::Delegate* delegate_;  // weak
  base::Lock* clients_lock_;  // weak
  std::set<internal::ClientData*>* clients_;  // weak
  uint64_t shutdown_token_;

  DISALLOW_COPY_AND_ASSIGN(PipeServiceContext);
};

//! \brief The context data for registered threadpool waits.
//!
//! This object must be created and destroyed on the main thread. Access must be
//! guarded by use of the lock() with the exception of the threadpool wait
//! variables which are accessed only by the main thread.
class ClientData {
 public:
  ClientData(HANDLE port,
             ExceptionHandlerServer::Delegate* delegate,
             ScopedKernelHANDLE process,
             WinVMAddress exception_information_address,
             WAITORTIMERCALLBACK dump_request_callback,
             WAITORTIMERCALLBACK process_end_callback)
      : dump_request_thread_pool_wait_(INVALID_HANDLE_VALUE),
        process_end_thread_pool_wait_(INVALID_HANDLE_VALUE),
        lock_(),
        port_(port),
        delegate_(delegate),
        dump_requested_event_(
            CreateEvent(nullptr, false /* auto reset */, false, nullptr)),
        process_(process.Pass()),
        exception_information_address_(exception_information_address) {
    RegisterThreadPoolWaits(dump_request_callback, process_end_callback);
  }

  ~ClientData() {
    // It is important that this only access the threadpool waits (it's called
    // from the main thread) until the waits are unregistered, to ensure that
    // any outstanding callbacks are complete.
    UnregisterThreadPoolWaits();
  }

  base::Lock* lock() { return &lock_; }
  HANDLE port() const { return port_; }
  ExceptionHandlerServer::Delegate* delegate() const { return delegate_; }
  HANDLE dump_requested_event() const { return dump_requested_event_.get(); }
  WinVMAddress exception_information_address() const {
    return exception_information_address_;
  }
  HANDLE process() const { return process_.get(); }

 private:
  void RegisterThreadPoolWaits(WAITORTIMERCALLBACK dump_request_callback,
                               WAITORTIMERCALLBACK process_end_callback) {
    if (!RegisterWaitForSingleObject(&dump_request_thread_pool_wait_,
                                     dump_requested_event_.get(),
                                     dump_request_callback,
                                     this,
                                     INFINITE,
                                     WT_EXECUTEDEFAULT)) {
      LOG(ERROR) << "RegisterWaitForSingleObject dump requested";
    }

    if (!RegisterWaitForSingleObject(&process_end_thread_pool_wait_,
                                     process_.get(),
                                     process_end_callback,
                                     this,
                                     INFINITE,
                                     WT_EXECUTEONLYONCE)) {
      LOG(ERROR) << "RegisterWaitForSingleObject process end";
    }
  }

  // This blocks until outstanding calls complete so that we know it's safe to
  // delete this object. Because of this, it must be executed on the main
  // thread, not a threadpool thread.
  void UnregisterThreadPoolWaits() {
    UnregisterWaitEx(dump_request_thread_pool_wait_, INVALID_HANDLE_VALUE);
    dump_request_thread_pool_wait_ = INVALID_HANDLE_VALUE;
    UnregisterWaitEx(process_end_thread_pool_wait_, INVALID_HANDLE_VALUE);
    process_end_thread_pool_wait_ = INVALID_HANDLE_VALUE;
  }

  // These are only accessed on the main thread.
  HANDLE dump_request_thread_pool_wait_;
  HANDLE process_end_thread_pool_wait_;

  base::Lock lock_;
  // Access to these fields must be guarded by lock_.
  HANDLE port_;  // weak
  ExceptionHandlerServer::Delegate* delegate_;  // weak
  ScopedKernelHANDLE dump_requested_event_;
  ScopedKernelHANDLE process_;
  WinVMAddress exception_information_address_;

  DISALLOW_COPY_AND_ASSIGN(ClientData);
};

}  // namespace internal

ExceptionHandlerServer::Delegate::~Delegate() {
}

ExceptionHandlerServer::ExceptionHandlerServer()
    : port_(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1)),
      clients_lock_(),
      clients_() {
}

ExceptionHandlerServer::~ExceptionHandlerServer() {
}

void ExceptionHandlerServer::Run(Delegate* delegate,
                                 const std::string& pipe_name) {
  uint64_t shutdown_token = base::RandUint64();
  // We create two pipe instances, so that there's one listening while the
  // PipeServiceProc is processing a registration.
  ScopedKernelHANDLE thread_handles[2];
  base::string16 pipe_name_16(base::UTF8ToUTF16(pipe_name));
  for (int i = 0; i < arraysize(thread_handles); ++i) {
    HANDLE pipe =
        CreateNamedPipe(pipe_name_16.c_str(),
                        PIPE_ACCESS_DUPLEX,
                        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                        arraysize(thread_handles),
                        512,
                        512,
                        0,
                        nullptr);

    // Ownership of this object (and the pipe instance) is given to the new
    // thread. We close the thread handles at the end of the scope. They clean
    // up the context object and the pipe instance on termination.
    internal::PipeServiceContext* context =
        new internal::PipeServiceContext(port_.get(),
                                         pipe,
                                         delegate,
                                         &clients_lock_,
                                         &clients_,
                                         shutdown_token);
    thread_handles[i].reset(
        CreateThread(nullptr, 0, &PipeServiceProc, context, 0, nullptr));
  }

  delegate->ExceptionHandlerServerStarted();

  // This is the main loop of the server. Most work is done on the threadpool,
  // other than process end handling which is posted back to this main thread,
  // as we must unregister the threadpool waits here.
  for (;;) {
    OVERLAPPED* ov = nullptr;
    ULONG_PTR key = 0;
    DWORD bytes = 0;
    GetQueuedCompletionStatus(port_.get(), &bytes, &key, &ov, INFINITE);
    if (!key) {
      // Shutting down.
      break;
    }

    // Otherwise, this is a request to unregister and destroy the given client.
    // delete'ing the ClientData blocks in UnregisterWaitEx to ensure all
    // outstanding threadpool waits are complete. This is important because the
    // process handle can be signalled *before* the dump request is signalled.
    internal::ClientData* client = reinterpret_cast<internal::ClientData*>(key);
    {
      base::AutoLock lock(clients_lock_);
      clients_.erase(client);
    }
    delete client;
  }

  // Signal to the named pipe instances that they should terminate.
  for (int i = 0; i < arraysize(thread_handles); ++i) {
    ClientToServerMessage message;
    memset(&message, 0, sizeof(message));
    message.type = ClientToServerMessage::kShutdown;
    message.shutdown.token = shutdown_token;
    ServerToClientMessage response;
    SendToCrashHandlerServer(base::UTF8ToUTF16(pipe_name),
                             reinterpret_cast<ClientToServerMessage&>(message),
                             &response);
  }

  for (auto& handle : thread_handles)
    WaitForSingleObject(handle.get(), INFINITE);

  // Deleting ClientData does a blocking wait until the threadpool executions
  // have terminated when unregistering them.
  {
    base::AutoLock lock(clients_lock_);
    for (auto* client : clients_)
      delete client;
    clients_.clear();
  }
}

void ExceptionHandlerServer::Stop() {
  // Post a null key (third argument) to trigger shutdown.
  PostQueuedCompletionStatus(port_.get(), 0, 0, nullptr);
}

// This function must be called with service_context.pipe() already connected to
// a client pipe. It exchanges data with the client and adds a ClientData record
// to service_context->clients().
//
// static
bool ExceptionHandlerServer::ServiceClientConnection(
    const internal::PipeServiceContext& service_context) {
  ClientToServerMessage message;

  if (!LoggingReadFile(service_context.pipe(), &message, sizeof(message)))
    return false;

  switch (message.type) {
    case ClientToServerMessage::kShutdown: {
      if (message.shutdown.token != service_context.shutdown_token()) {
        LOG(ERROR) << "forged shutdown request, got: "
                   << message.shutdown.token;
        return false;
      }
      ServerToClientMessage shutdown_response = {0};
      LoggingWriteFile(service_context.pipe(),
                       &shutdown_response,
                       sizeof(shutdown_response));
      return true;
    }

    case ClientToServerMessage::kRegister:
      // Handled below.
      break;

    default:
      LOG(ERROR) << "unhandled message type: " << message.type;
      return false;
  }

  if (message.registration.version != RegistrationRequest::kMessageVersion) {
    LOG(ERROR) << "unexpected version. got: " << message.registration.version
               << " expecting: " << RegistrationRequest::kMessageVersion;
    return false;
  }

  decltype(GetNamedPipeClientProcessId)* get_named_pipe_client_process_id =
      GetNamedPipeClientProcessIdFunction();
  if (get_named_pipe_client_process_id) {
    // GetNamedPipeClientProcessId is only available on Vista+.
    DWORD real_pid = 0;
    if (get_named_pipe_client_process_id(service_context.pipe(), &real_pid) &&
        message.registration.client_process_id != real_pid) {
      LOG(ERROR) << "forged client pid, real pid: " << real_pid
                 << ", got: " << message.registration.client_process_id;
      return false;
    }
  }

  // We attempt to open the process as us. This is the main case that should
  // almost always succeed as the server will generally be more privileged. If
  // we're running as a different user, it may be that we will fail to open
  // the process, but the client will be able to, so we make a second attempt
  // having impersonated the client.
  HANDLE client_process = OpenProcess(
      PROCESS_ALL_ACCESS, false, message.registration.client_process_id);
  if (!client_process) {
    if (!ImpersonateNamedPipeClient(service_context.pipe())) {
      PLOG(ERROR) << "ImpersonateNamedPipeClient";
      return false;
    }
    HANDLE client_process = OpenProcess(
        PROCESS_ALL_ACCESS, false, message.registration.client_process_id);
    PCHECK(RevertToSelf());
    if (!client_process) {
      LOG(ERROR) << "failed to open " << message.registration.client_process_id;
      return false;
    }
  }

  internal::ClientData* client;
  {
    base::AutoLock lock(*service_context.clients_lock());
    client =
        new internal::ClientData(service_context.port(),
                                 service_context.delegate(),
                                 ScopedKernelHANDLE(client_process),
                                 message.registration.exception_information,
                                 &OnDumpEvent,
                                 &OnProcessEnd);
    service_context.clients()->insert(client);
  }

  // Duplicate the events back to the client so they can request a dump.
  ServerToClientMessage response;
  response.registration.request_report_event = reinterpret_cast<uint32_t>(
      DuplicateEvent(client->process(), client->dump_requested_event()));

  if (!LoggingWriteFile(service_context.pipe(), &response, sizeof(response)))
    return false;

  return false;
}

// static
DWORD __stdcall ExceptionHandlerServer::PipeServiceProc(void* ctx) {
  internal::PipeServiceContext* service_context =
      reinterpret_cast<internal::PipeServiceContext*>(ctx);
  DCHECK(service_context);

  for (;;) {
    bool ret = ConnectNamedPipe(service_context->pipe(), nullptr);
    if (!ret && GetLastError() != ERROR_PIPE_CONNECTED) {
      PLOG(ERROR) << "ConnectNamedPipe";
    } else if (ServiceClientConnection(*service_context)) {
      break;
    }
    DisconnectNamedPipe(service_context->pipe());
  }

  delete service_context;

  return 0;
}

// static
void __stdcall ExceptionHandlerServer::OnDumpEvent(void* ctx, BOOLEAN) {
  // This function is executed on the thread pool.
  internal::ClientData* client = reinterpret_cast<internal::ClientData*>(ctx);
  base::AutoLock lock(*client->lock());

  // Capture the exception.
  unsigned int exit_code = client->delegate()->ExceptionHandlerServerException(
      client->process(), client->exception_information_address());

  TerminateProcess(client->process(), exit_code);
}

// static
void __stdcall ExceptionHandlerServer::OnProcessEnd(void* ctx, BOOLEAN) {
  // This function is executed on the thread pool.
  internal::ClientData* client = reinterpret_cast<internal::ClientData*>(ctx);
  base::AutoLock lock(*client->lock());

  // Post back to the main thread to have it delete this client record.
  PostQueuedCompletionStatus(client->port(), 0, ULONG_PTR(client), nullptr);
}

}  // namespace crashpad
