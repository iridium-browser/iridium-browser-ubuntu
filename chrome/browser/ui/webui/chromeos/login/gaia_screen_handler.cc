// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chromeos/chromeos_utils.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/signin/inline_login_ui.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/version_loader.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const char kJsScreenPath[] = "login.GaiaSigninScreen";
const char kAuthIframeParentName[] = "signin-frame";
const char kAuthIframeParentOrigin[] =
    "chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/";

const char kGaiaSandboxUrlSwitch[] = "gaia-sandbox-url";
const char kEndpointGen[] = "1.0";

void UpdateAuthParams(base::DictionaryValue* params,
                      bool has_users,
                      bool is_enrolling_consumer_management) {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = true;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  bool allow_guest = true;
  cros_settings->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  // Account creation depends on Guest sign-in (http://crosbug.com/24570).
  params->SetBoolean("createAccount", allow_new_user && allow_guest);
  params->SetBoolean("guestSignin", allow_guest);

  // Allow supervised user creation only if:
  // 1. Enterprise managed device > is allowed by policy.
  // 2. Consumer device > owner exists.
  // 3. New users are allowed by owner.
  // 4. Supervised users are allowed by owner.
  bool supervised_users_allowed =
      user_manager::UserManager::Get()->AreSupervisedUsersAllowed();
  bool supervised_users_can_create = true;
  int message_id = -1;
  if (!has_users) {
    supervised_users_can_create = false;
    message_id = IDS_CREATE_SUPERVISED_USER_NO_MANAGER_TEXT;
  }
  if (!allow_new_user || !supervised_users_allowed) {
    supervised_users_can_create = false;
    message_id = IDS_CREATE_SUPERVISED_USER_CREATION_RESTRICTED_TEXT;
  }
  if (supervised_users_can_create &&
      ChromeUserManager::Get()
          ->GetUsersAllowedForSupervisedUsersCreation()
          .empty()) {
    supervised_users_can_create = false;
    message_id = IDS_CREATE_SUPERVISED_USER_NO_MANAGER_EXCEPT_KIDS_TEXT;
  }

  params->SetBoolean("supervisedUsersEnabled", supervised_users_allowed);
  params->SetBoolean("supervisedUsersCanCreate", supervised_users_can_create);
  if (!supervised_users_can_create) {
    params->SetString("supervisedUsersRestrictionReason",
                      l10n_util::GetStringUTF16(message_id));
  }

  // Now check whether we're in multi-profiles user adding scenario and
  // disable GAIA right panel features if that's the case.
  // For consumer management enrollment, we also hide all right panel components
  // and show only an enrollment message.
  if (UserAddingScreen::Get()->IsRunning() ||
      is_enrolling_consumer_management) {
    params->SetBoolean("createAccount", false);
    params->SetBoolean("guestSignin", false);
    params->SetBoolean("supervisedUsersEnabled", false);
  }
}

void RecordSAMLScrapingVerificationResultInHistogram(bool success) {
  UMA_HISTOGRAM_BOOLEAN("ChromeOS.SAML.Scraping.VerificationResult", success);
}

void RecordGAIAFlowTypeHistogram() {
  UMA_HISTOGRAM_BOOLEAN("ChromeOS.GAIA.WebViewFlow",
                        StartupUtils::IsWebviewSigninEnabled());
}

// The Task posted to PostTaskAndReply in StartClearingDnsCache on the IO
// thread.
void ClearDnsCache(IOThread* io_thread) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (browser_shutdown::IsTryingToQuit())
    return;

  io_thread->ClearHostCache();
}

void PushFrontIMIfNotExists(const std::string& input_method,
                            std::vector<std::string>* input_methods) {
  if (input_method.empty())
    return;

  if (std::find(input_methods->begin(), input_methods->end(), input_method) ==
      input_methods->end())
    input_methods->insert(input_methods->begin(), input_method);
}

}  // namespace

GaiaContext::GaiaContext()
    : force_reload(false),
      is_local(false),
      password_changed(false),
      show_users(false),
      use_offline(false),
      has_users(false) {
}

GaiaScreenHandler::GaiaScreenHandler(
    CoreOobeActor* core_oobe_actor,
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    policy::ConsumerManagementService* consumer_management)
    : BaseScreenHandler(kJsScreenPath),
      frame_state_(FRAME_STATE_UNKNOWN),
      frame_error_(net::OK),
      network_state_informer_(network_state_informer),
      consumer_management_(consumer_management),
      core_oobe_actor_(core_oobe_actor),
      dns_cleared_(false),
      dns_clear_task_running_(false),
      cookies_cleared_(false),
      show_when_dns_and_cookies_cleared_(false),
      focus_stolen_(false),
      gaia_silent_load_(false),
      using_saml_api_(false),
      is_enrolling_consumer_management_(false),
      test_expects_complete_login_(false),
      use_easy_bootstrap_(false),
      signin_screen_handler_(NULL),
      weak_factory_(this) {
  DCHECK(network_state_informer_.get());
}

