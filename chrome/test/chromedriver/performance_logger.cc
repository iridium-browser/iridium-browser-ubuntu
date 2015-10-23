// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/performance_logger.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/session.h"

namespace {

// DevTools event domain prefixes to intercept.
const char* const kDomains[] = {"Network.", "Page.", "Timeline."};

// Whitelist of WebDriver commands on which to request buffered trace events.
const char* const kRequestTraceCommands[] = {"GetLog" /* required */,
    "Navigate"};

bool IsBrowserwideClient(DevToolsClient* client) {
  return (client->GetId() == DevToolsClientImpl::kBrowserwideDevToolsClientId);
}

bool IsEnabled(const PerfLoggingPrefs::InspectorDomainStatus& domain_status) {
    return (domain_status ==
            PerfLoggingPrefs::InspectorDomainStatus::kDefaultEnabled ||
            domain_status ==
            PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyEnabled);
}

// Returns whether |command| is in kRequestTraceCommands[] (case-insensitive).
bool ShouldRequestTraceEvents(const std::string& command) {
  for (size_t i_domain = 0; i_domain < arraysize(kRequestTraceCommands);
       ++i_domain) {
    if (base::EqualsCaseInsensitiveASCII(command,
                                         kRequestTraceCommands[i_domain]))
      return true;
  }
  return false;
}

// Returns whether the event belongs to one of kDomains.
bool ShouldLogEvent(const std::string& method) {
  for (size_t i_domain = 0; i_domain < arraysize(kDomains); ++i_domain) {
    if (base::StartsWith(method, kDomains[i_domain],
                         base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

}  // namespace

PerformanceLogger::PerformanceLogger(Log* log, const Session* session)
    : log_(log),
      session_(session),
      browser_client_(NULL),
      trace_buffering_(false) {}

PerformanceLogger::PerformanceLogger(Log* log,
                                     const Session* session,
                                     const PerfLoggingPrefs& prefs)
    : log_(log),
      session_(session),
      prefs_(prefs),
      browser_client_(NULL),
      trace_buffering_(false) {}

bool PerformanceLogger::subscribes_to_browser() {
  return true;
}

Status PerformanceLogger::OnConnected(DevToolsClient* client) {
  if (IsBrowserwideClient(client)) {
    browser_client_ = client;
    if (!prefs_.trace_categories.empty())  {
      Status status = StartTrace();
      if (status.IsError())
        return status;
    }
    return Status(kOk);
  }
  return EnableInspectorDomains(client);
}

Status PerformanceLogger::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (IsBrowserwideClient(client)) {
    return HandleTraceEvents(client, method, params);
  } else {
    return HandleInspectorEvents(client, method, params);
  }
}

Status PerformanceLogger::BeforeCommand(const std::string& command_name) {
  // Only dump trace buffer after tracing has been started.
  if (trace_buffering_ && ShouldRequestTraceEvents(command_name)) {
    Status status = CollectTraceEvents();
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

void PerformanceLogger::AddLogEntry(
    Log::Level level,
    const std::string& webview,
    const std::string& method,
    const base::DictionaryValue& params) {
  base::DictionaryValue log_message_dict;
  log_message_dict.SetString("webview", webview);
  log_message_dict.SetString("message.method", method);
  log_message_dict.Set("message.params", params.DeepCopy());
  std::string log_message_json;
  base::JSONWriter::Write(log_message_dict, &log_message_json);

  // TODO(klm): extract timestamp from params?
  // Look at where it is for Page, Network, Timeline, and trace events.
  log_->AddEntry(level, log_message_json);
}

void PerformanceLogger::AddLogEntry(
    const std::string& webview,
    const std::string& method,
    const base::DictionaryValue& params) {
  AddLogEntry(Log::kInfo, webview, method, params);
}

Status PerformanceLogger::EnableInspectorDomains(DevToolsClient* client) {
  std::vector<std::string> enable_commands;
  if (IsEnabled(prefs_.network))
    enable_commands.push_back("Network.enable");
  if (IsEnabled(prefs_.page))
    enable_commands.push_back("Page.enable");
  if (IsEnabled(prefs_.timeline)) {
    // Timeline feed implicitly disabled when trace categories are specified.
    // So even if kDefaultEnabled, don't enable unless empty |trace_categories|.
    if (prefs_.trace_categories.empty() || prefs_.timeline ==
        PerfLoggingPrefs::InspectorDomainStatus::kExplicitlyEnabled)
      enable_commands.push_back("Timeline.start");
  }
  for (std::vector<std::string>::const_iterator it = enable_commands.begin();
       it != enable_commands.end(); ++it) {
    base::DictionaryValue params;  // All the enable commands have empty params.
    Status status = client->SendCommand(*it, params);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status PerformanceLogger::HandleInspectorEvents(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (!ShouldLogEvent(method))
    return Status(kOk);

  AddLogEntry(client->GetId(), method, params);
  return Status(kOk);
}

Status PerformanceLogger::HandleTraceEvents(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (method == "Tracing.tracingComplete") {
    trace_buffering_ = false;
  } else if (method == "Tracing.dataCollected") {
    // The Tracing.dataCollected event contains a list of trace events.
    // Add each one as an individual log entry of method Tracing.dataCollected.
    const base::ListValue* traces;
    if (!params.GetList("value", &traces)) {
      return Status(kUnknownError,
                    "received DevTools trace data in unexpected format");
    }
    for (base::ListValue::const_iterator it = traces->begin();
             it != traces->end();
             ++it) {
      base::DictionaryValue* event_dict;
      if (!(*it)->GetAsDictionary(&event_dict))
        return Status(kUnknownError, "trace event must be a dictionary");
      AddLogEntry(client->GetId(), "Tracing.dataCollected", *event_dict);
    }
  } else if (method == "Tracing.bufferUsage") {
    // 'value' will be between 0-1 and represents how full the DevTools trace
    // buffer is. If the buffer is full, warn the user.
    double buffer_usage = 0;
    if (!params.GetDouble("value", &buffer_usage)) {
      // Tracing.bufferUsage event will occur once per second, and it really
      // only serves as a warning, so if we can't reliably tell whether the
      // buffer is full, just fail silently instead of spamming the logs.
      return Status(kOk);
    }
    if (buffer_usage >= 0.99999) {
      base::DictionaryValue params;
      std::string err("Chrome's trace buffer filled while collecting events, "
                      "so some trace events may have been lost");
      params.SetString("error", err);
      // Expose error to client via perf log using same format as other entries.
      AddLogEntry(Log::kWarning,
                  DevToolsClientImpl::kBrowserwideDevToolsClientId,
                  "Tracing.bufferUsage", params);
      LOG(WARNING) << err;
    }
  }
  return Status(kOk);
}

Status PerformanceLogger::StartTrace() {
  if (!browser_client_) {
    return Status(kUnknownError, "tried to start tracing, but connection to "
                  "browser was not yet established");
  }
  if (trace_buffering_) {
    LOG(WARNING) << "tried to start tracing, but a trace was already started";
    return Status(kOk);
  }
  base::DictionaryValue params;
  params.SetString("categories", prefs_.trace_categories);
  // Ask DevTools to report buffer usage.
  params.SetInteger("bufferUsageReportingInterval",
                    prefs_.buffer_usage_reporting_interval);
  Status status = browser_client_->SendCommand("Tracing.start", params);
  if (status.IsError()) {
    LOG(ERROR) << "error when starting trace: " << status.message();
    return status;
  }
  trace_buffering_ = true;
  return Status(kOk);
}

Status PerformanceLogger::CollectTraceEvents() {
  if (!browser_client_) {
    return Status(kUnknownError, "tried to collect trace events, but "
                  "connection to browser was not yet established");
  }
  if (!trace_buffering_) {
    return Status(kUnknownError, "tried to collect trace events, but tracing "
                  "was not started");
  }

  // As of r307466, DevTools no longer returns a response to Tracing.end
  // commands, so we need to ignore it here to avoid a timeout. See
  // https://code.google.com/p/chromedriver/issues/detail?id=997 for details.
  // TODO(samuong): find other commands where we don't need the response.
  bool wait_for_response = false;
  if (session_->chrome) {
    const BrowserInfo* browser_info = session_->chrome->GetBrowserInfo();
    if (browser_info->browser_name == "chrome" && browser_info->build_no < 2245)
      wait_for_response = true;
  }
  base::DictionaryValue params;
  Status status(kOk);
  if (wait_for_response)
    status = browser_client_->SendCommand("Tracing.end", params);
  else
    status = browser_client_->SendAsyncCommand("Tracing.end", params);
  if (status.IsError()) {
    LOG(ERROR) << "error when stopping trace: " << status.message();
    return status;
  }

  // Block up to 30 seconds until Tracing.tracingComplete event is received.
  status = browser_client_->HandleEventsUntil(
      base::Bind(&PerformanceLogger::IsTraceDone, base::Unretained(this)),
      base::TimeDelta::FromSeconds(30));
  if (status.IsError())
    return status;

  return StartTrace();
}

Status PerformanceLogger::IsTraceDone(bool* trace_done) const {
  *trace_done = !trace_buffering_;
  return Status(kOk);
}
