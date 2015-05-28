// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interstitials/interstitial_ui.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class InterstitialHTMLSource : public content::URLDataSource {
 public:
  explicit InterstitialHTMLSource(content::WebContents* web_contents);
  ~InterstitialHTMLSource() override;

  // content::URLDataSource:
  std::string GetMimeType(const std::string& mime_type) const override;
  std::string GetSource() const override;
  bool ShouldAddContentSecurityPolicy() const override;
  void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) override;

 private:
  content::WebContents* web_contents_;
  DISALLOW_COPY_AND_ASSIGN(InterstitialHTMLSource);
};

SSLBlockingPage* CreateSSLBlockingPage(content::WebContents* web_contents) {
  // Random parameters for SSL blocking page.
  int cert_error = net::ERR_CERT_CONTAINS_ERRORS;
  GURL request_url("https://example.com");
  bool overridable = false;
  bool strict_enforcement = false;
  base::Time time_triggered_ = base::Time::NowFromSystemTime();
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "url",
                                 &url_param)) {
    if (GURL(url_param).is_valid())
      request_url = GURL(url_param);
  }
  std::string overridable_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "overridable",
                                 &overridable_param)) {
    overridable = overridable_param == "1";
  }
  std::string strict_enforcement_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "strict_enforcement",
                                 &strict_enforcement_param)) {
    strict_enforcement = strict_enforcement_param == "1";
  }
  std::string clock_manipulation_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "clock_manipulation",
                                 &clock_manipulation_param) == 1) {
    cert_error = net::ERR_CERT_DATE_INVALID;
    int time_offset;
    if (base::StringToInt(clock_manipulation_param, &time_offset)) {
      time_triggered_ += base::TimeDelta::FromDays(365 * time_offset);
    } else {
      time_triggered_ += base::TimeDelta::FromDays(365 * 2);
    }
  }
  net::SSLInfo ssl_info;
  ssl_info.cert = new net::X509Certificate(
      request_url.host(), "CA", base::Time::Max(), base::Time::Max());
  // This delegate doesn't create an interstitial.
  int options_mask = 0;
  if (overridable)
    options_mask |= SSLBlockingPage::OVERRIDABLE;
  if (strict_enforcement)
    options_mask |= SSLBlockingPage::STRICT_ENFORCEMENT;
  return new SSLBlockingPage(web_contents, cert_error, ssl_info, request_url,
                             options_mask, time_triggered_, nullptr,
                             base::Callback<void(bool)>());
}

SafeBrowsingBlockingPage* CreateSafeBrowsingBlockingPage(
    content::WebContents* web_contents) {
  SBThreatType threat_type = SB_THREAT_TYPE_URL_MALWARE;
  GURL request_url("http://example.com");
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "url",
                                 &url_param)) {
    if (GURL(url_param).is_valid())
      request_url = GURL(url_param);
  }
  std::string type_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(),
                                 "type",
                                 &type_param)) {
    if (type_param == "malware") {
      threat_type =  SB_THREAT_TYPE_URL_MALWARE;
    } else if (type_param == "phishing") {
      threat_type = SB_THREAT_TYPE_URL_PHISHING;
    } else if (type_param == "clientside_malware") {
      threat_type = SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;
    } else if (type_param == "clientside_phishing") {
      threat_type = SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL;
      // Interstitials for client side phishing urls load after the page loads
      // (see SafeBrowsingBlockingPage::IsMainPageLoadBlocked), so there should
      // either be a new navigation entry, or there shouldn't be any pending
      // entries. Clear any pending navigation entries.
      content::NavigationController* controller =
          &web_contents->GetController();
      controller->DiscardNonCommittedEntries();
    }
  }
  SafeBrowsingBlockingPage::UnsafeResource resource;
  resource.url = request_url;
  resource.threat_type =  threat_type;
  // Create a blocking page without showing the interstitial.
  return SafeBrowsingBlockingPage::CreateBlockingPage(
      g_browser_process->safe_browsing_service()->ui_manager().get(),
      web_contents,
      resource);
}

} //  namespace

InterstitialUI::InterstitialUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  scoped_ptr<InterstitialHTMLSource> html_source(
      new InterstitialHTMLSource(web_ui->GetWebContents()));
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, html_source.release());
}

InterstitialUI::~InterstitialUI() {
}

// InterstitialHTMLSource

InterstitialHTMLSource::InterstitialHTMLSource(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
}

InterstitialHTMLSource::~InterstitialHTMLSource() {
}

std::string InterstitialHTMLSource::GetMimeType(
    const std::string& mime_type) const {
  return "text/html";
}

std::string InterstitialHTMLSource::GetSource() const {
  return chrome::kChromeUIInterstitialHost;
}

bool InterstitialHTMLSource::ShouldAddContentSecurityPolicy()
    const {
  return false;
}

void InterstitialHTMLSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  scoped_ptr<content::InterstitialPageDelegate> interstitial_delegate;
  if (StartsWithASCII(path, "ssl", true)) {
    interstitial_delegate.reset(CreateSSLBlockingPage(web_contents_));
  } else if (StartsWithASCII(path, "safebrowsing", true)) {
    interstitial_delegate.reset(CreateSafeBrowsingBlockingPage(web_contents_));
  }

  std::string html;
  if (interstitial_delegate.get()) {
    html = interstitial_delegate.get()->GetHTMLContents();
  } else {
    html = ResourceBundle::GetSharedInstance()
                          .GetRawDataResource(IDR_SECURITY_INTERSTITIAL_UI_HTML)
                          .as_string();
  }
  scoped_refptr<base::RefCountedString> html_bytes = new base::RefCountedString;
  html_bytes->data().assign(html.begin(), html.end());
  callback.Run(html_bytes.get());
}
