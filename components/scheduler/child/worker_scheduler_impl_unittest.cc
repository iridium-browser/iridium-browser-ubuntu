// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/worker_scheduler_impl.h"

#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "cc/test/test_now_source.h"
#include "components/scheduler/child/nestable_task_runner_for_test.h"
#include "components/scheduler/child/scheduler_message_loop_delegate.h"
#include "components/scheduler/child/test_time_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;

namespace scheduler {

namespace {
void NopTask() {
}

int TimeTicksToIntMs(const base::TimeTicks& time) {
  return static_cast<int>((time - base::TimeTicks()).InMilliseconds());
}

void WakeUpTask(std::vector<std::string>* timeline, cc::TestNowSource* clock) {
  if (timeline) {
    timeline->push_back(base::StringPrintf("run WakeUpTask @ %d",
                                           TimeTicksToIntMs(clock->Now())));
  }
}

void RecordTimelineTask(std::vector<std::string>* timeline,
                        cc::TestNowSource* clock) {
  timeline->push_back(base::StringPrintf("run RecordTimelineTask @ %d",
                                         TimeTicksToIntMs(clock->Now())));
}

void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(std::vector<std::string>* vector,
                                std::string value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void TimelineIdleTestTask(std::vector<std::string>* timeline,
                          base::TimeTicks deadline) {
  timeline->push_back(base::StringPrintf("run TimelineIdleTestTask deadline %d",
                                         TimeTicksToIntMs(deadline)));
}

};  // namespace

class WorkerSchedulerImplForTest : public WorkerSchedulerImpl {
 public:
  WorkerSchedulerImplForTest(
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
      scoped_refptr<cc::TestNowSource> clock_)
      : WorkerSchedulerImpl(main_task_runner),
        clock_(clock_),
        timeline_(nullptr) {}

  void RecordTimelineEvents(std::vector<std::string>* timeline) {
    timeline_ = timeline;
  }

 private:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override {
    if (timeline_) {
      timeline_->push_back(base::StringPrintf("CanEnterLongIdlePeriod @ %d",
                                              TimeTicksToIntMs(now)));
    }
    return WorkerSchedulerImpl::CanEnterLongIdlePeriod(
        now, next_long_idle_period_delay_out);
  }

  void IsNotQuiescent() override {
    if (timeline_) {
      timeline_->push_back(base::StringPrintf("IsNotQuiescent @ %d",
                                              TimeTicksToIntMs(clock_->Now())));
    }
    WorkerSchedulerImpl::IsNotQuiescent();
  }

  scoped_refptr<cc::TestNowSource> clock_;
  std::vector<std::string>* timeline_;  // NOT OWNED
};

class WorkerSchedulerImplTest : public testing::Test {
 public:
  WorkerSchedulerImplTest()
      : clock_(cc::TestNowSource::Create(5000)),
        mock_task_runner_(new cc::OrderedSimpleTaskRunner(clock_, true)),
        nestable_task_runner_(
            NestableTaskRunnerForTest::Create(mock_task_runner_)),
        scheduler_(
            new WorkerSchedulerImplForTest(nestable_task_runner_, clock_)),
        timeline_(nullptr) {
    scheduler_->GetSchedulerHelperForTesting()->SetTimeSourceForTesting(
        make_scoped_ptr(new TestTimeSource(clock_)));
    scheduler_->GetSchedulerHelperForTesting()
        ->GetTaskQueueManagerForTesting()
        ->SetTimeSourceForTesting(make_scoped_ptr(new TestTimeSource(clock_)));
  }

  ~WorkerSchedulerImplTest() override {}

  void TearDown() override {
    // Check that all tests stop posting tasks.
    while (mock_task_runner_->RunUntilIdle()) {
    }
  }

  void Init() {
    scheduler_->Init();
    default_task_runner_ = scheduler_->DefaultTaskRunner();
    idle_task_runner_ = scheduler_->IdleTaskRunner();
    timeline_ = nullptr;
  }

  void RecordTimelineEvents(std::vector<std::string>* timeline) {
    timeline_ = timeline;
    scheduler_->RecordTimelineEvents(timeline);
  }

