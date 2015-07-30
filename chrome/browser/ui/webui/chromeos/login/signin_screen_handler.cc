// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include <algorithm>
#include <vector>

#include "ash/shell.h"
#include "ash/wm/lock_state_controller.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/chromeos_utils.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/hwid_checker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/proximity_auth_facade.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

// Max number of users to show.
const size_t kMaxUsers = 18;

// Timeout to delay first notification about offline state for a
// current network.
const int kOfflineTimeoutSec = 5;

// Timeout used to prevent infinite connecting to a flaky network.
const int kConnectingTimeoutSec = 60;

// Type of the login screen UI that is currently presented to user.
const char kSourceGaiaSignin[] = "gaia-signin";
const char kSourceAccountPicker[] = "account-picker";

static bool Contains(const std::vector<std::string>& container,
                     const std::string& value) {
  return std::find(container.begin(), container.end(), value) !=
         container.end();
}

class CallOnReturn {
 public:
  explicit CallOnReturn(const base::Closure& callback)
      : callback_(callback), call_scheduled_(false) {}

  ~CallOnReturn() {
    if (call_scheduled_ && !callback_.is_null())
      callback_.Run();
  }

  void CancelScheduledCall() { call_scheduled_ = false; }
  void ScheduleCall() { call_scheduled_ = true; }

 private:
  base::Closure callback_;
  bool call_scheduled_;

  DISALLOW_COPY_AND_ASSIGN(CallOnReturn);
};

}  // namespace

namespace chromeos {

namespace {

bool IsOnline(NetworkStateInformer::State state,
              NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::ONLINE &&
         reason != NetworkError::ERROR_REASON_PORTAL_DETECTED &&
         reason != NetworkError::ERROR_REASON_LOADING_TIMEOUT;
}

bool IsBehindCaptivePortal(NetworkStateInformer::State state,
                           NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::CAPTIVE_PORTAL ||
         reason == NetworkError::ERROR_REASON_PORTAL_DETECTED;
}

bool IsProxyError(NetworkStateInformer::State state,
                  NetworkError::ErrorReason reason,
                  net::Error frame_error) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
         reason == NetworkError::ERROR_REASON_PROXY_AUTH_CANCELLED ||
         reason == NetworkError::ERROR_REASON_PROXY_CONNECTION_FAILED ||
         (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
          (frame_error == net::ERR_PROXY_CONNECTION_FAILED ||
           frame_error == net::ERR_TUNNEL_CONNECTION_FAILED));
}

bool IsSigninScreen(const OobeUI::Screen screen) {
  return screen == OobeUI::SCREEN_GAIA_SIGNIN ||
      screen == OobeUI::SCREEN_ACCOUNT_PICKER;
}

bool IsSigninScreenError(NetworkError::ErrorState error_state) {
  return error_state == NetworkError::ERROR_STATE_PORTAL ||
         error_state == NetworkError::ERROR_STATE_OFFLINE ||
         error_state == NetworkError::ERROR_STATE_PROXY ||
         error_state == NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT;
}

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (!network)
    return std::string();
  return network->name();
}

static bool SetUserInputMethodImpl(
    const std::string& username,
    const std::string& user_input_method,
    input_method::InputMethodManager::State* ime_state) {
  if (!chromeos::input_method::InputMethodManager::Get()->IsLoginKeyboard(
          user_input_method)) {
    LOG(WARNING) << "SetUserInputMethod('" << username
                 << "'): stored user LRU input method '" << user_input_method
                 << "' is no longer Full Latin Keyboard Language"
                 << " (entry dropped). Use hardware default instead.";

    PrefService* const local_state = g_browser_process->local_state();
    DictionaryPrefUpdate updater(local_state, prefs::kUsersLRUInputMethod);

    base::DictionaryValue* const users_lru_input_methods = updater.Get();
    if (users_lru_input_methods != NULL) {
      users_lru_input_methods->SetStringWithoutPathExpansion(username, "");
    }
    return false;
  }

  if (!Contains(ime_state->GetActiveInputMethodIds(), user_input_method)) {
    if (!ime_state->EnableInputMethod(user_input_method)) {
      DLOG(ERROR) << "SigninScreenHandler::SetUserInputMethod('" << username
                  << "'): user input method '" << user_input_method
                  << "' is not enabled and enabling failed (ignored!).";
    }
  }
  ime_state->ChangeInputMethod(user_input_method, false /* show_message */);

  return true;
}

}  // namespace

// LoginScreenContext implementation ------------------------------------------

LoginScreenContext::LoginScreenContext() {
  Init();
}

LoginScreenContext::LoginScreenContext(const base::ListValue* args) {
  Init();

  if (!args || args->GetSize() == 0)
    return;
  std::string email;
  if (args->GetString(0, &email))
    email_ = email;
}

void LoginScreenContext::Init() {
  oobe_ui_ = false;
}

// SigninScreenHandler implementation ------------------------------------------

SigninScreenHandler::SigninScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    NetworkErrorModel* network_error_model,
    CoreOobeActor* core_oobe_actor,
    GaiaScreenHandler* gaia_screen_handler)
    : network_state_informer_(network_state_informer),
      network_error_model_(network_error_model),
      core_oobe_actor_(core_oobe_actor),
      caps_lock_enabled_(chromeos::input_method::InputMethodManager::Get()
                             ->GetImeKeyboard()
                             ->CapsLockIsEnabled()),
      gaia_screen_handler_(gaia_screen_handler),
      histogram_helper_(new ErrorScreensHistogramHelper("Signin")),
      weak_factory_(this) {
  DCHECK(network_state_informer_.get());
  DCHECK(network_error_model_);
  DCHECK(core_oobe_actor_);
  DCHECK(gaia_screen_handler_);
  gaia_screen_handler_->SetSigninScreenHandler(this);
  network_state_informer_->AddObserver(this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_NEEDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());

  chromeos::input_method::ImeKeyboard* keyboard =
      chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
  if (keyboard)
    keyboard->AddObserver(this);

  max_mode_delegate_.reset(new TouchViewControllerDelegate());
  max_mode_delegate_->AddObserver(this);

  policy::ConsumerManagementService* consumer_management =
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetConsumerManagementService();
  is_enrolling_consumer_management_ =
      consumer_management &&
      consumer_management->GetStage().IsEnrollmentRequested();
}

