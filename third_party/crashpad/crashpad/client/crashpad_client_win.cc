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

#include "client/crashpad_client.h"

#include <windows.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/scoped_generic.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "util/file/file_io.h"
#include "util/win/address_types.h"
#include "util/win/command_line.h"
#include "util/win/critical_section_with_debug_info.h"
#include "util/win/get_function.h"
#include "util/win/handle.h"
#include "util/win/nt_internals.h"
#include "util/win/ntstatus_logging.h"
#include "util/win/process_info.h"
#include "util/win/registration_protocol_win.h"
#include "util/win/scoped_handle.h"
#include "util/win/scoped_process_suspend.h"

namespace {

// This handle is never closed. This is used to signal to the server that a dump
// should be taken in the event of a crash.
HANDLE g_signal_exception = INVALID_HANDLE_VALUE;

// Where we store the exception information that the crash handler reads.
crashpad::ExceptionInformation g_crash_exception_information;

// These handles are never closed. g_signal_non_crash_dump is used to signal to
// the server to take a dump (not due to an exception), and the server will
// signal g_non_crash_dump_done when the dump is completed.
HANDLE g_signal_non_crash_dump = INVALID_HANDLE_VALUE;
HANDLE g_non_crash_dump_done = INVALID_HANDLE_VALUE;

// Guards multiple simultaneous calls to DumpWithoutCrash(). This is leaked.
base::Lock* g_non_crash_dump_lock;

// Where we store a pointer to the context information when taking a non-crash
// dump.
crashpad::ExceptionInformation g_non_crash_exception_information;

// A CRITICAL_SECTION initialized with
// RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO to force it to be allocated with a
// valid .DebugInfo field. The address of this critical section is given to the
// handler. All critical sections with debug info are linked in a doubly-linked
// list, so this allows the handler to capture all of them.
CRITICAL_SECTION g_critical_section_with_debug_info;

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exception_pointers) {
  // Tracks whether a thread has already entered UnhandledExceptionHandler.
  static base::subtle::AtomicWord have_crashed;

  // This is a per-process handler. While this handler is being invoked, other
  // threads are still executing as usual, so multiple threads could enter at
  // the same time. Because we're in a crashing state, we shouldn't be doing
  // anything that might cause allocations, call into kernel mode, etc. So, we
  // don't want to take a critical section here to avoid simultaneous access to
  // the global exception pointers in ExceptionInformation. Because the crash
  // handler will record all threads, it's fine to simply have the second and
  // subsequent entrants block here. They will soon be suspended by the crash
  // handler, and then the entire process will be terminated below. This means
  // that we won't save the exception pointers from the second and further
  // crashes, but contention here is very unlikely, and we'll still have a stack
  // that's blocked at this location.
  if (base::subtle::Barrier_AtomicIncrement(&have_crashed, 1) > 1) {
    SleepEx(INFINITE, false);
  }

  // Otherwise, we're the first thread, so record the exception pointer and
  // signal the crash handler.
  g_crash_exception_information.thread_id = GetCurrentThreadId();
  g_crash_exception_information.exception_pointers =
      reinterpret_cast<crashpad::WinVMAddress>(exception_pointers);

  // Now signal the crash server, which will take a dump and then terminate us
  // when it's complete.
  SetEvent(g_signal_exception);

  // Time to wait for the handler to create a dump.
  const DWORD kMillisecondsUntilTerminate = 60 * 1000;

  // Sleep for a while to allow it to process us. Eventually, we terminate
  // ourselves in case the crash server is gone, so that we don't leave zombies
  // around. This would ideally never happen.
  Sleep(kMillisecondsUntilTerminate);

  LOG(ERROR) << "crash server did not respond, self-terminating";

  const UINT kCrashExitCodeNoDump = 0xffff7001;
  TerminateProcess(GetCurrentProcess(), kCrashExitCodeNoDump);

  return EXCEPTION_CONTINUE_SEARCH;
}

