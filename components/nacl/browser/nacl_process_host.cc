// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_process_host.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_iterator.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_browser_delegate.h"
#include "components/nacl/browser/nacl_host_message_filter.h"
#include "components/nacl/common/nacl_cmd_line.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/nacl/common/nacl_switches.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_switches.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "net/base/net_util.h"
#include "net/socket/tcp_listen_socket.h"
#include "ppapi/host/host_factory.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_constants.h"
#include "ppapi/shared_impl/ppapi_nacl_plugin_args.h"

#if defined(OS_POSIX)

#include <fcntl.h>

#include "ipc/ipc_channel_posix.h"
#elif defined(OS_WIN)
#include <windows.h>

#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"
#include "components/nacl/browser/nacl_broker_service_win.h"
#include "components/nacl/common/nacl_debug_exception_handler_win.h"
#include "content/public/common/sandbox_init.h"
#endif

using content::BrowserThread;
using content::ChildProcessData;
using content::ChildProcessHost;
using ppapi::proxy::SerializedHandle;

namespace nacl {

#if defined(OS_WIN)
namespace {

// Looks for the largest contiguous unallocated region of address
// space and returns it via |*out_addr| and |*out_size|.
void FindAddressSpace(base::ProcessHandle process,
                      char** out_addr, size_t* out_size) {
  *out_addr = NULL;
  *out_size = 0;
  char* addr = 0;
  while (true) {
    MEMORY_BASIC_INFORMATION info;
    size_t result = VirtualQueryEx(process, static_cast<void*>(addr),
                                   &info, sizeof(info));
    if (result < sizeof(info))
      break;
    if (info.State == MEM_FREE && info.RegionSize > *out_size) {
      *out_addr = addr;
      *out_size = info.RegionSize;
    }
    addr += info.RegionSize;
  }
}

#ifdef _DLL

bool IsInPath(const std::string& path_env_var, const std::string& dir) {
  std::vector<std::string> split;
  base::SplitString(path_env_var, ';', &split);
  for (std::vector<std::string>::const_iterator i(split.begin());
       i != split.end();
       ++i) {
    if (*i == dir)
      return true;
  }
  return false;
}

#endif  // _DLL

}  // namespace

// Allocates |size| bytes of address space in the given process at a
// randomised address.
void* AllocateAddressSpaceASLR(base::ProcessHandle process, size_t size) {
  char* addr;
  size_t avail_size;
  FindAddressSpace(process, &addr, &avail_size);
  if (avail_size < size)
    return NULL;
  size_t offset = base::RandGenerator(avail_size - size);
  const int kPageSize = 0x10000;
  void* request_addr =
      reinterpret_cast<void*>(reinterpret_cast<uint64>(addr + offset)
                              & ~(kPageSize - 1));
  return VirtualAllocEx(process, request_addr, size,
                        MEM_RESERVE, PAGE_NOACCESS);
}

namespace {

bool RunningOnWOW64() {
  return (base::win::OSInfo::GetInstance()->wow64_status() ==
          base::win::OSInfo::WOW64_ENABLED);
}

}  // namespace

#endif  // defined(OS_WIN)

namespace {

// NOTE: changes to this class need to be reviewed by the security team.
class NaClSandboxedProcessLauncherDelegate
    : public content::SandboxedProcessLauncherDelegate {
 public:
  NaClSandboxedProcessLauncherDelegate(ChildProcessHost* host)
#if defined(OS_POSIX)
      : ipc_fd_(host->TakeClientFileDescriptor())
#endif
  {}

  ~NaClSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  void PostSpawnTarget(base::ProcessHandle process) override {
    // For Native Client sel_ldr processes on 32-bit Windows, reserve 1 GB of
    // address space to prevent later failure due to address space fragmentation
    // from .dll loading. The NaCl process will attempt to locate this space by
    // scanning the address space using VirtualQuery.
    // TODO(bbudge) Handle the --no-sandbox case.
    // http://code.google.com/p/nativeclient/issues/detail?id=2131
    const SIZE_T kNaClSandboxSize = 1 << 30;
    if (!nacl::AllocateAddressSpaceASLR(process, kNaClSandboxSize)) {
      DLOG(WARNING) << "Failed to reserve address space for Native Client";
    }
  }
#elif defined(OS_POSIX)
  bool ShouldUseZygote() override { return true; }
  base::ScopedFD TakeIpcFd() override { return ipc_fd_.Pass(); }
#endif  // OS_WIN

 private:
#if defined(OS_POSIX)
  base::ScopedFD ipc_fd_;
#endif  // OS_POSIX
};

void SetCloseOnExec(NaClHandle fd) {
#if defined(OS_POSIX)
  int flags = fcntl(fd, F_GETFD);
  CHECK_NE(flags, -1);
  int rc = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
  CHECK_EQ(rc, 0);
#endif
}

void CloseFile(base::File file) {
  // The base::File destructor will close the file for us.
}

}  // namespace

unsigned NaClProcessHost::keepalive_throttle_interval_milliseconds_ =
    ppapi::kKeepaliveThrottleIntervalDefaultMilliseconds;

// Unfortunately, we cannot use ScopedGeneric directly for IPC::ChannelHandle,
// because there is neither operator== nor operator != definition for it.
// Instead, define a simple wrapper for IPC::ChannelHandle with an assumption
// that this only takes a transferred IPC::ChannelHandle or one to be
// transferred via IPC.
class NaClProcessHost::ScopedChannelHandle {
  MOVE_ONLY_TYPE_FOR_CPP_03(ScopedChannelHandle, RValue);
 public:
  ScopedChannelHandle() {
  }
  explicit ScopedChannelHandle(const IPC::ChannelHandle& handle)
      : handle_(handle) {
    DCHECK(IsSupportedHandle(handle_));
  }
  ScopedChannelHandle(RValue other) : handle_(other.object->handle_) {
    other.object->handle_ = IPC::ChannelHandle();
    DCHECK(IsSupportedHandle(handle_));
  }
  ~ScopedChannelHandle() {
    CloseIfNecessary();
  }

