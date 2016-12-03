// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"

#include "base/mac/sdk_forward_declarations.h"
#include "base/metrics/histogram.h"

namespace {

// The possible values of the setting "Screens Have Separate Spaces".
enum ScreenSpacesConfiguration {
  SCREENS_CANNOT_HAVE_SEPARATE_SPACES = 0,
  SCREENS_HAVE_SEPARATE_SPACES = 1,
  SCREENS_HAVE_SHARED_SPACES = 2,
  SCREEN_SPACES_CONFIGURATION_COUNT = 3
};

}  // namespace

void ChromeBrowserMainExtraPartsMetrics::RecordMacMetrics() {
  ScreenSpacesConfiguration separate_spaces =
      SCREENS_CANNOT_HAVE_SEPARATE_SPACES;

  if ([NSScreen respondsToSelector:@selector(screensHaveSeparateSpaces)]) {
    BOOL screens_have_separate_spaces = [NSScreen screensHaveSeparateSpaces];
    separate_spaces = screens_have_separate_spaces
                          ? SCREENS_HAVE_SEPARATE_SPACES
                          : SCREENS_HAVE_SHARED_SPACES;
  }
  UMA_HISTOGRAM_ENUMERATION("OSX.Settings.ScreensHaveSeparateSpaces",
                            separate_spaces,
                            SCREEN_SPACES_CONFIGURATION_COUNT);
}
