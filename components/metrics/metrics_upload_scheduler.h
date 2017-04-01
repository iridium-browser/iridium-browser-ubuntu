// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_UPLOAD_SCHEDULER_H_
#define COMPONENTS_METRICS_METRICS_UPLOAD_SCHEDULER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/metrics/metrics_scheduler.h"

namespace metrics {

// Scheduler task to drive a MetricsService object's uploading.
class MetricsUploadScheduler : public MetricsScheduler {
 public:
  // Creates MetricsUploadScheduler object with the given |upload_callback|
  // callback to call when uploading should happen.  The callback must
  // arrange to call either UploadFinished or UploadCancelled on completion.
  explicit MetricsUploadScheduler(const base::Closure& upload_callback);
  ~MetricsUploadScheduler() override;

  // Callback from MetricsService when a triggered upload finishes.
  void UploadFinished(bool server_is_healthy);

  // Callback from MetricsService when an upload is cancelled.
  void UploadCancelled();

  // Callback from MetricsService when an upload is cancelled because it would
  // be over the allowed data usage cap.
  void UploadOverDataUsageCap();

 private:
  // Record the upload interval time.
  virtual void LogActualUploadInterval(base::TimeDelta interval);

  // MetricsScheduler:
  void TriggerTask() override;

  // The tick count of the last time log upload has been finished and null if no
  // upload has been done yet.
  base::TimeTicks last_upload_finish_time_;

  // Time to wait for the next upload attempt.
  base::TimeDelta upload_interval_;

  DISALLOW_COPY_AND_ASSIGN(MetricsUploadScheduler);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_UPLOAD_SCHEDULER_H_
