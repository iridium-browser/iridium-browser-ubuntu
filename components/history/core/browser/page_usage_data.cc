// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/page_usage_data.h"

#include <algorithm>

namespace history {

PageUsageData::PageUsageData(SegmentID id) : id_(id), score_(0.0) {
}

PageUsageData::~PageUsageData() {
}

// static
bool PageUsageData::Predicate(const PageUsageData* lhs,
                              const PageUsageData* rhs) {
  return lhs->GetScore() > rhs->GetScore();
}

}  // namespace history