  const IPC::ChannelHandle& get() const { return handle_; }
  IPC::ChannelHandle release() WARN_UNUSED_RESULT {
    IPC::ChannelHandle result = handle_;
    handle_ = IPC::ChannelHandle();
    return result;
  }

  void reset(const IPC::ChannelHandle& handle = IPC::ChannelHandle()) {
    DCHECK(IsSupportedHandle(handle));
#if defined(OS_POSIX)
    // Following the manner of base::ScopedGeneric, we do not support
    // reset() with same handle for simplicity of the implementation.
    CHECK(handle.socket.fd == -1 || handle.socket.fd != handle_.socket.fd);
#endif
    CloseIfNecessary();
    handle_ = handle;
  }

 private:
  // Returns true if the given handle is closable automatically by this
  // class. This function is just a helper for validation.
  static bool IsSupportedHandle(const IPC::ChannelHandle& handle) {
#if defined(OS_WIN)
    // On Windows, it is not supported to marshal the |pipe.handle|.
    // In our case, we wrap a transferred ChannelHandle (or one to be
    // transferred) via IPC, so we can assume |handle.pipe.handle| is NULL.
    return handle.pipe.handle == NULL;
#else
    return true;
#endif
  }

  void CloseIfNecessary() {
#if defined(OS_POSIX)
    if (handle_.socket.auto_close) {
      // Defer closing task to the ScopedFD.
      base::ScopedFD(handle_.socket.fd);
    }
#endif
  }

  IPC::ChannelHandle handle_;
};

NaClProcessHost::NaClProcessHost(
    const GURL& manifest_url,
    base::File nexe_file,
    const NaClFileToken& nexe_token,
    const std::vector<NaClResourcePrefetchResult>& prefetched_resource_files,
    ppapi::PpapiPermissions permissions,
    int render_view_id,
    uint32 permission_bits,
    bool uses_nonsfi_mode,
    bool off_the_record,
    NaClAppProcessType process_type,
    const base::FilePath& profile_directory)
    : manifest_url_(manifest_url),
      nexe_file_(nexe_file.Pass()),
      nexe_token_(nexe_token),
      prefetched_resource_files_(prefetched_resource_files),
      permissions_(permissions),
#if defined(OS_WIN)
      process_launched_by_broker_(false),
#endif
      reply_msg_(NULL),
#if defined(OS_WIN)
      debug_exception_handler_requested_(false),
#endif
      uses_nonsfi_mode_(uses_nonsfi_mode),
      enable_debug_stub_(false),
      enable_crash_throttling_(false),
      off_the_record_(off_the_record),
      process_type_(process_type),
      profile_directory_(profile_directory),
      render_view_id_(render_view_id),
      weak_factory_(this) {
  process_.reset(content::BrowserChildProcessHost::Create(
      static_cast<content::ProcessType>(PROCESS_TYPE_NACL_LOADER), this));

  // Set the display name so the user knows what plugin the process is running.
  // We aren't on the UI thread so getting the pref locale for language
  // formatting isn't possible, so IDN will be lost, but this is probably OK
  // for this use case.
  process_->SetName(net::FormatUrl(manifest_url_, std::string()));

  enable_debug_stub_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNaClDebug);
  DCHECK(process_type_ != kUnknownNaClProcessType);
  enable_crash_throttling_ = process_type_ != kNativeNaClProcessType;
}

NaClProcessHost::~NaClProcessHost() {
  // Report exit status only if the process was successfully started.
  if (process_->GetData().handle != base::kNullProcessHandle) {
    int exit_code = 0;
    process_->GetTerminationStatus(false /* known_dead */, &exit_code);
    std::string message =
        base::StringPrintf("NaCl process exited with status %i (0x%x)",
                           exit_code, exit_code);
    if (exit_code == 0) {
      VLOG(1) << message;
    } else {
      LOG(ERROR) << message;
    }
    NaClBrowser::GetInstance()->OnProcessEnd(process_->GetData().id);
  }

  // Note: this does not work on Windows, though we currently support this
  // prefetching feature only on POSIX platforms, so it should be ok.
#if defined(OS_WIN)
  DCHECK(prefetched_resource_files_.empty());
#else
  for (size_t i = 0; i < prefetched_resource_files_.size(); ++i) {
    // The process failed to launch for some reason. Close resource file
    // handles.
    base::File file(IPC::PlatformFileForTransitToFile(
        prefetched_resource_files_[i].file));
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE,
        base::Bind(&CloseFile, base::Passed(file.Pass())));
  }
#endif

  if (reply_msg_) {
    // The process failed to launch for some reason.
    // Don't keep the renderer hanging.
    reply_msg_->set_reply_error();
    nacl_host_message_filter_->Send(reply_msg_);
  }
#if defined(OS_WIN)
  if (process_launched_by_broker_) {
    NaClBrokerService::GetInstance()->OnLoaderDied();
  }
#endif
}

void NaClProcessHost::OnProcessCrashed(int exit_status) {
  if (enable_crash_throttling_ &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePnaclCrashThrottling)) {
    NaClBrowser::GetInstance()->OnProcessCrashed();
  }
}

