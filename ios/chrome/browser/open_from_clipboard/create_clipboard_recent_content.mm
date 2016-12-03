// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/open_from_clipboard/create_clipboard_recent_content.h"

#include "base/memory/ptr_util.h"
#import "components/open_from_clipboard/clipboard_recent_content_ios.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/common/app_group/app_group_constants.h"

std::unique_ptr<ClipboardRecentContent> CreateClipboardRecentContentIOS() {
  return base::MakeUnique<ClipboardRecentContentIOS>(
      kChromeUIScheme, app_group::GetGroupUserDefaults());
}
