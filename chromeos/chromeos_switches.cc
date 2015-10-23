// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/chromeos_switches.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"

// TODO(rsorokin): alphabetize all of these switches so they
// match the order from the .h file

namespace chromeos {
namespace switches {

// Allows remote attestation (RA) in dev mode for testing purpose. Usually RA
// is disabled in dev mode because it will always fail. However, there are cases
// in testing where we do want to go through the permission flow even in dev
// mode. This can be enabled by this flag.
const char kAllowRAInDevMode[] = "allow-ra-in-dev-mode";

// Path for app's OEM manifest file.
const char kAppOemManifestFile[] = "app-mode-oem-manifest";

// When wallpaper boot animation is not disabled this switch
// is used to override OOBE/sign in WebUI init type.
// Possible values: parallel|postpone. Default: parallel.
const char kAshWebUIInit[] = "ash-webui-init";

// Default wallpaper to use for kids accounts
// (as paths to trusted, non-user-writable JPEG files).
const char kChildWallpaperLarge[] = "child-wallpaper-large";
const char kChildWallpaperSmall[] = "child-wallpaper-small";

// Specifies the URL of the consumer device management backend.
const char kConsumerDeviceManagementUrl[] = "consumer-device-management-url";

// Forces the stub implementation of dbus clients.
const char kDbusStub[] = "dbus-stub";

// Comma-spearated list of dbus clients that should be unstubbed.
// See chromeos/dbus/dbus_client_bundle.cc for the names of the dbus clients.
const char kDbusUnstubClients[] = "dbus-unstub-clients";

// Indicates that the wallpaper images specified by
// kAshDefaultWallpaper{Large,Small} are OEM-specific (i.e. they are not
// downloadable from Google).
const char kDefaultWallpaperIsOem[] = "default-wallpaper-is-oem";

// Default wallpaper to use (as paths to trusted, non-user-writable JPEG files).
const char kDefaultWallpaperLarge[] = "default-wallpaper-large";
const char kDefaultWallpaperSmall[] = "default-wallpaper-small";

// Time before a machine at OOBE is considered derelict.
const char kDerelictDetectionTimeout[] = "derelict-detection-timeout";

// Time before a derelict machines starts demo mode.
const char kDerelictIdleTimeout[] = "derelict-idle-timeout";

// Disables wallpaper boot animation (except of OOBE case).
const char kDisableBootAnimation[] = "disable-boot-animation";

// Disables cloud backup feature.
const char kDisableCloudImport[] = "disable-cloud-import";

// Disables the ChromeOS demo.
const char kDisableDemoMode[] = "disable-demo-mode";

// Disable HID-detection OOBE screen.
const char kDisableHIDDetectionOnOOBE[] = "disable-hid-detection-on-oobe";

// Avoid doing expensive animations upon login.
const char kDisableLoginAnimations[] = "disable-login-animations";

// Disable new channel switcher UI.
const char kDisableNewChannelSwitcherUI[] = "disable-new-channel-switcher-ui";

// Disables new Kiosk UI when kiosk apps are represented as user pods.
const char kDisableNewKioskUI[] = "disable-new-kiosk-ui";

// Disables the new File System Provider API based ZIP unpacker.
const char kDisableNewZIPUnpacker[] = "disable-new-zip-unpacker";

// Disable Office Editing for Docs, Sheets & Slides component app so handlers
// won't be registered, making it possible to install another version for
// testing.
const char kDisableOfficeEditingComponentApp[] =
    "disable-office-editing-component-extension";

// Disables rollback option on reset screen.
const char kDisableRollbackOption[] = "disable-rollback-option";

// Disables volume adjust sound.
const char kDisableVolumeAdjustSound[] = "disable-volume-adjust-sound";

// Disables wake on wifi features.
const char kDisableWakeOnWifi[] = "disable-wake-on-wifi";

// Disables notifications about captive portals in session.
const char kDisableNetworkPortalNotification[] =
    "disable-network-portal-notification";

// EAFE url and path to use for Easy bootstrapping.
const char kEafeUrl[] = "eafe-url";
const char kEafePath[] = "eafe-path";

// Enables consumer management, which allows user to enroll, remotely lock and
// locate the device.
const char kEnableConsumerManagement[] = "enable-consumer-management";

// If this switch is set, the device cannot be remotely disabled by its owner.
const char kDisableDeviceDisabling[] = "disable-device-disabling";

// If this switch is set, the new Korean IME will not be available in
// chrome://settings/languages.
const char kDisableNewKoreanIme[] = "disable-new-korean-ime";

// Disables mtp write support.
const char kDisableMtpWriteSupport[] = "disable-mtp-write-support";

// If this switch is set, the options for suggestions as typing on physical
// keyboard will be enabled.
const char kEnablePhysicalKeyboardAutocorrect[] =
    "enable-physical-keyboard-autocorrect";

// If this switch is set, the options for suggestions as typing on physical
// keyboard will be disabled.
const char kDisablePhysicalKeyboardAutocorrect[] =
    "disable-physical-keyboard-autocorrect";

// Enabled sharing assets for installed default apps.
const char kEnableExtensionAssetsSharing[]  = "enable-extension-assets-sharing";

// Enables notifications about captive portals in session.
const char kEnableNetworkPortalNotification[] =
    "enable-network-portal-notification";

// Enables touchpad three-finger-click as middle button.
const char kEnableTouchpadThreeFingerClick[]
    = "enable-touchpad-three-finger-click";

// Enables using screenshots in tests and seets mode.
const char kEnableScreenshotTestingWithMode[] =
    "enable-screenshot-testing-with-mode";

// Enable Kiosk mode for ChromeOS. Note this switch refers to retail mode rather
// than the kiosk app mode.
const char kEnableKioskMode[] = "enable-kiosk-mode";

// Enables request of tablet site (via user agent override).
const char kEnableRequestTabletSite[] = "enable-request-tablet-site";

// Whether to enable forced enterprise re-enrollment.
const char kEnterpriseEnableForcedReEnrollment[] =
    "enterprise-enable-forced-re-enrollment";

// Power of the power-of-2 initial modulus that will be used by the
// auto-enrollment client. E.g. "4" means the modulus will be 2^4 = 16.
const char kEnterpriseEnrollmentInitialModulus[] =
    "enterprise-enrollment-initial-modulus";

// Power of the power-of-2 maximum modulus that will be used by the
// auto-enrollment client.
const char kEnterpriseEnrollmentModulusLimit[] =
    "enterprise-enrollment-modulus-limit";

// Enables the chromecast support for video player app.
const char kEnableVideoPlayerChromecastSupport[] =
    "enable-video-player-chromecast-support";

// Passed to Chrome the first time that it's run after the system boots.
// Not passed on restart after sign out.
const char kFirstExecAfterBoot[] = "first-exec-after-boot";

// Usually in browser tests the usual login manager bringup is skipped so that
// tests can change how it's brought up. This flag disables that.
const char kForceLoginManagerInTests[] = "force-login-manager-in-tests";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[] = "bwsi";

// Wallpaper to use in guest mode (as paths to trusted, non-user-writable JPEG
// files).
const char kGuestWallpaperLarge[] = "guest-wallpaper-large";
const char kGuestWallpaperSmall[] = "guest-wallpaper-small";

// If true, the Chromebook has a keyboard with a diamond key.
const char kHasChromeOSDiamondKey[] = "has-chromeos-diamond-key";

// Defines user homedir. This defaults to primary user homedir.
const char kHomedir[]           = "homedir";

// With this switch, start remora OOBE with the pairing screen.
const char kHostPairingOobe[] = "host-pairing-oobe";

// If true, profile selection in UserManager will always return active user's
// profile.
// TODO(nkostlyev): http://crbug.com/364604 - Get rid of this switch after we
// turn on multi-profile feature on ChromeOS.
const char kIgnoreUserProfileMappingForTests[] =
    "ignore-user-profile-mapping-for-tests";

// File to load internal display ICC file from.
const char kInternalDisplayColorProfileFile[] =
    "internal-display-color-profile-file";

// Enables Chrome-as-a-login-manager behavior.
const char kLoginManager[] = "login-manager";

// Specifies the profile to use once a chromeos user is logged in.
// This parameter is ignored if user goes through login screen since user_id
// hash defines which profile directory to use.
// In case of browser restart within active session this parameter is used
// to pass user_id hash for primary user.
const char kLoginProfile[] = "login-profile";

// Specifies the user which is already logged in.
const char kLoginUser[] = "login-user";

// The memory pressure thresholds selection which is used to decide whether and
// when a memory pressure event needs to get fired.
const char kMemoryPressureExperimentName[] = "ChromeOSMemoryPressureHandling";
const char kMemoryPressureHandlingOff[] = "memory-pressure-off";
const char kMemoryPressureThresholds[] = "memory-pressure-thresholds";
const char kConservativeThreshold[] = "conservative";
const char kAggressiveCacheDiscardThreshold[] = "aggressive-cache-discard";
const char kAggressiveTabDiscardThreshold[] = "aggressive-tab-discard";
const char kAggressiveThreshold[] = "aggressive";

// Enables natural scroll by default.
const char kNaturalScrollDefault[] = "enable-natural-scroll-default";

// Skips all other OOBE pages after user login.
const char kOobeSkipPostLogin[] = "oobe-skip-postlogin";

// Disable GAIA services such as enrollment and OAuth session restore. Used by
// 'fake' telemetry login.
const char kDisableGaiaServices[] = "disable-gaia-services";

// Interval at which we check for total time on OOBE.
const char kOobeTimerInterval[] = "oobe-timer-interval";

// Indicates that a guest session has been started before OOBE completion.
const char kOobeGuestSession[] = "oobe-guest-session";

// Specifies power stub behavior:
//  'cycle=2' - Cycles power states every 2 seconds.
// See FakeDBusThreadManager::ParsePowerCommandLineSwitch for full details.
const char kPowerStub[] = "power-stub";

// Overrides network stub behavior. By default, ethernet, wifi and vpn are
// enabled, and transitions occur instantaneously. Multiple options can be
// comma separated (no spaces). Note: all options are in the format 'foo=x'.
// Values are case sensitive and based on Shill names in service_constants.h.
// See FakeShillManagerClient::SetInitialNetworkState for implementation.
// Examples:
//  'clear=1' - Clears all default configurations
//  'wifi=on' - A wifi network is initially connected ('1' also works)
//  'wifi=off' - Wifi networks are all initially disconnected ('0' also works)
//  'wifi=disabled' - Wifi is initially disabled
//  'wifi=none' - Wifi is unavailable
//  'wifi=portal' - Wifi connection will be in Portal state
//  'cellular=1' - Cellular is initially connected
//  'cellular=LTE' - Cellular is initially connected, technology is LTE
//  'interactive=3' - Interactive mode, connect/scan/etc requests take 3 secs
const char kShillStub[] = "shill-stub";

// Sends test messages on first call to RequestUpdate (stub only).
const char kSmsTestMessages[] = "sms-test-messages";

// Indicates that a stub implementation of CrosSettings that stores settings in
// memory without signing should be used, treating current user as the owner.
// This also modifies OwnerSettingsServiceChromeOS::HandlesSetting such that no
// settings are handled by OwnerSettingsServiceChromeOS.
// This option is for testing the chromeos build of chrome on the desktop only.
const char kStubCrosSettings[] = "stub-cros-settings";

// Indicates that the system is running in dev mode. The dev mode probing is
// done by session manager.
const char kSystemDevMode[] = "system-developer-mode";

// Enables animated transitions during first-run tutorial.
const char kEnableFirstRunUITransitions[] = "enable-first-run-ui-transitions";

// Forces first-run UI to be shown for every login.
const char kForceFirstRunUI[] = "force-first-run-ui";

// Enables testing for auto update UI.
const char kTestAutoUpdateUI[] = "test-auto-update-ui";

// Enables waking the device based on the receipt of some network packets.
const char kWakeOnPackets[] = "wake-on-packets";

// Screenshot testing: specifies the directory where the golden screenshots are
// stored.
const char kGoldenScreenshotsDir[] = "golden-screenshots-dir";

// Screenshot testing: specifies the directoru where artifacts will be stored.
const char kArtifactsDir[] = "artifacts-dir";

// Disable bypass proxy for captive portal authorization.
const char kDisableCaptivePortalBypassProxy[] =
    "disable-captive-portal-bypass-proxy";

// Disable automatic timezone update.
const char kDisableTimeZoneTrackingOption[] =
    "disable-timezone-tracking-option";

// Switches and optional value for Data Saver prompt on cellular networks.
const char kDisableDataSaverPrompt[] = "disable-datasaver-prompt";
const char kEnableDataSaverPrompt[] = "enable-datasaver-prompt";
const char kDataSaverPromptDemoMode[] = "demo";

bool WakeOnWifiEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(kDisableWakeOnWifi);
}

bool MemoryPressureHandlingEnabled() {
  if (base::FieldTrialList::FindFullName(kMemoryPressureExperimentName) ==
      kMemoryPressureHandlingOff) {
    return false;
  }
  return true;
}

base::chromeos::MemoryPressureMonitor::MemoryPressureThresholds
GetMemoryPressureThresholds() {
  using MemoryPressureMonitor = base::chromeos::MemoryPressureMonitor;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kMemoryPressureThresholds)) {
    const std::string group_name =
        base::FieldTrialList::FindFullName(kMemoryPressureExperimentName);
    if (group_name == kConservativeThreshold)
      return MemoryPressureMonitor::THRESHOLD_CONSERVATIVE;
    if (group_name == kAggressiveCacheDiscardThreshold)
      return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE_CACHE_DISCARD;
    if (group_name == kAggressiveTabDiscardThreshold)
      return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE_TAB_DISCARD;
    if (group_name == kAggressiveThreshold)
      return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE;
    return MemoryPressureMonitor::THRESHOLD_DEFAULT;
  }

  const std::string option =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kMemoryPressureThresholds);
  if (option == kConservativeThreshold)
    return MemoryPressureMonitor::THRESHOLD_CONSERVATIVE;
  if (option == kAggressiveCacheDiscardThreshold)
    return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE_CACHE_DISCARD;
  if (option == kAggressiveTabDiscardThreshold)
    return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE_TAB_DISCARD;
  if (option == kAggressiveThreshold)
    return MemoryPressureMonitor::THRESHOLD_AGGRESSIVE;

  return MemoryPressureMonitor::THRESHOLD_DEFAULT;
}

}  // namespace switches
}  // namespace chromeos
