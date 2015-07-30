// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_tracer.h"

#include <deque>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/gpu_timing.h"

namespace gpu {
namespace gles2 {

static const unsigned int kProcessInterval = 16;
static const char* kGpuTraceSourceNames[] = {
  "GroupMarker", // kTraceGroupMarker = 0,
  "TraceCHROMIUM", // kTraceCHROMIUM = 1,
  "TraceCmd", // kTraceDecoder = 2,
};
static_assert(NUM_TRACER_SOURCES == arraysize(kGpuTraceSourceNames),
              "Trace source names must match enumeration.");

static TraceOutputter* g_outputter_thread = NULL;

TraceMarker::TraceMarker(const std::string& category, const std::string& name)
    : category_(category),
      name_(name),
      trace_(NULL) {
}

TraceMarker::~TraceMarker() {
}

scoped_refptr<TraceOutputter> TraceOutputter::Create(const std::string& name) {
  if (!g_outputter_thread) {
    g_outputter_thread = new TraceOutputter(name);
  }
  return g_outputter_thread;
}

TraceOutputter::TraceOutputter(const std::string& name)
    : named_thread_(name.c_str()) {
  named_thread_.Start();
  named_thread_.Stop();
}

TraceOutputter::~TraceOutputter() { g_outputter_thread = NULL; }

void TraceOutputter::TraceDevice(GpuTracerSource source,
                                 const std::string& category,
                                 const std::string& name,
                                 int64 start_time,
                                 int64 end_time) {
  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);
  TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP2(
      TRACE_DISABLED_BY_DEFAULT("gpu.device"),
      name.c_str(),
      local_trace_device_id_,
      named_thread_.thread_id(),
      start_time,
      "gl_category",
      category.c_str(),
      "channel",
      kGpuTraceSourceNames[source]);
  TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP2(
      TRACE_DISABLED_BY_DEFAULT("gpu.device"),
      name.c_str(),
      local_trace_device_id_,
      named_thread_.thread_id(),
      end_time,
      "gl_category",
      category.c_str(),
      "channel",
      kGpuTraceSourceNames[source]);
  ++local_trace_device_id_;
}

void TraceOutputter::TraceServiceBegin(GpuTracerSource source,
                                       const std::string& category,
                                       const std::string& name) {
  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);
  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TTS2(
      TRACE_DISABLED_BY_DEFAULT("gpu.service"),
      name.c_str(), local_trace_service_id_,
      "gl_category", category.c_str(),
      "channel", kGpuTraceSourceNames[source]);

  trace_service_id_stack_[source].push(local_trace_service_id_);
  ++local_trace_service_id_;
}

void TraceOutputter::TraceServiceEnd(GpuTracerSource source,
                                     const std::string& category,
                                     const std::string& name) {
  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);
  DCHECK(!trace_service_id_stack_[source].empty());
  const uint64 local_trace_id = trace_service_id_stack_[source].top();
  trace_service_id_stack_[source].pop();

  TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TTS2(
      TRACE_DISABLED_BY_DEFAULT("gpu.service"),
      name.c_str(), local_trace_id,
      "gl_category", category.c_str(),
      "channel", kGpuTraceSourceNames[source]);
}

GPUTrace::GPUTrace(scoped_refptr<Outputter> outputter,
                   gfx::GPUTimingClient* gpu_timing_client,
                   const GpuTracerSource source,
                   const std::string& category,
                   const std::string& name,
                   const bool tracing_service,
                   const bool tracing_device)
    : source_(source),
      category_(category),
      name_(name),
      outputter_(outputter),
      service_enabled_(tracing_service),
      device_enabled_(tracing_device) {
  if (tracing_device &&
      gpu_timing_client->IsAvailable() &&
      gpu_timing_client->IsTimerOffsetAvailable()) {
    gpu_timer_ = gpu_timing_client->CreateGPUTimer();
  }
}

GPUTrace::~GPUTrace() {
}

