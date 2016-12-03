// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/pdf_unsupported_feature.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/grit/generated_resources.h"
#include "components/pdf/browser/open_pdf_in_reader_prompt_client.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "grit/browser_resources.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"
#endif

using base::UserMetricsAction;
using content::InterstitialPage;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using content::WebPluginInfo;

#if defined(OS_WIN)
namespace {

const char kAdobeReaderUpdateUrl[] = "http://www.adobe.com/go/getreader_chrome";

// The prompt delegate used to ask the user if they want to use Adobe Reader
// by default.
class PDFEnableAdobeReaderPromptClient
    : public pdf::OpenPDFInReaderPromptClient {
 public:
  explicit PDFEnableAdobeReaderPromptClient(Profile* profile);
  ~PDFEnableAdobeReaderPromptClient() override;

  // pdf::OpenPDFInReaderPromptClient
  base::string16 GetMessageText() const override;
  base::string16 GetAcceptButtonText() const override;
  base::string16 GetCancelButtonText() const override;
  bool ShouldExpire(
      const content::LoadCommittedDetails& details) const override;
  void Accept() override;
  void Cancel() override;

 private:
  void OnYes();
  void OnNo();

  Profile* profile_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PDFEnableAdobeReaderPromptClient);
};

PDFEnableAdobeReaderPromptClient::PDFEnableAdobeReaderPromptClient(
    Profile* profile)
    : profile_(profile) {
  content::RecordAction(UserMetricsAction("PDF_EnableReaderInfoBarShown"));
}

PDFEnableAdobeReaderPromptClient::~PDFEnableAdobeReaderPromptClient() {
}

bool PDFEnableAdobeReaderPromptClient::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  ui::PageTransition transition =
      ui::PageTransitionStripQualifier(details.entry->GetTransitionType());
  // We don't want to expire on a reload, because that is how we open the PDF in
  // Reader.
  return !details.is_in_page &&
         !ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD);
}

void PDFEnableAdobeReaderPromptClient::Accept() {
  content::RecordAction(UserMetricsAction("PDF_EnableReaderInfoBarOK"));
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile_).get();
  plugin_prefs->EnablePluginGroup(
      true, base::ASCIIToUTF16(PluginMetadata::kAdobeReaderGroupName));
  plugin_prefs->EnablePluginGroup(
      false, base::ASCIIToUTF16(ChromeContentClient::kPDFPluginName));
}

void PDFEnableAdobeReaderPromptClient::Cancel() {
  content::RecordAction(UserMetricsAction("PDF_EnableReaderInfoBarCancel"));
}

base::string16 PDFEnableAdobeReaderPromptClient::GetAcceptButtonText() const {
  return l10n_util::GetStringUTF16(IDS_PDF_INFOBAR_ALWAYS_USE_READER_BUTTON);
}

base::string16 PDFEnableAdobeReaderPromptClient::GetCancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_DONE);
}

base::string16 PDFEnableAdobeReaderPromptClient::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PDF_INFOBAR_QUESTION_ALWAYS_USE_READER);
}

// Launch the url to get the latest Adbobe Reader installer.
void OpenReaderUpdateURL(WebContents* web_contents) {
  OpenURLParams params(
      GURL(kAdobeReaderUpdateUrl), Referrer(), NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  web_contents->OpenURL(params);
}

// Opens the PDF using Adobe Reader.
void OpenUsingReader(WebContents* web_contents,
                     const WebPluginInfo& reader_plugin,
                     pdf::OpenPDFInReaderPromptClient* client) {
  ChromePluginServiceFilter::GetInstance()->OverridePluginForFrame(
      web_contents->GetRenderProcessHost()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID(),
      web_contents->GetURL(),
      reader_plugin);
  web_contents->ReloadFocusedFrame(false);

  pdf::PDFWebContentsHelper* pdf_tab_helper =
      pdf::PDFWebContentsHelper::FromWebContents(web_contents);
  if (client)
    pdf_tab_helper->ShowOpenInReaderPrompt(base::WrapUnique(client));
}

// An interstitial to be used when the user chooses to open a PDF using Adobe
// Reader, but it is out of date.
class PDFUnsupportedFeatureInterstitial
    : public content::InterstitialPageDelegate {
 public:
  PDFUnsupportedFeatureInterstitial(
      WebContents* web_contents,
      const WebPluginInfo& reader_webplugininfo)
      : web_contents_(web_contents),
        reader_webplugininfo_(reader_webplugininfo) {
    content::RecordAction(UserMetricsAction("PDF_ReaderInterstitialShown"));
    interstitial_page_ = InterstitialPage::Create(
        web_contents, false, web_contents->GetURL(), this);
    interstitial_page_->Show();
  }

 protected:
  // InterstitialPageDelegate implementation.
  std::string GetHTMLContents() override {
    base::DictionaryValue strings;
    strings.SetString(
        "title",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_TITLE));
    strings.SetString(
        "headLine",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_BODY));
    strings.SetString(
        "update",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_UPDATE));
    strings.SetString(
        "open_with_reader",
        l10n_util::GetStringUTF16(
            IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_PROCEED));
    strings.SetString(
        "ok",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_OK));
    strings.SetString(
        "cancel",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_CANCEL));

    base::StringPiece html(ResourceBundle::GetSharedInstance().
                           GetRawDataResource(IDR_READER_OUT_OF_DATE_HTML));

    return webui::GetI18nTemplateHtml(html, &strings);
  }

  void CommandReceived(const std::string& command) override {
    if (command == "0") {
      content::RecordAction(
          UserMetricsAction("PDF_ReaderInterstitialCancel"));
      interstitial_page_->DontProceed();
      return;
    }

    if (command == "1") {
      content::RecordAction(
          UserMetricsAction("PDF_ReaderInterstitialUpdate"));
      OpenReaderUpdateURL(web_contents_);
    } else if (command == "2") {
      content::RecordAction(
          UserMetricsAction("PDF_ReaderInterstitialIgnore"));
      // Pretend that the plugin is up to date so that we don't block it.
      reader_webplugininfo_.version = base::ASCIIToUTF16("11.0.0.0");
      OpenUsingReader(web_contents_, reader_webplugininfo_, NULL);
    } else {
      NOTREACHED();
    }
    interstitial_page_->Proceed();
  }

  void OverrideRendererPrefs(content::RendererPreferences* prefs) override {
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    renderer_preferences_util::UpdateFromSystemSettings(
        prefs, profile, web_contents_);
  }

 private:
  WebContents* web_contents_;
  WebPluginInfo reader_webplugininfo_;
  InterstitialPage* interstitial_page_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(PDFUnsupportedFeatureInterstitial);
};