GaiaScreenHandler::~GaiaScreenHandler() {
}

void GaiaScreenHandler::LoadGaia(const GaiaContext& context) {
  if (StartupUtils::IsWebviewSigninEnabled()) {
    base::PostTaskAndReplyWithResult(
        content::BrowserThread::GetBlockingPool(), FROM_HERE,
        base::Bind(&version_loader::GetVersion, version_loader::VERSION_SHORT),
        base::Bind(&GaiaScreenHandler::LoadGaiaWithVersion,
                   weak_factory_.GetWeakPtr(), context));
  } else {
    LoadGaiaWithVersion(context, "");
  }
}

void GaiaScreenHandler::LoadGaiaWithVersion(
    const GaiaContext& context,
    const std::string& platform_version) {
  if (!auth_extension_) {
    Profile* signin_profile = ProfileHelper::GetSigninProfile();
    auth_extension_.reset(new ScopedGaiaAuthExtension(signin_profile));
  }

  base::DictionaryValue params;
  const bool is_enrolling_consumer_management =
      context.is_enrolling_consumer_management;

  params.SetBoolean("forceReload", context.force_reload);
  params.SetBoolean("isLocal", context.is_local);
  params.SetBoolean("passwordChanged", context.password_changed);
  params.SetBoolean("isShowUsers", context.show_users);
  params.SetBoolean("useOffline", context.use_offline);
  params.SetString("gaiaId", context.gaia_id);
  params.SetString("email", context.email);
  params.SetBoolean("isEnrollingConsumerManagement",
                    is_enrolling_consumer_management);

  UpdateAuthParams(&params,
                   context.has_users,
                   is_enrolling_consumer_management);

  if (!context.use_offline) {
    const std::string app_locale = g_browser_process->GetApplicationLocale();
    if (!app_locale.empty())
      params.SetString("hl", app_locale);
  } else {
    base::DictionaryValue* localized_strings = new base::DictionaryValue();
    if (StartupUtils::IsWebviewSigninEnabled()) {
      policy::BrowserPolicyConnectorChromeOS* connector =
          g_browser_process->platform_part()
              ->browser_policy_connector_chromeos();
      std::string enterprise_domain(connector->GetEnterpriseDomain());
      if (!enterprise_domain.empty()) {
        localized_strings->SetString(
            "stringEnterpriseInfo",
            l10n_util::GetStringFUTF16(
                IDS_NEWGAIA_OFFLINE_DEVICE_MANAGED_BY_NOTICE,
                base::UTF8ToUTF16(enterprise_domain)));
      }
    } else {
      localized_strings->SetString(
          "stringEmail", l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_EMAIL));
      localized_strings->SetString(
          "stringPassword",
          l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_PASSWORD));
      localized_strings->SetString(
          "stringSignIn", l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_SIGNIN));
      localized_strings->SetString(
          "stringEmptyEmail",
          l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_EMPTY_EMAIL));
      localized_strings->SetString(
          "stringEmptyPassword",
          l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_EMPTY_PASSWORD));
      localized_strings->SetString(
          "stringError", l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_ERROR));
    }
    params.Set("localizedStrings", localized_strings);
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (StartupUtils::IsWebviewSigninEnabled()) {
    params.SetBoolean("useNewGaiaFlow", true);

    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    std::string enterprise_domain(connector->GetEnterpriseDomain());
    if (!enterprise_domain.empty())
      params.SetString("enterpriseDomain", enterprise_domain);

    chrome::VersionInfo version_info;
    params.SetString("chromeType", GetChromeDeviceTypeString());
    params.SetString("clientId",
                     GaiaUrls::GetInstance()->oauth2_chrome_client_id());
    params.SetString("clientVersion", version_info.Version());
    if (!platform_version.empty())
      params.SetString("platformVersion", platform_version);
    params.SetString("releaseChannel", chrome::VersionInfo::GetChannelString());
    params.SetString("endpointGen", kEndpointGen);

    std::string email_domain;
    if (CrosSettings::Get()->GetString(
            kAccountsPrefLoginScreenDomainAutoComplete, &email_domain) &&
        !email_domain.empty()) {
      params.SetString("emailDomain", email_domain);
    }
  } else {
    params.SetBoolean("useNewGaiaFlow", false);
  }

  if (!command_line->HasSwitch(::switches::kGaiaUrl) &&
      command_line->HasSwitch(kGaiaSandboxUrlSwitch) &&
      StartupUtils::IsWebviewSigninEnabled()) {
    // We can't use switch --gaia-url in this case cause we need get
    // auth_code from staging gaia and make all the other auths against prod
    // gaia so user could use all the google services.
    // Default to production Gaia for MM unless --gaia-url or --gaia-sandbox-url
    // is specified.
    // TODO(dpolukhin): crbug.com/462204
    const GURL gaia_url =
        GURL(command_line->GetSwitchValueASCII(kGaiaSandboxUrlSwitch));
    params.SetString("gaiaUrl", gaia_url.spec());
  } else {
    const GURL gaia_url =
        command_line->HasSwitch(::switches::kGaiaUrl)
            ? GURL(command_line->GetSwitchValueASCII(::switches::kGaiaUrl))
            : GaiaUrls::GetInstance()->gaia_url();
    params.SetString("gaiaUrl", gaia_url.spec());
  }

  if (use_easy_bootstrap_) {
    params.SetBoolean("useEafe", true);
    // Easy login overrides.
    std::string eafe_url = "https://easylogin.corp.google.com/";
    if (command_line->HasSwitch(switches::kEafeUrl))
      eafe_url = command_line->GetSwitchValueASCII(switches::kEafeUrl);
    std::string eafe_path = "planters/cbaudioChrome";
    if (command_line->HasSwitch(switches::kEafePath))
      eafe_path = command_line->GetSwitchValueASCII(switches::kEafePath);

    params.SetString("gaiaUrl", eafe_url);
    params.SetString("gaiaPath", eafe_path);
    params.SetString("clientId",
                     GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  }

  frame_state_ = FRAME_STATE_LOADING;
  CallJS("loadAuthExtension", params);
}

