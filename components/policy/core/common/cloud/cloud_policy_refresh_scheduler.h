// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_H_

#include <stdint.h>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/policy_export.h"
#include "net/base/network_change_notifier.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

// Observes CloudPolicyClient and CloudPolicyStore to trigger periodic policy
// fetches and issue retries on error conditions.
class POLICY_EXPORT CloudPolicyRefreshScheduler
    : public CloudPolicyClient::Observer,
      public CloudPolicyStore::Observer,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // Refresh constants.
  static const int64_t kDefaultRefreshDelayMs;
  static const int64_t kUnmanagedRefreshDelayMs;
  static const int64_t kWithInvalidationsRefreshDelayMs;
  static const int64_t kInitialErrorRetryDelayMs;

  // Refresh delay bounds.
  static const int64_t kRefreshDelayMinMs;
  static const int64_t kRefreshDelayMaxMs;

  // |client| and |store| pointers must stay valid throughout the
  // lifetime of CloudPolicyRefreshScheduler.
  CloudPolicyRefreshScheduler(
      CloudPolicyClient* client,
      CloudPolicyStore* store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~CloudPolicyRefreshScheduler() override;

  base::Time last_refresh() const { return last_refresh_; }

  // Sets the refresh delay to |refresh_delay| (actual refresh delay may vary
  // due to min/max clamping, changes to delay due to invalidations, etc).
  void SetDesiredRefreshDelay(int64_t refresh_delay);

  // Returns the current fixed refresh delay (can vary depending on whether
  // invalidations are available or not).
  int64_t GetActualRefreshDelay() const;

  // Requests a policy refresh to be performed soon.
  void RefreshSoon();

  // The refresh scheduler starts by assuming that invalidations are not
  // available. This call can be used to signal whether the invalidations
  // service is available or not, and can be called multiple times.
  // When the invalidations service is available then the refresh rate is much
  // lower.
  void SetInvalidationServiceAvailability(bool is_available);

  // Whether the invalidations service is available and receiving notifications
  // of policy updates.
  bool invalidations_available() {
    return invalidations_available_;
  }

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // net::NetworkChangeNotifier::IPAddressObserver:
  void OnIPAddressChanged() override;

 private:
  // Initializes |last_refresh_| to the policy timestamp from |store_| in case
  // there is policy present that indicates this client is not managed. This
  // results in policy fetches only to occur after the entire unmanaged refresh
  // delay expires, even over restarts. For managed clients, we want to trigger
  // a refresh on every restart.
  void UpdateLastRefreshFromPolicy();

  // Schedules a refresh to be performed immediately.
  void RefreshNow();

  // Evaluates when the next refresh is pending and updates the callback to
  // execute that refresh at the appropriate time.
  void ScheduleRefresh();

  // Triggers a policy refresh.
  void PerformRefresh();

  // Schedules a policy refresh to happen after |delta_ms| milliseconds,
  // relative to |last_refresh_|.
  void RefreshAfter(int delta_ms);

  CloudPolicyClient* client_;
  CloudPolicyStore* store_;

  // For scheduling delayed tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The delayed refresh callback.
  base::CancelableClosure refresh_callback_;

  // The last time a refresh callback completed.
  base::Time last_refresh_;

  // Error retry delay in milliseconds.
  int64_t error_retry_delay_ms_;

  // The refresh delay.
  int64_t refresh_delay_ms_;

  // Whether the invalidations service is available and receiving notifications
  // of policy updates.
  bool invalidations_available_;

  // Used to measure how long it took for the invalidations service to report
  // its initial status.
  base::Time creation_time_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyRefreshScheduler);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_H_
