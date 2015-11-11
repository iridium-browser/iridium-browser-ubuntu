// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/resource_type.h"

class ResourceDispatcherHost;

namespace net {
class URLRequest;
}

// SafeBrowsingResourceThrottle checks that URLs are "safe" before
// navigating to them. To be considered "safe", a URL must not appear in the
// malware/phishing blacklists (see SafeBrowsingService for details).
//
// On desktop (ifdef SAFE_BROWSING_DB_LOCAL)
// -----------------------------------------
// This check is done before requesting the original URL, and additionally
// before following any subsequent redirect.  In the common case the check
// completes synchronously (no match in the in-memory DB), so the request's
// flow is un-interrupted.  However if the URL fails this quick check, it
// has the possibility of being on the blacklist. Now the request is
// deferred (prevented from starting), and a more expensive safe browsing
// check is begun (fetches the full hashes).
//
// On mobile (ifdef SAFE_BROWSING_DB_REMOTE):
// -----------------------------------------
// The check is started and runs in parallel with the resource load.  If the
// check is not complete by the time the headers are loaded, the request is
// suspended until the URL is classified.  We let the headers load on mobile
// since the RemoteSafeBrowsingDatabase checks always have some non-zero
// latency -- there no synchronous pass.  This parallelism helps
// performance.  Redirects are handled the same way as desktop so they
// always defer.
//
//
// Note that the safe browsing check takes at most kCheckUrlTimeoutMs
// milliseconds. If it takes longer than this, then the system defaults to
// treating the URL as safe.
//
// If the URL is classified as dangerous, a warning page is thrown up and
// the request remains suspended.  If the user clicks "proceed" on warning
// page, we resume the request.
//
// Note: The ResourceThrottle interface is called in this order:
// WillStartRequest once, WillRedirectRequest zero or more times, and then
// WillProcessReponse once.
class SafeBrowsingResourceThrottle
    : public content::ResourceThrottle,
      public SafeBrowsingDatabaseManager::Client,
      public base::SupportsWeakPtr<SafeBrowsingResourceThrottle> {
 public:
  // Will construct a SafeBrowsingResourceThrottle, or return NULL
  // if on Android and not in the field trial.
  static SafeBrowsingResourceThrottle* MaybeCreate(
      net::URLRequest* request,
      content::ResourceType resource_type,
      SafeBrowsingService* sb_service);

  // content::ResourceThrottle implementation (called on IO thread):
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;

  const char* GetNameForLogging() const override;

  // SafeBrowsingDabaseManager::Client implementation (called on IO thread):
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType result,
                              const std::string& metadata) override;

 protected:
  enum DeferAtStartSetting {
    DEFER_AT_START,
    DONT_DEFER_AT_START
  };

  SafeBrowsingResourceThrottle(
      const net::URLRequest* request,
      content::ResourceType resource_type,
      SafeBrowsingService* sb_service,
      DeferAtStartSetting defer_setting,
      SafeBrowsingService::ResourceTypesToCheck types_to_check);

 private:
  // Describes what phase of the check a throttle is in.
  enum State {
    // Haven't started checking or checking is complete. Not deferred.
    STATE_NONE,
    // We have one outstanding URL-check. Could be deferred.
    STATE_CHECKING_URL,
    // We're displaying a blocking page. Could be deferred.
    STATE_DISPLAYING_BLOCKING_PAGE,
  };

  // Describes what stage of the request got paused by the check.
  enum DeferState {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_REDIRECT,
    DEFERRED_UNCHECKED_REDIRECT,  // unchecked_redirect_url_ is populated.
    DEFERRED_PROCESSING,
  };

  ~SafeBrowsingResourceThrottle() override;

  // SafeBrowsingService::UrlCheckCallback implementation.
  void OnBlockingPageComplete(bool proceed);

  // Starts running |url| through the safe browsing check. Returns true if the
  // URL is safe to visit. Otherwise returns false and will call
  // OnBrowseUrlResult() when the check has completed.
  bool CheckUrl(const GURL& url);

  // Callback for when the safe browsing check (which was initiated by
  // StartCheckingUrl()) has taken longer than kCheckUrlTimeoutMs.
  void OnCheckUrlTimeout();

  // Starts displaying the safe browsing interstitial page if it's not
  // prerendering. Called on the UI thread.
  static void StartDisplayingBlockingPage(
      const base::WeakPtr<SafeBrowsingResourceThrottle>& throttle,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      const SafeBrowsingUIManager::UnsafeResource& resource);

  // Called on the IO thread if the request turned out to be for a prerendered
  // page.
  void Cancel();

  // Resumes the request, by continuing the deferred action (either starting the
  // request, or following a redirect).
  void ResumeRequest();

  // True if we want to block the starting of requests until they're
  // deemed safe.  Otherwise we let the resource partially load.
  const bool defer_at_start_;

  // Check all types, or just the dangerous ones?
  const SafeBrowsingService::ResourceTypesToCheck resource_types_to_check_;

  State state_;
  DeferState defer_state_;

  // The result of the most recent safe browsing check. Only valid to read this
  // when state_ != STATE_CHECKING_URL.
  SBThreatType threat_type_;

  // The time when we started deferring the request.
  base::TimeTicks defer_start_time_;

  // Timer to abort the safe browsing check if it takes too long.
  base::OneShotTimer<SafeBrowsingResourceThrottle> timer_;

  // The redirect chain for this resource
  std::vector<GURL> redirect_urls_;

  // If in DEFERRED_UNCHECKED_REDIRECT state, this is the
  // URL we still need to check before resuming.
  GURL unchecked_redirect_url_;
  GURL url_being_checked_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  const net::URLRequest* request_;
  const content::ResourceType resource_type_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingResourceThrottle);
};
#endif  // CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_THROTTLE_H_
