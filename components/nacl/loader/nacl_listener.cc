// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_listener.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_renderer_messages.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/loader/nacl_ipc_adapter.h"
#include "components/nacl/loader/nacl_validation_db.h"
#include "components/nacl/loader/nacl_validation_query.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "mojo/nacl/mojo_syscall.h"
#include "native_client/src/public/chrome_main.h"
#include "native_client/src/public/nacl_app.h"
#include "native_client/src/public/nacl_desc.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/edk/embedder/platform_support.h"
#include "third_party/mojo/src/mojo/edk/embedder/simple_platform_support.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(OS_LINUX)
#include "content/public/common/child_process_sandbox_support_linux.h"
#endif

#if defined(OS_WIN)
#include <io.h>

#include "content/public/common/sandbox_init.h"
#endif

namespace {

NaClListener* g_listener;

void FatalLogHandler(const char* data, size_t bytes) {
  // We use uint32_t rather than size_t for the case when the browser and NaCl
  // processes are a mix of 32-bit and 64-bit processes.
  uint32_t copy_bytes = std::min<uint32_t>(static_cast<uint32_t>(bytes),
                                           nacl::kNaClCrashInfoMaxLogSize);

  // We copy the length of the crash data to the start of the shared memory
  // segment so we know how much to copy.
  memcpy(g_listener->crash_info_shmem_memory(), &copy_bytes, sizeof(uint32_t));

  memcpy((char*)g_listener->crash_info_shmem_memory() + sizeof(uint32_t),
         data,
         copy_bytes);
}

#if defined(OS_MACOSX)

// On Mac OS X, shm_open() works in the sandbox but does not give us
// an FD that we can map as PROT_EXEC.  Rather than doing an IPC to
// get an executable SHM region when CreateMemoryObject() is called,
// we preallocate one on startup, since NaCl's sel_ldr only needs one
// of them.  This saves a round trip.

base::subtle::Atomic32 g_shm_fd = -1;

int CreateMemoryObject(size_t size, int executable) {
  if (executable && size > 0) {
    int result_fd = base::subtle::NoBarrier_AtomicExchange(&g_shm_fd, -1);
    if (result_fd != -1) {
      // ftruncate() is disallowed by the Mac OS X sandbox and
      // returns EPERM.  Luckily, we can get the same effect with
      // lseek() + write().
      if (lseek(result_fd, size - 1, SEEK_SET) == -1) {
        LOG(ERROR) << "lseek() failed: " << errno;
        return -1;
      }
      if (write(result_fd, "", 1) != 1) {
        LOG(ERROR) << "write() failed: " << errno;
        return -1;
      }
      return result_fd;
    }
  }
  // Fall back to NaCl's default implementation.
  return -1;
}

#elif defined(OS_LINUX)

int CreateMemoryObject(size_t size, int executable) {
  return content::MakeSharedMemorySegmentViaIPC(size, executable);
}

#elif defined(OS_WIN)
// We wrap the function to convert the bool return value to an int.
int BrokerDuplicateHandle(NaClHandle source_handle,
                          uint32_t process_id,
                          NaClHandle* target_handle,
                          uint32_t desired_access,
                          uint32_t options) {
  return content::BrokerDuplicateHandle(source_handle, process_id,
                                        target_handle, desired_access,
                                        options);
}

int AttachDebugExceptionHandler(const void* info, size_t info_size) {
  std::string info_string(reinterpret_cast<const char*>(info), info_size);
  bool result = false;
  if (!g_listener->Send(new NaClProcessMsg_AttachDebugExceptionHandler(
           info_string, &result)))
    return false;
  return result;
}

void DebugStubPortSelectedHandler(uint16_t port) {
  g_listener->Send(new NaClProcessHostMsg_DebugStubPortSelected(port));
}

#endif

// Creates the PPAPI IPC channel between the NaCl IRT and the host
// (browser/renderer) process, and starts to listen it on the thread where
// the given message_loop_proxy runs.
// Also, creates and sets the corresponding NaClDesc to the given nap with
// the FD #.
scoped_refptr<NaClIPCAdapter> SetUpIPCAdapter(
    IPC::ChannelHandle* handle,
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
    struct NaClApp* nap,
    int nacl_fd) {
  scoped_refptr<NaClIPCAdapter> ipc_adapter(
      new NaClIPCAdapter(*handle, message_loop_proxy.get()));
  ipc_adapter->ConnectChannel();
#if defined(OS_POSIX)
  handle->socket =
      base::FileDescriptor(ipc_adapter->TakeClientFileDescriptor());
#endif

  // Pass a NaClDesc to the untrusted side. This will hold a ref to the
  // NaClIPCAdapter.
  NaClAppSetDesc(nap, nacl_fd, ipc_adapter->MakeNaClDesc());
  return ipc_adapter;
}

}  // namespace

