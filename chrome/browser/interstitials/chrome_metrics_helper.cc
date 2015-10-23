// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/chrome_metrics_helper.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/rappor/rappor_service.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#endif

ChromeMetricsHelper::ChromeMetricsHelper(
    content::WebContents* web_contents,
    const GURL& request_url,
    const security_interstitials::MetricsHelper::ReportDetails settings,
    const std::string& sampling_event_name)
    : security_interstitials::MetricsHelper(
          request_url,
          settings,
          HistoryServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              ServiceAccessType::EXPLICIT_ACCESS),
          g_browser_process->rappor_service()),
      web_contents_(web_contents),
      request_url_(request_url),
      sampling_event_name_(sampling_event_name) {
  DCHECK(!sampling_event_name_.empty());
}

ChromeMetricsHelper::~ChromeMetricsHelper() {}

void ChromeMetricsHelper::RecordExtraUserDecisionMetrics(
    security_interstitials::MetricsHelper::Decision decision) {
#if defined(ENABLE_EXTENSIONS)
  if (!sampling_event_.get()) {
    sampling_event_.reset(new extensions::ExperienceSamplingEvent(
        sampling_event_name_, request_url_,
        web_contents_->GetLastCommittedURL(),
        web_contents_->GetBrowserContext()));
  }
  switch (decision) {
    case PROCEED:
      sampling_event_->CreateUserDecisionEvent(
          extensions::ExperienceSamplingEvent::kProceed);
      break;
    case DONT_PROCEED:
      sampling_event_->CreateUserDecisionEvent(
          extensions::ExperienceSamplingEvent::kDeny);
      break;
    case SHOW:
    case PROCEEDING_DISABLED:
    case MAX_DECISION:
      break;
  }
#endif
}

void ChromeMetricsHelper::RecordExtraUserInteractionMetrics(
    security_interstitials::MetricsHelper::Interaction interaction) {
#if defined(ENABLE_EXTENSIONS)
  if (!sampling_event_.get()) {
    sampling_event_.reset(new extensions::ExperienceSamplingEvent(
        sampling_event_name_, request_url_,
        web_contents_->GetLastCommittedURL(),
        web_contents_->GetBrowserContext()));
  }
  switch (interaction) {
    case SHOW_LEARN_MORE:
      sampling_event_->set_has_viewed_learn_more(true);
      break;
    case SHOW_ADVANCED:
      sampling_event_->set_has_viewed_details(true);
      break;
    case SHOW_PRIVACY_POLICY:
    case SHOW_DIAGNOSTIC:
    case RELOAD:
    case OPEN_TIME_SETTINGS:
    case TOTAL_VISITS:
    case SET_EXTENDED_REPORTING_ENABLED:
    case SET_EXTENDED_REPORTING_DISABLED:
    case EXTENDED_REPORTING_IS_ENABLED:
    case REPORT_PHISHING_ERROR:
    case MAX_INTERACTION:
      break;
  }
#endif
}
