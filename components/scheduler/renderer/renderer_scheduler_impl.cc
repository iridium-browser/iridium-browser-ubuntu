// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_scheduler_impl.h"

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/begin_frame_args.h"
#include "components/scheduler/child/scheduler_task_runner_delegate.h"
#include "components/scheduler/child/task_queue_impl.h"
#include "components/scheduler/child/task_queue_selector.h"

namespace scheduler {
namespace {
const int kTimerTaskEstimationSampleCount = 4 * 60;
const double kTimerTaskEstimationPercentile = 80;
const int kShortIdlePeriodDurationSampleCount = 10;
const double kShortIdlePeriodDurationPercentile = 20;
}

RendererSchedulerImpl::RendererSchedulerImpl(
    scoped_refptr<SchedulerTaskRunnerDelegate> main_task_runner)
    : helper_(main_task_runner,
              "renderer.scheduler",
              TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
              TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug")),
      idle_helper_(&helper_,
                   this,
                   "renderer.scheduler",
                   TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                   "RendererSchedulerIdlePeriod",
                   base::TimeDelta()),
      control_task_runner_(helper_.ControlTaskRunner()),
      compositor_task_runner_(
          helper_.NewTaskQueue(TaskQueue::Spec("compositor_tq")
                                   .SetShouldMonitorQuiescence(true))),
      loading_task_runner_(
          helper_.NewTaskQueue(TaskQueue::Spec("loading_tq")
                                   .SetShouldMonitorQuiescence(true))),
      timer_task_runner_(
          helper_.NewTaskQueue(TaskQueue::Spec("timer_tq")
                                   .SetShouldMonitorQuiescence(true))),
      delayed_update_policy_runner_(
          base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                     base::Unretained(this)),
          helper_.ControlTaskRunner()),
      policy_may_need_update_(&any_thread_lock_),
      weak_factory_(this) {
  update_policy_closure_ = base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                                      weak_factory_.GetWeakPtr());
  end_renderer_hidden_idle_period_closure_.Reset(base::Bind(
      &RendererSchedulerImpl::EndIdlePeriod, weak_factory_.GetWeakPtr()));

  timer_task_runner_->AddTaskObserver(
      &MainThreadOnly().timer_task_cost_estimator_);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);
}

RendererSchedulerImpl::~RendererSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);
  timer_task_runner_->RemoveTaskObserver(
      &MainThreadOnly().timer_task_cost_estimator_);
  // Ensure the renderer scheduler was shut down explicitly, because otherwise
  // we could end up having stale pointers to the Blink heap which has been
  // terminated by this point.
  DCHECK(MainThreadOnly().was_shutdown_);
}

RendererSchedulerImpl::MainThreadOnly::MainThreadOnly()
    : timer_task_cost_estimator_(kTimerTaskEstimationSampleCount,
                                 kTimerTaskEstimationPercentile),
      short_idle_period_duration_(kShortIdlePeriodDurationSampleCount),
      current_policy_(Policy::NORMAL),
      timer_queue_suspend_count_(0),
      renderer_hidden_(false),
      was_shutdown_(false) {}

RendererSchedulerImpl::MainThreadOnly::~MainThreadOnly() {}

RendererSchedulerImpl::AnyThread::AnyThread()
    : pending_main_thread_input_event_count_(0),
      awaiting_touch_start_response_(false),
      in_idle_period_(false),
      begin_main_frame_on_critical_path_(false),
      timer_tasks_seem_expensive_(false) {}

RendererSchedulerImpl::CompositorThreadOnly::CompositorThreadOnly()
    : last_input_type_(blink::WebInputEvent::Undefined) {
}

RendererSchedulerImpl::CompositorThreadOnly::~CompositorThreadOnly() {
}

