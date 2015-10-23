// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_TASK_COST_ESTIMATOR_H_
#define COMPONENTS_SCHEDULER_RENDERER_TASK_COST_ESTIMATOR_H_

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "cc/base/rolling_time_delta_history.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

// Estimates the cost of running tasks based on historical timing data.
class SCHEDULER_EXPORT TaskCostEstimator
    : public base::MessageLoop::TaskObserver {
 public:
  TaskCostEstimator(int sample_count, double estimation_percentile);
  ~TaskCostEstimator() override;

  base::TimeDelta expected_task_duration() const {
    return expected_task_duration_;
  }

  // TaskObserver implementation:
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

 protected:
  // Virtual for testing.
  virtual base::TimeTicks Now();

 private:
  cc::RollingTimeDeltaHistory rolling_time_delta_history_;
  int outstanding_task_count_;
  double estimation_percentile_;
  base::TimeTicks task_start_time_;
  base::TimeDelta expected_task_duration_;

  DISALLOW_COPY_AND_ASSIGN(TaskCostEstimator);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_TASK_COST_ESTIMATOR_H_
