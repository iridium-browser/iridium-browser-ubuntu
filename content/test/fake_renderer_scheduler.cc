// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_renderer_scheduler.h"

namespace content {

FakeRendererScheduler::FakeRendererScheduler() {
}

FakeRendererScheduler::~FakeRendererScheduler() {
}

scoped_refptr<scheduler::TaskQueue> FakeRendererScheduler::DefaultTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::CompositorTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::LoadingTaskRunner() {
  return nullptr;
}

scoped_refptr<scheduler::SingleThreadIdleTaskRunner>
FakeRendererScheduler::IdleTaskRunner() {
  return nullptr;
}

scoped_refptr<scheduler::TaskQueue> FakeRendererScheduler::TimerTaskRunner() {
  return nullptr;
}

void FakeRendererScheduler::WillBeginFrame(const cc::BeginFrameArgs& args) {
}

void FakeRendererScheduler::BeginFrameNotExpectedSoon() {
}

void FakeRendererScheduler::DidCommitFrameToCompositor() {
}

void FakeRendererScheduler::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {
}

void FakeRendererScheduler::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event) {
}

void FakeRendererScheduler::DidAnimateForInputOnCompositorThread() {
}

bool FakeRendererScheduler::IsHighPriorityWorkAnticipated() {
  return false;
}

void FakeRendererScheduler::OnRendererHidden() {
}

void FakeRendererScheduler::OnRendererVisible() {
}

void FakeRendererScheduler::OnPageLoadStarted() {
}

bool FakeRendererScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

bool FakeRendererScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

void FakeRendererScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
}

void FakeRendererScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
}

void FakeRendererScheduler::Shutdown() {
}

void FakeRendererScheduler::SuspendTimerQueue() {
}

void FakeRendererScheduler::ResumeTimerQueue() {
}

}  // namespace content