  void RunUntilIdle() {
    if (timeline_) {
      timeline_->push_back(base::StringPrintf("RunUntilIdle begin @ %d",
                                              TimeTicksToIntMs(clock_->Now())));
    }
    mock_task_runner_->RunUntilIdle();
    if (timeline_) {
      timeline_->push_back(base::StringPrintf("RunUntilIdle end @ %d",
                                              TimeTicksToIntMs(clock_->Now())));
    }
  }

  void InitAndPostDelayedWakeupTask() {
    Init();
    // WorkerSchedulerImpl::Init causes a delayed task to be posted on the
    // after wakeup control runner.  We need a task to wake the system up
    // AFTER the delay for this has expired.
    default_task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&WakeUpTask, base::Unretained(timeline_),
                              base::Unretained(clock_.get())),
        base::TimeDelta::FromMilliseconds(100));
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type:
  // - 'D': Default task
  // - 'I': Idle task
  void PostTestTasks(std::vector<std::string>* run_order,
                     const std::string& task_descriptor) {
    std::istringstream stream(task_descriptor);
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'D':
          default_task_runner_->PostTask(
              FROM_HERE, base::Bind(&AppendToVectorTestTask, run_order, task));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE,
              base::Bind(&AppendToVectorIdleTestTask, run_order, task));
          break;
        default:
          NOTREACHED();
      }
    }
  }

  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        SchedulerHelper::kMaximumIdlePeriodMillis);
  }

 protected:
  scoped_refptr<cc::TestNowSource> clock_;
  // Only one of mock_task_runner_ or message_loop_ will be set.
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;

  scoped_refptr<NestableSingleThreadTaskRunner> nestable_task_runner_;
  scoped_ptr<WorkerSchedulerImplForTest> scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  std::vector<std::string>* timeline_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(WorkerSchedulerImplTest);
};

TEST_F(WorkerSchedulerImplTest, TestPostDefaultTask) {
  InitAndPostDelayedWakeupTask();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("D3"), std::string("D4")));
}

TEST_F(WorkerSchedulerImplTest, TestPostIdleTask) {
  InitAndPostDelayedWakeupTask();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1");

  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("I1")));
}

TEST_F(WorkerSchedulerImplTest, TestPostIdleTask_NoWakeup) {
  Init();
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1");

  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

TEST_F(WorkerSchedulerImplTest, TestPostDefaultAndIdleTasks) {
  InitAndPostDelayedWakeupTask();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D2"), std::string("D3"),
                                   std::string("D4"), std::string("I1")));
}

TEST_F(WorkerSchedulerImplTest, TestPostIdleTaskWithWakeupNeeded_NoWakeup) {
  InitAndPostDelayedWakeupTask();

  RunUntilIdle();
  // The delayed call to EnableLongIdlePeriod happened and it posted a call to
  // EnableLongIdlePeriod on the after wakeup control queue.

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1");

  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

TEST_F(WorkerSchedulerImplTest, TestPostDefaultDelayedAndIdleTasks) {
  InitAndPostDelayedWakeupTask();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D2 D3 D4");

  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&AppendToVectorTestTask, &run_order, "DELAYED"),
      base::TimeDelta::FromMilliseconds(1000));

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D2"), std::string("D3"),
                                   std::string("D4"), std::string("I1"),
                                   std::string("DELAYED")));
}

