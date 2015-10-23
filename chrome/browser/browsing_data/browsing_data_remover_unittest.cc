// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/domain_reliability/monitor.h"
#include "components/domain_reliability/service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/cookies/cookie_store.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"
#endif

class MockExtensionSpecialStoragePolicy;

using content::BrowserThread;
using content::StoragePartition;
using domain_reliability::CLEAR_BEACONS;
using domain_reliability::CLEAR_CONTEXTS;
using domain_reliability::DomainReliabilityClearMode;
using domain_reliability::DomainReliabilityMonitor;
using domain_reliability::DomainReliabilityService;
using domain_reliability::DomainReliabilityServiceFactory;
using testing::_;
using testing::ByRef;
using testing::Invoke;
using testing::Matcher;
using testing::MakeMatcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::Return;
using testing::WithArgs;

namespace {

const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:1/";
const char kTestOrigin3[] = "http://host3:1/";
const char kTestOriginExt[] = "chrome-extension://abcdefghijklmnopqrstuvwxyz/";
const char kTestOriginDevTools[] = "chrome-devtools://abcdefghijklmnopqrstuvw/";

// For Autofill.
const char kChromeOrigin[] = "Chrome settings";
const char kWebOrigin[] = "https://www.example.com/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);
const GURL kOriginExt(kTestOriginExt);
const GURL kOriginDevTools(kTestOriginDevTools);

const base::FilePath::CharType kDomStorageOrigin1[] =
    FILE_PATH_LITERAL("http_host1_1.localstorage");

const base::FilePath::CharType kDomStorageOrigin2[] =
    FILE_PATH_LITERAL("http_host2_1.localstorage");

const base::FilePath::CharType kDomStorageOrigin3[] =
    FILE_PATH_LITERAL("http_host3_1.localstorage");

const base::FilePath::CharType kDomStorageExt[] = FILE_PATH_LITERAL(
    "chrome-extension_abcdefghijklmnopqrstuvwxyz_0.localstorage");

#if defined(OS_CHROMEOS)
void FakeDBusCall(const chromeos::BoolDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, chromeos::DBUS_METHOD_CALL_SUCCESS, true));
}
#endif

struct StoragePartitionRemovalData {
  uint32 remove_mask = 0;
  uint32 quota_storage_remove_mask = 0;
  GURL remove_origin;
  base::Time remove_begin;
  base::Time remove_end;
  StoragePartition::OriginMatcherFunction origin_matcher;

  StoragePartitionRemovalData() {}
};

class TestStoragePartition : public StoragePartition {
 public:
  TestStoragePartition() {}
  ~TestStoragePartition() override {}

  // content::StoragePartition implementation.
  base::FilePath GetPath() override { return base::FilePath(); }
  net::URLRequestContextGetter* GetURLRequestContext() override {
    return nullptr;
  }
  net::URLRequestContextGetter* GetMediaURLRequestContext() override {
    return nullptr;
  }
  storage::QuotaManager* GetQuotaManager() override { return nullptr; }
  content::AppCacheService* GetAppCacheService() override { return nullptr; }
  storage::FileSystemContext* GetFileSystemContext() override {
    return nullptr;
  }
  storage::DatabaseTracker* GetDatabaseTracker() override { return nullptr; }
  content::DOMStorageContext* GetDOMStorageContext() override {
    return nullptr;
  }
  content::IndexedDBContext* GetIndexedDBContext() override { return nullptr; }
  content::ServiceWorkerContext* GetServiceWorkerContext() override {
    return nullptr;
  }
  content::GeofencingManager* GetGeofencingManager() override {
    return nullptr;
  }
  content::NavigatorConnectContext* GetNavigatorConnectContext() override {
    return nullptr;
  }

  content::PlatformNotificationContext* GetPlatformNotificationContext()
      override {
    return nullptr;
  }

  content::BackgroundSyncContext* GetBackgroundSyncContext() override {
    return nullptr;
  }

  content::HostZoomMap* GetHostZoomMap() override { return nullptr; }
  content::HostZoomLevelContext* GetHostZoomLevelContext() override {
    return nullptr;
  }
  content::ZoomLevelDelegate* GetZoomLevelDelegate() override {
    return nullptr;
  }

  void ClearDataForOrigin(uint32 remove_mask,
                          uint32 quota_storage_remove_mask,
                          const GURL& storage_origin,
                          net::URLRequestContextGetter* rq_context,
                          const base::Closure& callback) override {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&TestStoragePartition::AsyncRunCallback,
                                       base::Unretained(this),
                                       callback));
  }

  void ClearData(uint32 remove_mask,
                 uint32 quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const OriginMatcherFunction& origin_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback) override {
    // Store stuff to verify parameters' correctness later.
    storage_partition_removal_data_.remove_mask = remove_mask;
    storage_partition_removal_data_.quota_storage_remove_mask =
        quota_storage_remove_mask;
    storage_partition_removal_data_.remove_origin = storage_origin;
    storage_partition_removal_data_.remove_begin = begin;
    storage_partition_removal_data_.remove_end = end;
    storage_partition_removal_data_.origin_matcher = origin_matcher;

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&TestStoragePartition::AsyncRunCallback,
                   base::Unretained(this), callback));
  }

  void Flush() override {}

  StoragePartitionRemovalData GetStoragePartitionRemovalData() {
    return storage_partition_removal_data_;
  }

 private:
  void AsyncRunCallback(const base::Closure& callback) {
    callback.Run();
  }

  StoragePartitionRemovalData storage_partition_removal_data_;

  DISALLOW_COPY_AND_ASSIGN(TestStoragePartition);
};

// Custom matcher to verify is-same-origin relationship to given reference
// origin.
// (We cannot use equality-based matching because operator== is not defined for
// Origin, and we in fact want to rely on IsSameOrigin for matching purposes.)
class SameOriginMatcher : public MatcherInterface<const url::Origin&> {
 public:
  explicit SameOriginMatcher(const url::Origin& reference)
      : reference_(reference) {}

