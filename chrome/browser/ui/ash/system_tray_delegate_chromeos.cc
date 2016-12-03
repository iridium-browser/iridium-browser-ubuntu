// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/common/ash_switches.h"
#include "ash/common/login_status.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/session/session_state_observer.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/chromeos/bluetooth/bluetooth_observer.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/session/logout_button_observer.h"
#include "ash/common/system/chromeos/shutdown_policy_observer.h"
#include "ash/common/system/date/clock_observer.h"
#include "ash/common/system/ime/ime_observer.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray_accessibility.h"
#include "ash/common/system/update/update_observer.h"
#include "ash/common/system/user/user_observer.h"
#include "ash/common/system/volume_control_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ash/system/chromeos/rotation/tray_rotation_lock.h"
#include "ash/system/chromeos/tray_display.h"
#include "ash/wm/lock_state_controller.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/chromeos/events/system_key_event_listener.h"
#include "chrome/browser/chromeos/input_method/input_method_switch_recorder.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/multiprofiles_intro_dialog.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/set_time_dialog.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/ash/cast_config_delegate_chromeos.h"
#include "chrome/browser/ui/ash/cast_config_delegate_media_router.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/networking_config_delegate_chromeos.h"
#include "chrome/browser/ui/ash/system_tray_delegate_utils.h"
#include "chrome/browser/ui/ash/volume_controller_chromeos.h"
#include "chrome/browser/ui/ash/vpn_delegate_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/google/core/browser/google_util.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "net/base/escape.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/chromeos/ime/input_method_menu_item.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

namespace chromeos {

namespace {

// The minimum session length limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000;  // 30 seconds.

// The maximum session length limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000;  // 24 hours.

const char kDisplaySettingsSubPageName[] = "display";
const char kDisplayOverscanSettingsSubPageName[] = "displayOverscan";
const char kPaletteSettingsSubPageName[] = "stylus-overlay";

void ExtractIMEInfo(const input_method::InputMethodDescriptor& ime,
                    const input_method::InputMethodUtil& util,
                    ash::IMEInfo* info) {
  info->id = ime.id();
  info->name = util.GetInputMethodLongName(ime);
  info->medium_name = util.GetInputMethodMediumName(ime);
  info->short_name = util.GetInputMethodShortName(ime);
  info->third_party = extension_ime_util::IsExtensionIME(ime.id());
}

gfx::NativeWindow GetNativeWindowByStatus(ash::LoginStatus login_status,
                                          bool session_started) {
  ash::WmShell* wm_shell = ash::WmShell::Get();
  const bool is_in_secondary_login_screen =
      wm_shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen();

  int container_id =
      (!session_started || login_status == ash::LoginStatus::NOT_LOGGED_IN ||
       login_status == ash::LoginStatus::LOCKED || is_in_secondary_login_screen)
          ? ash::kShellWindowId_LockSystemModalContainer
          : ash::kShellWindowId_SystemModalContainer;
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container_id);
}

void BluetoothSetDiscoveringError() {
  LOG(ERROR) << "BluetoothSetDiscovering failed.";
}

void BluetoothDeviceConnectError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
}

std::unique_ptr<ash::CastConfigDelegate> CreateCastConfigDelegate() {
  if (CastConfigDelegateMediaRouter::IsEnabled())
    return base::WrapUnique(new CastConfigDelegateMediaRouter());
  return base::WrapUnique(new CastConfigDelegateChromeos());
}

void ShowSettingsSubPageForActiveUser(const std::string& sub_page) {
  chrome::ShowSettingsSubPageForProfile(
      ProfileManager::GetActiveUserProfile(), sub_page);
}

void OnAcceptMultiprofilesIntro(bool no_show_again) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kMultiProfileNeverShowIntro, no_show_again);
  UserAddingScreen::Get()->Start();
}

}  // namespace

SystemTrayDelegateChromeOS::SystemTrayDelegateChromeOS()
    : user_profile_(NULL),
      clock_type_(base::GetHourClockType()),
      search_key_mapped_to_(input_method::kSearchKey),
      screen_locked_(false),
      have_session_start_time_(false),
      have_session_length_limit_(false),
      should_run_bluetooth_discovery_(false),
      session_started_(false),
      cast_config_delegate_(nullptr),
      networking_config_delegate_(new NetworkingConfigDelegateChromeos()),
      volume_control_delegate_(new VolumeController()),
      vpn_delegate_(new VPNDelegateChromeOS),
      weak_ptr_factory_(this) {
  // Register notifications on construction so that events such as
  // PROFILE_CREATED do not get missed if they happen before Initialize().
  registrar_.reset(new content::NotificationRegistrar);
  registrar_->Add(this,
                  chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                  content::NotificationService::AllSources());
  registrar_->Add(this,
                  chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                  content::NotificationService::AllSources());
  if (GetUserLoginStatus() == ash::LoginStatus::NOT_LOGGED_IN) {
    registrar_->Add(this,
                    chrome::NOTIFICATION_SESSION_STARTED,
                    content::NotificationService::AllSources());
  }
  registrar_->Add(this,
                  chrome::NOTIFICATION_PROFILE_CREATED,
                  content::NotificationService::AllSources());
  registrar_->Add(this,
                  chrome::NOTIFICATION_PROFILE_DESTROYED,
                  content::NotificationService::AllSources());

  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityStatusChanged,
                 base::Unretained(this)));

  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  shutdown_policy_handler_.reset(
      new ShutdownPolicyHandler(CrosSettings::Get(), this));
}

