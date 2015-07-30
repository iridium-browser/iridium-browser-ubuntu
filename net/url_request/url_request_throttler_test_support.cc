// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_test_support.h"

#include "net/url_request/url_request_throttler_entry.h"

namespace net {

TestTickClock::TestTickClock() {}

TestTickClock::TestTickClock(base::TimeTicks now) : now_ticks_(now) {}

TestTickClock::~TestTickClock() {}

base::TimeTicks TestTickClock::NowTicks() {
  return now_ticks_;
}

MockURLRequestThrottlerHeaderAdapter::MockURLRequestThrottlerHeaderAdapter(
    int response_code)
    : fake_response_code_(response_code) {
}

MockURLRequestThrottlerHeaderAdapter::MockURLRequestThrottlerHeaderAdapter(
    const std::string& retry_value,
    const std::string& opt_out_value,
    int response_code)
    : fake_retry_value_(retry_value),
      fake_opt_out_value_(opt_out_value),
      fake_response_code_(response_code) {
}

MockURLRequestThrottlerHeaderAdapter::~MockURLRequestThrottlerHeaderAdapter() {
}

std::string MockURLRequestThrottlerHeaderAdapter::GetNormalizedValue(
    const std::string& key) const {
  if (key ==
      URLRequestThrottlerEntry::kExponentialThrottlingHeader &&
      !fake_opt_out_value_.empty()) {
    return fake_opt_out_value_;
  }

  return std::string();
}

int MockURLRequestThrottlerHeaderAdapter::GetResponseCode() const {
  return fake_response_code_;
}

}  // namespace net