void GPUTrace::Destroy(bool have_context) {
  if (gpu_timer_.get()) {
    gpu_timer_->Destroy(have_context);
  }
}

void GPUTrace::Start() {
  if (service_enabled_) {
    outputter_->TraceServiceBegin(source_, category_, name_);
  }
  if (gpu_timer_.get()) {
    gpu_timer_->Start();
  }
}

void GPUTrace::End() {
  if (gpu_timer_.get()) {
    gpu_timer_->End();
  }
  if (service_enabled_) {
    outputter_->TraceServiceEnd(source_, category_, name_);
  }
}

bool GPUTrace::IsAvailable() {
  return !gpu_timer_.get() || gpu_timer_->IsAvailable();
}

void GPUTrace::Process() {
  if (gpu_timer_.get()) {
    DCHECK(IsAvailable());

    int64 start = 0;
    int64 end = 0;
    gpu_timer_->GetStartEndTimestamps(&start, &end);
    outputter_->TraceDevice(source_, category_, name_, start, end);
  }
}

GPUTracer::GPUTracer(gles2::GLES2Decoder* decoder)
    : gpu_trace_srv_category(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.service"))),
      gpu_trace_dev_category(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.device"))),
      decoder_(decoder),
      gpu_executing_(false),
      process_posted_(false) {
  DCHECK(decoder_);
  gfx::GLContext* context = decoder_->GetGLContext();
  if (context) {
    gpu_timing_client_ = context->CreateGPUTimingClient();
  } else {
    gpu_timing_client_ = new gfx::GPUTimingClient();
  }
}

GPUTracer::~GPUTracer() {
}

void GPUTracer::Destroy(bool have_context) {
  for (int n = 0; n < NUM_TRACER_SOURCES; n++) {
    for (size_t i = 0; i < markers_[n].size(); i++) {
      TraceMarker& marker = markers_[n][i];
      if (marker.trace_.get()) {
        marker.trace_->Destroy(have_context);
        marker.trace_ = 0;
      }
    }
  }

  ClearFinishedTraces(have_context);
}

bool GPUTracer::BeginDecoding() {
  if (gpu_executing_)
    return false;

  if (!outputter_) {
    outputter_ = CreateOutputter(gpu_timing_client_->GetTimerTypeName());
  }

  if (*gpu_trace_dev_category == '\0') {
    // If GPU device category is off, invalidate timing sync.
    gpu_timing_client_->InvalidateTimerOffset();
  }

  gpu_executing_ = true;
  if (IsTracing()) {
    gpu_timing_client_->CheckAndResetTimerErrors();
    // Begin a Trace for all active markers
    for (int n = 0; n < NUM_TRACER_SOURCES; n++) {
      for (size_t i = 0; i < markers_[n].size(); i++) {
        TraceMarker& trace_marker = markers_[n][i];
        trace_marker.trace_ =
            new GPUTrace(outputter_, gpu_timing_client_.get(),
                         static_cast<GpuTracerSource>(n),
                         trace_marker.category_, trace_marker.name_,
                         *gpu_trace_srv_category != 0,
                         *gpu_trace_dev_category != 0);
        trace_marker.trace_->Start();
      }
    }
  }
  return true;
}

bool GPUTracer::EndDecoding() {
  if (!gpu_executing_)
    return false;

  // End Trace for all active markers
  if (IsTracing()) {
    for (int n = 0; n < NUM_TRACER_SOURCES; n++) {
      if (!markers_[n].empty()) {
        for (int i = static_cast<int>(markers_[n].size()) - 1; i >= 0; --i) {
          TraceMarker& marker = markers_[n][i];
          if (marker.trace_.get()) {
            marker.trace_->End();

            finished_traces_.push_back(marker.trace_);
            marker.trace_ = 0;
          }
        }
      }
    }
    IssueProcessTask();
  }

  gpu_executing_ = false;

  // NOTE(vmiura): glFlush() here can help give better trace results,
  // but it distorts the normal device behavior.
  return true;
}

bool GPUTracer::Begin(const std::string& category, const std::string& name,
                      GpuTracerSource source) {
  if (!gpu_executing_)
    return false;

  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);

  // Push new marker from given 'source'
  markers_[source].push_back(TraceMarker(category, name));

  // Create trace
  if (IsTracing()) {
    scoped_refptr<GPUTrace> trace = new GPUTrace(
        outputter_, gpu_timing_client_.get(), source, category, name,
        *gpu_trace_srv_category != 0,
        *gpu_trace_dev_category != 0);
    trace->Start();
    markers_[source].back().trace_ = trace;
  }

  return true;
}

bool GPUTracer::End(GpuTracerSource source) {
  if (!gpu_executing_)
    return false;

  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);

  // Pop last marker with matching 'source'
  if (!markers_[source].empty()) {
    scoped_refptr<GPUTrace> trace = markers_[source].back().trace_;
    if (trace.get()) {
      if (IsTracing()) {
        trace->End();
      }

      finished_traces_.push_back(trace);
      IssueProcessTask();
    }

    markers_[source].pop_back();
    return true;
  }
  return false;
}