SigninScreenHandler::~SigninScreenHandler() {
  OobeUI* oobe_ui = GetOobeUI();
  if (oobe_ui && oobe_ui_observer_added_)
    oobe_ui->RemoveObserver(this);
  chromeos::input_method::ImeKeyboard* keyboard =
      chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
  if (keyboard)
    keyboard->RemoveObserver(this);
  weak_factory_.InvalidateWeakPtrs();
  if (delegate_)
    delegate_->SetWebUIHandler(NULL);
  network_state_informer_->RemoveObserver(this);
  if (max_mode_delegate_) {
    max_mode_delegate_->RemoveObserver(this);
    max_mode_delegate_.reset(NULL);
  }
  GetScreenlockBridgeInstance()->SetLockHandler(NULL);
  GetScreenlockBridgeInstance()->SetFocusedUser("");
}

// static
std::string SigninScreenHandler::GetUserLRUInputMethod(
    const std::string& username) {
  PrefService* const local_state = g_browser_process->local_state();
  const base::DictionaryValue* users_lru_input_methods =
      local_state->GetDictionary(prefs::kUsersLRUInputMethod);

  if (!users_lru_input_methods) {
    DLOG(WARNING) << "GetUserLRUInputMethod('" << username
                  << "'): no kUsersLRUInputMethod";
    return std::string();
  }

  std::string input_method;

  if (!users_lru_input_methods->GetStringWithoutPathExpansion(username,
                                                              &input_method)) {
    DVLOG(0) << "GetUserLRUInputMethod('" << username
             << "'): no input method for this user";
    return std::string();
  }

  return input_method;
}

// static
// Update keyboard layout to least recently used by the user.
void SigninScreenHandler::SetUserInputMethod(
    const std::string& username,
    input_method::InputMethodManager::State* ime_state) {
  bool succeed = false;

  const std::string input_method = GetUserLRUInputMethod(username);

  if (!input_method.empty())
    succeed = SetUserInputMethodImpl(username, input_method, ime_state);

  // This is also a case when LRU layout is set only for a few local users,
  // thus others need to be switched to default locale.
  // Otherwise they will end up using another user's locale to log in.
  if (!succeed) {
    DVLOG(0) << "SetUserInputMethod('" << username
             << "'): failed to set user layout. Switching to default.";

    ime_state->SetInputMethodLoginDefault();
  }
}

void SigninScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("passwordHint", IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT);
  builder->Add("signingIn", IDS_LOGIN_POD_SIGNING_IN);
  builder->Add("podMenuButtonAccessibleName",
               IDS_LOGIN_POD_MENU_BUTTON_ACCESSIBLE_NAME);
  builder->Add("podMenuRemoveItemAccessibleName",
               IDS_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME);
  builder->Add("passwordFieldAccessibleName",
               IDS_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME);
  builder->Add("signedIn", IDS_SCREEN_LOCK_ACTIVE_USER);
  builder->Add("signinButton", IDS_LOGIN_BUTTON);
  builder->Add("launchAppButton", IDS_LAUNCH_APP_BUTTON);
  builder->Add("restart", IDS_RESTART_BUTTON);
  builder->Add("shutDown", IDS_SHUTDOWN_BUTTON);
  builder->Add("addUser", IDS_ADD_USER_BUTTON);
  builder->Add("browseAsGuest", IDS_GO_INCOGNITO_BUTTON);
  builder->Add("moreOptions", IDS_MORE_OPTIONS_BUTTON);
  builder->Add("addSupervisedUser", IDS_CREATE_SUPERVISED_USER_MENU_LABEL);
  builder->Add("cancel", IDS_CANCEL);
  builder->Add("signOutUser", IDS_SCREEN_LOCK_SIGN_OUT);
  builder->Add("offlineLogin", IDS_OFFLINE_LOGIN_HTML);
  builder->Add("ownerUserPattern", IDS_LOGIN_POD_OWNER_USER);
  builder->Add("removeUser", IDS_LOGIN_POD_REMOVE_USER);
  builder->Add("errorTpmFailureTitle", IDS_LOGIN_ERROR_TPM_FAILURE_TITLE);
  builder->Add("errorTpmFailureReboot", IDS_LOGIN_ERROR_TPM_FAILURE_REBOOT);
  builder->Add("errorTpmFailureRebootButton",
               IDS_LOGIN_ERROR_TPM_FAILURE_REBOOT_BUTTON);

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  builder->Add("disabledAddUserTooltip",
               connector->IsEnterpriseManaged()
                   ? IDS_DISABLED_ADD_USER_TOOLTIP_ENTERPRISE
                   : IDS_DISABLED_ADD_USER_TOOLTIP);

  builder->Add("supervisedUserExpiredTokenWarning",
               IDS_SUPERVISED_USER_EXPIRED_TOKEN_WARNING);
  builder->Add("signinBannerText", IDS_LOGIN_USER_ADDING_BANNER);

  // Multi-profiles related strings.
  builder->Add("multiProfilesRestrictedPolicyTitle",
               IDS_MULTI_PROFILES_RESTRICTED_POLICY_TITLE);
  builder->Add("multiProfilesNotAllowedPolicyMsg",
               IDS_MULTI_PROFILES_NOT_ALLOWED_POLICY_MSG);
  builder->Add("multiProfilesPrimaryOnlyPolicyMsg",
               IDS_MULTI_PROFILES_PRIMARY_ONLY_POLICY_MSG);
  builder->Add("multiProfilesOwnerPrimaryOnlyMsg",
               IDS_MULTI_PROFILES_OWNER_PRIMARY_ONLY_MSG);

  // Strings used by password changed dialog.
  builder->Add("passwordChangedTitle", IDS_LOGIN_PASSWORD_CHANGED_TITLE);
  builder->Add("passwordChangedDesc", IDS_LOGIN_PASSWORD_CHANGED_DESC);
  builder->AddF("passwordChangedMoreInfo",
                IDS_LOGIN_PASSWORD_CHANGED_MORE_INFO,
                IDS_SHORT_PRODUCT_OS_NAME);

  builder->Add("oldPasswordHint", IDS_LOGIN_PASSWORD_CHANGED_OLD_PASSWORD_HINT);
  builder->Add("oldPasswordIncorrect",
               IDS_LOGIN_PASSWORD_CHANGED_INCORRECT_OLD_PASSWORD);
  builder->Add("passwordChangedCantRemember",
               IDS_LOGIN_PASSWORD_CHANGED_CANT_REMEMBER);
  builder->Add("passwordChangedBackButton",
               IDS_LOGIN_PASSWORD_CHANGED_BACK_BUTTON);
  builder->Add("passwordChangedsOkButton", IDS_OK);
  builder->Add("passwordChangedProceedAnyway",
               IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY);
  builder->Add("proceedAnywayButton",
               IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY_BUTTON);
  builder->Add("nextButtonText", IDS_NEWGAIA_OFFLINE_NEXT_BUTTON_TEXT);
  builder->Add("forgotOldPasswordButtonText",
               IDS_LOGIN_NEWGAIA_PASSWORD_CHANGED_FORGOT_PASSWORD);
  builder->AddF("passwordChangedTitle",
                IDS_LOGIN_NEWGAIA_PASSWORD_CHANGED_TITLE,
                GetChromeDeviceType());
  builder->Add("passwordChangedProceedAnywayTitle",
               IDS_LOGIN_NEWGAIA_PASSWORD_CHANGED_PROCEED_ANYWAY);
  builder->Add("passwordChangedTryAgain",
               IDS_LOGIN_NEWGAIA_PASSWORD_CHANGED_TRY_AGAIN);
  builder->Add("publicAccountInfoFormat", IDS_LOGIN_PUBLIC_ACCOUNT_INFO_FORMAT);
  builder->Add("publicAccountReminder",
               IDS_LOGIN_PUBLIC_ACCOUNT_SIGNOUT_REMINDER);
  builder->Add("publicSessionLanguageAndInput",
               IDS_LOGIN_PUBLIC_SESSION_LANGUAGE_AND_INPUT);
  builder->Add("publicAccountEnter", IDS_LOGIN_PUBLIC_ACCOUNT_ENTER);
  builder->Add("publicAccountEnterAccessibleName",
               IDS_LOGIN_PUBLIC_ACCOUNT_ENTER_ACCESSIBLE_NAME);
  builder->Add("publicSessionSelectLanguage", IDS_LANGUAGE_SELECTION_SELECT);
  builder->Add("publicSessionSelectKeyboard", IDS_KEYBOARD_SELECTION_SELECT);
  builder->Add("removeUserWarningText",
               base::string16());
  builder->AddF("removeLegacySupervisedUserWarningText",
               IDS_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
               base::UTF8ToUTF16(chrome::kSupervisedUserManagementDisplayURL));
  builder->Add("removeUserWarningButtonTitle",
               IDS_LOGIN_POD_USER_REMOVE_WARNING_BUTTON);

  if (StartupUtils::IsWebviewSigninEnabled()) {
    builder->Add("samlNotice", IDS_LOGIN_SAML_NOTICE_NEW_GAIA_FLOW);
    builder->Add("confirmPasswordTitle",
                 IDS_LOGIN_CONFIRM_PASSWORD_TITLE_NEW_GAIA_FLOW);
    builder->Add("confirmPasswordLabel",
                 IDS_LOGIN_CONFIRM_PASSWORD_LABEL_NEW_GAIA_FLOW);
  } else {
    builder->Add("samlNotice", IDS_LOGIN_SAML_NOTICE);
    builder->Add("confirmPasswordTitle", IDS_LOGIN_CONFIRM_PASSWORD_TITLE);
    builder->Add("confirmPasswordLabel", IDS_LOGIN_CONFIRM_PASSWORD_LABEL);
  }
  builder->Add("confirmPasswordConfirmButton",
               IDS_LOGIN_CONFIRM_PASSWORD_CONFIRM_BUTTON);
  builder->Add("confirmPasswordText", IDS_LOGIN_CONFIRM_PASSWORD_TEXT);
  builder->Add("confirmPasswordErrorText",
               IDS_LOGIN_CONFIRM_PASSWORD_ERROR_TEXT);

  builder->Add("confirmPasswordIncorrectPassword",
               IDS_LOGIN_CONFIRM_PASSWORD_INCORRECT_PASSWORD);
  builder->Add("accountSetupCancelDialogTitle",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_TITLE);
  builder->Add("accountSetupCancelDialogNo",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_NO);
  builder->Add("accountSetupCancelDialogYes",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_YES);

  builder->Add("fatalEnrollmentError",
               IDS_ENTERPRISE_ENROLLMENT_AUTH_FATAL_ERROR);
  builder->Add("insecureURLEnrollmentError",
               IDS_ENTERPRISE_ENROLLMENT_AUTH_INSECURE_URL_ERROR);
}