  virtual bool MatchAndExplain(const url::Origin& origin,
                               MatchResultListener* listener) const {
    return reference_.IsSameOriginWith(origin);
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "is same origin with " << reference_;
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "is not same origin with " << reference_;
  }

 private:
  const url::Origin& reference_;
};

inline Matcher<const url::Origin&> SameOrigin(const url::Origin& reference) {
  return MakeMatcher(new SameOriginMatcher(reference));
}

}  // namespace

// Testers -------------------------------------------------------------------

class RemoveCookieTester {
 public:
  RemoveCookieTester() {}

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie() {
    scoped_refptr<content::MessageLoopRunner> message_loop_runner =
        new content::MessageLoopRunner;
    quit_closure_ = message_loop_runner->QuitClosure();
    get_cookie_success_ = false;
    cookie_store_->GetCookiesWithOptionsAsync(
        kOrigin1, net::CookieOptions(),
        base::Bind(&RemoveCookieTester::GetCookieCallback,
                   base::Unretained(this)));
    message_loop_runner->Run();
    return get_cookie_success_;
  }

  void AddCookie() {
    scoped_refptr<content::MessageLoopRunner> message_loop_runner =
        new content::MessageLoopRunner;
    quit_closure_ = message_loop_runner->QuitClosure();
    cookie_store_->SetCookieWithOptionsAsync(
        kOrigin1, "A=1", net::CookieOptions(),
        base::Bind(&RemoveCookieTester::SetCookieCallback,
                   base::Unretained(this)));
    message_loop_runner->Run();
  }

 protected:
  void SetMonster(net::CookieStore* monster) {
    cookie_store_ = monster;
  }

 private:
  void GetCookieCallback(const std::string& cookies) {
    if (cookies == "A=1") {
      get_cookie_success_ = true;
    } else {
      EXPECT_EQ("", cookies);
      get_cookie_success_ = false;
    }
    quit_closure_.Run();
  }

  void SetCookieCallback(bool result) {
    ASSERT_TRUE(result);
    quit_closure_.Run();
  }

  bool get_cookie_success_ = false;
  base::Closure quit_closure_;
  net::CookieStore* cookie_store_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RemoveCookieTester);
};

#if defined(SAFE_BROWSING_SERVICE)
class RemoveSafeBrowsingCookieTester : public RemoveCookieTester {
 public:
  RemoveSafeBrowsingCookieTester()
      : browser_process_(TestingBrowserProcess::GetGlobal()) {
    scoped_refptr<SafeBrowsingService> sb_service =
        SafeBrowsingService::CreateSafeBrowsingService();
    browser_process_->SetSafeBrowsingService(sb_service.get());
    sb_service->Initialize();
    base::MessageLoop::current()->RunUntilIdle();

    // Create a cookiemonster that does not have persistant storage, and replace
    // the SafeBrowsingService created one with it.
    net::CookieStore* monster =
        content::CreateCookieStore(content::CookieStoreConfig());
    sb_service->url_request_context()->GetURLRequestContext()->
        set_cookie_store(monster);
    SetMonster(monster);
  }

  virtual ~RemoveSafeBrowsingCookieTester() {
    browser_process_->safe_browsing_service()->ShutDown();
    base::MessageLoop::current()->RunUntilIdle();
    browser_process_->SetSafeBrowsingService(nullptr);
  }

 private:
  TestingBrowserProcess* browser_process_;

  DISALLOW_COPY_AND_ASSIGN(RemoveSafeBrowsingCookieTester);
};
#endif

class RemoveChannelIDTester : public net::SSLConfigService::Observer {
 public:
  explicit RemoveChannelIDTester(TestingProfile* profile) {
    channel_id_service_ = profile->GetRequestContext()->
        GetURLRequestContext()->channel_id_service();
    ssl_config_service_ = profile->GetSSLConfigService();
    ssl_config_service_->AddObserver(this);
  }

  ~RemoveChannelIDTester() override {
    ssl_config_service_->RemoveObserver(this);
  }

  int ChannelIDCount() { return channel_id_service_->channel_id_count(); }

  // Add a server bound cert for |server| with specific creation and expiry
  // times.  The cert and key data will be filled with dummy values.
  void AddChannelIDWithTimes(const std::string& server_identifier,
                             base::Time creation_time) {
    GetChannelIDStore()->SetChannelID(
        make_scoped_ptr(new net::ChannelIDStore::ChannelID(
            server_identifier, creation_time,
            make_scoped_ptr(crypto::ECPrivateKey::Create()))));
  }

  // Add a server bound cert for |server|, with the current time as the
  // creation time.  The cert and key data will be filled with dummy values.
  void AddChannelID(const std::string& server_identifier) {
    base::Time now = base::Time::Now();
    AddChannelIDWithTimes(server_identifier, now);
  }

  void GetChannelIDList(net::ChannelIDStore::ChannelIDList* channel_ids) {
    GetChannelIDStore()->GetAllChannelIDs(
        base::Bind(&RemoveChannelIDTester::GetAllChannelIDsCallback,
                   channel_ids));
  }

  net::ChannelIDStore* GetChannelIDStore() {
    return channel_id_service_->GetChannelIDStore();
  }

  int ssl_config_changed_count() const {
    return ssl_config_changed_count_;
  }

  // net::SSLConfigService::Observer implementation:
  void OnSSLConfigChanged() override { ssl_config_changed_count_++; }

 private:
  static void GetAllChannelIDsCallback(
      net::ChannelIDStore::ChannelIDList* dest,
      const net::ChannelIDStore::ChannelIDList& result) {
    *dest = result;
  }

  net::ChannelIDService* channel_id_service_;
  scoped_refptr<net::SSLConfigService> ssl_config_service_;
  int ssl_config_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(RemoveChannelIDTester);
};

class RemoveHistoryTester {
 public:
  RemoveHistoryTester() {}

