// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADER_PROVIDER_H_
#define COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADER_PROVIDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial.h"
#include "base/synchronization/lock.h"
#include "components/metrics/metrics_service.h"
#include "components/variations/variations_associated_data.h"

namespace content {
class ResourceContext;
}

namespace net {
class HttpRequestHeaders;
}

class GURL;

template <typename T> struct DefaultSingletonTraits;

namespace variations {

// A helper class for maintaining client experiments and metrics state
// transmitted in custom HTTP request headers.
// This class is a thread-safe singleton.
class VariationsHttpHeaderProvider : public base::FieldTrialList::Observer,
                                     public metrics::SyntheticTrialObserver {
 public:
  static VariationsHttpHeaderProvider* GetInstance();

  // Adds Chrome experiment and metrics state as custom headers to |headers|.
  // Some headers may not be set given the |incognito| mode or whether
  // the user has |uma_enabled|.  Also, we never transmit headers to non-Google
  // sites, which is checked based on the destination |url|.
  void AppendHeaders(const GURL& url,
                     bool incognito,
                     bool uma_enabled,
                     net::HttpRequestHeaders* headers);

  // Sets *additional* variation ids and trigger variation ids to be encoded in
  // the X-Client-Data request header.  This is intended for development use to
  // force a server side experiment id.  |variation_ids| should be a
  // comma-separated string of numeric experiment ids.  If an id is prefixed
  // with "t" it will be treated as a trigger experiment id.
  bool SetDefaultVariationIds(const std::string& variation_ids);

  // Returns the HTTP header names which are added in this class.
  std::set<std::string> GetVariationHeaderNames() const;

  // Resets any cached state for tests.
  void ResetForTesting();

 private:
  friend struct DefaultSingletonTraits<VariationsHttpHeaderProvider>;

  FRIEND_TEST_ALL_PREFIXES(VariationsHttpHeaderProviderTest,
                           ShouldAppendHeaders);
  FRIEND_TEST_ALL_PREFIXES(VariationsHttpHeaderProviderTest,
                           SetDefaultVariationIds_Valid);
  FRIEND_TEST_ALL_PREFIXES(VariationsHttpHeaderProviderTest,
                           SetDefaultVariationIds_Invalid);
  FRIEND_TEST_ALL_PREFIXES(VariationsHttpHeaderProviderTest,
                           OnFieldTrialGroupFinalized);

  VariationsHttpHeaderProvider();
  ~VariationsHttpHeaderProvider() override;

  // base::FieldTrialList::Observer:
  // This will add the variation ID associated with |trial_name| and
  // |group_name| to the variation ID cache.
  void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                  const std::string& group_name) override;

  // metrics::SyntheticTrialObserver:
  void OnSyntheticTrialsChanged(
      const std::vector<metrics::SyntheticTrialGroup>& groups) override;

  // Prepares the variation IDs cache with initial values if not already done.
  // This method also registers the caller with the FieldTrialList to receive
  // new variation IDs.
  void InitVariationIDsCacheIfNeeded();

  // Takes whatever is currently in |variation_ids_set_| and recreates
  // |variation_ids_header_| with it.  Assumes the the |lock_| is currently
  // held.
  void UpdateVariationIDsHeaderValue();

  // Checks whether variation headers should be appended to requests to the
  // specified |url|. Returns true for google.<TLD> and youtube.<TLD> URLs.
  static bool ShouldAppendHeaders(const GURL& url);

  // Guards |variation_ids_cache_initialized_|, |variation_ids_set_| and
  // |variation_ids_header_|.
  base::Lock lock_;

  // Whether or not we've initialized the cache.
  bool variation_ids_cache_initialized_;

  // Keep a cache of variation IDs that are transmitted in headers to Google.
  // This consists of a list of valid IDs, and the actual transmitted header.
  std::set<VariationID> variation_ids_set_;
  std::set<VariationID> variation_trigger_ids_set_;

  // Provides the google experiment ids forced from command line.
  std::set<VariationID> default_variation_ids_set_;
  std::set<VariationID> default_trigger_id_set_;

  // Variations ids from synthetic field trials.
  std::set<VariationID> synthetic_variation_ids_set_;

  std::string variation_ids_header_;

  DISALLOW_COPY_AND_ASSIGN(VariationsHttpHeaderProvider);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_VARIATIONS_HTTP_HEADER_PROVIDER_H_
