// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/chrome_translate_client.h"

#include <vector>

#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_accept_languages_factory.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/translate/content/browser/browser_cld_data_provider_factory.h"
#include "components/translate/content/common/cld_data_source.h"
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "url/gurl.h"

namespace {

// TODO(andrewhayden): Make the data file path into a gyp/gn define
// If you change this, also update standalone_cld_data_harness.cc
// accordingly!
const base::FilePath::CharType kCldDataFileName[] =
    FILE_PATH_LITERAL("cld2_data.bin");

bool g_cld_file_path_initialized_ = false;

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromeTranslateClient);

ChromeTranslateClient::ChromeTranslateClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      translate_driver_(&web_contents->GetController()),
      translate_manager_(
          new translate::TranslateManager(this, prefs::kAcceptLanguages)),
      cld_data_provider_(
          translate::BrowserCldDataProviderFactory::Get()->
            CreateBrowserCldDataProvider(web_contents)) {
  translate_driver_.AddObserver(this);
  translate_driver_.set_translate_manager(translate_manager_.get());
  // Customization: for the standalone data source, we configure the path to
  // CLD data immediately on startup.
  if (translate::CldDataSource::IsUsingStandaloneDataSource() &&
      !g_cld_file_path_initialized_) {
    DVLOG(1) << "Initializing CLD file path for the first time.";
    base::FilePath path;
    if (!PathService::Get(chrome::DIR_USER_DATA, &path)) {
      // Chrome isn't properly installed
      LOG(WARNING) << "Unable to locate user data directory";
    } else {
      g_cld_file_path_initialized_ = true;
      path = path.Append(kCldDataFileName);
      DVLOG(1) << "Setting CLD data file path: " << path.value();
      translate::CldDataSource::Get()->SetCldDataFilePath(path);
    }
  }
}

ChromeTranslateClient::~ChromeTranslateClient() {
  translate_driver_.RemoveObserver(this);
}

translate::LanguageState& ChromeTranslateClient::GetLanguageState() {
  return translate_manager_->GetLanguageState();
}

// static
scoped_ptr<translate::TranslatePrefs>
ChromeTranslateClient::CreateTranslatePrefs(PrefService* prefs) {
#if defined(OS_CHROMEOS)
  const char* preferred_languages_prefs = prefs::kLanguagePreferredLanguages;
#else
  const char* preferred_languages_prefs = NULL;
#endif
  return scoped_ptr<translate::TranslatePrefs>(new translate::TranslatePrefs(
      prefs, prefs::kAcceptLanguages, preferred_languages_prefs));
}

// static
translate::TranslateAcceptLanguages*
ChromeTranslateClient::GetTranslateAcceptLanguages(
    content::BrowserContext* browser_context) {
  return TranslateAcceptLanguagesFactory::GetForBrowserContext(browser_context);
}

// static
translate::TranslateManager* ChromeTranslateClient::GetManagerFromWebContents(
    content::WebContents* web_contents) {
  ChromeTranslateClient* chrome_translate_client =
      FromWebContents(web_contents);
  if (!chrome_translate_client)
    return NULL;
  return chrome_translate_client->GetTranslateManager();
}

// static
void ChromeTranslateClient::GetTranslateLanguages(
    content::WebContents* web_contents,
    std::string* source,
    std::string* target) {
  DCHECK(source != NULL);
  DCHECK(target != NULL);

  ChromeTranslateClient* chrome_translate_client =
      FromWebContents(web_contents);
  if (!chrome_translate_client)
    return;

  *source = translate::TranslateDownloadManager::GetLanguageCode(
      chrome_translate_client->GetLanguageState().original_language());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  Profile* original_profile = profile->GetOriginalProfile();
  PrefService* prefs = original_profile->GetPrefs();
  scoped_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefs(prefs);
  if (!web_contents->GetBrowserContext()->IsOffTheRecord()) {
    std::string auto_translate_language =
        translate::TranslateManager::GetAutoTargetLanguage(
            *source, translate_prefs.get());
    if (!auto_translate_language.empty()) {
      *target = auto_translate_language;
      return;
    }
  }

  std::string accept_languages_str = prefs->GetString(prefs::kAcceptLanguages);
  std::vector<std::string> accept_languages_list;
  base::SplitString(accept_languages_str, ',', &accept_languages_list);
  *target =
      translate::TranslateManager::GetTargetLanguage(accept_languages_list);
}

translate::TranslateManager* ChromeTranslateClient::GetTranslateManager() {
  return translate_manager_.get();
}

void ChromeTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string source_language,
    const std::string target_language,
    translate::TranslateErrors::Type error_type,
    bool triggered_from_menu) {
  DCHECK(web_contents());
  if (error_type != translate::TranslateErrors::NONE)
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;

  if (TranslateService::IsTranslateBubbleEnabled()) {
    // Bubble UI.
    if (step == translate::TRANSLATE_STEP_BEFORE_TRANSLATE) {
      // TODO(droger): Move this logic out of UI code.
      GetLanguageState().SetTranslateEnabled(true);
      if (!GetLanguageState().HasLanguageChanged())
        return;

      if (!triggered_from_menu) {
        if (web_contents()->GetBrowserContext()->IsOffTheRecord())
          return;
        if (GetTranslatePrefs()->IsTooOftenDenied(source_language))
          return;
      }
    }
    ShowBubble(step, error_type);
    return;
  }

  // Infobar UI.
  translate::TranslateInfoBarDelegate::Create(
      step != translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
      translate_manager_->GetWeakPtr(),
      InfoBarService::FromWebContents(web_contents()),
      web_contents()->GetBrowserContext()->IsOffTheRecord(),
      step,
      source_language,
      target_language,
      error_type,
      triggered_from_menu);
}

translate::TranslateDriver* ChromeTranslateClient::GetTranslateDriver() {
  return &translate_driver_;
}

PrefService* ChromeTranslateClient::GetPrefs() {
  DCHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->GetOriginalProfile()->GetPrefs();
}

scoped_ptr<translate::TranslatePrefs>
ChromeTranslateClient::GetTranslatePrefs() {
  DCHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return CreateTranslatePrefs(profile->GetPrefs());
}

translate::TranslateAcceptLanguages*
ChromeTranslateClient::GetTranslateAcceptLanguages() {
  DCHECK(web_contents());
  return GetTranslateAcceptLanguages(web_contents()->GetBrowserContext());
}

int ChromeTranslateClient::GetInfobarIconID() const {
  return IDR_INFOBAR_TRANSLATE;
}

bool ChromeTranslateClient::IsTranslatableURL(const GURL& url) {
  return TranslateService::IsTranslatableURL(url);
}

void ChromeTranslateClient::ShowReportLanguageDetectionErrorUI(
    const GURL& report_url) {
#if defined(OS_ANDROID)
  // Android does not support reporting language detection errors.
  NOTREACHED();
#else
  // We'll open the URL in a new tab so that the user can tell us more.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser) {
    NOTREACHED();
    return;
  }

  chrome::AddSelectedTabWithURL(
      browser, report_url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
#endif  // defined(OS_ANDROID)
}

bool ChromeTranslateClient::OnMessageReceived(const IPC::Message& message) {
  return cld_data_provider_->OnMessageReceived(message);
}

void ChromeTranslateClient::WebContentsDestroyed() {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with NULL WebContents.
  translate_manager_.reset();
}

// ContentTranslateDriver::Observer implementation.

void ChromeTranslateClient::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  // TODO: Remove translate notifications and have the clients be
  // ContentTranslateDriver::Observer directly instead.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::Source<content::WebContents>(web_contents()),
      content::Details<const translate::LanguageDetectionDetails>(&details));
}

void ChromeTranslateClient::OnPageTranslated(
    const std::string& original_lang,
    const std::string& translated_lang,
    translate::TranslateErrors::Type error_type) {
  // TODO: Remove translate notifications and have the clients be
  // ContentTranslateDriver::Observer directly instead.
  DCHECK(web_contents());
  translate::PageTranslatedDetails details;
  details.source_language = original_lang;
  details.target_language = translated_lang;
  details.error_type = error_type;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PAGE_TRANSLATED,
      content::Source<content::WebContents>(web_contents()),
      content::Details<translate::PageTranslatedDetails>(&details));
}

void ChromeTranslateClient::ShowBubble(
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type) {
// The bubble is implemented only on the desktop platforms.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  // |browser| might be NULL when testing. In this case, Show(...) should be
  // called because the implementation for testing is used.
  if (!browser) {
    TranslateBubbleFactory::Show(NULL, web_contents(), step, error_type);
    return;
  }

  if (web_contents() != browser->tab_strip_model()->GetActiveWebContents())
    return;

  // This ShowBubble function is also used for upating the existing bubble.
  // However, with the bubble shown, any browser windows are NOT activated
  // because the bubble takes the focus from the other widgets including the
  // browser windows. So it is checked that |browser| is the last activated
  // browser, not is now activated.
  if (browser !=
      chrome::FindLastActiveWithHostDesktopType(browser->host_desktop_type())) {
    return;
  }

  // During auto-translating, the bubble should not be shown.
  if (step == translate::TRANSLATE_STEP_TRANSLATING ||
      step == translate::TRANSLATE_STEP_AFTER_TRANSLATE) {
    if (GetLanguageState().InTranslateNavigation())
      return;
  }

  TranslateBubbleFactory::Show(
      browser->window(), web_contents(), step, error_type);
#else
  NOTREACHED();
#endif
}
