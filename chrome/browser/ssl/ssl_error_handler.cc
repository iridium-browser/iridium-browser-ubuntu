// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include <stdint.h>
#include <utility>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/bad_clock_blocking_page.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/common/features.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ssl_errors/error_classification.h"
#include "components/ssl_errors/error_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#include "chrome/browser/ssl/captive_portal_blocking_page.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
const base::Feature kCaptivePortalInterstitial{
    "CaptivePortalInterstitial", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

const base::Feature kSSLCommonNameMismatchHandling{
    "SSLCommonNameMismatchHandling", base::FEATURE_ENABLED_BY_DEFAULT};

// Default delay in milliseconds before displaying the SSL interstitial.
// This can be changed in tests.
// - If there is a name mismatch and a suggested URL available result arrives
//   during this time, the user is redirected to the suggester URL.
// - If a "captive portal detected" result arrives during this time,
//   a captive portal interstitial is displayed.
// - Otherwise, an SSL interstitial is displayed.
const int64_t kInterstitialDelayInMilliseconds = 3000;

// Events for UMA.
enum SSLErrorHandlerEvent {
  HANDLE_ALL,
  SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE,
  SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE,
  SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE,
  SHOW_SSL_INTERSTITIAL_OVERRIDABLE,
  WWW_MISMATCH_FOUND,
  WWW_MISMATCH_URL_AVAILABLE,
  WWW_MISMATCH_URL_NOT_AVAILABLE,
  SHOW_BAD_CLOCK,
  SSL_ERROR_HANDLER_EVENT_COUNT
};

// Adds a message to console after navigation commits and then, deletes itself.
// Also deletes itself if the navigation is stopped.
class CommonNameMismatchRedirectObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<CommonNameMismatchRedirectObserver> {
 public:
  static void AddToConsoleAfterNavigation(
      content::WebContents* web_contents,
      const std::string& request_url_hostname,
      const std::string& suggested_url_hostname) {
    web_contents->SetUserData(
        UserDataKey(),
        new CommonNameMismatchRedirectObserver(
            web_contents, request_url_hostname, suggested_url_hostname));
  }

 private:
  CommonNameMismatchRedirectObserver(content::WebContents* web_contents,
                                     const std::string& request_url_hostname,
                                     const std::string& suggested_url_hostname)
      : WebContentsObserver(web_contents),
        web_contents_(web_contents),
        request_url_hostname_(request_url_hostname),
        suggested_url_hostname_(suggested_url_hostname) {}
  ~CommonNameMismatchRedirectObserver() override {}

  // WebContentsObserver:
  void NavigationStopped() override {
    // Deletes |this|.
    web_contents_->RemoveUserData(UserDataKey());
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& /* load_details */) override {
    web_contents_->GetMainFrame()->AddMessageToConsole(
        content::CONSOLE_MESSAGE_LEVEL_LOG,
        base::StringPrintf(
            "Redirecting navigation %s -> %s because the server presented a "
            "certificate valid for %s but not for %s. To disable such "
            "redirects launch Chrome with the following flag: "
            "--disable-features=SSLCommonNameMismatchHandling",
            request_url_hostname_.c_str(), suggested_url_hostname_.c_str(),
            suggested_url_hostname_.c_str(), request_url_hostname_.c_str()));
    web_contents_->RemoveUserData(UserDataKey());
  }

  void WebContentsDestroyed() override {
    web_contents_->RemoveUserData(UserDataKey());
  }

  content::WebContents* web_contents_;
  const std::string request_url_hostname_;
  const std::string suggested_url_hostname_;

  DISALLOW_COPY_AND_ASSIGN(CommonNameMismatchRedirectObserver);
};

void RecordUMA(SSLErrorHandlerEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl_error_handler", event,
                            SSL_ERROR_HANDLER_EVENT_COUNT);
}

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
bool IsCaptivePortalInterstitialEnabled() {
  return base::FeatureList::IsEnabled(kCaptivePortalInterstitial);
}
#endif

bool IsSSLCommonNameMismatchHandlingEnabled() {
  return base::FeatureList::IsEnabled(kSSLCommonNameMismatchHandling);
}

// Configuration for SSLErrorHandler.
class ConfigSingleton : public base::NonThreadSafe {
 public:
  ConfigSingleton();