// This is called at browser startup.
// static
void NaClProcessHost::EarlyStartup() {
  NaClBrowser::GetInstance()->EarlyStartup();
  // Inform NaClBrowser that we exist and will have a debug port at some point.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Open the IRT file early to make sure that it isn't replaced out from
  // under us by autoupdate.
  NaClBrowser::GetInstance()->EnsureIrtAvailable();
#endif
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  UMA_HISTOGRAM_BOOLEAN(
      "NaCl.nacl-gdb",
      !cmd->GetSwitchValuePath(switches::kNaClGdb).empty());
  UMA_HISTOGRAM_BOOLEAN(
      "NaCl.nacl-gdb-script",
      !cmd->GetSwitchValuePath(switches::kNaClGdbScript).empty());
  UMA_HISTOGRAM_BOOLEAN(
      "NaCl.enable-nacl-debug",
      cmd->HasSwitch(switches::kEnableNaClDebug));
  std::string nacl_debug_mask =
      cmd->GetSwitchValueASCII(switches::kNaClDebugMask);
  // By default, exclude debugging SSH and the PNaCl translator.
  // about::flags only allows empty flags as the default, so replace
  // the empty setting with the default. To debug all apps, use a wild-card.
  if (nacl_debug_mask.empty()) {
    nacl_debug_mask = "!*://*/*ssh_client.nmf,chrome://pnacl-translator/*";
  }
  NaClBrowser::GetDelegate()->SetDebugPatterns(nacl_debug_mask);
}

// static
void NaClProcessHost::SetPpapiKeepAliveThrottleForTesting(
    unsigned milliseconds) {
  keepalive_throttle_interval_milliseconds_ = milliseconds;
}

void NaClProcessHost::Launch(
    NaClHostMessageFilter* nacl_host_message_filter,
    IPC::Message* reply_msg,
    const base::FilePath& manifest_path) {
  nacl_host_message_filter_ = nacl_host_message_filter;
  reply_msg_ = reply_msg;
  manifest_path_ = manifest_path;

  // Do not launch the requested NaCl module if NaCl is marked "unstable" due
  // to too many crashes within a given time period.
  if (enable_crash_throttling_ &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePnaclCrashThrottling) &&
      NaClBrowser::GetInstance()->IsThrottled()) {
    SendErrorToRenderer("Process creation was throttled due to excessive"
                        " crashes");
    delete this;
    return;
  }

  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
#if defined(OS_WIN)
  if (cmd->HasSwitch(switches::kEnableNaClDebug) &&
      !cmd->HasSwitch(switches::kNoSandbox)) {
    // We don't switch off sandbox automatically for security reasons.
    SendErrorToRenderer("NaCl's GDB debug stub requires --no-sandbox flag"
                        " on Windows. See crbug.com/265624.");
    delete this;
    return;
  }
#endif
  if (cmd->HasSwitch(switches::kNaClGdb) &&
      !cmd->HasSwitch(switches::kEnableNaClDebug)) {
    LOG(WARNING) << "--nacl-gdb flag requires --enable-nacl-debug flag";
  }

  // Start getting the IRT open asynchronously while we launch the NaCl process.
  // We'll make sure this actually finished in StartWithLaunchedProcess, below.
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  nacl_browser->EnsureAllResourcesAvailable();
  if (!nacl_browser->IsOk()) {
    SendErrorToRenderer("could not find all the resources needed"
                        " to launch the process");
    delete this;
    return;
  }

  if (uses_nonsfi_mode_) {
    bool nonsfi_mode_forced_by_command_line = false;
    bool nonsfi_mode_allowed = false;
#if defined(OS_LINUX)
    nonsfi_mode_forced_by_command_line =
        cmd->HasSwitch(switches::kEnableNaClNonSfiMode);
#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
    nonsfi_mode_allowed = NaClBrowser::GetDelegate()->IsNonSfiModeAllowed(
        nacl_host_message_filter->profile_directory(), manifest_url_);
#endif
#endif
    bool nonsfi_mode_enabled =
        nonsfi_mode_forced_by_command_line || nonsfi_mode_allowed;

    if (!nonsfi_mode_enabled) {
      SendErrorToRenderer(
          "NaCl non-SFI mode is not available for this platform"
          " and NaCl module.");
      delete this;
      return;
    }
  } else {
    // Rather than creating a socket pair in the renderer, and passing
    // one side through the browser to sel_ldr, socket pairs are created
    // in the browser and then passed to the renderer and sel_ldr.
    //
    // This is mainly for the benefit of Windows, where sockets cannot
    // be passed in messages, but are copied via DuplicateHandle().
    // This means the sandboxed renderer cannot send handles to the
    // browser process.

    NaClHandle pair[2];
    // Create a connected socket
    if (NaClSocketPair(pair) == -1) {
      SendErrorToRenderer("NaClSocketPair() failed");
      delete this;
      return;
    }
    socket_for_renderer_ = base::File(pair[0]);
    socket_for_sel_ldr_ = base::File(pair[1]);
    SetCloseOnExec(pair[0]);
    SetCloseOnExec(pair[1]);
  }

  // Create a shared memory region that the renderer and plugin share for
  // reporting crash information.
  crash_info_shmem_.CreateAnonymous(kNaClCrashInfoShmemSize);

  // Launch the process
  if (!LaunchSelLdr()) {
    delete this;
  }
}

void NaClProcessHost::OnChannelConnected(int32 peer_pid) {
  if (!base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kNaClGdb).empty()) {
    LaunchNaClGdb();
  }
}

#if defined(OS_WIN)
void NaClProcessHost::OnProcessLaunchedByBroker(base::ProcessHandle handle) {
  process_launched_by_broker_ = true;
  process_->SetHandle(handle);
  SetDebugStubPort(nacl::kGdbDebugStubPortUnknown);
  if (!StartWithLaunchedProcess())
    delete this;
}

void NaClProcessHost::OnDebugExceptionHandlerLaunchedByBroker(bool success) {
  IPC::Message* reply = attach_debug_exception_handler_reply_msg_.release();
  NaClProcessMsg_AttachDebugExceptionHandler::WriteReplyParams(reply, success);
  Send(reply);
}
#endif

// Needed to handle sync messages in OnMessageReceived.
bool NaClProcessHost::Send(IPC::Message* msg) {
  return process_->Send(msg);
}

