// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/metrics_helper.h"

#include "base/metrics/histogram.h"
#include "components/history/core/browser/history_service.h"
#include "components/rappor/rappor_service.h"
#include "components/rappor/rappor_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace security_interstitials {

namespace {

// Used for setting bits in Rappor's "interstitial.*.flags"
enum InterstitialFlagBits {
  DID_PROCEED = 0,
  IS_REPEAT_VISIT = 1,
  HIGHEST_USED_BIT = 1
};

// Directly adds to the UMA histograms, using the same properties as
// UMA_HISTOGRAM_ENUMERATION, because the macro doesn't allow non-constant
// histogram names.
void RecordSingleDecisionToMetrics(MetricsHelper::Decision decision,
                                   const std::string& histogram_name) {
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, MetricsHelper::MAX_DECISION,
      MetricsHelper::MAX_DECISION + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(decision);
}

void RecordSingleInteractionToMetrics(MetricsHelper::Interaction interaction,
                                      const std::string& histogram_name) {
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, MetricsHelper::MAX_INTERACTION,
      MetricsHelper::MAX_INTERACTION + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(interaction);
}

}  // namespace

MetricsHelper::ReportDetails::ReportDetails()
    : rappor_report_type(rappor::NUM_RAPPOR_TYPES) {}

MetricsHelper::MetricsHelper(const GURL& request_url,
                             const ReportDetails settings,
                             history::HistoryService* history_service,
                             rappor::RapporService* rappor_service)
    : request_url_(request_url),
      settings_(settings),
      rappor_service_(rappor_service),
      num_visits_(-1) {
  DCHECK(!settings_.metric_prefix.empty());
  if (settings_.rappor_report_type == rappor::NUM_RAPPOR_TYPES)  // Default.
    rappor_service_ = nullptr;
  DCHECK(!rappor_service_ || !settings_.rappor_prefix.empty());
  if (history_service) {
    history_service->GetVisibleVisitCountToHost(
        request_url_,
        base::Bind(&MetricsHelper::OnGotHistoryCount, base::Unretained(this)),
        &request_tracker_);
  }
}

void MetricsHelper::RecordUserDecision(Decision decision) {
  const std::string histogram_name(
      "interstitial." + settings_.metric_prefix + ".decision");

  RecordUserDecisionToMetrics(decision, histogram_name);
  // Record additional information about sites that users have visited before.
  // Report |decision| and SHOW together, filtered by the same history state
  // so they they are paired regardless of when if num_visits_ is populated.
  if (num_visits_ > 0 && (decision == PROCEED || decision == DONT_PROCEED)) {
    RecordUserDecisionToMetrics(SHOW, histogram_name + ".repeat_visit");
    RecordUserDecisionToMetrics(decision, histogram_name + ".repeat_visit");
  }
  RecordUserDecisionToRappor(decision);
  RecordExtraUserDecisionMetrics(decision);
}

void MetricsHelper::RecordUserDecisionToMetrics(
    Decision decision,
    const std::string& histogram_name) {
  // Record the decision, and additionally |with extra_suffix|.
  RecordSingleDecisionToMetrics(decision, histogram_name);
  if (!settings_.extra_suffix.empty()) {
    RecordSingleDecisionToMetrics(
        decision, histogram_name + "." + settings_.extra_suffix);
  }
}

void MetricsHelper::RecordUserDecisionToRappor(Decision decision) {
  if (!rappor_service_ || (decision != PROCEED && decision != DONT_PROCEED))
    return;

  scoped_ptr<rappor::Sample> sample =
      rappor_service_->CreateSample(settings_.rappor_report_type);

  // This will populate, for example, "intersitial.malware.domain" or
  // "interstitial.ssl2.domain".  |domain| will be empty for hosts w/o TLDs.
  const std::string domain =
      rappor::GetDomainAndRegistrySampleFromGURL(request_url_);
  sample->SetStringField("domain", domain);

  // Only report history and decision if we have history data.
  if (num_visits_ >= 0) {
    int flags = 0;
    if (decision == PROCEED)
      flags |= 1 << InterstitialFlagBits::DID_PROCEED;
    if (num_visits_ > 0)
      flags |= 1 << InterstitialFlagBits::IS_REPEAT_VISIT;
    // e.g. "interstitial.malware.flags"
    sample->SetFlagsField("flags", flags,
                          InterstitialFlagBits::HIGHEST_USED_BIT + 1);
  }
  rappor_service_->RecordSampleObj("interstitial." + settings_.rappor_prefix,
                                   sample.Pass());
}

void MetricsHelper::RecordUserInteraction(Interaction interaction) {
  const std::string histogram_name(
      "interstitial." + settings_.metric_prefix + ".interaction");

  RecordSingleInteractionToMetrics(interaction, histogram_name);
  if (!settings_.extra_suffix.empty()) {
    RecordSingleInteractionToMetrics(
        interaction, histogram_name + "." + settings_.extra_suffix);
  }
  RecordExtraUserInteractionMetrics(interaction);
}

int MetricsHelper::NumVisits() {
  return num_visits_;
}

void MetricsHelper::OnGotHistoryCount(bool success,
                                      int num_visits,
                                      base::Time /*first_visit*/) {
  if (success)
    num_visits_ = num_visits;
}

}  // namespace security_interstitials
