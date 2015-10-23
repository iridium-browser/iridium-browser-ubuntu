// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover.h"

#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/storage_partition_http_cache_data_remover.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/domain_reliability/service.h"
#include "components/history/core/browser/history_service.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/pnacl_host.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/power/origin_power_map.h"
#include "components/power/origin_power_map_factory.h"
#include "components/search_engines/template_url_service.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_data_remover.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_store.h"
#include "net/http/transport_security_state.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/precache/precache_manager_factory.h"
#include "components/precache/content/precache_manager.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/attestation/attestation_constants.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_manager/user.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/apps/ephemeral_app_service.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "extensions/browser/extension_prefs.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "chrome/browser/media/webrtc_log_list.h"
#include "chrome/browser/media/webrtc_log_util.h"
#endif

using base::UserMetricsAction;
using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;

namespace {

using CallbackList =
    base::CallbackList<void(const BrowsingDataRemover::NotificationDetails&)>;

// Contains all registered callbacks for browsing data removed notifications.
CallbackList* g_on_browsing_data_removed_callbacks = nullptr;

// Accessor for |*g_on_browsing_data_removed_callbacks|. Creates a new object
// the first time so that it always returns a valid object.
CallbackList* GetOnBrowsingDataRemovedCallbacks() {
  if (!g_on_browsing_data_removed_callbacks)
    g_on_browsing_data_removed_callbacks = new CallbackList();
  return g_on_browsing_data_removed_callbacks;
}

}  // namespace

bool BrowsingDataRemover::is_removing_ = false;

BrowsingDataRemover::CompletionInhibitor*
    BrowsingDataRemover::completion_inhibitor_ = nullptr;

// Helper to create callback for BrowsingDataRemover::DoesOriginMatchMask.
// Static.
bool DoesOriginMatchMask(
    int origin_type_mask,
    const GURL& origin,
    storage::SpecialStoragePolicy* special_storage_policy) {
  return BrowsingDataHelper::DoesOriginMatchMask(
      origin, origin_type_mask, special_storage_policy);
}

BrowsingDataRemover::NotificationDetails::NotificationDetails()
    : removal_begin(base::Time()),
      removal_mask(-1),
      origin_type_mask(-1) {
}

BrowsingDataRemover::NotificationDetails::NotificationDetails(
    const BrowsingDataRemover::NotificationDetails& details)
    : removal_begin(details.removal_begin),
      removal_mask(details.removal_mask),
      origin_type_mask(details.origin_type_mask) {
}

BrowsingDataRemover::NotificationDetails::NotificationDetails(
    base::Time removal_begin,
    int removal_mask,
    int origin_type_mask)
    : removal_begin(removal_begin),
      removal_mask(removal_mask),
      origin_type_mask(origin_type_mask) {
}

BrowsingDataRemover::NotificationDetails::~NotificationDetails() {}

// Static.
BrowsingDataRemover* BrowsingDataRemover::CreateForUnboundedRange(
    Profile* profile) {
  return new BrowsingDataRemover(profile, base::Time(), base::Time::Max());
}

// Static.
BrowsingDataRemover* BrowsingDataRemover::CreateForRange(Profile* profile,
    base::Time start, base::Time end) {
  return new BrowsingDataRemover(profile, start, end);
}

// Static.
BrowsingDataRemover* BrowsingDataRemover::CreateForPeriod(Profile* profile,
    TimePeriod period) {
  switch (period) {
    case LAST_HOUR:
      content::RecordAction(
          UserMetricsAction("ClearBrowsingData_LastHour"));
      break;
    case LAST_DAY:
      content::RecordAction(
          UserMetricsAction("ClearBrowsingData_LastDay"));
      break;
    case LAST_WEEK:
      content::RecordAction(
          UserMetricsAction("ClearBrowsingData_LastWeek"));
      break;
    case FOUR_WEEKS:
      content::RecordAction(
          UserMetricsAction("ClearBrowsingData_LastMonth"));
      break;
    case EVERYTHING:
      content::RecordAction(
          UserMetricsAction("ClearBrowsingData_Everything"));
      break;
  }
  return new BrowsingDataRemover(profile,
      BrowsingDataRemover::CalculateBeginDeleteTime(period),
      base::Time::Max());
}