void SystemTrayDelegateChromeOS::Initialize() {
  DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);

  input_method::InputMethodManager::Get()->AddObserver(this);
  input_method::InputMethodManager::Get()->AddImeMenuObserver(this);
  ui::ime::InputMethodMenuManager::GetInstance()->AddObserver(this);

  g_browser_process->platform_part()->GetSystemClock()->AddObserver(this);

  OnSystemClockChanged(g_browser_process->platform_part()->GetSystemClock());

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&SystemTrayDelegateChromeOS::InitializeOnAdapterReady,
                 weak_ptr_factory_.GetWeakPtr()));

  ash::WmShell::Get()->GetSessionStateDelegate()->AddSessionStateObserver(this);

  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->AddAudioObserver(this);

  BrowserList::AddObserver(this);
}

void SystemTrayDelegateChromeOS::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  CHECK(bluetooth_adapter_);
  bluetooth_adapter_->AddObserver(this);

  local_state_registrar_.reset(new PrefChangeRegistrar);
  local_state_registrar_->Init(g_browser_process->local_state());

  UpdateSessionStartTime();
  UpdateSessionLengthLimit();

  local_state_registrar_->Add(
      prefs::kSessionStartTime,
      base::Bind(&SystemTrayDelegateChromeOS::UpdateSessionStartTime,
                 base::Unretained(this)));
  local_state_registrar_->Add(
      prefs::kSessionLengthLimit,
      base::Bind(&SystemTrayDelegateChromeOS::UpdateSessionLengthLimit,
                 base::Unretained(this)));

  policy::BrowserPolicyConnectorChromeOS* policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      policy_connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->AddObserver(this);
  UpdateEnterpriseDomain();
}

SystemTrayDelegateChromeOS::~SystemTrayDelegateChromeOS() {
  // Unregister PrefChangeRegistrars.
  local_state_registrar_.reset();
  user_pref_registrar_.reset();

  // Unregister content notifications before destroying any components.
  registrar_.reset();

  // Unregister a11y status subscription.
  accessibility_subscription_.reset();

  g_browser_process->platform_part()->GetSystemClock()->RemoveObserver(this);
  DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
  input_method::InputMethodManager::Get()->RemoveObserver(this);
  ui::ime::InputMethodMenuManager::GetInstance()->RemoveObserver(this);
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
  ash::WmShell::Get()->GetSessionStateDelegate()->RemoveSessionStateObserver(
      this);

  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);

  BrowserList::RemoveObserver(this);
  StopObservingAppWindowRegistry();
  StopObservingCustodianInfoChanges();

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->RemoveObserver(this);

  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

// Overridden from ash::SystemTrayDelegate:
bool SystemTrayDelegateChromeOS::GetTrayVisibilityOnStartup() {
  // In case of OOBE / sign in screen tray will be shown later.
  return LoginState::Get()->IsUserLoggedIn();
}

ash::LoginStatus SystemTrayDelegateChromeOS::GetUserLoginStatus() const {
  if (!LoginState::Get()->IsUserLoggedIn())
    return ash::LoginStatus::NOT_LOGGED_IN;

  if (screen_locked_)
    return ash::LoginStatus::LOCKED;

  LoginState::LoggedInUserType user_type =
      LoginState::Get()->GetLoggedInUserType();
  switch (user_type) {
    case LoginState::LOGGED_IN_USER_NONE:
      return ash::LoginStatus::NOT_LOGGED_IN;
    case LoginState::LOGGED_IN_USER_REGULAR:
      return ash::LoginStatus::USER;
    case LoginState::LOGGED_IN_USER_OWNER:
      return ash::LoginStatus::OWNER;
    case LoginState::LOGGED_IN_USER_GUEST:
      return ash::LoginStatus::GUEST;
    case LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT:
      return ash::LoginStatus::PUBLIC;
    case LoginState::LOGGED_IN_USER_SUPERVISED:
      return ash::LoginStatus::SUPERVISED;
    case LoginState::LOGGED_IN_USER_KIOSK_APP:
      return ash::LoginStatus::KIOSK_APP;
  }
  NOTREACHED();
  return ash::LoginStatus::NOT_LOGGED_IN;
}

void SystemTrayDelegateChromeOS::ChangeProfilePicture() {
  content::RecordAction(
      base::UserMetricsAction("OpenChangeProfilePictureDialog"));
  ShowSettingsSubPageForActiveUser(chrome::kChangeProfilePictureSubPage);
}

std::string SystemTrayDelegateChromeOS::GetEnterpriseDomain() const {
  return enterprise_domain_;
}

base::string16 SystemTrayDelegateChromeOS::GetEnterpriseMessage() const {
  if (GetEnterpriseDomain().empty())
    return base::string16();
  return l10n_util::GetStringFUTF16(IDS_DEVICE_OWNED_BY_NOTICE,
                                    base::UTF8ToUTF16(GetEnterpriseDomain()));
}

std::string SystemTrayDelegateChromeOS::GetSupervisedUserManager() const {
  if (!IsUserSupervised())
    return std::string();
  return SupervisedUserServiceFactory::GetForProfile(user_profile_)->
      GetCustodianEmailAddress();
}

base::string16
SystemTrayDelegateChromeOS::GetSupervisedUserManagerName() const {
  if (!IsUserSupervised())
    return base::string16();
  return base::UTF8ToUTF16(SupervisedUserServiceFactory::GetForProfile(
      user_profile_)->GetCustodianName());
}

base::string16 SystemTrayDelegateChromeOS::GetSupervisedUserMessage()
    const {
  if (!IsUserSupervised())
    return base::string16();
  if (IsUserChild())
    return GetChildUserMessage();
  return GetLegacySupervisedUserMessage();
}

bool SystemTrayDelegateChromeOS::IsUserSupervised() const {
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  return user && user->IsSupervised();
}

bool SystemTrayDelegateChromeOS::IsUserChild() const {
  return user_manager::UserManager::Get()->IsLoggedInAsChildUser();
}

