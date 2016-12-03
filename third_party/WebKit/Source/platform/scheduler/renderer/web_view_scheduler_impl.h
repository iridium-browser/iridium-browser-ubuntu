// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "public/platform/scheduler/base/task_queue.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebViewScheduler.h"

namespace base {
namespace trace_event {
class BlameContext;
}  // namespace trace_event
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {
namespace scheduler {

class RendererSchedulerImpl;
class WebFrameSchedulerImpl;

class BLINK_PLATFORM_EXPORT WebViewSchedulerImpl : public WebViewScheduler {
 public:
  WebViewSchedulerImpl(
      WebScheduler::InterventionReporter* intervention_reporter,
      RendererSchedulerImpl* renderer_scheduler,
      bool disable_background_timer_throttling);

  ~WebViewSchedulerImpl() override;

  // WebViewScheduler implementation:
  void setPageVisible(bool page_visible) override;
  std::unique_ptr<WebFrameScheduler> createFrameScheduler(
      BlameContext* blame_context) override;
  void enableVirtualTime() override;
  bool virtualTimeAllowedToAdvance() const override;
  void setVirtualTimePolicy(VirtualTimePolicy virtual_time_policy) override;

  // Virtual for testing.
  virtual void ReportIntervention(const std::string& message);

  std::unique_ptr<WebFrameSchedulerImpl> createWebFrameSchedulerImpl(
      base::trace_event::BlameContext* blame_context);

  void DidStartLoading(unsigned long identifier);
  void DidStopLoading(unsigned long identifier);
  void IncrementBackgroundParserCount();
  void DecrementBackgroundParserCount();
  void Unregister(WebFrameSchedulerImpl* frame_scheduler);

 private:
  void setAllowVirtualTimeToAdvance(bool allow_virtual_time_to_advance);
  void ApplyVirtualTimePolicy();

  std::set<WebFrameSchedulerImpl*> frame_schedulers_;
  std::set<unsigned long> pending_loads_;
  WebScheduler::InterventionReporter* intervention_reporter_;  // Not owned.
  RendererSchedulerImpl* renderer_scheduler_;
  VirtualTimePolicy virtual_time_policy_;
  int background_parser_count_;
  bool page_visible_;
  bool disable_background_timer_throttling_;
  bool allow_virtual_time_to_advance_;
  bool have_seen_loading_task_;
  bool virtual_time_;

  DISALLOW_COPY_AND_ASSIGN(WebViewSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_IMPL_H_
