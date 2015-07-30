// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_scheduler_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/begin_frame_args.h"
#include "components/scheduler/child/nestable_single_thread_task_runner.h"
#include "components/scheduler/child/prioritizing_task_queue_selector.h"
#include "ui/gfx/frame_time.h"

namespace scheduler {

RendererSchedulerImpl::RendererSchedulerImpl(
    scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner)
    : helper_(main_task_runner,
              this,
              "renderer.scheduler",
              TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
              "RendererSchedulerIdlePeriod",
              TASK_QUEUE_COUNT,
              base::TimeDelta()),
      control_task_runner_(helper_.ControlTaskRunner()),
      compositor_task_runner_(
          helper_.TaskRunnerForQueue(COMPOSITOR_TASK_QUEUE)),
      loading_task_runner_(helper_.TaskRunnerForQueue(LOADING_TASK_QUEUE)),
      timer_task_runner_(helper_.TaskRunnerForQueue(TIMER_TASK_QUEUE)),
      delayed_update_policy_runner_(
          base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                     base::Unretained(this)),
          helper_.ControlTaskRunner()),
      current_policy_(Policy::NORMAL),
      renderer_hidden_(false),
      pending_main_thread_input_event_count_(0),
      awaiting_touch_start_response_(false),
      last_input_type_(blink::WebInputEvent::Undefined),
      policy_may_need_update_(&incoming_signals_lock_),
      timer_queue_suspend_count_(0),
      weak_factory_(this) {
  update_policy_closure_ = base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                                      weak_factory_.GetWeakPtr());
  end_renderer_hidden_idle_period_closure_.Reset(base::Bind(
      &RendererSchedulerImpl::EndIdlePeriod, weak_factory_.GetWeakPtr()));

  for (size_t i = SchedulerHelper::TASK_QUEUE_COUNT; i < TASK_QUEUE_COUNT;
       i++) {
    helper_.SetQueueName(i, TaskQueueIdToString(static_cast<QueueId>(i)));
  }
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);
}

RendererSchedulerImpl::~RendererSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);
}

void RendererSchedulerImpl::Shutdown() {
  helper_.Shutdown();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::DefaultTaskRunner() {
  return helper_.DefaultTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::CompositorTaskRunner() {
  helper_.CheckOnValidThread();
  return compositor_task_runner_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
RendererSchedulerImpl::IdleTaskRunner() {
  return helper_.IdleTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::LoadingTaskRunner() {
  helper_.CheckOnValidThread();
  return loading_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::TimerTaskRunner() {
  helper_.CheckOnValidThread();
  return timer_task_runner_;
}

bool RendererSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  return helper_.CanExceedIdleDeadlineIfRequired();
}

void RendererSchedulerImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.AddTaskObserver(task_observer);
}

void RendererSchedulerImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.RemoveTaskObserver(task_observer);
}

void RendererSchedulerImpl::WillBeginFrame(const cc::BeginFrameArgs& args) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::WillBeginFrame", "args", args.AsValue());
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  EndIdlePeriod();
  estimated_next_frame_begin_ = args.frame_time + args.interval;
}

void RendererSchedulerImpl::DidCommitFrameToCompositor() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidCommitFrameToCompositor");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now(helper_.Now());
  if (now < estimated_next_frame_begin_) {
    // TODO(rmcilroy): Consider reducing the idle period based on the runtime of
    // the next pending delayed tasks (as currently done in for long idle times)
    helper_.StartIdlePeriod(
        SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, now,
        estimated_next_frame_begin_, true);
  }
}

void RendererSchedulerImpl::BeginFrameNotExpectedSoon() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::BeginFrameNotExpectedSoon");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  helper_.EnableLongIdlePeriod();
}

void RendererSchedulerImpl::OnRendererHidden() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererHidden");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || renderer_hidden_)
    return;

  helper_.EnableLongIdlePeriod();

  // Ensure that we stop running idle tasks after a few seconds of being hidden.
  end_renderer_hidden_idle_period_closure_.Cancel();
  base::TimeDelta end_idle_when_hidden_delay =
      base::TimeDelta::FromMilliseconds(kEndIdleWhenHiddenDelayMillis);
  control_task_runner_->PostDelayedTask(
      FROM_HERE, end_renderer_hidden_idle_period_closure_.callback(),
      end_idle_when_hidden_delay);
  renderer_hidden_ = true;

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValueLocked(helper_.Now()));
}

void RendererSchedulerImpl::OnRendererVisible() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererVisible");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || !renderer_hidden_)
    return;

  end_renderer_hidden_idle_period_closure_.Cancel();
  renderer_hidden_ = false;
  EndIdlePeriod();

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValueLocked(helper_.Now()));
}