void SystemTrayDelegateChromeOS::GetSystemUpdateInfo(
    ash::UpdateInfo* info) const {
  GetUpdateInfo(UpgradeDetector::GetInstance(), info);
}

base::HourClockType SystemTrayDelegateChromeOS::GetHourClockType() const {
  return clock_type_;
}

void SystemTrayDelegateChromeOS::ShowSettings() {
  ShowSettingsSubPageForActiveUser("");
}

bool SystemTrayDelegateChromeOS::ShouldShowSettings() {
  ash::WmShell* wm_shell = ash::WmShell::Get();
  return ChromeUserManager::Get()->GetCurrentUserFlow()->ShouldShowSettings() &&
         !wm_shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen();
}

void SystemTrayDelegateChromeOS::ShowDateSettings() {
  content::RecordAction(base::UserMetricsAction("ShowDateOptions"));
  std::string sub_page =
      std::string(chrome::kSearchSubPage) + "#" +
      l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME);
  // Everybody can change the time zone (even though it is a device setting).
  ShowSettingsSubPageForActiveUser(sub_page);
}

void SystemTrayDelegateChromeOS::ShowSetTimeDialog() {
  SetTimeDialog::ShowDialog(GetNativeWindow());
}

void SystemTrayDelegateChromeOS::ShowNetworkSettingsForGuid(
    const std::string& guid) {
  ash::WmShell* wm_shell = ash::WmShell::Get();
  if (LoginState::Get()->IsUserLoggedIn() &&
      !wm_shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen()) {
    std::string page = chrome::kInternetOptionsSubPage;
    if (!guid.empty())
      page += "?guid=" + net::EscapeUrlEncodedData(guid, true);
    content::RecordAction(base::UserMetricsAction("OpenInternetOptionsDialog"));
    ShowSettingsSubPageForActiveUser(page);
  }
}

void SystemTrayDelegateChromeOS::ShowDisplaySettings() {
  content::RecordAction(base::UserMetricsAction("ShowDisplayOptions"));
  ShowSettingsSubPageForActiveUser(kDisplaySettingsSubPageName);
}

void SystemTrayDelegateChromeOS::ShowPowerSettings() {
  if (!(switches::PowerOverlayEnabled() ||
        (ash::PowerStatus::Get()->IsBatteryPresent() &&
         ash::PowerStatus::Get()->SupportsDualRoleDevices()))) {
    return;
  }
  content::RecordAction(base::UserMetricsAction("Tray_ShowPowerOptions"));
  ShowSettingsSubPageForActiveUser(chrome::kPowerOptionsSubPage);
}

void SystemTrayDelegateChromeOS::ShowChromeSlow() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetPrimaryUserProfile());
  chrome::ShowSlow(displayer.browser());
}

bool SystemTrayDelegateChromeOS::ShouldShowDisplayNotification() {
  // Packaged app is not counted as 'last active', so if a browser opening the
  // display settings is in background of a packaged app, it will return true.
  // TODO(mukai): fix this.
  Browser* active_browser = chrome::FindLastActive();
  if (!active_browser)
    return true;

  content::WebContents* active_contents =
      active_browser->tab_strip_model()->GetActiveWebContents();
  if (!active_contents)
    return true;

  GURL visible_url = active_contents->GetLastCommittedURL();
  return !(chrome::IsSettingsSubPage(visible_url,
                                     kDisplaySettingsSubPageName) ||
           chrome::IsSettingsSubPage(visible_url,
                                     kDisplayOverscanSettingsSubPageName));
}

void SystemTrayDelegateChromeOS::ShowIMESettings() {
  content::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  ShowSettingsSubPageForActiveUser(chrome::kLanguageOptionsSubPage);
}

void SystemTrayDelegateChromeOS::ShowHelp() {
  chrome::ShowHelpForProfile(ProfileManager::GetActiveUserProfile(),
                             chrome::HELP_SOURCE_MENU);
}

void SystemTrayDelegateChromeOS::ShowAccessibilityHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  accessibility::ShowAccessibilityHelp(displayer.browser());
}

void SystemTrayDelegateChromeOS::ShowAccessibilitySettings() {
  content::RecordAction(base::UserMetricsAction("ShowAccessibilitySettings"));
  std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
                         l10n_util::GetStringUTF8(
                             IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY);
  ShowSettingsSubPageForActiveUser(sub_page);
}

void SystemTrayDelegateChromeOS::ShowPaletteHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowSingletonTab(displayer.browser(),
                           GURL(chrome::kChromePaletteHelpURL));
}

void SystemTrayDelegateChromeOS::ShowPaletteSettings() {
  content::RecordAction(base::UserMetricsAction("ShowPaletteOptions"));
  ShowSettingsSubPageForActiveUser(kPaletteSettingsSubPageName);
}

void SystemTrayDelegateChromeOS::ShowPublicAccountInfo() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowPolicy(displayer.browser());
}

void SystemTrayDelegateChromeOS::ShowSupervisedUserInfo() {
  // TODO(antrim): find out what should we show in this case.
  // http://crbug.com/229762
}

void SystemTrayDelegateChromeOS::ShowEnterpriseInfo() {
  ash::LoginStatus status = GetUserLoginStatus();
  ash::WmShell* wm_shell = ash::WmShell::Get();
  if (status == ash::LoginStatus::NOT_LOGGED_IN ||
      status == ash::LoginStatus::LOCKED ||
      wm_shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen()) {
    scoped_refptr<chromeos::HelpAppLauncher> help_app(
        new chromeos::HelpAppLauncher(GetNativeWindow()));
    help_app->ShowHelpTopic(chromeos::HelpAppLauncher::HELP_ENTERPRISE);
  } else {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        ProfileManager::GetActiveUserProfile());
    chrome::ShowSingletonTab(displayer.browser(),
                             GURL(chrome::kLearnMoreEnterpriseURL));
  }
}