std::wstring FormatArgumentString(const std::string& name,
                                  const std::wstring& value) {
  return std::wstring(L"--") + base::UTF8ToUTF16(name) + L"=" + value;
}

struct ScopedProcThreadAttributeListTraits {
  static PPROC_THREAD_ATTRIBUTE_LIST InvalidValue() {
    return nullptr;
  }

  static void Free(PPROC_THREAD_ATTRIBUTE_LIST proc_thread_attribute_list) {
    // This is able to use GET_FUNCTION_REQUIRED() instead of GET_FUNCTION()
    // because it will only be called if InitializeProcThreadAttributeList() and
    // UpdateProcThreadAttribute() are present.
    static const auto delete_proc_thread_attribute_list =
        GET_FUNCTION_REQUIRED(L"kernel32.dll", ::DeleteProcThreadAttributeList);
    delete_proc_thread_attribute_list(proc_thread_attribute_list);
  }
};

using ScopedProcThreadAttributeList =
    base::ScopedGeneric<PPROC_THREAD_ATTRIBUTE_LIST,
                        ScopedProcThreadAttributeListTraits>;

bool IsInheritableHandle(HANDLE handle) {
  if (!handle || handle == INVALID_HANDLE_VALUE)
    return false;

  // File handles (FILE_TYPE_DISK) and pipe handles (FILE_TYPE_PIPE) are known
  // to be inheritable. Console handles (FILE_TYPE_CHAR) are not inheritable via
  // PROC_THREAD_ATTRIBUTE_HANDLE_LIST. See
  // https://crashpad.chromium.org/bug/77.
  DWORD handle_type = GetFileType(handle);
  return handle_type == FILE_TYPE_DISK || handle_type == FILE_TYPE_PIPE;
}

// Adds |handle| to |handle_list| if it appears valid, and is not already in
// |handle_list|.
//
// Invalid handles (including INVALID_HANDLE_VALUE and null handles) cannot be
// added to a PPROC_THREAD_ATTRIBUTE_LIST’s PROC_THREAD_ATTRIBUTE_HANDLE_LIST.
// If INVALID_HANDLE_VALUE appears, CreateProcess() will fail with
// ERROR_INVALID_PARAMETER. If a null handle appears, the child process will
// silently not inherit any handles.
//
// Use this function to add handles with uncertain validities.
void AddHandleToListIfValidAndInheritable(std::vector<HANDLE>* handle_list,
                                          HANDLE handle) {
  // There doesn't seem to be any documentation of this, but if there's a handle
  // duplicated in this list, CreateProcess() fails with
  // ERROR_INVALID_PARAMETER.
  if (IsInheritableHandle(handle) &&
      std::find(handle_list->begin(), handle_list->end(), handle) ==
          handle_list->end()) {
    handle_list->push_back(handle);
  }
}

void AddUint32(std::vector<unsigned char>* data_vector, uint32_t data) {
  data_vector->push_back(static_cast<unsigned char>(data & 0xff));
  data_vector->push_back(static_cast<unsigned char>((data & 0xff00) >> 8));
  data_vector->push_back(static_cast<unsigned char>((data & 0xff0000) >> 16));
  data_vector->push_back(static_cast<unsigned char>((data & 0xff000000) >> 24));
}

void AddUint64(std::vector<unsigned char>* data_vector, uint64_t data) {
  AddUint32(data_vector, static_cast<uint32_t>(data & 0xffffffffULL));
  AddUint32(data_vector,
            static_cast<uint32_t>((data & 0xffffffff00000000ULL) >> 32));
}

}  // namespace