class BrowserValidationDBProxy : public NaClValidationDB {
 public:
  explicit BrowserValidationDBProxy(NaClListener* listener)
      : listener_(listener) {
  }

  bool QueryKnownToValidate(const std::string& signature) override {
    // Initialize to false so that if the Send fails to write to the return
    // value we're safe.  For example if the message is (for some reason)
    // dispatched as an async message the return parameter will not be written.
    bool result = false;
    if (!listener_->Send(new NaClProcessMsg_QueryKnownToValidate(signature,
                                                                 &result))) {
      LOG(ERROR) << "Failed to query NaCl validation cache.";
      result = false;
    }
    return result;
  }

  void SetKnownToValidate(const std::string& signature) override {
    // Caching is optional: NaCl will still work correctly if the IPC fails.
    if (!listener_->Send(new NaClProcessMsg_SetKnownToValidate(signature))) {
      LOG(ERROR) << "Failed to update NaCl validation cache.";
    }
  }

 private:
  // The listener never dies, otherwise this might be a dangling reference.
  NaClListener* listener_;
};


NaClListener::NaClListener() : shutdown_event_(true, false),
                               io_thread_("NaCl_IOThread"),
#if defined(OS_LINUX)
                               prereserved_sandbox_size_(0),
#endif
#if defined(OS_POSIX)
                               number_of_cores_(-1),  // unknown/error
#endif
                               main_loop_(NULL) {
  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  DCHECK(g_listener == NULL);
  g_listener = this;
}

NaClListener::~NaClListener() {
  NOTREACHED();
  shutdown_event_.Signal();
  g_listener = NULL;
}

bool NaClListener::Send(IPC::Message* msg) {
  DCHECK(main_loop_ != NULL);
  if (base::MessageLoop::current() == main_loop_) {
    // This thread owns the channel.
    return channel_->Send(msg);
  } else {
    // This thread does not own the channel.
    return filter_->Send(msg);
  }
}

// The NaClProcessMsg_ResolveFileTokenAsyncReply message must be
// processed in a MessageFilter so it can be handled on the IO thread.
// The main thread used by NaClListener is busy in
// NaClChromeMainAppStart(), so it can't be used for servicing messages.
class FileTokenMessageFilter : public IPC::MessageFilter {
 public:
  bool OnMessageReceived(const IPC::Message& msg) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(FileTokenMessageFilter, msg)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_ResolveFileTokenReply,
                          OnResolveFileTokenReply)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnResolveFileTokenReply(
      uint64_t token_lo,
      uint64_t token_hi,
      IPC::PlatformFileForTransit ipc_fd,
      base::FilePath file_path) {
    CHECK(g_listener);
    g_listener->OnFileTokenResolved(token_lo, token_hi, ipc_fd, file_path);
  }
 private:
  ~FileTokenMessageFilter() override {}
};

void NaClListener::Listen() {
  std::string channel_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID);
  channel_ = IPC::SyncChannel::Create(
      this, io_thread_.message_loop_proxy().get(), &shutdown_event_);
  filter_ = new IPC::SyncMessageFilter(&shutdown_event_);
  channel_->AddFilter(filter_.get());
  channel_->AddFilter(new FileTokenMessageFilter());
  channel_->Init(channel_name, IPC::Channel::MODE_CLIENT, true);
  main_loop_ = base::MessageLoop::current();
  main_loop_->Run();
}

