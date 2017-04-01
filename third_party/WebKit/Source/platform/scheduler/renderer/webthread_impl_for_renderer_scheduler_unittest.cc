// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"

#include <stddef.h>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {
namespace scheduler {
namespace {

const int kWorkBatchSize = 2;

class MockTask {
 public:
  MOCK_METHOD0(run, void());
};

class MockTaskObserver : public blink::WebThread::TaskObserver {
 public:
  MOCK_METHOD0(willProcessTask, void());
  MOCK_METHOD0(didProcessTask, void());
};
}  // namespace

class WebThreadImplForRendererSchedulerTest : public testing::Test {
 public:
  WebThreadImplForRendererSchedulerTest() {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    scheduler_.reset(new RendererSchedulerImpl(SchedulerTqmDelegateImpl::Create(
        &message_loop_, base::MakeUnique<TestTimeSource>(clock_.get()))));
    default_task_runner_ = scheduler_->DefaultTaskRunner();
    thread_ = scheduler_->CreateMainThread();
  }

  ~WebThreadImplForRendererSchedulerTest() override {}

  void SetWorkBatchSizeForTesting(size_t work_batch_size) {
    scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(
        work_batch_size);
  }

  void TearDown() override { scheduler_->Shutdown(); }

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  std::unique_ptr<blink::WebThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(WebThreadImplForRendererSchedulerTest);
};

TEST_F(WebThreadImplForRendererSchedulerTest, TestTaskObserver) {
  MockTaskObserver observer;
  thread_->addTaskObserver(&observer);
  MockTask task;

  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(task, run());
    EXPECT_CALL(observer, didProcessTask());
  }

  thread_->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, WTF::bind(&MockTask::run, WTF::unretained(&task)));
  base::RunLoop().RunUntilIdle();
  thread_->removeTaskObserver(&observer);
}

TEST_F(WebThreadImplForRendererSchedulerTest, TestWorkBatchWithOneTask) {
  MockTaskObserver observer;
  thread_->addTaskObserver(&observer);
  MockTask task;

  SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(task, run());
    EXPECT_CALL(observer, didProcessTask());
  }

  thread_->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, WTF::bind(&MockTask::run, WTF::unretained(&task)));
  base::RunLoop().RunUntilIdle();
  thread_->removeTaskObserver(&observer);
}

TEST_F(WebThreadImplForRendererSchedulerTest, TestWorkBatchWithTwoTasks) {
  MockTaskObserver observer;
  thread_->addTaskObserver(&observer);
  MockTask task1;
  MockTask task2;

  SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(task1, run());
    EXPECT_CALL(observer, didProcessTask());

    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(task2, run());
    EXPECT_CALL(observer, didProcessTask());
  }

  thread_->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, WTF::bind(&MockTask::run, WTF::unretained(&task1)));
  thread_->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, WTF::bind(&MockTask::run, WTF::unretained(&task2)));
  base::RunLoop().RunUntilIdle();
  thread_->removeTaskObserver(&observer);
}

TEST_F(WebThreadImplForRendererSchedulerTest, TestWorkBatchWithThreeTasks) {
  MockTaskObserver observer;
  thread_->addTaskObserver(&observer);
  MockTask task1;
  MockTask task2;
  MockTask task3;

  SetWorkBatchSizeForTesting(kWorkBatchSize);
  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(task1, run());
    EXPECT_CALL(observer, didProcessTask());

    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(task2, run());
    EXPECT_CALL(observer, didProcessTask());

    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(task3, run());
    EXPECT_CALL(observer, didProcessTask());
  }

  thread_->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, WTF::bind(&MockTask::run, WTF::unretained(&task1)));
  thread_->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, WTF::bind(&MockTask::run, WTF::unretained(&task2)));
  thread_->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE, WTF::bind(&MockTask::run, WTF::unretained(&task3)));
  base::RunLoop().RunUntilIdle();
  thread_->removeTaskObserver(&observer);
}

void EnterRunLoop(base::MessageLoop* message_loop, blink::WebThread* thread) {
  // Note: WebThreads do not support nested run loops, which is why we use a
  // run loop directly.
  base::RunLoop run_loop;
  thread->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE,
      WTF::bind(&base::RunLoop::Quit, WTF::unretained(&run_loop)));
  message_loop->SetNestableTasksAllowed(true);
  run_loop.Run();
}

TEST_F(WebThreadImplForRendererSchedulerTest, TestNestedRunLoop) {
  MockTaskObserver observer;
  thread_->addTaskObserver(&observer);

  {
    testing::InSequence sequence;

    // One callback for EnterRunLoop.
    EXPECT_CALL(observer, willProcessTask());

    // A pair for ExitRunLoopTask.
    EXPECT_CALL(observer, willProcessTask());
    EXPECT_CALL(observer, didProcessTask());

    // A final callback for EnterRunLoop.
    EXPECT_CALL(observer, didProcessTask());
  }

  message_loop_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&EnterRunLoop, base::Unretained(&message_loop_),
                            base::Unretained(thread_.get())));
  base::RunLoop().RunUntilIdle();
  thread_->removeTaskObserver(&observer);
}

}  // namespace scheduler
}  // namespace blink