void SystemTrayDelegateChromeOS::ShowUserLogin() {
  ash::WmShell* wm_shell = ash::WmShell::Get();
  if (!wm_shell->delegate()->IsMultiProfilesEnabled())
    return;

  // Only regular non-supervised users could add other users to current session.
  if (user_manager::UserManager::Get()->GetActiveUser()->GetType() !=
      user_manager::USER_TYPE_REGULAR) {
    return;
  }

  if (static_cast<int>(
          user_manager::UserManager::Get()->GetLoggedInUsers().size()) >=
      wm_shell->GetSessionStateDelegate()->GetMaximumNumberOfLoggedInUsers()) {
    return;
  }

  // Launch sign in screen to add another user to current session.
  if (user_manager::UserManager::Get()
          ->GetUsersAllowedForMultiProfile()
          .size()) {
    // Don't show dialog if any logged in user in multi-profiles session
    // dismissed it.
    bool show_intro = true;
    const user_manager::UserList logged_in_users =
        user_manager::UserManager::Get()->GetLoggedInUsers();
    for (user_manager::UserList::const_iterator it = logged_in_users.begin();
         it != logged_in_users.end();
         ++it) {
      show_intro &=
          !multi_user_util::GetProfileFromAccountId((*it)->GetAccountId())
               ->GetPrefs()
               ->GetBoolean(prefs::kMultiProfileNeverShowIntro);
      if (!show_intro)
        break;
    }
    if (show_intro) {
      base::Callback<void(bool)> on_accept =
          base::Bind(&OnAcceptMultiprofilesIntro);
      ShowMultiprofilesIntroDialog(on_accept);
    } else {
      UserAddingScreen::Get()->Start();
    }
  }
}

void SystemTrayDelegateChromeOS::SignOut() {
  chrome::AttemptUserExit();
}

void SystemTrayDelegateChromeOS::RequestRestartForUpdate() {
  // We expect that UpdateEngine is in "Reboot for update" state now.
  chrome::NotifyAndTerminate(true /* fast path */);
}

void SystemTrayDelegateChromeOS::RequestShutdown() {
  ash::Shell::GetInstance()->lock_state_controller()->RequestShutdown();
}

void SystemTrayDelegateChromeOS::GetAvailableBluetoothDevices(
    ash::BluetoothDeviceList* list) {
  device::BluetoothAdapter::DeviceList devices =
      bluetooth_adapter_->GetDevices();
  for (size_t i = 0; i < devices.size(); ++i) {
    device::BluetoothDevice* device = devices[i];
    ash::BluetoothDeviceInfo info;
    info.address = device->GetAddress();
    info.display_name = device->GetNameForDisplay();
    info.connected = device->IsConnected();
    info.connecting = device->IsConnecting();
    info.paired = device->IsPaired();
    list->push_back(info);
  }
}

void SystemTrayDelegateChromeOS::BluetoothStartDiscovering() {
  if (GetBluetoothDiscovering()) {
    LOG(WARNING) << "Already have active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Requesting new Bluetooth device discovery session.";
  should_run_bluetooth_discovery_ = true;
  bluetooth_adapter_->StartDiscoverySession(
      base::Bind(&SystemTrayDelegateChromeOS::OnStartBluetoothDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothSetDiscoveringError));
}

void SystemTrayDelegateChromeOS::BluetoothStopDiscovering() {
  should_run_bluetooth_discovery_ = false;
  if (!GetBluetoothDiscovering()) {
    LOG(WARNING) << "No active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Stopping Bluetooth device discovery session.";
  bluetooth_discovery_session_->Stop(
      base::Bind(&base::DoNothing), base::Bind(&BluetoothSetDiscoveringError));
}

void SystemTrayDelegateChromeOS::ConnectToBluetoothDevice(
    const std::string& address) {
  device::BluetoothDevice* device = bluetooth_adapter_->GetDevice(address);
  if (!device || device->IsConnecting() ||
      (device->IsConnected() && device->IsPaired())) {
    return;
  }
  if (device->IsPaired() && !device->IsConnectable())
    return;
  if (device->IsPaired() || !device->IsPairable()) {
    ash::WmShell::Get()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_BLUETOOTH_CONNECT_KNOWN_DEVICE);
    device->Connect(NULL,
                    base::Bind(&base::DoNothing),
                    base::Bind(&BluetoothDeviceConnectError));
  } else {  // Show paring dialog for the unpaired device.
    ash::WmShell::Get()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_BLUETOOTH_CONNECT_UNKNOWN_DEVICE);
    BluetoothPairingDialog* dialog =
        new BluetoothPairingDialog(GetNativeWindow(), device);
    // The dialog deletes itself on close.
    dialog->Show();
  }
}

bool SystemTrayDelegateChromeOS::IsBluetoothDiscovering() {
  return bluetooth_adapter_ && bluetooth_adapter_->IsDiscovering();
}

void SystemTrayDelegateChromeOS::GetCurrentIME(ash::IMEInfo* info) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  input_method::InputMethodDescriptor ime =
      manager->GetActiveIMEState()->GetCurrentInputMethod();
  ExtractIMEInfo(ime, *util, info);
  info->selected = true;
}

void SystemTrayDelegateChromeOS::GetAvailableIMEList(ash::IMEInfoList* list) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  std::unique_ptr<input_method::InputMethodDescriptors> ime_descriptors(
      manager->GetActiveIMEState()->GetActiveInputMethods());
  std::string current =
      manager->GetActiveIMEState()->GetCurrentInputMethod().id();
  for (size_t i = 0; i < ime_descriptors->size(); i++) {
    input_method::InputMethodDescriptor& ime = ime_descriptors->at(i);
    ash::IMEInfo info;
    ExtractIMEInfo(ime, *util, &info);
    info.selected = ime.id() == current;
    list->push_back(info);
  }
}