bool GPUTracer::IsTracing() {
  return (*gpu_trace_srv_category != 0) || (*gpu_trace_dev_category != 0);
}

const std::string& GPUTracer::CurrentCategory(GpuTracerSource source) const {
  if (source >= 0 &&
      source < NUM_TRACER_SOURCES &&
      !markers_[source].empty()) {
    return markers_[source].back().category_;
  }
  return base::EmptyString();
}

const std::string& GPUTracer::CurrentName(GpuTracerSource source) const {
  if (source >= 0 &&
      source < NUM_TRACER_SOURCES &&
      !markers_[source].empty()) {
    return markers_[source].back().name_;
  }
  return base::EmptyString();
}

scoped_refptr<Outputter> GPUTracer::CreateOutputter(const std::string& name) {
  return TraceOutputter::Create(name);
}

void GPUTracer::PostTask() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&GPUTracer::Process, base::AsWeakPtr(this)),
      base::TimeDelta::FromMilliseconds(kProcessInterval));
}

void GPUTracer::Process() {
  process_posted_ = false;
  ProcessTraces();
  IssueProcessTask();
}

void GPUTracer::ProcessTraces() {
  if (!gpu_timing_client_->IsAvailable()) {
    ClearFinishedTraces(false);
    return;
  }

  TRACE_EVENT0("gpu", "GPUTracer::ProcessTraces");

  // Make owning decoder's GL context current
  if (!decoder_->MakeCurrent()) {
    // Skip subsequent GL calls if MakeCurrent fails
    ClearFinishedTraces(false);
    return;
  }

  // Check if timers are still valid (e.g: a disjoint operation
  // might have occurred.)
  if (gpu_timing_client_->CheckAndResetTimerErrors()) {
    ClearFinishedTraces(true);
  }

  while (!finished_traces_.empty()) {
    scoped_refptr<GPUTrace>& trace = finished_traces_.front();
    if (trace->IsDeviceTraceEnabled()) {
      if (!finished_traces_.front()->IsAvailable())
        break;
      finished_traces_.front()->Process();
    }
    finished_traces_.front()->Destroy(true);
    finished_traces_.pop_front();
  }

  // Clear pending traces if there were are any errors
  GLenum err = glGetError();
  if (err != GL_NO_ERROR)
    ClearFinishedTraces(true);
}

void GPUTracer::ClearFinishedTraces(bool have_context) {
  while (!finished_traces_.empty()) {
    finished_traces_.front()->Destroy(have_context);
    finished_traces_.pop_front();
  }
}

void GPUTracer::IssueProcessTask() {
  if (finished_traces_.empty() || process_posted_)
    return;

  process_posted_ = true;
  PostTask();
}

}  // namespace gles2
}  // namespace gpu
