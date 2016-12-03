// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_

#include <stddef.h>

#include <bitset>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Helper class for logging data from the NTP. Attached to each NTP instance.
class NTPUserDataLogger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NTPUserDataLogger> {
 public:
  ~NTPUserDataLogger() override;

  // Gets the associated NTPUserDataLogger, creating it if necessary.
  //
  // MUST be called only when the NTP is active.
  static NTPUserDataLogger* GetOrCreateFromWebContents(
      content::WebContents* content);

  // Called when an event occurs on the NTP that requires a counter to be
  // incremented. |time| is the delta time in ms from navigation start until
  // this event happened.
  void LogEvent(NTPLoggingEventType event, base::TimeDelta time);

  // Logs an impression on one of the NTP tiles by a given source.
  void LogMostVisitedImpression(int position, NTPLoggingTileSource tile_source);

  // Logs a navigation on one of the NTP tiles by a given source.
  void LogMostVisitedNavigation(int position, NTPLoggingTileSource tile_source);

 protected:
  explicit NTPUserDataLogger(content::WebContents* contents);

 private:
  friend class content::WebContentsUserData<NTPUserDataLogger>;

  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           OnMostVisitedItemsChangedFromServer);
  FRIEND_TEST_ALL_PREFIXES(SearchTabHelperTest,
                           OnMostVisitedItemsChangedFromClient);
  FRIEND_TEST_ALL_PREFIXES(NTPUserDataLoggerTest,
                           TestLogging);
  FRIEND_TEST_ALL_PREFIXES(NTPUserDataLoggerTest, TestLogMostVisitedImpression);
  FRIEND_TEST_ALL_PREFIXES(NTPUserDataLoggerTest, TestNumberOfTiles);

  // Number of Most Visited elements on the NTP for logging purposes.
  static const int kNumMostVisited = 8;

  // content::WebContentsObserver override
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  // Implementation of NavigationEntryCommitted; separate for test.
  void NavigatedFromURLToURL(const GURL& from, const GURL& to);

  // Logs a number of statistics regarding the NTP. Called when an NTP tab is
  // about to be deactivated (be it by switching tabs, losing focus or closing
  // the tab/shutting down Chrome), or when the user navigates to a URL.
  void EmitNtpStatistics(base::TimeDelta load_time);

  // Records whether we have yet logged an impression for the tile at a given
  // index. A typical NTP will log 8 impressions, but could record fewer for new
  // users that haven't built up a history yet.
  //
  // If something happens that causes the NTP to pull tiles from different
  // sources, such as signing in (switching from client to server tiles), then
  // only the impressions for the first source will be logged, leaving the
  // number of impressions for a source slightly out-of-sync with navigations.
  std::bitset<kNumMostVisited> impression_was_logged_;

  // True if at least one iframe came from a server-side suggestion.
  bool has_server_side_suggestions_;

  // True if at least one iframe came from a client-side suggestion.
  bool has_client_side_suggestions_;

  // Total number of tiles rendered, no matter if it's a thumbnail, a gray tile
  // or an external tile.
  size_t number_of_tiles_;

  // Whether we have already emitted NTP stats for this web contents.
  bool has_emitted_;

  // Are stats being logged during Chrome startup?
  bool during_startup_;

  // The URL of this New Tab Page - varies based on NTP version.
  GURL ntp_url_;

  DISALLOW_COPY_AND_ASSIGN(NTPUserDataLogger);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NTP_USER_DATA_LOGGER_H_
