// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"

#include <string>

#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy_indicator_localized_strings_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/devicetype_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#else
#include "chrome/browser/ui/webui/settings/system_handler.h"
#endif

namespace settings {
namespace {

// Note that settings.html contains a <script> tag which imports a script of
// the following name. These names must be kept in sync.
const char kLocalizedStringsFile[] = "strings.js";

struct LocalizedString {
  const char* name;
  int id;
};

void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             LocalizedString localized_strings[],
                             size_t num_strings) {
  for (size_t i = 0; i < num_strings; i++) {
    html_source->AddLocalizedString(localized_strings[i].name,
                                    localized_strings[i].id);
  }
}

void AddCommonStrings(content::WebUIDataSource* html_source, Profile* profile) {
  LocalizedString localized_strings[] = {
      {"add", IDS_ADD},
      {"cancel", IDS_CANCEL},
      {"confirm", IDS_CONFIRM},
      {"disable", IDS_DISABLE},
      {"learnMore", IDS_LEARN_MORE},
      {"ok", IDS_OK},
      {"save", IDS_SAVE},
      {"advancedPageTitle", IDS_SETTINGS_ADVANCED},
      {"basicPageTitle", IDS_SETTINGS_BASIC},
      {"settings", IDS_SETTINGS_SETTINGS},
      {"restart", IDS_SETTINGS_RESTART},
      {"menuButtonLabel", IDS_SETTINGS_MENU_BUTTON_LABEL},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddBoolean(
      "isGuest",
#if defined(OS_CHROMEOS)
      user_manager::UserManager::Get()->IsLoggedInAsGuest());
#else
      profile->IsOffTheRecord());
#endif
}

void AddA11yStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"a11yPageTitle", IDS_SETTINGS_ACCESSIBILITY},
    {"moreFeaturesLink", IDS_SETTINGS_MORE_FEATURES_LINK},
#if defined(OS_CHROMEOS)
    {"optionsInMenuLabel", IDS_SETTINGS_OPTIONS_IN_MENU_LABEL},
    {"largeMouseCursorLabel", IDS_SETTINGS_LARGE_MOUSE_CURSOR_LABEL},
    {"highContrastLabel", IDS_SETTINGS_HIGH_CONTRAST_LABEL},
    {"stickyKeysLabel", IDS_SETTINGS_STICKY_KEYS_LABEL},
    {"chromeVoxLabel", IDS_SETTINGS_CHROMEVOX_LABEL},
    {"screenMagnifierLabel", IDS_SETTINGS_SCREEN_MAGNIFIER_LABEL},
    {"tapDraggingLabel", IDS_SETTINGS_TAP_DRAGGING_LABEL},
    {"clickOnStopLabel", IDS_SETTINGS_CLICK_ON_STOP_LABEL},
    {"delayBeforeClickLabel", IDS_SETTINGS_DELAY_BEFORE_CLICK_LABEL},
    {"delayBeforeClickExtremelyShort",
     IDS_SETTINGS_DELAY_BEFORE_CLICK_EXTREMELY_SHORT},
    {"delayBeforeClickVeryShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_SHORT},
    {"delayBeforeClickShort", IDS_SETTINGS_DELAY_BEFORE_CLICK_SHORT},
    {"delayBeforeClickLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_LONG},
    {"delayBeforeClickVeryLong", IDS_SETTINGS_DELAY_BEFORE_CLICK_VERY_LONG},
    {"onScreenKeyboardLabel", IDS_SETTINGS_ON_SCREEN_KEYBOARD_LABEL},
    {"monoAudioLabel", IDS_SETTINGS_MONO_AUDIO_LABEL},
    {"a11yExplanation", IDS_SETTINGS_ACCESSIBILITY_EXPLANATION},
    {"caretHighlightLabel",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_CARET_HIGHLIGHT_DESCRIPTION},
    {"cursorHighlightLabel",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_CURSOR_HIGHLIGHT_DESCRIPTION},
    {"focusHighlightLabel",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_FOCUS_HIGHLIGHT_DESCRIPTION},
    {"selectToSpeakTitle",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_TITLE},
    {"selectToSpeakDescription",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SELECT_TO_SPEAK_DESCRIPTION},
    {"switchAccessLabel",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SWITCH_ACCESS_DESCRIPTION},
    {"manageAccessibilityFeatures",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_MANAGE_ACCESSIBILITY_FEATURES},
    {"textToSpeechHeading",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_TEXT_TO_SPEECH_HEADING},
    {"displayHeading",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_DISPLAY_HEADING},
    {"displaySettingsTitle",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_TITLE},
    {"displaySettingsDescription",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_DISPLAY_SETTINGS_DESCRIPTION},
    {"appearanceSettingsTitle",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_TITLE},
    {"appearanceSettingsDescription",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_APPEARANCE_SETTINGS_DESCRIPTION},
    {"keyboardHeading",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_KEYBOARD_HEADING},
    {"keyboardSettingsTitle",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_TITLE},
    {"keyboardSettingsDescription",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_KEYBOARD_SETTINGS_DESCRIPTION},
    {"mouseAndTouchpadHeading",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_MOUSE_AND_TOUCHPAD_HEADING},
    {"mouseSettingsTitle",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_TITLE},
    {"mouseSettingsDescription",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_MOUSE_SETTINGS_DESCRIPTION},
    {"audioHeading",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_AUDIO_HEADING},
    {"additionalFeaturesTitle",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_ADDITIONAL_FEATURES_TITLE},
    {"additionalFeaturesDescription",
     IDS_OPTIONS_SETTINGS_ACCESSIBILITY_ADDITIONAL_FEATURES_DESCRIPTION},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

#if defined(OS_CHROMEOS)
  html_source->AddString("a11yLearnMoreUrl",
                         chrome::kChromeAccessibilityHelpURL);

  html_source->AddBoolean(
      "showExperimentalA11yFeatures",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableExperimentalAccessibilityFeatures));
#endif
}

void AddAboutStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"aboutPageTitle", IDS_SETTINGS_ABOUT_PROGRAM},
#if defined(OS_CHROMEOS)
    {"aboutProductTitle", IDS_PRODUCT_OS_NAME},
#else
    {"aboutProductTitle", IDS_PRODUCT_NAME},
#endif
    {"aboutGetHelpUsingChrome", IDS_SETTINGS_GET_HELP_USING_CHROME},

#if defined(GOOGLE_CHROME_BUILD)
    {"aboutReportAnIssue", IDS_SETTINGS_ABOUT_PAGE_REPORT_AN_ISSUE},
#endif

    {"aboutRelaunch", IDS_SETTINGS_ABOUT_PAGE_RELAUNCH},
    {"aboutUpgradeCheckStarted", IDS_SETTINGS_ABOUT_UPGRADE_CHECK_STARTED},
    {"aboutUpgradeRelaunch", IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH},
    {"aboutUpgradeUpdating", IDS_SETTINGS_UPGRADE_UPDATING},

#if defined(OS_CHROMEOS)
    {"aboutArcVersionLabel", IDS_SETTINGS_ABOUT_PAGE_ARC_VERSION},
    {"aboutBuildDateLabel", IDS_VERSION_UI_BUILD_DATE},
    {"aboutChannelBeta", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_BETA},
    {"aboutChannelDev", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_DEV},
    {"aboutChannelLabel", IDS_SETTINGS_ABOUT_PAGE_CHANNEL},
    {"aboutChannelStable", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL_STABLE},
    {"aboutCheckForUpdates", IDS_SETTINGS_ABOUT_PAGE_CHECK_FOR_UPDATES},
    {"aboutCommandLineLabel", IDS_VERSION_UI_COMMAND_LINE},
    {"aboutCurrentlyOnChannel", IDS_SETTINGS_ABOUT_PAGE_CURRENT_CHANNEL},
    {"aboutDetailedBuildInfo", IDS_SETTINGS_ABOUT_PAGE_DETAILED_BUILD_INFO},
    {"aboutFirmwareLabel", IDS_SETTINGS_ABOUT_PAGE_FIRMWARE},
    {"aboutPlatformLabel", IDS_SETTINGS_ABOUT_PAGE_PLATFORM},
    {"aboutRelaunchAndPowerwash",
     IDS_SETTINGS_ABOUT_PAGE_RELAUNCH_AND_POWERWASH},
    {"aboutUpgradeUpdatingChannelSwitch",
     IDS_SETTINGS_UPGRADE_UPDATING_CHANNEL_SWITCH},
    {"aboutUpgradeSuccessChannelSwitch",
     IDS_SETTINGS_UPGRADE_SUCCESSFUL_CHANNEL_SWITCH},
    {"aboutUserAgentLabel", IDS_VERSION_UI_USER_AGENT},

    // About page, channel switcher dialog.
    {"aboutChangeChannel", IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL},
    {"aboutChangeChannelAndPowerwash",
     IDS_SETTINGS_ABOUT_PAGE_CHANGE_CHANNEL_AND_POWERWASH},
    {"aboutDelayedWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_MESSAGE},
    {"aboutDelayedWarningTitle", IDS_SETTINGS_ABOUT_PAGE_DELAYED_WARNING_TITLE},
    {"aboutPowerwashWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_MESSAGE},
    {"aboutPowerwashWarningTitle",
     IDS_SETTINGS_ABOUT_PAGE_POWERWASH_WARNING_TITLE},
    {"aboutUnstableWarningMessage",
     IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_MESSAGE},
    {"aboutUnstableWarningTitle",
     IDS_SETTINGS_ABOUT_PAGE_UNSTABLE_WARNING_TITLE},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddString(
      "aboutUpgradeUpToDate",
#if defined(OS_CHROMEOS)
      ash::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
#else
      l10n_util::GetStringUTF16(IDS_SETTINGS_UPGRADE_UP_TO_DATE));
#endif
}

#if defined(OS_CHROMEOS)
void AddAccountUITweaksStrings(content::WebUIDataSource* html_source,
                               Profile* profile) {
  base::DictionaryValue localized_values;
  chromeos::AddAccountUITweaksLocalizedValues(&localized_values, profile);
  html_source->AddLocalizedStrings(localized_values);
}
#endif

void AddAppearanceStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"appearancePageTitle", IDS_SETTINGS_APPEARANCE},
    {"exampleDotCom", IDS_SETTINGS_EXAMPLE_DOT_COM},
    {"getThemes", IDS_SETTINGS_THEMES},
    {"resetToDefaultTheme", IDS_SETTINGS_RESET_TO_DEFAULT_THEME},
    {"showHomeButton", IDS_SETTINGS_SHOW_HOME_BUTTON},
    {"showBookmarksBar", IDS_SETTINGS_SHOW_BOOKMARKS_BAR},
    {"homePageNtp", IDS_SETTINGS_HOME_PAGE_NTP},
    {"other", IDS_SETTINGS_OTHER},
    {"changeHomePage", IDS_SETTINGS_CHANGE_HOME_PAGE},
    {"themesGalleryUrl", IDS_THEMES_GALLERY_URL},
    {"chooseFromWebStore", IDS_SETTINGS_WEB_STORE},
    {"chooseFontsAndEncoding", IDS_SETTINGS_CHOOSE_FONTS_AND_ENCODING},
