// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_BINARIES_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_BINARIES_DISTRIBUTION_H_

#include "base/macros.h"
#include "chrome/installer/util/chromium_binaries_distribution.h"

class GoogleChromeBinariesDistribution : public ChromiumBinariesDistribution {
 public:
  base::string16 GetDisplayName() override;

  base::string16 GetShortcutName() override;

  void UpdateInstallStatus(bool system_install,
                           installer::ArchiveType archive_type,
                           installer::InstallStatus install_status) override;

 protected:
  friend class BrowserDistribution;

  GoogleChromeBinariesDistribution();

 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleChromeBinariesDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_BINARIES_DISTRIBUTION_H_