void RendererSchedulerImpl::Shutdown() {
  helper_.Shutdown();
  MainThreadOnly().was_shutdown_ = true;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::DefaultTaskRunner() {
  return helper_.DefaultTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::CompositorTaskRunner() {
  helper_.CheckOnValidThread();
  return compositor_task_runner_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
RendererSchedulerImpl::IdleTaskRunner() {
  return idle_helper_.IdleTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererSchedulerImpl::LoadingTaskRunner() {
  helper_.CheckOnValidThread();
  return loading_task_runner_;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::TimerTaskRunner() {
  helper_.CheckOnValidThread();
  return timer_task_runner_;
}

bool RendererSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  return idle_helper_.CanExceedIdleDeadlineIfRequired();
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
  MainThreadOnly().estimated_next_frame_begin_ =
      args.frame_time + args.interval;
  {
    base::AutoLock lock(any_thread_lock_);
    AnyThread().begin_main_frame_on_critical_path_ = args.on_critical_path;
  }
}

void RendererSchedulerImpl::DidCommitFrameToCompositor() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidCommitFrameToCompositor");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now(helper_.Now());
  if (now < MainThreadOnly().estimated_next_frame_begin_) {
    // TODO(rmcilroy): Consider reducing the idle period based on the runtime of
    // the next pending delayed tasks (as currently done in for long idle times)
    idle_helper_.StartIdlePeriod(
        IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, now,
        MainThreadOnly().estimated_next_frame_begin_);
    MainThreadOnly().short_idle_period_duration_.InsertSample(
        MainThreadOnly().estimated_next_frame_begin_ - now);
    MainThreadOnly().expected_short_idle_period_duration_ =
        MainThreadOnly().short_idle_period_duration_.Percentile(
            kShortIdlePeriodDurationPercentile);
  }
}

void RendererSchedulerImpl::BeginFrameNotExpectedSoon() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::BeginFrameNotExpectedSoon");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  idle_helper_.EnableLongIdlePeriod();
}

void RendererSchedulerImpl::OnRendererHidden() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererHidden");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || MainThreadOnly().renderer_hidden_)
    return;

  idle_helper_.EnableLongIdlePeriod();

  // Ensure that we stop running idle tasks after a few seconds of being hidden.
  end_renderer_hidden_idle_period_closure_.Cancel();
  base::TimeDelta end_idle_when_hidden_delay =
      base::TimeDelta::FromMilliseconds(kEndIdleWhenHiddenDelayMillis);
  control_task_runner_->PostDelayedTask(
      FROM_HERE, end_renderer_hidden_idle_period_closure_.callback(),
      end_idle_when_hidden_delay);
  MainThreadOnly().renderer_hidden_ = true;

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValue(helper_.Now()));
}

void RendererSchedulerImpl::OnRendererVisible() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererVisible");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || !MainThreadOnly().renderer_hidden_)
    return;

  end_renderer_hidden_idle_period_closure_.Cancel();
  MainThreadOnly().renderer_hidden_ = false;
  EndIdlePeriod();

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValue(helper_.Now()));
}

void RendererSchedulerImpl::EndIdlePeriod() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::EndIdlePeriod");
  helper_.CheckOnValidThread();
  idle_helper_.EndIdlePeriod();
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
  base::AutoLock lock(any_thread_lock_);
  base::TimeTicks now = helper_.Now();
  bool was_in_compositor_priority = InputSignalsSuggestCompositorPriority(now);
  bool was_awaiting_touch_start_response =
      AnyThread().awaiting_touch_start_response_;

  if (type) {
    switch (type) {
      case blink::WebInputEvent::TouchStart:
        AnyThread().awaiting_touch_start_response_ = true;
        break;

      case blink::WebInputEvent::TouchMove:
        // Observation of consecutive touchmoves is a strong signal that the
        // page is consuming the touch sequence, in which case touchstart
        // response prioritization is no longer necessary. Otherwise, the
        // initial touchmove should preserve the touchstart response pending
        // state.
        if (AnyThread().awaiting_touch_start_response_ &&
            CompositorThreadOnly().last_input_type_ ==
                blink::WebInputEvent::TouchMove) {
          AnyThread().awaiting_touch_start_response_ = false;
        }
        break;

      case blink::WebInputEvent::Undefined:
      case blink::WebInputEvent::GestureTapDown:
      case blink::WebInputEvent::GestureShowPress:
      case blink::WebInputEvent::GestureFlingCancel:
      case blink::WebInputEvent::GestureScrollEnd:
        // With no observable effect, these meta events do not indicate a
        // meaningful touchstart response and should not impact task priority.
        break;

      default:
        AnyThread().awaiting_touch_start_response_ = false;
        break;
    }
  }

  // Avoid unnecessary policy updates, while in compositor priority.
  if (!was_in_compositor_priority ||
      was_awaiting_touch_start_response !=
          AnyThread().awaiting_touch_start_response_) {
    EnsureUrgentPolicyUpdatePostedOnMainThread(FROM_HERE);
  }
  AnyThread().last_input_signal_time_ = now;
  CompositorThreadOnly().last_input_type_ = type;

  if (input_event_state == InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD)
    AnyThread().pending_main_thread_input_event_count_++;
}