BrowsingDataRemover::BrowsingDataRemover(Profile* profile,
                                         base::Time delete_begin,
                                         base::Time delete_end)
    : profile_(profile),
      delete_begin_(delete_begin),
      delete_end_(delete_end),
      main_context_getter_(profile->GetRequestContext()),
      media_context_getter_(profile->GetMediaRequestContext()) {
  DCHECK(profile);
  // crbug.com/140910: Many places were calling this with base::Time() as
  // delete_end, even though they should've used base::Time::Max(). Work around
  // it here. New code should use base::Time::Max().
  DCHECK(delete_end_ != base::Time());
  if (delete_end_ == base::Time())
    delete_end_ = base::Time::Max();
}

BrowsingDataRemover::~BrowsingDataRemover() {
  DCHECK(AllDone());
}

// Static.
void BrowsingDataRemover::set_removing(bool is_removing) {
  DCHECK(is_removing_ != is_removing);
  is_removing_ = is_removing;
}

void BrowsingDataRemover::Remove(int remove_mask, int origin_type_mask) {
  RemoveImpl(remove_mask, GURL(), origin_type_mask);
}

void BrowsingDataRemover::RemoveImpl(int remove_mask,
                                     const GURL& remove_url,
                                     int origin_type_mask) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  set_removing(true);
  remove_mask_ = remove_mask;
  origin_type_mask_ = origin_type_mask;
  url::Origin remove_origin(remove_url);

  PrefService* prefs = profile_->GetPrefs();
  bool may_delete_history = prefs->GetBoolean(
      prefs::kAllowDeletingBrowserHistory);

  // All the UI entry points into the BrowsingDataRemover should be disabled,
  // but this will fire if something was missed or added.
  DCHECK(may_delete_history || (remove_mask & REMOVE_NOCHECKS) ||
      (!(remove_mask & REMOVE_HISTORY) && !(remove_mask & REMOVE_DOWNLOADS)));

  if (origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsUnprotectedWeb"));
  }
  if (origin_type_mask_ & BrowsingDataHelper::PROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsProtectedWeb"));
  }
  if (origin_type_mask_ & BrowsingDataHelper::EXTENSION) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsExtension"));
  }
  // If this fires, we added a new BrowsingDataHelper::OriginTypeMask without
  // updating the user metrics above.
  static_assert(
      BrowsingDataHelper::ALL == (BrowsingDataHelper::UNPROTECTED_WEB |
                                  BrowsingDataHelper::PROTECTED_WEB |
                                  BrowsingDataHelper::EXTENSION),
      "OriginTypeMask has been updated without updating user metrics");

  if ((remove_mask & REMOVE_HISTORY) && may_delete_history) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (history_service) {
      std::set<GURL> restrict_urls;
      if (!remove_url.is_empty())
        restrict_urls.insert(remove_url);
      content::RecordAction(UserMetricsAction("ClearBrowsingData_History"));
      waiting_for_clear_history_ = true;

      history_service->ExpireLocalAndRemoteHistoryBetween(
          WebHistoryServiceFactory::GetForProfile(profile_),
          restrict_urls, delete_begin_, delete_end_,
          base::Bind(&BrowsingDataRemover::OnHistoryDeletionDone,
                     base::Unretained(this)),
          &history_task_tracker_);

#if defined(ENABLE_EXTENSIONS)
      // The extension activity contains details of which websites extensions
      // were active on. It therefore indirectly stores details of websites a
      // user has visited so best clean from here as well.
      extensions::ActivityLog::GetInstance(profile_)->RemoveURLs(restrict_urls);
#endif
    }

#if defined(ENABLE_EXTENSIONS)
    // Clear launch times as they are a form of history.
    extensions::ExtensionPrefs* extension_prefs =
        extensions::ExtensionPrefs::Get(profile_);
    extension_prefs->ClearLastLaunchTimes();
