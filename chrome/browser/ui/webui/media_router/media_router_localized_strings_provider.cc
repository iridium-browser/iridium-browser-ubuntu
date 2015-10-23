// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_localized_strings_provider.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

// Note that media_router.html contains a <script> tag which imports a script
// of the following name. These names must be kept in sync.
const char kLocalizedStringsFile[] = "strings.js";

void AddMediaRouterStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("mediaRouterTitle", IDS_MEDIA_ROUTER_TITLE);
}

void AddRouteDetailsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("castingActivityStatus",
      IDS_MEDIA_ROUTER_CASTING_ACTIVITY_STATUS);
  html_source->AddLocalizedString("selectCastModeHeader",
      IDS_MEDIA_ROUTER_SELECT_CAST_MODE_HEADER);
  html_source->AddLocalizedString("shareYourScreenSubheading",
      IDS_MEDIA_ROUTER_SHARE_YOUR_SCREEN_SUBHEADING);
  html_source->AddLocalizedString("stopCastingButton",
      IDS_MEDIA_ROUTER_STOP_CASTING_BUTTON);
}

void AddIssuesActionsStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("cancelButton",
                                  IDS_MEDIA_ROUTER_CANCEL_BUTTON);
  html_source->AddLocalizedString("dismissButton",
                                  IDS_MEDIA_ROUTER_DISMISS_BUTTON);
  html_source->AddLocalizedString("learnMoreButton",
                                  IDS_MEDIA_ROUTER_LEARN_MORE_BUTTON);
  html_source->AddLocalizedString("okButton", IDS_MEDIA_ROUTER_OK_BUTTON);
}

void AddSinkListStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString("deviceMissing",
                                  IDS_MEDIA_ROUTER_DEVICE_MISSING);
}

}  // namespace

namespace media_router {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  AddMediaRouterStrings(html_source);
  AddRouteDetailsStrings(html_source);
  AddIssuesActionsStrings(html_source);
  AddSinkListStrings(html_source);
  html_source->SetJsonPath(kLocalizedStringsFile);
}

}  // namespace media_router
