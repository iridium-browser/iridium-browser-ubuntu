// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a specific implementation of BrowserDistribution class for
// Chrome Frame. It overrides the bare minimum of methods necessary to get a
// Chrome Frame installer that does not interact with Google Chrome or
// Chromium installations.

#include "chrome/installer/util/chrome_frame_distribution.h"

#include "base/strings/string_util.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/updating_app_registration_data.h"

namespace {
const wchar_t kChromeFrameGuid[] = L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}";
}

ChromeFrameDistribution::ChromeFrameDistribution()
    : BrowserDistribution(
          CHROME_FRAME,
          std::unique_ptr<AppRegistrationData>(
              new UpdatingAppRegistrationData(kChromeFrameGuid))) {}

base::string16 ChromeFrameDistribution::GetBaseAppName() {
  return L"Google Chrome Frame";
}

base::string16 ChromeFrameDistribution::GetBrowserProgIdPrefix() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromeFrameDistribution::GetBrowserProgIdDesc() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromeFrameDistribution::GetDisplayName() {
#if defined(GOOGLE_CHROME_BUILD)
  return L"Google Chrome Frame";
#else
  return L"Chromium Frame";
#endif
}

base::string16 ChromeFrameDistribution::GetShortcutName() {
  NOTREACHED();
  return base::string16();
}

base::string16 ChromeFrameDistribution::GetInstallSubDir() {
  return L"Google\\Chrome Frame";
}

base::string16 ChromeFrameDistribution::GetPublisherName() {
  const base::string16& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

base::string16 ChromeFrameDistribution::GetAppDescription() {
  return L"Chrome in a Frame.";
}

base::string16 ChromeFrameDistribution::GetLongAppDescription() {
  return L"Chrome in a Frame.";
}

std::string ChromeFrameDistribution::GetSafeBrowsingName() {
  return "googlechromeframe";
}

base::string16 ChromeFrameDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome Frame";
}

base::string16 ChromeFrameDistribution::GetIconFilename() {
  return installer::kChromeExe;
}

int ChromeFrameDistribution::GetIconIndex() {
  return 0;
}

BrowserDistribution::DefaultBrowserControlPolicy
    ChromeFrameDistribution::GetDefaultBrowserControlPolicy() {
  return DEFAULT_BROWSER_UNSUPPORTED;
}

bool ChromeFrameDistribution::CanCreateDesktopShortcuts() {
  return false;
}

base::string16 ChromeFrameDistribution::GetCommandExecuteImplClsid() {
  return base::string16();
}

void ChromeFrameDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
#if defined(GOOGLE_CHROME_BUILD)
  GoogleUpdateSettings::UpdateInstallStatus(system_install,
      archive_type, InstallUtil::GetInstallReturnCode(install_status),
      kChromeFrameGuid);
#endif
}
