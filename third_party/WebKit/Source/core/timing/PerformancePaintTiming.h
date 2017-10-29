// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformancePaintTiming_h
#define PerformancePaintTiming_h

#include "core/CoreExport.h"
#include "core/timing/PerformanceEntry.h"

namespace blink {

class CORE_EXPORT PerformancePaintTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class PaintType { kFirstPaint, kFirstContentfulPaint };

  PerformancePaintTiming(PaintType, double start_time);
  ~PerformancePaintTiming() override;

  static String FromPaintTypeToString(PaintType);
};
}  // namespace blink

#endif  // PerformancePaintTiming_h
