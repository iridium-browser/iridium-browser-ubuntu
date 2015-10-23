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

#include "snapshot/win/thread_snapshot_win.h"

#include "base/logging.h"
#include "snapshot/win/cpu_context_win.h"
#include "snapshot/win/process_reader_win.h"

namespace crashpad {
namespace internal {

ThreadSnapshotWin::ThreadSnapshotWin()
    : ThreadSnapshot(), context_(), stack_(), thread_(), initialized_() {
}

ThreadSnapshotWin::~ThreadSnapshotWin() {
}

bool ThreadSnapshotWin::Initialize(
    ProcessReaderWin* process_reader,
    const ProcessReaderWin::Thread& process_reader_thread) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  thread_ = process_reader_thread;
  stack_.Initialize(
      process_reader, thread_.stack_region_address, thread_.stack_region_size);

#if defined(ARCH_CPU_X86_64)
  if (process_reader->Is64Bit()) {
    context_.architecture = kCPUArchitectureX86_64;
    context_.x86_64 = &context_union_.x86_64;
    InitializeX64Context(process_reader_thread.context, context_.x86_64);
  } else {
    context_.architecture = kCPUArchitectureX86;
    context_.x86 = &context_union_.x86;
    InitializeX86Context(
        *reinterpret_cast<const WOW64_CONTEXT*>(&process_reader_thread.context),
        context_.x86);
  }
#else
#error ARCH_CPU_X86
#endif  // ARCH_CPU_X86_64

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ThreadSnapshotWin::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

const MemorySnapshot* ThreadSnapshotWin::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

uint64_t ThreadSnapshotWin::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.id;
}

int ThreadSnapshotWin::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.suspend_count;
}

int ThreadSnapshotWin::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.priority;
}

uint64_t ThreadSnapshotWin::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.teb;
}

}  // namespace internal
}  // namespace crashpad