void NaClProcessHost::LaunchNaClGdb() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if defined(OS_WIN)
  base::FilePath nacl_gdb =
      command_line.GetSwitchValuePath(switches::kNaClGdb);
  base::CommandLine cmd_line(nacl_gdb);
#else
  base::CommandLine::StringType nacl_gdb =
      command_line.GetSwitchValueNative(switches::kNaClGdb);
  base::CommandLine::StringVector argv;
  // We don't support spaces inside arguments in --nacl-gdb switch.
  base::SplitString(nacl_gdb, static_cast<base::CommandLine::CharType>(' '),
                    &argv);
  base::CommandLine cmd_line(argv);
#endif
  cmd_line.AppendArg("--eval-command");
  base::FilePath::StringType irt_path(
      NaClBrowser::GetInstance()->GetIrtFilePath().value());
  // Avoid back slashes because nacl-gdb uses posix escaping rules on Windows.
  // See issue https://code.google.com/p/nativeclient/issues/detail?id=3482.
  std::replace(irt_path.begin(), irt_path.end(), '\\', '/');
  cmd_line.AppendArgNative(FILE_PATH_LITERAL("nacl-irt \"") + irt_path +
                           FILE_PATH_LITERAL("\""));
  if (!manifest_path_.empty()) {
    cmd_line.AppendArg("--eval-command");
    base::FilePath::StringType manifest_path_value(manifest_path_.value());
    std::replace(manifest_path_value.begin(), manifest_path_value.end(),
                 '\\', '/');
    cmd_line.AppendArgNative(FILE_PATH_LITERAL("nacl-manifest \"") +
                             manifest_path_value + FILE_PATH_LITERAL("\""));
  }
  cmd_line.AppendArg("--eval-command");
  cmd_line.AppendArg("target remote :4014");
  base::FilePath script =
      command_line.GetSwitchValuePath(switches::kNaClGdbScript);
  if (!script.empty()) {
    cmd_line.AppendArg("--command");
    cmd_line.AppendArgNative(script.value());
  }
  base::LaunchProcess(cmd_line, base::LaunchOptions());
}

bool NaClProcessHost::LaunchSelLdr() {
  std::string channel_id = process_->GetHost()->CreateChannel();
  if (channel_id.empty()) {
    SendErrorToRenderer("CreateChannel() failed");
    return false;
  }

  // Build command line for nacl.

#if defined(OS_MACOSX)
  // The Native Client process needs to be able to allocate a 1GB contiguous
  // region to use as the client environment's virtual address space. ASLR
  // (PIE) interferes with this by making it possible that no gap large enough
  // to accomodate this request will exist in the child process' address
  // space. Disable PIE for NaCl processes. See http://crbug.com/90221 and
  // http://code.google.com/p/nativeclient/issues/detail?id=2043.
  int flags = ChildProcessHost::CHILD_NO_PIE;
#elif defined(OS_LINUX)
  int flags = ChildProcessHost::CHILD_ALLOW_SELF;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  base::FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    return false;

#if defined(OS_WIN)
  // On Windows 64-bit NaCl loader is called nacl64.exe instead of chrome.exe
  if (RunningOnWOW64()) {
    if (!NaClBrowser::GetInstance()->GetNaCl64ExePath(&exe_path)) {
      SendErrorToRenderer("could not get path to nacl64.exe");
      return false;
    }

#ifdef _DLL
    // When using the DLL CRT on Windows, we need to amend the PATH to include
    // the location of the x64 CRT DLLs. This is only the case when using a
    // component=shared_library build (i.e. generally dev debug builds). The
    // x86 CRT DLLs are in e.g. out\Debug for chrome.exe etc., so the x64 ones
    // are put in out\Debug\x64 which we add to the PATH here so that loader
    // can find them. See http://crbug.com/346034.
    scoped_ptr<base::Environment> env(base::Environment::Create());
    static const char kPath[] = "PATH";
    std::string old_path;
    base::FilePath module_path;
    if (!PathService::Get(base::FILE_MODULE, &module_path)) {
      SendErrorToRenderer("could not get path to current module");
      return false;
    }
    std::string x64_crt_path =
        base::WideToUTF8(module_path.DirName().Append(L"x64").value());
    if (!env->GetVar(kPath, &old_path)) {
      env->SetVar(kPath, x64_crt_path);
    } else if (!IsInPath(old_path, x64_crt_path)) {
      std::string new_path(old_path);
      new_path.append(";");
      new_path.append(x64_crt_path);
      env->SetVar(kPath, new_path);
    }
#endif  // _DLL
  }
#endif

  scoped_ptr<base::CommandLine> cmd_line(new base::CommandLine(exe_path));
  CopyNaClCommandLineArguments(cmd_line.get());

  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              (uses_nonsfi_mode_ ?
                               switches::kNaClLoaderNonSfiProcess :
                               switches::kNaClLoaderProcess));
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id);
  if (NaClBrowser::GetDelegate()->DialogsAreSuppressed())
    cmd_line->AppendSwitch(switches::kNoErrorDialogs);

  // On Windows we might need to start the broker process to launch a new loader
#if defined(OS_WIN)
  if (RunningOnWOW64()) {
    if (!NaClBrokerService::GetInstance()->LaunchLoader(
            weak_factory_.GetWeakPtr(), channel_id)) {
      SendErrorToRenderer("broker service did not launch process");
      return false;
    }
    return true;
  }
#endif
  process_->Launch(
      new NaClSandboxedProcessLauncherDelegate(process_->GetHost()),
      cmd_line.release(),
      true);
  return true;
}

