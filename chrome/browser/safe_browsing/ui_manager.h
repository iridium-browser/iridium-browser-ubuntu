// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "content/public/browser/notification_observer.h"
#include "url/gurl.h"

class SafeBrowsingService;

namespace base {
class Thread;
}  // namespace base

namespace net {
class SSLInfo;
}  // namespace net

// Construction needs to happen on the main thread.
class SafeBrowsingUIManager
    : public base::RefCountedThreadSafe<SafeBrowsingUIManager> {
 public:
  // Passed a boolean indicating whether or not it is OK to proceed with
  // loading an URL.
  typedef base::Callback<void(bool /*proceed*/)> UrlCheckCallback;

  // Structure used to pass parameters between the IO and UI thread when
  // interacting with the blocking page.
  struct UnsafeResource {
    UnsafeResource();
    ~UnsafeResource();

    GURL url;
    GURL original_url;
    std::vector<GURL> redirect_urls;
    bool is_subresource;
    bool is_subframe;
    SBThreatType threat_type;
    std::string threat_metadata;
    UrlCheckCallback callback;  // This is called back on the IO thread.
    int render_process_host_id;
    int render_view_id;
  };

  // Observer class can be used to get notified when a SafeBrowsing hit
  // was found.
  class Observer {
   public:
    // The |resource| was classified as unsafe by SafeBrowsing.
    // This method will be called every time an unsafe resource is
    // loaded, even if it has already been whitelisted by the user.
    // The |resource| must not be accessed after OnSafeBrowsingHit returns.
    // This method will be called on the UI thread.
    virtual void OnSafeBrowsingMatch(const UnsafeResource& resource) = 0;

    // The |resource| was classified as unsafe by SafeBrowsing, and is
    // not whitelisted.
    // The |resource| must not be accessed after OnSafeBrowsingHit returns.
    // This method will be called on the UI thread.
    virtual void OnSafeBrowsingHit(const UnsafeResource& resource) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  explicit SafeBrowsingUIManager(
      const scoped_refptr<SafeBrowsingService>& service);

  // Called to stop or shutdown operations on the io_thread. This may be called
  // multiple times during the life of the UIManager. Should be called
  // on IO thread. If shutdown is true, the manager is disabled permanently.
  void StopOnIOThread(bool shutdown);

  // Called on UI thread to decide if safe browsing related stats
  // could be reported.
  virtual bool CanReportStats() const;

  // Called on the UI thread to display an interstitial page.
  // |url| is the url of the resource that matches a safe browsing list.
  // If the request contained a chain of redirects, |url| is the last url
  // in the chain, and |original_url| is the first one (the root of the
  // chain). Otherwise, |original_url| = |url|.
  virtual void DisplayBlockingPage(const UnsafeResource& resource);

  // Returns true if we already displayed an interstitial for that resource,
  // or if we should hide a UwS interstitial. Called on the UI thread.
  bool IsWhitelisted(const UnsafeResource& resource);

  // The blocking page on the UI thread has completed.
  void OnBlockingPageDone(const std::vector<UnsafeResource>& resources,
                          bool proceed);

  // Log the user perceived delay caused by SafeBrowsing. This delay is the time
  // delta starting from when we would have started reading data from the
  // network, and ending when the SafeBrowsing check completes indicating that
  // the current page is 'safe'.
  void LogPauseDelay(base::TimeDelta time);

  // Called on the IO thread by the MalwareDetails with the serialized
  // protocol buffer, so the service can send it over.
  virtual void SendSerializedMalwareDetails(const std::string& serialized);

  // Report hits to the unsafe contents (malware, phishing, unsafe download URL)
  // to the server. Can only be called on UI thread.  If |post_data| is
  // non-empty, the request will be sent as a POST instead of a GET.
  virtual void ReportSafeBrowsingHit(const GURL& malicious_url,
                                     const GURL& page_url,
                                     const GURL& referrer_url,
                                     bool is_subresource,
                                     SBThreatType threat_type,
                                     const std::string& post_data);

  // Report an invalid TLS/SSL certificate chain to the server. Can only
  // be called on UI thread.
  void ReportInvalidCertificateChain(const std::string& serialized_report,
                                     const base::Closure& callback);

  // Add and remove observers.  These methods must be invoked on the UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* remove);

 protected:
  virtual ~SafeBrowsingUIManager();

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingUIManager>;

  // Used for whitelisting a render view when the user ignores our warning.
  struct WhiteListedEntry;

  // Call protocol manager on IO thread to report hits of unsafe contents.
  void ReportSafeBrowsingHitOnIOThread(const GURL& malicious_url,
                                       const GURL& page_url,
                                       const GURL& referrer_url,
                                       bool is_subresource,
                                       SBThreatType threat_type,
                                       const std::string& post_data);

  // Sends an invalid certificate chain report over the network.
  void ReportInvalidCertificateChainOnIOThread(
      const std::string& serialized_report);

  // Adds the given entry to the whitelist.  Called on the UI thread.
  void UpdateWhitelist(const UnsafeResource& resource);

  // Safebrowsing service.
  scoped_refptr<SafeBrowsingService> sb_service_;

  // Only access this whitelist from the UI thread.
  std::vector<WhiteListedEntry> white_listed_entries_;

  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUIManager);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_