void GaiaScreenHandler::UpdateGaia(const GaiaContext& context) {
  base::DictionaryValue params;
  UpdateAuthParams(&params, context.has_users,
                   context.is_enrolling_consumer_management);
  CallJS("updateAuthExtension", params);
}

void GaiaScreenHandler::ReloadGaia(bool force_reload) {
  if (frame_state_ == FRAME_STATE_LOADING && !force_reload) {
    VLOG(1) << "Skipping reloading of Gaia since gaia is loading.";
    return;
  }
  NetworkStateInformer::State state = network_state_informer_->state();
  if (state != NetworkStateInformer::ONLINE) {
    VLOG(1) << "Skipping reloading of Gaia since network state="
            << NetworkStateInformer::StatusString(state);
    return;
  }
  VLOG(1) << "Reloading Gaia.";
  frame_state_ = FRAME_STATE_LOADING;
  CallJS("doReload");
}

void GaiaScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("signinScreenTitle", IDS_SIGNIN_SCREEN_TITLE_TAB_PROMPT);
  builder->Add("signinScreenPasswordChanged",
               IDS_SIGNIN_SCREEN_PASSWORD_CHANGED);
  builder->Add("createAccount", IDS_CREATE_ACCOUNT_HTML);
  builder->Add("guestSignin", IDS_BROWSE_WITHOUT_SIGNING_IN_HTML);
  builder->Add("createSupervisedUser",
               IDS_CREATE_SUPERVISED_USER_HTML);
  builder->Add("createSupervisedUserFeatureName",
               IDS_CREATE_SUPERVISED_USER_FEATURE_NAME);
  builder->Add("consumerManagementEnrollmentSigninMessage",
               IDS_LOGIN_CONSUMER_MANAGEMENT_ENROLLMENT);
  builder->Add("backButton", IDS_ACCNAME_BACK);
  builder->Add("closeButton", IDS_CLOSE);
  builder->Add("whitelistErrorConsumer", IDS_LOGIN_ERROR_WHITELIST);
  builder->Add("whitelistErrorEnterprise",
               IDS_ENTERPRISE_LOGIN_ERROR_WHITELIST);
  builder->Add("tryAgainButton", IDS_WHITELIST_ERROR_TRY_AGAIN_BUTTON);
  builder->Add("learnMoreButton", IDS_WHITELIST_ERROR_LEARN_MORE_BUTTON);
  builder->Add("gaiaLoadingNewGaia", IDS_LOGIN_GAIA_LOADING_MESSAGE);

  // Strings used by the SAML fatal error dialog.
  builder->Add("fatalErrorMessageNoAccountDetails",
               IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS);
  builder->Add("fatalErrorMessageNoPassword",
               IDS_LOGIN_FATAL_ERROR_NO_PASSWORD);
  builder->Add("fatalErrorMessageVerificationFailed",
               IDS_LOGIN_FATAL_ERROR_PASSWORD_VERIFICATION);
  builder->Add("fatalErrorMessageInsecureURL",
               IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL);
  builder->Add("fatalErrorInstructions", IDS_LOGIN_FATAL_ERROR_INSTRUCTIONS);
  builder->Add("fatalErrorDismissButton", IDS_OK);

  builder->AddF("offlineLoginWelcome", IDS_NEWGAIA_OFFLINE_WELCOME,
                GetChromeDeviceType());
  builder->Add("offlineLoginEmail", IDS_NEWGAIA_OFFLINE_EMAIL);
  builder->Add("offlineLoginPassword", IDS_NEWGAIA_OFFLINE_PASSWORD);
  builder->Add("offlineLoginInvalidEmail", IDS_NEWGAIA_OFFLINE_INVALID_EMAIL);
  builder->Add("offlineLoginInvalidPassword",
               IDS_NEWGAIA_OFFLINE_INVALID_PASSWORD);
  builder->Add("offlineLoginNextBtn", IDS_NEWGAIA_OFFLINE_NEXT_BUTTON_TEXT);
  builder->Add("offlineLoginForgotPasswordBtn",
               IDS_NEWGAIA_OFFLINE_FORGOT_PASSWORD_BUTTON_TEXT);
  builder->Add("offlineLoginForgotPasswordDlg",
               IDS_NEWGAIA_OFFLINE_FORGOT_PASSWORD_DIALOG_TEXT);
  builder->Add("offlineLoginCloseBtn", IDS_NEWGAIA_OFFLINE_CLOSE_BUTTON_TEXT);
}

void GaiaScreenHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
  dict->SetBoolean("isWebviewSignin", StartupUtils::IsWebviewSigninEnabled());
}

void GaiaScreenHandler::Initialize() {
}

void GaiaScreenHandler::RegisterMessages() {
  AddCallback("frameLoadingCompleted",
              &GaiaScreenHandler::HandleFrameLoadingCompleted);
  AddCallback("webviewLoadAborted",
              &GaiaScreenHandler::HandleWebviewLoadAborted);
  AddCallback("completeLogin", &GaiaScreenHandler::HandleCompleteLogin);
  AddCallback("completeAuthentication",
              &GaiaScreenHandler::HandleCompleteAuthentication);
  AddCallback("completeAuthenticationAuthCodeOnly",
              &GaiaScreenHandler::HandleCompleteAuthenticationAuthCodeOnly);
  AddCallback("usingSAMLAPI", &GaiaScreenHandler::HandleUsingSAMLAPI);
  AddCallback("scrapedPasswordCount",
              &GaiaScreenHandler::HandleScrapedPasswordCount);
  AddCallback("scrapedPasswordVerificationFailed",
              &GaiaScreenHandler::HandleScrapedPasswordVerificationFailed);
  AddCallback("loginWebuiReady", &GaiaScreenHandler::HandleGaiaUIReady);
  AddCallback("toggleWebviewSignin",
              &GaiaScreenHandler::HandleToggleWebviewSignin);
  AddCallback("toggleEasyBootstrap",
              &GaiaScreenHandler::HandleToggleEasyBootstrap);
}

void GaiaScreenHandler::HandleFrameLoadingCompleted(int status) {
  const net::Error frame_error = static_cast<net::Error>(-status);
  if (frame_error == net::ERR_ABORTED) {
    LOG(WARNING) << "Ignoring Gaia frame error: " << frame_error;
    return;
  }
  frame_error_ = frame_error;
  if (frame_error == net::OK) {
    VLOG(1) << "Gaia is loaded";
    frame_state_ = FRAME_STATE_LOADED;
  } else {
    LOG(WARNING) << "Gaia frame error: " << frame_error_;
    frame_state_ = FRAME_STATE_ERROR;
  }

  if (network_state_informer_->state() != NetworkStateInformer::ONLINE)
    return;
  if (frame_state_ == FRAME_STATE_LOADED)
    UpdateState(NetworkError::ERROR_REASON_UPDATE);
  else if (frame_state_ == FRAME_STATE_ERROR)
    UpdateState(NetworkError::ERROR_REASON_FRAME_ERROR);
}