namespace crashpad {

CrashpadClient::CrashpadClient()
    : ipc_pipe_() {
}

CrashpadClient::~CrashpadClient() {
}

bool CrashpadClient::StartHandler(
    const base::FilePath& handler,
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    bool restartable) {
  DCHECK(ipc_pipe_.empty());

  HANDLE pipe_read;
  HANDLE pipe_write;
  SECURITY_ATTRIBUTES security_attributes = {};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = TRUE;
  if (!CreatePipe(&pipe_read, &pipe_write, &security_attributes, 0)) {
    PLOG(ERROR) << "CreatePipe";
    return false;
  }
  ScopedFileHandle pipe_read_owner(pipe_read);
  ScopedFileHandle pipe_write_owner(pipe_write);

  // The new process only needs the write side of the pipe.
  BOOL rv = SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0);
  PLOG_IF(WARNING, !rv) << "SetHandleInformation";

  std::wstring command_line;
  AppendCommandLineArgument(handler.value(), &command_line);
  for (const std::string& argument : arguments) {
    AppendCommandLineArgument(base::UTF8ToUTF16(argument), &command_line);
  }
  if (!database.value().empty()) {
    AppendCommandLineArgument(FormatArgumentString("database",
                                                   database.value()),
                              &command_line);
  }
  if (!url.empty()) {
    AppendCommandLineArgument(FormatArgumentString("url",
                                                   base::UTF8ToUTF16(url)),
                              &command_line);
  }
  for (const auto& kv : annotations) {
    AppendCommandLineArgument(
        FormatArgumentString("annotation",
                             base::UTF8ToUTF16(kv.first + '=' + kv.second)),
        &command_line);
  }
  AppendCommandLineArgument(
      base::UTF8ToUTF16(base::StringPrintf("--handshake-handle=0x%x",
                                           HandleToInt(pipe_write))),
      &command_line);

  DWORD creation_flags;
  STARTUPINFOEX startup_info = {};
  startup_info.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup_info.StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup_info.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startup_info.StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);

  std::vector<HANDLE> handle_list;
  std::unique_ptr<uint8_t[]> proc_thread_attribute_list_storage;
  ScopedProcThreadAttributeList proc_thread_attribute_list_owner;

  static const auto initialize_proc_thread_attribute_list =
      GET_FUNCTION(L"kernel32.dll", ::InitializeProcThreadAttributeList);
  static const auto update_proc_thread_attribute =
      initialize_proc_thread_attribute_list
          ? GET_FUNCTION(L"kernel32.dll", ::UpdateProcThreadAttribute)
          : nullptr;
  if (!initialize_proc_thread_attribute_list || !update_proc_thread_attribute) {
    // The OS doesn’t allow handle inheritance to be restricted, so the handler
    // will inherit every inheritable handle.
    creation_flags = 0;
    startup_info.StartupInfo.cb = sizeof(startup_info.StartupInfo);
  } else {
    // Restrict handle inheritance to just those needed in the handler.

    creation_flags = EXTENDED_STARTUPINFO_PRESENT;
    startup_info.StartupInfo.cb = sizeof(startup_info);
    SIZE_T size;
    rv = initialize_proc_thread_attribute_list(nullptr, 1, 0, &size);
    if (rv) {
      LOG(ERROR) << "InitializeProcThreadAttributeList (size) succeeded, "
                    "expected failure";
      return false;
    } else if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      PLOG(ERROR) << "InitializeProcThreadAttributeList (size)";
      return false;
    }

    proc_thread_attribute_list_storage.reset(new uint8_t[size]);
    startup_info.lpAttributeList =
        reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(
            proc_thread_attribute_list_storage.get());
    rv = initialize_proc_thread_attribute_list(
        startup_info.lpAttributeList, 1, 0, &size);
    if (!rv) {
      PLOG(ERROR) << "InitializeProcThreadAttributeList";
      return false;
    }
    proc_thread_attribute_list_owner.reset(startup_info.lpAttributeList);

    handle_list.reserve(4);
    handle_list.push_back(pipe_write);
    AddHandleToListIfValidAndInheritable(&handle_list,
                                         startup_info.StartupInfo.hStdInput);
    AddHandleToListIfValidAndInheritable(&handle_list,
                                         startup_info.StartupInfo.hStdOutput);
    AddHandleToListIfValidAndInheritable(&handle_list,
                                         startup_info.StartupInfo.hStdError);
    rv = update_proc_thread_attribute(
        startup_info.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        &handle_list[0],
        handle_list.size() * sizeof(handle_list[0]),
        nullptr,
        nullptr);
    if (!rv) {
      PLOG(ERROR) << "UpdateProcThreadAttribute";
      return false;
    }
  }

  PROCESS_INFORMATION process_info;
  rv = CreateProcess(handler.value().c_str(),
                     &command_line[0],
                     nullptr,
                     nullptr,
                     true,
                     creation_flags,
                     nullptr,
                     nullptr,
                     &startup_info.StartupInfo,
                     &process_info);
  if (!rv) {
    PLOG(ERROR) << "CreateProcess";
    return false;
  }

  rv = CloseHandle(process_info.hThread);
  PLOG_IF(WARNING, !rv) << "CloseHandle thread";

  rv = CloseHandle(process_info.hProcess);
  PLOG_IF(WARNING, !rv) << "CloseHandle process";

  pipe_write_owner.reset();

  uint32_t ipc_pipe_length;
  if (!LoggingReadFile(pipe_read, &ipc_pipe_length, sizeof(ipc_pipe_length))) {
    return false;
  }

  ipc_pipe_.resize(ipc_pipe_length);
  if (ipc_pipe_length &&
      !LoggingReadFile(
          pipe_read, &ipc_pipe_[0], ipc_pipe_length * sizeof(ipc_pipe_[0]))) {
    return false;
  }

  return true;
}