TEST_F(WorkerSchedulerImplTest, TestIdleDeadlineWithPendingDelayedTask) {
  std::vector<std::string> timeline;
  RecordTimelineEvents(&timeline);
  InitAndPostDelayedWakeupTask();

  timeline.push_back("Post delayed and idle tasks");
  // Post a delayed task timed to occur mid way during the long idle period.
  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&RecordTimelineTask, base::Unretained(&timeline),
                            base::Unretained(clock_.get())),
      base::TimeDelta::FromMilliseconds(420));
  idle_task_runner_->PostIdleTask(FROM_HERE,
                                  base::Bind(&TimelineIdleTestTask, &timeline));

  RunUntilIdle();

  std::string expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 5",
      "Post delayed and idle tasks",
      "IsNotQuiescent @ 105",
      "CanEnterLongIdlePeriod @ 405",
      "run TimelineIdleTestTask deadline 425",  // Note the short 20ms deadline.
      "CanEnterLongIdlePeriod @ 425",
      "run RecordTimelineTask @ 425"};

  EXPECT_THAT(timeline, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerSchedulerImplTest,
       TestIdleDeadlineWithPendingDelayedTaskFarInTheFuture) {
  std::vector<std::string> timeline;
  RecordTimelineEvents(&timeline);
  InitAndPostDelayedWakeupTask();

  timeline.push_back("Post delayed and idle tasks");
  // Post a delayed task timed to occur well after the long idle period.
  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&RecordTimelineTask, base::Unretained(&timeline),
                            base::Unretained(clock_.get())),
      base::TimeDelta::FromMilliseconds(1000));
  idle_task_runner_->PostIdleTask(FROM_HERE,
                                  base::Bind(&TimelineIdleTestTask, &timeline));

  RunUntilIdle();

  std::string expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 5",
      "Post delayed and idle tasks",
      "IsNotQuiescent @ 105",
      "CanEnterLongIdlePeriod @ 405",
      "run TimelineIdleTestTask deadline 455",  // Note the full 50ms deadline.
      "CanEnterLongIdlePeriod @ 455",
      "run RecordTimelineTask @ 1005"};

  EXPECT_THAT(timeline, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerSchedulerImplTest,
       TestPostIdleTaskAfterRunningUntilIdle_NoWakeUp) {
  InitAndPostDelayedWakeupTask();

  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&NopTask), base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  // The delayed call to EnableLongIdlePeriod happened and it posted a call to
  // EnableLongIdlePeriod on the after wakeup control queue. Without an other
  // non-idle task posted, the idle tasks won't run.
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 I2");

  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

TEST_F(WorkerSchedulerImplTest,
       TestPostIdleTaskAfterRunningUntilIdle_WithWakeUp) {
  InitAndPostDelayedWakeupTask();

  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&NopTask), base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();
  // The delayed call to EnableLongIdlePeriod happened and it posted a call to
  // EnableLongIdlePeriod on the after wakeup control queue. Without an other
  // non-idle task posted, the idle tasks won't run.

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 I2 D3");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D3"), std::string("I1"),
                                   std::string("I2")));
}

TEST_F(WorkerSchedulerImplTest, TestLongIdlePeriodTimeline) {
  Init();

  std::vector<std::string> timeline;
  RecordTimelineEvents(&timeline);

  // The scheduler should not run the initiate_next_long_idle_period task if
  // there are no idle tasks and no other task woke up the scheduler, thus
  // the idle period deadline shouldn't update at the end of the current long
  // idle period.
  base::TimeTicks idle_period_deadline =
      scheduler_->CurrentIdleTaskDeadlineForTesting();
  clock_->AdvanceNow(maximum_idle_period_duration());
  RunUntilIdle();

  base::TimeTicks new_idle_period_deadline =
      scheduler_->CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);

  // Posting a after-wakeup idle task also shouldn't wake the scheduler or
  // initiate the next long idle period.
  timeline.push_back("PostIdleTaskAfterWakeup");
  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&TimelineIdleTestTask, &timeline));
  RunUntilIdle();
  new_idle_period_deadline = scheduler_->CurrentIdleTaskDeadlineForTesting();

  // Running a normal task should initiate a new long idle period after waiting
  // 300ms for quiescence.
  timeline.push_back("Post RecordTimelineTask");
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RecordTimelineTask, base::Unretained(&timeline),
                            base::Unretained(clock_.get())));
  RunUntilIdle();

  std::string expected_timeline[] = {
      "RunUntilIdle begin @ 55",
      "RunUntilIdle end @ 55",
      "PostIdleTaskAfterWakeup",
      "RunUntilIdle begin @ 55",  // NOTE idle task doesn't run till later.
      "RunUntilIdle end @ 55",
      "Post RecordTimelineTask",
      "RunUntilIdle begin @ 55",
      "run RecordTimelineTask @ 55",
      "IsNotQuiescent @ 55",  // NOTE we have to wait for quiescence.
      "CanEnterLongIdlePeriod @ 355",
      "run TimelineIdleTestTask deadline 405",
      "CanEnterLongIdlePeriod @ 405",
      "RunUntilIdle end @ 455"};

  EXPECT_THAT(timeline, ElementsAreArray(expected_timeline));
}

}  // namespace scheduler