bool NaClProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  if (uses_nonsfi_mode_) {
    // IPC messages relating to NaCl's validation cache must not be exposed
    // in Non-SFI Mode, otherwise a Non-SFI nexe could use
    // SetKnownToValidate to create a hole in the SFI sandbox.
    IPC_BEGIN_MESSAGE_MAP(NaClProcessHost, msg)
      IPC_MESSAGE_HANDLER(NaClProcessHostMsg_PpapiChannelsCreated,
                          OnPpapiChannelsCreated)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  } else {
    IPC_BEGIN_MESSAGE_MAP(NaClProcessHost, msg)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_QueryKnownToValidate,
                          OnQueryKnownToValidate)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_SetKnownToValidate,
                          OnSetKnownToValidate)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_ResolveFileToken,
                          OnResolveFileToken)

#if defined(OS_WIN)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(
          NaClProcessMsg_AttachDebugExceptionHandler,
          OnAttachDebugExceptionHandler)
      IPC_MESSAGE_HANDLER(NaClProcessHostMsg_DebugStubPortSelected,
                          OnDebugStubPortSelected)
#endif
      IPC_MESSAGE_HANDLER(NaClProcessHostMsg_PpapiChannelsCreated,
                          OnPpapiChannelsCreated)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  }
  return handled;
}

void NaClProcessHost::OnProcessLaunched() {
  if (!StartWithLaunchedProcess())
    delete this;
}

// Called when the NaClBrowser singleton has been fully initialized.
void NaClProcessHost::OnResourcesReady() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  if (!nacl_browser->IsReady()) {
    SendErrorToRenderer("could not acquire shared resources needed by NaCl");
    delete this;
  } else if (!StartNaClExecution()) {
    delete this;
  }
}

void NaClProcessHost::ReplyToRenderer(
    ScopedChannelHandle ppapi_channel_handle,
    ScopedChannelHandle trusted_channel_handle,
    ScopedChannelHandle manifest_service_channel_handle) {
#if defined(OS_WIN)
  // If we are on 64-bit Windows, the NaCl process's sandbox is
  // managed by a different process from the renderer's sandbox.  We
  // need to inform the renderer's sandbox about the NaCl process so
  // that the renderer can send handles to the NaCl process using
  // BrokerDuplicateHandle().
  if (RunningOnWOW64()) {
    if (!content::BrokerAddTargetPeer(process_->GetData().handle)) {
      SendErrorToRenderer("BrokerAddTargetPeer() failed");
      return;
    }
  }
#endif

  // First, create an |imc_channel_handle| for the renderer.
  IPC::PlatformFileForTransit imc_handle_for_renderer =
      IPC::TakeFileHandleForProcess(socket_for_renderer_.Pass(),
                                    nacl_host_message_filter_->PeerHandle());
  if (imc_handle_for_renderer == IPC::InvalidPlatformFileForTransit()) {
    // Failed to create the handle.
    SendErrorToRenderer("imc_channel_handle creation failed.");
    return;
  }

  // Hereafter, we always send an IPC message with handles including imc_handle
  // created above which, on Windows, are not closable in this process.
  std::string error_message;
  base::SharedMemoryHandle crash_info_shmem_renderer_handle;
  if (!crash_info_shmem_.ShareToProcess(nacl_host_message_filter_->PeerHandle(),
                                        &crash_info_shmem_renderer_handle)) {
    // On error, we do not send "IPC::ChannelHandle"s to the renderer process.
    // Note that some other FDs/handles still get sent to the renderer, but
    // will be closed there.
    ppapi_channel_handle.reset();
    trusted_channel_handle.reset();
    manifest_service_channel_handle.reset();
    error_message = "ShareToProcess() failed";
  }

  const ChildProcessData& data = process_->GetData();
  SendMessageToRenderer(
      NaClLaunchResult(imc_handle_for_renderer,
                       ppapi_channel_handle.release(),
                       trusted_channel_handle.release(),
                       manifest_service_channel_handle.release(),
                       base::GetProcId(data.handle),
                       data.id,
                       crash_info_shmem_renderer_handle),
      error_message);

  // Now that the crash information shmem handles have been shared with the
  // plugin and the renderer, the browser can close its handle.
  crash_info_shmem_.Close();
}

void NaClProcessHost::SendErrorToRenderer(const std::string& error_message) {
  LOG(ERROR) << "NaCl process launch failed: " << error_message;
  SendMessageToRenderer(NaClLaunchResult(), error_message);
}

void NaClProcessHost::SendMessageToRenderer(
    const NaClLaunchResult& result,
    const std::string& error_message) {
  DCHECK(nacl_host_message_filter_.get());
  DCHECK(reply_msg_);
  if (nacl_host_message_filter_.get() == NULL || reply_msg_ == NULL) {
    // As DCHECKed above, this case should not happen in general.
    // Though, in this case, unfortunately there is no proper way to release
    // resources which are already created in |result|. We just give up on
    // releasing them, and leak them.
    return;
  }

  NaClHostMsg_LaunchNaCl::WriteReplyParams(reply_msg_, result, error_message);
  nacl_host_message_filter_->Send(reply_msg_);
  nacl_host_message_filter_ = NULL;
  reply_msg_ = NULL;
}

void NaClProcessHost::SetDebugStubPort(int port) {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  nacl_browser->SetProcessGdbDebugStubPort(process_->GetData().id, port);
}

#if defined(OS_POSIX)
// TCP port we chose for NaCl debug stub. It can be any other number.
static const uint16_t kInitialDebugStubPort = 4014;

net::SocketDescriptor NaClProcessHost::GetDebugStubSocketHandle() {
  net::SocketDescriptor s = net::kInvalidSocket;
  // We always try to allocate the default port first. If this fails, we then
  // allocate any available port.
  // On success, if the test system has register a handler
  // (GdbDebugStubPortListener), we fire a notification.
  uint16 port = kInitialDebugStubPort;
  s = net::TCPListenSocket::CreateAndBind("127.0.0.1", port);
  if (s == net::kInvalidSocket) {
    s = net::TCPListenSocket::CreateAndBindAnyPort("127.0.0.1", &port);
  }
  if (s != net::kInvalidSocket) {
    SetDebugStubPort(port);
  }
  if (s == net::kInvalidSocket) {
    LOG(ERROR) << "failed to open socket for debug stub";
    return net::kInvalidSocket;
  } else {
    LOG(WARNING) << "debug stub on port " << port;
  }
  if (listen(s, 1)) {
    LOG(ERROR) << "listen() failed on debug stub socket";
    if (IGNORE_EINTR(close(s)) < 0)
      PLOG(ERROR) << "failed to close debug stub socket";
    return net::kInvalidSocket;
  }
  return s;
}
#endif