void RendererSchedulerImpl::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidHandleInputEventOnMainThread");
  helper_.CheckOnValidThread();
  if (ShouldPrioritizeInputEvent(web_input_event)) {
    base::AutoLock lock(any_thread_lock_);
    AnyThread().last_input_signal_time_ = helper_.Now();
    if (AnyThread().pending_main_thread_input_event_count_ > 0)
      AnyThread().pending_main_thread_input_event_count_--;
  }
}

bool RendererSchedulerImpl::IsHighPriorityWorkAnticipated() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // The touchstart and compositor policies indicate a strong likelihood of
  // high-priority work in the near future.
  return MainThreadOnly().current_policy_ == Policy::COMPOSITOR_PRIORITY ||
         MainThreadOnly().current_policy_ ==
             Policy::COMPOSITOR_CRITICAL_PATH_PRIORITY ||
         MainThreadOnly().current_policy_ == Policy::TOUCHSTART_PRIORITY;
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
  switch (MainThreadOnly().current_policy_) {
    case Policy::NORMAL:
      return false;

    case Policy::COMPOSITOR_PRIORITY:
      return !compositor_task_runner_->IsQueueEmpty();

    case Policy::COMPOSITOR_CRITICAL_PATH_PRIORITY:
      return !compositor_task_runner_->IsQueueEmpty();

    case Policy::TOUCHSTART_PRIORITY:
      return true;

    case Policy::LOADING_PRIORITY:
      return false;

    default:
      NOTREACHED();
      return false;
  }
}

base::TimeTicks RendererSchedulerImpl::CurrentIdleTaskDeadlineForTesting()
    const {
  return idle_helper_.CurrentIdleTaskDeadline();
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
  any_thread_lock_.AssertAcquired();
  if (!policy_may_need_update_.IsSet()) {
    policy_may_need_update_.SetWhileLocked(true);
    control_task_runner_->PostTask(from_here, update_policy_closure_);
  }
}

void RendererSchedulerImpl::UpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::ForceUpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::FORCE_UPDATE);
}

void RendererSchedulerImpl::UpdatePolicyLocked(UpdateType update_type) {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now = helper_.Now();
  policy_may_need_update_.SetWhileLocked(false);

  AnyThread().timer_tasks_seem_expensive_ =
      MainThreadOnly().expected_short_idle_period_duration_ >
          base::TimeDelta() &&
      MainThreadOnly().timer_task_cost_estimator_.expected_task_duration() >
          MainThreadOnly().expected_short_idle_period_duration_;

  base::TimeDelta new_policy_duration;
  Policy new_policy = ComputeNewPolicy(now, &new_policy_duration);
  if (new_policy_duration > base::TimeDelta()) {
    MainThreadOnly().current_policy_expiration_time_ =
        now + new_policy_duration;
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, new_policy_duration,
                                              now);
  } else {
    MainThreadOnly().current_policy_expiration_time_ = base::TimeTicks();
  }

  if (update_type == UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED &&
      new_policy == MainThreadOnly().current_policy_)
    return;

  TaskQueue::QueuePriority compositor_queue_priority =
      TaskQueue::NORMAL_PRIORITY;
  TaskQueue::QueuePriority loading_queue_priority = TaskQueue::NORMAL_PRIORITY;
  TaskQueue::QueuePriority timer_queue_priority =
      MainThreadOnly().timer_queue_suspend_count_ != 0
          ? TaskQueue::DISABLED_PRIORITY
          : TaskQueue::NORMAL_PRIORITY;

  switch (new_policy) {
    case Policy::COMPOSITOR_PRIORITY:
      compositor_queue_priority = TaskQueue::HIGH_PRIORITY;
      break;
    case Policy::COMPOSITOR_CRITICAL_PATH_PRIORITY:
      compositor_queue_priority = TaskQueue::HIGH_PRIORITY;
      loading_queue_priority = TaskQueue::DISABLED_PRIORITY;
      break;
    case Policy::TOUCHSTART_PRIORITY:
      compositor_queue_priority = TaskQueue::HIGH_PRIORITY;
      loading_queue_priority = TaskQueue::DISABLED_PRIORITY;
      timer_queue_priority = TaskQueue::DISABLED_PRIORITY;
      break;
    case Policy::NORMAL:
      break;
    case Policy::LOADING_PRIORITY:
      // We prioritize loading tasks by deprioritizing compositing and timers.
      compositor_queue_priority = TaskQueue::BEST_EFFORT_PRIORITY;
      timer_queue_priority = TaskQueue::BEST_EFFORT_PRIORITY;
      // TODO(alexclarke): See if we can safely mark the loading task queue as
      // high priority.
      break;
    default:
      NOTREACHED();
  }

  compositor_task_runner_->SetQueuePriority(compositor_queue_priority);
  loading_task_runner_->SetQueuePriority(loading_queue_priority);
  timer_task_runner_->SetQueuePriority(timer_queue_priority);

  DCHECK(compositor_task_runner_->IsQueueEnabled());
  if (new_policy != Policy::TOUCHSTART_PRIORITY &&
      new_policy != Policy::COMPOSITOR_CRITICAL_PATH_PRIORITY) {
    DCHECK(loading_task_runner_->IsQueueEnabled());
  }
  MainThreadOnly().current_policy_ = new_policy;

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValueLocked(now));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.policy", MainThreadOnly().current_policy_);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.timer_tasks_seem_expensive",
                 AnyThread().timer_tasks_seem_expensive_);
}