  bool Init(TestingProfile* profile) WARN_UNUSED_RESULT {
    if (!profile->CreateHistoryService(true, false))
      return false;
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    return true;
  }

  // Returns true, if the given URL exists in the history service.
  bool HistoryContainsURL(const GURL& url) {
    scoped_refptr<content::MessageLoopRunner> message_loop_runner =
        new content::MessageLoopRunner;
    quit_closure_ = message_loop_runner->QuitClosure();
    history_service_->QueryURL(
        url,
        true,
        base::Bind(&RemoveHistoryTester::SaveResultAndQuit,
                   base::Unretained(this)),
        &tracker_);
    message_loop_runner->Run();
    return query_url_success_;
  }

  void AddHistory(const GURL& url, base::Time time) {
    history_service_->AddPage(url, time, nullptr, 0, GURL(),
                              history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                              history::SOURCE_BROWSED, false);
  }

 private:
  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(bool success,
                         const history::URLRow&,
                         const history::VisitVector&) {
    query_url_success_ = success;
    quit_closure_.Run();
  }

  // For History requests.
  base::CancelableTaskTracker tracker_;
  bool query_url_success_ = false;
  base::Closure quit_closure_;

  // TestingProfile owns the history service; we shouldn't delete it.
  history::HistoryService* history_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RemoveHistoryTester);
};

class RemoveFaviconTester {
 public:
  RemoveFaviconTester() {}

  bool Init(TestingProfile* profile) WARN_UNUSED_RESULT {
    // Create the history service if it has not been created yet.
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (!history_service_) {
      if (!profile->CreateHistoryService(true, false))
        return false;
      history_service_ = HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
    }

    profile->CreateFaviconService();
    favicon_service_ = FaviconServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    return true;
  }

  // Returns true if there is a favicon stored for |page_url| in the favicon
  // database.
  bool HasFaviconForPageURL(const GURL& page_url) {
    RequestFaviconSyncForPageURL(page_url);
    return got_favicon_;
  }

  // Returns true if:
  // - There is a favicon stored for |page_url| in the favicon database.
  // - The stored favicon is expired.
  bool HasExpiredFaviconForPageURL(const GURL& page_url) {
    RequestFaviconSyncForPageURL(page_url);
    return got_expired_favicon_;
  }

  // Adds a visit to history and stores an arbitrary favicon bitmap for
  // |page_url|.
  void VisitAndAddFavicon(const GURL& page_url) {
    history_service_->AddPage(page_url, base::Time::Now(), nullptr, 0, GURL(),
        history::RedirectList(), ui::PAGE_TRANSITION_LINK,
        history::SOURCE_BROWSED, false);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
    bitmap.eraseColor(SK_ColorBLUE);
    favicon_service_->SetFavicons(page_url, page_url, favicon_base::FAVICON,
                                  gfx::Image::CreateFrom1xBitmap(bitmap));
  }

 private:
  // Synchronously requests the favicon for |page_url| from the favicon
  // database.
  void RequestFaviconSyncForPageURL(const GURL& page_url) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    favicon_service_->GetRawFaviconForPageURL(
        page_url,
        favicon_base::FAVICON,
        gfx::kFaviconSize,
        base::Bind(&RemoveFaviconTester::SaveResultAndQuit,
                   base::Unretained(this)),
        &tracker_);
    run_loop.Run();
  }

  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(const favicon_base::FaviconRawBitmapResult& result) {
    got_favicon_ = result.is_valid();
    got_expired_favicon_ = result.is_valid() && result.expired;
    quit_closure_.Run();
  }

  // For favicon requests.
  base::CancelableTaskTracker tracker_;
  bool got_favicon_ = false;
  bool got_expired_favicon_ = false;
  base::Closure quit_closure_;

  // Owned by TestingProfile.
  history::HistoryService* history_service_ = nullptr;
  favicon::FaviconService* favicon_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RemoveFaviconTester);
};

class RemoveAutofillTester : public autofill::PersonalDataManagerObserver {
 public:
  explicit RemoveAutofillTester(TestingProfile* profile)
      : personal_data_manager_(
            autofill::PersonalDataManagerFactory::GetForProfile(profile)) {
    autofill::test::DisableSystemServices(profile->GetPrefs());
    personal_data_manager_->AddObserver(this);
  }

  ~RemoveAutofillTester() override {
    personal_data_manager_->RemoveObserver(this);
  }

  // Returns true if there are autofill profiles.
  bool HasProfile() {
    return !personal_data_manager_->GetProfiles().empty() &&
           !personal_data_manager_->GetCreditCards().empty();
  }

  bool HasOrigin(const std::string& origin) {
    const std::vector<autofill::AutofillProfile*>& profiles =
        personal_data_manager_->GetProfiles();
    for (const autofill::AutofillProfile* profile : profiles) {
      if (profile->origin() == origin)
        return true;
    }

    const std::vector<autofill::CreditCard*>& credit_cards =
        personal_data_manager_->GetCreditCards();
    for (const autofill::CreditCard* credit_card : credit_cards) {
      if (credit_card->origin() == origin)
        return true;
    }

    return false;
  }

  // Add two profiles and two credit cards to the database.  In each pair, one
  // entry has a web origin and the other has a Chrome origin.
  void AddProfilesAndCards() {
    std::vector<autofill::AutofillProfile> profiles;
    autofill::AutofillProfile profile;
    profile.set_guid(base::GenerateGUID());
    profile.set_origin(kWebOrigin);
    profile.SetRawInfo(autofill::NAME_FIRST, base::ASCIIToUTF16("Bob"));
    profile.SetRawInfo(autofill::NAME_LAST, base::ASCIIToUTF16("Smith"));
    profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, base::ASCIIToUTF16("94043"));
    profile.SetRawInfo(autofill::EMAIL_ADDRESS,
                       base::ASCIIToUTF16("sue@example.com"));
    profile.SetRawInfo(autofill::COMPANY_NAME, base::ASCIIToUTF16("Company X"));
    profiles.push_back(profile);