#if defined(OS_WIN)
void NaClProcessHost::OnDebugStubPortSelected(uint16_t debug_stub_port) {
  CHECK(!uses_nonsfi_mode_);
  SetDebugStubPort(debug_stub_port);
}
#endif

bool NaClProcessHost::StartNaClExecution() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();

  NaClStartParams params;

  // Enable PPAPI proxy channel creation only for renderer processes.
  params.enable_ipc_proxy = enable_ppapi_proxy();
  params.process_type = process_type_;
  bool enable_nacl_debug = enable_debug_stub_ &&
      NaClBrowser::GetDelegate()->URLMatchesDebugPatterns(manifest_url_);
  if (uses_nonsfi_mode_) {
    // Currently, non-SFI mode is supported only on Linux.
#if defined(OS_LINUX)
    // In non-SFI mode, we do not use SRPC. Make sure that the socketpair is
    // not created.
    DCHECK(!socket_for_sel_ldr_.IsValid());
#endif
    if (enable_nacl_debug) {
      base::ProcessId pid = base::GetProcId(process_->GetData().handle);
      LOG(WARNING) << "nonsfi nacl plugin running in " << pid;
    }
  } else {
    params.validation_cache_enabled = nacl_browser->ValidationCacheIsEnabled();
    params.validation_cache_key = nacl_browser->GetValidationCacheKey();
    params.version = NaClBrowser::GetDelegate()->GetVersionString();
    params.enable_debug_stub = enable_nacl_debug;

    const ChildProcessData& data = process_->GetData();
    params.imc_bootstrap_handle =
        IPC::TakeFileHandleForProcess(socket_for_sel_ldr_.Pass(), data.handle);
    if (params.imc_bootstrap_handle == IPC::InvalidPlatformFileForTransit()) {
      return false;
    }

    const base::File& irt_file = nacl_browser->IrtFile();
    CHECK(irt_file.IsValid());
    // Send over the IRT file handle.  We don't close our own copy!
    params.irt_handle = IPC::GetFileHandleForProcess(
        irt_file.GetPlatformFile(), data.handle, false);
    if (params.irt_handle == IPC::InvalidPlatformFileForTransit()) {
      return false;
    }

#if defined(OS_MACOSX)
    // For dynamic loading support, NaCl requires a file descriptor that
    // was created in /tmp, since those created with shm_open() are not
    // mappable with PROT_EXEC.  Rather than requiring an extra IPC
    // round trip out of the sandbox, we create an FD here.
    base::SharedMemory memory_buffer;
    base::SharedMemoryCreateOptions options;
    options.size = 1;
    options.executable = true;
    if (!memory_buffer.Create(options)) {
      DLOG(ERROR) << "Failed to allocate memory buffer";
      return false;
    }
    base::ScopedFD memory_fd(dup(memory_buffer.handle().fd));
    if (!memory_fd.is_valid()) {
      DLOG(ERROR) << "Failed to dup() a file descriptor";
      return false;
    }
    params.mac_shm_fd = IPC::GetFileHandleForProcess(
        memory_fd.release(), data.handle, true);
#endif

#if defined(OS_POSIX)
    if (params.enable_debug_stub) {
      net::SocketDescriptor server_bound_socket = GetDebugStubSocketHandle();
      if (server_bound_socket != net::kInvalidSocket) {
        params.debug_stub_server_bound_socket = IPC::GetFileHandleForProcess(
            server_bound_socket, data.handle, true);
      }
    }
#endif
  }

  if (!crash_info_shmem_.ShareToProcess(process_->GetData().handle,
                                        &params.crash_info_shmem_handle)) {
    DLOG(ERROR) << "Failed to ShareToProcess() a shared memory buffer";
    return false;
  }

  // Pass the pre-opened resource files to the loader. We do not have to reopen
  // resource files here even for SFI mode because the descriptors are not from
  // a renderer.
  for (size_t i = 0; i < prefetched_resource_files_.size(); ++i) {
    process_->Send(new NaClProcessMsg_AddPrefetchedResource(
        NaClResourcePrefetchResult(
            prefetched_resource_files_[i].file,
            // For the same reason as the comment below, always use an empty
            // base::FilePath for non-SFI mode.
            (uses_nonsfi_mode_ ? base::FilePath() :
             prefetched_resource_files_[i].file_path_metadata),
            prefetched_resource_files_[i].file_key)));
  }
  prefetched_resource_files_.clear();

  base::FilePath file_path;
  if (uses_nonsfi_mode_) {
    // Don't retrieve the file path when using nonsfi mode; there's no
    // validation caching in that case, so it's unnecessary work, and would
    // expose the file path to the plugin.
  } else {
    if (NaClBrowser::GetInstance()->GetFilePath(nexe_token_.lo,
                                                nexe_token_.hi,
                                                &file_path)) {
      // We have to reopen the file in the browser process; we don't want a
      // compromised renderer to pass an arbitrary fd that could get loaded
      // into the plugin process.
      if (base::PostTaskAndReplyWithResult(
              content::BrowserThread::GetBlockingPool(),
              FROM_HERE,
              base::Bind(OpenNaClReadExecImpl,
                         file_path,
                         true /* is_executable */),
              base::Bind(&NaClProcessHost::StartNaClFileResolved,
                         weak_factory_.GetWeakPtr(),
                         params,
                         file_path))) {
        return true;
      }
    }
  }

  params.nexe_file = IPC::TakeFileHandleForProcess(nexe_file_.Pass(),
                                                   process_->GetData().handle);
  process_->Send(new NaClProcessMsg_Start(params));
  return true;
}