bool NaClListener::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClListener, msg)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_Start, OnStart)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NaClListener::OnStart(const nacl::NaClStartParams& params) {
#if defined(OS_LINUX) || defined(OS_MACOSX)
  int urandom_fd = dup(base::GetUrandomFD());
  if (urandom_fd < 0) {
    LOG(ERROR) << "Failed to dup() the urandom FD";
    return;
  }
  NaClChromeMainSetUrandomFd(urandom_fd);
#endif
  struct NaClApp* nap = NULL;
  NaClChromeMainInit();

  crash_info_shmem_.reset(new base::SharedMemory(params.crash_info_shmem_handle,
                                                 false));
  CHECK(crash_info_shmem_->Map(nacl::kNaClCrashInfoShmemSize));
  NaClSetFatalErrorCallback(&FatalLogHandler);

  nap = NaClAppCreate();
  if (nap == NULL) {
    LOG(ERROR) << "NaClAppCreate() failed";
    return;
  }

  IPC::ChannelHandle browser_handle;
  IPC::ChannelHandle ppapi_renderer_handle;
  IPC::ChannelHandle manifest_service_handle;

  if (params.enable_ipc_proxy) {
    browser_handle = IPC::Channel::GenerateVerifiedChannelID("nacl");
    ppapi_renderer_handle = IPC::Channel::GenerateVerifiedChannelID("nacl");
    manifest_service_handle = IPC::Channel::GenerateVerifiedChannelID("nacl");

    // Create the PPAPI IPC channels between the NaCl IRT and the host
    // (browser/renderer) processes. The IRT uses these channels to
    // communicate with the host and to initialize the IPC dispatchers.
    SetUpIPCAdapter(&browser_handle, io_thread_.message_loop_proxy(),
                    nap, NACL_CHROME_DESC_BASE);
    SetUpIPCAdapter(&ppapi_renderer_handle, io_thread_.message_loop_proxy(),
                    nap, NACL_CHROME_DESC_BASE + 1);

    scoped_refptr<NaClIPCAdapter> manifest_ipc_adapter =
        SetUpIPCAdapter(&manifest_service_handle,
                        io_thread_.message_loop_proxy(),
                        nap,
                        NACL_CHROME_DESC_BASE + 2);
    manifest_ipc_adapter->set_resolve_file_token_callback(
        base::Bind(&NaClListener::ResolveFileToken, base::Unretained(this)));
  }

  trusted_listener_ = new NaClTrustedListener(
      IPC::Channel::GenerateVerifiedChannelID("nacl"),
      io_thread_.message_loop_proxy().get(),
      &shutdown_event_);
  if (!Send(new NaClProcessHostMsg_PpapiChannelsCreated(
          browser_handle,
          ppapi_renderer_handle,
          trusted_listener_->TakeClientChannelHandle(),
          manifest_service_handle)))
    LOG(ERROR) << "Failed to send IPC channel handle to NaClProcessHost.";

  std::vector<nacl::FileDescriptor> handles = params.handles;
  struct NaClChromeMainArgs* args = NaClChromeMainArgsCreate();
  if (args == NULL) {
    LOG(ERROR) << "NaClChromeMainArgsCreate() failed";
    return;
  }

#if defined(OS_LINUX) || defined(OS_MACOSX)
  args->number_of_cores = number_of_cores_;
  args->create_memory_object_func = CreateMemoryObject;
# if defined(OS_MACOSX)
  CHECK(handles.size() >= 1);
  g_shm_fd = nacl::ToNativeHandle(handles[handles.size() - 1]);
  handles.pop_back();
# endif
#endif

  DCHECK(params.process_type != nacl::kUnknownNaClProcessType);
  CHECK(handles.size() >= 1);
  NaClHandle irt_handle = nacl::ToNativeHandle(handles[handles.size() - 1]);
  handles.pop_back();

#if defined(OS_WIN)
  args->irt_fd = _open_osfhandle(reinterpret_cast<intptr_t>(irt_handle),
                                 _O_RDONLY | _O_BINARY);
  if (args->irt_fd < 0) {
    LOG(ERROR) << "_open_osfhandle() failed";
    return;
  }
#else
  args->irt_fd = irt_handle;
#endif

  if (params.validation_cache_enabled) {
    // SHA256 block size.
    CHECK_EQ(params.validation_cache_key.length(), (size_t) 64);
    // The cache structure is not freed and exists until the NaCl process exits.
    args->validation_cache = CreateValidationCache(
        new BrowserValidationDBProxy(this), params.validation_cache_key,
        params.version);
  }

  CHECK(handles.size() == 1);
  args->imc_bootstrap_handle = nacl::ToNativeHandle(handles[0]);
  args->enable_debug_stub = params.enable_debug_stub;

  // Now configure parts that depend on process type.
  // Start with stricter settings.
  args->enable_exception_handling = 0;
  args->enable_dyncode_syscalls = 0;
  // pnacl_mode=1 mostly disables things (IRT interfaces and syscalls).
  args->pnacl_mode = 1;
  // Bound the initial nexe's code segment size under PNaCl to reduce the
  // chance of a code spraying attack succeeding (see
  // https://code.google.com/p/nativeclient/issues/detail?id=3572).
  // We can't apply this arbitrary limit outside of PNaCl because it might
  // break existing NaCl apps, and this limit is only useful if the dyncode
  // syscalls are disabled.
  args->initial_nexe_max_code_bytes = 64 << 20;  // 64 MB.

  if (params.process_type == nacl::kNativeNaClProcessType) {
    args->enable_exception_handling = 1;
    args->enable_dyncode_syscalls = 1;
    args->pnacl_mode = 0;
    args->initial_nexe_max_code_bytes = 0;
  } else if (params.process_type == nacl::kPNaClTranslatorProcessType) {
    // Transitioning the PNaCl translators to use the IRT again:
    // https://code.google.com/p/nativeclient/issues/detail?id=3914.
    // Once done, this can be removed.
    args->irt_load_optional = 1;
    args->pnacl_mode = 0;
  }

#if defined(OS_LINUX) || defined(OS_MACOSX)
  args->debug_stub_server_bound_socket_fd = nacl::ToNativeHandle(
      params.debug_stub_server_bound_socket);
#endif
#if defined(OS_WIN)
  args->broker_duplicate_handle_func = BrokerDuplicateHandle;
  args->attach_debug_exception_handler_func = AttachDebugExceptionHandler;
  args->debug_stub_server_port_selected_handler_func =
      DebugStubPortSelectedHandler;
#endif
#if defined(OS_LINUX)
  args->prereserved_sandbox_size = prereserved_sandbox_size_;
#endif

  base::PlatformFile nexe_file = IPC::PlatformFileForTransitToPlatformFile(
      params.nexe_file);
  std::string file_path_str = params.nexe_file_path_metadata.AsUTF8Unsafe();
  args->nexe_desc = NaClDescCreateWithFilePathMetadata(nexe_file,
                                                       file_path_str.c_str());

#if defined(OS_POSIX)
  if (params.enable_mojo) {
#if !defined(OS_MACOSX)
    // Don't call mojo::embedder::Init on Mac; it's already been called from
    // ChromeMain() (see chrome/app/chrome_exe_main_mac.cc).
    mojo::embedder::Init(make_scoped_ptr(
        new mojo::embedder::SimplePlatformSupport()));
#endif
    // InjectMojo adds a file descriptor to the process that allows Mojo calls
    // to use an implementation defined outside the NaCl sandbox. See
    // //mojo/nacl for implementation details.
    InjectMojo(nap);
  } else {
    // When Mojo isn't enabled, we inject a file descriptor that intentionally
    // fails on any imc_sendmsg() call to make debugging easier.
    InjectDisabledMojo(nap);
  }
#else
  InjectDisabledMojo(nap);
#endif
  // TODO(yusukes): Support pre-opening resource files.
  CHECK(params.prefetched_resource_files.empty());

  int exit_status;
  if (!NaClChromeMainStart(nap, args, &exit_status))
    NaClExit(1);

  // Report the plugin's exit status if the application started successfully.
  trusted_listener_->Send(new NaClRendererMsg_ReportExitStatus(exit_status));
  NaClExit(exit_status);
}

void NaClListener::ResolveFileToken(
    uint64_t token_lo,
    uint64_t token_hi,
    base::Callback<void(IPC::PlatformFileForTransit, base::FilePath)> cb) {
  if (!Send(new NaClProcessMsg_ResolveFileToken(token_lo, token_hi))) {
    cb.Run(IPC::PlatformFileForTransit(), base::FilePath());
    return;
  }
  resolved_cb_ = cb;
}

void NaClListener::OnFileTokenResolved(
    uint64_t token_lo,
    uint64_t token_hi,
    IPC::PlatformFileForTransit ipc_fd,
    base::FilePath file_path) {
  resolved_cb_.Run(ipc_fd, file_path);
  resolved_cb_.Reset();
}
