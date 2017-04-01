// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/css/CSSTransitionData.h"

#include "core/animation/Timing.h"

namespace blink {

CSSTransitionData::CSSTransitionData() {
  m_propertyList.push_back(initialProperty());
}

CSSTransitionData::CSSTransitionData(const CSSTransitionData& other)
    : CSSTimingData(other), m_propertyList(other.m_propertyList) {}

bool CSSTransitionData::transitionsMatchForStyleRecalc(
    const CSSTransitionData& other) const {
  return m_propertyList == other.m_propertyList;
}

Timing CSSTransitionData::convertToTiming(size_t index) const {
  DCHECK_LT(index, m_propertyList.size());
  // Note that the backwards fill part is required for delay to work.
  Timing timing = CSSTimingData::convertToTiming(index);
  timing.fillMode = Timing::FillMode::NONE;
  return timing;
}

}  // namespace blink