  base::TimeDelta interstitial_delay() const;
  SSLErrorHandler::TimerStartedCallback* timer_started_callback() const;
  base::Clock* clock() const;
  network_time::NetworkTimeTracker* network_time_tracker() const;

  void SetInterstitialDelayForTesting(const base::TimeDelta& delay);
  void SetTimerStartedCallbackForTesting(
      SSLErrorHandler::TimerStartedCallback* callback);
  void SetClockForTesting(base::Clock* clock);
  void SetNetworkTimeTrackerForTesting(
      network_time::NetworkTimeTracker* tracker);

 private:
  base::TimeDelta interstitial_delay_;

  // Callback to call when the interstitial timer is started. Used for
  // testing.
  SSLErrorHandler::TimerStartedCallback* timer_started_callback_ = nullptr;

  // The clock to use when deciding which error type to display. Used for
  // testing.
  base::Clock* testing_clock_ = nullptr;

  network_time::NetworkTimeTracker* network_time_tracker_ = nullptr;
};

ConfigSingleton::ConfigSingleton()
    : interstitial_delay_(
          base::TimeDelta::FromMilliseconds(kInterstitialDelayInMilliseconds)) {
}

base::TimeDelta ConfigSingleton::interstitial_delay() const {
  return interstitial_delay_;
}

SSLErrorHandler::TimerStartedCallback* ConfigSingleton::timer_started_callback()
    const {
  return timer_started_callback_;
}

network_time::NetworkTimeTracker* ConfigSingleton::network_time_tracker()
    const {
  return network_time_tracker_ ? network_time_tracker_
                               : g_browser_process->network_time_tracker();
}

base::Clock* ConfigSingleton::clock() const {
  return testing_clock_;
}

void ConfigSingleton::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  interstitial_delay_ = delay;
}

void ConfigSingleton::SetTimerStartedCallbackForTesting(
    SSLErrorHandler::TimerStartedCallback* callback) {
  DCHECK(!callback || !callback->is_null());
  timer_started_callback_ = callback;
}

void ConfigSingleton::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

void ConfigSingleton::SetNetworkTimeTrackerForTesting(
    network_time::NetworkTimeTracker* tracker) {
  network_time_tracker_ = tracker;
}

static base::LazyInstance<ConfigSingleton>::Leaky g_config =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SSLErrorHandler);
DEFINE_WEB_CONTENTS_USER_DATA_KEY(CommonNameMismatchRedirectObserver);

void SSLErrorHandler::HandleSSLError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
  DCHECK(!FromWebContents(web_contents));
  SSLErrorHandler* error_handler =
      new SSLErrorHandler(web_contents, cert_error, ssl_info, request_url,
                          options_mask, std::move(ssl_cert_reporter), callback);
  web_contents->SetUserData(UserDataKey(), error_handler);
  error_handler->StartHandlingError();
}

// static
void SSLErrorHandler::SetInterstitialDelayForTesting(
    const base::TimeDelta& delay) {
  g_config.Pointer()->SetInterstitialDelayForTesting(delay);
}

// static
void SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(
    TimerStartedCallback* callback) {
  g_config.Pointer()->SetTimerStartedCallbackForTesting(callback);
}

// static
void SSLErrorHandler::SetClockForTesting(base::Clock* testing_clock) {
  g_config.Pointer()->SetClockForTesting(testing_clock);
}

// static
void SSLErrorHandler::SetNetworkTimeTrackerForTesting(
    network_time::NetworkTimeTracker* tracker) {
  g_config.Pointer()->SetNetworkTimeTrackerForTesting(tracker);
}

SSLErrorHandler::SSLErrorHandler(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(content::CertificateRequestResultType)>& callback)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      request_url_(request_url),
      options_mask_(options_mask),
      callback_(callback),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      ssl_cert_reporter_(std::move(ssl_cert_reporter)),
      weak_ptr_factory_(this) {}

SSLErrorHandler::~SSLErrorHandler() {
}

