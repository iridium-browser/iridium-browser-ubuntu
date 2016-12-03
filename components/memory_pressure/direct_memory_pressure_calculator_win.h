// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_PRESSURE_DIRECT_MEMORY_PRESSURE_CALCULATOR_WIN_H_
#define COMPONENTS_MEMORY_PRESSURE_DIRECT_MEMORY_PRESSURE_CALCULATOR_WIN_H_

#include "base/macros.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "components/memory_pressure/memory_pressure_calculator.h"

namespace memory_pressure {

#if defined(MEMORY_PRESSURE_IS_POLLING)

// OS-specific implementation of MemoryPressureCalculator. This is only defined
// and used on platforms that do not have native memory pressure signals
// (ChromeOS, Linux, Windows). OSes that do have native signals simply hook into
// the appropriate subsystem (Android, Mac OS X).
class DirectMemoryPressureCalculator : public MemoryPressureCalculator {
 public:
  // Exposed for unittesting. See .cc file for detailed discussion of these
  // constants.
  static const int kLargeMemoryThresholdMb;
  static const int kSmallMemoryDefaultModerateThresholdMb;
  static const int kSmallMemoryDefaultCriticalThresholdMb;
  static const int kLargeMemoryDefaultModerateThresholdMb;
  static const int kLargeMemoryDefaultCriticalThresholdMb;

  // Default constructor. Will choose thresholds automatically based on the
  // actual amount of system memory installed.
  DirectMemoryPressureCalculator();

  // Constructor with explicit memory thresholds. These represent the amount of
  // free memory below which the applicable memory pressure state applies.
  DirectMemoryPressureCalculator(int moderate_threshold_mb,
                                 int critical_threshold_mb);

  ~DirectMemoryPressureCalculator() override {}

  // Calculates the current pressure level.
  MemoryPressureLevel CalculateCurrentPressureLevel() override;

  int moderate_threshold_mb() const { return moderate_threshold_mb_; }
  int critical_threshold_mb() const { return critical_threshold_mb_; }

 private:
  friend class TestDirectMemoryPressureCalculator;

  // Gets system memory status. This is virtual as a unittesting hook and by
  // default this invokes base::GetSystemMemoryInfo.
  virtual bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* mem_info) const;

  // Uses GetSystemMemoryInfo to automatically infer appropriate values for
  // moderate_threshold_mb_ and critical_threshold_mb_.
  void InferThresholds();

  // Threshold amounts of available memory that trigger pressure levels. See
  // memory_pressure_monitor_win.cc for a discussion of reasonable values for
  // these.
  int moderate_threshold_mb_;
  int critical_threshold_mb_;

  DISALLOW_COPY_AND_ASSIGN(DirectMemoryPressureCalculator);
};

#endif  // defined(MEMORY_PRESSURE_IS_POLLING)

}  // namespace memory_pressure

#endif  // COMPONENTS_MEMORY_PRESSURE_DIRECT_MEMORY_PRESSURE_CALCULATOR_WIN_H_