    profile.set_guid(base::GenerateGUID());
    profile.set_origin(kChromeOrigin);
    profiles.push_back(profile);

    personal_data_manager_->SetProfiles(&profiles);
    base::MessageLoop::current()->Run();

    std::vector<autofill::CreditCard> cards;
    autofill::CreditCard card;
    card.set_guid(base::GenerateGUID());
    card.set_origin(kWebOrigin);
    card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                    base::ASCIIToUTF16("1234-5678-9012-3456"));
    cards.push_back(card);

    card.set_guid(base::GenerateGUID());
    card.set_origin(kChromeOrigin);
    cards.push_back(card);

    personal_data_manager_->SetCreditCards(&cards);
    base::MessageLoop::current()->Run();
  }

 private:
  void OnPersonalDataChanged() override {
    base::MessageLoop::current()->Quit();
  }

  autofill::PersonalDataManager* personal_data_manager_;
  DISALLOW_COPY_AND_ASSIGN(RemoveAutofillTester);
};

class RemoveLocalStorageTester {
 public:
  explicit RemoveLocalStorageTester(TestingProfile* profile)
      : profile_(profile) {
    dom_storage_context_ =
        content::BrowserContext::GetDefaultStoragePartition(profile)->
            GetDOMStorageContext();
  }

  // Returns true, if the given origin URL exists.
  bool DOMStorageExistsForOrigin(const GURL& origin) {
    scoped_refptr<content::MessageLoopRunner> message_loop_runner =
        new content::MessageLoopRunner;
    quit_closure_ = message_loop_runner->QuitClosure();
    GetLocalStorageUsage();
    message_loop_runner->Run();
    for (size_t i = 0; i < infos_.size(); ++i) {
      if (origin == infos_[i].origin)
        return true;
    }
    return false;
  }

  void AddDOMStorageTestData() {
    // Note: This test depends on details of how the dom_storage library
    // stores data in the host file system.
    base::FilePath storage_path =
        profile_->GetPath().AppendASCII("Local Storage");
    base::CreateDirectory(storage_path);

    // Write some files.
    base::WriteFile(storage_path.Append(kDomStorageOrigin1), nullptr, 0);
    base::WriteFile(storage_path.Append(kDomStorageOrigin2), nullptr, 0);
    base::WriteFile(storage_path.Append(kDomStorageOrigin3), nullptr, 0);
    base::WriteFile(storage_path.Append(kDomStorageExt), nullptr, 0);

    // Tweak their dates.
    base::Time now = base::Time::Now();
    base::TouchFile(storage_path.Append(kDomStorageOrigin1), now, now);

    base::Time one_day_ago = now - base::TimeDelta::FromDays(1);
    base::TouchFile(storage_path.Append(kDomStorageOrigin2),
                    one_day_ago, one_day_ago);

    base::Time sixty_days_ago = now - base::TimeDelta::FromDays(60);
    base::TouchFile(storage_path.Append(kDomStorageOrigin3),
                    sixty_days_ago, sixty_days_ago);

    base::TouchFile(storage_path.Append(kDomStorageExt), now, now);
  }

 private:
  void GetLocalStorageUsage() {
    dom_storage_context_->GetLocalStorageUsage(
        base::Bind(&RemoveLocalStorageTester::OnGotLocalStorageUsage,
                   base::Unretained(this)));
  }
  void OnGotLocalStorageUsage(
      const std::vector<content::LocalStorageUsageInfo>& infos) {
    infos_ = infos;
    quit_closure_.Run();
  }

  // We don't own these pointers.
  TestingProfile* profile_;
  content::DOMStorageContext* dom_storage_context_ = nullptr;

  std::vector<content::LocalStorageUsageInfo> infos_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RemoveLocalStorageTester);
};

class MockDomainReliabilityService : public DomainReliabilityService {
 public:
  MockDomainReliabilityService() {}

  ~MockDomainReliabilityService() override {}

  scoped_ptr<DomainReliabilityMonitor> CreateMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
      override {
    NOTREACHED();
    return scoped_ptr<DomainReliabilityMonitor>();
  }

  void ClearBrowsingData(DomainReliabilityClearMode clear_mode,
                         const base::Closure& callback) override {
    clear_count_++;
    last_clear_mode_ = clear_mode;
    callback.Run();
  }

  void GetWebUIData(const base::Callback<void(scoped_ptr<base::Value>)>&
                        callback) const override {
    NOTREACHED();
  }

  int clear_count() const { return clear_count_; }

  DomainReliabilityClearMode last_clear_mode() const {
    return last_clear_mode_;
  }

 private:
  unsigned clear_count_ = 0;
  DomainReliabilityClearMode last_clear_mode_;
};

struct TestingDomainReliabilityServiceFactoryUserData
    : public base::SupportsUserData::Data {
  TestingDomainReliabilityServiceFactoryUserData(
      content::BrowserContext* context,
      MockDomainReliabilityService* service)
      : context(context),
        service(service),
        attached(false) {}
  ~TestingDomainReliabilityServiceFactoryUserData() override {}

  content::BrowserContext* const context;
  MockDomainReliabilityService* const service;
  bool attached;

  static const void* kKey;
};

// static
const void* TestingDomainReliabilityServiceFactoryUserData::kKey =
    &TestingDomainReliabilityServiceFactoryUserData::kKey;

scoped_ptr<KeyedService> TestingDomainReliabilityServiceFactoryFunction(
    content::BrowserContext* context) {
  const void* kKey = TestingDomainReliabilityServiceFactoryUserData::kKey;

  TestingDomainReliabilityServiceFactoryUserData* data =
      static_cast<TestingDomainReliabilityServiceFactoryUserData*>(
          context->GetUserData(kKey));
  EXPECT_TRUE(data);
  EXPECT_EQ(data->context, context);
  EXPECT_FALSE(data->attached);

  data->attached = true;
  return make_scoped_ptr(data->service);
}