#if defined(OS_CHROMEOS)
    {"openWallpaperApp", IDS_SETTINGS_OPEN_WALLPAPER_APP},
    {"setWallpaper", IDS_SETTINGS_SET_WALLPAPER},
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    {"showWindowDecorations", IDS_SHOW_WINDOW_DECORATIONS},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if defined(OS_CHROMEOS)
void AddBluetoothStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"bluetoothAccept", IDS_OPTIONS_SETTINGS_BLUETOOTH_ACCEPT_PASSKEY},
      {"bluetoothAddDevice", IDS_OPTIONS_SETTINGS_ADD_BLUETOOTH_DEVICE},
      {"bluetoothAddDevicePageTitle", IDS_SETTINGS_BLUETOOTH_ADD_DEVICE},
      {"bluetoothConnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_CONNECT},
      {"bluetoothConnecting", IDS_SETTINGS_BLUETOOTH_CONNECTING},
      {"bluetoothDisconnect", IDS_OPTIONS_SETTINGS_BLUETOOTH_DISCONNECT},
      {"bluetoothDismiss", IDS_OPTIONS_SETTINGS_BLUETOOTH_DISMISS_ERROR},
      {"bluetoothEnable", IDS_SETTINGS_BLUETOOTH_ENABLE},
      {"bluetoothNoDevices", IDS_OPTIONS_SETTINGS_BLUETOOTH_NO_DEVICES},
      {"bluetoothPageTitle", IDS_SETTINGS_BLUETOOTH},
      {"bluetoothPairDevicePageTitle", IDS_SETTINGS_BLUETOOTH_PAIR_DEVICE},
      {"bluetoothReject", IDS_OPTIONS_SETTINGS_BLUETOOTH_REJECT_PASSKEY},
      {"bluetoothRemove", IDS_SETTINGS_BLUETOOTH_REMOVE},
      {"bluetoothScanning", IDS_OPTIONS_SETTINGS_BLUETOOTH_SCANNING},
      // Device connecting and pairing.
      {"bluetoothStartConnecting", IDS_SETTINGS_BLUETOOTH_START_CONNECTING},
      {"bluetoothEnterKey", IDS_OPTIONS_SETTINGS_BLUETOOTH_ENTER_KEY},
      // These ids are generated in JS using 'bluetooth_' + a value from
      // bluetoothPrivate.PairingEventType (see bluetooth_private.idl).
      // 'keysEntered', and 'requestAuthorization' have no associated message.
      {"bluetooth_requestPincode", IDS_SETTINGS_BLUETOOTH_REQUEST_PINCODE},
      {"bluetooth_displayPincode", IDS_SETTINGS_BLUETOOTH_DISPLAY_PINCODE},
      {"bluetooth_requestPasskey", IDS_SETTINGS_BLUETOOTH_REQUEST_PASSKEY},
      {"bluetooth_displayPasskey", IDS_SETTINGS_BLUETOOTH_DISPLAY_PASSKEY},
      {"bluetooth_confirmPasskey", IDS_SETTINGS_BLUETOOTH_CONFIRM_PASSKEY},
      // These ids are generated in JS using 'bluetooth_result_' + a value from
      // bluetoothPrivate.ConnectResultType (see bluetooth_private.idl).
      {"bluetooth_connect_attributeLengthInvalid",
       IDS_SETTINGS_BLUETOOTH_CONNECT_ATTRIBUTE_LENGTH_INVALID},
      {"bluetooth_connect_authCanceled",
       IDS_SETTINGS_BLUETOOTH_CONNECT_AUTH_CANCELED},
      {"bluetooth_connect_authFailed",
       IDS_SETTINGS_BLUETOOTH_CONNECT_AUTH_FAILED},
      {"bluetooth_connect_authRejected",
       IDS_SETTINGS_BLUETOOTH_CONNECT_AUTH_REJECTED},
      {"bluetooth_connect_authTimeout",
       IDS_SETTINGS_BLUETOOTH_CONNECT_AUTH_TIMEOUT},
      {"bluetooth_connect_connectionCongested",
       IDS_SETTINGS_BLUETOOTH_CONNECT_CONNECTION_CONGESTED},
      {"bluetooth_connect_failed", IDS_SETTINGS_BLUETOOTH_CONNECT_FAILED},
      {"bluetooth_connect_inProgress",
       IDS_SETTINGS_BLUETOOTH_CONNECT_IN_PROGRESS},
      {"bluetooth_connect_insufficientEncryption",
       IDS_SETTINGS_BLUETOOTH_CONNECT_INSUFFICIENT_ENCRYPTION},
      {"bluetooth_connect_offsetInvalid",
       IDS_SETTINGS_BLUETOOTH_CONNECT_OFFSET_INVALID},
      {"bluetooth_connect_readNotPermitted",
       IDS_SETTINGS_BLUETOOTH_CONNECT_READ_NOT_PERMITTED},
      {"bluetooth_connect_requestNotSupported",
       IDS_SETTINGS_BLUETOOTH_CONNECT_REQUEST_NOT_SUPPORTED},
      {"bluetooth_connect_unsupportedDevice",
       IDS_SETTINGS_BLUETOOTH_CONNECT_UNSUPPORTED_DEVICE},
      {"bluetooth_connect_writeNotPermitted",
       IDS_SETTINGS_BLUETOOTH_CONNECT_WRITE_NOT_PERMITTED},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

#if defined(USE_NSS_CERTS)
void AddCertificateManagerStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"certificateManagerPageTitle", IDS_SETTINGS_CERTIFICATE_MANAGER},
      {"certificateManagerNoCertificates",
       IDS_SETTINGS_CERTIFICATE_MANAGER_NO_CERTIFICATES},
      {"certificateManagerYourCertificates",
       IDS_SETTINGS_CERTIFICATE_MANAGER_YOUR_CERTIFICATES},
      {"certificateManagerYourCertificatesDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_YOUR_CERTIFICATES_DESCRIPTION},
      {"certificateManagerServers", IDS_SETTINGS_CERTIFICATE_MANAGER_SERVERS},
      {"certificateManagerServersDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_SERVERS_DESCRIPTION},
      {"certificateManagerAuthorities",
       IDS_SETTINGS_CERTIFICATE_MANAGER_AUTHORITIES},
      {"certificateManagerAuthoritiesDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_AUTHORITIES_DESCRIPTION},
      {"certificateManagerOthers", IDS_SETTINGS_CERTIFICATE_MANAGER_OTHERS},
      {"certificateManagerOthersDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_OTHERS_DESCRIPTION},
      {"certificateManagerView", IDS_SETTINGS_CERTIFICATE_MANAGER_VIEW},
      {"certificateManagerEdit", IDS_SETTINGS_CERTIFICATE_MANAGER_EDIT},
      {"certificateManagerImport", IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT},
      {"certificateManagerImportAndBind",
       IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_AND_BIND},
      {"certificateManagerExport", IDS_SETTINGS_CERTIFICATE_MANAGER_EXPORT},
      {"certificateManagerDelete", IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE},
      {"certificateManagerDone", IDS_SETTINGS_CERTIFICATE_MANAGER_DONE},
      {"certificateManagerUntrusted",
       IDS_SETTINGS_CERTIFICATE_MANAGER_UNTRUSTED},
      // CA trust edit dialog.
      {"certificateManagerCaTrustEditDialogTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_TITLE},
      {"certificateManagerCaTrustEditDialogDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_DESCRIPTION},
      {"certificateManagerCaTrustEditDialogExplanation",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_EXPLANATION},
      {"certificateManagerCaTrustEditDialogSsl",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_SSL},
      {"certificateManagerCaTrustEditDialogEmail",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_EMAIL},
      {"certificateManagerCaTrustEditDialogObjSign",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CA_TRUST_EDIT_DIALOG_OBJ_SIGN},
      // Certificate delete confirmation dialog.
      {"certificateManagerDeleteUserTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_USER_TITLE},
      {"certificateManagerDeleteUserDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_USER_DESCRIPTION},
      {"certificateManagerDeleteServerTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_SERVER_TITLE},
      {"certificateManagerDeleteServerDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_SERVER_DESCRIPTION},
      {"certificateManagerDeleteCaTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_CA_TITLE},
      {"certificateManagerDeleteCaDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_CA_DESCRIPTION},
      {"certificateManagerDeleteOtherTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_OTHER_TITLE},
      // Encrypt/decrypt password dialogs.
      {"certificateManagerEncryptPasswordTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_ENCRYPT_PASSWORD_TITLE},
      {"certificateManagerDecryptPasswordTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_DECRYPT_PASSWORD_TITLE},
      {"certificateManagerEncryptPasswordDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_ENCRYPT_PASSWORD_DESCRIPTION},
      {"certificateManagerPassword", IDS_SETTINGS_CERTIFICATE_MANAGER_PASSWORD},
      {"certificateManagerConfirmPassword",
       IDS_SETTINGS_CERTIFICATE_MANAGER_CONFIRM_PASSWORD},
      {"certificateImportErrorFormat",
       IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ERROR_FORMAT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

void AddClearBrowsingDataStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"clearFollowingItemsFrom", IDS_SETTINGS_CLEAR_FOLLOWING_ITEMS_FROM},
      {"clearBrowsingHistory", IDS_SETTINGS_CLEAR_BROWSING_HISTORY},
      {"clearDownloadHistory", IDS_SETTINGS_CLEAR_DOWNLOAD_HISTORY},
      {"clearCache", IDS_SETTINGS_CLEAR_CACHE},
      {"clearCookies", IDS_SETTINGS_CLEAR_COOKIES},
      {"clearCookiesCounter", IDS_DEL_COOKIES_COUNTER},
      {"clearCookiesFlash", IDS_SETTINGS_CLEAR_COOKIES_FLASH},
      {"clearPasswords", IDS_SETTINGS_CLEAR_PASSWORDS},
      {"clearFormData", IDS_SETTINGS_CLEAR_FORM_DATA},
      {"clearHostedAppData", IDS_SETTINGS_CLEAR_HOSTED_APP_DATA},
      {"clearMediaLicenses", IDS_SETTINGS_CLEAR_MEDIA_LICENSES},
      {"clearDataHour", IDS_SETTINGS_CLEAR_DATA_HOUR},
      {"clearDataDay", IDS_SETTINGS_CLEAR_DATA_DAY},
      {"clearDataWeek", IDS_SETTINGS_CLEAR_DATA_WEEK},
      {"clearData4Weeks", IDS_SETTINGS_CLEAR_DATA_4WEEKS},
      {"clearDataEverything", IDS_SETTINGS_CLEAR_DATA_EVERYTHING},
      {"warnAboutNonClearedData", IDS_SETTINGS_CLEAR_DATA_SOME_STUFF_REMAINS},
      {"clearsSyncedData", IDS_SETTINGS_CLEAR_DATA_CLEARS_SYNCED_DATA},
      {"clearBrowsingDataLearnMoreUrl", IDS_SETTINGS_CLEAR_DATA_LEARN_MORE_URL},
      {"historyDeletionDialogTitle",
       IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE},
      {"historyDeletionDialogOK",
       IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OK},
  };

  html_source->AddString(
      "otherFormsOfBrowsingHistory",
      l10n_util::GetStringFUTF16(
          IDS_CLEAR_BROWSING_DATA_HISTORY_FOOTER,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_CLEAR_DATA_WEB_HISTORY_URL_IN_FOOTER)));
  html_source->AddString(
      "historyDeletionDialogBody",
      l10n_util::GetStringFUTF16(
          IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE,
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_CLEAR_DATA_WEB_HISTORY_URL_IN_DIALOG)));

  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if !defined(OS_CHROMEOS)
void AddDefaultBrowserStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"defaultBrowser", IDS_SETTINGS_DEFAULT_BROWSER},
      {"defaultBrowserDefault", IDS_SETTINGS_DEFAULT_BROWSER_DEFAULT},
      {"defaultBrowserMakeDefault", IDS_SETTINGS_DEFAULT_BROWSER_MAKE_DEFAULT},
      {"defaultBrowserUnknown", IDS_SETTINGS_DEFAULT_BROWSER_UNKNOWN},
      {"defaultBrowserSecondary", IDS_SETTINGS_DEFAULT_BROWSER_SECONDARY},
      {"unableToSetDefaultBrowser", IDS_SETTINGS_DEFAULT_BROWSER_ERROR},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

#if defined(OS_CHROMEOS)
void AddDeviceStrings(content::WebUIDataSource* html_source) {
  LocalizedString device_strings[] = {
      {"devicePageTitle", IDS_SETTINGS_DEVICE_TITLE},
      {"scrollLabel", IDS_SETTINGS_SCROLL_LABEL},
      {"traditionalScrollLabel", IDS_SETTINGS_TRADITIONAL_SCROLL_LABEL},
      {"naturalScrollLabel", IDS_SETTINGS_NATURAL_SCROLL_LABEL},
      {"naturalScrollLearnMore", IDS_SETTINGS_NATURAL_SCROLL_LEARN_MORE},
  };
  AddLocalizedStringsBulk(html_source, device_strings,
                          arraysize(device_strings));

  LocalizedString pointers_strings[] = {
      {"mouseTitle", IDS_SETTINGS_MOUSE_TITLE},
      {"touchpadTitle", IDS_SETTINGS_TOUCHPAD_TITLE},
      {"mouseAndTouchpadTitle", IDS_SETTINGS_MOUSE_AND_TOUCHPAD_TITLE},
      {"touchpadTapToClickEnabledLabel",
       IDS_SETTINGS_TOUCHPAD_TAP_TO_CLICK_ENABLED_LABEL},
      {"touchpadSpeed", IDS_SETTINGS_TOUCHPAD_SPEED_LABEL},
      {"pointerSlow", IDS_SETTINGS_POINTER_SPEED_SLOW_LABEL},
      {"pointerFast", IDS_SETTINGS_POINTER_SPEED_FAST_LABEL},
      {"mouseSpeed", IDS_SETTINGS_MOUSE_SPEED_LABEL},
      {"mouseSwapButtons", IDS_SETTINGS_MOUSE_SWAP_BUTTONS_LABEL},
  };
  AddLocalizedStringsBulk(html_source, pointers_strings,
                          arraysize(pointers_strings));

  LocalizedString keyboard_strings[] = {
      {"keyboardTitle", IDS_SETTINGS_KEYBOARD_TITLE},
      {"keyboardKeySearch", IDS_SETTINGS_KEYBOARD_KEY_SEARCH},
      {"keyboardKeyCtrl", IDS_SETTINGS_KEYBOARD_KEY_LEFT_CTRL},
      {"keyboardKeyAlt", IDS_SETTINGS_KEYBOARD_KEY_LEFT_ALT},
      {"keyboardKeyCapsLock", IDS_SETTINGS_KEYBOARD_KEY_CAPS_LOCK},
      {"keyboardKeyDiamond", IDS_SETTINGS_KEYBOARD_KEY_DIAMOND},
      {"keyboardKeyEscape", IDS_SETTINGS_KEYBOARD_KEY_ESCAPE},
      {"keyboardKeyDisabled", IDS_SETTINGS_KEYBOARD_KEY_DISABLED},
      {"keyboardSendFunctionKeys", IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS},
      {"keyboardSendFunctionKeysDescription",
       IDS_SETTINGS_KEYBOARD_SEND_FUNCTION_KEYS_DESCRIPTION},
      {"keyboardEnableAutoRepeat", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_ENABLE},
      {"keyRepeatDelay", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY},
      {"keyRepeatDelayLong", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_LONG},
      {"keyRepeatDelayShort", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_DELAY_SHORT},
      {"keyRepeatRate", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE},
      {"keyRepeatRateSlow", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_RATE_SLOW},
      {"keyRepeatRateFast", IDS_SETTINGS_KEYBOARD_AUTO_REPEAT_FAST},
      {"showKeyboardShortcutsOverlay",
       IDS_SETTINGS_KEYBOARD_SHOW_KEYBOARD_SHORTCUTS_OVERLAY},
      {"keyboardShowLanguageAndInput",
       IDS_SETTINGS_KEYBOARD_SHOW_LANGUAGE_AND_INPUT},
  };
  AddLocalizedStringsBulk(html_source, keyboard_strings,
                          arraysize(keyboard_strings));

  LocalizedString stylus_strings[] = {
      {"stylusTitle", IDS_SETTINGS_STYLUS_TITLE},
      {"stylusEnableStylusTools", IDS_SETTINGS_STYLUS_ENABLE_STYLUS_TOOLS},
      {"stylusAutoOpenStylusTools", IDS_SETTINGS_STYLUS_AUTO_OPEN_STYLUS_TOOLS},
      {"stylusFindMoreApps", IDS_SETTINGS_STYLUS_FIND_MORE_APPS}};
  AddLocalizedStringsBulk(html_source, stylus_strings,
                          arraysize(stylus_strings));

  LocalizedString display_strings[] = {
      {"displayTitle", IDS_SETTINGS_DISPLAY_TITLE},
      {"displayArrangement", IDS_SETTINGS_DISPLAY_ARRANGEMENT},
      {"displayMirror", IDS_SETTINGS_DISPLAY_MIRROR},
      {"displayMakePrimary", IDS_SETTINGS_DISPLAY_MAKE_PRIMARY},
      {"displayResolutionTitle", IDS_SETTINGS_DISPLAY_RESOLUTION_TITLE},
      {"displayResolutionText", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT},
      {"displayResolutionTextBest", IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_BEST},
      {"displayResolutionTextNative",
       IDS_SETTINGS_DISPLAY_RESOLUTION_TEXT_NATIVE},
      {"displayOrientation", IDS_SETTINGS_DISPLAY_ORIENTATION},
      {"displayOrientationStandard", IDS_SETTINGS_DISPLAY_ORIENTATION_STANDARD},
      {"displayOverscanPageTitle", IDS_SETTINGS_DISPLAY_OVERSCAN_TITLE},
      {"displayOverscanInstructions",
       IDS_SETTINGS_DISPLAY_OVERSCAN_INSTRUCTIONS},
      {"displayOverscanResize", IDS_SETTINGS_DISPLAY_OVERSCAN_RESIZE},
      {"displayOverscanPosition", IDS_SETTINGS_DISPLAY_OVERSCAN_POSITION},
      {"displayOverscanReset", IDS_SETTINGS_DISPLAY_OVERSCAN_RESET},
      {"displayOverscanSave", IDS_SETTINGS_DISPLAY_OVERSCAN_SAVE},
  };
  AddLocalizedStringsBulk(html_source, display_strings,
                          arraysize(display_strings));

  html_source->AddString("naturalScrollLearnMoreLink",
                         base::ASCIIToUTF16(chrome::kNaturalScrollHelpURL));
}
#endif

void AddDownloadsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"downloadsPageTitle", IDS_SETTINGS_DOWNLOADS},
      {"downloadLocation", IDS_SETTINGS_DOWNLOAD_LOCATION},
      {"changeDownloadLocation", IDS_SETTINGS_CHANGE_DOWNLOAD_LOCATION},
      {"promptForDownload", IDS_SETTINGS_PROMPT_FOR_DOWNLOAD},
      {"disconnectGoogleDriveAccount", IDS_SETTINGS_DISCONNECT_GOOGLE_DRIVE},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddResetStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"resetPageTitle", IDS_SETTINGS_RESET},
    {"resetPageDescription", IDS_RESET_PROFILE_SETTINGS_DESCRIPTION},
    {"resetPageExplanation", IDS_RESET_PROFILE_SETTINGS_EXPLANATION},
    {"resetPageCommit", IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON},
    {"resetPageFeedback", IDS_SETTINGS_RESET_PROFILE_FEEDBACK},
    {"viewReportedSettings", IDS_SETTINGS_RESET_VIEW_REPORTED_SETTINGS},
#if defined(OS_CHROMEOS)
    {"powerwashTitle", IDS_OPTIONS_FACTORY_RESET},
    {"powerwashDialogTitle", IDS_OPTIONS_FACTORY_RESET_HEADING},
    {"powerwashDialogExplanation", IDS_OPTIONS_FACTORY_RESET_WARNING},
    {"powerwashDialogButton", IDS_SETTINGS_RESTART},
    {"powerwashLearnMoreUrl", IDS_FACTORY_RESET_HELP_URL},
#endif
    // Automatic reset banner.
    {"resetProfileBannerButton",
     IDS_AUTOMATIC_SETTINGS_RESET_BANNER_RESET_BUTTON_TEXT},
    {"resetProfileBannerDescription", IDS_AUTOMATIC_SETTINGS_RESET_BANNER_TEXT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddString("resetPageLearnMoreUrl",
                         chrome::kResetProfileSettingsLearnMoreURL);
  html_source->AddString("resetProfileBannerLearnMoreUrl",
                         chrome::kAutomaticSettingsResetLearnMoreURL);
#if defined(OS_CHROMEOS)
  html_source->AddString(
      "powerwashDescription",
      l10n_util::GetStringFUTF16(IDS_OPTIONS_FACTORY_RESET_DESCRIPTION,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
#endif
}

void AddDateTimeStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"dateTimePageTitle", IDS_SETTINGS_DATE_TIME},
      {"timeZone", IDS_SETTINGS_TIME_ZONE},
      {"use24HourClock", IDS_SETTINGS_USE_24_HOUR_CLOCK},
      {"dateTimeSetAutomatically", IDS_SETTINGS_DATE_TIME_SET_AUTOMATICALLY},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if defined(OS_CHROMEOS)
void AddEasyUnlockStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"easyUnlockSectionTitle", IDS_SETTINGS_EASY_UNLOCK_SECTION_TITLE},
      {"easyUnlockSetupButton", IDS_SETTINGS_EASY_UNLOCK_SETUP},
      // Easy Unlock turn-off dialog.
      {"easyUnlockTurnOffButton", IDS_SETTINGS_EASY_UNLOCK_TURN_OFF},
      {"easyUnlockTurnOffOfflineTitle",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_OFFLINE_TITLE},
      {"easyUnlockTurnOffOfflineMessage",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_OFFLINE_MESSAGE},
      {"easyUnlockTurnOffErrorTitle",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_ERROR_TITLE},
      {"easyUnlockTurnOffErrorMessage",
       IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_ERROR_MESSAGE},
      {"easyUnlockTurnOffRetryButton", IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_RETRY},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  base::string16 device_name =
      l10n_util::GetStringUTF16(ash::GetChromeOSDeviceTypeResourceId());
  html_source->AddString(
      "easyUnlockSetupIntro",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_EASY_UNLOCK_SETUP_INTRO,
                                 device_name));
  html_source->AddString(
      "easyUnlockDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_EASY_UNLOCK_DESCRIPTION,
                                 device_name));
  html_source->AddString(
      "easyUnlockTurnOffTitle",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_TITLE,
                                 device_name));
  html_source->AddString(
      "easyUnlockTurnOffDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_EASY_UNLOCK_TURN_OFF_DESCRIPTION,
                                 device_name));
  html_source->AddString(
      "easyUnlockRequireProximityLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_EASY_UNLOCK_REQUIRE_PROXIMITY_LABEL, device_name));

  html_source->AddString("easyUnlockLearnMoreURL",
                         chrome::kEasyUnlockLearnMoreUrl);
}

void AddInternetStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"internetPageTitle", IDS_SETTINGS_INTERNET},
      {"internetDetailPageTitle", IDS_SETTINGS_INTERNET_DETAIL},
      {"internetKnownNetworksPageTitle", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS},
      {"knownNetworksButton", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_BUTTON},
      {"knownNetworksMessage", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MESSAGE},
      {"knownNetworksPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_PREFFERED},
      {"knownNetworksNoPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_NO_PREFERRED},
      {"knownNetworksAll", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_ALL},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif

void AddLanguagesStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"languagesPageTitle", IDS_SETTINGS_LANGUAGES_PAGE_TITLE},
      {"languagesListTitle", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_TITLE},
      {"orderLanguagesInstructions",
       IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_ORDERING_INSTRUCTIONS},
      {"moveUp", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_UP},
      {"moveDown", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_MOVE_DOWN},
      {"removeLanguage", IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_REMOVE},
      {"languageDetail",
       IDS_SETTINGS_LANGUAGES_LANGUAGES_LIST_LANGUAGE_DETAIL},
      {"addLanguages", IDS_SETTINGS_LANGUAGES_LANGUAGES_ADD},
#if defined(OS_CHROMEOS)
      {"inputMethodsListTitle",
       IDS_SETTINGS_LANGUAGES_INPUT_METHODS_LIST_TITLE},
      {"manageInputMethods", IDS_SETTINGS_LANGUAGES_INPUT_METHODS_MANAGE},
      {"manageInputMethodsPageTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_INPUT_METHODS_TITLE},
#endif
      {"addLanguagesDialogTitle",
       IDS_SETTINGS_LANGUAGES_MANAGE_LANGUAGES_TITLE},
      {"allLanguages", IDS_SETTINGS_LANGUAGES_ALL_LANGUAGES},
      {"enabledLanguages", IDS_SETTINGS_LANGUAGES_ENABLED_LANGUAGES},
      {"cannotBeDisplayedInThisLanguage",
       IDS_SETTINGS_LANGUAGES_CANNOT_BE_DISPLAYED_IN_THIS_LANGUAGE},
      {"isDisplayedInThisLanguage",
       IDS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE},
      {"displayInThisLanguage",
       IDS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE},
      {"offerToTranslateInThisLanguage",
       IDS_SETTINGS_LANGUAGES_OFFER_TO_TRANSLATE_IN_THIS_LANGUAGE},
      {"cannotTranslateInThisLanguage",
       IDS_SETTINGS_LANGUAGES_CANNOT_TRANSLATE_IN_THIS_LANGUAGE},
#if !defined(OS_MACOSX)
      {"spellCheckListTitle", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_LIST_TITLE},
      {"spellCheckSummaryTwoLanguages",
       IDS_SETTINGS_LANGUAGES_SPELL_CHECK_SUMMARY_TWO_LANGUAGES},
      // TODO(michaelpg): Use ICU plural format when available to properly
      // translate "and [n] other(s)".
      {"spellCheckSummaryThreeLanguages",
       IDS_SETTINGS_LANGUAGES_SPELL_CHECK_SUMMARY_THREE_LANGUAGES},
      {"spellCheckSummaryMultipleLanguages",
       IDS_SETTINGS_LANGUAGES_SPELL_CHECK_SUMMARY_MULTIPLE_LANGUAGES},
      {"manageSpellCheck", IDS_SETTINGS_LANGUAGES_SPELL_CHECK_MANAGE},
      {"editDictionaryPageTitle", IDS_SETTINGS_LANGUAGES_EDIT_DICTIONARY_TITLE},
      {"addDictionaryWordLabel", IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD},
      {"addDictionaryWordButton",
       IDS_SETTINGS_LANGUAGES_ADD_DICTIONARY_WORD_BUTTON},
      {"customDictionaryWords", IDS_SETTINGS_LANGUAGES_DICTIONARY_WORDS},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddString(
      "languagesLearnMoreURL",
      base::ASCIIToUTF16(chrome::kLanguageSettingsLearnMoreUrl));
}

#if defined(OS_CHROMEOS)
void AddMultiProfilesStrings(content::WebUIDataSource* html_source,
                             Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  std::string primary_user_email = user_manager->GetPrimaryUser()->email();
  html_source->AddString("primaryUserEmail", primary_user_email);
  html_source->AddBoolean("isSecondaryUser",
                          user && user->email() != primary_user_email);
}
#endif

void AddOnStartupStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"onStartup", IDS_SETTINGS_ON_STARTUP},
      {"onStartupOpenNewTab", IDS_SETTINGS_ON_STARTUP_OPEN_NEW_TAB},
      {"onStartupContinue", IDS_SETTINGS_ON_STARTUP_CONTINUE},
      {"onStartupOpenSpecific", IDS_SETTINGS_ON_STARTUP_OPEN_SPECIFIC},
      {"onStartupUseCurrent", IDS_SETTINGS_ON_STARTUP_USE_CURRENT},
      {"onStartupAddNewPage", IDS_SETTINGS_ON_STARTUP_ADD_NEW_PAGE},
      {"onStartupEditPage", IDS_SETTINGS_ON_STARTUP_EDIT_PAGE},
      {"onStartupSiteUrl", IDS_SETTINGS_ON_STARTUP_SITE_URL},
      {"onStartupRemove", IDS_SETTINGS_ON_STARTUP_REMOVE},
      {"onStartupEdit", IDS_SETTINGS_ON_STARTUP_EDIT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddPasswordsAndFormsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"overflowMenu", IDS_SETTINGS_OVERFLOW_MENU},
      {"passwordsAndAutofillPageTitle",
       IDS_SETTINGS_PASSWORDS_AND_AUTOFILL_PAGE_TITLE},
      {"autofill", IDS_SETTINGS_AUTOFILL},
      {"googlePayments", IDS_SETTINGS_GOOGLE_PAYMENTS},
      {"googlePaymentsCached", IDS_SETTINGS_GOOGLE_PAYMENTS_CACHED},
      {"addresses", IDS_SETTINGS_AUTOFILL_ADDRESSES_HEADING},
      {"addAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_ADD_TITLE},
      {"editAddressTitle", IDS_SETTINGS_AUTOFILL_ADDRESSES_EDIT_TITLE},
      {"addressCountry", IDS_SETTINGS_AUTOFILL_ADDRESSES_COUNTRY},
      {"addressPhone", IDS_SETTINGS_AUTOFILL_ADDRESSES_PHONE},
      {"addressEmail", IDS_SETTINGS_AUTOFILL_ADDRESSES_EMAIL},
      {"addAddress", IDS_SETTINGS_AUTOFILL_ADD_ADDRESS_BUTTON},
      {"editAddress", IDS_SETTINGS_ADDRESS_EDIT},
      {"removeAddress", IDS_SETTINGS_ADDRESS_REMOVE},
      {"creditCards", IDS_SETTINGS_AUTOFILL_CREDIT_CARD_HEADING},
      {"addCreditCard", IDS_SETTINGS_AUTOFILL_ADD_CREDIT_CARD_BUTTON},
      {"editCreditCard", IDS_SETTINGS_CREDIT_CARD_EDIT},
      {"removeCreditCard", IDS_SETTINGS_CREDIT_CARD_REMOVE},
      {"clearCreditCard", IDS_SETTINGS_CREDIT_CARD_CLEAR},
      {"creditCardType", IDS_SETTINGS_AUTOFILL_CREDIT_CARD_TYPE_COLUMN_LABEL},
      {"creditCardExpiration", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_DATE},
      {"creditCardName", IDS_SETTINGS_NAME_ON_CREDIT_CARD},
      {"creditCardNumber", IDS_SETTINGS_CREDIT_CARD_NUMBER},
      {"creditCardExpirationMonth", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_MONTH},
      {"creditCardExpirationYear", IDS_SETTINGS_CREDIT_CARD_EXPIRATION_YEAR},
      {"editCreditCardTitle", IDS_SETTINGS_EDIT_CREDIT_CARD_TITLE},
      {"addCreditCardTitle", IDS_SETTINGS_ADD_CREDIT_CARD_TITLE},
      {"autofillDetail", IDS_SETTINGS_AUTOFILL_DETAIL},
      {"passwords", IDS_SETTINGS_PASSWORDS},
      {"passwordsAutosigninLabel",
       IDS_SETTINGS_PASSWORDS_AUTOSIGNIN_CHECKBOX_LABEL},
      {"passwordsAutosigninDescription",
       IDS_SETTINGS_PASSWORDS_AUTOSIGNIN_CHECKBOX_DESC},
      {"passwordsDetail", IDS_SETTINGS_PASSWORDS_DETAIL},
      {"savedPasswordsHeading", IDS_SETTINGS_PASSWORDS_SAVED_HEADING},
      {"passwordExceptionsHeading", IDS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING},
      {"deletePasswordException", IDS_SETTINGS_PASSWORDS_DELETE_EXCEPTION},
      {"passwordsDone", IDS_SETTINGS_PASSWORD_DONE},
      {"removePassword", IDS_SETTINGS_PASSWORD_REMOVE},
      {"searchPasswords", IDS_SETTINGS_PASSWORD_SEARCH},
      {"passwordDetailsTitle", IDS_SETTINGS_PASSWORDS_VIEW_DETAILS_TITLE},
      {"passwordViewDetails", IDS_SETTINGS_PASSWORD_VIEW_DETAILS},
      {"editPasswordWebsiteLabel", IDS_SETTINGS_PASSWORDS_WEBSITE},
      {"editPasswordUsernameLabel", IDS_SETTINGS_PASSWORDS_USERNAME},
      {"editPasswordPasswordLabel", IDS_SETTINGS_PASSWORDS_PASSWORD},
  };

  html_source->AddString(
      "managePasswordsLabel",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PASSWORDS_MANAGE_PASSWORDS,
          base::ASCIIToUTF16(
              password_manager::kPasswordManagerAccountDashboardURL)));
  html_source->AddString(
      "manageAddressesUrl",
      autofill::payments::GetManageAddressesUrl(0).spec());
  html_source->AddString(
      "manageCreditCardsUrl",
      autofill::payments::GetManageInstrumentsUrl(0).spec());

  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddPeopleStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"peoplePageTitle", IDS_SETTINGS_PEOPLE},
    {"manageOtherPeople", IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE},
    {"manageSupervisedUsers", IDS_SETTINGS_PEOPLE_MANAGE_SUPERVISED_USERS},
#if defined(OS_CHROMEOS)
    {"configurePinChoosePinTitle",
     IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CHOOSE_PIN_TITLE},
    {"configurePinConfirmPinTitle",
     IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CONFIRM_PIN_TITLE},
    {"configurePinContinueButton",
     IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CONTINUE_BUTTON},
    {"configurePinMismatched", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_MISMATCHED},
    {"configurePinTooShort", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_TOO_SHORT},
    {"configurePinWeakPin", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_WEAK_PIN},
    {"enableScreenlock", IDS_SETTINGS_PEOPLE_ENABLE_SCREENLOCK},
    {"lockScreenChangePinButton",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_CHANGE_PIN_BUTTON},
    {"lockScreenNone", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NONE},
    {"lockScreenPasswordOnly", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PASSWORD_ONLY},
    {"lockScreenPinOrPassword",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_OR_PASSWORD},
    {"lockScreenSetupPinButton",
     IDS_SETTINGS_PEOPLE_LOCK_SCREEN_SETUP_PIN_BUTTON},
    {"lockScreenTitle", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE},
    {"passwordPromptEnterPassword",
     IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_ENTER_PASSWORD},
    {"passwordPromptInvalidPassword",
     IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_INVALID_PASSWORD},
    {"passwordPromptPasswordLabel",
     IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_PASSWORD_LABEL},
    {"passwordPromptTitle", IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_TITLE},
    {"pinKeyboardPlaceholderPin", IDS_PIN_KEYBOARD_HINT_TEXT_PIN},
    {"pinKeyboardPlaceholderPinPassword",
     IDS_PIN_KEYBOARD_HINT_TEXT_PIN_PASSWORD},
    {"changePictureTitle", IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TITLE},
    {"changePicturePageDescription", IDS_SETTINGS_CHANGE_PICTURE_DIALOG_TEXT},
    {"takePhoto", IDS_SETTINGS_CHANGE_PICTURE_TAKE_PHOTO},
    {"discardPhoto", IDS_SETTINGS_CHANGE_PICTURE_DISCARD_PHOTO},
    {"flipPhoto", IDS_SETTINGS_CHANGE_PICTURE_FLIP_PHOTO},
    {"chooseFile", IDS_SETTINGS_CHANGE_PICTURE_CHOOSE_FILE},
    {"profilePhoto", IDS_SETTINGS_CHANGE_PICTURE_PROFILE_PHOTO},
    {"oldPhoto", IDS_SETTINGS_CHANGE_PICTURE_OLD_PHOTO},
    {"profilePhotoLoading", IDS_SETTINGS_CHANGE_PICTURE_PROFILE_LOADING_PHOTO},
    {"previewAltText", IDS_SETTINGS_CHANGE_PICTURE_PREVIEW_ALT},
    {"authorCredit", IDS_SETTINGS_CHANGE_PICTURE_AUTHOR_TEXT},
    {"photoFromCamera", IDS_SETTINGS_CHANGE_PICTURE_PHOTO_FROM_CAMERA},
    {"photoFlippedAccessibleText", IDS_SETTINGS_PHOTO_FLIP_ACCESSIBLE_TEXT},
    {"photoFlippedBackAccessibleText",
     IDS_SETTINGS_PHOTO_FLIPBACK_ACCESSIBLE_TEXT},
    {"photoCaptureAccessibleText", IDS_SETTINGS_PHOTO_CAPTURE_ACCESSIBLE_TEXT},
    {"photoDiscardAccessibleText", IDS_SETTINGS_PHOTO_DISCARD_ACCESSIBLE_TEXT},
#else   // !defined(OS_CHROMEOS)
    {"domainManagedProfile", IDS_SETTINGS_PEOPLE_DOMAIN_MANAGED_PROFILE},
    {"syncDisconnectManagedProfileExplanation",
     IDS_SETTINGS_SYNC_DISCONNECT_MANAGED_PROFILE_EXPLANATION},
    {"editPerson", IDS_SETTINGS_EDIT_PERSON},
#endif  // defined(OS_CHROMEOS)
    {"syncOverview", IDS_SETTINGS_SYNC_OVERVIEW},
    {"syncSignin", IDS_SETTINGS_SYNC_SIGNIN},
    {"syncDisconnect", IDS_SETTINGS_SYNC_DISCONNECT},
    {"syncDisconnectTitle", IDS_SETTINGS_SYNC_DISCONNECT_TITLE},
    {"syncDisconnectExplanation", IDS_SETTINGS_SYNC_DISCONNECT_EXPLANATION},
    {"syncDisconnectDeleteProfile",
     IDS_SETTINGS_SYNC_DISCONNECT_DELETE_PROFILE},
    {"syncDisconnectConfirm", IDS_SETTINGS_SYNC_DISCONNECT_CONFIRM},
    {"sync", IDS_SETTINGS_SYNC},
    {"syncPageTitle", IDS_SETTINGS_SYNC_PAGE_TITLE},
    {"syncLoading", IDS_SETTINGS_SYNC_LOADING},
    {"syncTimeout", IDS_SETTINGS_SYNC_TIMEOUT},
    {"syncEverythingCheckboxLabel",
     IDS_SETTINGS_SYNC_EVERYTHING_CHECKBOX_LABEL},
    {"appCheckboxLabel", IDS_SETTINGS_APPS_CHECKBOX_LABEL},
    {"extensionsCheckboxLabel", IDS_SETTINGS_EXTENSIONS_CHECKBOX_LABEL},
    {"settingsCheckboxLabel", IDS_SETTINGS_SETTINGS_CHECKBOX_LABEL},
    {"autofillCheckboxLabel", IDS_SETTINGS_AUTOFILL_CHECKBOX_LABEL},
    {"historyCheckboxLabel", IDS_SETTINGS_HISTORY_CHECKBOX_LABEL},
    {"themesAndWallpapersCheckboxLabel",
     IDS_SETTINGS_THEMES_AND_WALLPAPERS_CHECKBOX_LABEL},
    {"bookmarksCheckboxLabel", IDS_SETTINGS_BOOKMARKS_CHECKBOX_LABEL},
    {"passwordsCheckboxLabel", IDS_SETTINGS_PASSWORDS_CHECKBOX_LABEL},
    {"openTabsCheckboxLabel", IDS_SETTINGS_OPEN_TABS_CHECKBOX_LABEL},
    {"enablePaymentsIntegrationCheckboxLabel",
     IDS_SETTINGS_ENABLE_PAYMENTS_INTEGRATION_CHECKBOX_LABEL},
    {"manageSyncedDataTitle", IDS_SETTINGS_MANAGE_SYNCED_DATA_TITLE},
    {"manageSyncedDataDescription",
     IDS_SETTINGS_MANAGE_SYNCED_DATA_DESCRIPTION},
    {"encryptionOptionsTitle", IDS_SETTINGS_ENCRYPTION_OPTIONS},
    {"syncDataEncryptedText", IDS_SETTINGS_SYNC_DATA_ENCRYPTED_TEXT},
    {"encryptWithGoogleCredentialsLabel",
     IDS_SETTINGS_ENCRYPT_WITH_GOOGLE_CREDENTIALS_LABEL},
    {"encryptWithSyncPassphraseLabel",
     IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LABEL},
    {"encryptWithSyncPassphraseLearnMoreLink",
     IDS_SETTINGS_ENCRYPT_WITH_SYNC_PASSPHRASE_LEARN_MORE_LINK},
    {"useDefaultSettingsButton", IDS_SETTINGS_USE_DEFAULT_SETTINGS},
    {"passphraseExplanationText", IDS_SETTINGS_PASSPHRASE_EXPLANATION_TEXT},
    {"emptyPassphraseError", IDS_SETTINGS_EMPTY_PASSPHRASE_ERROR},
    {"mismatchedPassphraseError", IDS_SETTINGS_MISMATCHED_PASSPHRASE_ERROR},
    {"incorrectPassphraseError", IDS_SETTINGS_INCORRECT_PASSPHRASE_ERROR},
    {"passphrasePlaceholder", IDS_SETTINGS_PASSPHRASE_PLACEHOLDER},
    {"passphraseConfirmationPlaceholder",
     IDS_SETTINGS_PASSPHRASE_CONFIRMATION_PLACEHOLDER},
    {"submitPassphraseButton", IDS_SETTINGS_SUBMIT_PASSPHRASE},
    {"personalizeGoogleServicesTitle",
     IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_TITLE},
    {"personalizeGoogleServicesText",
     IDS_SETTINGS_PERSONALIZE_GOOGLE_SERVICES_TEXT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  // Format numbers to be used on the pin keyboard.
  for (int j = 0; j <= 9; j++) {
    html_source->AddString("pinKeyboard" + base::IntToString(j),
                           base::FormatNumber(int64_t{j}));
  }

  html_source->AddString("autofillHelpURL", autofill::kHelpURL);
  html_source->AddString("supervisedUsersUrl",
                         chrome::kLegacySupervisedUserManagementURL);

  std::string sync_dashboard_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();
  html_source->AddString("syncDashboardUrl", sync_dashboard_url);
  html_source->AddString(
      "passphraseRecover",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_PASSPHRASE_RECOVER,
                                base::ASCIIToUTF16(sync_dashboard_url)));

  html_source->AddString("activityControlsUrl",
                         chrome::kGoogleAccountActivityControlsURL);
}

void AddPrintingStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"printingPageTitle", IDS_SETTINGS_PRINTING},
    {"printingCloudPrintLearnMoreLabel",
     IDS_SETTINGS_PRINTING_CLOUD_PRINT_LEARN_MORE_LABEL},
    {"printingNotificationsLabel", IDS_SETTINGS_PRINTING_NOTIFICATIONS_LABEL},
    {"printingManageCloudPrintDevices",
     IDS_SETTINGS_PRINTING_MANAGE_CLOUD_PRINT_DEVICES},
    {"cloudPrintersTitle", IDS_SETTINGS_PRINTING_CLOUD_PRINTERS},
#if defined(OS_CHROMEOS)
    {"cupsPrintersTitle", IDS_SETTINGS_PRINTING_CUPS_PRINTERS},
    {"addCupsPrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_ADD_PRINTER},
    {"cupsPrinterDetails", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_DETAILS},
    {"removePrinter", IDS_SETTINGS_PRINTING_CUPS_PRINTERS_REMOVE},
    {"searchLabel", IDS_SETTINGS_PRINTING_CUPS_SEARCH_LABEL},
    {"printerDetailsTitle", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_TITLE},
    {"printerName", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_NAME},
    {"printerModel", IDS_SETTINGS_PRINTING_CUPS_PRINTER_DETAILS_MODEL},
    {"addPrinterTitle", IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_TITLE},
    {"cancelButtonText", IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_CANCEL},
    {"addPrinterButtonText", IDS_SETTINGS_PRINTING_CUPS_ADD_PRINTER_BUTTON_ADD},
    {"printerDetailsAdvanced", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED},
    {"printerAddress", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_ADDRESS},
    {"printerProtocol", IDS_SETTINGS_PRINTING_CUPS_PRINTER_ADVANCED_PROTOCOL},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddString("devicesUrl", chrome::kChromeUIDevicesURL);
  html_source->AddString("printingCloudPrintLearnMoreUrl",
                         chrome::kCloudPrintLearnMoreURL);
}

void AddPrivacyStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"privacyPageTitle", IDS_SETTINGS_PRIVACY},
      {"linkDoctorPref", IDS_SETTINGS_LINKDOCTOR_PREF},
      {"searchSuggestPref", IDS_SETTINGS_SUGGEST_PREF},
      {"networkPredictionEnabled",
       IDS_SETTINGS_NETWORK_PREDICTION_ENABLED_DESCRIPTION},
      {"safeBrowsingEnableProtection",
       IDS_SETTINGS_SAFEBROWSING_ENABLEPROTECTION},
      {"safeBrowsingEnableExtendedReporting",
       IDS_SETTINGS_SAFEBROWSING_ENABLE_EXTENDED_REPORTING},
      {"spellingPref", IDS_SETTINGS_SPELLING_PREF},
