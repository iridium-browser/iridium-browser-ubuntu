// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/server_log_entry_client.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "remoting/protocol/performance_tracker.h"
#include "remoting/signaling/server_log_entry.h"

using base::StringPrintf;
using base::SysInfo;
using remoting::protocol::ConnectionToHost;
using remoting::protocol::ErrorCode;

namespace remoting {

namespace {
const char kValueRoleClient[] = "client";

const char kValueEventNameSessionState[] = "session-state";
const char kValueEventNameStatistics[] = "connection-statistics";
const char kValueEventNameSessionIdOld[] = "session-id-old";
const char kValueEventNameSessionIdNew[] = "session-id-new";

const char kKeySessionId[] = "session-id";
const char kKeySessionDuration[] = "session-duration";

const char kKeySessionState[] = "session-state";
const char kKeyConnectionError[] = "connection-error";
const char kValueSessionStateConnected[] = "connected";
const char kValueSessionStateClosed[] = "closed";

const char kKeyOsName[] = "os-name";
const char kKeyOsVersion[] = "os-version";
const char kKeyAppVersion[] = "app-version";

const char* GetValueSessionState(ConnectionToHost::State state) {
  switch (state) {
    // Where possible, these are the same strings that the webapp sends for the
    // corresponding state - see remoting/webapp/server_log_entry.js.
    case ConnectionToHost::INITIALIZING:
      return "initializing";
    case ConnectionToHost::CONNECTING:
      return "connecting";
    case ConnectionToHost::AUTHENTICATED:
      return "authenticated";
    case ConnectionToHost::CONNECTED:
      return kValueSessionStateConnected;
    case ConnectionToHost::FAILED:
      return "connection-failed";
    case ConnectionToHost::CLOSED:
      return kValueSessionStateClosed;
  }
  NOTREACHED();
  return nullptr;
}

const char* GetValueError(ErrorCode error) {
  switch (error) {
    // Where possible, these are the same strings that the webapp sends for the
    // corresponding error - see remoting/webapp/crd/js/server_log_entry.js.
    case protocol::OK:
      return "none";
    case protocol::PEER_IS_OFFLINE:
      return "host-is-offline";
    case protocol::SESSION_REJECTED:
      return "session-rejected";
    case protocol::INCOMPATIBLE_PROTOCOL:
      return "incompatible-protocol";
    case protocol::AUTHENTICATION_FAILED:
      return "authentication-failed";
    case protocol::CHANNEL_CONNECTION_ERROR:
      return "p2p-failure";
    case protocol::SIGNALING_ERROR:
      return "network-failure";
    case protocol::SIGNALING_TIMEOUT:
      return "network-failure";
    case protocol::HOST_OVERLOAD:
      return "host-overload";
    case protocol::MAX_SESSION_LENGTH:
      return "max-session-length";
    case protocol::HOST_CONFIGURATION_ERROR:
      return "host-configuration-error";
    case protocol::UNKNOWN_ERROR:
      return "unknown-error";
    case protocol::INVALID_ACCOUNT:
      return "invalid-account";
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

std::unique_ptr<ServerLogEntry> MakeLogEntryForSessionStateChange(
    ConnectionToHost::State state,
    ErrorCode error) {
  std::unique_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->AddRoleField(kValueRoleClient);
  entry->AddEventNameField(kValueEventNameSessionState);

  entry->Set(kKeySessionState, GetValueSessionState(state));
  if (error != protocol::OK) {
    entry->Set(kKeyConnectionError, GetValueError(error));
  }

  return entry;
}

std::unique_ptr<ServerLogEntry> MakeLogEntryForStatistics(
    protocol::PerformanceTracker* perf_tracker) {
  std::unique_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->AddRoleField(kValueRoleClient);
  entry->AddEventNameField(kValueEventNameStatistics);

  entry->Set("video-bandwidth",
             StringPrintf("%.2f", perf_tracker->video_bandwidth()));
  entry->Set("capture-latency",
             StringPrintf("%.2f", perf_tracker->video_capture_ms().Average()));
  entry->Set("encode-latency",
             StringPrintf("%.2f", perf_tracker->video_encode_ms().Average()));
  entry->Set("decode-latency",
             StringPrintf("%.2f", perf_tracker->video_decode_ms().Average()));
  entry->Set("render-latency",
             StringPrintf("%.2f", perf_tracker->video_paint_ms().Average()));
  entry->Set("roundtrip-latency",
             StringPrintf("%.2f", perf_tracker->round_trip_ms().Average()));

  return entry;
}

std::unique_ptr<ServerLogEntry> MakeLogEntryForSessionIdOld(
    const std::string& session_id) {
  std::unique_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->AddRoleField(kValueRoleClient);
  entry->AddEventNameField(kValueEventNameSessionIdOld);
  AddSessionIdToLogEntry(entry.get(), session_id);
  return entry;
}

std::unique_ptr<ServerLogEntry> MakeLogEntryForSessionIdNew(
    const std::string& session_id) {
  std::unique_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->AddRoleField(kValueRoleClient);
  entry->AddEventNameField(kValueEventNameSessionIdNew);
  AddSessionIdToLogEntry(entry.get(), session_id);
  return entry;
}

void AddClientFieldsToLogEntry(ServerLogEntry* entry) {
  entry->Set(kKeyOsName, SysInfo::OperatingSystemName());
  entry->Set(kKeyOsVersion, SysInfo::OperatingSystemVersion());
  entry->Set(kKeyAppVersion, STRINGIZE(VERSION));
  entry->AddCpuField();
}

void AddSessionIdToLogEntry(ServerLogEntry* entry, const std::string& id) {
  entry->Set(kKeySessionId, id);
}

void AddSessionDurationToLogEntry(ServerLogEntry* entry,
                                  base::TimeDelta duration) {
  entry->Set(kKeySessionDuration, base::Int64ToString(duration.InSeconds()));
}

}  // namespace remoting
