// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/WebTaskRunner.h"

#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"

namespace base {

using RunnerMethodType =
    void (blink::TaskHandle::Runner::*)(const blink::TaskHandle&);

template <>
struct CallbackCancellationTraits<
    RunnerMethodType,
    std::tuple<WTF::WeakPtr<blink::TaskHandle::Runner>, blink::TaskHandle>> {
  static constexpr bool is_cancellable = true;

  static bool IsCancelled(RunnerMethodType,
                          const WTF::WeakPtr<blink::TaskHandle::Runner>&,
                          const blink::TaskHandle& handle) {
    return !handle.isActive();
  }
};

}  // namespace base

namespace blink {

namespace {

void runCrossThreadClosure(std::unique_ptr<CrossThreadClosure> task) {
  (*task)();
}

}  // namespace

class TaskHandle::Runner : public WTF::ThreadSafeRefCounted<Runner> {
 public:
  explicit Runner(std::unique_ptr<WTF::Closure> task)
      : m_task(std::move(task)), m_weakPtrFactory(this) {}

  WTF::WeakPtr<Runner> asWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

  bool isActive() const { return m_task && !m_task->isCancelled(); }

  void cancel() {
    std::unique_ptr<WTF::Closure> task = std::move(m_task);
    m_weakPtrFactory.revokeAll();
  }

  ~Runner() { cancel(); }

  // The TaskHandle parameter on run() holds a reference to the Runner to keep
  // it alive while a task is pending in a task queue, and clears the reference
  // on the task disposal, so that it doesn't leave a circular reference like
  // below:
  //   struct Foo : GarbageCollected<Foo> {
  //     void bar() {}
  //     TaskHandle m_handle;
  //   };
  //
  //   foo.m_handle = taskRunner->postCancellableTask(
  //       BLINK_FROM_HERE, WTF::bind(&Foo::bar, wrapPersistent(foo)));
  //
  // There is a circular reference in the example above as:
  //   foo -> m_handle -> m_runner -> m_task -> Persistent<Foo> in WTF::bind.
  // The TaskHandle parameter on run() is needed to break the circle by clearing
  // |m_task| when the wrapped WTF::Closure is deleted.
  void run(const TaskHandle&) {
    std::unique_ptr<WTF::Closure> task = std::move(m_task);
    m_weakPtrFactory.revokeAll();
    (*task)();
  }

 private:
  std::unique_ptr<WTF::Closure> m_task;
  WTF::WeakPtrFactory<Runner> m_weakPtrFactory;

  DISALLOW_COPY_AND_ASSIGN(Runner);
};

bool TaskHandle::isActive() const {
  return m_runner && m_runner->isActive();
}

void TaskHandle::cancel() {
  if (m_runner) {
    m_runner->cancel();
    m_runner = nullptr;
  }
}

TaskHandle::TaskHandle() = default;

TaskHandle::~TaskHandle() {
  cancel();
}

TaskHandle::TaskHandle(TaskHandle&&) = default;

TaskHandle& TaskHandle::operator=(TaskHandle&& other) {
  TaskHandle tmp(std::move(other));
  m_runner.swap(tmp.m_runner);
  return *this;
}

TaskHandle::TaskHandle(RefPtr<Runner> runner) : m_runner(std::move(runner)) {
  DCHECK(m_runner);
}

// Use a custom function for base::Bind instead of convertToBaseCallback to
// avoid copying the closure later in the call chain. Copying the bound state
// can lead to data races with ref counted objects like StringImpl. See
// crbug.com/679915 for more details.
void WebTaskRunner::postTask(const WebTraceLocation& location,
                             std::unique_ptr<CrossThreadClosure> task) {
  toSingleThreadTaskRunner()->PostTask(
      location, base::Bind(&runCrossThreadClosure, base::Passed(&task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location,
                                    std::unique_ptr<CrossThreadClosure> task,
                                    long long delayMs) {
  toSingleThreadTaskRunner()->PostDelayedTask(
      location, base::Bind(&runCrossThreadClosure, base::Passed(&task)),
      base::TimeDelta::FromMilliseconds(delayMs));
}

void WebTaskRunner::postTask(const WebTraceLocation& location,
                             std::unique_ptr<WTF::Closure> task) {
  toSingleThreadTaskRunner()->PostTask(location,
                                       convertToBaseCallback(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location,
                                    std::unique_ptr<WTF::Closure> task,
                                    long long delayMs) {
  toSingleThreadTaskRunner()->PostDelayedTask(
      location, convertToBaseCallback(std::move(task)),
      base::TimeDelta::FromMilliseconds(delayMs));
}

TaskHandle WebTaskRunner::postCancellableTask(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::Closure> task) {
  DCHECK(runsTasksOnCurrentThread());
  RefPtr<TaskHandle::Runner> runner =
      adoptRef(new TaskHandle::Runner(std::move(task)));
  postTask(location, WTF::bind(&TaskHandle::Runner::run, runner->asWeakPtr(),
                               TaskHandle(runner)));
  return TaskHandle(runner);
}

TaskHandle WebTaskRunner::postDelayedCancellableTask(
    const WebTraceLocation& location,
    std::unique_ptr<WTF::Closure> task,
    long long delayMs) {
  DCHECK(runsTasksOnCurrentThread());
  RefPtr<TaskHandle::Runner> runner =
      adoptRef(new TaskHandle::Runner(std::move(task)));
  postDelayedTask(location, WTF::bind(&TaskHandle::Runner::run,
                                      runner->asWeakPtr(), TaskHandle(runner)),
                  delayMs);
  return TaskHandle(runner);
}

WebTaskRunner::~WebTaskRunner() = default;

}  // namespace blink
