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
#include "chrome/browser/ssl/bad_clock_blocking_page.h"
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

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/ssl/captive_portal_blocking_page.h"
#endif

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

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
class FakeConnectionInfoDelegate : public CaptivePortalBlockingPage::Delegate {
 public:
  FakeConnectionInfoDelegate(bool is_wifi_connection, std::string wifi_ssid)
      : is_wifi_connection_(is_wifi_connection), wifi_ssid_(wifi_ssid) {}
  ~FakeConnectionInfoDelegate() override {}

  bool IsWifiConnection() const override { return is_wifi_connection_; }
  std::string GetWiFiSSID() const override { return wifi_ssid_; }

 private:
  bool is_wifi_connection_;
  const std::string wifi_ssid_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionInfoDelegate);
};
#endif

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

BadClockBlockingPage* CreateBadClockBlockingPage(
    content::WebContents* web_contents) {
  // Set up a fake clock error.
  int cert_error = net::ERR_CERT_DATE_INVALID;
  GURL request_url("https://example.com");
  bool overridable = false;
  bool strict_enforcement = false;
  std::string url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "url", &url_param) &&
      GURL(url_param).is_valid()) {
    request_url = GURL(url_param);
  }
  std::string overridable_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "overridable",
                                 &overridable_param)) {
    overridable = overridable_param == "1";
  }
  std::string strict_enforcement_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "strict_enforcement",
                                 &strict_enforcement_param)) {
    strict_enforcement = strict_enforcement_param == "1";
  }

  // Determine whether to change the clock to be ahead or behind.
  base::Time time_triggered_ = base::Time::NowFromSystemTime();
  std::string clock_manipulation_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "clock_manipulation",
                                 &clock_manipulation_param)) {
    int time_offset;
    if (!base::StringToInt(clock_manipulation_param, &time_offset))
      time_offset = 2;
    time_triggered_ += base::TimeDelta::FromDays(365 * time_offset);
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
  return new BadClockBlockingPage(web_contents, cert_error, ssl_info,
                                  request_url, time_triggered_,
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

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
CaptivePortalBlockingPage* CreateCaptivePortalBlockingPage(
    content::WebContents* web_contents) {
  bool is_wifi_connection = false;
  GURL landing_url("https://captive.portal/login");
  GURL request_url("https://google.com");
  // Not initialized to a default value, since non-empty wifi_ssid is
  // considered a wifi connection, even if is_wifi_connection is false.
  std::string wifi_ssid;

  std::string request_url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "url",
                                 &request_url_param)) {
    if (GURL(request_url_param).is_valid())
      request_url = GURL(request_url_param);
  }
  std::string landing_url_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "landing_page",
                                 &landing_url_param)) {
    if (GURL(landing_url_param).is_valid())
      landing_url = GURL(landing_url_param);
  }
  std::string wifi_connection_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "is_wifi",
                                 &wifi_connection_param)) {
    is_wifi_connection = wifi_connection_param == "1";
  }
  std::string wifi_ssid_param;
  if (net::GetValueForKeyInQuery(web_contents->GetURL(), "wifi_name",
                                 &wifi_ssid_param)) {
    wifi_ssid = wifi_ssid_param;
  }
  FakeConnectionInfoDelegate* delegate =
      new FakeConnectionInfoDelegate(is_wifi_connection, wifi_ssid);
  net::SSLInfo ssl_info;
  ssl_info.cert = new net::X509Certificate(
      request_url.host(), "CA", base::Time::Max(), base::Time::Max());
  CaptivePortalBlockingPage* blocking_page = new CaptivePortalBlockingPage(
      web_contents, request_url, landing_url, nullptr, ssl_info,
      base::Callback<void(bool)>());
  blocking_page->SetDelegate(delegate);
  return blocking_page;
}
#endif

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
  if (base::StartsWith(path, "ssl", base::CompareCase::SENSITIVE)) {
    interstitial_delegate.reset(CreateSSLBlockingPage(web_contents_));
  } else if (base::StartsWith(path, "safebrowsing",
                              base::CompareCase::SENSITIVE)) {
    interstitial_delegate.reset(CreateSafeBrowsingBlockingPage(web_contents_));
  } else if (base::StartsWith(path, "clock", base::CompareCase::SENSITIVE)) {
    interstitial_delegate.reset(CreateBadClockBlockingPage(web_contents_));
  }
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  else if (base::StartsWith(path, "captiveportal",
                            base::CompareCase::SENSITIVE))
  {
    interstitial_delegate.reset(CreateCaptivePortalBlockingPage(web_contents_));
  }
#endif
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