#endif

    // The power consumption history by origin contains details of websites
    // that were visited.
    power::OriginPowerMap* origin_power_map =
        power::OriginPowerMapFactory::GetForBrowserContext(profile_);
    if (origin_power_map)
      origin_power_map->ClearOriginMap();

    // Need to clear the host cache and accumulated speculative data, as it also
    // reveals some history: we have no mechanism to track when these items were
    // created, so we'll clear them all. Better safe than sorry.
    if (g_browser_process->io_thread()) {
      waiting_for_clear_hostname_resolution_cache_ = true;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(
              &BrowsingDataRemover::ClearHostnameResolutionCacheOnIOThread,
              base::Unretained(this),
              g_browser_process->io_thread()));
    }
    if (profile_->GetNetworkPredictor()) {
      waiting_for_clear_network_predictor_ = true;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&BrowsingDataRemover::ClearNetworkPredictorOnIOThread,
                     base::Unretained(this),
                     profile_->GetNetworkPredictor()));
    }

    // As part of history deletion we also delete the auto-generated keywords.
    TemplateURLService* keywords_model =
        TemplateURLServiceFactory::GetForProfile(profile_);
    if (keywords_model && !keywords_model->loaded()) {
      template_url_sub_ = keywords_model->RegisterOnLoadedCallback(
          base::Bind(&BrowsingDataRemover::OnKeywordsLoaded,
                     base::Unretained(this)));
      keywords_model->Load();
      waiting_for_clear_keyword_data_ = true;
    } else if (keywords_model) {
      keywords_model->RemoveAutoGeneratedForOriginBetween(
          remove_url, delete_begin_, delete_end_);
    }

    // The PrerenderManager keeps history of prerendered pages, so clear that.
    // It also may have a prerendered page. If so, the page could be
    // considered to have a small amount of historical information, so delete
    // it, too.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForProfile(profile_);
    if (prerender_manager) {
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS |
          prerender::PrerenderManager::CLEAR_PRERENDER_HISTORY);
    }

    // If the caller is removing history for all hosts, then clear ancillary
    // historical information.
    if (remove_url.is_empty()) {
      // We also delete the list of recently closed tabs. Since these expire,
      // they can't be more than a day old, so we can simply clear them all.
      TabRestoreService* tab_service =
          TabRestoreServiceFactory::GetForProfile(profile_);
      if (tab_service) {
        tab_service->ClearEntries();
        tab_service->DeleteLastSession();
      }

#if defined(ENABLE_SESSION_SERVICE)
      // We also delete the last session when we delete the history.
      SessionService* session_service =
          SessionServiceFactory::GetForProfile(profile_);
      if (session_service)
        session_service->DeleteLastSession();
#endif
    }

    // The saved Autofill profiles and credit cards can include the origin from
    // which these profiles and credit cards were learned.  These are a form of
    // history, so clear them as well.
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (web_data_service.get()) {
      waiting_for_clear_autofill_origin_urls_ = true;
      web_data_service->RemoveOriginURLsModifiedBetween(
          delete_begin_, delete_end_);
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE,
          base::Bind(&base::DoNothing),
          base::Bind(&BrowsingDataRemover::OnClearedAutofillOriginURLs,
                     base::Unretained(this)));

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }

#if defined(ENABLE_WEBRTC)
    waiting_for_clear_webrtc_logs_ = true;
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &WebRtcLogUtil::DeleteOldAndRecentWebRtcLogFiles,
            WebRtcLogList::GetWebRtcLogDirectoryForProfile(profile_->GetPath()),
            delete_begin_),
        base::Bind(&BrowsingDataRemover::OnClearedWebRtcLogs,
                   base::Unretained(this)));
#endif

    // The SSL Host State that tracks SSL interstitial "proceed" decisions may
    // include origins that the user has visited, so it must be cleared.
    if (profile_->GetSSLHostStateDelegate())
      profile_->GetSSLHostStateDelegate()->Clear();

#if defined(OS_ANDROID)
    precache::PrecacheManager* precache_manager =
        precache::PrecacheManagerFactory::GetForBrowserContext(profile_);
    // |precache_manager| could be nullptr if the profile is off the record.
    if (!precache_manager) {
      waiting_for_clear_precache_history_ = true;
      precache_manager->ClearHistory();
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE,
          base::Bind(&base::DoNothing),
          base::Bind(&BrowsingDataRemover::OnClearedPrecacheHistory,
                     base::Unretained(this)));
    }
