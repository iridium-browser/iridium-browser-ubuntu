// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/version.h"
#include "chrome/installer/util/util_constants.h"

#if defined(OS_WIN)
#include <windows.h>  // NOLINT
#endif

class AppRegistrationData;

class BrowserDistribution {
 public:
  enum Type {
    CHROME_BROWSER,
    CHROME_FRAME,
    CHROME_BINARIES,
    NUM_TYPES
  };

  enum ShortcutType {
    SHORTCUT_CHROME,
    SHORTCUT_CHROME_ALTERNATE,
    SHORTCUT_APP_LAUNCHER
  };

  enum Subfolder {
    SUBFOLDER_CHROME,
    SUBFOLDER_APPS,
  };

  enum DefaultBrowserControlPolicy {
    DEFAULT_BROWSER_UNSUPPORTED,
    DEFAULT_BROWSER_OS_CONTROL_ONLY,
    DEFAULT_BROWSER_FULL_CONTROL
  };

  virtual ~BrowserDistribution();

  static BrowserDistribution* GetDistribution();

  static BrowserDistribution* GetSpecificDistribution(Type type);

  Type GetType() const { return type_; }

  // Getter and adaptors for the underlying |app_reg_data_|.
  const AppRegistrationData& GetAppRegistrationData() const;
  base::string16 GetAppGuid() const;
  base::string16 GetStateKey() const;
  base::string16 GetStateMediumKey() const;
  base::string16 GetVersionKey() const;

  virtual void DoPostUninstallOperations(
      const Version& version,
      const base::FilePath& local_data_path,
      const base::string16& distribution_data);

  // Returns the GUID to be used when registering for Active Setup.
  virtual base::string16 GetActiveSetupGuid();

  // Returns the unsuffixed application name of this program.
  // This is the base of the name registered with Default Programs on Windows.
  // IMPORTANT: This should only be called by the installer which needs to make
  // decisions on the suffixing of the upcoming install, not by external callers
  // at run-time.
  virtual base::string16 GetBaseAppName();

  // Returns the localized display name of this distribution.
  virtual base::string16 GetDisplayName();

  // Returns the localized name of the shortcut identified by |shortcut_type|
  // for this distribution.
  virtual base::string16 GetShortcutName(ShortcutType shortcut_type);

  // Returns the index of the icon for the product identified by
  // |shortcut_type|, inside the file specified by GetIconFilename().
  virtual int GetIconIndex(ShortcutType shortcut_type);

  // Returns the executable filename (not path) that contains the product icon.
  virtual base::string16 GetIconFilename();

  // Returns the localized name of the subfolder in the Start Menu identified by
  // |subfolder_type| that this distribution should create shortcuts in. For
  // SUBFOLDER_CHROME this returns GetShortcutName(SHORTCUT_CHROME).
  virtual base::string16 GetStartMenuShortcutSubfolder(
      Subfolder subfolder_type);

  // Returns the unsuffixed appid of this program.
  // The AppUserModelId is a property of Windows programs.
  // IMPORTANT: This should only be called by ShellUtil::GetAppId as the appid
  // should be suffixed in all scenarios.
  virtual base::string16 GetBaseAppId();

  // Returns the Browser ProgId prefix (e.g. ChromeHTML, ChromiumHTM, etc...).
  // The full id is of the form |prefix|.|suffix| and is limited to a maximum
  // length of 39 characters including null-terminator.  See
  // http://msdn.microsoft.com/library/aa911706.aspx for details.  We define
  // |suffix| as a fixed-length 26-character alphanumeric identifier, therefore
  // the return value of this function must have a maximum length of
  // 39 - 1(null-term) - 26(|suffix|) - 1(dot separator) = 11 characters.
  virtual base::string16 GetBrowserProgIdPrefix();

  // Returns the Browser ProgId description.
  virtual base::string16 GetBrowserProgIdDesc();

  virtual base::string16 GetInstallSubDir();

  virtual base::string16 GetPublisherName();

  virtual base::string16 GetAppDescription();

  virtual base::string16 GetLongAppDescription();

  virtual std::string GetSafeBrowsingName();

  virtual std::string GetNetworkStatsServer() const;

#if defined(OS_WIN)
  virtual base::string16 GetDistributionData(HKEY root_key);
#endif

  virtual base::string16 GetUninstallLinkName();

  virtual base::string16 GetUninstallRegPath();

  // Returns an enum specifying the different ways in which this distribution
  // is allowed to be set as default.
  virtual DefaultBrowserControlPolicy GetDefaultBrowserControlPolicy();

  virtual bool CanCreateDesktopShortcuts();

  virtual bool GetChromeChannel(base::string16* channel);

  // Returns true if this distribution includes a DelegateExecute verb handler,
  // and provides the CommandExecuteImpl class UUID if |handler_class_uuid| is
  // non-NULL.
  virtual bool GetCommandExecuteImplClsid(base::string16* handler_class_uuid);

  virtual void UpdateInstallStatus(bool system_install,
      installer::ArchiveType archive_type,
      installer::InstallStatus install_status);

  // Returns true if this distribution should set the Omaha experiment_labels
  // registry value.
  virtual bool ShouldSetExperimentLabels();

  virtual bool HasUserExperiments();

 protected:
  BrowserDistribution(Type type, scoped_ptr<AppRegistrationData> app_reg_data);

  template<class DistributionClass>
  static BrowserDistribution* GetOrCreateBrowserDistribution(
      BrowserDistribution** dist);

  const Type type_;

  scoped_ptr<AppRegistrationData> app_reg_data_;

 private:
  BrowserDistribution();

  DISALLOW_COPY_AND_ASSIGN(BrowserDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
