// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_SERVER_LOG_ENTRY_CLIENT_H_
#define REMOTING_CLIENT_SERVER_LOG_ENTRY_CLIENT_H_

#include "base/time/time.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/errors.h"

namespace remoting {

class ServerLogEntry;

namespace protocol {
class PerformanceTracker;
}  // namespace protocol

// Constructs a log entry for a session state change.
std::unique_ptr<ServerLogEntry> MakeLogEntryForSessionStateChange(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error);

// Constructs a log entry for reporting statistics.
std::unique_ptr<ServerLogEntry> MakeLogEntryForStatistics(
    protocol::PerformanceTracker* statistics);

// Constructs a log entry for reporting session ID is old.
std::unique_ptr<ServerLogEntry> MakeLogEntryForSessionIdOld(
    const std::string& session_id);

// Constructs a log entry for reporting session ID is old.
std::unique_ptr<ServerLogEntry> MakeLogEntryForSessionIdNew(
    const std::string& session_id);

void AddClientFieldsToLogEntry(ServerLogEntry* entry);
void AddSessionIdToLogEntry(ServerLogEntry* entry, const std::string& id);
void AddSessionDurationToLogEntry(ServerLogEntry* entry,
                                  base::TimeDelta duration);

}  // namespace remoting

#endif  // REMOTING_CLIENT_SERVER_LOG_ENTRY_CLIENT_H_