#endif
  }

  if ((remove_mask & REMOVE_DOWNLOADS) && may_delete_history) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Downloads"));
    content::DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(profile_);
    if (remove_origin.unique())
      download_manager->RemoveDownloadsBetween(delete_begin_, delete_end_);
    else
      download_manager->RemoveDownloadsByOriginAndTime(
          remove_origin, delete_begin_, delete_end_);
    DownloadPrefs* download_prefs = DownloadPrefs::FromDownloadManager(
        download_manager);
    download_prefs->SetSaveFilePath(download_prefs->DownloadPath());
  }

  uint32 storage_partition_remove_mask = 0;

  // We ignore the REMOVE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request REMOVE_SITE_DATA with PROTECTED_WEB
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and PROTECTED_WEB.
  if (remove_mask & REMOVE_COOKIES &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Cookies"));

    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_COOKIES;

#if defined(SAFE_BROWSING_SERVICE)
    // Clear the safebrowsing cookies only if time period is for "all time".  It
    // doesn't make sense to apply the time period of deleting in the last X
    // hours/days to the safebrowsing cookies since they aren't the result of
    // any user action.
    if (delete_begin_ == base::Time()) {
      SafeBrowsingService* sb_service =
          g_browser_process->safe_browsing_service();
      if (sb_service) {
        net::URLRequestContextGetter* sb_context =
            sb_service->url_request_context();
        ++waiting_for_clear_cookies_count_;
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&BrowsingDataRemover::ClearCookiesOnIOThread,
                       base::Unretained(this), base::Unretained(sb_context)));
      }
    }
#endif
    MediaDeviceIDSalt::Reset(profile_->GetPrefs());

    // TODO(mkwst): If we're not removing passwords, then clear the 'zero-click'
    // flag for all credentials in the password store.
  }

  // Channel IDs are not separated for protected and unprotected web
  // origins. We check the origin_type_mask_ to prevent unintended deletion.
  if (remove_mask & REMOVE_CHANNEL_IDS &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_ChannelIDs"));
    // Since we are running on the UI thread don't call GetURLRequestContext().
    net::URLRequestContextGetter* rq_context = profile_->GetRequestContext();
    if (rq_context) {
      waiting_for_clear_channel_ids_ = true;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&BrowsingDataRemover::ClearChannelIDsOnIOThread,
                     base::Unretained(this), base::Unretained(rq_context)));
    }
  }

  if (remove_mask & REMOVE_LOCAL_STORAGE) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  }

  if (remove_mask & REMOVE_INDEXEDDB) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  }
  if (remove_mask & REMOVE_WEBSQL) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  }
  if (remove_mask & REMOVE_APPCACHE) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  }
  if (remove_mask & REMOVE_SERVICE_WORKERS) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  }
  if (remove_mask & REMOVE_FILE_SYSTEMS) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  }

#if defined(ENABLE_PLUGINS)
  // Plugin is data not separated for protected and unprotected web origins. We
  // check the origin_type_mask_ to prevent unintended deletion.
  if (remove_mask & REMOVE_PLUGIN_DATA &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_LSOData"));

    waiting_for_clear_plugin_data_ = true;
    if (!plugin_data_remover_.get())
      plugin_data_remover_.reset(content::PluginDataRemover::Create(profile_));
    base::WaitableEvent* event =
        plugin_data_remover_->StartRemoving(delete_begin_);

    base::WaitableEventWatcher::EventCallback watcher_callback =
        base::Bind(&BrowsingDataRemover::OnWaitableEventSignaled,
                   base::Unretained(this));
    watcher_.StartWatching(event, watcher_callback);
  }