void GaiaScreenHandler::HandleWebviewLoadAborted(
    const std::string& error_reason_str) {
  // TODO(nkostylev): Switch to int code once webview supports that.
  // http://crbug.com/470483
  if (error_reason_str == "ERR_ABORTED") {
    LOG(WARNING) << "Ignoring Gaia webview error: " << error_reason_str;
    return;
  }

  // TODO(nkostylev): Switch to int code once webview supports that.
  // http://crbug.com/470483
  // Extract some common codes used by SigninScreenHandler for now.
  if (error_reason_str == "ERR_NAME_NOT_RESOLVED")
    frame_error_ = net::ERR_NAME_NOT_RESOLVED;
  else if (error_reason_str == "ERR_INTERNET_DISCONNECTED")
    frame_error_ = net::ERR_INTERNET_DISCONNECTED;
  else if (error_reason_str == "ERR_NETWORK_CHANGED")
    frame_error_ = net::ERR_NETWORK_CHANGED;
  else if (error_reason_str == "ERR_INTERNET_DISCONNECTED")
    frame_error_ = net::ERR_INTERNET_DISCONNECTED;
  else if (error_reason_str == "ERR_PROXY_CONNECTION_FAILED")
    frame_error_ = net::ERR_PROXY_CONNECTION_FAILED;
  else if (error_reason_str == "ERR_TUNNEL_CONNECTION_FAILED")
    frame_error_ = net::ERR_TUNNEL_CONNECTION_FAILED;
  else
    frame_error_ = net::ERR_INTERNET_DISCONNECTED;

  LOG(ERROR) << "Gaia webview error: " << error_reason_str;
  NetworkError::ErrorReason error_reason =
      NetworkError::ERROR_REASON_FRAME_ERROR;
  frame_state_ = FRAME_STATE_ERROR;
  UpdateState(error_reason);
}

void GaiaScreenHandler::HandleCompleteAuthentication(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& password,
    const std::string& auth_code,
    bool using_saml) {
  if (!Delegate())
    return;

  RecordGAIAFlowTypeHistogram();

  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());
  const std::string sanitized_email = gaia::SanitizeEmail(email);
  Delegate()->SetDisplayEmail(sanitized_email);
  UserContext user_context(sanitized_email);
  user_context.SetGaiaID(gaia_id);
  user_context.SetKey(Key(password));
  user_context.SetAuthCode(auth_code);
  user_context.SetAuthFlow(using_saml
                               ? UserContext::AUTH_FLOW_GAIA_WITH_SAML
                               : UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  Delegate()->CompleteLogin(user_context);
}

void GaiaScreenHandler::HandleCompleteAuthenticationAuthCodeOnly(
    const std::string& auth_code) {
  if (!Delegate())
    return;

  RecordGAIAFlowTypeHistogram();

  UserContext user_context;
  user_context.SetAuthFlow(UserContext::AUTH_FLOW_EASY_BOOTSTRAP);
  user_context.SetAuthCode(auth_code);
  Delegate()->CompleteLogin(user_context);
}

void GaiaScreenHandler::HandleCompleteLogin(const std::string& gaia_id,
                                            const std::string& typed_email,
                                            const std::string& password,
                                            bool using_saml) {
  if (!is_enrolling_consumer_management_) {
    DoCompleteLogin(gaia_id, typed_email, password, using_saml);
    return;
  }

  // Consumer management enrollment is in progress.
  const std::string owner_email =
      user_manager::UserManager::Get()->GetOwnerEmail();
  if (typed_email != owner_email) {
    // Show Gaia sign-in screen again, since we only allow the owner to sign
    // in.
    populated_email_ = owner_email;
    ShowGaiaAsync(is_enrolling_consumer_management_);
    return;
  }

  CHECK(consumer_management_);
  consumer_management_->SetOwner(owner_email,
                                 base::Bind(&GaiaScreenHandler::OnSetOwnerDone,
                                            weak_factory_.GetWeakPtr(),
                                            gaia_id,
                                            typed_email,
                                            password,
                                            using_saml));
}

void GaiaScreenHandler::HandleUsingSAMLAPI() {
  SetSAMLPrincipalsAPIUsed(true);
}

void GaiaScreenHandler::HandleScrapedPasswordCount(int password_count) {
  SetSAMLPrincipalsAPIUsed(false);
  // Use a histogram that has 11 buckets, one for each of the values in [0, 9]
  // and an overflow bucket at the end.
  UMA_HISTOGRAM_ENUMERATION(
      "ChromeOS.SAML.Scraping.PasswordCount", std::min(password_count, 10), 11);
  if (password_count == 0)
    HandleScrapedPasswordVerificationFailed();
}

void GaiaScreenHandler::HandleScrapedPasswordVerificationFailed() {
  RecordSAMLScrapingVerificationResultInHistogram(false);
}

void GaiaScreenHandler::HandleToggleWebviewSignin() {
  if (StartupUtils::EnableWebviewSignin(
        !StartupUtils::IsWebviewSigninEnabled())) {
    chrome::AttemptRestart();
  }
}

