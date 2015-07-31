// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_debug_log.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace banners {

const char kRendererRequestCancel[] =
    "renderer has requested the banner prompt be cancelled";
const char kManifestEmpty[] = "manifest is empty";
const char kCannotDetermineBestIcon[] =
    "could not determine the best icon to use";
const char kNoMatchingServiceWorker[] =
    "no matching service worker detected. You may need to reload the page, or "
    "check that the service worker for the current page also controls the "
    "start URL from the manifest";
const char kNoIconAvailable[] = "no icon available to display";
const char kBannerAlreadyAdded[] =
    "the banner has already been added to the homescreen";
const char kUserNavigatedBeforeBannerShown[] =
    "the user navigated before the banner could be shown";
const char kStartURLNotValid[] = "start URL in manifest is not valid";
const char kManifestMissingNameOrShortName[] =
    "one of manifest name or short name must be specified";
const char kManifestMissingSuitableIcon[] =
    "manifest does not contain a suitable icon - PNG format of at least "
    "144x144px is required";
const char kNotServedFromSecureOrigin[] =
    "page not served from a secure origin";
// The leading space is intentional as another string is prepended.
const char kIgnoredNotSupportedOnAndroid[] =
    " application ignored: not supported on Android";
const char kIgnoredNoId[] = "play application ignored: no id provided";

void OutputDeveloperNotShownMessage(content::WebContents* web_contents,
                                    const std::string& message) {
  OutputDeveloperDebugMessage(web_contents, "not shown: " + message);
}

void OutputDeveloperDebugMessage(content::WebContents* web_contents,
                                 const std::string& message) {
  std::string log_message = "App banner " + message;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBypassAppBannerEngagementChecks) && web_contents) {
    web_contents->GetMainFrame()->Send(
        new ChromeViewMsg_AppBannerDebugMessageRequest(
            web_contents->GetMainFrame()->GetRoutingID(), log_message));
  }
}

}  // namespace banners
