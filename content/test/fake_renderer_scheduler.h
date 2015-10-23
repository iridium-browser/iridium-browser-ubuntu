// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_RENDERER_SCHEDULER_H_
#define CONTENT_TEST_FAKE_RENDERER_SCHEDULER_H_

#include "components/scheduler/renderer/renderer_scheduler.h"

namespace content {

class FakeRendererScheduler : public scheduler::RendererScheduler {
 public:
  FakeRendererScheduler();
  ~FakeRendererScheduler() override;

  // RendererScheduler implementation.
  scoped_refptr<scheduler::TaskQueue> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() override;
  scoped_refptr<scheduler::SingleThreadIdleTaskRunner> IdleTaskRunner()
      override;
  scoped_refptr<scheduler::TaskQueue> TimerTaskRunner() override;
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
  void OnPageLoadStarted() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  bool ShouldYieldForHighPriorityWork() override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SuspendTimerQueue() override;
  void ResumeTimerQueue() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeRendererScheduler);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_RENDERER_SCHEDULER_H_
