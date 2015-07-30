// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace content {
class RenderViewHost;
class WebContents;
}

// This class is responsible for deciding whether to show an SSL warning or a
// captive portal error page. It makes this decision by delaying the display of
// SSL interstitial for a few seconds (2 by default), and waiting for a captive
// portal result to arrive during this window. If a captive portal detected
// result arrives in this window, a captive portal error page is shown.
// Otherwise, an SSL interstitial is shown.
//
// An SSLErrorHandler is associated with a particular WebContents, and is
// deleted if the WebContents is destroyed, or an interstitial is displayed.
// It should only be used on the UI thread because its implementation uses
// captive_portal::CaptivePortalService which can only be accessed on the UI
// thread.
class SSLErrorHandler : public content::WebContentsUserData<SSLErrorHandler>,
                        public content::WebContentsObserver,
                        public content::NotificationObserver {
 public:
  // Type of the delay to display the SSL interstitial.
  enum InterstitialDelayType {
    NORMAL,  // Default interstitial timer delay used in production.
    NONE,    // No interstitial timer delay (i.e. zero), used in tests.
    LONG     // Very long interstitial timer delay (ie. an hour), used in tests.
  };

  static void HandleSSLError(content::WebContents* web_contents,
                             int cert_error,
                             const net::SSLInfo& ssl_info,
                             const GURL& request_url,
                             int options_mask,
                             scoped_ptr<SSLCertReporter> ssl_cert_reporter,
                             const base::Callback<void(bool)>& callback);

  static void SetInterstitialDelayTypeForTest(InterstitialDelayType delay);

  typedef base::Callback<void(content::WebContents*)> TimerStartedCallback;
  static void SetInterstitialTimerStartedCallbackForTest(
      TimerStartedCallback* callback);

 protected:
  SSLErrorHandler(content::WebContents* web_contents,
                  int cert_error,
                  const net::SSLInfo& ssl_info,
                  const GURL& request_url,
                  int options_mask,
                  scoped_ptr<SSLCertReporter> ssl_cert_reporter,
                  const base::Callback<void(bool)>& callback);

  ~SSLErrorHandler() override;

  // Called when an SSL cert error is encountered. Triggers a captive portal
  // check and fires a one shot timer to wait for a "captive portal detected"
  // result to arrive.
  void StartHandlingError();
  const base::OneShotTimer<SSLErrorHandler>& get_timer() const {
    return timer_;
  }

 private:
  // Callback for the one-shot timer. When the timer expires, an SSL error is
  // immediately displayed.
  void OnTimerExpired();

  // These are virtual for tests:
  virtual void CheckForCaptivePortal();
  virtual void ShowCaptivePortalInterstitial(const GURL& landing_url);
  virtual void ShowSSLInterstitial();

  // content::NotificationObserver:
  void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) override;

  // content::WebContentsObserver:
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;

  content::WebContents* web_contents_;
  const int cert_error_;
  const net::SSLInfo ssl_info_;
  const GURL request_url_;
  const int options_mask_;
  base::Callback<void(bool)> callback_;

  content::NotificationRegistrar registrar_;
  base::OneShotTimer<SSLErrorHandler> timer_;

  scoped_ptr<SSLCertReporter> ssl_cert_reporter_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