bool CrashpadClient::SetHandlerIPCPipe(const std::wstring& ipc_pipe) {
  DCHECK(ipc_pipe_.empty());
  DCHECK(!ipc_pipe.empty());

  ipc_pipe_ = ipc_pipe;

  return true;
}

std::wstring CrashpadClient::GetHandlerIPCPipe() const {
  DCHECK(!ipc_pipe_.empty());
  return ipc_pipe_;
}

bool CrashpadClient::UseHandler() {
  DCHECK(!ipc_pipe_.empty());
  DCHECK_EQ(g_signal_exception, INVALID_HANDLE_VALUE);
  DCHECK_EQ(g_signal_non_crash_dump, INVALID_HANDLE_VALUE);
  DCHECK_EQ(g_non_crash_dump_done, INVALID_HANDLE_VALUE);
  DCHECK(!g_critical_section_with_debug_info.DebugInfo);
  DCHECK(!g_non_crash_dump_lock);

  ClientToServerMessage message;
  memset(&message, 0, sizeof(message));
  message.type = ClientToServerMessage::kRegister;
  message.registration.version = RegistrationRequest::kMessageVersion;
  message.registration.client_process_id = GetCurrentProcessId();
  message.registration.crash_exception_information =
      reinterpret_cast<WinVMAddress>(&g_crash_exception_information);
  message.registration.non_crash_exception_information =
      reinterpret_cast<WinVMAddress>(&g_non_crash_exception_information);

  // We create this dummy CRITICAL_SECTION with the
  // RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO flag set to have an entry point
  // into the doubly-linked list of RTL_CRITICAL_SECTION_DEBUG objects. This
  // allows us to walk the list at crash time to gather data for !locks. A
  // debugger would instead inspect ntdll!RtlCriticalSectionList to get the head
  // of the list. But that is not an exported symbol, so on an arbitrary client
  // machine, we don't have a way of getting that pointer.
  if (InitializeCriticalSectionWithDebugInfoIfPossible(
          &g_critical_section_with_debug_info)) {
    message.registration.critical_section_address =
        reinterpret_cast<WinVMAddress>(&g_critical_section_with_debug_info);
  }

  ServerToClientMessage response = {};

  if (!SendToCrashHandlerServer(ipc_pipe_, message, &response)) {
    return false;
  }

  // The server returns these already duplicated to be valid in this process.
  g_signal_exception =
      IntToHandle(response.registration.request_crash_dump_event);
  g_signal_non_crash_dump =
      IntToHandle(response.registration.request_non_crash_dump_event);
  g_non_crash_dump_done =
      IntToHandle(response.registration.non_crash_dump_completed_event);

  g_non_crash_dump_lock = new base::Lock();

  // In theory we could store the previous handler but it is not clear what
  // use we have for it.
  SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
  return true;
}

