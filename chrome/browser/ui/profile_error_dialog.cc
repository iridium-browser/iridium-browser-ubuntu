// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_error_dialog.h"

#include "base/auto_reset.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

void ShowProfileErrorDialog(ProfileErrorType type, int message_id) {
#if defined(OS_ANDROID) || defined(OS_IOS)
  NOTIMPLEMENTED();
#else
  UMA_HISTOGRAM_ENUMERATION("Profile.ProfileError", type, PROFILE_ERROR_END);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoErrorDialogs))
    return;

  static bool is_showing_profile_error_dialog = false;
  if (!is_showing_profile_error_dialog) {
    base::AutoReset<bool> resetter(&is_showing_profile_error_dialog, true);
    chrome::ShowMessageBox(NULL,
                           l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                           l10n_util::GetStringUTF16(message_id),
                           chrome::MESSAGE_BOX_TYPE_WARNING);
  }
#endif
}