void SigninScreenHandler::RegisterMessages() {
  AddCallback("authenticateUser", &SigninScreenHandler::HandleAuthenticateUser);
  AddCallback("launchIncognito", &SigninScreenHandler::HandleLaunchIncognito);
  AddCallback("showSupervisedUserCreationScreen",
              &SigninScreenHandler::HandleShowSupervisedUserCreationScreen);
  AddCallback("launchPublicSession",
              &SigninScreenHandler::HandleLaunchPublicSession);
  AddRawCallback("offlineLogin", &SigninScreenHandler::HandleOfflineLogin);
  AddCallback("rebootSystem", &SigninScreenHandler::HandleRebootSystem);
  AddRawCallback("showAddUser", &SigninScreenHandler::HandleShowAddUser);
  AddCallback("shutdownSystem", &SigninScreenHandler::HandleShutdownSystem);
  AddCallback("loadWallpaper", &SigninScreenHandler::HandleLoadWallpaper);
  AddCallback("removeUser", &SigninScreenHandler::HandleRemoveUser);
  AddCallback("toggleEnrollmentScreen",
              &SigninScreenHandler::HandleToggleEnrollmentScreen);
  AddCallback("toggleEnableDebuggingScreen",
              &SigninScreenHandler::HandleToggleEnableDebuggingScreen);
  AddCallback("toggleKioskEnableScreen",
              &SigninScreenHandler::HandleToggleKioskEnableScreen);
  AddCallback("createAccount", &SigninScreenHandler::HandleCreateAccount);
  AddCallback("accountPickerReady",
              &SigninScreenHandler::HandleAccountPickerReady);
  AddCallback("wallpaperReady", &SigninScreenHandler::HandleWallpaperReady);
  AddCallback("signOutUser", &SigninScreenHandler::HandleSignOutUser);
  AddCallback("openProxySettings",
              &SigninScreenHandler::HandleOpenProxySettings);
  AddCallback("loginVisible", &SigninScreenHandler::HandleLoginVisible);
  AddCallback("cancelPasswordChangedFlow",
              &SigninScreenHandler::HandleCancelPasswordChangedFlow);
  AddCallback("cancelUserAdding", &SigninScreenHandler::HandleCancelUserAdding);
  AddCallback("migrateUserData", &SigninScreenHandler::HandleMigrateUserData);
  AddCallback("resyncUserData", &SigninScreenHandler::HandleResyncUserData);
  AddCallback("loginUIStateChanged",
              &SigninScreenHandler::HandleLoginUIStateChanged);
  AddCallback("unlockOnLoginSuccess",
              &SigninScreenHandler::HandleUnlockOnLoginSuccess);
  AddCallback("showLoadingTimeoutError",
              &SigninScreenHandler::HandleShowLoadingTimeoutError);
  AddCallback("updateOfflineLogin",
              &SigninScreenHandler::HandleUpdateOfflineLogin);
  AddCallback("focusPod", &SigninScreenHandler::HandleFocusPod);
  AddCallback("getPublicSessionKeyboardLayouts",
              &SigninScreenHandler::HandleGetPublicSessionKeyboardLayouts);
  AddCallback("cancelConsumerManagementEnrollment",
              &SigninScreenHandler::HandleCancelConsumerManagementEnrollment);
  AddCallback("getTouchViewState",
              &SigninScreenHandler::HandleGetTouchViewState);
  AddCallback("logRemoveUserWarningShown",
              &SigninScreenHandler::HandleLogRemoveUserWarningShown);
  AddCallback("firstIncorrectPasswordAttempt",
              &SigninScreenHandler::HandleFirstIncorrectPasswordAttempt);
  AddCallback("maxIncorrectPasswordAttempts",
              &SigninScreenHandler::HandleMaxIncorrectPasswordAttempts);

  // This message is sent by the kiosk app menu, but is handled here
  // so we can tell the delegate to launch the app.
  AddCallback("launchKioskApp", &SigninScreenHandler::HandleLaunchKioskApp);
}