// static
void CrashpadClient::DumpWithoutCrash(const CONTEXT& context) {
  if (g_signal_non_crash_dump == INVALID_HANDLE_VALUE ||
      g_non_crash_dump_done == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "haven't called UseHandler()";
    return;
  }

  // In the non-crashing case, we aren't concerned about avoiding calls into
  // Win32 APIs, so just use regular locking here in case of multiple threads
  // calling this function. If a crash occurs while we're in here, the worst
  // that can happen is that the server captures a partial dump for this path
  // because on the other thread gathering a crash dump, it TerminateProcess()d,
  // causing this one to abort.
  base::AutoLock lock(*g_non_crash_dump_lock);

  // Create a fake EXCEPTION_POINTERS to give the handler something to work
  // with.
  EXCEPTION_POINTERS exception_pointers = {};

  // This is logically const, but EXCEPTION_POINTERS does not declare it as
  // const, so we have to cast that away from the argument.
  exception_pointers.ContextRecord = const_cast<CONTEXT*>(&context);

  // We include a fake exception and use a code of '0x517a7ed' (something like
  // "simulated") so that it's relatively obvious in windbg that it's not
  // actually an exception. Most values in
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa363082.aspx have
  // some of the top nibble set, so we make sure to pick a value that doesn't,
  // so as to be unlikely to conflict.
  const uint32_t kSimulatedExceptionCode = 0x517a7ed;
  EXCEPTION_RECORD record = {};
  record.ExceptionCode = kSimulatedExceptionCode;
#if defined(ARCH_CPU_64_BITS)
  record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
#else
  record.ExceptionAddress = reinterpret_cast<void*>(context.Eip);
#endif  // ARCH_CPU_64_BITS

  exception_pointers.ExceptionRecord = &record;

  g_non_crash_exception_information.thread_id = GetCurrentThreadId();
  g_non_crash_exception_information.exception_pointers =
      reinterpret_cast<crashpad::WinVMAddress>(&exception_pointers);

  bool set_event_result = !!SetEvent(g_signal_non_crash_dump);
  PLOG_IF(ERROR, !set_event_result) << "SetEvent";

  DWORD wfso_result = WaitForSingleObject(g_non_crash_dump_done, INFINITE);
  PLOG_IF(ERROR, wfso_result != WAIT_OBJECT_0) << "WaitForSingleObject";
}

// static
void CrashpadClient::DumpAndCrash(EXCEPTION_POINTERS* exception_pointers) {
  if (g_signal_exception == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "haven't called UseHandler()";
    return;
  }

  UnhandledExceptionHandler(exception_pointers);
}