void GaiaScreenHandler::HandleToggleEasyBootstrap() {
  use_easy_bootstrap_ = !use_easy_bootstrap_;
  const bool kForceReload = true;
  const bool kSilentLoad = true;
  const bool kNoOfflineUI = false;
  LoadAuthExtension(kForceReload, kSilentLoad, kNoOfflineUI);
}

void GaiaScreenHandler::HandleGaiaUIReady() {
  if (focus_stolen_) {
    // Set focus to the Gaia page.
    // TODO(altimofeev): temporary solution, until focus parameters are
    // implemented on the Gaia side.
    // Do this only once. Any subsequent call would relod GAIA frame.
    focus_stolen_ = false;
    const char code[] =
        "if (typeof gWindowOnLoad != 'undefined') gWindowOnLoad();";
    content::RenderFrameHost* frame = InlineLoginUI::GetAuthFrame(
        web_ui()->GetWebContents(),
        GURL(kAuthIframeParentOrigin),
        kAuthIframeParentName);
    frame->ExecuteJavaScript(base::ASCIIToUTF16(code));
  }
  if (gaia_silent_load_) {
    focus_stolen_ = true;
    // Prevent focus stealing by the Gaia page.
    // TODO(altimofeev): temporary solution, until focus parameters are
    // implemented on the Gaia side.
    const char code[] =
        "var gWindowOnLoad = window.onload; "
        "window.onload=function() {};";
    content::RenderFrameHost* frame = InlineLoginUI::GetAuthFrame(
        web_ui()->GetWebContents(),
        GURL(kAuthIframeParentOrigin),
        kAuthIframeParentName);
    frame->ExecuteJavaScript(base::ASCIIToUTF16(code));

    // As we could miss and window.onload could already be called, restore
    // focus to current pod (see crbug/175243).
    DCHECK(signin_screen_handler_);
    signin_screen_handler_->RefocusCurrentPod();
  }
  HandleFrameLoadingCompleted(0);

  if (test_expects_complete_login_)
    SubmitLoginFormForTest();
}

void GaiaScreenHandler::OnSetOwnerDone(const std::string& gaia_id,
                                       const std::string& typed_email,
                                       const std::string& password,
                                       bool using_saml,
                                       bool success) {
  CHECK(consumer_management_);
  if (success) {
    consumer_management_->SetStage(
        policy::ConsumerManagementStage::EnrollmentOwnerStored());
  } else {
    LOG(ERROR) << "Failed to write owner e-mail to boot lockbox.";
    consumer_management_->SetStage(
        policy::ConsumerManagementStage::EnrollmentBootLockboxFailed());
    // We should continue logging in the user, as there's not much we can do
    // here.
  }
  DoCompleteLogin(gaia_id, typed_email, password, using_saml);
}

void GaiaScreenHandler::DoCompleteLogin(const std::string& gaia_id,
                                        const std::string& typed_email,
                                        const std::string& password,
                                        bool using_saml) {
  if (!Delegate())
    return;

  if (using_saml && !using_saml_api_)
    RecordSAMLScrapingVerificationResultInHistogram(true);
  RecordGAIAFlowTypeHistogram();

  DCHECK(!typed_email.empty());
  DCHECK(!gaia_id.empty());
  const std::string sanitized_email = gaia::SanitizeEmail(typed_email);
  Delegate()->SetDisplayEmail(sanitized_email);
  UserContext user_context(sanitized_email);
  user_context.SetGaiaID(gaia_id);
  user_context.SetKey(Key(password));
  user_context.SetAuthFlow(using_saml
                               ? UserContext::AUTH_FLOW_GAIA_WITH_SAML
                               : UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML);
  Delegate()->CompleteLogin(user_context);

  if (test_expects_complete_login_) {
    VLOG(2) << "Complete test login for " << typed_email
            << ", requested=" << test_user_;

    test_expects_complete_login_ = false;
    test_user_.clear();
    test_pass_.clear();
  }
}

void GaiaScreenHandler::PopulateEmail(const std::string& user_id) {
  populated_email_ = user_id;
}

void GaiaScreenHandler::PasswordChangedFor(const std::string& user_id) {
  password_changed_for_.insert(user_id);
}

void GaiaScreenHandler::StartClearingDnsCache() {
  if (dns_clear_task_running_ || !g_browser_process->io_thread())
    return;

  dns_cleared_ = false;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ClearDnsCache, g_browser_process->io_thread()),
      base::Bind(&GaiaScreenHandler::OnDnsCleared, weak_factory_.GetWeakPtr()));
  dns_clear_task_running_ = true;
}

void GaiaScreenHandler::OnDnsCleared() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  dns_clear_task_running_ = false;
  dns_cleared_ = true;
  ShowGaiaScreenIfReady();
}