void SSLErrorHandler::StartHandlingError() {
  RecordUMA(HANDLE_ALL);

  if (ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error_) ==
      ssl_errors::ErrorInfo::CERT_DATE_INVALID) {
    HandleCertDateInvalidError();
    return;
  }

  std::vector<std::string> dns_names;
  ssl_info_.cert->GetDNSNames(&dns_names);
  DCHECK(!dns_names.empty());
  GURL suggested_url;
  if (IsSSLCommonNameMismatchHandlingEnabled() &&
      cert_error_ == net::ERR_CERT_COMMON_NAME_INVALID &&
      IsErrorOverridable() && GetSuggestedUrl(dns_names, &suggested_url)) {
    RecordUMA(WWW_MISMATCH_FOUND);
    net::CertStatus extra_cert_errors =
        ssl_info_.cert_status ^ net::CERT_STATUS_COMMON_NAME_INVALID;

    // Show the SSL intersitial if |CERT_STATUS_COMMON_NAME_INVALID| is not
    // the only error. Need not check for captive portal in this case.
    // (See the comment below).
    if (net::IsCertStatusError(extra_cert_errors) &&
        !net::IsCertStatusMinorError(ssl_info_.cert_status)) {
      ShowSSLInterstitial();
      return;
    }
    CheckSuggestedUrl(suggested_url);
    timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(), this,
                 &SSLErrorHandler::ShowSSLInterstitial);

    if (g_config.Pointer()->timer_started_callback())
      g_config.Pointer()->timer_started_callback()->Run(web_contents_);

    // Do not check for a captive portal in this case, because a captive
    // portal most likely cannot serve a valid certificate which passes the
    // similarity check.
    return;
  }

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalTabHelper* captive_portal_tab_helper =
      CaptivePortalTabHelper::FromWebContents(web_contents_);
  if (captive_portal_tab_helper) {
    captive_portal_tab_helper->OnSSLCertError(ssl_info_);
  }

  registrar_.Add(this, chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                 content::Source<Profile>(profile_));

  if (IsCaptivePortalInterstitialEnabled()) {
    CheckForCaptivePortal();
    timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(), this,
                 &SSLErrorHandler::ShowSSLInterstitial);
    if (g_config.Pointer()->timer_started_callback())
      g_config.Pointer()->timer_started_callback()->Run(web_contents_);
    return;
  }
#endif
  // Display an SSL interstitial.
  ShowSSLInterstitial();
}

void SSLErrorHandler::CheckForCaptivePortal() {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(profile_);
  captive_portal_service->DetectCaptivePortal();
#else
  NOTREACHED();
#endif
}

bool SSLErrorHandler::GetSuggestedUrl(const std::vector<std::string>& dns_names,
                                      GURL* suggested_url) const {
  return CommonNameMismatchHandler::GetSuggestedUrl(request_url_, dns_names,
                                                    suggested_url);
}

void SSLErrorHandler::CheckSuggestedUrl(const GURL& suggested_url) {
  scoped_refptr<net::URLRequestContextGetter> request_context(
      profile_->GetRequestContext());
  common_name_mismatch_handler_.reset(
      new CommonNameMismatchHandler(request_url_, request_context));

  common_name_mismatch_handler_->CheckSuggestedUrl(
      suggested_url,
      base::Bind(&SSLErrorHandler::CommonNameMismatchHandlerCallback,
                 base::Unretained(this)));
}

void SSLErrorHandler::NavigateToSuggestedURL(const GURL& suggested_url) {
  content::NavigationController::LoadURLParams load_params(suggested_url);
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  web_contents()->GetController().LoadURLWithParams(load_params);
}

bool SSLErrorHandler::IsErrorOverridable() const {
  return SSLBlockingPage::IsOverridable(options_mask_, profile_);
}

void SSLErrorHandler::ShowCaptivePortalInterstitial(const GURL& landing_url) {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Show captive portal blocking page. The interstitial owns the blocking page.
  RecordUMA(IsErrorOverridable()
                ? SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE
                : SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE);
  (new CaptivePortalBlockingPage(web_contents_, request_url_, landing_url,
                                 std::move(ssl_cert_reporter_), ssl_info_,
                                 callback_))
      ->Show();
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this". It also destroys the timer.
  web_contents_->RemoveUserData(UserDataKey());
#else
  NOTREACHED();
#endif
}