#endif

  if (remove_mask & REMOVE_SITE_USAGE_DATA || remove_mask & REMOVE_HISTORY) {
    profile_->GetHostContentSettingsMap()->ClearSettingsForOneType(
        CONTENT_SETTINGS_TYPE_APP_BANNER);
    profile_->GetHostContentSettingsMap()->ClearSettingsForOneType(
        CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT);
  }

  if (remove_mask & REMOVE_PASSWORDS) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Passwords"));
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS).get();

    if (password_store) {
      waiting_for_clear_passwords_ = true;
      password_store->RemoveLoginsCreatedBetween(
          delete_begin_, delete_end_,
          base::Bind(&BrowsingDataRemover::OnClearedPasswords,
                     base::Unretained(this)));
    }
  }

  if (remove_mask & REMOVE_FORM_DATA) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Autofill"));
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);

    if (web_data_service.get()) {
      waiting_for_clear_form_ = true;
      web_data_service->RemoveFormElementsAddedBetween(delete_begin_,
          delete_end_);
      web_data_service->RemoveAutofillDataModifiedBetween(
          delete_begin_, delete_end_);
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE,
          base::Bind(&base::DoNothing),
          base::Bind(&BrowsingDataRemover::OnClearedFormData,
                     base::Unretained(this)));

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }
  }

  if (remove_mask & REMOVE_CACHE) {
    // Tell the renderers to clear their cache.
    web_cache::WebCacheManager::GetInstance()->ClearCache();

    content::RecordAction(UserMetricsAction("ClearBrowsingData_Cache"));

    waiting_for_clear_cache_ = true;
    // StoragePartitionHttpCacheDataRemover deletes itself when it is done.
    browsing_data::StoragePartitionHttpCacheDataRemover::CreateForRange(
        BrowserContext::GetDefaultStoragePartition(profile_), delete_begin_,
        delete_end_)
        ->Remove(base::Bind(&BrowsingDataRemover::ClearedCache,
                            base::Unretained(this)));

#if !defined(DISABLE_NACL)
    waiting_for_clear_nacl_cache_ = true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BrowsingDataRemover::ClearNaClCacheOnIOThread,
                   base::Unretained(this)));

    waiting_for_clear_pnacl_cache_ = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BrowsingDataRemover::ClearPnaclCacheOnIOThread,
                   base::Unretained(this), delete_begin_, delete_end_));
#endif

    // The PrerenderManager may have a page actively being prerendered, which
    // is essentially a preemptively cached page.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForProfile(profile_);
    if (prerender_manager) {
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS);
    }

    // Tell the shader disk cache to clear.
    content::RecordAction(UserMetricsAction("ClearBrowsingData_ShaderCache"));
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;

    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_WEBRTC_IDENTITY;

#if defined(ENABLE_EXTENSIONS)
    // Clear the ephemeral apps cache. This is nullptr while testing. OTR
    // Profile has neither apps nor an ExtensionService, so ClearCachedApps
    // fails.
    EphemeralAppService* ephemeral_app_service =
        EphemeralAppService::Get(profile_);
    if (ephemeral_app_service && !profile_->IsOffTheRecord())
      ephemeral_app_service->ClearCachedApps();
#endif
  }

  if (remove_mask & REMOVE_WEBRTC_IDENTITY) {
    storage_partition_remove_mask |=
        content::StoragePartition::REMOVE_DATA_MASK_WEBRTC_IDENTITY;
  }

  if (storage_partition_remove_mask) {
    waiting_for_clear_storage_partition_data_ = true;

    content::StoragePartition* storage_partition;
    if (storage_partition_for_testing_)
      storage_partition = storage_partition_for_testing_;
    else
      storage_partition = BrowserContext::GetDefaultStoragePartition(profile_);

    uint32 quota_storage_remove_mask =
        ~content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;

    if (delete_begin_ == base::Time() ||
        origin_type_mask_ &
          (BrowsingDataHelper::PROTECTED_WEB | BrowsingDataHelper::EXTENSION)) {
      // If we're deleting since the beginning of time, or we're removing
      // protected origins, then remove persistent quota data.
      quota_storage_remove_mask |=
          content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
    }

    storage_partition->ClearData(
        storage_partition_remove_mask, quota_storage_remove_mask, remove_url,
        base::Bind(&DoesOriginMatchMask, origin_type_mask_), delete_begin_,
        delete_end_,
        base::Bind(&BrowsingDataRemover::OnClearedStoragePartitionData,
                   base::Unretained(this)));
  }