void RendererSchedulerImpl::EndIdlePeriod() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::EndIdlePeriod");
  helper_.CheckOnValidThread();
  helper_.EndIdlePeriod();
}

// static
bool RendererSchedulerImpl::ShouldPrioritizeInputEvent(
    const blink::WebInputEvent& web_input_event) {
  // We regard MouseMove events with the left mouse button down as a signal
  // that the user is doing something requiring a smooth frame rate.
  if (web_input_event.type == blink::WebInputEvent::MouseMove &&
      (web_input_event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    return true;
  }
  // Ignore all other mouse events because they probably don't signal user
  // interaction needing a smooth framerate. NOTE isMouseEventType returns false
  // for mouse wheel events, hence we regard them as user input.
  // Ignore keyboard events because it doesn't really make sense to enter
  // compositor priority for them.
  if (blink::WebInputEvent::isMouseEventType(web_input_event.type) ||
      blink::WebInputEvent::isKeyboardEventType(web_input_event.type)) {
    return false;
  }
  return true;
}

void RendererSchedulerImpl::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidHandleInputEventOnCompositorThread");
  if (!ShouldPrioritizeInputEvent(web_input_event))
    return;

  UpdateForInputEventOnCompositorThread(web_input_event.type, event_state);
}

void RendererSchedulerImpl::DidAnimateForInputOnCompositorThread() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidAnimateForInputOnCompositorThread");
  UpdateForInputEventOnCompositorThread(
      blink::WebInputEvent::Undefined,
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
}

void RendererSchedulerImpl::UpdateForInputEventOnCompositorThread(
    blink::WebInputEvent::Type type,
    InputEventState input_event_state) {
  base::AutoLock lock(incoming_signals_lock_);
  base::TimeTicks now = helper_.Now();
  bool was_in_compositor_priority = InputSignalsSuggestCompositorPriority(now);
  bool was_awaiting_touch_start_response_ = awaiting_touch_start_response_;

  if (type) {
    switch (type) {
      case blink::WebInputEvent::TouchStart:
        awaiting_touch_start_response_ = true;
        break;

      case blink::WebInputEvent::TouchMove:
        // Observation of consecutive touchmoves is a strong signal that the
        // page is consuming the touch sequence, in which case touchstart
        // response prioritization is no longer necessary. Otherwise, the
        // initial touchmove should preserve the touchstart response pending
        // state.
        if (awaiting_touch_start_response_ &&
            last_input_type_ == blink::WebInputEvent::TouchMove) {
          awaiting_touch_start_response_ = false;
        }
        break;

      case blink::WebInputEvent::GestureTapDown:
      case blink::WebInputEvent::GestureShowPress:
      case blink::WebInputEvent::GestureFlingCancel:
      case blink::WebInputEvent::GestureScrollEnd:
        // With no observable effect, these meta events do not indicate a
        // meaningful touchstart response and should not impact task priority.
        break;

      default:
        awaiting_touch_start_response_ = false;
        break;
    }
  }

  // Avoid unnecessary policy updates, while in compositor priority.
  if (!was_in_compositor_priority ||
      was_awaiting_touch_start_response_ != awaiting_touch_start_response_) {
    EnsureUrgentPolicyUpdatePostedOnMainThread(FROM_HERE);
  }
  last_input_signal_time_ = now;
  last_input_type_ = type;

  if (input_event_state == InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD)
    pending_main_thread_input_event_count_++;
}

void RendererSchedulerImpl::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidHandleInputEventOnMainThread");
  helper_.CheckOnValidThread();
  if (ShouldPrioritizeInputEvent(web_input_event)) {
    base::AutoLock lock(incoming_signals_lock_);
    last_input_signal_time_ = helper_.Now();
    if (pending_main_thread_input_event_count_ > 0)
      pending_main_thread_input_event_count_--;
  }
}

bool RendererSchedulerImpl::IsHighPriorityWorkAnticipated() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // The touchstart and compositor policies indicate a strong likelihood of
  // high-priority work in the near future.
  return SchedulerPolicy() == Policy::COMPOSITOR_PRIORITY ||
         SchedulerPolicy() == Policy::TOUCHSTART_PRIORITY;
}

bool RendererSchedulerImpl::ShouldYieldForHighPriorityWork() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // We only yield if we are in the compositor priority and there is compositor
  // work outstanding, or if we are in the touchstart response priority.
  // Note: even though the control queue is higher priority we don't yield for
  // it since these tasks are not user-provided work and they are only intended
  // to run before the next task, not interrupt the tasks.
  switch (SchedulerPolicy()) {
    case Policy::NORMAL:
      return false;

    case Policy::COMPOSITOR_PRIORITY:
      return !helper_.IsQueueEmpty(COMPOSITOR_TASK_QUEUE);

    case Policy::TOUCHSTART_PRIORITY:
      return true;

    default:
      NOTREACHED();
      return false;
  }
}

