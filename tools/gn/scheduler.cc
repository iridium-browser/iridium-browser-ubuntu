// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/scheduler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/switches.h"

Scheduler* g_scheduler = nullptr;

namespace {

int GetThreadCount() {
  std::string thread_count =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kThreads);

  int result;
  if (thread_count.empty() || !base::StringToInt(thread_count, &result))
    return 32;
  return result;
}

}  // namespace

Scheduler::Scheduler()
    : pool_(new base::SequencedWorkerPool(GetThreadCount(), "worker_")),
      input_file_manager_(new InputFileManager),
      verbose_logging_(false),
      work_count_(0),
      is_failed_(false),
      has_been_shutdown_(false) {
  g_scheduler = this;
}

Scheduler::~Scheduler() {
  if (!has_been_shutdown_)
    pool_->Shutdown();
  g_scheduler = nullptr;
}

bool Scheduler::Run() {
  runner_.Run();
  bool local_is_failed;
  {
    base::AutoLock lock(lock_);
    local_is_failed = is_failed();
    has_been_shutdown_ = true;
  }
  // Don't do this inside the lock since it will block on the workers, which
  // may be in turn waiting on the lock.
  pool_->Shutdown();
  return !local_is_failed;
}

void Scheduler::Log(const std::string& verb, const std::string& msg) {
  if (base::MessageLoop::current() == &main_loop_) {
    LogOnMainThread(verb, msg);
  } else {
    // The run loop always joins on the sub threads, so the lifetime of this
    // object outlives the invocations of this function, hence "unretained".
    main_loop_.PostTask(FROM_HERE,
                        base::Bind(&Scheduler::LogOnMainThread,
                                   base::Unretained(this), verb, msg));
  }
}

void Scheduler::FailWithError(const Err& err) {
  DCHECK(err.has_error());
  {
    base::AutoLock lock(lock_);

    if (is_failed_ || has_been_shutdown_)
      return;  // Ignore errors once we see one.
    is_failed_ = true;
  }

  if (base::MessageLoop::current() == &main_loop_) {
    FailWithErrorOnMainThread(err);
  } else {
    // The run loop always joins on the sub threads, so the lifetime of this
    // object outlives the invocations of this function, hence "unretained".
    main_loop_.PostTask(FROM_HERE,
                        base::Bind(&Scheduler::FailWithErrorOnMainThread,
                                   base::Unretained(this), err));
  }
}

void Scheduler::ScheduleWork(const base::Closure& work) {
  IncrementWorkCount();
  pool_->PostWorkerTaskWithShutdownBehavior(
      FROM_HERE, base::Bind(&Scheduler::DoWork,
                            base::Unretained(this), work),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

void Scheduler::AddGenDependency(const base::FilePath& file) {
  base::AutoLock lock(lock_);
  gen_dependencies_.push_back(file);
}

std::vector<base::FilePath> Scheduler::GetGenDependencies() const {
  base::AutoLock lock(lock_);
  return gen_dependencies_;
}

void Scheduler::AddWrittenFile(const SourceFile& file) {
  base::AutoLock lock(lock_);
  written_files_.push_back(file);
}

std::vector<SourceFile> Scheduler::GetWrittenFiles() const {
  base::AutoLock lock(lock_);
  return written_files_;
}

void Scheduler::AddUnknownGeneratedInput(const Target* target,
                                         const SourceFile& file) {
  base::AutoLock lock(lock_);
  unknown_generated_inputs_.insert(std::make_pair(file, target));
}

std::multimap<SourceFile, const Target*>
    Scheduler::GetUnknownGeneratedInputs() const {
  base::AutoLock lock(lock_);

  // Remove all unknown inputs that were written files. These are OK as inputs
  // to build steps since they were written as a side-effect of running GN.
  //
  // It's assumed that this function is called once during cleanup to check for
  // errors, so performing this work in the lock doesn't matter.
  std::multimap<SourceFile, const Target*> filtered = unknown_generated_inputs_;
  for (const SourceFile& file : written_files_)
    filtered.erase(file);

  return filtered;
}

void Scheduler::ClearUnknownGeneratedInputsAndWrittenFiles() {
  base::AutoLock lock(lock_);
  unknown_generated_inputs_.clear();
  written_files_.clear();
}

void Scheduler::IncrementWorkCount() {
  base::AtomicRefCountInc(&work_count_);
}

void Scheduler::DecrementWorkCount() {
  if (!base::AtomicRefCountDec(&work_count_)) {
    if (base::MessageLoop::current() == &main_loop_) {
      OnComplete();
    } else {
      main_loop_.PostTask(FROM_HERE,
                          base::Bind(&Scheduler::OnComplete,
                                     base::Unretained(this)));
    }
  }
}

void Scheduler::LogOnMainThread(const std::string& verb,
                                const std::string& msg) {
  OutputString(verb, DECORATION_YELLOW);
  OutputString(" " + msg + "\n");
}

void Scheduler::FailWithErrorOnMainThread(const Err& err) {
  err.PrintToStdout();
  runner_.Quit();
}

void Scheduler::DoWork(const base::Closure& closure) {
  closure.Run();
  DecrementWorkCount();
}

void Scheduler::OnComplete() {
  // Should be called on the main thread.
  DCHECK(base::MessageLoop::current() == main_loop());
  runner_.Quit();
}