#if defined(ENABLE_PLUGINS)
  if (remove_mask & REMOVE_CONTENT_LICENSES) {
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_ContentLicenses"));

    waiting_for_clear_content_licenses_ = true;
    if (!pepper_flash_settings_manager_.get()) {
      pepper_flash_settings_manager_.reset(
          new PepperFlashSettingsManager(this, profile_));
    }
    deauthorize_content_licenses_request_id_ =
        pepper_flash_settings_manager_->DeauthorizeContentLicenses(prefs);
#if defined(OS_CHROMEOS)
    // On Chrome OS, also delete any content protection platform keys.
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (!user) {
      LOG(WARNING) << "Failed to find user for current profile.";
    } else {
      chromeos::DBusThreadManager::Get()->GetCryptohomeClient()->
          TpmAttestationDeleteKeys(
              chromeos::attestation::KEY_USER,
              user->email(),
              chromeos::attestation::kContentProtectionKeyPrefix,
              base::Bind(&BrowsingDataRemover::OnClearPlatformKeys,
                         base::Unretained(this)));
      waiting_for_clear_platform_keys_ = true;
    }
#endif
  }
#endif

  // Remove omnibox zero-suggest cache results.
  if ((remove_mask & (REMOVE_CACHE | REMOVE_COOKIES)))
    prefs->SetString(omnibox::kZeroSuggestCachedResults, std::string());

  // Always wipe accumulated network related data (TransportSecurityState and
  // HttpServerPropertiesManager data).
  waiting_for_clear_networking_history_ = true;
  profile_->ClearNetworkingHistorySince(
      delete_begin_,
      base::Bind(&BrowsingDataRemover::OnClearedNetworkingHistory,
                 base::Unretained(this)));

  if (remove_mask & (REMOVE_COOKIES | REMOVE_HISTORY)) {
    domain_reliability::DomainReliabilityService* service =
      domain_reliability::DomainReliabilityServiceFactory::
          GetForBrowserContext(profile_);
    if (service) {
      domain_reliability::DomainReliabilityClearMode mode;
      if (remove_mask & REMOVE_COOKIES)
        mode = domain_reliability::CLEAR_CONTEXTS;
      else
        mode = domain_reliability::CLEAR_BEACONS;

      waiting_for_clear_domain_reliability_monitor_ = true;
      service->ClearBrowsingData(
          mode,
          base::Bind(&BrowsingDataRemover::OnClearedDomainReliabilityMonitor,
                     base::Unretained(this)));
    }
  }

  // Record the combined deletion of cookies and cache.
  CookieOrCacheDeletionChoice choice = NEITHER_COOKIES_NOR_CACHE;
  if (remove_mask & REMOVE_COOKIES &&
      origin_type_mask_ & BrowsingDataHelper::UNPROTECTED_WEB) {
    choice = remove_mask & REMOVE_CACHE ? BOTH_COOKIES_AND_CACHE
                                        : ONLY_COOKIES;
  } else if (remove_mask & REMOVE_CACHE) {
    choice = ONLY_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCache",
      choice, MAX_CHOICE_VALUE);
}

