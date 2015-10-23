// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/browser_process_platform_part_base.h"

namespace base {
class CommandLine;
}

namespace chromeos {
class ChromeUserManager;
class ProfileHelper;
class TimeZoneResolver;
}

namespace chromeos {
namespace system {
class AutomaticRebootManager;
class DeviceDisablingManager;
class DeviceDisablingManagerDefaultDelegate;
class SystemClock;
}
}

namespace policy {
class BrowserPolicyConnector;
class BrowserPolicyConnectorChromeOS;
}

namespace session_manager {
class SessionManager;
}

class Profile;

class BrowserProcessPlatformPart : public BrowserProcessPlatformPartBase,
                                   public base::NonThreadSafe {
 public:
  BrowserProcessPlatformPart();
  ~BrowserProcessPlatformPart() override;

  void InitializeAutomaticRebootManager();
  void ShutdownAutomaticRebootManager();

  void InitializeChromeUserManager();
  void DestroyChromeUserManager();

  void InitializeDeviceDisablingManager();
  void ShutdownDeviceDisablingManager();

  void InitializeSessionManager(const base::CommandLine& parsed_command_line,
                                Profile* profile,
                                bool is_running_test);
  void ShutdownSessionManager();

  // Disable the offline interstitial easter egg if the device is enterprise
  // enrolled.
  void DisableDinoEasterEggIfEnrolled();

  // Returns the SessionManager instance that is used to initialize and
  // start user sessions as well as responsible on launching pre-session UI like
  // out-of-box or login.
  virtual session_manager::SessionManager* SessionManager();

  // Returns the ProfileHelper instance that is used to identify
  // users and their profiles in Chrome OS multi user session.
  chromeos::ProfileHelper* profile_helper();

  chromeos::system::AutomaticRebootManager* automatic_reboot_manager() {
    return automatic_reboot_manager_.get();
  }

  policy::BrowserPolicyConnectorChromeOS* browser_policy_connector_chromeos();

  chromeos::ChromeUserManager* user_manager() {
    return chrome_user_manager_.get();
  }

  chromeos::system::DeviceDisablingManager* device_disabling_manager() {
    return device_disabling_manager_.get();
  }

  chromeos::TimeZoneResolver* GetTimezoneResolver();

  // Overridden from BrowserProcessPlatformPartBase:
  void StartTearDown() override;

  scoped_ptr<policy::BrowserPolicyConnector> CreateBrowserPolicyConnector()
      override;

  chromeos::system::SystemClock* GetSystemClock();

 private:
  void CreateProfileHelper();

  scoped_ptr<session_manager::SessionManager> session_manager_;

  bool created_profile_helper_;
  scoped_ptr<chromeos::ProfileHelper> profile_helper_;

  scoped_ptr<chromeos::system::AutomaticRebootManager>
      automatic_reboot_manager_;

  scoped_ptr<chromeos::ChromeUserManager> chrome_user_manager_;

  scoped_ptr<chromeos::system::DeviceDisablingManagerDefaultDelegate>
      device_disabling_manager_delegate_;
  scoped_ptr<chromeos::system::DeviceDisablingManager>
      device_disabling_manager_;

  scoped_ptr<chromeos::TimeZoneResolver> timezone_resolver_;

  scoped_ptr<chromeos::system::SystemClock> system_clock_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPart);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