void SSLErrorHandler::ShowSSLInterstitial() {
  // Show SSL blocking page. The interstitial owns the blocking page.
  RecordUMA(IsErrorOverridable() ? SHOW_SSL_INTERSTITIAL_OVERRIDABLE
                                 : SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE);

  (SSLBlockingPage::Create(web_contents_, cert_error_, ssl_info_, request_url_,
                           options_mask_, base::Time::NowFromSystemTime(),
                           std::move(ssl_cert_reporter_), callback_))
      ->Show();
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::ShowBadClockInterstitial(
    const base::Time& now,
    ssl_errors::ClockState clock_state) {
  RecordUMA(SHOW_BAD_CLOCK);
  (new BadClockBlockingPage(web_contents_, cert_error_, ssl_info_, request_url_,
                            now, clock_state, std::move(ssl_cert_reporter_),
                            callback_))
      ->Show();
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::CommonNameMismatchHandlerCallback(
    const CommonNameMismatchHandler::SuggestedUrlCheckResult& result,
    const GURL& suggested_url) {
  timer_.Stop();
  if (result == CommonNameMismatchHandler::SuggestedUrlCheckResult::
                    SUGGESTED_URL_AVAILABLE) {
    RecordUMA(WWW_MISMATCH_URL_AVAILABLE);
    CommonNameMismatchRedirectObserver::AddToConsoleAfterNavigation(
        web_contents(), request_url_.host(), suggested_url.host());
    NavigateToSuggestedURL(suggested_url);
  } else {
    RecordUMA(WWW_MISMATCH_URL_NOT_AVAILABLE);
    ShowSSLInterstitial();
  }
}

void SSLErrorHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  DCHECK_EQ(chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT, type);

  timer_.Stop();
  CaptivePortalService::Results* results =
      content::Details<CaptivePortalService::Results>(details).ptr();
  if (results->result == captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL)
    ShowCaptivePortalInterstitial(results->landing_url);
  else
    ShowSSLInterstitial();
#endif
}

void SSLErrorHandler::DidStartNavigationToPendingEntry(
    const GURL& /* url */,
    content::ReloadType /* reload_type */) {
  // Destroy the error handler on all new navigations. This ensures that the
  // handler is properly recreated when a hanging page is navigated to an SSL
  // error, even when the tab's WebContents doesn't change.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::NavigationStopped() {
// Destroy the error handler when the page load is stopped.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::DeleteSSLErrorHandler() {
  // Need to explicity deny the certificate via the callback, otherwise memory
  // is leaked.
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_)
        .Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  }
  if (common_name_mismatch_handler_) {
    common_name_mismatch_handler_->Cancel();
    common_name_mismatch_handler_.reset();
  }
  // Deletes |this| and also destroys the timer.
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::HandleCertDateInvalidError() {
  const base::TimeTicks now = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, g_config.Pointer()->interstitial_delay(),
               base::Bind(&SSLErrorHandler::HandleCertDateInvalidErrorImpl,
                          base::Unretained(this), now));
  // Try kicking off a time fetch to get an up-to-date estimate of the
  // true time. This will only have an effect if network time is
  // unavailable or if there is not already a query in progress.
  //
  // Pass a weak pointer as the callback; if the timer fires before the
  // fetch completes and shows an interstitial, this SSLErrorHandler
  // will be deleted.
  network_time::NetworkTimeTracker* tracker =
      g_config.Pointer()->network_time_tracker();
  if (!tracker->StartTimeFetch(
          base::Bind(&SSLErrorHandler::HandleCertDateInvalidErrorImpl,
                     weak_ptr_factory_.GetWeakPtr(), now))) {
    HandleCertDateInvalidErrorImpl(now);
    return;
  }

  if (g_config.Pointer()->timer_started_callback())
    g_config.Pointer()->timer_started_callback()->Run(web_contents_);
}

void SSLErrorHandler::HandleCertDateInvalidErrorImpl(
    base::TimeTicks started_handling_error) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "interstitial.ssl_error_handler.cert_date_error_delay",
      base::TimeTicks::Now() - started_handling_error,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromSeconds(4),
      50);

  timer_.Stop();
  base::Clock* testing_clock = g_config.Pointer()->clock();
  const base::Time now =
      testing_clock ? testing_clock->Now() : base::Time::NowFromSystemTime();

  network_time::NetworkTimeTracker* tracker =
      g_config.Pointer()->network_time_tracker();
  ssl_errors::ClockState clock_state = ssl_errors::GetClockState(now, tracker);
  if (clock_state == ssl_errors::CLOCK_STATE_FUTURE ||
      clock_state == ssl_errors::CLOCK_STATE_PAST) {
    ShowBadClockInterstitial(now, clock_state);
    return;  // |this| is deleted after showing the interstitial.
  }
  ShowSSLInterstitial();
}