void SigninScreenHandler::Show(const LoginScreenContext& context) {
  CHECK(delegate_);

  // Just initialize internal fields from context and call ShowImpl().
  oobe_ui_ = context.oobe_ui();

  std::string email;
  if (is_enrolling_consumer_management_) {
    // We don't check if the value of the owner e-mail is trusted because it is
    // only used to pre-fill the e-mail field in Gaia sign-in page and a cached
    // value is sufficient.
    CrosSettings::Get()->GetString(kDeviceOwner, &email);
  } else {
    email = context.email();
  }
  gaia_screen_handler_->PopulateEmail(email);
  ShowImpl();
  histogram_helper_->OnScreenShow();
}

void SigninScreenHandler::SetDelegate(SigninScreenHandlerDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_)
    delegate_->SetWebUIHandler(this);
}

void SigninScreenHandler::SetNativeWindowDelegate(
    NativeWindowDelegate* native_window_delegate) {
  native_window_delegate_ = native_window_delegate;
}

void SigninScreenHandler::OnNetworkReady() {
  VLOG(1) << "OnNetworkReady() call.";
  DCHECK(gaia_screen_handler_);
  gaia_screen_handler_->MaybePreloadAuthExtension();
}

void SigninScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  // ERROR_REASON_FRAME_ERROR is an explicit signal from GAIA frame so it shoud
  // force network error UI update.
  bool force_update = reason == NetworkError::ERROR_REASON_FRAME_ERROR;
  UpdateStateInternal(reason, force_update);
}

void SigninScreenHandler::SetFocusPODCallbackForTesting(
    base::Closure callback) {
  test_focus_pod_callback_ = callback;
}

void SigninScreenHandler::ZeroOfflineTimeoutForTesting() {
  zero_offline_timeout_for_test_ = true;
}

// SigninScreenHandler, private: -----------------------------------------------

void SigninScreenHandler::ShowImpl() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  if (!ime_state_.get())
    ime_state_ = input_method::InputMethodManager::Get()->GetActiveIMEState();

  if (!oobe_ui_observer_added_) {
    oobe_ui_observer_added_ = true;
    GetOobeUI()->AddObserver(this);
  }

  if (oobe_ui_ || is_enrolling_consumer_management_) {
    // Shows new user sign-in for OOBE.
    OnShowAddUser();
  } else {
    // Populates account picker. Animation is turned off for now until we
    // figure out how to make it fast enough.
    delegate_->HandleGetUsers();

    // Reset Caps Lock state when login screen is shown.
    input_method::InputMethodManager::Get()
        ->GetImeKeyboard()
        ->SetCapsLockEnabled(false);

    base::DictionaryValue params;
    params.SetBoolean("disableAddUser", AllWhitelistedUsersPresent());
    UpdateUIState(UI_STATE_ACCOUNT_PICKER, &params);
  }
}

void SigninScreenHandler::UpdateUIState(UIState ui_state,
                                        base::DictionaryValue* params) {
  switch (ui_state) {
    case UI_STATE_GAIA_SIGNIN:
      ui_state_ = UI_STATE_GAIA_SIGNIN;
      ShowScreen(OobeUI::kScreenGaiaSignin, params);
      break;
    case UI_STATE_ACCOUNT_PICKER:
      ui_state_ = UI_STATE_ACCOUNT_PICKER;
      DCHECK(gaia_screen_handler_);
      gaia_screen_handler_->CancelShowGaiaAsync();
      ShowScreen(OobeUI::kScreenAccountPicker, params);
      break;
    default:
      NOTREACHED();
      break;
  }
}

