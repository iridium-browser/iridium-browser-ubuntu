// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ui_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/debug/leak_tracker.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/threat_details.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing_db/metadata.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::HitReport;

namespace {

const void* const kWhitelistKey = &kWhitelistKey;

// A WhitelistUrlSet holds the set of URLs that have been whitelisted for a
// specific WebContents, along with pending entries that are still undecided.
class WhitelistUrlSet : public base::SupportsUserData::Data {
 public:
  WhitelistUrlSet() {}

  bool Contains(const GURL url) {
    return set_.find(url.GetWithEmptyPath()) != set_.end();
  }

  void Insert(const GURL url) {
    set_.insert(url.GetWithEmptyPath());
    pending_.erase(url.GetWithEmptyPath());
  }

  bool ContainsPending(const GURL url) {
    return pending_.find(url.GetWithEmptyPath()) != pending_.end();
  }

  void InsertPending(const GURL url) {
    pending_.insert(url.GetWithEmptyPath());
  }

 private:
  std::set<GURL> set_;
  std::set<GURL> pending_;

  DISALLOW_COPY_AND_ASSIGN(WhitelistUrlSet);
};

}  // namespace

namespace safe_browsing {

// SafeBrowsingUIManager::UnsafeResource ---------------------------------------

SafeBrowsingUIManager::UnsafeResource::UnsafeResource()
    : is_subresource(false),
      threat_type(SB_THREAT_TYPE_SAFE),
      threat_source(safe_browsing::ThreatSource::UNKNOWN) {}

SafeBrowsingUIManager::UnsafeResource::UnsafeResource(
    const UnsafeResource& other) = default;

SafeBrowsingUIManager::UnsafeResource::~UnsafeResource() { }

bool SafeBrowsingUIManager::UnsafeResource::IsMainPageLoadBlocked() const {
  // Subresource hits cannot happen until after main page load is committed.
  if (is_subresource)
    return false;

  // Client-side phishing detection interstitials never block the main frame
  // load, since they happen after the page is finished loading.
  if (threat_type == SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL ||
      threat_type == SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL) {
    return false;
  }

  return true;
}

content::NavigationEntry*
SafeBrowsingUIManager::UnsafeResource::GetNavigationEntryForResource() const {
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return nullptr;
  // If a safebrowsing hit occurs during main frame navigation, the navigation
  // will not be committed, and the pending navigation entry refers to the hit.
  if (IsMainPageLoadBlocked())
    return web_contents->GetController().GetPendingEntry();
  // If a safebrowsing hit occurs on a subresource load, or on a main frame
  // after the navigation is committed, the last committed navigation entry
  // refers to the page with the hit. Note that there may concurrently be an
  // unrelated pending navigation to another site, so GetActiveEntry() would be
  // wrong.
  return web_contents->GetController().GetLastCommittedEntry();
}

// static
base::Callback<content::WebContents*(void)>
SafeBrowsingUIManager::UnsafeResource::GetWebContentsGetter(
    int render_process_host_id,
    int render_frame_id) {
  return base::Bind(&tab_util::GetWebContentsByFrameID, render_process_host_id,
                    render_frame_id);
}

// SafeBrowsingUIManager -------------------------------------------------------

SafeBrowsingUIManager::SafeBrowsingUIManager(
    const scoped_refptr<SafeBrowsingService>& service)
    : sb_service_(service) {}

SafeBrowsingUIManager::~SafeBrowsingUIManager() {}

void SafeBrowsingUIManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (shutdown)
    sb_service_ = NULL;
}

void SafeBrowsingUIManager::LogPauseDelay(base::TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SB2.Delay", time);
}

void SafeBrowsingUIManager::OnBlockingPageDone(
    const std::vector<UnsafeResource>& resources,
    bool proceed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& resource : resources) {
    if (!resource.callback.is_null()) {
      DCHECK(resource.callback_thread);
      resource.callback_thread->PostTask(
          FROM_HERE, base::Bind(resource.callback, proceed));
    }

    if (proceed)
      AddToWhitelistUrlSet(resource, false /* Pending -> permanent */);
  }
}

void SafeBrowsingUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (resource.is_subresource && !resource.is_subframe) {
    // Sites tagged as serving Unwanted Software should only show a warning for
    // main-frame or sub-frame resource. Similar warning restrictions should be
    // applied to malware sites tagged as "landing sites" (see "Types of
    // Malware sites" under
    // https://developers.google.com/safe-browsing/developers_guide_v3#UserWarnings).
    MalwarePatternType proto;
    if (resource.threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
        (resource.threat_type == SB_THREAT_TYPE_URL_MALWARE &&
         resource.threat_metadata.threat_pattern_type ==
             ThreatPatternType::MALWARE_LANDING)) {
      if (!resource.callback.is_null()) {
        DCHECK(resource.callback_thread);
        resource.callback_thread->PostTask(FROM_HERE,
                                           base::Bind(resource.callback, true));
      }

      return;
    }
  }

  // The tab might have been closed. If it was closed, just act as if "Don't
  // Proceed" had been chosen.
  WebContents* web_contents = resource.web_contents_getter.Run();
  if (!web_contents) {
    std::vector<UnsafeResource> resources;
    resources.push_back(resource);
    OnBlockingPageDone(resources, false);
    return;
  }

  // Check if the user has already ignored a SB warning for the same WebContents
  // and top-level domain.
  if (IsWhitelisted(resource)) {
    if (!resource.callback.is_null()) {
      DCHECK(resource.callback_thread);
      resource.callback_thread->PostTask(FROM_HERE,
                                         base::Bind(resource.callback, true));
    }
    return;
  }

  if (resource.threat_type != SB_THREAT_TYPE_SAFE) {
    HitReport hit_report;
    hit_report.malicious_url = resource.url;
    hit_report.is_subresource = resource.is_subresource;
    hit_report.threat_type = resource.threat_type;
    hit_report.threat_source = resource.threat_source;
    hit_report.population_id = resource.threat_metadata.population_id;

    NavigationEntry* entry = resource.GetNavigationEntryForResource();
    if (entry) {
      hit_report.page_url = entry->GetURL();
      hit_report.referrer_url = entry->GetReferrer().url;
    }

    // When the malicious url is on the main frame, and resource.original_url
    // is not the same as the resource.url, that means we have a redirect from
    // resource.original_url to resource.url.
    // Also, at this point, page_url points to the _previous_ page that we
    // were on. We replace page_url with resource.original_url and referrer
    // with page_url.
    if (!resource.is_subresource &&
        !resource.original_url.is_empty() &&
        resource.original_url != resource.url) {
      hit_report.referrer_url = hit_report.page_url;
      hit_report.page_url = resource.original_url;
    }

    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    hit_report.is_extended_reporting =
        profile &&
        profile->GetPrefs()->GetBoolean(
            prefs::kSafeBrowsingExtendedReportingEnabled);
    hit_report.is_metrics_reporting_active =
        ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();

    MaybeReportSafeBrowsingHit(hit_report);
  }

  if (resource.threat_type != SB_THREAT_TYPE_SAFE) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnSafeBrowsingHit(resource));
  }
  AddToWhitelistUrlSet(resource, true /* A decision is now pending */);
  SafeBrowsingBlockingPage::ShowBlockingPage(this, resource);
}

// A safebrowsing hit is sent after a blocking page for malware/phishing
// or after the warning dialog for download urls, only for
// UMA || extended_reporting users.
void SafeBrowsingUIManager::MaybeReportSafeBrowsingHit(
    const HitReport& hit_report) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Send report if user opted-in extended reporting.
  if (hit_report.is_extended_reporting) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingUIManager::ReportSafeBrowsingHitOnIOThread,
                   this, hit_report));
  }
}

void SafeBrowsingUIManager::ReportSafeBrowsingHitOnIOThread(
    const HitReport& hit_report) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (!sb_service_ || !sb_service_->ping_manager())
    return;

  DVLOG(1) << "ReportSafeBrowsingHit: " << hit_report.malicious_url << " "
           << hit_report.page_url << " " << hit_report.referrer_url << " "
           << hit_report.is_subresource << " " << hit_report.threat_type;
  sb_service_->ping_manager()->ReportSafeBrowsingHit(hit_report);
}