class ClearDomainReliabilityTester {
 public:
  explicit ClearDomainReliabilityTester(TestingProfile* profile) :
      profile_(profile),
      mock_service_(new MockDomainReliabilityService()) {
    AttachService();
  }

  unsigned clear_count() const { return mock_service_->clear_count(); }

  DomainReliabilityClearMode last_clear_mode() const {
    return mock_service_->last_clear_mode();
  }

 private:
  void AttachService() {
    const void* kKey = TestingDomainReliabilityServiceFactoryUserData::kKey;

    // Attach kludgey UserData struct to profile.
    TestingDomainReliabilityServiceFactoryUserData* data =
        new TestingDomainReliabilityServiceFactoryUserData(profile_,
                                                           mock_service_);
    EXPECT_FALSE(profile_->GetUserData(kKey));
    profile_->SetUserData(kKey, data);

    // Set and use factory that will attach service stuffed in kludgey struct.
    DomainReliabilityServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_,
        &TestingDomainReliabilityServiceFactoryFunction);

    // Verify and detach kludgey struct.
    EXPECT_EQ(data, profile_->GetUserData(kKey));
    EXPECT_TRUE(data->attached);
    profile_->RemoveUserData(kKey);
  }

  TestingProfile* profile_;
  MockDomainReliabilityService* mock_service_;
};

class RemoveDownloadsTester {
 public:
  explicit RemoveDownloadsTester(TestingProfile* testing_profile)
      : download_manager_(new content::MockDownloadManager()),
        chrome_download_manager_delegate_(testing_profile) {
    content::BrowserContext::SetDownloadManagerForTesting(testing_profile,
                                                          download_manager_);
    EXPECT_EQ(download_manager_,
              content::BrowserContext::GetDownloadManager(testing_profile));

    EXPECT_CALL(*download_manager_, GetDelegate())
        .WillOnce(Return(&chrome_download_manager_delegate_));
    EXPECT_CALL(*download_manager_, Shutdown());
  }

  ~RemoveDownloadsTester() { chrome_download_manager_delegate_.Shutdown(); }

  content::MockDownloadManager* download_manager() { return download_manager_; }

 private:
  content::MockDownloadManager* download_manager_;
  ChromeDownloadManagerDelegate chrome_download_manager_delegate_;

  DISALLOW_COPY_AND_ASSIGN(RemoveDownloadsTester);
};

// Test Class ----------------------------------------------------------------

class BrowsingDataRemoverTest : public testing::Test {
 public:
  BrowsingDataRemoverTest()
      : profile_(new TestingProfile()),
        clear_domain_reliability_tester_(GetProfile()) {
    callback_subscription_ =
        BrowsingDataRemover::RegisterOnBrowsingDataRemovedCallback(
            base::Bind(&BrowsingDataRemoverTest::NotifyWithDetails,
                       base::Unretained(this)));
  }

  ~BrowsingDataRemoverTest() override {}

  void TearDown() override {
#if defined(ENABLE_EXTENSIONS)
    mock_policy_ = nullptr;
#endif

    // TestingProfile contains a DOMStorageContext.  BrowserContext's destructor
    // posts a message to the WEBKIT thread to delete some of its member
    // variables. We need to ensure that the profile is destroyed, and that
    // the message loop is cleared out, before destroying the threads and loop.
    // Otherwise we leak memory.
    profile_.reset();
    base::MessageLoop::current()->RunUntilIdle();

    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void BlockUntilBrowsingDataRemoved(BrowsingDataRemover::TimePeriod period,
                                     int remove_mask,
                                     bool include_protected_origins) {
    BrowsingDataRemover* remover = BrowsingDataRemover::CreateForPeriod(
        profile_.get(), period);

    TestStoragePartition storage_partition;
    remover->OverrideStoragePartitionForTesting(&storage_partition);

    called_with_details_.reset(new BrowsingDataRemover::NotificationDetails());

    // BrowsingDataRemover deletes itself when it completes.
    int origin_type_mask = BrowsingDataHelper::UNPROTECTED_WEB;
    if (include_protected_origins)
      origin_type_mask |= BrowsingDataHelper::PROTECTED_WEB;

    BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->Remove(remove_mask, origin_type_mask);
    completion_observer.BlockUntilCompletion();

    // Save so we can verify later.
    storage_partition_removal_data_ =
        storage_partition.GetStoragePartitionRemovalData();
  }

  void BlockUntilOriginDataRemoved(BrowsingDataRemover::TimePeriod period,
                                   int remove_mask,
                                   const GURL& remove_origin) {
    BrowsingDataRemover* remover = BrowsingDataRemover::CreateForPeriod(
        profile_.get(), period);
    TestStoragePartition storage_partition;
    remover->OverrideStoragePartitionForTesting(&storage_partition);

    called_with_details_.reset(new BrowsingDataRemover::NotificationDetails());

    // BrowsingDataRemover deletes itself when it completes.
    BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveImpl(remove_mask, remove_origin,
        BrowsingDataHelper::UNPROTECTED_WEB);
    completion_observer.BlockUntilCompletion();

    // Save so we can verify later.
    storage_partition_removal_data_ =
        storage_partition.GetStoragePartitionRemovalData();
  }

  TestingProfile* GetProfile() {
    return profile_.get();
  }

  base::Time GetBeginTime() {
    return called_with_details_->removal_begin;
  }

  int GetRemovalMask() {
    return called_with_details_->removal_mask;
  }

  int GetOriginTypeMask() {
    return called_with_details_->origin_type_mask;
  }

  StoragePartitionRemovalData GetStoragePartitionRemovalData() {
    return storage_partition_removal_data_;
  }

  // Callback for browsing data removal events.
  void NotifyWithDetails(
      const BrowsingDataRemover::NotificationDetails& details) {
    // We're not taking ownership of the details object, but storing a copy of
    // it locally.
    called_with_details_.reset(
        new BrowsingDataRemover::NotificationDetails(details));

    callback_subscription_.reset();
  }

  MockExtensionSpecialStoragePolicy* CreateMockPolicy() {
#if defined(ENABLE_EXTENSIONS)
    mock_policy_ = new MockExtensionSpecialStoragePolicy;
    return mock_policy_.get();
#else
    NOTREACHED();
    return nullptr;
#endif
  }

  storage::SpecialStoragePolicy* mock_policy() {
#if defined(ENABLE_EXTENSIONS)
    return mock_policy_.get();
#else
    return nullptr;
#endif
  }

  // If |kOrigin1| is protected when extensions are enabled, the expected
  // result for tests where the OriginMatcherFunction result is variable.
  bool ShouldRemoveForProtectedOriginOne() const {
#if defined(ENABLE_EXTENSIONS)
    return false;
#else
    return true;
#endif
  }

  const ClearDomainReliabilityTester& clear_domain_reliability_tester() {
    return clear_domain_reliability_tester_;
  }

 protected:
  scoped_ptr<BrowsingDataRemover::NotificationDetails> called_with_details_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;

  StoragePartitionRemovalData storage_partition_removal_data_;

#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy_;
#endif

  BrowsingDataRemover::CallbackSubscription callback_subscription_;

  // Needed to mock out DomainReliabilityService, even for unrelated tests.
  ClearDomainReliabilityTester clear_domain_reliability_tester_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverTest);
};

