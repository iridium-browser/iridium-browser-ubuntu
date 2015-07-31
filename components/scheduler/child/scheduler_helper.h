// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_SCHEDULER_HELPER_H_
#define COMPONENTS_SCHEDULER_CHILD_SCHEDULER_HELPER_H_

#include "components/scheduler/child/cancelable_closure_holder.h"
#include "components/scheduler/child/prioritizing_task_queue_selector.h"
#include "components/scheduler/child/single_thread_idle_task_runner.h"
#include "components/scheduler/child/task_queue_manager.h"
#include "components/scheduler/child/time_source.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class NestableSingleThreadTaskRunner;

// Common scheduler functionality for Default and Idle tasks.
class SCHEDULER_EXPORT SchedulerHelper {
 public:
  // Used to by scheduler implementations to customize idle behaviour.
  class SCHEDULER_EXPORT SchedulerHelperDelegate {
   public:
    SchedulerHelperDelegate();
    virtual ~SchedulerHelperDelegate();

    // If it's ok to enter a Long Idle period, return true.  Otherwise return
    // false and set next_long_idle_period_delay_out so we know when to try
    // again.
    virtual bool CanEnterLongIdlePeriod(
        base::TimeTicks now,
        base::TimeDelta* next_long_idle_period_delay_out) = 0;

    // Signals that the Long Idle Period hasn't started yet because the system
    // isn't quiescent.
    virtual void IsNotQuiescent() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(SchedulerHelperDelegate);
  };

  // NOTE |total_task_queue_count| must be >= TASK_QUEUE_COUNT.
  // Category strings must have application lifetime (statics or
  // literals). They may not include " chars.
  SchedulerHelper(
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
      SchedulerHelperDelegate* scheduler_helper_delegate,
      const char* tracing_category,
      const char* disabled_by_default_tracing_category,
      const char* idle_period_tracing_name,
      size_t total_task_queue_count,
      base::TimeDelta required_quiescence_duration_before_long_idle_period);
  ~SchedulerHelper();

  // Returns the default task runner.
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner();

  // Returns the idle task runner. Tasks posted to this runner may be reordered
  // relative to other task types and may be starved for an arbitrarily long
  // time if no idle time is available.
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner();

  // Returns the control task runner.  Tasks posted to this runner are executed
  // with the highest priority. Care must be taken to avoid starvation of other
  // task queues.
  scoped_refptr<base::SingleThreadTaskRunner> ControlTaskRunner();

  // Returns true if a currently running idle task could exceed its deadline
  // without impacting user experience too much. This should only be used if
  // there is a task which cannot be pre-empted and is likely to take longer
  // than the largest expected idle task deadline. It should NOT be polled to
  // check whether more work can be performed on the current idle task after
  // its deadline has expired - post a new idle task for the continuation of the
  // work in this case.
  // Must be called from the thread this class was created on.
  bool CanExceedIdleDeadlineIfRequired() const;

