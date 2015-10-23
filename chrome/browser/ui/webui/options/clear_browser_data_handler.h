// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_

#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/browsing_data/browsing_data_counter.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

namespace options {

// Clear browser data handler page UI handler.
class ClearBrowserDataHandler : public OptionsPageUIHandler,
                                public BrowsingDataRemover::Observer {
 public:
  ClearBrowserDataHandler();
  ~ClearBrowserDataHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;
  void InitializePage() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void UpdateInfoBannerVisibility();

 private:
  // Javascript callback to start clearing data.
  void HandleClearBrowserData(const base::ListValue* value);

  // BrowsingDataRemover::Observer implementation.
  // Closes the dialog once all requested data has been removed.
  void OnBrowsingDataRemoverDone() override;

  // Updates UI when the pref to allow clearing history changes.
  virtual void OnBrowsingHistoryPrefChanged();

  // Adds a |counter| for browsing data. Its output will be displayed
  // in the dialog with the string |text_grd_id|.
  void AddCounter(scoped_ptr<BrowsingDataCounter> counter, int text_grd_id);

  // Updates the counter of the pref |pref_name| in the UI according
  // to a callback from a |BrowsingDataCounter| that specifies whether
  // the counting has |finished| and what the |count| is. The |count| will
  // be substituted into the string with the ID |text_grd_id|.
  void UpdateCounterText(const std::string& pref_name,
                         int text_grd_id,
                         bool finished,
                         uint32 count);

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* remover_;

  // Keeps track of whether clearing LSO data is supported.
  BooleanPrefMember clear_plugin_lso_data_enabled_;

  // Keeps track of whether Pepper Flash is enabled and thus Flapper-specific
  // settings and removal options (e.g. Content Licenses) are available.
  BooleanPrefMember pepper_flash_settings_enabled_;

  // Keeps track of whether deleting browsing history and downloads is allowed.
  BooleanPrefMember allow_deleting_browser_history_;

  // Counters that calculate the data volume for some of the data types.
  ScopedVector<BrowsingDataCounter> counters_;

  DISALLOW_COPY_AND_ASSIGN(ClearBrowserDataHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CLEAR_BROWSER_DATA_HANDLER_H_