// Tests ---------------------------------------------------------------------

TEST_F(BrowsingDataRemoverTest, RemoveCookieForever) {
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_COOKIES,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverTest, RemoveCookieLastHour) {
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_COOKIES,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  // Removing with time period other than EVERYTHING should not clear
  // persistent storage data.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

#if defined(SAFE_BROWSING_SERVICE)
TEST_F(BrowsingDataRemoverTest, RemoveSafeBrowsingCookieForever) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(BrowsingDataRemoverTest, RemoveSafeBrowsingCookieLastHour) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  // Removing with time period other than EVERYTHING should not clear safe
  // browsing cookies.
  EXPECT_TRUE(tester.ContainsCookie());
}
#endif

TEST_F(BrowsingDataRemoverTest, RemoveChannelIDForever) {
  RemoveChannelIDTester tester(GetProfile());

  tester.AddChannelID(kTestOrigin1);
  EXPECT_EQ(0, tester.ssl_config_changed_count());
  EXPECT_EQ(1, tester.ChannelIDCount());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_CHANNEL_IDS, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_CHANNEL_IDS, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_EQ(1, tester.ssl_config_changed_count());
  EXPECT_EQ(0, tester.ChannelIDCount());
}

TEST_F(BrowsingDataRemoverTest, RemoveChannelIDLastHour) {
  RemoveChannelIDTester tester(GetProfile());

  base::Time now = base::Time::Now();
  tester.AddChannelID(kTestOrigin1);
  tester.AddChannelIDWithTimes(kTestOrigin2,
                               now - base::TimeDelta::FromHours(2));
  EXPECT_EQ(0, tester.ssl_config_changed_count());
  EXPECT_EQ(2, tester.ChannelIDCount());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_CHANNEL_IDS, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_CHANNEL_IDS, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_EQ(1, tester.ssl_config_changed_count());
  ASSERT_EQ(1, tester.ChannelIDCount());
  net::ChannelIDStore::ChannelIDList channel_ids;
  tester.GetChannelIDList(&channel_ids);
  ASSERT_EQ(1U, channel_ids.size());
  EXPECT_EQ(kTestOrigin2, channel_ids.front().server_identifier());
}

TEST_F(BrowsingDataRemoverTest, RemoveUnprotectedLocalStorageForever) {
#if defined(ENABLE_EXTENSIONS)
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_LOCAL_STORAGE,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher.
  EXPECT_EQ(ShouldRemoveForProtectedOriginOne(),
            removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginExt, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveProtectedLocalStorageForever) {
#if defined(ENABLE_EXTENSIONS)
  // Protect kOrigin1.
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_LOCAL_STORAGE,
                                true);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB |
            BrowsingDataHelper::PROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher all http origin will match since we specified
  // both protected and unprotected.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginExt, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveLocalStorageForLastWeek) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_WEEK,
                                BrowsingDataRemover::REMOVE_LOCAL_STORAGE,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  // Persistent storage won't be deleted.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginExt, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForever) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_HISTORY, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForLastHour) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_HISTORY, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

// This should crash (DCHECK) in Debug, but death tests don't work properly
// here.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(BrowsingDataRemoverTest, RemoveHistoryProhibited) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_HISTORY, false);
  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Nothing should have been deleted.
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}
#endif

TEST_F(BrowsingDataRemoverTest, RemoveMultipleTypes) {
  // Add some history.
  RemoveHistoryTester history_tester;
  ASSERT_TRUE(history_tester.Init(GetProfile()));
  history_tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(history_tester.HistoryContainsURL(kOrigin1));

  int removal_mask = BrowsingDataRemover::REMOVE_HISTORY |
                     BrowsingDataRemover::REMOVE_COOKIES;

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      removal_mask, false);

  EXPECT_EQ(removal_mask, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_FALSE(history_tester.HistoryContainsURL(kOrigin1));

  // The cookie would be deleted throught the StorageParition, check if the
  // partition was requested to remove cookie.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
}

// This should crash (DCHECK) in Debug, but death tests don't work properly
// here.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(BrowsingDataRemoverTest, RemoveMultipleTypesHistoryProhibited) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  // Add some history.
  RemoveHistoryTester history_tester;
  ASSERT_TRUE(history_tester.Init(GetProfile()));
  history_tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(history_tester.HistoryContainsURL(kOrigin1));

  int removal_mask = BrowsingDataRemover::REMOVE_HISTORY |
                     BrowsingDataRemover::REMOVE_COOKIES;

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
                                removal_mask, false);
  EXPECT_EQ(removal_mask, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // 1/2. History should remain.
  EXPECT_TRUE(history_tester.HistoryContainsURL(kOrigin1));

  // 2/2. The cookie(s) would be deleted throught the StorageParition, check if
  // the partition was requested to remove cookie.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  // Persistent storage won't be deleted, since EVERYTHING was not specified.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
}
#endif