  // Adds or removes a task observer from the scheduler. The observer will be
  // notified before and after every executed task. These functions can only be
  // called on the thread this class was created on.
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer);
  void RemoveTaskObserver(base::MessageLoop::TaskObserver* task_observer);

  // Shuts down the scheduler by dropping any remaining pending work in the work
  // queues. After this call any work posted to the task runners will be
  // silently dropped.
  void Shutdown();

  // Returns true if Shutdown() has been called. Otherwise returns false.
  bool IsShutdown() const { return !task_queue_manager_.get(); }

  // Keep SchedulerHelper::TaskQueueIdToString in sync with this enum.
  enum QueueId {
    DEFAULT_TASK_QUEUE,
    IDLE_TASK_QUEUE,
    CONTROL_TASK_QUEUE,
    CONTROL_TASK_AFTER_WAKEUP_QUEUE,
    // Must be the last entry.
    TASK_QUEUE_COUNT,
  };

  // Keep SchedulerHelper::IdlePeriodStateToString in sync with this enum.
  enum class IdlePeriodState {
    NOT_IN_IDLE_PERIOD,
    IN_SHORT_IDLE_PERIOD,
    IN_LONG_IDLE_PERIOD,
    IN_LONG_IDLE_PERIOD_WITH_MAX_DEADLINE,
    ENDING_LONG_IDLE_PERIOD
  };

  static const char* TaskQueueIdToString(QueueId queue_id);
  static const char* IdlePeriodStateToString(IdlePeriodState state);

  // The maximum length of an idle period.
  static const int kMaximumIdlePeriodMillis = 50;

  // The minimum delay to wait between retrying to initiate a long idle time.
  static const int kRetryEnableLongIdlePeriodDelayMillis = 1;

  // IdleTaskDeadlineSupplier Implementation:
  void CurrentIdleTaskDeadlineCallback(base::TimeTicks* deadline_out) const;

  // Returns the new idle period state for the next long idle period. Fills in
  // |next_long_idle_period_delay_out| with the next time we should try to
  // initiate the next idle period.
  IdlePeriodState ComputeNewLongIdlePeriodState(
      const base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out);

  // If |required_quiescence_duration_before_long_idle_period_| is zero then
  // immediately initiate a long idle period, otherwise check if any tasks have
  // run recently and if so, check again after a delay of
  // |required_quiescence_duration_before_long_idle_period_|.
  // Calling this function will end any previous idle period immediately, and
  // potentially again later if
  // |required_quiescence_duration_before_long_idle_period_| is non-zero.
  // NOTE EndIdlePeriod will disable the long idle periods.
  void EnableLongIdlePeriod();

  // Start and end an idle period. If |post_end_idle_period| is true, it will
  // post a delayed EndIdlePeriod scheduled to occur at |idle_period_deadline|.
  void StartIdlePeriod(IdlePeriodState new_idle_period_state,
                       base::TimeTicks now,
                       base::TimeTicks idle_period_deadline,
                       bool post_end_idle_period);

  // This will end an idle period either started with StartIdlePeriod or
  // EnableLongIdlePeriod.
  void EndIdlePeriod();

  // Returns true if |state| represents being within an idle period state.
  static bool IsInIdlePeriod(IdlePeriodState state);

  void CheckOnValidThread() const {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  // Accessor methods.
  base::TimeTicks Now() const;
  IdlePeriodState SchedulerIdlePeriodState() const;
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerForQueue(
      size_t queue_index) const;
  void SetQueueName(size_t queue_index, const char* name);
  bool IsQueueEmpty(size_t queue_index) const;
  void SetQueuePriority(size_t queue_index,
                        PrioritizingTaskQueueSelector::QueuePriority priority);
  void EnableQueue(size_t queue_index,
                   PrioritizingTaskQueueSelector::QueuePriority priority);
  void DisableQueue(size_t queue_index);
  bool IsQueueEnabled(size_t queue_index) const;

  // Test helpers.
  void SetTimeSourceForTesting(scoped_ptr<TimeSource> time_source);
  void SetWorkBatchSizeForTesting(size_t work_batch_size);
  TaskQueueManager* GetTaskQueueManagerForTesting();

 private:
  friend class SchedulerHelperTest;

  bool ShouldWaitForQuiescence();
  void EnableLongIdlePeriodAfterWakeup();

  base::ThreadChecker thread_checker_;
  scoped_ptr<PrioritizingTaskQueueSelector> task_queue_selector_;
  scoped_ptr<TaskQueueManager> task_queue_manager_;

  CancelableClosureHolder end_idle_period_closure_;
  CancelableClosureHolder enable_next_long_idle_period_closure_;
  CancelableClosureHolder enable_next_long_idle_period_after_wakeup_closure_;

  IdlePeriodState idle_period_state_;
  SchedulerHelperDelegate* scheduler_helper_delegate_;  // NOT OWNED

  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> control_task_after_wakeup_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  // A bitmap which controls the set of queues that are checked for quiescence
  // before triggering a long idle period.
  uint64 quiescence_monitored_task_queue_mask_;
  base::TimeDelta required_quiescence_duration_before_long_idle_period_;

  base::TimeTicks idle_period_deadline_;
  scoped_ptr<TimeSource> time_source_;

  const char* tracing_category_;
  const char* disabled_by_default_tracing_category_;
  const char* idle_period_tracing_name_;

  base::WeakPtr<SchedulerHelper> weak_scheduler_ptr_;
  base::WeakPtrFactory<SchedulerHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerHelper);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_SCHEDULER_HELPER_H_