bool RendererSchedulerImpl::InputSignalsSuggestCompositorPriority(
    base::TimeTicks now) const {
  base::TimeDelta unused_policy_duration;
  switch (ComputeNewPolicy(now, &unused_policy_duration)) {
    case Policy::TOUCHSTART_PRIORITY:
    case Policy::COMPOSITOR_PRIORITY:
    case Policy::COMPOSITOR_CRITICAL_PATH_PRIORITY:
      return true;

    default:
      break;
  }
  return false;
}

RendererSchedulerImpl::Policy RendererSchedulerImpl::ComputeNewPolicy(
    base::TimeTicks now,
    base::TimeDelta* new_policy_duration) const {
  any_thread_lock_.AssertAcquired();
  // Above all else we want to be responsive to user input.
  *new_policy_duration = TimeLeftInInputEscalatedPolicy(now);
  if (*new_policy_duration > base::TimeDelta()) {
    if (AnyThread().awaiting_touch_start_response_)
      return Policy::TOUCHSTART_PRIORITY;
    // If BeginMainFrame is on the critical path, we want to try and prevent
    // timers and loading tasks from running if we think they might be
    // expensive.
    // TODO(skyostil): Consider removing in_idle_period_ and
    // HadAnIdlePeriodRecently() unless we need them here.
    if (AnyThread().timer_tasks_seem_expensive_ &&
        AnyThread().begin_main_frame_on_critical_path_) {
      return Policy::COMPOSITOR_CRITICAL_PATH_PRIORITY;
    }
    return Policy::COMPOSITOR_PRIORITY;
  }

  if (AnyThread().rails_loading_priority_deadline_ > now) {
    *new_policy_duration = AnyThread().rails_loading_priority_deadline_ - now;
    return Policy::LOADING_PRIORITY;
  }

  return Policy::NORMAL;
}

base::TimeDelta RendererSchedulerImpl::TimeLeftInInputEscalatedPolicy(
    base::TimeTicks now) const {
  any_thread_lock_.AssertAcquired();

  base::TimeDelta escalated_priority_duration =
      base::TimeDelta::FromMilliseconds(kPriorityEscalationAfterInputMillis);

  // If the input event is still pending, go into input prioritized policy
  // and check again later.
  if (AnyThread().pending_main_thread_input_event_count_ > 0)
    return escalated_priority_duration;
  if (AnyThread().last_input_signal_time_.is_null() ||
      AnyThread().last_input_signal_time_ + escalated_priority_duration < now) {
    return base::TimeDelta();
  }
  return AnyThread().last_input_signal_time_ + escalated_priority_duration -
         now;
}

bool RendererSchedulerImpl::CanEnterLongIdlePeriod(
    base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  helper_.CheckOnValidThread();

  MaybeUpdatePolicy();
  if (MainThreadOnly().current_policy_ == Policy::TOUCHSTART_PRIORITY) {
    // Don't start a long idle task in touch start priority, try again when
    // the policy is scheduled to end.
    *next_long_idle_period_delay_out =
        MainThreadOnly().current_policy_expiration_time_ - now;
    return false;
  }
  return true;
}

SchedulerHelper* RendererSchedulerImpl::GetSchedulerHelperForTesting() {
  return &helper_;
}

