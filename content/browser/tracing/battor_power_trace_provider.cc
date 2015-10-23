// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/battor_power_trace_provider.h"

namespace content {

BattorPowerTraceProvider::BattorPowerTraceProvider() {}

BattorPowerTraceProvider::~BattorPowerTraceProvider() {}

bool BattorPowerTraceProvider::IsConnected() {
  return true;
}

bool BattorPowerTraceProvider::StartTracing() {
  return true;
}

bool BattorPowerTraceProvider::StopTracing() {
  return true;
}

void BattorPowerTraceProvider::GetLog(std::string* log_str) {
  // Get logs from battor.
  *log_str = "";
}
}