void SystemTrayDelegateChromeOS::GetCurrentIMEProperties(
    ash::IMEPropertyInfoList* list) {
  ui::ime::InputMethodMenuItemList menu_list =
      ui::ime::InputMethodMenuManager::GetInstance()->
      GetCurrentInputMethodMenuItemList();
  for (size_t i = 0; i < menu_list.size(); ++i) {
    ash::IMEPropertyInfo property;
    property.key = menu_list[i].key;
    property.name = base::UTF8ToUTF16(menu_list[i].label);
    property.selected = menu_list[i].is_selection_item_checked;
    list->push_back(property);
  }
}

void SystemTrayDelegateChromeOS::SwitchIME(const std::string& ime_id) {
  input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->ChangeInputMethod(ime_id, false /* show_message */);
  input_method::InputMethodSwitchRecorder::Get()->RecordSwitch(
      true /* by_tray_menu */);
}

void SystemTrayDelegateChromeOS::ActivateIMEProperty(const std::string& key) {
  input_method::InputMethodManager::Get()->ActivateInputMethodMenuItem(key);
}

void SystemTrayDelegateChromeOS::ManageBluetoothDevices() {
  content::RecordAction(base::UserMetricsAction("ShowBluetoothSettingsPage"));
  std::string sub_page =
      std::string(chrome::kSearchSubPage) + "#" +
      l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_BLUETOOTH);
  ShowSettingsSubPageForActiveUser(sub_page);
}

void SystemTrayDelegateChromeOS::ToggleBluetooth() {
  bluetooth_adapter_->SetPowered(!bluetooth_adapter_->IsPowered(),
                                 base::Bind(&base::DoNothing),
                                 base::Bind(&base::DoNothing));
}

void SystemTrayDelegateChromeOS::ShowOtherNetworkDialog(
    const std::string& type) {
  if (type == shill::kTypeCellular) {
    ChooseMobileNetworkDialog::ShowDialog(GetNativeWindow());
    return;
  }
  NetworkConfigView::ShowForType(type, GetNativeWindow());
}

bool SystemTrayDelegateChromeOS::GetBluetoothAvailable() {
  return bluetooth_adapter_ && bluetooth_adapter_->IsPresent();
}

bool SystemTrayDelegateChromeOS::GetBluetoothEnabled() {
  return bluetooth_adapter_ && bluetooth_adapter_->IsPowered();
}

bool SystemTrayDelegateChromeOS::GetBluetoothDiscovering() {
  return bluetooth_discovery_session_ &&
         bluetooth_discovery_session_->IsActive();
}

void SystemTrayDelegateChromeOS::ChangeProxySettings() {
  CHECK(GetUserLoginStatus() == ash::LoginStatus::NOT_LOGGED_IN);
  LoginDisplayHost::default_host()->OpenProxySettings();
}

ash::CastConfigDelegate* SystemTrayDelegateChromeOS::GetCastConfigDelegate() {
  if (!cast_config_delegate_)
    cast_config_delegate_ = CreateCastConfigDelegate();
  return cast_config_delegate_.get();
}

ash::NetworkingConfigDelegate*
SystemTrayDelegateChromeOS::GetNetworkingConfigDelegate() const {
  return networking_config_delegate_.get();
}

ash::VolumeControlDelegate*
SystemTrayDelegateChromeOS::GetVolumeControlDelegate() const {
  return volume_control_delegate_.get();
}

void SystemTrayDelegateChromeOS::SetVolumeControlDelegate(
    std::unique_ptr<ash::VolumeControlDelegate> delegate) {
  volume_control_delegate_.swap(delegate);
}

bool SystemTrayDelegateChromeOS::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  *session_start_time = session_start_time_;
  return have_session_start_time_;
}

bool SystemTrayDelegateChromeOS::GetSessionLengthLimit(
    base::TimeDelta* session_length_limit) {
  *session_length_limit = session_length_limit_;
  return have_session_length_limit_;
}

int SystemTrayDelegateChromeOS::GetSystemTrayMenuWidth() {
  return l10n_util::GetLocalizedContentsWidthInPixels(
      IDS_SYSTEM_TRAY_MENU_BUBBLE_WIDTH_PIXELS);
}

void SystemTrayDelegateChromeOS::ActiveUserWasChanged() {
  SetProfile(ProfileManager::GetActiveUserProfile());
  GetSystemTrayNotifier()->NotifyUserUpdate();
}

bool SystemTrayDelegateChromeOS::IsSearchKeyMappedToCapsLock() {
  return search_key_mapped_to_ == input_method::kCapsLockKey;
}

void SystemTrayDelegateChromeOS::AddCustodianInfoTrayObserver(
    ash::CustodianInfoTrayObserver* observer) {
  custodian_info_changed_observers_.AddObserver(observer);
}

void SystemTrayDelegateChromeOS::RemoveCustodianInfoTrayObserver(
    ash::CustodianInfoTrayObserver* observer) {
  custodian_info_changed_observers_.RemoveObserver(observer);
}

void SystemTrayDelegateChromeOS::AddShutdownPolicyObserver(
    ash::ShutdownPolicyObserver* observer) {
  shutdown_policy_observers_.AddObserver(observer);
}

void SystemTrayDelegateChromeOS::RemoveShutdownPolicyObserver(
    ash::ShutdownPolicyObserver* observer) {
  shutdown_policy_observers_.RemoveObserver(observer);
}

void SystemTrayDelegateChromeOS::ShouldRebootOnShutdown(
    const ash::RebootOnShutdownCallback& callback) {
  shutdown_policy_handler_->CheckIfRebootOnShutdown(callback);
}

ash::VPNDelegate* SystemTrayDelegateChromeOS::GetVPNDelegate() const {
  return vpn_delegate_.get();
}