void RendererSchedulerImpl::SuspendTimerQueue() {
  MainThreadOnly().timer_queue_suspend_count_++;
  ForceUpdatePolicy();
  DCHECK(!timer_task_runner_->IsQueueEnabled());
}

void RendererSchedulerImpl::ResumeTimerQueue() {
  MainThreadOnly().timer_queue_suspend_count_--;
  DCHECK_GE(MainThreadOnly().timer_queue_suspend_count_, 0);
  ForceUpdatePolicy();
}

// static
const char* RendererSchedulerImpl::PolicyToString(Policy policy) {
  switch (policy) {
    case Policy::NORMAL:
      return "normal";
    case Policy::COMPOSITOR_PRIORITY:
      return "compositor";
    case Policy::COMPOSITOR_CRITICAL_PATH_PRIORITY:
      return "compositor_critical_path";
    case Policy::TOUCHSTART_PRIORITY:
      return "touchstart";
    case Policy::LOADING_PRIORITY:
      return "loading";
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValue(base::TimeTicks optional_now) const {
  base::AutoLock lock(any_thread_lock_);
  return AsValueLocked(optional_now);
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValueLocked(base::TimeTicks optional_now) const {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();

  if (optional_now.is_null())
    optional_now = helper_.Now();
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();

  state->SetString("current_policy",
                   PolicyToString(MainThreadOnly().current_policy_));
  state->SetString("idle_period_state",
                   IdleHelper::IdlePeriodStateToString(
                       idle_helper_.SchedulerIdlePeriodState()));
  state->SetBoolean("renderer_hidden", MainThreadOnly().renderer_hidden_);
  state->SetDouble("now", (optional_now - base::TimeTicks()).InMillisecondsF());
  state->SetDouble("last_input_signal_time",
                   (AnyThread().last_input_signal_time_ - base::TimeTicks())
                       .InMillisecondsF());
  state->SetDouble("rails_loading_priority_deadline",
                   (AnyThread().rails_loading_priority_deadline_ -
                    base::TimeTicks()).InMillisecondsF());
  state->SetDouble("last_idle_period_end_time",
                   (AnyThread().last_idle_period_end_time_ - base::TimeTicks())
                       .InMillisecondsF());
  state->SetInteger("pending_main_thread_input_event_count",
                    AnyThread().pending_main_thread_input_event_count_);
  state->SetBoolean("awaiting_touch_start_response",
                    AnyThread().awaiting_touch_start_response_);
  state->SetBoolean("begin_main_frame_on_critical_path",
                    AnyThread().begin_main_frame_on_critical_path_);
  state->SetDouble("expected_timer_task_duration",
                   MainThreadOnly()
                       .timer_task_cost_estimator_.expected_task_duration()
                       .InMillisecondsF());
  // TODO(skyostil): Can we somehow trace how accurate these estimates were?
  state->SetDouble(
      "expected_short_idle_period_duration",
      MainThreadOnly().expected_short_idle_period_duration_.InMillisecondsF());
  state->SetBoolean("timer_tasks_seem_expensive",
                    AnyThread().timer_tasks_seem_expensive_);
  state->SetDouble("estimated_next_frame_begin",
                   (MainThreadOnly().estimated_next_frame_begin_ -
                    base::TimeTicks()).InMillisecondsF());
  state->SetBoolean("in_idle_period", AnyThread().in_idle_period_);

  return state;
}

void RendererSchedulerImpl::OnIdlePeriodStarted() {
  base::AutoLock lock(any_thread_lock_);
  AnyThread().in_idle_period_ = true;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::OnIdlePeriodEnded() {
  base::AutoLock lock(any_thread_lock_);
  AnyThread().last_idle_period_end_time_ = helper_.Now();
  AnyThread().in_idle_period_ = false;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::OnPageLoadStarted() {
  base::AutoLock lock(any_thread_lock_);
  AnyThread().rails_loading_priority_deadline_ =
      helper_.Now() + base::TimeDelta::FromMilliseconds(
                          kRailsInitialLoadingPrioritizationMillis);
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

bool RendererSchedulerImpl::HadAnIdlePeriodRecently(base::TimeTicks now) const {
  return (now - AnyThread().last_idle_period_end_time_) <=
         base::TimeDelta::FromMilliseconds(
             kIdlePeriodStarvationThresholdMillis);
}

}  // namespace scheduler