// Test that clearing history deletes favicons not associated with bookmarks.
TEST_F(BrowsingDataRemoverTest, RemoveFaviconsForever) {
  GURL page_url("http://a");

  RemoveFaviconTester favicon_tester;
  ASSERT_TRUE(favicon_tester.Init(GetProfile()));
  favicon_tester.VisitAndAddFavicon(page_url);
  ASSERT_TRUE(favicon_tester.HasFaviconForPageURL(page_url));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_HISTORY, false);
  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_FALSE(favicon_tester.HasFaviconForPageURL(page_url));
}

// Test that a bookmark's favicon is expired and not deleted when clearing
// history. Expiring the favicon causes the bookmark's favicon to be updated
// when the user next visits the bookmarked page. Expiring the bookmark's
// favicon is useful when the bookmark's favicon becomes incorrect (See
// crbug.com/474421 for a sample bug which causes this).
TEST_F(BrowsingDataRemoverTest, ExpireBookmarkFavicons) {
  GURL bookmarked_page("http://a");

  TestingProfile* profile = GetProfile();
  profile->CreateBookmarkModel(true);
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfile(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0,
                         base::ASCIIToUTF16("a"), bookmarked_page);

  RemoveFaviconTester favicon_tester;
  ASSERT_TRUE(favicon_tester.Init(GetProfile()));
  favicon_tester.VisitAndAddFavicon(bookmarked_page);
  ASSERT_TRUE(favicon_tester.HasFaviconForPageURL(bookmarked_page));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_HISTORY, false);
  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_TRUE(favicon_tester.HasExpiredFaviconForPageURL(bookmarked_page));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverBoth) {
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                BrowsingDataRemover::REMOVE_WEBSQL |
                                BrowsingDataRemover::REMOVE_APPCACHE |
                                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                BrowsingDataRemover::REMOVE_INDEXEDDB,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverOnlyTemporary) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                BrowsingDataRemover::REMOVE_WEBSQL |
                                BrowsingDataRemover::REMOVE_APPCACHE |
                                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                BrowsingDataRemover::REMOVE_INDEXEDDB,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());

  // Check that all related origin data would be removed, that is, origin
  // matcher would match these origin.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverOnlyPersistent) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                BrowsingDataRemover::REMOVE_WEBSQL |
                                BrowsingDataRemover::REMOVE_APPCACHE |
                                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                BrowsingDataRemover::REMOVE_INDEXEDDB,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());

  // Check that all related origin data would be removed, that is, origin
  // matcher would match these origin.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverNeither) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif


  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                BrowsingDataRemover::REMOVE_WEBSQL |
                                BrowsingDataRemover::REMOVE_APPCACHE |
                                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                BrowsingDataRemover::REMOVE_INDEXEDDB,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());

  // Check that all related origin data would be removed, that is, origin
  // matcher would match these origin.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverSpecificOrigin) {
  // Remove Origin 1.
  BlockUntilOriginDataRemoved(BrowsingDataRemover::EVERYTHING,
                              BrowsingDataRemover::REMOVE_APPCACHE |
                              BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                              BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                              BrowsingDataRemover::REMOVE_INDEXEDDB |
                              BrowsingDataRemover::REMOVE_WEBSQL,
                              kOrigin1);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_origin, kOrigin1);
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForLastHour) {
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                BrowsingDataRemover::REMOVE_WEBSQL |
                                BrowsingDataRemover::REMOVE_APPCACHE |
                                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                BrowsingDataRemover::REMOVE_INDEXEDDB,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);

  // Persistent data would be left out since we are not removing from
  // beginning of time.
  uint32 expected_quota_mask =
      ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
  EXPECT_EQ(removal_data.quota_storage_remove_mask, expected_quota_mask);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());
  // Check removal begin time.
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForLastWeek) {
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_WEEK,
                                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                BrowsingDataRemover::REMOVE_WEBSQL |
                                BrowsingDataRemover::REMOVE_APPCACHE |
                                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                BrowsingDataRemover::REMOVE_INDEXEDDB,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);

  // Persistent data would be left out since we are not removing from
  // beginning of time.
  uint32 expected_quota_mask =
      ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
  EXPECT_EQ(removal_data.quota_storage_remove_mask, expected_quota_mask);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());
  // Check removal begin time.
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedUnprotectedOrigins) {
#if defined(ENABLE_EXTENSIONS)
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                BrowsingDataRemover::REMOVE_WEBSQL |
                                BrowsingDataRemover::REMOVE_APPCACHE |
                                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                BrowsingDataRemover::REMOVE_INDEXEDDB,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());

  // Check OriginMatcherFunction.
  EXPECT_EQ(ShouldRemoveForProtectedOriginOne(),
            removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedProtectedSpecificOrigin) {
#if defined(ENABLE_EXTENSIONS)
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  // Try to remove kOrigin1. Expect failure.
  BlockUntilOriginDataRemoved(BrowsingDataRemover::EVERYTHING,
                              BrowsingDataRemover::REMOVE_APPCACHE |
                              BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                              BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                              BrowsingDataRemover::REMOVE_INDEXEDDB |
                              BrowsingDataRemover::REMOVE_WEBSQL,
                              kOrigin1);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_origin, kOrigin1);

  // Check OriginMatcherFunction.
  EXPECT_EQ(ShouldRemoveForProtectedOriginOne(),
            removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedProtectedOrigins) {
#if defined(ENABLE_EXTENSIONS)
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  // Try to remove kOrigin1. Expect success.
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_APPCACHE |
                                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                BrowsingDataRemover::REMOVE_INDEXEDDB |
                                BrowsingDataRemover::REMOVE_WEBSQL,
                                true);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::PROTECTED_WEB |
      BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());

  // Check OriginMatcherFunction, |kOrigin1| would match mask since we
  // would have 'protected' specified in origin_type_mask.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedIgnoreExtensionsAndDevTools) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_APPCACHE |
                                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                BrowsingDataRemover::REMOVE_INDEXEDDB |
                                BrowsingDataRemover::REMOVE_WEBSQL,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.remove_origin.is_empty());

  // Check that extension and devtools data wouldn't be removed, that is,
  // origin matcher would not match these origin.
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginExt, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginDevTools, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, OriginBasedHistoryRemoval) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilOriginDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_HISTORY, kOrigin2);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Nothing should have been deleted.
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin2));
}