void SafeBrowsingUIManager::ReportInvalidCertificateChain(
    const std::string& serialized_report,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SafeBrowsingUIManager::ReportInvalidCertificateChainOnIOThread, this,
          serialized_report),
      callback);
}

void SafeBrowsingUIManager::ReportPermissionAction(
    const PermissionReportInfo& report_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeBrowsingUIManager::ReportPermissionActionOnIOThread, this,
                 report_info));
}

void SafeBrowsingUIManager::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.AddObserver(observer);
}

void SafeBrowsingUIManager::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void SafeBrowsingUIManager::ReportInvalidCertificateChainOnIOThread(
    const std::string& serialized_report) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (!sb_service_ || !sb_service_->ping_manager())
    return;

  sb_service_->ping_manager()->ReportInvalidCertificateChain(serialized_report);
}

void SafeBrowsingUIManager::ReportPermissionActionOnIOThread(
    const PermissionReportInfo& report_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (!sb_service_ || !sb_service_->ping_manager())
    return;

  sb_service_->ping_manager()->ReportPermissionAction(report_info);
}

// If the user had opted-in to send ThreatDetails, this gets called
// when the report is ready.
void SafeBrowsingUIManager::SendSerializedThreatDetails(
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The service may delete the ping manager (i.e. when user disabling service,
  // etc). This happens on the IO thread.
  if (sb_service_.get() == NULL || sb_service_->ping_manager() == NULL)
    return;

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized threat details.";
    sb_service_->ping_manager()->ReportThreatDetails(serialized);
  }
}

// Record this domain in the current WebContents as either whitelisted or
// pending whitelisting (if an interstitial is currently displayed). If an
// existing WhitelistUrlSet does not yet exist, create a new WhitelistUrlSet.
void SafeBrowsingUIManager::AddToWhitelistUrlSet(const UnsafeResource& resource,
                                                 bool pending) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = resource.web_contents_getter.Run();
  WhitelistUrlSet* site_list =
      static_cast<WhitelistUrlSet*>(web_contents->GetUserData(kWhitelistKey));
  if (!site_list) {
    site_list = new WhitelistUrlSet;
    web_contents->SetUserData(kWhitelistKey, site_list);
  }

  GURL whitelisted_url;
  if (resource.is_subresource) {
    NavigationEntry* entry = resource.GetNavigationEntryForResource();
    if (!entry)
      return;
    whitelisted_url = entry->GetURL();
  } else {
    whitelisted_url = resource.url;
  }

  if (pending) {
    site_list->InsertPending(whitelisted_url);
  } else {
    site_list->Insert(whitelisted_url);
  }
}

bool SafeBrowsingUIManager::IsWhitelisted(const UnsafeResource& resource) {
  NavigationEntry* entry = nullptr;
  if (resource.is_subresource) {
    entry = resource.GetNavigationEntryForResource();
  }
  return IsUrlWhitelistedOrPendingForWebContents(
      resource.url, resource.is_subresource, entry,
      resource.web_contents_getter.Run(), true);
}

// Check if the user has already seen and/or ignored a SB warning for this
// WebContents and top-level domain.
bool SafeBrowsingUIManager::IsUrlWhitelistedOrPendingForWebContents(
    const GURL& url,
    bool is_subresource,
    NavigationEntry* entry,
    content::WebContents* web_contents,
    bool whitelist_only) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL lookup_url;
  if (is_subresource) {
    if (!entry)
      return false;
    lookup_url = entry->GetURL();
  } else {
    lookup_url = url;
  }

  WhitelistUrlSet* site_list =
      static_cast<WhitelistUrlSet*>(web_contents->GetUserData(kWhitelistKey));
  if (!site_list)
    return false;

  bool whitelisted = site_list->Contains(lookup_url);
  if (whitelist_only) {
    return whitelisted;
  } else {
    return whitelisted || site_list->ContainsPending(lookup_url);
  }
}

}  // namespace safe_browsing