void GaiaScreenHandler::StartClearingCookies(
    const base::Closure& on_clear_callback) {
  cookies_cleared_ = false;
  ProfileHelper* profile_helper = ProfileHelper::Get();
  LOG_ASSERT(Profile::FromWebUI(web_ui()) ==
             profile_helper->GetSigninProfile());
  profile_helper->ClearSigninProfile(
      base::Bind(&GaiaScreenHandler::OnCookiesCleared,
                 weak_factory_.GetWeakPtr(), on_clear_callback));
}

void GaiaScreenHandler::OnCookiesCleared(
    const base::Closure& on_clear_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cookies_cleared_ = true;
  on_clear_callback.Run();
}

void GaiaScreenHandler::ShowSigninScreenForCreds(const std::string& username,
                                                 const std::string& password) {
  VLOG(2) << "ShowSigninScreenForCreds  for user " << username
          << ", frame_state=" << frame_state();

  test_user_ = username;
  test_pass_ = password;
  test_expects_complete_login_ = true;

  // Submit login form for test if gaia is ready. If gaia is loading, login
  // will be attempted in HandleLoginWebuiReady after gaia is ready. Otherwise,
  // reload gaia then follow the loading case.
  if (frame_state() == GaiaScreenHandler::FRAME_STATE_LOADED) {
    SubmitLoginFormForTest();
  } else if (frame_state() != GaiaScreenHandler::FRAME_STATE_LOADING) {
    DCHECK(signin_screen_handler_);
    signin_screen_handler_->OnShowAddUser();
  }
}

void GaiaScreenHandler::SubmitLoginFormForTest() {
  VLOG(2) << "Submit login form for test, user=" << test_user_;

  content::RenderFrameHost* frame = InlineLoginUI::GetAuthFrame(
      web_ui()->GetWebContents(),
      GURL(kAuthIframeParentOrigin),
      kAuthIframeParentName);

  if (!StartupUtils::IsWebviewSigninEnabled()) {
    std::string code;
    code += "document.getElementById('Email').value = '" + test_user_ + "';";
    code += "document.getElementById('Passwd').value = '" + test_pass_ + "';";
    code += "document.getElementById('signIn').click();";

    frame->ExecuteJavaScript(base::ASCIIToUTF16(code));
  } else {
    std::string code;

    code =
        "document.getElementById('identifier').value = '" + test_user_ + "';";
    code += "document.getElementById('nextButton').click();";
    frame->ExecuteJavaScript(base::ASCIIToUTF16(code));

    if (!test_pass_.empty()) {
      code =
          "document.getElementById('password').value = '" + test_pass_ + "';";
      code += "document.getElementById('nextButton').click();";
      frame->ExecuteJavaScript(base::ASCIIToUTF16(code));
    }
  }

  // Test properties are cleared in HandleCompleteLogin because the form
  // submission might fail and login will not be attempted after reloading
  // if they are cleared here.
}

void GaiaScreenHandler::SetSAMLPrincipalsAPIUsed(bool api_used) {
  using_saml_api_ = api_used;
  UMA_HISTOGRAM_BOOLEAN("ChromeOS.SAML.APIUsed", api_used);
}

void GaiaScreenHandler::ShowGaiaAsync(bool is_enrolling_consumer_management) {
  is_enrolling_consumer_management_ = is_enrolling_consumer_management;
  show_when_dns_and_cookies_cleared_ = true;
  if (gaia_silent_load_ && populated_email_.empty()) {
    dns_cleared_ = true;
    cookies_cleared_ = true;
    ShowGaiaScreenIfReady();
  } else {
    StartClearingDnsCache();
    StartClearingCookies(base::Bind(&GaiaScreenHandler::ShowGaiaScreenIfReady,
                                    weak_factory_.GetWeakPtr()));
  }
}

void GaiaScreenHandler::CancelShowGaiaAsync() {
  show_when_dns_and_cookies_cleared_ = false;
}