std::unique_ptr<ash::SystemTrayItem>
SystemTrayDelegateChromeOS::CreateDisplayTrayItem(ash::SystemTray* tray) {
  return base::MakeUnique<ash::TrayDisplay>(tray);
}

std::unique_ptr<ash::SystemTrayItem>
SystemTrayDelegateChromeOS::CreateRotationLockTrayItem(ash::SystemTray* tray) {
  return base::MakeUnique<ash::TrayRotationLock>(tray);
}

void SystemTrayDelegateChromeOS::UserAddedToSession(
    const user_manager::User* active_user) {
}

void SystemTrayDelegateChromeOS::ActiveUserChanged(
    const user_manager::User* /* active_user */) {
}

void SystemTrayDelegateChromeOS::UserChangedChildStatus(
    user_manager::User* user) {
  Profile* user_profile = ProfileHelper::Get()->GetProfileByUser(user);

  // Returned user_profile might be NULL on restoring Users on browser start.
  // At some point profile is not yet fully initiated.
  if (session_started_ &&
      user_profile != NULL &&
      user_profile_ == user_profile) {
    ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(
        GetUserLoginStatus());
  }
}

ash::SystemTray* SystemTrayDelegateChromeOS::GetPrimarySystemTray() {
  return ash::Shell::GetInstance()->GetPrimarySystemTray();
}

ash::SystemTrayNotifier* SystemTrayDelegateChromeOS::GetSystemTrayNotifier() {
  return ash::WmShell::Get()->system_tray_notifier();
}

void SystemTrayDelegateChromeOS::SetProfile(Profile* profile) {
  // Stop observing the AppWindowRegistry of the current |user_profile_|.
  StopObservingAppWindowRegistry();

  // Stop observing custodian info changes of the current |user_profile_|.
  StopObservingCustodianInfoChanges();

  user_profile_ = profile;

  // Start observing the AppWindowRegistry of the newly set |user_profile_|.
  extensions::AppWindowRegistry::Get(user_profile_)->AddObserver(this);

  // Start observing custodian info changes of the newly set |user_profile_|.
  SupervisedUserServiceFactory::GetForProfile(profile)->AddObserver(this);

  PrefService* prefs = profile->GetPrefs();
  user_pref_registrar_.reset(new PrefChangeRegistrar);
  user_pref_registrar_->Init(prefs);
  user_pref_registrar_->Add(
      prefs::kLanguageRemapSearchKeyTo,
      base::Bind(&SystemTrayDelegateChromeOS::OnLanguageRemapSearchKeyToChanged,
                 base::Unretained(this)));
  user_pref_registrar_->Add(
      prefs::kShowLogoutButtonInTray,
      base::Bind(&SystemTrayDelegateChromeOS::UpdateShowLogoutButtonInTray,
                 base::Unretained(this)));
  user_pref_registrar_->Add(
      prefs::kLogoutDialogDurationMs,
      base::Bind(&SystemTrayDelegateChromeOS::UpdateLogoutDialogDuration,
                 base::Unretained(this)));
  user_pref_registrar_->Add(
      prefs::kAccessibilityLargeCursorEnabled,
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityModeChanged,
                 base::Unretained(this), ash::A11Y_NOTIFICATION_NONE));
  user_pref_registrar_->Add(
      prefs::kAccessibilityAutoclickEnabled,
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityModeChanged,
                 base::Unretained(this), ash::A11Y_NOTIFICATION_NONE));
  user_pref_registrar_->Add(
      prefs::kShouldAlwaysShowAccessibilityMenu,
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityModeChanged,
                 base::Unretained(this), ash::A11Y_NOTIFICATION_NONE));
  user_pref_registrar_->Add(
      prefs::kPerformanceTracingEnabled,
      base::Bind(&SystemTrayDelegateChromeOS::UpdatePerformanceTracing,
                 base::Unretained(this)));

  UpdateShowLogoutButtonInTray();
  UpdateLogoutDialogDuration();
  UpdatePerformanceTracing();
  OnCustodianInfoChanged();
  search_key_mapped_to_ =
      profile->GetPrefs()->GetInteger(prefs::kLanguageRemapSearchKeyTo);
}

bool SystemTrayDelegateChromeOS::UnsetProfile(Profile* profile) {
  if (profile != user_profile_)
    return false;
  user_pref_registrar_.reset();
  user_profile_ = NULL;
  return true;
}

bool SystemTrayDelegateChromeOS::GetShouldUse24HourClockForTesting() const {
  return g_browser_process->platform_part()
      ->GetSystemClock()
      ->ShouldUse24HourClock();
}

void SystemTrayDelegateChromeOS::OnSystemClockChanged(
    system::SystemClock* system_clock) {
  const bool use_24_hour_clock = system_clock->ShouldUse24HourClock();
  clock_type_ = use_24_hour_clock ? base::k24HourClock : base::k12HourClock;
  GetSystemTrayNotifier()->NotifyDateFormatChanged();
}

void SystemTrayDelegateChromeOS::UpdateShowLogoutButtonInTray() {
  GetSystemTrayNotifier()->NotifyShowLoginButtonChanged(
      user_pref_registrar_->prefs()->GetBoolean(
          prefs::kShowLogoutButtonInTray));
}

void SystemTrayDelegateChromeOS::UpdateLogoutDialogDuration() {
  const int duration_ms =
      user_pref_registrar_->prefs()->GetInteger(prefs::kLogoutDialogDurationMs);
  GetSystemTrayNotifier()->NotifyLogoutDialogDurationChanged(
      base::TimeDelta::FromMilliseconds(duration_ms));
}

