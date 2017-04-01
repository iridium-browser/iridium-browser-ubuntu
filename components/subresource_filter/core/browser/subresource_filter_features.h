// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_FEATURES_H_
#define COMPONENTS_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_FEATURES_H_

#include "base/feature_list.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/common/activation_state.h"

namespace subresource_filter {

// The master toggle to enable/disable the Safe Browsing Subresource Filter.
extern const base::Feature kSafeBrowsingSubresourceFilter;

// Name/values of the variation parameter controlling maximum activation state.
extern const char kActivationStateParameterName[];
extern const char kActivationStateDryRun[];
extern const char kActivationStateEnabled[];
extern const char kActivationStateDisabled[];

extern const char kActivationScopeParameterName[];
extern const char kActivationScopeAllSites[];
extern const char kActivationScopeActivationList[];
extern const char kActivationScopeNoSites[];

extern const char kActivationListsParameterName[];
extern const char kActivationListSocialEngineeringAdsInterstitial[];
extern const char kActivationListPhishingInterstitial[];

extern const char kPerformanceMeasurementRateParameterName[];

// Returns the maximum degree to which subresource filtering should be activated
// on any RenderFrame. This will be ActivationState::DISABLED unless the feature
// is enabled and variation parameters prescribe a higher activation state.
ActivationState GetMaximumActivationState();

// Returns the current activation scope, that is, the subset of page loads where
// subresource filtering should be activated. The function returns
// ActivationScope::NO_SITES unless the feature is enabled and variation
// parameters prescribe a wider activation scope.
ActivationScope GetCurrentActivationScope();

// Returns current activation list, based on the values from variation params in
// the feature |kSafeBrowsingSubresourceFilter|. When the corresponding
// variation param is empty, returns most conservative ActivationList::NONE.
ActivationList GetCurrentActivationList();

// Returns a number in the range [0, 1], indicating the fraction of page loads
// that should have extended performance measurements enabled. The rate will be
// 0 unless a greater frequency is specified by variation parameters.
double GetPerformanceMeasurementRate();

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_FEATURES_H_
