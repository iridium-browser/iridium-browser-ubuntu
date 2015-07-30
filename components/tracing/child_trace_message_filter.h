// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CHILD_TRACE_MESSAGE_FILTER_H_
#define COMPONENTS_TRACING_CHILD_TRACE_MESSAGE_FILTER_H_

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "ipc/message_filter.h"

namespace base {
class MessageLoopProxy;
}

namespace tracing {

// This class sends and receives trace messages on child processes.
class ChildTraceMessageFilter : public IPC::MessageFilter {
 public:
  explicit ChildTraceMessageFilter(base::MessageLoopProxy* ipc_message_loop);

  // IPC::MessageFilter implementation.
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void SendGlobalMemoryDumpRequest(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const base::trace_event::MemoryDumpCallback& callback);

  base::MessageLoopProxy* ipc_message_loop() const { return ipc_message_loop_; }

 protected:
  ~ChildTraceMessageFilter() override;

 private:
  // Message handlers.
  void OnBeginTracing(const std::string& category_filter_str,
                      base::TimeTicks browser_time,
                      const std::string& options);
  void OnEndTracing();
  void OnEnableMonitoring(const std::string& category_filter_str,
                          base::TimeTicks browser_time,
                          const std::string& options);
  void OnDisableMonitoring();
  void OnCaptureMonitoringSnapshot();
  void OnGetTraceLogStatus();
  void OnSetWatchEvent(const std::string& category_name,
                       const std::string& event_name);
  void OnCancelWatchEvent();
  void OnWatchEventMatched();
  void OnProcessMemoryDumpRequest(
      const base::trace_event::MemoryDumpRequestArgs& args);
  void OnGlobalMemoryDumpResponse(uint64 dump_guid, bool success);

  // Callback from trace subsystem.
  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  void OnMonitoringTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  void OnProcessMemoryDumpDone(uint64 dump_guid, bool success);

  IPC::Sender* sender_;
  base::MessageLoopProxy* ipc_message_loop_;

  // guid of the outstanding request (to the Browser's MemoryDumpManager), if
  // any. 0 if there is no request pending.
  uint64 pending_memory_dump_guid_;

  // callback of the outstanding memory dump request, if any.
  base::trace_event::MemoryDumpCallback pending_memory_dump_callback_;

  DISALLOW_COPY_AND_ASSIGN(ChildTraceMessageFilter);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_CHILD_TRACE_MESSAGE_FILTER_H_
