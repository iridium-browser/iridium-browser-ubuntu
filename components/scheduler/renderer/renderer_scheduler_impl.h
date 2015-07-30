// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_

#include "base/atomicops.h"
#include "base/synchronization/lock.h"
#include "components/scheduler/child/scheduler_helper.h"
#include "components/scheduler/renderer/deadline_task_runner.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace scheduler {

class SCHEDULER_EXPORT RendererSchedulerImpl
    : public RendererScheduler,
      public SchedulerHelper::SchedulerHelperDelegate {
 public:
  RendererSchedulerImpl(
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner);
  ~RendererSchedulerImpl() override;

  // RendererScheduler implementation:
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> TimerTaskRunner() override;
  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void DidHandleInputEventOnMainThread(
      const blink::WebInputEvent& web_input_event) override;
  void DidAnimateForInputOnCompositorThread() override;
  void OnRendererHidden() override;
  void OnRendererVisible() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SuspendTimerQueue() override;
  void ResumeTimerQueue() override;

  SchedulerHelper* GetSchedulerHelperForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;

 private:
  friend class RendererSchedulerImplTest;
  friend class RendererSchedulerImplForTest;

  // Keep RendererSchedulerImpl::TaskQueueIdToString in sync with this enum.
  enum QueueId {
    COMPOSITOR_TASK_QUEUE = SchedulerHelper::TASK_QUEUE_COUNT,
    LOADING_TASK_QUEUE,
    TIMER_TASK_QUEUE,
    // Must be the last entry.
    TASK_QUEUE_COUNT,
  };

  // Keep RendererSchedulerImpl::PolicyToString in sync with this enum.
  enum class Policy {
    NORMAL,
    COMPOSITOR_PRIORITY,
    TOUCHSTART_PRIORITY,
  };

  class PollableNeedsUpdateFlag {
   public:
    PollableNeedsUpdateFlag(base::Lock* write_lock);
    ~PollableNeedsUpdateFlag();

    // Set the flag. May only be called if |write_lock| is held.
    void SetWhileLocked(bool value);

    // Returns true iff the flag is set to true.
    bool IsSet() const;

   private:
    base::subtle::Atomic32 flag_;
    base::Lock* write_lock_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(PollableNeedsUpdateFlag);
  };

  // SchedulerHelperDelegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}

  void EndIdlePeriod();

  // Returns the serialized scheduler state for tracing.
  scoped_refptr<base::trace_event::ConvertableToTraceFormat> AsValue(
      base::TimeTicks optional_now) const;
  scoped_refptr<base::trace_event::ConvertableToTraceFormat> AsValueLocked(
      base::TimeTicks optional_now) const;
  static const char* TaskQueueIdToString(QueueId queue_id);
  static const char* PolicyToString(Policy policy);

  static bool ShouldPrioritizeInputEvent(
      const blink::WebInputEvent& web_input_event);

  // The time we should stay in a priority-escalated mode after an input event.
  static const int kPriorityEscalationAfterInputMillis = 100;

  // The amount of time which idle periods can continue being scheduled when the
  // renderer has been hidden, before going to sleep for good.
  static const int kEndIdleWhenHiddenDelayMillis = 10000;

  // Returns the current scheduler policy. Must be called from the main thread.
  Policy SchedulerPolicy() const;

  // Schedules an immediate PolicyUpdate, if there isn't one already pending and
  // sets |policy_may_need_update_|. Note |incoming_signals_lock_| must be
  // locked.
  void EnsureUrgentPolicyUpdatePostedOnMainThread(
      const tracked_objects::Location& from_here);

  // Update the policy if a new signal has arrived. Must be called from the main
  // thread.
  void MaybeUpdatePolicy();

  // Locks |incoming_signals_lock_| and updates the scheduler policy.  May early
  // out if the policy is unchanged. Must be called from the main thread.
  void UpdatePolicy();

  // Like UpdatePolicy, except it doesn't early out.
  void ForceUpdatePolicy();

  enum class UpdateType {
    MAY_EARLY_OUT_IF_POLICY_UNCHANGED,
    FORCE_UPDATE,
  };

  // The implelemtation of UpdatePolicy & ForceUpdatePolicy.  It is allowed to
  // early out if |update_type| is MAY_EARLY_OUT_IF_POLICY_UNCHANGED.
  virtual void UpdatePolicyLocked(UpdateType update_type);

  // Returns the amount of time left in the current input escalated priority
  // policy.  Can be called from any thread.
  base::TimeDelta TimeLeftInInputEscalatedPolicy(base::TimeTicks now) const;

  // Helper for computing the new policy. |new_policy_duration| will be filled
  // with the amount of time after which the policy should be updated again. If
  // the duration is zero, a new policy update will not be scheduled. Must be
  // called with |incoming_signals_lock_| held. Can be called from any thread.
  Policy ComputeNewPolicy(base::TimeTicks now,
                          base::TimeDelta* new_policy_duration) const;

  // Works out if compositor tasks would be prioritized based on the current
  // input signals.  Can be called from any thread.
  bool InputSignalsSuggestCompositorPriority(base::TimeTicks now) const;

  // An input event of some sort happened, the policy may need updating.
  void UpdateForInputEventOnCompositorThread(blink::WebInputEvent::Type type,
                                             InputEventState input_event_state);

  SchedulerHelper helper_;

  scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;

  base::Closure update_policy_closure_;
  DeadlineTaskRunner delayed_update_policy_runner_;
  CancelableClosureHolder end_renderer_hidden_idle_period_closure_;

  // Don't access current_policy_ directly, instead use SchedulerPolicy().
  Policy current_policy_;
  base::TimeTicks current_policy_expiration_time_;
  bool renderer_hidden_;

  base::TimeTicks estimated_next_frame_begin_;

  // The incoming_signals_lock_ mutex protects access to all variables in the
  // (contiguous) block below.
  mutable base::Lock incoming_signals_lock_;
  base::TimeTicks last_input_signal_time_;
  int pending_main_thread_input_event_count_;
  bool awaiting_touch_start_response_;

  // Variables in this (contiguous) block are only accessed from the compositor
  // thread.
  blink::WebInputEvent::Type last_input_type_;

  PollableNeedsUpdateFlag policy_may_need_update_;
  int timer_queue_suspend_count_;  // TIMER_TASK_QUEUE suspended if non-zero.

  base::WeakPtrFactory<RendererSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_