base::TimeTicks RendererSchedulerImpl::CurrentIdleTaskDeadlineForTesting()
    const {
  base::TimeTicks deadline;
  helper_.CurrentIdleTaskDeadlineCallback(&deadline);
  return deadline;
}

RendererSchedulerImpl::Policy RendererSchedulerImpl::SchedulerPolicy() const {
  helper_.CheckOnValidThread();
  return current_policy_;
}

void RendererSchedulerImpl::MaybeUpdatePolicy() {
  helper_.CheckOnValidThread();
  if (policy_may_need_update_.IsSet()) {
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
    const tracked_objects::Location& from_here) {
  // TODO(scheduler-dev): Check that this method isn't called from the main
  // thread.
  incoming_signals_lock_.AssertAcquired();
  if (!policy_may_need_update_.IsSet()) {
    policy_may_need_update_.SetWhileLocked(true);
    control_task_runner_->PostTask(from_here, update_policy_closure_);
  }
}

void RendererSchedulerImpl::UpdatePolicy() {
  base::AutoLock lock(incoming_signals_lock_);
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::ForceUpdatePolicy() {
  base::AutoLock lock(incoming_signals_lock_);
  UpdatePolicyLocked(UpdateType::FORCE_UPDATE);
}

void RendererSchedulerImpl::UpdatePolicyLocked(UpdateType update_type) {
  helper_.CheckOnValidThread();
  incoming_signals_lock_.AssertAcquired();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now = helper_.Now();
  policy_may_need_update_.SetWhileLocked(false);

  base::TimeDelta new_policy_duration;
  Policy new_policy = ComputeNewPolicy(now, &new_policy_duration);
  if (new_policy_duration > base::TimeDelta()) {
    current_policy_expiration_time_ = now + new_policy_duration;
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, new_policy_duration,
                                              now);
  } else {
    current_policy_expiration_time_ = base::TimeTicks();
  }

  if (update_type == UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED &&
      new_policy == current_policy_)
    return;

  bool policy_disables_timers = false;

  switch (new_policy) {
    case Policy::COMPOSITOR_PRIORITY:
      helper_.SetQueuePriority(COMPOSITOR_TASK_QUEUE,
                               PrioritizingTaskQueueSelector::HIGH_PRIORITY);
      // TODO(scheduler-dev): Add a task priority between HIGH and BEST_EFFORT
      // that still has some guarantee of running.
      helper_.SetQueuePriority(
          LOADING_TASK_QUEUE,
          PrioritizingTaskQueueSelector::BEST_EFFORT_PRIORITY);
      break;
    case Policy::TOUCHSTART_PRIORITY:
      helper_.SetQueuePriority(COMPOSITOR_TASK_QUEUE,
                               PrioritizingTaskQueueSelector::HIGH_PRIORITY);
      helper_.DisableQueue(LOADING_TASK_QUEUE);
      // TODO(alexclarke): Set policy_disables_timers once the blink TimerBase
      // refactor is safely landed.
      break;
    case Policy::NORMAL:
      helper_.SetQueuePriority(COMPOSITOR_TASK_QUEUE,
                               PrioritizingTaskQueueSelector::NORMAL_PRIORITY);
      helper_.SetQueuePriority(LOADING_TASK_QUEUE,
                               PrioritizingTaskQueueSelector::NORMAL_PRIORITY);
      break;
  }
  if (timer_queue_suspend_count_ != 0 || policy_disables_timers) {
    helper_.DisableQueue(TIMER_TASK_QUEUE);
  } else {
    helper_.SetQueuePriority(TIMER_TASK_QUEUE,
                             PrioritizingTaskQueueSelector::NORMAL_PRIORITY);
  }
  DCHECK(helper_.IsQueueEnabled(COMPOSITOR_TASK_QUEUE));
  if (new_policy != Policy::TOUCHSTART_PRIORITY)
    DCHECK(helper_.IsQueueEnabled(LOADING_TASK_QUEUE));

  current_policy_ = new_policy;

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValueLocked(now));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.policy", current_policy_);
}

bool RendererSchedulerImpl::InputSignalsSuggestCompositorPriority(
    base::TimeTicks now) const {
  base::TimeDelta unused_policy_duration;
  switch (ComputeNewPolicy(now, &unused_policy_duration)) {
    case Policy::TOUCHSTART_PRIORITY:
    case Policy::COMPOSITOR_PRIORITY:
      return true;

    default:
      break;
  }
  return false;
}