void BrowsingDataRemover::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BrowsingDataRemover::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowsingDataRemover::OnHistoryDeletionDone() {
  waiting_for_clear_history_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::OverrideStoragePartitionForTesting(
    content::StoragePartition* storage_partition) {
  storage_partition_for_testing_ = storage_partition;
}

base::Time BrowsingDataRemover::CalculateBeginDeleteTime(
    TimePeriod time_period) {
  base::TimeDelta diff;
  base::Time delete_begin_time = base::Time::Now();
  switch (time_period) {
    case LAST_HOUR:
      diff = base::TimeDelta::FromHours(1);
      break;
    case LAST_DAY:
      diff = base::TimeDelta::FromHours(24);
      break;
    case LAST_WEEK:
      diff = base::TimeDelta::FromHours(7*24);
      break;
    case FOUR_WEEKS:
      diff = base::TimeDelta::FromHours(4*7*24);
      break;
    case EVERYTHING:
      delete_begin_time = base::Time();
      break;
    default:
      NOTREACHED() << L"Missing item";
      break;
  }
  return delete_begin_time - diff;
}

bool BrowsingDataRemover::AllDone() {
  return !waiting_for_clear_autofill_origin_urls_ &&
         !waiting_for_clear_cache_ &&
         !waiting_for_clear_content_licenses_ &&
         !waiting_for_clear_channel_ids_ &&
         !waiting_for_clear_cookies_count_ &&
         !waiting_for_clear_domain_reliability_monitor_ &&
         !waiting_for_clear_form_ &&
         !waiting_for_clear_history_ &&
         !waiting_for_clear_hostname_resolution_cache_ &&
         !waiting_for_clear_keyword_data_ &&
         !waiting_for_clear_nacl_cache_ &&
         !waiting_for_clear_network_predictor_ &&
         !waiting_for_clear_networking_history_ &&
         !waiting_for_clear_passwords_ &&
         !waiting_for_clear_platform_keys_ &&
         !waiting_for_clear_plugin_data_ &&
         !waiting_for_clear_pnacl_cache_ &&
#if defined(OS_ANDROID)
         !waiting_for_clear_precache_history_ &&
#endif
#if defined(ENABLE_WEBRTC)
         !waiting_for_clear_webrtc_logs_ &&
#endif
         !waiting_for_clear_storage_partition_data_;
}

void BrowsingDataRemover::OnKeywordsLoaded() {
  // Deletes the entries from the model, and if we're not waiting on anything
  // else notifies observers and deletes this BrowsingDataRemover.
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  model->RemoveAutoGeneratedBetween(delete_begin_, delete_end_);
  waiting_for_clear_keyword_data_ = false;
  template_url_sub_.reset();
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::NotifyAndDelete() {
  set_removing(false);

  // Notify observers.
  BrowsingDataRemover::NotificationDetails details(delete_begin_, remove_mask_,
      origin_type_mask_);

  GetOnBrowsingDataRemovedCallbacks()->Notify(details);

  FOR_EACH_OBSERVER(Observer, observer_list_, OnBrowsingDataRemoverDone());

  // History requests aren't happy if you delete yourself from the callback.
  // As such, we do a delete later.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void BrowsingDataRemover::NotifyAndDeleteIfDone() {
  // TODO(brettw) http://crbug.com/305259: This should also observe session
  // clearing (what about other things such as passwords, etc.?) and wait for
  // them to complete before continuing.

  if (!AllDone())
    return;

  if (completion_inhibitor_) {
    completion_inhibitor_->OnBrowsingDataRemoverWouldComplete(
        this,
        base::Bind(&BrowsingDataRemover::NotifyAndDelete,
                   base::Unretained(this)));
  } else {
    NotifyAndDelete();
  }
}

void BrowsingDataRemover::OnClearedHostnameResolutionCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_hostname_resolution_cache_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearHostnameResolutionCacheOnIOThread(
    IOThread* io_thread) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  io_thread->ClearHostCache();

  // Notify the UI thread that we are done.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&BrowsingDataRemover::OnClearedHostnameResolutionCache,
                 base::Unretained(this)));
}

void BrowsingDataRemover::OnClearedNetworkPredictor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_network_predictor_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearNetworkPredictorOnIOThread(
    chrome_browser_net::Predictor* predictor) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(predictor);

  predictor->DiscardInitialNavigationHistory();
  predictor->DiscardAllResults();

  // Notify the UI thread that we are done.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&BrowsingDataRemover::OnClearedNetworkPredictor,
                 base::Unretained(this)));
}

void BrowsingDataRemover::OnClearedNetworkingHistory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_networking_history_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearedCache() {
  waiting_for_clear_cache_ = false;

  NotifyAndDeleteIfDone();
}

#if !defined(DISABLE_NACL)
void BrowsingDataRemover::ClearedNaClCache() {
  // This function should be called on the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  waiting_for_clear_nacl_cache_ = false;

  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearedNaClCacheOnIOThread() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Notify the UI thread that we are done.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::ClearedNaClCache,
                 base::Unretained(this)));
}

void BrowsingDataRemover::ClearNaClCacheOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  nacl::NaClBrowser::GetInstance()->ClearValidationCache(
      base::Bind(&BrowsingDataRemover::ClearedNaClCacheOnIOThread,
                 base::Unretained(this)));
}

void BrowsingDataRemover::ClearedPnaclCache() {
  // This function should be called on the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  waiting_for_clear_pnacl_cache_ = false;

  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearedPnaclCacheOnIOThread() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Notify the UI thread that we are done.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::ClearedPnaclCache,
                 base::Unretained(this)));
}

void BrowsingDataRemover::ClearPnaclCacheOnIOThread(base::Time begin,
                                                    base::Time end) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  pnacl::PnaclHost::GetInstance()->ClearTranslationCacheEntriesBetween(
      begin, end,
      base::Bind(&BrowsingDataRemover::ClearedPnaclCacheOnIOThread,
                 base::Unretained(this)));
}
#endif

