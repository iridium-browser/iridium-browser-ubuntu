// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/stats_collection_controller.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/services/tracing/public/cpp/switches.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace html_viewer {

namespace {

// Initialize the histogram data using the given startup performance times.
// TODO(msw): Use TimeTicks to avoid system clock changes: crbug.com/521164
void GetStartupPerformanceTimesCallbackImpl(
    tracing::StartupPerformanceTimesPtr times) {
  base::StatisticsRecorder::Initialize();

  const base::Time shell_process_creation_time =
      base::Time::FromInternalValue(times->shell_process_creation_time);
  startup_metric_utils::RecordMainEntryPointTime(shell_process_creation_time);

  const base::Time browser_message_loop_start_time =
      base::Time::FromInternalValue(times->browser_message_loop_start_time);
  // TODO(msw): Determine if this is the first run.
  startup_metric_utils::RecordBrowserMainMessageLoopStart(
      browser_message_loop_start_time, false);

  // TODO(msw): Consolidate with chrome's Browser::OnWindowDidShow()...
  const base::Time browser_window_display_time =
      base::Time::FromInternalValue(times->browser_window_display_time);
  base::TimeDelta browser_window_display_delta =
      browser_window_display_time - shell_process_creation_time;
  UMA_HISTOGRAM_LONG_TIMES("Startup.BrowserWindowDisplay",
                           browser_window_display_delta);

  // TODO(msw): Consolidate with chrome's PreMainMessageLoopRunImpl()...
  // TODO(msw): Need to measure the "browser_open_start" time for this delta...
  const base::Time browser_open_tabs_time =
      base::Time::FromInternalValue(times->browser_open_tabs_time);
  base::TimeDelta browser_open_tabs_delta =
      browser_open_tabs_time - shell_process_creation_time;
  UMA_HISTOGRAM_LONG_TIMES_100("Startup.BrowserOpenTabs",
                               browser_open_tabs_delta);

  // TODO(msw): Consolidate with chrome's first_web_contents_profiler.cc...
  const base::Time first_web_contents_main_frame_load_time =
      base::Time::FromInternalValue(
          times->first_web_contents_main_frame_load_time);
  base::TimeDelta first_web_contents_main_frame_load_delta =
      first_web_contents_main_frame_load_time - shell_process_creation_time;
  UMA_HISTOGRAM_LONG_TIMES_100("Startup.FirstWebContents.MainFrameLoad",
                               first_web_contents_main_frame_load_delta);

  // TODO(msw): Consolidate with chrome's first_web_contents_profiler.cc...
  const base::Time first_visually_non_empty_layout_time =
      base::Time::FromInternalValue(
          times->first_visually_non_empty_layout_time);
  base::TimeDelta first_web_contents_non_empty_paint_delta =
      first_visually_non_empty_layout_time - shell_process_creation_time;
  UMA_HISTOGRAM_LONG_TIMES_100("Startup.FirstWebContents.NonEmptyPaint",
                               first_web_contents_non_empty_paint_delta);
}

}  // namespace

// static
gin::WrapperInfo StatsCollectionController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
tracing::StartupPerformanceDataCollectorPtr StatsCollectionController::Install(
    blink::WebFrame* frame,
    mojo::ApplicationImpl* app) {
  // Only make startup tracing available when running in the context of a test.
  if (!app ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          tracing::kEnableStatsCollectionBindings)) {
    return nullptr;
  }

  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return nullptr;

  v8::Context::Scope context_scope(context);

  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:tracing");
  scoped_ptr<mojo::ApplicationConnection> connection =
      app->ConnectToApplication(request.Pass());
  if (!connection)
    return nullptr;
  tracing::StartupPerformanceDataCollectorPtr collector_for_controller;
  tracing::StartupPerformanceDataCollectorPtr collector_for_caller;
  connection->ConnectToService(&collector_for_controller);
  connection->ConnectToService(&collector_for_caller);

  gin::Handle<StatsCollectionController> controller = gin::CreateHandle(
      isolate, new StatsCollectionController(collector_for_controller.Pass()));
  DCHECK(!controller.IsEmpty());
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "statsCollectionController"),
              controller.ToV8());
  return collector_for_caller.Pass();
}

StatsCollectionController::StatsCollectionController(
    tracing::StartupPerformanceDataCollectorPtr collector)
    : startup_performance_data_collector_(collector.Pass()) {}

StatsCollectionController::~StatsCollectionController() {}

gin::ObjectTemplateBuilder StatsCollectionController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<StatsCollectionController>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("getHistogram", &StatsCollectionController::GetHistogram)
      .SetMethod("getBrowserHistogram",
                 &StatsCollectionController::GetBrowserHistogram);
}

std::string StatsCollectionController::GetHistogram(
    const std::string& histogram_name) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      tracing::kEnableStatsCollectionBindings));

  static bool startup_histogram_initialized = false;
  if (!startup_histogram_initialized) {
    // Get the startup performance times from the tracing service.
    auto callback = base::Bind(&GetStartupPerformanceTimesCallbackImpl);
    startup_performance_data_collector_->GetStartupPerformanceTimes(callback);
    startup_performance_data_collector_.WaitForIncomingResponse();
    DCHECK(base::StatisticsRecorder::IsActive());
    startup_histogram_initialized = true;
  }

  std::string histogram_json = "{}";
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);
  if (histogram)
    histogram->WriteJSON(&histogram_json);
  return histogram_json;
}

std::string StatsCollectionController::GetBrowserHistogram(
    const std::string& histogram_name) {
  return GetHistogram(histogram_name);
}

}  // namespace html_viewer