void NaClProcessHost::StartNaClFileResolved(
    NaClStartParams params,
    const base::FilePath& file_path,
    base::File checked_nexe_file) {
  if (checked_nexe_file.IsValid()) {
    // Release the file received from the renderer. This has to be done on a
    // thread where IO is permitted, though.
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE,
        base::Bind(&CloseFile, base::Passed(nexe_file_.Pass())));
    params.nexe_file_path_metadata = file_path;
    params.nexe_file = IPC::TakeFileHandleForProcess(
        checked_nexe_file.Pass(), process_->GetData().handle);
  } else {
    params.nexe_file = IPC::TakeFileHandleForProcess(
        nexe_file_.Pass(), process_->GetData().handle);
  }
  process_->Send(new NaClProcessMsg_Start(params));
}

// This method is called when NaClProcessHostMsg_PpapiChannelCreated is
// received.
void NaClProcessHost::OnPpapiChannelsCreated(
    const IPC::ChannelHandle& raw_browser_channel_handle,
    const IPC::ChannelHandle& raw_ppapi_renderer_channel_handle,
    const IPC::ChannelHandle& raw_trusted_renderer_channel_handle,
    const IPC::ChannelHandle& raw_manifest_service_channel_handle) {
  ScopedChannelHandle browser_channel_handle(raw_browser_channel_handle);
  ScopedChannelHandle ppapi_renderer_channel_handle(
      raw_ppapi_renderer_channel_handle);
  ScopedChannelHandle trusted_renderer_channel_handle(
      raw_trusted_renderer_channel_handle);
  ScopedChannelHandle manifest_service_channel_handle(
      raw_manifest_service_channel_handle);

  if (!enable_ppapi_proxy()) {
    ReplyToRenderer(ScopedChannelHandle(),
                    trusted_renderer_channel_handle.Pass(),
                    manifest_service_channel_handle.Pass());
    return;
  }

  if (ipc_proxy_channel_.get()) {
    // Attempt to open more than 1 browser channel is not supported.
    // Shut down the NaCl process.
    process_->GetHost()->ForceShutdown();
    return;
  }

  DCHECK_EQ(PROCESS_TYPE_NACL_LOADER, process_->GetData().process_type);

  ipc_proxy_channel_ =
      IPC::ChannelProxy::Create(browser_channel_handle.release(),
                                IPC::Channel::MODE_CLIENT,
                                NULL,
                                base::MessageLoopProxy::current().get());
  // Create the browser ppapi host and enable PPAPI message dispatching to the
  // browser process.
  ppapi_host_.reset(content::BrowserPpapiHost::CreateExternalPluginProcess(
      ipc_proxy_channel_.get(),  // sender
      permissions_,
      process_->GetData().handle,
      ipc_proxy_channel_.get(),
      nacl_host_message_filter_->render_process_id(),
      render_view_id_,
      profile_directory_));
  ppapi_host_->SetOnKeepaliveCallback(
      NaClBrowser::GetDelegate()->GetOnKeepaliveCallback());

  ppapi::PpapiNaClPluginArgs args;
  args.off_the_record = nacl_host_message_filter_->off_the_record();
  args.permissions = permissions_;
  args.keepalive_throttle_interval_milliseconds =
      keepalive_throttle_interval_milliseconds_;
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  DCHECK(cmdline);
  std::string flag_whitelist[] = {
    switches::kV,
    switches::kVModule,
  };
  for (size_t i = 0; i < arraysize(flag_whitelist); ++i) {
    std::string value = cmdline->GetSwitchValueASCII(flag_whitelist[i]);
    if (!value.empty()) {
      args.switch_names.push_back(flag_whitelist[i]);
      args.switch_values.push_back(value);
    }
  }

  ppapi_host_->GetPpapiHost()->AddHostFactoryFilter(
      scoped_ptr<ppapi::host::HostFactory>(
          NaClBrowser::GetDelegate()->CreatePpapiHostFactory(
              ppapi_host_.get())));

  // Send a message to initialize the IPC dispatchers in the NaCl plugin.
  ipc_proxy_channel_->Send(new PpapiMsg_InitializeNaClDispatcher(args));

  // Let the renderer know that the IPC channels are established.
  ReplyToRenderer(ppapi_renderer_channel_handle.Pass(),
                  trusted_renderer_channel_handle.Pass(),
                  manifest_service_channel_handle.Pass());
}

bool NaClProcessHost::StartWithLaunchedProcess() {
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();

  if (nacl_browser->IsReady()) {
    return StartNaClExecution();
  } else if (nacl_browser->IsOk()) {
    nacl_browser->WaitForResources(
        base::Bind(&NaClProcessHost::OnResourcesReady,
                   weak_factory_.GetWeakPtr()));
    return true;
  } else {
    SendErrorToRenderer("previously failed to acquire shared resources");
    return false;
  }
}

void NaClProcessHost::OnQueryKnownToValidate(const std::string& signature,
                                             bool* result) {
  CHECK(!uses_nonsfi_mode_);
  NaClBrowser* nacl_browser = NaClBrowser::GetInstance();
  *result = nacl_browser->QueryKnownToValidate(signature, off_the_record_);
}

void NaClProcessHost::OnSetKnownToValidate(const std::string& signature) {
  CHECK(!uses_nonsfi_mode_);
  NaClBrowser::GetInstance()->SetKnownToValidate(
      signature, off_the_record_);
}