#if defined(OS_CHROMEOS)
      {"enableLogging", IDS_SETTINGS_ENABLE_LOGGING_DIAGNOSTIC_AND_USAGE_DATA},
#else
      {"enableLogging", IDS_SETTINGS_ENABLE_LOGGING},
#endif
      {"doNotTrack", IDS_SETTINGS_ENABLE_DO_NOT_TRACK},
      {"enableContentProtectionAttestation",
       IDS_SETTINGS_ENABLE_CONTENT_PROTECTION_ATTESTATION},
      {"wakeOnWifi", IDS_SETTINGS_WAKE_ON_WIFI_DESCRIPTION},
      {"manageCertificates", IDS_SETTINGS_MANAGE_CERTIFICATES},
      {"manageCertificatesDescription",
       IDS_SETTINGS_MANAGE_CERTIFICATES_DESCRIPTION},
      {"siteSettings", IDS_SETTINGS_SITE_SETTINGS},
      {"siteSettingsDescription", IDS_SETTINGS_SITE_SETTINGS_DESCRIPTION},
      {"clearBrowsingData", IDS_SETTINGS_CLEAR_DATA},
      {"titleAndCount", IDS_SETTINGS_TITLE_AND_COUNT},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  html_source->AddString("improveBrowsingExperience",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_IMPROVE_BROWSING_EXPERIENCE,
                             base::ASCIIToUTF16(chrome::kPrivacyLearnMoreURL)));
}

void AddSearchInSettingsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"searchPrompt", IDS_SETTINGS_SEARCH_PROMPT},
    {"searchNoResults", IDS_SETTINGS_SEARCH_NO_RESULTS},
    // TODO(dpapad); IDS_DOWNLOAD_CLEAR_SEARCH and IDS_MD_HISTORY_CLEAR_SEARCH
    // are identical, merge them to one and re-use here.
    {"clearSearch", IDS_DOWNLOAD_CLEAR_SEARCH},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  base::string16 help_text = l10n_util::GetStringFUTF16(
      IDS_SETTINGS_SEARCH_NO_RESULTS_HELP,
      base::ASCIIToUTF16(chrome::kSettingsSearchHelpURL));
  html_source->AddString("searchNoResultsHelp", help_text);
}

void AddSearchStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"searchPageTitle", IDS_SETTINGS_SEARCH},
      {"searchExplanation", IDS_SETTINGS_SEARCH_EXPLANATION},
      {"searchEnginesManage", IDS_SETTINGS_SEARCH_MANAGE_SEARCH_ENGINES},
      {"searchOkGoogleLabel", IDS_SETTINGS_SEARCH_OK_GOOGLE_LABEL},
      {"searchOkGoogleLearnMoreLink",
       IDS_SETTINGS_SEARCH_OK_GOOGLE_LEARN_MORE_LINK},
      {"searchOkGoogleDescriptionLabel",
       IDS_SETTINGS_SEARCH_OK_GOOGLE_DESCRIPTION_LABEL},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddSearchEnginesStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"searchEnginesPageTitle", IDS_SETTINGS_SEARCH_ENGINES},
      {"searchEnginesAddSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_ADD_SEARCH_ENGINE},
      {"searchEnginesEditSearchEngine",
       IDS_SETTINGS_SEARCH_ENGINES_EDIT_SEARCH_ENGINE},
      {"searchEnginesNotValid", IDS_SETTINGS_SEARCH_ENGINES_NOT_VALID},
      {"searchEngines", IDS_SETTINGS_SEARCH_ENGINES},
      {"searchEnginesDefault", IDS_SETTINGS_SEARCH_ENGINES_DEFAULT_ENGINES},
      {"searchEnginesOther", IDS_SETTINGS_SEARCH_ENGINES_OTHER_ENGINES},
      {"searchEnginesExtension", IDS_SETTINGS_SEARCH_ENGINES_EXTENSION_ENGINES},
      {"searchEnginesSearchEngine", IDS_SETTINGS_SEARCH_ENGINES_SEARCH_ENGINE},
      {"searchEnginesKeyword", IDS_SETTINGS_SEARCH_ENGINES_KEYWORD},
      {"searchEnginesQueryURL", IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL},
      {"searchEnginesQueryURLExplanation",
       IDS_SETTINGS_SEARCH_ENGINES_QUERY_URL_EXPLANATION},
      {"searchEnginesMakeDefault", IDS_SETTINGS_SEARCH_ENGINES_MAKE_DEFAULT},
      {"searchEnginesEdit", IDS_SETTINGS_SEARCH_ENGINES_EDIT},
      {"searchEnginesRemoveFromList",
       IDS_SETTINGS_SEARCH_ENGINES_REMOVE_FROM_LIST},
      {"searchEnginesManageExtension",
       IDS_SETTINGS_SEARCH_ENGINES_MANAGE_EXTENSION},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddSiteSettingsStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"addSiteHeader", IDS_SETTINGS_ADD_SITE_HEADER},
      {"addSiteLink", IDS_SETTINGS_ADD_SITE_LINK},
      {"addSite", IDS_SETTINGS_ADD_SITE},
      {"cookieAppCache", IDS_COOKIES_APPLICATION_CACHE},
      {"cookieCacheStorage", IDS_COOKIES_CACHE_STORAGE},
      {"cookieChannelId", IDS_COOKIES_CHANNEL_ID},
      {"cookieDatabaseStorage", IDS_COOKIES_DATABASE_STORAGE},
      {"cookieFileSystem", IDS_COOKIES_FILE_SYSTEM},
      {"cookieFlashLso", IDS_COOKIES_FLASH_LSO},
      {"cookieLocalStorage", IDS_COOKIES_LOCAL_STORAGE},
      {"cookiePlural", IDS_COOKIES_PLURAL_COOKIES},
      {"cookieServiceWorker", IDS_COOKIES_SERVICE_WORKER},
      {"cookieSingular", IDS_COOKIES_SINGLE_COOKIE},
      {"embeddedOnHost", IDS_EXCEPTIONS_GEOLOCATION_EMBEDDED_ON_HOST},
      {"appCacheManifest",
       IDS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL},
      {"cacheStorageLastModified",
       IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
      {"cacheStorageOrigin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
      {"cacheStorageSize",
       IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
      {"channelIdServerId", IDS_COOKIES_CHANNEL_ID_ORIGIN_LABEL},
      {"channelIdType", IDS_COOKIES_CHANNEL_ID_TYPE_LABEL},
      {"channelIdCreated", IDS_COOKIES_CHANNEL_ID_CREATED_LABEL},
      {"channelIdExpires", IDS_COOKIES_CHANNEL_ID_EXPIRES_LABEL},
      {"cookieAccessibleToScript",
       IDS_COOKIES_COOKIE_ACCESSIBLE_TO_SCRIPT_LABEL},
      {"cookieLastAccessed", IDS_COOKIES_LAST_ACCESSED_LABEL},
      {"cookieContent", IDS_COOKIES_COOKIE_CONTENT_LABEL},
      {"cookieCreated", IDS_COOKIES_COOKIE_CREATED_LABEL},
      {"cookieDomain", IDS_COOKIES_COOKIE_DOMAIN_LABEL},
      {"cookieExpires", IDS_COOKIES_COOKIE_EXPIRES_LABEL},
      {"cookieName", IDS_COOKIES_COOKIE_NAME_LABEL},
      {"cookiePath", IDS_COOKIES_COOKIE_PATH_LABEL},
      {"cookieSendFor", IDS_COOKIES_COOKIE_SENDFOR_LABEL},
      {"fileSystemOrigin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
      {"fileSystemPersistentUsage",
       IDS_COOKIES_FILE_SYSTEM_PERSISTENT_USAGE_LABEL},
      {"fileSystemTemporaryUsage",
       IDS_COOKIES_FILE_SYSTEM_TEMPORARY_USAGE_LABEL},
      {"indexedDbSize", IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
      {"indexedDbLastModified",
       IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
      {"indexedDbOrigin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
      {"localStorageLastModified",
       IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL},
      {"localStorageOrigin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
      {"localStorageSize",
       IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
      {"serviceWorkerOrigin", IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL},
      {"serviceWorkerScopes", IDS_COOKIES_SERVICE_WORKER_SCOPES_LABEL},
      {"serviceWorkerSize",
       IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL},
      {"webdbDesc", IDS_COOKIES_WEB_DATABASE_DESCRIPTION_LABEL},
      {"siteSettingsCategoryPageTitle", IDS_SETTINGS_SITE_SETTINGS_CATEGORY},
      {"siteSettingsCategoryAllSites", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES},
      {"siteSettingsCategoryCamera", IDS_SETTINGS_SITE_SETTINGS_CAMERA},
      {"siteSettingsCategoryCookies", IDS_SETTINGS_SITE_SETTINGS_COOKIES},
      {"siteSettingsCategoryHandlers", IDS_SETTINGS_SITE_SETTINGS_HANDLERS},
      {"siteSettingsCategoryImages", IDS_SETTINGS_SITE_SETTINGS_IMAGES},
      {"siteSettingsCategoryLocation", IDS_SETTINGS_SITE_SETTINGS_LOCATION},
      {"siteSettingsCategoryJavascript", IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT},
      {"siteSettingsCategoryMicrophone", IDS_SETTINGS_SITE_SETTINGS_MIC},
      {"siteSettingsCategoryNotifications",
       IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS},
      {"siteSettingsCategoryPopups", IDS_SETTINGS_SITE_SETTINGS_POPUPS},
      {"siteSettingsAllSites", IDS_SETTINGS_SITE_SETTINGS_ALL_SITES},
      {"siteSettingsAutomaticDownloads",
       IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOADS},
      {"siteSettingsBackgroundSync",
       IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC},
      {"siteSettingsCamera", IDS_SETTINGS_SITE_SETTINGS_CAMERA},
      {"siteSettingsCookies", IDS_SETTINGS_SITE_SETTINGS_COOKIES},
      {"siteSettingsHandlers", IDS_SETTINGS_SITE_SETTINGS_HANDLERS},
      {"siteSettingsKeygen", IDS_SETTINGS_SITE_SETTINGS_KEYGEN},
      {"siteSettingsLocation", IDS_SETTINGS_SITE_SETTINGS_LOCATION},
      {"siteSettingsMic", IDS_SETTINGS_SITE_SETTINGS_MIC},
      {"siteSettingsNotifications", IDS_SETTINGS_SITE_SETTINGS_NOTIFICATIONS},
      {"siteSettingsImages", IDS_SETTINGS_SITE_SETTINGS_IMAGES},
      {"siteSettingsJavascript", IDS_SETTINGS_SITE_SETTINGS_JAVASCRIPT},
      {"siteSettingsPlugins", IDS_SETTINGS_SITE_SETTINGS_PLUGINS},
      {"siteSettingsPopups", IDS_SETTINGS_SITE_SETTINGS_POPUPS},
      {"siteSettingsUnsandboxedPlugins",
       IDS_SETTINGS_SITE_SETTINGS_UNSANDBOXED_PLUGINS},
      {"siteSettingsUsbDevices",
       IDS_SETTINGS_SITE_SETTINGS_USB_DEVICES},
      {"siteSettingsFullscreen", IDS_SETTINGS_SITE_SETTINGS_FULLSCREEN},
      {"siteSettingsMaySaveCookies",
       IDS_SETTINGS_SITE_SETTINGS_MAY_SAVE_COOKIES},
      {"siteSettingsAskFirst", IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST},
      {"siteSettingsAskFirstRecommended",
       IDS_SETTINGS_SITE_SETTINGS_ASK_FIRST_RECOMMENDED},
      {"siteSettingsAskBeforeAccessing",
       IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_ACCESSING},
      {"siteSettingsAskBeforeAccessingRecommended",
       IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_ACCESSING_RECOMMENDED},
      {"siteSettingsAskBeforeSending",
       IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_SENDING},
      {"siteSettingsAskBeforeSendingRecommended",
       IDS_SETTINGS_SITE_SETTINGS_ASK_BEFORE_SENDING_RECOMMENDED},
      {"siteSettingsDetectAndRunImportant",
       IDS_SETTINGS_SITE_SETTINGS_PLUGINS_DETECT_IMPORTANT},
      {"siteSettingsDetectAndRunImportantRecommended",
       IDS_SETTINGS_SITE_SETTINGS_PLUGINS_DETECT_IMPORTANT_RECOMMENDED},
      {"siteSettingsLetMeChoose",
       IDS_SETTINGS_SITE_SETTINGS_PLUGINS_CHOOSE},
      {"siteSettingsAllowRecentlyClosedSites",
       IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_ALLOW_RECENTLY_CLOSED_SITES},
      {"siteSettingsAllowRecentlyClosedSitesRecommended",
       IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_ALLOW_RECENTLY_CLOSED_SITES_RECOMMENDED},
      {"siteSettingsBackgroundSyncBlocked",
       IDS_SETTINGS_SITE_SETTINGS_BACKGROUND_SYNC_BLOCKED},
      {"siteSettingsHandlersAsk",
       IDS_SETTINGS_SITE_SETTINGS_HANDLERS_ASK},
      {"siteSettingsHandlersAskRecommended",
       IDS_SETTINGS_SITE_SETTINGS_HANDLERS_ASK_RECOMMENDED},
      {"siteSettingsHandlersBlocked",
       IDS_SETTINGS_SITE_SETTINGS_HANDLERS_BLOCKED},
      {"siteSettingsKeygenAllow",
       IDS_SETTINGS_SITE_SETTINGS_KEYGEN_ALLOW},
      {"siteSettingsKeygenBlock",
       IDS_SETTINGS_SITE_SETTINGS_KEYGEN_BLOCK},
      {"siteSettingsKeygenBlockRecommended",
       IDS_SETTINGS_SITE_SETTINGS_KEYGEN_BLOCK_RECOMMENDED},
      {"siteSettingsAutoDownloadAsk",
       IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOAD_ASK},
      {"siteSettingsAutoDownloadAskRecommended",
       IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOAD_ASK_RECOMMENDED},
      {"siteSettingsAutoDownloadBlock",
       IDS_SETTINGS_SITE_SETTINGS_AUTOMATIC_DOWNLOAD_BLOCK},
      {"siteSettingsUnsandboxedPluginsAsk",
       IDS_SETTINGS_SITE_SETTINGS_UNSANDBOXED_PLUGINS_ASK},
      {"siteSettingsUnsandboxedPluginsAskRecommended",
       IDS_SETTINGS_SITE_SETTINGS_UNSANDBOXED_PLUGINS_ASK_RECOMMENDED},
      {"siteSettingsUnsandboxedPluginsBlock",
       IDS_SETTINGS_SITE_SETTINGS_UNSANDBOXED_PLUGINS_BLOCK},
      {"siteSettingsDontShowImages",
       IDS_SETTINGS_SITE_SETTINGS_DONT_SHOW_IMAGES},
      {"siteSettingsShowAll", IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL},
      {"siteSettingsShowAllRecommended",
       IDS_SETTINGS_SITE_SETTINGS_SHOW_ALL_RECOMMENDED},
      {"siteSettingsCookiesAllowed",
       IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES},
      {"siteSettingsCookiesAllowedRecommended",
       IDS_SETTINGS_SITE_SETTINGS_COOKIES_ALLOW_SITES_RECOMMENDED},
      {"siteSettingsAllow", IDS_SETTINGS_SITE_SETTINGS_ALLOW},
      {"siteSettingsBlock", IDS_SETTINGS_SITE_SETTINGS_BLOCK},
      {"siteSettingsSessionOnly", IDS_SETTINGS_SITE_SETTINGS_SESSION_ONLY},
      {"siteSettingsAllowed", IDS_SETTINGS_SITE_SETTINGS_ALLOWED},
      {"siteSettingsAllowedRecommended",
       IDS_SETTINGS_SITE_SETTINGS_ALLOWED_RECOMMENDED},
      {"siteSettingsBlocked", IDS_SETTINGS_SITE_SETTINGS_BLOCKED},
      {"siteSettingsBlockedRecommended",
       IDS_SETTINGS_SITE_SETTINGS_BLOCKED_RECOMMENDED},
      {"siteSettingsExceptions", IDS_SETTINGS_SITE_SETTINGS_EXCEPTIONS},
      {"siteSettingsAddSite", IDS_SETTINGS_SITE_SETTINGS_ADD_SITE},
      {"siteSettingsSiteUrl", IDS_SETTINGS_SITE_SETTINGS_SITE_URL},
      {"siteSettingsActionAllow", IDS_SETTINGS_SITE_SETTINGS_ALLOW_MENU},
      {"siteSettingsActionBlock", IDS_SETTINGS_SITE_SETTINGS_BLOCK_MENU},
      {"siteSettingsActionReset", IDS_SETTINGS_SITE_SETTINGS_RESET_MENU},
      {"siteSettingsActionSessionOnly",
       IDS_SETTINGS_SITE_SETTINGS_SESSION_ONLY_MENU},
      {"siteSettingsUsage", IDS_SETTINGS_SITE_SETTINGS_USAGE},
      {"siteSettingsPermissions", IDS_SETTINGS_SITE_SETTINGS_PERMISSIONS},
      {"siteSettingsClearAndReset", IDS_SETTINGS_SITE_SETTINGS_CLEAR_BUTTON},
      {"siteSettingsDelete", IDS_SETTINGS_SITE_SETTINGS_DELETE},
      {"siteSettingsCookieHeader", IDS_SETTINGS_SITE_SETTINGS_COOKIE_HEADER},
      {"siteSettingsCookieDialog", IDS_SETTINGS_SITE_SETTINGS_COOKIE_DIALOG},
      {"siteSettingsCookieRemove", IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE},
      {"siteSettingsCookieRemoveAll",
       IDS_SETTINGS_SITE_SETTINGS_COOKIE_REMOVE_ALL},
      {"thirdPartyCookie", IDS_SETTINGS_SITE_SETTINGS_THIRD_PARTY_COOKIE},
      {"thirdPartyCookieSublabel",
       IDS_SETTINGS_SITE_SETTINGS_THIRD_PARTY_COOKIE_SUBLABEL},
      {"handlerIsDefault", IDS_SETTINGS_SITE_SETTINGS_HANDLER_IS_DEFAULT},
      {"handlerSetDefault", IDS_SETTINGS_SITE_SETTINGS_HANDLER_SET_DEFAULT},
      {"handlerRemove", IDS_SETTINGS_SITE_SETTINGS_REMOVE},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

void AddUsersStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"usersPageTitle", IDS_SETTINGS_USERS},
#if defined(OS_CHROMEOS)
    {"usersModifiedByOwnerLabel", IDS_SETTINGS_USERS_MODIFIED_BY_OWNER_LABEL},
    {"guestBrowsingLabel", IDS_SETTINGS_USERS_GUEST_BROWSING_LABEL},
    {"settingsManagedLabel", IDS_SETTINGS_USERS_MANAGED_LABEL},
    {"supervisedUsersLabel", IDS_SETTINGS_USERS_SUPERVISED_USERS_LABEL},
    {"showOnSigninLabel", IDS_SETTINGS_USERS_SHOW_ON_SIGNIN_LABEL},
    {"restrictSigninLabel", IDS_SETTINGS_USERS_RESTRICT_SIGNIN_LABEL},
    {"addUsers", IDS_SETTINGS_USERS_ADD_USERS},
    {"addUsersEmail", IDS_SETTINGS_USERS_ADD_USERS_EMAIL},
#endif
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

#if !defined(OS_CHROMEOS)
void AddSystemStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
    {"systemPageTitle", IDS_SETTINGS_SYSTEM},
#if !defined(OS_MACOSX)
    {"backgroundAppsLabel", IDS_SETTINGS_SYSTEM_BACKGROUND_APPS_LABEL},
#endif
    {"hardwareAccelerationLabel",
     IDS_SETTINGS_SYSTEM_HARDWARE_ACCELERATION_LABEL},
    {"changeProxySettings", IDS_SETTINGS_SYSTEM_PROXY_SETTINGS_BUTTON},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));

  // TODO(dbeam): we should probably rename anything involving "localized
  // strings" to "load time data" as all primitive types are used now.
  SystemHandler::AddLoadTimeData(html_source);
}
#endif

void AddWebContentStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"webContent", IDS_SETTINGS_WEB_CONTENT},
      {"pageZoom", IDS_SETTINGS_PAGE_ZOOM_LABEL},
      {"fontSize", IDS_SETTINGS_FONT_SIZE_LABEL},
      {"verySmall", IDS_SETTINGS_VERY_SMALL_FONT},
      {"small", IDS_SETTINGS_SMALL_FONT},
      {"medium", IDS_SETTINGS_MEDIUM_FONT},
      {"large", IDS_SETTINGS_LARGE_FONT},
      {"veryLarge", IDS_SETTINGS_VERY_LARGE_FONT},
      {"custom", IDS_SETTINGS_CUSTOM},
      {"customizeFonts", IDS_SETTINGS_CUSTOMIZE_FONTS},
      {"fontsAndEncoding", IDS_SETTINGS_FONTS_AND_ENCODING},
      {"standardFont", IDS_SETTINGS_STANDARD_FONT_LABEL},
      {"serifFont", IDS_SETTINGS_SERIF_FONT_LABEL},
      {"sansSerifFont", IDS_SETTINGS_SANS_SERIF_FONT_LABEL},
      {"fixedWidthFont", IDS_SETTINGS_FIXED_WIDTH_FONT_LABEL},
      {"minimumFont", IDS_SETTINGS_MINIMUM_FONT_SIZE_LABEL},
      {"encoding", IDS_SETTINGS_ENCODING_LABEL},
      {"tiny", IDS_SETTINGS_TINY_FONT_SIZE},
      {"huge", IDS_SETTINGS_HUGE_FONT_SIZE},
      {"loremIpsum", IDS_SETTINGS_LOREM_IPSUM},
      {"loading", IDS_SETTINGS_LOADING},
      {"advancedFontSettings", IDS_SETTINGS_ADVANCED_FONT_SETTINGS},
      {"openAdvancedFontSettings", IDS_SETTINGS_OPEN_ADVANCED_FONT_SETTINGS},
      {"requiresWebStoreExtension", IDS_SETTINGS_REQUIRES_WEB_STORE_EXTENSION},
      {"quickBrownFox", IDS_SETTINGS_QUICK_BROWN_FOX},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}

}  // namespace

#if defined(OS_CHROMEOS)
void AddCrNetworkStrings(content::WebUIDataSource* html_source) {
  LocalizedString localized_strings[] = {
      {"networkConnecting", IDS_SETTINGS_INTERNET_NETWORK_CONNECTING},
      {"networkDisabled", IDS_SETTINGS_INTERNET_NETWORK_DISABLED},
      {"networkNotConnected", IDS_SETTINGS_INTERNET_NETWORK_NOT_CONNECTED},
      {"networkListItemConnected",
       IDS_SETTINGS_INTERNET_NETWORK_LIST_ITEM_CONNECTED},
      {"OncTypeCellular", IDS_SETTINGS_NETWORK_TYPE_CELLULAR},
      {"OncTypeEthernet", IDS_SETTINGS_NETWORK_TYPE_ETHERNET},
      {"OncTypeVPN", IDS_SETTINGS_NETWORK_TYPE_VPN},
      {"OncTypeWiFi", IDS_SETTINGS_NETWORK_TYPE_WIFI},
      {"OncTypeWiMAX", IDS_SETTINGS_NETWORK_TYPE_WIMAX},
      {"vpnNameTemplate", IDS_SETTINGS_THIRD_PARTY_VPN_NAME_TEMPLATE},
  };
  AddLocalizedStringsBulk(html_source, localized_strings,
                          arraysize(localized_strings));
}
#endif  // OS_CHROMEOS

void AddLocalizedStrings(content::WebUIDataSource* html_source,
                         Profile* profile) {
  AddCommonStrings(html_source, profile);

  AddA11yStrings(html_source);
  AddAboutStrings(html_source);
#if defined(OS_CHROMEOS)
  AddAccountUITweaksStrings(html_source, profile);
#endif
  AddAppearanceStrings(html_source);
#if defined(OS_CHROMEOS)
  AddBluetoothStrings(html_source);
#endif
#if defined(USE_NSS_CERTS)
  AddCertificateManagerStrings(html_source);
#endif
  AddClearBrowsingDataStrings(html_source);
#if !defined(OS_CHROMEOS)
  AddDefaultBrowserStrings(html_source);
#endif
  AddDateTimeStrings(html_source);
#if defined(OS_CHROMEOS)
  AddDeviceStrings(html_source);
#endif
  AddDownloadsStrings(html_source);

#if defined(OS_CHROMEOS)
  AddEasyUnlockStrings(html_source);
  AddInternetStrings(html_source);
  AddCrNetworkStrings(html_source);
#endif
  AddLanguagesStrings(html_source);
#if defined(OS_CHROMEOS)
  AddMultiProfilesStrings(html_source, profile);
#endif
  AddOnStartupStrings(html_source);
  AddPasswordsAndFormsStrings(html_source);
  AddPeopleStrings(html_source);
  AddPrintingStrings(html_source);
  AddPrivacyStrings(html_source);
  AddResetStrings(html_source);
  AddSearchEnginesStrings(html_source);
  AddSearchInSettingsStrings(html_source);
  AddSearchStrings(html_source);
  AddSiteSettingsStrings(html_source);
#if !defined(OS_CHROMEOS)
  AddSystemStrings(html_source);
#endif
  AddUsersStrings(html_source);
  AddWebContentStrings(html_source);

  policy_indicator::AddLocalizedStrings(html_source);

  html_source->SetJsonPath(kLocalizedStringsFile);
}

}  // namespace settings