TEST_F(BrowsingDataRemoverTest, OriginAndTimeBasedHistoryRemoval) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilOriginDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_HISTORY, kOrigin2);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

// Verify that clearing autofill form data works.
TEST_F(BrowsingDataRemoverTest, AutofillRemovalLastHour) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_FORM_DATA, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

TEST_F(BrowsingDataRemoverTest, AutofillRemovalEverything) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_FORM_DATA, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

// Verify that clearing autofill form data works.
TEST_F(BrowsingDataRemoverTest, AutofillOriginsRemovedWithHistory) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  tester.AddProfilesAndCards();
  EXPECT_FALSE(tester.HasOrigin(std::string()));
  EXPECT_TRUE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(kChromeOrigin));

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_HISTORY, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_TRUE(tester.HasOrigin(std::string()));
  EXPECT_FALSE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(kChromeOrigin));
}

TEST_F(BrowsingDataRemoverTest, CompletionInhibition) {
  // The |completion_inhibitor| on the stack should prevent removal sessions
  // from completing until after ContinueToCompletion() is called.
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor;

  called_with_details_.reset(new BrowsingDataRemover::NotificationDetails());

  // BrowsingDataRemover deletes itself when it completes.
  BrowsingDataRemover* remover = BrowsingDataRemover::CreateForPeriod(
      GetProfile(), BrowsingDataRemover::EVERYTHING);
  remover->Remove(BrowsingDataRemover::REMOVE_HISTORY,
                  BrowsingDataHelper::UNPROTECTED_WEB);

  // Process messages until the inhibitor is notified, and then some, to make
  // sure we do not complete asynchronously before ContinueToCompletion() is
  // called.
  completion_inhibitor.BlockUntilNearCompletion();
  base::RunLoop().RunUntilIdle();

  // Verify that the completion notification has not yet been broadcasted.
  EXPECT_EQ(-1, GetRemovalMask());
  EXPECT_EQ(-1, GetOriginTypeMask());

  // Now run the removal process until completion, and verify that observers are
  // now notified, and the notifications is sent out.
  BrowsingDataRemoverCompletionObserver completion_observer(remover);
  completion_inhibitor.ContinueToCompletion();
  completion_observer.BlockUntilCompletion();

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
}

TEST_F(BrowsingDataRemoverTest, ZeroSuggestCacheClear) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults,
                   "[\"\", [\"foo\", \"bar\"]]");
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
                                BrowsingDataRemover::REMOVE_COOKIES,
                                false);

  // Expect the prefs to be cleared when cookies are removed.
  EXPECT_TRUE(prefs->GetString(omnibox::kZeroSuggestCachedResults).empty());
  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
}

#if defined(OS_CHROMEOS)
TEST_F(BrowsingDataRemoverTest, ContentProtectionPlatformKeysRemoval) {
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service;
  chromeos::ScopedTestCrosSettings test_cros_settings;
  chromeos::MockUserManager* mock_user_manager =
      new testing::NiceMock<chromeos::MockUserManager>();
  mock_user_manager->SetActiveUser("test@example.com");
  chromeos::ScopedUserManagerEnabler user_manager_enabler(mock_user_manager);

  scoped_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
      chromeos::DBusThreadManager::GetSetterForTesting();
  chromeos::MockCryptohomeClient* cryptohome_client =
      new chromeos::MockCryptohomeClient;
  dbus_setter->SetCryptohomeClient(
      scoped_ptr<chromeos::CryptohomeClient>(cryptohome_client));

  // Expect exactly one call.  No calls means no attempt to delete keys and more
  // than one call means a significant performance problem.
  EXPECT_CALL(*cryptohome_client, TpmAttestationDeleteKeys(_, _, _, _))
      .WillOnce(WithArgs<3>(Invoke(FakeDBusCall)));

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_CONTENT_LICENSES, false);

  chromeos::DBusThreadManager::Shutdown();
}
#endif

TEST_F(BrowsingDataRemoverTest, DomainReliability_Null) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  EXPECT_EQ(0u, tester.clear_count());
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_Beacons) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_HISTORY, false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_BEACONS, tester.last_clear_mode());
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_Contexts) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_COOKIES, false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_ContextsWin) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_HISTORY |
      BrowsingDataRemover::REMOVE_COOKIES, false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_ProtectedOrigins) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_COOKIES, true);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
}

// TODO(ttuttle): This isn't actually testing the no-monitor case, since
// BrowsingDataRemoverTest now creates one unconditionally, since it's needed
// for some unrelated test cases. This should be fixed so it tests the no-
// monitor case again.
TEST_F(BrowsingDataRemoverTest, DISABLED_DomainReliability_NoMonitor) {
  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_HISTORY |
      BrowsingDataRemover::REMOVE_COOKIES, false);
}

TEST_F(BrowsingDataRemoverTest, RemoveSameOriginDownloads) {
  RemoveDownloadsTester tester(GetProfile());
  const url::Origin expectedOrigin(kOrigin1);

  EXPECT_CALL(*tester.download_manager(),
              RemoveDownloadsByOriginAndTime(SameOrigin(expectedOrigin), _, _));

  BlockUntilOriginDataRemoved(BrowsingDataRemover::EVERYTHING,
                              BrowsingDataRemover::REMOVE_DOWNLOADS, kOrigin1);
}
