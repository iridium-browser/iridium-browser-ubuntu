// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file extends generic BrowserDistribution class to declare Google Chrome
// specific implementation.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_

#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "chrome/installer/util/browser_distribution.h"

namespace base {
class FilePath;
}

class AppRegistrationData;

class GoogleChromeDistribution : public BrowserDistribution {
 public:
  // Opens the Google Chrome uninstall survey window.
  // version refers to the version of Chrome being uninstalled.
  // local_data_path is the path of the file containing json metrics that
  //   will be parsed. If this file indicates that the user has opted in to
  //   providing anonymous usage data, then some additional statistics will
  //   be added to the survey url.
  // distribution_data contains Google Update related data that will be
  //   concatenated to the survey url if the file in local_data_path indicates
  //   the user has opted in to providing anonymous usage data.
  void DoPostUninstallOperations(
      const Version& version,
      const base::FilePath& local_data_path,
      const base::string16& distribution_data) override;

  base::string16 GetActiveSetupGuid() override;

  base::string16 GetShortcutName(ShortcutType shortcut_type) override;

  base::string16 GetIconFilename() override;

  int GetIconIndex(ShortcutType shortcut_type) override;

  base::string16 GetBaseAppName() override;

  base::string16 GetBaseAppId() override;

  base::string16 GetBrowserProgIdPrefix() override;

  base::string16 GetBrowserProgIdDesc() override;

  base::string16 GetInstallSubDir() override;

  base::string16 GetPublisherName() override;

  base::string16 GetAppDescription() override;

  std::string GetSafeBrowsingName() override;

  std::string GetNetworkStatsServer() const override;

  // This method reads data from the Google Update ClientState key for
  // potential use in the uninstall survey. It must be called before the
  // key returned by GetVersionKey() is deleted.
  base::string16 GetDistributionData(HKEY root_key) override;

  base::string16 GetUninstallLinkName() override;

  base::string16 GetUninstallRegPath() override;

  bool GetCommandExecuteImplClsid(base::string16* handler_class_uuid) override;

  void UpdateInstallStatus(
      bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status) override;

  bool ShouldSetExperimentLabels() override;

  bool HasUserExperiments() override;

 protected:
  // Disallow construction from others.
  GoogleChromeDistribution();

  explicit GoogleChromeDistribution(
      scoped_ptr<AppRegistrationData> app_reg_data);

 private:
  friend class BrowserDistribution;
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