// TODO(antrim@): split this method into small parts.
// TODO(antrim@): move this logic to GaiaScreenHandler.
void SigninScreenHandler::UpdateStateInternal(NetworkError::ErrorReason reason,
                                              bool force_update) {
  // Do nothing once user has signed in or sign in is in progress.
  // TODO(antrim): We will end up here when processing network state
  // notification but no ShowSigninScreen() was called so delegate_ will be
  // NULL. Network state processing logic does not belong here.
  if (delegate_ &&
      (delegate_->IsUserSigninCompleted() || delegate_->IsSigninInProgress())) {
    return;
  }

  NetworkStateInformer::State state = network_state_informer_->state();
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name = GetNetworkName(network_path);

  // Skip "update" notification about OFFLINE state from
  // NetworkStateInformer if previous notification already was
  // delayed.
  if ((state == NetworkStateInformer::OFFLINE || has_pending_auth_ui_) &&
      !force_update && !update_state_closure_.IsCancelled()) {
    return;
  }

  update_state_closure_.Cancel();

  if ((state == NetworkStateInformer::OFFLINE && !force_update) ||
      has_pending_auth_ui_) {
    update_state_closure_.Reset(
        base::Bind(&SigninScreenHandler::UpdateStateInternal,
                   weak_factory_.GetWeakPtr(),
                   reason,
                   true));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        update_state_closure_.callback(),
        base::TimeDelta::FromSeconds(
            zero_offline_timeout_for_test_ ? 0 : kOfflineTimeoutSec));
    return;
  }

  // Don't show or hide error screen if we're in connecting state.
  if (state == NetworkStateInformer::CONNECTING && !force_update) {
    if (connecting_closure_.IsCancelled()) {
      // First notification about CONNECTING state.
      connecting_closure_.Reset(
          base::Bind(&SigninScreenHandler::UpdateStateInternal,
                     weak_factory_.GetWeakPtr(),
                     reason,
                     true));
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          connecting_closure_.callback(),
          base::TimeDelta::FromSeconds(kConnectingTimeoutSec));
    }
    return;
  }
  connecting_closure_.Cancel();

  const bool is_online = IsOnline(state, reason);
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_gaia_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);
  const bool is_gaia_error =
      FrameError() != net::OK && FrameError() != net::ERR_NETWORK_CHANGED;
  const bool is_gaia_signin = IsGaiaVisible() || IsGaiaHiddenByError();
  const bool error_screen_should_overlay =
      !offline_login_active_ && IsGaiaVisible();
  const bool from_not_online_to_online_transition =
      is_online && last_network_state_ != NetworkStateInformer::ONLINE;
  last_network_state_ = state;

  CallOnReturn reload_gaia(base::Bind(
      &SigninScreenHandler::ReloadGaia, weak_factory_.GetWeakPtr(), true));

  if (is_online || !is_behind_captive_portal)
    network_error_model_->HideCaptivePortal();

  // Hide offline message (if needed) and return if current screen is
  // not a Gaia frame.
  if (!is_gaia_signin) {
    if (!IsSigninScreenHiddenByError())
      HideOfflineMessage(state, reason);
    return;
  }

  // Reload frame if network state is changed from {!ONLINE} -> ONLINE state.
  if (reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED &&
      from_not_online_to_online_transition) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frame load since network has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (reason == NetworkError::ERROR_REASON_PROXY_CONFIG_CHANGED &&
      error_screen_should_overlay) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frameload since proxy settings has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
      reason != gaia_reload_reason_ &&
      !IsProxyError(state, reason, FrameError())) {
    LOG(WARNING) << "Retry frame load due to reason: "
                 << NetworkError::ErrorReasonString(reason);
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (is_gaia_loading_timeout) {
    LOG(WARNING) << "Retry frame load due to loading timeout.";
    LOG(ERROR) << "UpdateStateInternal reload 4";
    reload_gaia.ScheduleCall();
  }

  if ((!is_online || is_gaia_loading_timeout || is_gaia_error) &&
      !offline_login_active_) {
    SetupAndShowOfflineMessage(state, reason);
  } else {
    HideOfflineMessage(state, reason);

    // Cancel scheduled GAIA reload (if any) to prevent double reloads.
    reload_gaia.CancelScheduledCall();
  }
}

void SigninScreenHandler::SetupAndShowOfflineMessage(
    NetworkStateInformer::State state,
    NetworkError::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = IsProxyError(state, reason, FrameError());
  const bool is_gaia_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);

  if (is_proxy_error) {
    network_error_model_->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                        std::string());
  } else if (is_behind_captive_portal) {
    // Do not bother a user with obsessive captive portal showing. This
    // check makes captive portal being shown only once: either when error
    // screen is shown for the first time or when switching from another
    // error screen (offline, proxy).
    if (IsGaiaVisible() || (network_error_model_->GetErrorState() !=
                            NetworkError::ERROR_STATE_PORTAL)) {
      network_error_model_->FixCaptivePortal();
    }
    const std::string network_name = GetNetworkName(network_path);
    network_error_model_->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                        network_name);
  } else if (is_gaia_loading_timeout) {
    network_error_model_->SetErrorState(
        NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT, std::string());
  } else {
    network_error_model_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                        std::string());
  }

  const bool guest_signin_allowed =
      IsGuestSigninAllowed() &&
      IsSigninScreenError(network_error_model_->GetErrorState());
  network_error_model_->AllowGuestSignin(guest_signin_allowed);

  const bool offline_login_allowed =
      IsOfflineLoginAllowed() &&
      IsSigninScreenError(network_error_model_->GetErrorState()) &&
      network_error_model_->GetErrorState() !=
          NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT;
  network_error_model_->AllowOfflineLogin(offline_login_allowed);

  if (GetCurrentScreen() != OobeUI::SCREEN_ERROR_MESSAGE) {
    network_error_model_->SetUIState(NetworkError::UI_STATE_SIGNIN);
    network_error_model_->SetParentScreen(OobeUI::SCREEN_GAIA_SIGNIN);
    network_error_model_->Show();
    histogram_helper_->OnErrorShow(network_error_model_->GetErrorState());
  }
}

void SigninScreenHandler::HideOfflineMessage(NetworkStateInformer::State state,
                                             NetworkError::ErrorReason reason) {
  if (!IsSigninScreenHiddenByError())
    return;

  gaia_reload_reason_ = NetworkError::ERROR_REASON_NONE;

  network_error_model_->Hide();
  histogram_helper_->OnErrorHide();

  // Forces a reload for Gaia screen on hiding error message.
  if (IsGaiaVisible() || IsGaiaHiddenByError())
    ReloadGaia(reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED);
}

void SigninScreenHandler::ReloadGaia(bool force_reload) {
  gaia_screen_handler_->ReloadGaia(force_reload);
}

void SigninScreenHandler::Initialize() {
  // If delegate_ is NULL here (e.g. WebUIScreenLocker has been destroyed),
  // don't do anything, just return.
  if (!delegate_)
    return;

  if (show_on_init_) {
    show_on_init_ = false;
    ShowImpl();
  }
}

gfx::NativeWindow SigninScreenHandler::GetNativeWindow() {
  if (native_window_delegate_)
    return native_window_delegate_->GetNativeWindow();
  return NULL;
}

void SigninScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kUsersLRUInputMethod);
}

void SigninScreenHandler::OnCurrentScreenChanged(OobeUI::Screen current_screen,
                                                 OobeUI::Screen new_screen) {
  if (new_screen == OobeUI::SCREEN_ACCOUNT_PICKER) {
    // Restore active IME state if returning to user pod row screen.
    input_method::InputMethodManager::Get()->SetState(ime_state_);
  }
}

void SigninScreenHandler::ClearAndEnablePassword() {
  core_oobe_actor_->ResetSignInUI(false);
}