// The delegate for the bubble used to inform the user that we don't support a
// feature in the PDF.
class PDFUnsupportedFeaturePromptClient
    : public pdf::OpenPDFInReaderPromptClient {
 public:
  PDFUnsupportedFeaturePromptClient(WebContents* web_contents,
                                    const AdobeReaderPluginInfo& reader_info);
  ~PDFUnsupportedFeaturePromptClient() override;

  // pdf::OpenPDFInReaderPromptClient:
  base::string16 GetMessageText() const override;
  base::string16 GetAcceptButtonText() const override;
  base::string16 GetCancelButtonText() const override;
  bool ShouldExpire(
      const content::LoadCommittedDetails& details) const override;
  void Accept() override;
  void Cancel() override;

 private:
  WebContents* web_contents_;
  const AdobeReaderPluginInfo reader_info_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PDFUnsupportedFeaturePromptClient);
};

PDFUnsupportedFeaturePromptClient::PDFUnsupportedFeaturePromptClient(
    WebContents* web_contents,
    const AdobeReaderPluginInfo& reader_info)
    : web_contents_(web_contents), reader_info_(reader_info) {
  content::RecordAction(reader_info_.is_installed ?
                        UserMetricsAction("PDF_UseReaderInfoBarShown") :
                        UserMetricsAction("PDF_InstallReaderInfoBarShown"));
}

PDFUnsupportedFeaturePromptClient::~PDFUnsupportedFeaturePromptClient() {
}

base::string16 PDFUnsupportedFeaturePromptClient::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PDF_BUBBLE_MESSAGE);
}

base::string16 PDFUnsupportedFeaturePromptClient::GetAcceptButtonText() const {
  return l10n_util::GetStringUTF16(
      reader_info_.is_installed ? IDS_PDF_BUBBLE_OPEN_IN_READER_LINK
                                : IDS_PDF_BUBBLE_INSTALL_READER_LINK);
}

base::string16 PDFUnsupportedFeaturePromptClient::GetCancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_DONE);
}

bool PDFUnsupportedFeaturePromptClient::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  return !details.is_in_page;
}

void PDFUnsupportedFeaturePromptClient::Accept() {
  if (!reader_info_.is_installed) {
    content::RecordAction(UserMetricsAction("PDF_InstallReaderInfoBarOK"));
    OpenReaderUpdateURL(web_contents_);
    return;
  }

  content::RecordAction(UserMetricsAction("PDF_UseReaderInfoBarOK"));

  if (!reader_info_.is_secure) {
    new PDFUnsupportedFeatureInterstitial(web_contents_,
                                          reader_info_.plugin_info);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  pdf::OpenPDFInReaderPromptClient* client =
      new PDFEnableAdobeReaderPromptClient(profile);

  OpenUsingReader(web_contents_, reader_info_.plugin_info, client);
}

void PDFUnsupportedFeaturePromptClient::Cancel() {
  content::RecordAction(reader_info_.is_installed ?
                        UserMetricsAction("PDF_UseReaderInfoBarCancel") :
                        UserMetricsAction("PDF_InstallReaderInfoBarCancel"));
}

void MaybeShowOpenPDFInReaderPrompt(WebContents* web_contents,
                                    const AdobeReaderPluginInfo& reader_info) {
  // If the Reader plugin is disabled by policy, don't prompt them.
  if (!reader_info.is_installed || !reader_info.is_enabled)
    return;

  std::unique_ptr<pdf::OpenPDFInReaderPromptClient> prompt(
      new PDFUnsupportedFeaturePromptClient(web_contents, reader_info));
  pdf::PDFWebContentsHelper* pdf_tab_helper =
      pdf::PDFWebContentsHelper::FromWebContents(web_contents);
  pdf_tab_helper->ShowOpenInReaderPrompt(std::move(prompt));
}

void GotPluginsCallback(int process_id,
                        int routing_id,
                        const AdobeReaderPluginInfo& reader_info) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(process_id, routing_id);
  if (web_contents)
    MaybeShowOpenPDFInReaderPrompt(web_contents, reader_info);
}

}  // namespace
#endif  // defined(OS_WIN)

void PDFHasUnsupportedFeature(WebContents* web_contents) {
#if defined(OS_WIN)
  // Only works for Windows for now.  For Mac, we'll have to launch the file
  // externally since Adobe Reader doesn't work inside Chrome.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  AdobeReaderPluginInfo reader_info;
  if (GetAdobeReaderPluginInfo(profile, &reader_info)) {
    MaybeShowOpenPDFInReaderPrompt(web_contents, reader_info);
    return;
  }
  GetAdobeReaderPluginInfoAsync(
      profile,
      base::Bind(&GotPluginsCallback,
                 web_contents->GetRenderProcessHost()->GetID(),
                 web_contents->GetRenderViewHost()->GetRoutingID()));
#endif  // defined(OS_WIN)
}