void BrowsingDataRemover::OnWaitableEventSignaled(
    base::WaitableEvent* waitable_event) {
  waiting_for_clear_plugin_data_ = false;
  NotifyAndDeleteIfDone();
}

#if defined(ENABLE_PLUGINS)
void BrowsingDataRemover::OnDeauthorizeContentLicensesCompleted(
    uint32 request_id,
    bool /* success */) {
  DCHECK(waiting_for_clear_content_licenses_);
  DCHECK_EQ(request_id, deauthorize_content_licenses_request_id_);

  waiting_for_clear_content_licenses_ = false;
  NotifyAndDeleteIfDone();
}
#endif

#if defined(OS_CHROMEOS)
void BrowsingDataRemover::OnClearPlatformKeys(
    chromeos::DBusMethodCallStatus call_status,
    bool result) {
  DCHECK(waiting_for_clear_platform_keys_);
  if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS || !result) {
    LOG(ERROR) << "Failed to clear platform keys.";
  }
  waiting_for_clear_platform_keys_ = false;
  NotifyAndDeleteIfDone();
}
#endif


void BrowsingDataRemover::OnClearedPasswords() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_passwords_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::OnClearedCookies(int num_deleted) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&BrowsingDataRemover::OnClearedCookies,
                   base::Unretained(this), num_deleted));
    return;
  }

  DCHECK_GT(waiting_for_clear_cookies_count_, 0);
  --waiting_for_clear_cookies_count_;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearCookiesOnIOThread(
    net::URLRequestContextGetter* rq_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CookieStore* cookie_store = rq_context->
      GetURLRequestContext()->cookie_store();
  cookie_store->DeleteAllCreatedBetweenAsync(
      delete_begin_, delete_end_,
      base::Bind(&BrowsingDataRemover::OnClearedCookies,
                 base::Unretained(this)));
}

void BrowsingDataRemover::ClearChannelIDsOnIOThread(
    net::URLRequestContextGetter* rq_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::ChannelIDService* channel_id_service =
      rq_context->GetURLRequestContext()->channel_id_service();
  channel_id_service->GetChannelIDStore()->DeleteAllCreatedBetween(
      delete_begin_, delete_end_,
      base::Bind(&BrowsingDataRemover::OnClearedChannelIDsOnIOThread,
                 base::Unretained(this), base::Unretained(rq_context)));
}

void BrowsingDataRemover::OnClearedChannelIDsOnIOThread(
    net::URLRequestContextGetter* rq_context) {
  // Need to close open SSL connections which may be using the channel ids we
  // are deleting.
  // TODO(mattm): http://crbug.com/166069 Make the server bound cert
  // service/store have observers that can notify relevant things directly.
  rq_context->GetURLRequestContext()->ssl_config_service()->
      NotifySSLConfigChange();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataRemover::OnClearedChannelIDs,
                 base::Unretained(this)));
}

void BrowsingDataRemover::OnClearedChannelIDs() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_channel_ids_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::OnClearedFormData() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_form_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::OnClearedAutofillOriginURLs() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_autofill_origin_urls_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::OnClearedStoragePartitionData() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_storage_partition_data_ = false;
  NotifyAndDeleteIfDone();
}

#if defined(ENABLE_WEBRTC)
void BrowsingDataRemover::OnClearedWebRtcLogs() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_webrtc_logs_ = false;
  NotifyAndDeleteIfDone();
}
#endif

#if defined(OS_ANDROID)
void BrowsingDataRemover::OnClearedPrecacheHistory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_precache_history_ = false;
  NotifyAndDeleteIfDone();
}
#endif

void BrowsingDataRemover::OnClearedDomainReliabilityMonitor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  waiting_for_clear_domain_reliability_monitor_ = false;
  NotifyAndDeleteIfDone();
}

// static
BrowsingDataRemover::CallbackSubscription
    BrowsingDataRemover::RegisterOnBrowsingDataRemovedCallback(
        const BrowsingDataRemover::Callback& callback) {
  return GetOnBrowsingDataRemovedCallbacks()->Add(callback);
}
