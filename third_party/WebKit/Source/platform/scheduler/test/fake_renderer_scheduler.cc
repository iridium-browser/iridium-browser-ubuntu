// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/scheduler/test/fake_renderer_scheduler.h"

#include "public/platform/WebThread.h"

namespace blink {
namespace scheduler {

FakeRendererScheduler::FakeRendererScheduler() {}

FakeRendererScheduler::~FakeRendererScheduler() {}

std::unique_ptr<blink::WebThread> FakeRendererScheduler::CreateMainThread() {
  return nullptr;
}

scoped_refptr<TaskQueue> FakeRendererScheduler::DefaultTaskRunner() {
  return nullptr;
}

scoped_refptr<TaskQueue> FakeRendererScheduler::CompositorTaskRunner() {
  return nullptr;
}

scoped_refptr<TaskQueue> FakeRendererScheduler::LoadingTaskRunner() {
  return nullptr;
}

scoped_refptr<SingleThreadIdleTaskRunner>
FakeRendererScheduler::IdleTaskRunner() {
  return nullptr;
}

scoped_refptr<TaskQueue> FakeRendererScheduler::TimerTaskRunner() {
  return nullptr;
}

scoped_refptr<TaskQueue> FakeRendererScheduler::NewLoadingTaskRunner(
    const char* name) {
  return nullptr;
}

scoped_refptr<TaskQueue> FakeRendererScheduler::NewTimerTaskRunner(
    const char* name) {
  return nullptr;
}

scoped_refptr<TaskQueue> FakeRendererScheduler::NewUnthrottledTaskRunner(
    const char* name) {
  return nullptr;
}

std::unique_ptr<RenderWidgetSchedulingState>
FakeRendererScheduler::NewRenderWidgetSchedulingState() {
  return nullptr;
}

void FakeRendererScheduler::WillBeginFrame(const cc::BeginFrameArgs& args) {}

void FakeRendererScheduler::BeginFrameNotExpectedSoon() {}

void FakeRendererScheduler::DidCommitFrameToCompositor() {}

void FakeRendererScheduler::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {}

void FakeRendererScheduler::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event) {}

void FakeRendererScheduler::DidAnimateForInputOnCompositorThread() {}

bool FakeRendererScheduler::IsHighPriorityWorkAnticipated() {
  return false;
}

void FakeRendererScheduler::OnRendererBackgrounded() {}

void FakeRendererScheduler::OnRendererForegrounded() {}

void FakeRendererScheduler::SuspendRenderer() {}

void FakeRendererScheduler::AddPendingNavigation(
    blink::WebScheduler::NavigatingFrameType type) {}

void FakeRendererScheduler::RemovePendingNavigation(
    blink::WebScheduler::NavigatingFrameType type) {}

void FakeRendererScheduler::OnNavigationStarted() {}

bool FakeRendererScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

bool FakeRendererScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

void FakeRendererScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {}

void FakeRendererScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {}

void FakeRendererScheduler::Shutdown() {}

void FakeRendererScheduler::SuspendTimerQueue() {}

void FakeRendererScheduler::ResumeTimerQueue() {}

void FakeRendererScheduler::SetTimerQueueSuspensionWhenBackgroundedEnabled(
    bool enabled) {}

void FakeRendererScheduler::SetTopLevelBlameContext(
    base::trace_event::BlameContext* blame_context) {}

void FakeRendererScheduler::SetRAILModeObserver(RAILModeObserver* observer) {}

}  // namespace scheduler
}  // namespace blink
