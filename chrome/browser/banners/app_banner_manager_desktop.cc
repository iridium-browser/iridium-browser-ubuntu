// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include "base/command_line.h"
#include "chrome/browser/banners/app_banner_data_fetcher_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/constants.h"

namespace {
// TODO(dominickn) Enforce the set of icons which will guarantee the best
// user experience.
int kMinimumIconSize = extension_misc::EXTENSION_ICON_LARGE;
}  // anonymous namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(banners::AppBannerManagerDesktop);

namespace banners {

bool AppBannerManagerDesktop::IsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAddToShelf);
}

AppBannerDataFetcher* AppBannerManagerDesktop::CreateAppBannerDataFetcher(
    base::WeakPtr<AppBannerDataFetcher::Delegate> weak_delegate,
    const int ideal_icon_size) {
  return new AppBannerDataFetcherDesktop(web_contents(), weak_delegate,
                                         ideal_icon_size);
}

AppBannerManagerDesktop::AppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents, kMinimumIconSize) {
}

}  // namespace banners