void NaClProcessHost::OnResolveFileToken(uint64 file_token_lo,
                                         uint64 file_token_hi) {
  // Was the file registered?
  //
  // Note that the file path cache is of bounded size, and old entries can get
  // evicted.  If a large number of NaCl modules are being launched at once,
  // resolving the file_token may fail because the path cache was thrashed
  // while the file_token was in flight.  In this case the query fails, and we
  // need to fall back to the slower path.
  //
  // However: each NaCl process will consume 2-3 entries as it starts up, this
  // means that eviction will not happen unless you start up 33+ NaCl processes
  // at the same time, and this still requires worst-case timing.  As a
  // practical matter, no entries should be evicted prematurely.
  // The cache itself should take ~ (150 characters * 2 bytes/char + ~60 bytes
  // data structure overhead) * 100 = 35k when full, so making it bigger should
  // not be a problem, if needed.
  //
  // Each NaCl process will consume 2-3 entries because the manifest and main
  // nexe are currently not resolved.  Shared libraries will be resolved.  They
  // will be loaded sequentially, so they will only consume a single entry
  // while the load is in flight.
  //
  // TODO(ncbray): track behavior with UMA. If entries are getting evicted or
  // bogus keys are getting queried, this would be good to know.
  CHECK(!uses_nonsfi_mode_);
  base::FilePath file_path;
  if (!NaClBrowser::GetInstance()->GetFilePath(
        file_token_lo, file_token_hi, &file_path)) {
    Send(new NaClProcessMsg_ResolveFileTokenReply(
             file_token_lo,
             file_token_hi,
             IPC::PlatformFileForTransit(),
             base::FilePath()));
    return;
  }

  // Open the file.
  if (!base::PostTaskAndReplyWithResult(
          content::BrowserThread::GetBlockingPool(),
          FROM_HERE,
          base::Bind(OpenNaClReadExecImpl, file_path, true /* is_executable */),
          base::Bind(&NaClProcessHost::FileResolved,
                     weak_factory_.GetWeakPtr(),
                     file_token_lo,
                     file_token_hi,
                     file_path))) {
    Send(new NaClProcessMsg_ResolveFileTokenReply(
            file_token_lo,
            file_token_hi,
            IPC::PlatformFileForTransit(),
            base::FilePath()));
  }
}

void NaClProcessHost::FileResolved(
    uint64_t file_token_lo,
    uint64_t file_token_hi,
    const base::FilePath& file_path,
    base::File file) {
  base::FilePath out_file_path;
  IPC::PlatformFileForTransit out_handle;
  if (file.IsValid()) {
    out_file_path = file_path;
    out_handle = IPC::TakeFileHandleForProcess(
        file.Pass(),
        process_->GetData().handle);
  } else {
    out_handle = IPC::InvalidPlatformFileForTransit();
  }
  Send(new NaClProcessMsg_ResolveFileTokenReply(
           file_token_lo,
           file_token_hi,
           out_handle,
           out_file_path));
}

#if defined(OS_WIN)
void NaClProcessHost::OnAttachDebugExceptionHandler(const std::string& info,
                                                    IPC::Message* reply_msg) {
  CHECK(!uses_nonsfi_mode_);
  if (!AttachDebugExceptionHandler(info, reply_msg)) {
    // Send failure message.
    NaClProcessMsg_AttachDebugExceptionHandler::WriteReplyParams(reply_msg,
                                                                 false);
    Send(reply_msg);
  }
}

bool NaClProcessHost::AttachDebugExceptionHandler(const std::string& info,
                                                  IPC::Message* reply_msg) {
  bool enable_exception_handling = process_type_ == kNativeNaClProcessType;
  if (!enable_exception_handling && !enable_debug_stub_) {
    DLOG(ERROR) <<
        "Debug exception handler requested by NaCl process when not enabled";
    return false;
  }
  if (debug_exception_handler_requested_) {
    // The NaCl process should not request this multiple times.
    DLOG(ERROR) << "Multiple AttachDebugExceptionHandler requests received";
    return false;
  }
  debug_exception_handler_requested_ = true;

  base::ProcessId nacl_pid = base::GetProcId(process_->GetData().handle);
  // We cannot use process_->GetData().handle because it does not have
  // the necessary access rights.  We open the new handle here rather
  // than in the NaCl broker process in case the NaCl loader process
  // dies before the NaCl broker process receives the message we send.
  // The debug exception handler uses DebugActiveProcess() to attach,
  // but this takes a PID.  We need to prevent the NaCl loader's PID
  // from being reused before DebugActiveProcess() is called, and
  // holding a process handle open achieves this.
  base::Process process =
      base::Process::OpenWithAccess(nacl_pid,
                                    PROCESS_QUERY_INFORMATION |
                                    PROCESS_SUSPEND_RESUME |
                                    PROCESS_TERMINATE |
                                    PROCESS_VM_OPERATION |
                                    PROCESS_VM_READ |
                                    PROCESS_VM_WRITE |
                                    PROCESS_DUP_HANDLE |
                                    SYNCHRONIZE);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to get process handle";
    return false;
  }

  attach_debug_exception_handler_reply_msg_.reset(reply_msg);
  // If the NaCl loader is 64-bit, the process running its debug
  // exception handler must be 64-bit too, so we use the 64-bit NaCl
  // broker process for this.  Otherwise, on a 32-bit system, we use
  // the 32-bit browser process to run the debug exception handler.
  if (RunningOnWOW64()) {
    return NaClBrokerService::GetInstance()->LaunchDebugExceptionHandler(
               weak_factory_.GetWeakPtr(), nacl_pid, process.Handle(),
               info);
  } else {
    NaClStartDebugExceptionHandlerThread(
        process.Pass(), info,
        base::MessageLoopProxy::current(),
        base::Bind(&NaClProcessHost::OnDebugExceptionHandlerLaunchedByBroker,
                   weak_factory_.GetWeakPtr()));
    return true;
  }
}
#endif

}  // namespace nacl