void SigninScreenHandler::ClearUserPodPassword() {
  core_oobe_actor_->ClearUserPodPassword();
}

void SigninScreenHandler::RefocusCurrentPod() {
  core_oobe_actor_->RefocusCurrentPod();
}

void SigninScreenHandler::OnUserRemoved(const std::string& username) {
  CallJS("login.AccountPickerScreen.removeUser", username);
  if (delegate_->GetUsers().empty())
    OnShowAddUser();
}

void SigninScreenHandler::OnUserImageChanged(const user_manager::User& user) {
  if (page_is_ready())
    CallJS("login.AccountPickerScreen.updateUserImage", user.email());
}

void SigninScreenHandler::OnPreferencesChanged() {
  // Make sure that one of the login UI is fully functional now, otherwise
  // preferences update would be picked up next time it will be shown.
  if (!webui_visible_) {
    LOG(WARNING) << "Login UI is not active - postponed prefs change.";
    preferences_changed_delayed_ = true;
    return;
  }

  if (delegate_ && !delegate_->IsShowUsers()) {
    HandleShowAddUser(NULL);
  } else {
    if (delegate_)
      delegate_->HandleGetUsers();
    UpdateUIState(UI_STATE_ACCOUNT_PICKER, NULL);
  }
  preferences_changed_delayed_ = false;
}

void SigninScreenHandler::ResetSigninScreenHandlerDelegate() {
  SetDelegate(NULL);
}

void SigninScreenHandler::ShowError(int login_attempts,
                                    const std::string& error_text,
                                    const std::string& help_link_text,
                                    HelpAppLauncher::HelpTopic help_topic_id) {
  core_oobe_actor_->ShowSignInError(login_attempts, error_text, help_link_text,
                                    help_topic_id);
}

void SigninScreenHandler::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  switch (error_id) {
    case LoginDisplay::TPM_ERROR:
      core_oobe_actor_->ShowTpmError();
      break;
    default:
      NOTREACHED() << "Unknown sign in error";
      break;
  }
}

void SigninScreenHandler::ShowSigninUI(const std::string& email) {
  core_oobe_actor_->ShowSignInUI(email);
}

void SigninScreenHandler::ShowGaiaPasswordChanged(const std::string& username) {
  gaia_screen_handler_->PasswordChangedFor(username);
  gaia_screen_handler_->PopulateEmail(username);
  core_oobe_actor_->ShowSignInUI(username);
  CallJS("login.setAuthType", username,
         static_cast<int>(UserSelectionScreen::ONLINE_SIGN_IN),
         base::StringValue(""));
}

void SigninScreenHandler::ShowPasswordChangedDialog(bool show_password_error,
                                                    const std::string& email) {
  core_oobe_actor_->ShowPasswordChangedScreen(show_password_error, email);
}

void SigninScreenHandler::ShowSigninScreenForCreds(
    const std::string& username,
    const std::string& password) {
  DCHECK(gaia_screen_handler_);
  gaia_screen_handler_->ShowSigninScreenForCreds(username, password);
}

void SigninScreenHandler::ShowWhitelistCheckFailedError() {
  DCHECK(gaia_screen_handler_);
  gaia_screen_handler_->ShowWhitelistCheckFailedError();
}

void SigninScreenHandler::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_AUTH_NEEDED: {
      has_pending_auth_ui_ = true;
      break;
    }
    case chrome::NOTIFICATION_AUTH_SUPPLIED:
      has_pending_auth_ui_ = false;
      // Reload auth extension as proxy credentials are supplied.
      if (!IsSigninScreenHiddenByError() && ui_state_ == UI_STATE_GAIA_SIGNIN)
        ReloadGaia(true);
      update_state_closure_.Cancel();
      break;
    case chrome::NOTIFICATION_AUTH_CANCELLED: {
      // Don't reload auth extension if proxy auth dialog was cancelled.
      has_pending_auth_ui_ = false;
      update_state_closure_.Cancel();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void SigninScreenHandler::OnMaximizeModeStarted() {
  CallJS("login.AccountPickerScreen.setTouchViewState", true);
}

void SigninScreenHandler::OnMaximizeModeEnded() {
  CallJS("login.AccountPickerScreen.setTouchViewState", false);
}

bool SigninScreenHandler::ShouldLoadGaia() const {
  // Fetching of the extension is not started before account picker page is
  // loaded because it can affect the loading speed.
  // Do not load the extension for the screen locker, see crosbug.com/25018.
  return !ScreenLocker::default_screen_locker() &&
         is_account_picker_showing_first_time_;
}

void SigninScreenHandler::UserSettingsChanged() {
  DCHECK(gaia_screen_handler_);
  GaiaContext context;
  if (delegate_)
    context.has_users = !delegate_->GetUsers().empty();
  gaia_screen_handler_->UpdateGaia(context);
  UpdateAddButtonStatus();
}

void SigninScreenHandler::UpdateAddButtonStatus() {
  CallJS("cr.ui.login.DisplayManager.updateAddUserButtonStatus",
         AllWhitelistedUsersPresent());
}

void SigninScreenHandler::HandleAuthenticateUser(const std::string& username,
                                                 const std::string& password) {
  if (!delegate_)
    return;
  UserContext user_context(gaia::SanitizeEmail(username));
  user_context.SetKey(Key(password));
  delegate_->Login(user_context, SigninSpecifics());
}

void SigninScreenHandler::HandleLaunchIncognito() {
  UserContext context(user_manager::USER_TYPE_GUEST, std::string());
  if (delegate_)
    delegate_->Login(context, SigninSpecifics());
}

void SigninScreenHandler::HandleShowSupervisedUserCreationScreen() {
  if (!user_manager::UserManager::Get()->AreSupervisedUsersAllowed()) {
    LOG(ERROR) << "Managed users not allowed.";
    return;
  }
  LoginDisplayHostImpl::default_host()->
      StartWizard(WizardController::kSupervisedUserCreationScreenName);
}

void SigninScreenHandler::HandleLaunchPublicSession(
    const std::string& user_id,
    const std::string& locale,
    const std::string& input_method) {
  if (!delegate_)
    return;

  UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, user_id);
  context.SetPublicSessionLocale(locale),
  context.SetPublicSessionInputMethod(input_method);
  delegate_->Login(context, SigninSpecifics());
}