void SystemTrayDelegateChromeOS::UpdateSessionStartTime() {
  const PrefService* local_state = local_state_registrar_->prefs();
  if (local_state->HasPrefPath(prefs::kSessionStartTime)) {
    have_session_start_time_ = true;
    session_start_time_ = base::TimeTicks::FromInternalValue(
        local_state->GetInt64(prefs::kSessionStartTime));
  } else {
    have_session_start_time_ = false;
    session_start_time_ = base::TimeTicks();
  }
  GetSystemTrayNotifier()->NotifySessionStartTimeChanged();
}

void SystemTrayDelegateChromeOS::UpdateSessionLengthLimit() {
  const PrefService* local_state = local_state_registrar_->prefs();
  if (local_state->HasPrefPath(prefs::kSessionLengthLimit)) {
    have_session_length_limit_ = true;
    session_length_limit_ = base::TimeDelta::FromMilliseconds(
        std::min(std::max(local_state->GetInteger(prefs::kSessionLengthLimit),
                          kSessionLengthLimitMinMs),
                 kSessionLengthLimitMaxMs));
  } else {
    have_session_length_limit_ = false;
    session_length_limit_ = base::TimeDelta();
  }
  GetSystemTrayNotifier()->NotifySessionLengthLimitChanged();
}

void SystemTrayDelegateChromeOS::StopObservingAppWindowRegistry() {
  if (!user_profile_)
    return;

  extensions::AppWindowRegistry* registry =
      extensions::AppWindowRegistry::Factory::GetForBrowserContext(
          user_profile_, false);
  if (registry)
    registry->RemoveObserver(this);
}

void SystemTrayDelegateChromeOS::StopObservingCustodianInfoChanges() {
  if (!user_profile_)
    return;

  SupervisedUserService* service = SupervisedUserServiceFactory::GetForProfile(
      user_profile_);
  if (service)
    service->RemoveObserver(this);
}

void SystemTrayDelegateChromeOS::NotifyIfLastWindowClosed() {
  if (!user_profile_)
    return;

  BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end();
       ++it) {
    if ((*it)->profile()->IsSameProfile(user_profile_)) {
      // The current user has at least one open browser window.
      return;
    }
  }

  if (!extensions::AppWindowRegistry::Get(
          user_profile_)->app_windows().empty()) {
    // The current user has at least one open app window.
    return;
  }

  GetSystemTrayNotifier()->NotifyLastWindowClosed();
}

// Overridden from SessionManagerClient::Observer.
void SystemTrayDelegateChromeOS::ScreenIsLocked() {
  screen_locked_ = true;
  ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(GetUserLoginStatus());
}

void SystemTrayDelegateChromeOS::ScreenIsUnlocked() {
  screen_locked_ = false;
  ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(GetUserLoginStatus());
}

gfx::NativeWindow SystemTrayDelegateChromeOS::GetNativeWindow() const {
  bool session_started = ash::WmShell::Get()
                             ->GetSessionStateDelegate()
                             ->IsActiveUserSessionStarted();
  return GetNativeWindowByStatus(GetUserLoginStatus(), session_started);
}

// content::NotificationObserver implementation.
void SystemTrayDelegateChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_UPGRADE_RECOMMENDED: {
      ash::UpdateInfo info;
      GetUpdateInfo(content::Source<UpgradeDetector>(source).ptr(), &info);
      GetSystemTrayNotifier()->NotifyUpdateRecommended(info);
      break;
    }
    case chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED: {
      // This notification is also sent on login screen when user avatar
      // is loaded from file.
      if (GetUserLoginStatus() != ash::LoginStatus::NOT_LOGGED_IN) {
        GetSystemTrayNotifier()->NotifyUserUpdate();
      }
      break;
    }
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      SetProfile(content::Source<Profile>(source).ptr());
      registrar_->Remove(this,
                         chrome::NOTIFICATION_PROFILE_CREATED,
                         content::NotificationService::AllSources());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      if (UnsetProfile(content::Source<Profile>(source).ptr())) {
        registrar_->Remove(this,
                           chrome::NOTIFICATION_PROFILE_DESTROYED,
                           content::NotificationService::AllSources());
      }
      break;
    }
    case chrome::NOTIFICATION_SESSION_STARTED: {
      session_started_ = true;
      ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(
          GetUserLoginStatus());
      SetProfile(ProfileManager::GetActiveUserProfile());
      break;
    }
    default:
      NOTREACHED();
  }
}

void SystemTrayDelegateChromeOS::OnLanguageRemapSearchKeyToChanged() {
  search_key_mapped_to_ = user_pref_registrar_->prefs()->GetInteger(
      prefs::kLanguageRemapSearchKeyTo);
}

void SystemTrayDelegateChromeOS::OnAccessibilityModeChanged(
    ash::AccessibilityNotificationVisibility notify) {
  GetSystemTrayNotifier()->NotifyAccessibilityModeChanged(notify);
}

void SystemTrayDelegateChromeOS::UpdatePerformanceTracing() {
  if (!user_pref_registrar_)
    return;
  bool value = user_pref_registrar_->prefs()->GetBoolean(
      prefs::kPerformanceTracingEnabled);
  GetSystemTrayNotifier()->NotifyTracingModeChanged(value);
}

// Overridden from InputMethodManager::Observer.
void SystemTrayDelegateChromeOS::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* /* profile */,
    bool show_message) {
  GetSystemTrayNotifier()->NotifyRefreshIME();
}

// Overridden from InputMethodMenuManager::Observer.
void SystemTrayDelegateChromeOS::InputMethodMenuItemChanged(
    ui::ime::InputMethodMenuManager* manager) {
  GetSystemTrayNotifier()->NotifyRefreshIME();
}

// Overridden from CrasAudioHandler::AudioObserver.
void SystemTrayDelegateChromeOS::OnOutputNodeVolumeChanged(uint64_t node_id,
                                                           int volume) {
  GetSystemTrayNotifier()->NotifyAudioOutputVolumeChanged(node_id, volume);
}

