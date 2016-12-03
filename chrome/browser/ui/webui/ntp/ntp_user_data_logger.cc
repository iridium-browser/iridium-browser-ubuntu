// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include <algorithm>
#include <string>

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/search/search_urls.h"
#include "chrome/common/url_constants.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/sync_sessions_metrics.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

namespace {

// Name of the histogram keeping track of suggestion impressions.
const char kMostVisitedImpressionHistogramName[] =
    "NewTabPage.SuggestionsImpression";

// Format string to generate the name for the histogram keeping track of
// suggestion impressions.
const char kMostVisitedImpressionHistogramWithProvider[] =
    "NewTabPage.SuggestionsImpression.%s";

// Name of the histogram keeping track of suggestion navigations.
const char kMostVisitedNavigationHistogramName[] =
    "NewTabPage.MostVisited";

// Format string to generate the name for the histogram keeping track of
// suggestion navigations.
const char kMostVisitedNavigationHistogramWithProvider[] =
    "NewTabPage.MostVisited.%s";

std::string GetSourceName(NTPLoggingTileSource tile_source) {
  switch (tile_source) {
    case NTPLoggingTileSource::CLIENT:
      return "client";
    case NTPLoggingTileSource::SERVER:
      return "server";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NTPUserDataLogger);


// Log a time event for a given |histogram| at a given |value|. This
// routine exists because regular histogram macros are cached thus can't be used
// if the name of the histogram will change at a given call site.
void logLoadTimeHistogram(const std::string& histogram, base::TimeDelta value) {
  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      histogram,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(60), 100,
      base::Histogram::kUmaTargetedHistogramFlag);
  if (counter)
    counter->AddTime(value);
}


NTPUserDataLogger::~NTPUserDataLogger() {}

// static
NTPUserDataLogger* NTPUserDataLogger::GetOrCreateFromWebContents(
      content::WebContents* content) {
  DCHECK(search::IsInstantNTP(content));

  // Calling CreateForWebContents when an instance is already attached has no
  // effect, so we can do this.
  NTPUserDataLogger::CreateForWebContents(content);
  NTPUserDataLogger* logger = NTPUserDataLogger::FromWebContents(content);

  // We record the URL of this NTP in order to identify navigations that
  // originate from it. We use the NavigationController's URL since it might
  // differ from the WebContents URL which is usually chrome://newtab/.
  //
  // We update the NTP URL every time this function is called, because the NTP
  // URL sometimes changes while it is open, and we care about the final one for
  // detecting when the user leaves or returns to the NTP. In particular, if the
  // Google URL changes (e.g. google.com -> google.de), then we fall back to the
  // local NTP.
  const content::NavigationEntry* entry =
      content->GetController().GetVisibleEntry();
  if (entry && (logger->ntp_url_ != entry->GetURL())) {
    DVLOG(1) << "NTP URL changed from \"" << logger->ntp_url_ << "\" to \""
             << entry->GetURL() << "\"";
    logger->ntp_url_ = entry->GetURL();
  }

  return logger;
}

void NTPUserDataLogger::LogEvent(NTPLoggingEventType event,
                                 base::TimeDelta time) {
  switch (event) {
    case NTP_SERVER_SIDE_SUGGESTION:
      has_server_side_suggestions_ = true;
      break;
    case NTP_CLIENT_SIDE_SUGGESTION:
      has_client_side_suggestions_ = true;
      break;
    case NTP_TILE:
      // TODO(sfiera): remove NTP_TILE and use NTP_*_SIDE_SUGGESTION.
      number_of_tiles_++;
      break;
    case NTP_TILE_LOADED:
      // We no longer emit statistics for the multi-iframe NTP.
      break;
    case NTP_ALL_TILES_LOADED:
      EmitNtpStatistics(time);
      break;
    default:
      NOTREACHED();
  }
}

void NTPUserDataLogger::LogMostVisitedImpression(
    int position, NTPLoggingTileSource tile_source) {
  if ((position >= kNumMostVisited) || impression_was_logged_[position]) {
    return;
  }
  impression_was_logged_[position] = true;

  UMA_HISTOGRAM_ENUMERATION(kMostVisitedImpressionHistogramName, position,
                            kNumMostVisited);

  // Cannot rely on UMA histograms macro because the name of the histogram is
  // generated dynamically.
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      base::StringPrintf(kMostVisitedImpressionHistogramWithProvider,
                         GetSourceName(tile_source).c_str()),
      1,
      kNumMostVisited,
      kNumMostVisited + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(position);
}

void NTPUserDataLogger::LogMostVisitedNavigation(
    int position, NTPLoggingTileSource tile_source) {
  UMA_HISTOGRAM_ENUMERATION(kMostVisitedNavigationHistogramName, position,
                            kNumMostVisited);

  // Cannot rely on UMA histograms macro because the name of the histogram is
  // generated dynamically.
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      base::StringPrintf(kMostVisitedNavigationHistogramWithProvider,
                         GetSourceName(tile_source).c_str()),
      1,
      kNumMostVisited,
      kNumMostVisited + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(position);

  // Records the action. This will be available as a time-stamped stream
  // server-side and can be used to compute time-to-long-dwell.
  content::RecordAction(base::UserMetricsAction("MostVisited_Clicked"));
}

NTPUserDataLogger::NTPUserDataLogger(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      has_server_side_suggestions_(false),
      has_client_side_suggestions_(false),
      number_of_tiles_(0),
      has_emitted_(false),
      during_startup_(false) {
  during_startup_ = !AfterStartupTaskUtils::IsBrowserStartupComplete();

  // We record metrics about session data here because when this class typically
  // emits metrics it is too late. This session data would theoretically have
  // been used to populate the page, and we want to learn about its state when
  // the NTP is being generated.
  if (contents) {
    ProfileSyncService* sync = ProfileSyncServiceFactory::GetForProfile(
        Profile::FromBrowserContext(contents->GetBrowserContext()));
    if (sync) {
      browser_sync::SessionsSyncManager* sessions =
          static_cast<browser_sync::SessionsSyncManager*>(
              sync->GetSessionsSyncableService());
      if (sessions) {
        sync_sessions::SyncSessionsMetrics::RecordYoungestForeignTabAgeOnNTP(
            sessions);
      }
    }
  }
}

// content::WebContentsObserver override
void NTPUserDataLogger::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  NavigatedFromURLToURL(load_details.previous_url,
                        load_details.entry->GetURL());
}

void NTPUserDataLogger::NavigatedFromURLToURL(const GURL& from,
                                              const GURL& to) {
  // User is returning to NTP, probably via the back button; reset stats.
  if (from.is_valid() && to.is_valid() && (to == ntp_url_)) {
    DVLOG(1) << "Returning to New Tab Page";
    impression_was_logged_.reset();
    has_emitted_ = false;
    number_of_tiles_ = 0;
    has_server_side_suggestions_ = false;
    has_client_side_suggestions_ = false;
  }
}

void NTPUserDataLogger::EmitNtpStatistics(base::TimeDelta load_time) {
  // We only send statistics once per page.
  if (has_emitted_)
    return;
  DVLOG(1) << "Emitting NTP load time: " << load_time << ", "
           << "number of tiles: " << number_of_tiles_;

  logLoadTimeHistogram("NewTabPage.LoadTime", load_time);

  // Split between ML and MV.
  std::string type = has_server_side_suggestions_ ?
      "MostLikely" : "MostVisited";
  logLoadTimeHistogram("NewTabPage.LoadTime." + type, load_time);
  // Split between Web and Local.
  std::string source = ntp_url_.SchemeIsHTTPOrHTTPS() ? "Web" : "LocalNTP";
  logLoadTimeHistogram("NewTabPage.LoadTime." + source, load_time);

  // Split between Startup and non-startup.
  std::string status = during_startup_ ? "Startup" : "NewTab";
  logLoadTimeHistogram("NewTabPage.LoadTime." + status, load_time);

  has_server_side_suggestions_ = false;
  has_client_side_suggestions_ = false;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "NewTabPage.NumberOfTiles", number_of_tiles_, 1, kNumMostVisited,
      kNumMostVisited + 1);
  number_of_tiles_ = 0;
  has_emitted_ = true;
  during_startup_ = false;
}