void SigninScreenHandler::HandleOfflineLogin(const base::ListValue* args) {
  if (!delegate_ || delegate_->IsShowUsers()) {
    NOTREACHED();
    return;
  }
  std::string email;
  args->GetString(0, &email);

  gaia_screen_handler_->PopulateEmail(email);
  // Load auth extension. Parameters are: force reload, do not load extension in
  // background, use offline version.
  gaia_screen_handler_->LoadAuthExtension(true, false, true);
  UpdateUIState(UI_STATE_GAIA_SIGNIN, NULL);
}

void SigninScreenHandler::HandleShutdownSystem() {
  ash::Shell::GetInstance()->lock_state_controller()->RequestShutdown();
}

void SigninScreenHandler::HandleLoadWallpaper(const std::string& email) {
  if (delegate_)
    delegate_->LoadWallpaper(email);
}

void SigninScreenHandler::HandleRebootSystem() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void SigninScreenHandler::HandleRemoveUser(const std::string& email) {
  if (!delegate_)
    return;
  delegate_->RemoveUser(email);
  UpdateAddButtonStatus();
}

void SigninScreenHandler::HandleShowAddUser(const base::ListValue* args) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ui",
                               "ShowLoginWebUI",
                               LoginDisplayHostImpl::kShowLoginWebUIid,
                               "ShowAddUser");
  std::string email;
  // |args| can be null if it's OOBE.
  if (args)
    args->GetString(0, &email);
  gaia_screen_handler_->PopulateEmail(email);
  if (!email.empty())
    SendReauthReason(email);
  OnShowAddUser();
}

void SigninScreenHandler::HandleToggleEnrollmentScreen() {
  if (delegate_)
    delegate_->ShowEnterpriseEnrollmentScreen();
}

void SigninScreenHandler::HandleToggleEnableDebuggingScreen() {
  if (delegate_)
    delegate_->ShowEnableDebuggingScreen();
}

void SigninScreenHandler::HandleToggleKioskEnableScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (delegate_ && !connector->IsEnterpriseManaged() &&
      LoginDisplayHostImpl::default_host()) {
    delegate_->ShowKioskEnableScreen();
  }
}

void SigninScreenHandler::HandleToggleKioskAutolaunchScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (delegate_ && !connector->IsEnterpriseManaged())
    delegate_->ShowKioskAutolaunchScreen();
}

void SigninScreenHandler::LoadUsers(const base::ListValue& users_list,
                                    bool showGuest) {
  CallJS("login.AccountPickerScreen.loadUsers",
         users_list,
         delegate_->IsShowGuest());
}

void SigninScreenHandler::HandleAccountPickerReady() {
  VLOG(0) << "Login WebUI >> AccountPickerReady";

  if (delegate_ && !ScreenLocker::default_screen_locker() &&
      !chromeos::IsMachineHWIDCorrect() &&
      !oobe_ui_) {
    delegate_->ShowWrongHWIDScreen();
    return;
  }

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
    if (core_oobe_actor_)
      core_oobe_actor_->ShowDeviceResetScreen();

    return;
  } else if (prefs->GetBoolean(prefs::kDebuggingFeaturesRequested)) {
    if (core_oobe_actor_)
      core_oobe_actor_->ShowEnableDebuggingScreen();

    return;
  }

  is_account_picker_showing_first_time_ = true;

  if (delegate_)
    delegate_->OnSigninScreenReady();
}

void SigninScreenHandler::HandleWallpaperReady() {
  if (ScreenLocker::default_screen_locker()) {
    ScreenLocker::default_screen_locker()->delegate()->
        OnLockBackgroundDisplayed();
  }
}

void SigninScreenHandler::HandleSignOutUser() {
  if (delegate_)
    delegate_->Signout();
}

void SigninScreenHandler::HandleCreateAccount() {
  if (delegate_)
    delegate_->CreateAccount();
}

void SigninScreenHandler::HandleOpenProxySettings() {
  LoginDisplayHostImpl::default_host()->OpenProxySettings();
}

void SigninScreenHandler::HandleLoginVisible(const std::string& source) {
  VLOG(1) << "Login WebUI >> loginVisible, src: " << source << ", "
          << "webui_visible_: " << webui_visible_;
  if (!webui_visible_) {
    // There might be multiple messages from OOBE UI so send notifications after
    // the first one only.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
    TRACE_EVENT_ASYNC_END0(
        "ui", "ShowLoginWebUI", LoginDisplayHostImpl::kShowLoginWebUIid);
  }
  webui_visible_ = true;
  if (preferences_changed_delayed_)
    OnPreferencesChanged();
}

void SigninScreenHandler::HandleCancelPasswordChangedFlow(
    const std::string& user_id) {
  if (!user_id.empty())
    RecordReauthReason(user_id, ReauthReason::PASSWORD_UPDATE_SKIPPED);
  gaia_screen_handler_->StartClearingCookies(
      base::Bind(&SigninScreenHandler::CancelPasswordChangedFlowInternal,
                 weak_factory_.GetWeakPtr()));
}

void SigninScreenHandler::HandleCancelUserAdding() {
  if (delegate_)
    delegate_->CancelUserAdding();
}

void SigninScreenHandler::HandleMigrateUserData(
    const std::string& old_password) {
  if (delegate_)
    delegate_->MigrateUserData(old_password);
}

void SigninScreenHandler::HandleResyncUserData() {
  if (delegate_)
    delegate_->ResyncUserData();
}

void SigninScreenHandler::HandleLoginUIStateChanged(const std::string& source,
                                                    bool active) {
  VLOG(0) << "Login WebUI >> active: " << active << ", "
            << "source: " << source;

  if (!KioskAppManager::Get()->GetAutoLaunchApp().empty() &&
      KioskAppManager::Get()->IsAutoLaunchRequested()) {
    VLOG(0) << "Showing auto-launch warning";
    // On slow devices, the wallpaper animation is not shown initially, so we
    // must explicitly load the wallpaper. This is also the case for the
    // account-picker and gaia-signin UI states.
    delegate_->LoadSigninWallpaper();
    HandleToggleKioskAutolaunchScreen();
    return;
  }

  if (source == kSourceGaiaSignin) {
    ui_state_ = UI_STATE_GAIA_SIGNIN;
  } else if (source == kSourceAccountPicker) {
    ui_state_ = UI_STATE_ACCOUNT_PICKER;
  } else {
    NOTREACHED();
    return;
  }
}

void SigninScreenHandler::HandleUnlockOnLoginSuccess() {
  DCHECK(user_manager::UserManager::Get()->IsUserLoggedIn());
  if (ScreenLocker::default_screen_locker())
    ScreenLocker::default_screen_locker()->UnlockOnLoginSuccess();
}