void SystemTrayDelegateChromeOS::OnOutputMuteChanged(bool mute_on,
                                                     bool system_adjust) {
  GetSystemTrayNotifier()->NotifyAudioOutputMuteChanged(mute_on, system_adjust);
}

void SystemTrayDelegateChromeOS::OnInputNodeGainChanged(uint64_t /* node_id */,
                                                        int /* gain */) {
}

void SystemTrayDelegateChromeOS::OnInputMuteChanged(bool /* mute_on */) {
}

void SystemTrayDelegateChromeOS::OnAudioNodesChanged() {
  GetSystemTrayNotifier()->NotifyAudioNodesChanged();
}

void SystemTrayDelegateChromeOS::OnActiveOutputNodeChanged() {
  GetSystemTrayNotifier()->NotifyAudioActiveOutputNodeChanged();
}

void SystemTrayDelegateChromeOS::OnActiveInputNodeChanged() {
  GetSystemTrayNotifier()->NotifyAudioActiveInputNodeChanged();
}

// Overridden from BluetoothAdapter::Observer.
void SystemTrayDelegateChromeOS::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter,
    bool discovering) {
  GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
}

void SystemTrayDelegateChromeOS::DeviceAdded(device::BluetoothAdapter* adapter,
                                             device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::DeviceChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::DeviceRemoved(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::OnStartBluetoothDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // If the discovery session was returned after a request to stop discovery
  // (e.g. the user dismissed the Bluetooth detailed view before the call
  // returned), don't claim the discovery session and let it clean up.
  if (!should_run_bluetooth_discovery_)
    return;
  VLOG(1) << "Claiming new Bluetooth device discovery session.";
  bluetooth_discovery_session_ = std::move(discovery_session);
  GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
}

void SystemTrayDelegateChromeOS::UpdateEnterpriseDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  std::string enterprise_domain = connector->GetEnterpriseDomain();
  if (enterprise_domain_ != enterprise_domain) {
    enterprise_domain_ = enterprise_domain;
    GetSystemTrayNotifier()->NotifyEnterpriseDomainChanged();
  }
}

// Overridden from CloudPolicyStore::Observer
void SystemTrayDelegateChromeOS::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  UpdateEnterpriseDomain();
}

void SystemTrayDelegateChromeOS::OnStoreError(policy::CloudPolicyStore* store) {
  UpdateEnterpriseDomain();
}

// Overridden from ash::SessionStateObserver
void SystemTrayDelegateChromeOS::UserAddedToSession(
    const AccountId& /*account_id*/) {
  GetSystemTrayNotifier()->NotifyUserAddedToSession();
}

void SystemTrayDelegateChromeOS::ActiveUserChanged(
    const AccountId& /* user_id */) {}

// Overridden from chrome::BrowserListObserver.
void SystemTrayDelegateChromeOS::OnBrowserRemoved(Browser* browser) {
  NotifyIfLastWindowClosed();
}

// Overridden from extensions::AppWindowRegistry::Observer.
void SystemTrayDelegateChromeOS::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  NotifyIfLastWindowClosed();
}

// Overridden from SupervisedUserServiceObserver.
void SystemTrayDelegateChromeOS::OnCustodianInfoChanged() {
  FOR_EACH_OBSERVER(
      ash::CustodianInfoTrayObserver, custodian_info_changed_observers_,
      OnCustodianInfoChanged());
}

void SystemTrayDelegateChromeOS::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type == ACCESSIBILITY_MANAGER_SHUTDOWN)
    accessibility_subscription_.reset();
  else
    OnAccessibilityModeChanged(details.notify);
}

void SystemTrayDelegateChromeOS::OnShutdownPolicyChanged(
    bool reboot_on_shutdown) {
  // Notify all observers.
  FOR_EACH_OBSERVER(ash::ShutdownPolicyObserver, shutdown_policy_observers_,
                    OnShutdownPolicyChanged(reboot_on_shutdown));
}

void SystemTrayDelegateChromeOS::ImeMenuActivationChanged(bool is_active) {
  GetSystemTrayNotifier()->NotifyRefreshIMEMenu(is_active);
}

void SystemTrayDelegateChromeOS::ImeMenuListChanged() {}

void SystemTrayDelegateChromeOS::ImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<input_method::InputMethodManager::MenuItem>& items) {}

const base::string16
SystemTrayDelegateChromeOS::GetLegacySupervisedUserMessage() const {
  std::string user_manager_name = GetSupervisedUserManager();
  return l10n_util::GetStringFUTF16(
      IDS_USER_IS_SUPERVISED_BY_NOTICE,
      base::UTF8ToUTF16(user_manager_name));
}

const base::string16
SystemTrayDelegateChromeOS::GetChildUserMessage() const {
#if defined(ENABLE_SUPERVISED_USERS)
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(user_profile_);
  base::string16 first_custodian =
      base::UTF8ToUTF16(service->GetCustodianEmailAddress());
  base::string16 second_custodian =
      base::UTF8ToUTF16(service->GetSecondCustodianEmailAddress());
  LOG_IF(WARNING, first_custodian.empty()) <<
      "Returning incomplete child user message as manager not known yet.";
  if (second_custodian.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_CHILD_USER_IS_MANAGED_BY_ONE_PARENT_NOTICE, first_custodian);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_CHILD_USER_IS_MANAGED_BY_TWO_PARENTS_NOTICE,
        first_custodian,
        second_custodian);
  }
#endif

  LOG(WARNING) << "SystemTrayDelegateChromeOS::GetChildUserMessage call while "
               << "ENABLE_SUPERVISED_USERS undefined.";
  return base::string16();
}

ash::SystemTrayDelegate* CreateSystemTrayDelegate() {
  return new SystemTrayDelegateChromeOS();
}

}  // namespace chromeos