RendererSchedulerImpl::Policy RendererSchedulerImpl::ComputeNewPolicy(
    base::TimeTicks now,
    base::TimeDelta* new_policy_duration) const {
  incoming_signals_lock_.AssertAcquired();
  *new_policy_duration = TimeLeftInInputEscalatedPolicy(now);

  if (*new_policy_duration == base::TimeDelta())
    return Policy::NORMAL;

  return awaiting_touch_start_response_ ? Policy::TOUCHSTART_PRIORITY
                                        : Policy::COMPOSITOR_PRIORITY;
}

base::TimeDelta RendererSchedulerImpl::TimeLeftInInputEscalatedPolicy(
    base::TimeTicks now) const {
  incoming_signals_lock_.AssertAcquired();

  base::TimeDelta escalated_priority_duration =
      base::TimeDelta::FromMilliseconds(kPriorityEscalationAfterInputMillis);

  // If the input event is still pending, go into input prioritized policy
  // and check again later.
  if (pending_main_thread_input_event_count_ > 0)
    return escalated_priority_duration;
  if (last_input_signal_time_.is_null() ||
      last_input_signal_time_ + escalated_priority_duration < now) {
    return base::TimeDelta();
  }
  return last_input_signal_time_ + escalated_priority_duration - now;
}

bool RendererSchedulerImpl::CanEnterLongIdlePeriod(
    base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  helper_.CheckOnValidThread();

  MaybeUpdatePolicy();
  if (SchedulerPolicy() == Policy::TOUCHSTART_PRIORITY) {
    // Don't start a long idle task in touch start priority, try again when
    // the policy is scheduled to end.
    *next_long_idle_period_delay_out = current_policy_expiration_time_ - now;
    return false;
  }
  return true;
}

SchedulerHelper* RendererSchedulerImpl::GetSchedulerHelperForTesting() {
  return &helper_;
}

RendererSchedulerImpl::PollableNeedsUpdateFlag::PollableNeedsUpdateFlag(
    base::Lock* write_lock_)
    : flag_(false), write_lock_(write_lock_) {
}

RendererSchedulerImpl::PollableNeedsUpdateFlag::~PollableNeedsUpdateFlag() {
}

void RendererSchedulerImpl::PollableNeedsUpdateFlag::SetWhileLocked(
    bool value) {
  write_lock_->AssertAcquired();
  base::subtle::Release_Store(&flag_, value);
}

bool RendererSchedulerImpl::PollableNeedsUpdateFlag::IsSet() const {
  return base::subtle::Acquire_Load(&flag_) != 0;
}

void RendererSchedulerImpl::SuspendTimerQueue() {
  helper_.CheckOnValidThread();
  timer_queue_suspend_count_++;
  ForceUpdatePolicy();
  DCHECK(!helper_.IsQueueEnabled(TIMER_TASK_QUEUE));
}

void RendererSchedulerImpl::ResumeTimerQueue() {
  helper_.CheckOnValidThread();
  timer_queue_suspend_count_--;
  DCHECK_GE(timer_queue_suspend_count_, 0);
  ForceUpdatePolicy();
}

// static
const char* RendererSchedulerImpl::TaskQueueIdToString(QueueId queue_id) {
  switch (queue_id) {
    case COMPOSITOR_TASK_QUEUE:
      return "compositor_tq";
    case LOADING_TASK_QUEUE:
      return "loading_tq";
    case TIMER_TASK_QUEUE:
      return "timer_tq";
    default:
      return SchedulerHelper::TaskQueueIdToString(
          static_cast<SchedulerHelper::QueueId>(queue_id));
  }
}

// static
const char* RendererSchedulerImpl::PolicyToString(Policy policy) {
  switch (policy) {
    case Policy::NORMAL:
      return "normal";
    case Policy::COMPOSITOR_PRIORITY:
      return "compositor";
    case Policy::TOUCHSTART_PRIORITY:
      return "touchstart";
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValueLocked(base::TimeTicks optional_now) const {
  helper_.CheckOnValidThread();
  incoming_signals_lock_.AssertAcquired();

  if (optional_now.is_null())
    optional_now = helper_.Now();
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->SetString("current_policy", PolicyToString(current_policy_));
  state->SetString("idle_period_state",
                   SchedulerHelper::IdlePeriodStateToString(
                       helper_.SchedulerIdlePeriodState()));
  state->SetBoolean("renderer_hidden_", renderer_hidden_);
  state->SetDouble("now", (optional_now - base::TimeTicks()).InMillisecondsF());
  state->SetDouble(
      "last_input_signal_time",
      (last_input_signal_time_ - base::TimeTicks()).InMillisecondsF());
  state->SetInteger("pending_main_thread_input_event_count",
                    pending_main_thread_input_event_count_);
  state->SetBoolean("awaiting_touch_start_response",
                    awaiting_touch_start_response_);
  state->SetDouble(
      "estimated_next_frame_begin",
      (estimated_next_frame_begin_ - base::TimeTicks()).InMillisecondsF());

  return state;
}

}  // namespace scheduler