void SigninScreenHandler::HandleShowLoadingTimeoutError() {
  UpdateState(NetworkError::ERROR_REASON_LOADING_TIMEOUT);
}

void SigninScreenHandler::HandleUpdateOfflineLogin(bool offline_login_active) {
  offline_login_active_ = offline_login_active;
}

void SigninScreenHandler::HandleFocusPod(const std::string& user_id) {
  SetUserInputMethod(user_id, ime_state_.get());
  WallpaperManager::Get()->SetUserWallpaperDelayed(user_id);
  GetScreenlockBridgeInstance()->SetFocusedUser(user_id);
  if (delegate_)
    delegate_->CheckUserStatus(user_id);
  if (!test_focus_pod_callback_.is_null())
    test_focus_pod_callback_.Run();
}

void SigninScreenHandler::HandleGetPublicSessionKeyboardLayouts(
    const std::string& user_id,
    const std::string& locale) {
  GetKeyboardLayoutsForLocale(
      base::Bind(&SigninScreenHandler::SendPublicSessionKeyboardLayouts,
                 weak_factory_.GetWeakPtr(),
                 user_id,
                 locale),
      locale);
}

void SigninScreenHandler::SendPublicSessionKeyboardLayouts(
    const std::string& user_id,
    const std::string& locale,
    scoped_ptr<base::ListValue> keyboard_layouts) {
  CallJS("login.AccountPickerScreen.setPublicSessionKeyboardLayouts",
         user_id,
         locale,
         *keyboard_layouts);
}

void SigninScreenHandler::HandleLaunchKioskApp(const std::string& app_id,
                                               bool diagnostic_mode) {
  UserContext context(user_manager::USER_TYPE_KIOSK_APP, app_id);
  SigninSpecifics specifics;
  specifics.kiosk_diagnostic_mode = diagnostic_mode;
  if (delegate_)
    delegate_->Login(context, specifics);
}

void SigninScreenHandler::HandleCancelConsumerManagementEnrollment() {
  policy::ConsumerManagementService* consumer_management =
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetConsumerManagementService();
  CHECK(consumer_management);
  consumer_management->SetStage(
      policy::ConsumerManagementStage::EnrollmentCanceled());
  is_enrolling_consumer_management_ = false;
  ShowImpl();
}

void SigninScreenHandler::HandleGetTouchViewState() {
  if (max_mode_delegate_) {
    CallJS("login.AccountPickerScreen.setTouchViewState",
           max_mode_delegate_->IsMaximizeModeEnabled());
  }
}

void SigninScreenHandler::HandleLogRemoveUserWarningShown() {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER_SHOW_WARNING);
}

void SigninScreenHandler::HandleFirstIncorrectPasswordAttempt(
    const std::string& email) {
  // TODO(ginkage): Fix this case once crbug.com/469987 is ready.
  /*
    if (user_manager::UserManager::Get()->FindUsingSAML(email))
      RecordReauthReason(email, ReauthReason::INCORRECT_SAML_PASSWORD_ENTERED);
  */
}

void SigninScreenHandler::HandleMaxIncorrectPasswordAttempts(
    const std::string& email) {
  RecordReauthReason(email, ReauthReason::INCORRECT_PASSWORD_ENTERED);
}

bool SigninScreenHandler::AllWhitelistedUsersPresent() {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return false;
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::UserList& users = user_manager->GetUsers();
  if (!delegate_ || users.size() > kMaxUsers) {
    return false;
  }
  const base::ListValue* whitelist = NULL;
  if (!cros_settings->GetList(kAccountsPrefUsers, &whitelist) || !whitelist)
    return false;
  for (size_t i = 0; i < whitelist->GetSize(); ++i) {
    std::string whitelisted_user;
    // NB: Wildcards in the whitelist are also detected as not present here.
    if (!whitelist->GetString(i, &whitelisted_user) ||
        !user_manager->IsKnownUser(whitelisted_user)) {
      return false;
    }
  }
  return true;
}

void SigninScreenHandler::CancelPasswordChangedFlowInternal() {
  if (delegate_) {
    ShowImpl();
    delegate_->CancelPasswordChangedFlow();
  }
}

OobeUI* SigninScreenHandler::GetOobeUI() const {
  return static_cast<OobeUI*>(web_ui()->GetController());
}

OobeUI::Screen SigninScreenHandler::GetCurrentScreen() const {
  OobeUI::Screen screen = OobeUI::SCREEN_UNKNOWN;
  OobeUI* oobe_ui = GetOobeUI();
  if (oobe_ui)
    screen = oobe_ui->current_screen();
  return screen;
}

bool SigninScreenHandler::IsGaiaVisible() const {
  return IsSigninScreen(GetCurrentScreen()) &&
      ui_state_ == UI_STATE_GAIA_SIGNIN;
}

bool SigninScreenHandler::IsGaiaHiddenByError() const {
  return IsSigninScreenHiddenByError() &&
      ui_state_ == UI_STATE_GAIA_SIGNIN;
}

bool SigninScreenHandler::IsSigninScreenHiddenByError() const {
  return (GetCurrentScreen() == OobeUI::SCREEN_ERROR_MESSAGE) &&
         (IsSigninScreen(network_error_model_->GetParentScreen()));
}

bool SigninScreenHandler::IsGuestSigninAllowed() const {
  CrosSettings* cros_settings = CrosSettings::Get();
  if (!cros_settings)
    return false;
  bool allow_guest;
  cros_settings->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  return allow_guest;
}

bool SigninScreenHandler::IsOfflineLoginAllowed() const {
  CrosSettings* cros_settings = CrosSettings::Get();
  if (!cros_settings)
    return false;

  // Offline login is allowed only when user pods are hidden.
  bool show_pods;
  cros_settings->GetBoolean(kAccountsPrefShowUserNamesOnSignIn, &show_pods);
  return !show_pods;
}

void SigninScreenHandler::OnShowAddUser() {
  is_account_picker_showing_first_time_ = false;
  DCHECK(gaia_screen_handler_);
  gaia_screen_handler_->ShowGaiaAsync(is_enrolling_consumer_management_);
}

net::Error SigninScreenHandler::FrameError() const {
  DCHECK(gaia_screen_handler_);
  return gaia_screen_handler_->frame_error();
}

void SigninScreenHandler::OnCapsLockChanged(bool enabled) {
  caps_lock_enabled_ = enabled;
  if (page_is_ready())
    CallJS("login.AccountPickerScreen.setCapsLockState", caps_lock_enabled_);
}

}  // namespace chromeos