void GaiaScreenHandler::ShowGaiaScreenIfReady() {
  if (!dns_cleared_ ||
      !cookies_cleared_ ||
      !show_when_dns_and_cookies_cleared_ ||
      !Delegate()) {
    return;
  }

  std::string active_network_path = network_state_informer_->network_path();
  if (gaia_silent_load_ &&
      (network_state_informer_->state() != NetworkStateInformer::ONLINE ||
       gaia_silent_load_network_ != active_network_path)) {
    // Network has changed. Force Gaia reload.
    gaia_silent_load_ = false;
    // Gaia page will be realoded, so focus isn't stolen anymore.
    focus_stolen_ = false;
  }

  // Note that LoadAuthExtension clears |populated_email_|.
  if (populated_email_.empty())
    Delegate()->LoadSigninWallpaper();
  else
    Delegate()->LoadWallpaper(populated_email_);

  input_method::InputMethodManager* imm =
      input_method::InputMethodManager::Get();

  scoped_refptr<input_method::InputMethodManager::State> gaia_ime_state =
      imm->GetActiveIMEState()->Clone();
  imm->SetState(gaia_ime_state);

  // Set Least Recently Used input method for the user.
  if (!populated_email_.empty()) {
    SigninScreenHandler::SetUserInputMethod(populated_email_,
                                            gaia_ime_state.get());
  } else {
    std::vector<std::string> input_methods =
        imm->GetInputMethodUtil()->GetHardwareLoginInputMethodIds();
    const std::string owner_im = SigninScreenHandler::GetUserLRUInputMethod(
        user_manager::UserManager::Get()->GetOwnerEmail());
    const std::string system_im = g_browser_process->local_state()->GetString(
        language_prefs::kPreferredKeyboardLayout);

    PushFrontIMIfNotExists(owner_im, &input_methods);
    PushFrontIMIfNotExists(system_im, &input_methods);

    gaia_ime_state->EnableLoginLayouts(
        g_browser_process->GetApplicationLocale(), input_methods);

    if (!system_im.empty()) {
      gaia_ime_state->ChangeInputMethod(system_im, false /* show_message */);
    } else if (!owner_im.empty()) {
      gaia_ime_state->ChangeInputMethod(owner_im, false /* show_message */);
    }
  }

  LoadAuthExtension(!gaia_silent_load_, false, false);
  signin_screen_handler_->UpdateUIState(
      SigninScreenHandler::UI_STATE_GAIA_SIGNIN, NULL);

  if (gaia_silent_load_) {
    // The variable is assigned to false because silently loaded Gaia page was
    // used.
    gaia_silent_load_ = false;
    if (focus_stolen_)
      HandleGaiaUIReady();
  }
  signin_screen_handler_->UpdateState(NetworkError::ERROR_REASON_UPDATE);

  if (core_oobe_actor_) {
    PrefService* prefs = g_browser_process->local_state();
    if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
      core_oobe_actor_->ShowDeviceResetScreen();
    } else if (prefs->GetBoolean(prefs::kDebuggingFeaturesRequested)) {
      core_oobe_actor_->ShowEnableDebuggingScreen();
    }
  }
}

void GaiaScreenHandler::MaybePreloadAuthExtension() {
  VLOG(1) << "MaybePreloadAuthExtension() call.";

  // If cookies clearing was initiated or |dns_clear_task_running_| then auth
  // extension showing has already been initiated and preloading is senseless.
  if (signin_screen_handler_->ShouldLoadGaia() &&
      !gaia_silent_load_ &&
      !cookies_cleared_ &&
      !dns_clear_task_running_ &&
      network_state_informer_->state() == NetworkStateInformer::ONLINE) {
    gaia_silent_load_ = true;
    gaia_silent_load_network_ = network_state_informer_->network_path();
    LoadAuthExtension(true, true, false);
  }
}

void GaiaScreenHandler::ShowWhitelistCheckFailedError() {
  base::DictionaryValue params;
  params.SetBoolean("enterpriseManaged",
                    g_browser_process->platform_part()
                        ->browser_policy_connector_chromeos()
                        ->IsEnterpriseManaged());
  CallJS("showWhitelistCheckFailedError", true, params);
}

void GaiaScreenHandler::LoadAuthExtension(bool force,
                                          bool silent_load,
                                          bool offline) {
  GaiaContext context;
  context.force_reload = force;
  context.is_local = offline;
  context.password_changed = !populated_email_.empty() &&
                             password_changed_for_.count(populated_email_);
  context.use_offline = offline;
  context.email = populated_email_;
  context.is_enrolling_consumer_management = is_enrolling_consumer_management_;

  std::string gaia_id;
  if (user_manager::UserManager::Get()->FindGaiaID(context.email, &gaia_id))
    context.gaia_id = gaia_id;

  if (Delegate()) {
    context.show_users = Delegate()->IsShowUsers();
    context.has_users = !Delegate()->GetUsers().empty();
  }

  populated_email_.clear();

  LoadGaia(context);
}

void GaiaScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  if (signin_screen_handler_)
    signin_screen_handler_->UpdateState(reason);
}

SigninScreenHandlerDelegate* GaiaScreenHandler::Delegate() {
  DCHECK(signin_screen_handler_);
  return signin_screen_handler_->delegate_;
}

void GaiaScreenHandler::SetSigninScreenHandler(SigninScreenHandler* handler) {
  signin_screen_handler_ = handler;
}

}  // namespace chromeos