bool CrashpadClient::DumpAndCrashTargetProcess(HANDLE process,
                                               HANDLE blame_thread,
                                               DWORD exception_code) const {
  // Confirm we're on Vista or later.
  const DWORD version = GetVersion();
  const DWORD major_version = LOBYTE(LOWORD(version));
  if (major_version < 6) {
    LOG(ERROR) << "unavailable before Vista";
    return false;
  }

  // Confirm that our bitness is the same as the process we're crashing.
  ProcessInfo process_info;
  if (!process_info.Initialize(process)) {
    LOG(ERROR) << "ProcessInfo::Initialize";
    return false;
  }
#if defined(ARCH_CPU_64_BITS)
  if (!process_info.Is64Bit()) {
    LOG(ERROR) << "DumpAndCrashTargetProcess currently not supported x64->x86";
    return false;
  }
#endif  // ARCH_CPU_64_BITS

  ScopedProcessSuspend suspend(process);

  // If no thread handle was provided, or the thread has already exited, we pass
  // 0 to the handler, which indicates no fake exception record to be created.
  DWORD thread_id = 0;
  if (blame_thread) {
    // Now that we've suspended the process, if our thread hasn't exited, we
    // know we're relatively safe to pass the thread id through.
    if (WaitForSingleObject(blame_thread, 0) == WAIT_TIMEOUT) {
      static const auto get_thread_id =
          GET_FUNCTION_REQUIRED(L"kernel32.dll", ::GetThreadId);
      thread_id = get_thread_id(blame_thread);
    }
  }

  const size_t kInjectBufferSize = 4 * 1024;
  WinVMAddress inject_memory =
      reinterpret_cast<WinVMAddress>(VirtualAllocEx(process,
                                                    nullptr,
                                                    kInjectBufferSize,
                                                    MEM_RESERVE | MEM_COMMIT,
                                                    PAGE_READWRITE));
  if (!inject_memory) {
    PLOG(ERROR) << "VirtualAllocEx";
    return false;
  }

  // Because we're the same bitness as our target, we can rely kernel32 being
  // loaded at the same address in our process as the target, and just look up
  // its address here.
  WinVMAddress raise_exception_address =
      reinterpret_cast<WinVMAddress>(&RaiseException);

  WinVMAddress code_entry_point = 0;
  std::vector<unsigned char> data_to_write;
  if (process_info.Is64Bit()) {
    // Data written is first, the data for the 4th argument (lpArguments) to
    // RaiseException(). A two element array:
    //
    // DWORD64: thread_id
    // DWORD64: exception_code
    //
    // Following that, code which sets the arguments to RaiseException() and
    // then calls it:
    //
    // mov r9, <data_array_address>
    // mov r8d, 2  ; nNumberOfArguments
    // mov edx, 1  ; dwExceptionFlags = EXCEPTION_NONCONTINUABLE
    // mov ecx, 0xcca11ed  ; dwExceptionCode, interpreted specially by the
    //                     ;                  handler.
    // jmp <address_of_RaiseException>
    //
    // Note that the first three arguments to RaiseException() are DWORDs even
    // on x64, so only the 4th argument (a pointer) is a full-width register.
    //
    // We also don't need to set up a stack or use call, since the only
    // registers modified are volatile ones, and we can just jmp straight to
    // RaiseException().

    // The data array.
    AddUint64(&data_to_write, thread_id);
    AddUint64(&data_to_write, exception_code);

    // The thread entry point.
    code_entry_point = inject_memory + data_to_write.size();

    // r9 = pointer to data.
    data_to_write.push_back(0x49);
    data_to_write.push_back(0xb9);
    AddUint64(&data_to_write, inject_memory);

    // r8d = 2 for nNumberOfArguments.
    data_to_write.push_back(0x41);
    data_to_write.push_back(0xb8);
    AddUint32(&data_to_write, 2);

    // edx = 1 for dwExceptionFlags.
    data_to_write.push_back(0xba);
    AddUint32(&data_to_write, 1);

    // ecx = kTriggeredExceptionCode for dwExceptionCode.
    data_to_write.push_back(0xb9);
    AddUint32(&data_to_write, kTriggeredExceptionCode);

    // jmp to RaiseException() via rax.
    data_to_write.push_back(0x48);  // mov rax, imm.
    data_to_write.push_back(0xb8);
    AddUint64(&data_to_write, raise_exception_address);
    data_to_write.push_back(0xff);  // jmp rax.
    data_to_write.push_back(0xe0);
  } else {
    // Data written is first, the data for the 4th argument (lpArguments) to
    // RaiseException(). A two element array:
    //
    // DWORD: thread_id
    // DWORD: exception_code
    //
    // Following that, code which pushes our arguments to RaiseException() and
    // then calls it:
    //
    // push <data_array_address>
    // push 2  ; nNumberOfArguments
    // push 1  ; dwExceptionFlags = EXCEPTION_NONCONTINUABLE
    // push 0xcca11ed  ; dwExceptionCode, interpreted specially by the handler.
    // call <address_of_RaiseException>
    // ud2  ; Generate invalid opcode to make sure we still crash if we return
    //      ; for some reason.
    //
    // No need to clean up the stack, as RaiseException() is __stdcall.

    // The data array.
    AddUint32(&data_to_write, thread_id);
    AddUint32(&data_to_write, exception_code);

    // The thread entry point.
    code_entry_point = inject_memory + data_to_write.size();

    // Push data address.
    data_to_write.push_back(0x68);
    AddUint32(&data_to_write, static_cast<uint32_t>(inject_memory));

    // Push 2 for nNumberOfArguments.
    data_to_write.push_back(0x6a);
    data_to_write.push_back(2);

    // Push 1 for dwExceptionCode.
    data_to_write.push_back(0x6a);
    data_to_write.push_back(1);

    // Push dwExceptionFlags.
    data_to_write.push_back(0x68);
    AddUint32(&data_to_write, kTriggeredExceptionCode);

    // Relative call to RaiseException().
    int64_t relative_address_to_raise_exception =
        raise_exception_address - (inject_memory + data_to_write.size() + 5);
    data_to_write.push_back(0xe8);
    AddUint32(&data_to_write,
              static_cast<uint32_t>(relative_address_to_raise_exception));

    // ud2.
    data_to_write.push_back(0x0f);
    data_to_write.push_back(0x0b);
  }

  DCHECK_LT(data_to_write.size(), kInjectBufferSize);

  SIZE_T bytes_written;
  if (!WriteProcessMemory(process,
                          reinterpret_cast<void*>(inject_memory),
                          data_to_write.data(),
                          data_to_write.size(),
                          &bytes_written)) {
    PLOG(ERROR) << "WriteProcessMemory";
    return false;
  }

  if (bytes_written != data_to_write.size()) {
    LOG(ERROR) << "WriteProcessMemory unexpected number of bytes";
    return false;
  }

  if (!FlushInstructionCache(
          process, reinterpret_cast<void*>(inject_memory), bytes_written)) {
    PLOG(ERROR) << "FlushInstructionCache";
    return false;
  }

  DWORD old_protect;
  if (!VirtualProtectEx(process,
                        reinterpret_cast<void*>(inject_memory),
                        kInjectBufferSize,
                        PAGE_EXECUTE_READ,
                        &old_protect)) {
    PLOG(ERROR) << "VirtualProtectEx";
    return false;
  }

  // Cause an exception in the target process by creating a thread which calls
  // RaiseException with our arguments above. Note that we cannot get away with
  // using DebugBreakProcess() (nothing happens unless a debugger is attached)
  // and we cannot get away with CreateRemoteThread() because it doesn't work if
  // the target is hung waiting for the loader lock. We use NtCreateThreadEx()
  // with the SKIP_THREAD_ATTACH flag, which skips various notifications,
  // letting this cause an exception, even when the target is stuck in the
  // loader lock.
  HANDLE injected_thread;
  const size_t kStackSize = 0x4000;  // This is what DebugBreakProcess() uses.
  NTSTATUS status = NtCreateThreadEx(&injected_thread,
                                     STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
                                     nullptr,
                                     process,
                                     reinterpret_cast<void*>(code_entry_point),
                                     nullptr,
                                     THREAD_CREATE_FLAGS_SKIP_THREAD_ATTACH,
                                     0,
                                     kStackSize,
                                     0,
                                     nullptr);
  if (!NT_SUCCESS(status)) {
    NTSTATUS_LOG(ERROR, status) << "NtCreateThreadEx";
    return false;
  }

  bool result = true;
  if (WaitForSingleObject(injected_thread, 60 * 1000) != WAIT_OBJECT_0) {
    PLOG(ERROR) << "WaitForSingleObject";
    result = false;
  }

  status = NtClose(injected_thread);
  if (!NT_SUCCESS(status)) {
    NTSTATUS_LOG(ERROR, status) << "NtClose";
    result = false;
  }

  return result;
}

}  // namespace crashpad
