// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/renderer/instant_restricted_id_cache.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {
class RenderView;
}

class SearchBox : public content::RenderViewObserver,
                  public content::RenderViewObserverTracker<SearchBox> {
 public:
  enum ImageSourceType {
    NONE = -1,
    FAVICON,
    LARGE_ICON,
    FALLBACK_ICON,
    THUMB
  };

  // Helper class for GenerateImageURLFromTransientURL() to adapt SearchBox's
  // instance, thereby allow mocking for unit tests.
  class IconURLHelper {
   public:
    IconURLHelper();
    virtual ~IconURLHelper();
    // Retruns view id for validating icon URL.
    virtual int GetViewID() const = 0;
    // Returns the page URL string for |rid|, or empty string for invalid |rid|.
    virtual std::string GetURLStringFromRestrictedID(InstantRestrictedID rid)
        const = 0;
  };

  explicit SearchBox(content::RenderView* render_view);
  ~SearchBox() override;

  // Sends ChromeViewHostMsg_LogEvent to the browser.
  void LogEvent(NTPLoggingEventType event);

  // Sends ChromeViewHostMsg_LogMostVisitedImpression to the browser.
  void LogMostVisitedImpression(int position, NTPLoggingTileSource tile_source);

  // Sends ChromeViewHostMsg_LogMostVisitedNavigation to the browser.
  void LogMostVisitedNavigation(int position, NTPLoggingTileSource tile_source);

  // Sends ChromeViewHostMsg_ChromeIdentityCheck to the browser.
  void CheckIsUserSignedInToChromeAs(const base::string16& identity);

  // Sends ChromeViewHostMsg_HistorySyncCheck to the browser.
  void CheckIsUserSyncingHistory();

  // Sends ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem to the browser.
  void DeleteMostVisitedItem(InstantRestrictedID most_visited_item_id);

  // Generates the image URL of |type| for the most visited item specified in
  // |transient_url|. If |transient_url| is valid, |url| with a translated URL
  // and returns true.  Otherwise it depends on |type|:
  // - FAVICON: Returns true and renders an URL to display the default favicon.
  // - LARGE_ICON and FALLBACK_ICON: Returns false.
  //
  // For |type| == FAVICON, valid forms of |transient_url|:
  //    chrome-search://favicon/<view_id>/<restricted_id>
  //    chrome-search://favicon/<favicon_parameters>/<view_id>/<restricted_id>
  //
  // For |type| == LARGE_ICON, valid form of |transient_url|:
  //    chrome-search://large-icon/<size>/<view_id>/<restricted_id>
  //
  // For |type| == FALLBACK_ICON, valid form of |transient_url|:
  //    chrome-search://fallback-icon/<icon specs>/<view_id>/<restricted_id>
  //
  // For |type| == THUMB, valid form of |transient_url|:
  //    chrome-search://thumb/<render_view_id>/<most_visited_item_id>
  //
  // We do this to prevent search providers from abusing image URLs and deduce
  // whether the user has visited a particular page. For example, if
  // "chrome-search://favicon/http://www.secretsite.com" is accessible, then
  // the search provider can use its return code to determine whether the user
  // has visited "http://www.secretsite.com". Therefore we require search
  // providers to specify URL by "<view_id>/<restricted_id>". We then translate
  // this to the original |url|, and pass the request to the proper endpoint.
  bool GenerateImageURLFromTransientURL(const GURL& transient_url,
                                        ImageSourceType type,
                                        GURL* url) const;

  // Returns the latest most visited items sent by the browser.
  void GetMostVisitedItems(
      std::vector<InstantMostVisitedItemIDPair>* items) const;

  // If the |most_visited_item_id| is found in the cache, sets |item| to it
  // and returns true.
  bool GetMostVisitedItemWithID(InstantRestrictedID most_visited_item_id,
                                InstantMostVisitedItem* item) const;

  // Sends ChromeViewHostMsg_FocusOmnibox to the browser.
  void Focus();

  // Sends ChromeViewHostMsg_SearchBoxPaste to the browser.
  void Paste(const base::string16& text);

  const ThemeBackgroundInfo& GetThemeBackgroundInfo();
  const EmbeddedSearchRequestParams& GetEmbeddedSearchRequestParams();

  // Sends ChromeViewHostMsg_StartCapturingKeyStrokes to the browser.
  void StartCapturingKeyStrokes();

  // Sends ChromeViewHostMsg_StopCapturingKeyStrokes to the browser.
  void StopCapturingKeyStrokes();

  // Sends ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions to the
  // browser.
  void UndoAllMostVisitedDeletions();

  // Sends ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion to the browser.
  void UndoMostVisitedDeletion(InstantRestrictedID most_visited_item_id);

  bool is_focused() const { return is_focused_; }
  bool is_input_in_progress() const { return is_input_in_progress_; }
  bool is_key_capture_enabled() const { return is_key_capture_enabled_; }
  const base::string16& query() const { return query_; }
  const InstantSuggestion& suggestion() const { return suggestion_; }

 private:
  // Overridden from content::RenderViewObserver:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  void OnSetPageSequenceNumber(int page_seq_no);
  void OnChromeIdentityCheckResult(const base::string16& identity,
                                   bool identity_match);
  void OnDetermineIfPageSupportsInstant();
  void OnFocusChanged(OmniboxFocusState new_focus_state,
                      OmniboxFocusChangeReason reason);
  void OnHistorySyncCheckResult(bool sync_history);
  void OnMostVisitedChanged(
      const std::vector<InstantMostVisitedItem>& items);
  void OnSetInputInProgress(bool input_in_progress);
  void OnSetSuggestionToPrefetch(const InstantSuggestion& suggestion);
  void OnSubmit(const base::string16& query,
                const EmbeddedSearchRequestParams& params);
  void OnThemeChanged(const ThemeBackgroundInfo& theme_info);

  // Returns the current zoom factor of the render view or 1 on failure.
  double GetZoom() const;

  // Sets the searchbox values to their initial value.
  void Reset();

  // Returns the URL of the Most Visited item specified by the |item_id|.
  GURL GetURLForMostVisitedItem(InstantRestrictedID item_id) const;

  int page_seq_no_;
  bool is_focused_;
  bool is_input_in_progress_;
  bool is_key_capture_enabled_;
  InstantRestrictedIDCache<InstantMostVisitedItem> most_visited_items_cache_;
  ThemeBackgroundInfo theme_info_;
  base::string16 query_;
  EmbeddedSearchRequestParams embedded_search_request_params_;
  InstantSuggestion suggestion_;

  DISALLOW_COPY_AND_ASSIGN(SearchBox);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCHBOX_H_
