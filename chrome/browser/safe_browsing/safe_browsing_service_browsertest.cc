// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test creates a safebrowsing service using test safebrowsing database
// and a test protocol manager. It is used to test logics in safebrowsing
// service.

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/thread_test_helper.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/startup_task_runner_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/local_database_manager.h"
#include "chrome/browser/safe_browsing/metadata.pb.h"
#include "chrome/browser/safe_browsing/protocol_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_database.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/startup_task_runner_service.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/chromeos_switches.h"
#endif

#if !defined(SAFE_BROWSING_DB_LOCAL)
#error This test requires SAFE_BROWSING_DB_LOCAL.
#endif

using content::BrowserThread;
using content::InterstitialPage;
using content::WebContents;
using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

namespace {

void InvokeFullHashCallback(
    SafeBrowsingProtocolManager::FullHashCallback callback,
    const std::vector<SBFullHashResult>& result) {
  callback.Run(result, base::TimeDelta::FromMinutes(45));
}

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  explicit FakeSafeBrowsingService(const std::string& url_prefix)
      : url_prefix_(url_prefix) {}

  SafeBrowsingProtocolConfig GetProtocolConfig() const override {
    SafeBrowsingProtocolConfig config;
    config.url_prefix = url_prefix_;
    // Makes sure the auto update is not triggered. The tests will force the
    // update when needed.
    config.disable_auto_update = true;
#if defined(OS_ANDROID)
    config.disable_connection_check = true;
#endif
    config.client_name = "browser_tests";
    return config;
  }

 private:
  ~FakeSafeBrowsingService() override {}

  std::string url_prefix_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  explicit TestSafeBrowsingServiceFactory(const std::string& url_prefix)
      : url_prefix_(url_prefix) {}

  SafeBrowsingService* CreateSafeBrowsingService() override {
    return new FakeSafeBrowsingService(url_prefix_);
  }

 private:
  std::string url_prefix_;
};

// A SafeBrowingDatabase class that allows us to inject the malicious URLs.
class TestSafeBrowsingDatabase :  public SafeBrowsingDatabase {
 public:
  TestSafeBrowsingDatabase() {}

  ~TestSafeBrowsingDatabase() override {}

  // Initializes the database with the given filename.
  void Init(const base::FilePath& filename) override {}

  // Deletes the current database and creates a new one.
  bool ResetDatabase() override {
    badurls_.clear();
    return true;
  }

  // Called on the IO thread to check if the given URL is safe or not.  If we
  // can synchronously determine that the URL is safe, CheckUrl returns true,
  // otherwise it returns false.
  bool ContainsBrowseUrl(const GURL& url,
                         std::vector<SBPrefix>* prefix_hits,
                         std::vector<SBFullHashResult>* cache_hits) override {
    cache_hits->clear();
    return ContainsUrl(safe_browsing_util::MALWARE,
                       safe_browsing_util::PHISH,
                       std::vector<GURL>(1, url),
                       prefix_hits);
  }
  bool ContainsUnwantedSoftwareUrl(
      const GURL& url,
      std::vector<SBPrefix>* prefix_hits,
      std::vector<SBFullHashResult>* cache_hits) override {
    cache_hits->clear();
    return ContainsUrl(safe_browsing_util::UNWANTEDURL,
                       safe_browsing_util::UNWANTEDURL,
                       std::vector<GURL>(1, url),
                       prefix_hits);
  }
  bool ContainsDownloadUrlPrefixes(
      const std::vector<SBPrefix>& prefixes,
      std::vector<SBPrefix>* prefix_hits) override {
    bool found =
        ContainsUrlPrefixes(safe_browsing_util::BINURL,
                            safe_browsing_util::BINURL, prefixes, prefix_hits);
    if (!found)
      return false;
    DCHECK_LE(1U, prefix_hits->size());
    return true;
  }
  bool ContainsCsdWhitelistedUrl(const GURL& url) override { return true; }
  bool ContainsDownloadWhitelistedString(const std::string& str) override {
    return true;
  }
  bool ContainsDownloadWhitelistedUrl(const GURL& url) override { return true; }
  bool ContainsInclusionWhitelistedUrl(const GURL& url) override {
    return true;
  }
  bool ContainsExtensionPrefixes(const std::vector<SBPrefix>& prefixes,
                                 std::vector<SBPrefix>* prefix_hits) override {
    return false;
  }
  bool ContainsMalwareIP(const std::string& ip_address) override {
    return true;
  }
  bool UpdateStarted(std::vector<SBListChunkRanges>* lists) override {
    ADD_FAILURE() << "Not implemented.";
    return false;
  }
  void InsertChunks(const std::string& list_name,
                    const std::vector<SBChunkData*>& chunks) override {
    ADD_FAILURE() << "Not implemented.";
  }
  void DeleteChunks(const std::vector<SBChunkDelete>& chunk_deletes) override {
    ADD_FAILURE() << "Not implemented.";
  }
  void UpdateFinished(bool update_succeeded) override {
    ADD_FAILURE() << "Not implemented.";
  }
  void CacheHashResults(const std::vector<SBPrefix>& prefixes,
                        const std::vector<SBFullHashResult>& cache_hits,
                        const base::TimeDelta& cache_lifetime) override {
    // Do nothing for the cache.
  }
  bool IsMalwareIPMatchKillSwitchOn() override { return false; }
  bool IsCsdWhitelistKillSwitchOn() override { return false; }

  // Fill up the database with test URL.
  void AddUrl(const GURL& url,
              const SBFullHashResult& full_hash,
              const std::vector<SBPrefix>& prefix_hits) {
    Hits* hits_for_url = &badurls_[url.spec()];
    hits_for_url->list_ids.push_back(full_hash.list_id);
    hits_for_url->prefix_hits.insert(hits_for_url->prefix_hits.end(),
                                     prefix_hits.begin(),
                                     prefix_hits.end());
    bad_prefixes_.insert(
        std::make_pair(full_hash.list_id, full_hash.hash.prefix));
  }

 private:
  // Stores |list_ids| of safe browsing lists that match some |prefix_hits|.
  struct Hits {
    std::vector<int> list_ids;
    std::vector<SBPrefix> prefix_hits;
  };

  bool ContainsUrl(int list_id0,
                   int list_id1,
                   const std::vector<GURL>& urls,
                   std::vector<SBPrefix>* prefix_hits) {
    bool hit = false;
    for (const GURL& url : urls) {
      base::hash_map<std::string, Hits>::const_iterator
          badurls_it = badurls_.find(url.spec());

      if (badurls_it == badurls_.end())
        continue;

      std::vector<int> list_ids_for_url = badurls_it->second.list_ids;
      if (std::find(list_ids_for_url.begin(), list_ids_for_url.end(), list_id0)
              != list_ids_for_url.end() ||
          std::find(list_ids_for_url.begin(), list_ids_for_url.end(), list_id1)
              != list_ids_for_url.end()) {
        prefix_hits->insert(prefix_hits->end(),
                            badurls_it->second.prefix_hits.begin(),
                            badurls_it->second.prefix_hits.end());
        hit = true;
      }
    }
    return hit;
  }

  bool ContainsUrlPrefixes(int list_id0,
                           int list_id1,
                           const std::vector<SBPrefix>& prefixes,
                           std::vector<SBPrefix>* prefix_hits) {
    bool hit = false;
    for (const SBPrefix& prefix : prefixes) {
      for (const std::pair<int, SBPrefix>& entry : bad_prefixes_) {
        if (entry.second == prefix &&
            (entry.first == list_id0 || entry.first == list_id1)) {
          prefix_hits->push_back(prefix);
          hit = true;
        }
      }
    }
    return hit;
  }

  base::hash_map<std::string, Hits> badurls_;
  base::hash_set<std::pair<int, SBPrefix>> bad_prefixes_;

  DISALLOW_COPY_AND_ASSIGN(TestSafeBrowsingDatabase);
};

// Factory that creates TestSafeBrowsingDatabase instances.
class TestSafeBrowsingDatabaseFactory : public SafeBrowsingDatabaseFactory {
 public:
  TestSafeBrowsingDatabaseFactory() : db_(NULL) {}
  ~TestSafeBrowsingDatabaseFactory() override {}

  SafeBrowsingDatabase* CreateSafeBrowsingDatabase(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      bool enable_download_protection,
      bool enable_client_side_whitelist,
      bool enable_download_whitelist,
      bool enable_extension_blacklist,
      bool enable_ip_blacklist,
      bool enabled_unwanted_software_list) override {
    db_ = new TestSafeBrowsingDatabase();
    return db_;
  }
  TestSafeBrowsingDatabase* GetDb() {
    return db_;
  }
 private:
  // Owned by the SafebrowsingService.
  TestSafeBrowsingDatabase* db_;
};

// A TestProtocolManager that could return fixed responses from
// safebrowsing server for testing purpose.
class TestProtocolManager :  public SafeBrowsingProtocolManager {
 public:
  TestProtocolManager(SafeBrowsingProtocolManagerDelegate* delegate,
                      net::URLRequestContextGetter* request_context_getter,
                      const SafeBrowsingProtocolConfig& config)
      : SafeBrowsingProtocolManager(delegate, request_context_getter, config) {
    create_count_++;
  }

  ~TestProtocolManager() override { delete_count_++; }

  // This function is called when there is a prefix hit in local safebrowsing
  // database and safebrowsing service issues a get hash request to backends.
  // We return a result from the prefilled full_hashes_ hash_map to simulate
  // server's response. At the same time, latency is added to simulate real
  // life network issues.
  void GetFullHash(const std::vector<SBPrefix>& prefixes,
                   SafeBrowsingProtocolManager::FullHashCallback callback,
                   bool is_download,
                   bool is_extended_reporting) override {
    BrowserThread::PostDelayedTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(InvokeFullHashCallback, callback, full_hashes_),
        delay_);
  }

  // Prepare the GetFullHash results for the next request.
  void AddGetFullHashResponse(const SBFullHashResult& full_hash_result) {
    full_hashes_.push_back(full_hash_result);
  }

  void IntroduceDelay(const base::TimeDelta& delay) {
    delay_ = delay;
  }

  static int create_count() {
    return create_count_;
  }

  static int delete_count() {
    return delete_count_;
  }

 private:
  std::vector<SBFullHashResult> full_hashes_;
  base::TimeDelta delay_;
  static int create_count_;
  static int delete_count_;
};

// static
int TestProtocolManager::create_count_ = 0;
// static
int TestProtocolManager::delete_count_ = 0;

// Factory that creates TestProtocolManager instances.
class TestSBProtocolManagerFactory : public SBProtocolManagerFactory {
 public:
  TestSBProtocolManagerFactory() : pm_(NULL) {}
  ~TestSBProtocolManagerFactory() override {}

  SafeBrowsingProtocolManager* CreateProtocolManager(
      SafeBrowsingProtocolManagerDelegate* delegate,
      net::URLRequestContextGetter* request_context_getter,
      const SafeBrowsingProtocolConfig& config) override {
    pm_ = new TestProtocolManager(delegate, request_context_getter, config);
    return pm_;
  }

  TestProtocolManager* GetProtocolManager() {
    return pm_;
  }

 private:
  // Owned by the SafebrowsingService.
  TestProtocolManager* pm_;
};

class MockObserver : public SafeBrowsingUIManager::Observer {
 public:
  MockObserver() {}
  virtual ~MockObserver() {}
  MOCK_METHOD1(OnSafeBrowsingHit,
               void(const SafeBrowsingUIManager::UnsafeResource&));
  MOCK_METHOD1(OnSafeBrowsingMatch,
               void(const SafeBrowsingUIManager::UnsafeResource&));
};

MATCHER_P(IsUnsafeResourceFor, url, "") {
  return (arg.url.spec() == url.spec() &&
          arg.threat_type != SB_THREAT_TYPE_SAFE);
}

class ServiceEnabledHelper : public base::ThreadTestHelper {
 public:
  ServiceEnabledHelper(
      SafeBrowsingService* service,
      bool enabled,
      scoped_refptr<base::SingleThreadTaskRunner> target_thread)
      : base::ThreadTestHelper(target_thread),
        service_(service),
        expected_enabled_(enabled) {}

  void RunTest() override {
    set_test_result(service_->enabled() == expected_enabled_);
  }

 private:
  ~ServiceEnabledHelper() override {}

  scoped_refptr<SafeBrowsingService> service_;
  const bool expected_enabled_;
};

}  // namespace

// Tests the safe browsing blocking page in a browser.
class SafeBrowsingServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingServiceTest() {
  }

  static void GenUrlFullhashResult(const GURL& url,
                                   int list_id,
                                   SBFullHashResult* full_hash) {
    std::string host;
    std::string path;
    safe_browsing_util::CanonicalizeUrl(url, &host, &path, NULL);
    full_hash->hash = SBFullHashForString(host + path);
    full_hash->list_id = list_id;
  }

  void SetUp() override {
    // InProcessBrowserTest::SetUp() instantiates SafebrowsingService and
    // RegisterFactory has to be called before SafeBrowsingService is created.
    sb_factory_.reset(new TestSafeBrowsingServiceFactory(
        "https://definatelynotarealdomain/safebrowsing"));
    SafeBrowsingService::RegisterFactory(sb_factory_.get());
    SafeBrowsingDatabase::RegisterFactory(&db_factory_);
    SafeBrowsingProtocolManager::RegisterFactory(&pm_factory_);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();

    // Unregister test factories after InProcessBrowserTest::TearDown
    // (which destructs SafeBrowsingService).
    SafeBrowsingDatabase::RegisterFactory(NULL);
    SafeBrowsingProtocolManager::RegisterFactory(NULL);
    SafeBrowsingService::RegisterFactory(NULL);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Makes sure the auto update is not triggered during the test.
    // This test will fill up the database using testing prefixes
    // and urls.
    command_line->AppendSwitch(switches::kSbDisableAutoUpdate);
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    g_browser_process->safe_browsing_service()->ui_manager()->AddObserver(
        &observer_);
  }

  void TearDownOnMainThread() override {
    g_browser_process->safe_browsing_service()->ui_manager()->RemoveObserver(
        &observer_);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  // This will setup the "url" prefix in database and prepare protocol manager
  // to respond with |full_hash|, as well as other |full_hash|es previously set
  // via this call, on GetFullHash requests.
  void SetupResponseForUrl(const GURL& url, const SBFullHashResult& full_hash) {
    std::vector<SBPrefix> prefix_hits;
    prefix_hits.push_back(full_hash.hash.prefix);

    // Make sure the full hits is empty unless we need to test the
    // full hash is hit in database's local cache.
    TestSafeBrowsingDatabase* db = db_factory_.GetDb();
    db->AddUrl(url, full_hash, prefix_hits);

    TestProtocolManager* pm = pm_factory_.GetProtocolManager();
    pm->AddGetFullHashResponse(full_hash);
  }

  bool ShowingInterstitialPage() {
    WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InterstitialPage* interstitial_page = contents->GetInterstitialPage();
    return interstitial_page != NULL;
  }

  void IntroduceGetHashDelay(const base::TimeDelta& delay) {
    pm_factory_.GetProtocolManager()->IntroduceDelay(delay);
  }

  // TODO(nparker): Remove the need for this by wiring in our own
  // SafeBrowsingDatabaseManager factory and keep a ptr to the subclass.
  // Or add a Get/SetTimeout to sbdbmgr.
  static LocalSafeBrowsingDatabaseManager* LocalDatabaseManagerForService(
      SafeBrowsingService* sb_service) {
    return static_cast<LocalSafeBrowsingDatabaseManager*>(
        sb_service->database_manager().get());
  }

  static base::TimeDelta GetCheckTimeout(SafeBrowsingService* sb_service) {
    return LocalDatabaseManagerForService(sb_service)->check_timeout_;
  }

  static void SetCheckTimeout(SafeBrowsingService* sb_service,
                              const base::TimeDelta& delay) {
    LocalDatabaseManagerForService(sb_service)->check_timeout_ = delay;
  }

  void CreateCSDService() {
#if defined(SAFE_BROWSING_CSD)
    safe_browsing::ClientSideDetectionService* csd_service =
        safe_browsing::ClientSideDetectionService::Create(NULL);
    SafeBrowsingService* sb_service =
        g_browser_process->safe_browsing_service();

    // A CSD service should already exist.
    EXPECT_TRUE(sb_service->csd_service_);

    sb_service->csd_service_.reset(csd_service);
    sb_service->RefreshState();
#endif
  }

  void ProceedAndWhitelist(
      const SafeBrowsingUIManager::UnsafeResource& resource) {
    std::vector<SafeBrowsingUIManager::UnsafeResource> resources;
    resources.push_back(resource);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingUIManager::OnBlockingPageDone,
                   g_browser_process->safe_browsing_service()->ui_manager(),
                   resources, true));
    WaitForIOThread();
  }

 protected:
  StrictMock<MockObserver> observer_;

  // Temporary profile dir for test cases that create a second profile.  This is
  // owned by the SafeBrowsingServiceTest object so that it will not get
  // destructed until after the test Browser has been torn down, since the
  // ImportantFileWriter may still be modifying it after the Profile object has
  // been destroyed.
  base::ScopedTempDir temp_profile_dir_;

  // Waits for pending tasks on the IO thread to complete. This is useful
  // to wait for the SafeBrowsingService to finish loading/stopping.
  void WaitForIOThread() {
    scoped_refptr<base::ThreadTestHelper> io_helper(new base::ThreadTestHelper(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get()));
    ASSERT_TRUE(io_helper->Run());
  }

  // Waits for pending tasks on the IO thread to complete and check if the
  // SafeBrowsingService enabled state matches |enabled|.
  void WaitForIOAndCheckEnabled(SafeBrowsingService* service, bool enabled) {
    scoped_refptr<ServiceEnabledHelper> enabled_helper(new ServiceEnabledHelper(
        service, enabled,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get()));
    ASSERT_TRUE(enabled_helper->Run());
  }

 private:
  scoped_ptr<TestSafeBrowsingServiceFactory> sb_factory_;
  TestSafeBrowsingDatabaseFactory db_factory_;
  TestSBProtocolManagerFactory pm_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceTest);
};

enum MalwareMetadataTestType {
  METADATA_NONE,
  METADATA_LANDING,
  METADATA_DISTRIBUTION,
};

class SafeBrowsingServiceMetadataTest
    : public SafeBrowsingServiceTest,
      public ::testing::WithParamInterface<MalwareMetadataTestType> {
 public:
  SafeBrowsingServiceMetadataTest() {}

  void GenUrlFullhashResultWithMetadata(const GURL& url,
                                        SBFullHashResult* full_hash) {
    GenUrlFullhashResult(url, safe_browsing_util::MALWARE, full_hash);

    safe_browsing::MalwarePatternType proto;
    switch (GetParam()) {
      case METADATA_NONE:
        full_hash->metadata = std::string();
        break;
      case METADATA_LANDING:
        proto.set_pattern_type(safe_browsing::MalwarePatternType::LANDING);
        full_hash->metadata = proto.SerializeAsString();
        break;
      case METADATA_DISTRIBUTION:
        proto.set_pattern_type(safe_browsing::MalwarePatternType::DISTRIBUTION);
        full_hash->metadata = proto.SerializeAsString();
        break;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceMetadataTest);
};

namespace {

const char kEmptyPage[] = "/empty.html";
const char kMalwareFile[] = "/downloads/dangerous/dangerous.exe";
const char kMalwarePage[] = "/safe_browsing/malware.html";
const char kMalwareIFrame[] = "/safe_browsing/malware_iframe.html";
const char kMalwareImg[] = "/safe_browsing/malware_image.png";

// This test goes through DownloadResourceHandler.
IN_PROC_BROWSER_TEST_P(SafeBrowsingServiceMetadataTest, MalwareMainFrame) {
  GURL url = embedded_test_server()->GetURL(kEmptyPage);

  // After adding the url to safebrowsing database and getfullhash result,
  // we should see the interstitial page.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResultWithMetadata(url, &malware_full_hash);
  EXPECT_CALL(observer_,
              OnSafeBrowsingMatch(IsUnsafeResourceFor(url))).Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(1);
  SetupResponseForUrl(url, malware_full_hash);
  ui_test_utils::NavigateToURL(browser(), url);
  // All types should show the interstitial.
  EXPECT_TRUE(ShowingInterstitialPage());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingServiceMetadataTest, MalwareIFrame) {
  GURL main_url = embedded_test_server()->GetURL(kMalwarePage);
  GURL iframe_url = embedded_test_server()->GetURL(kMalwareIFrame);

  // Add the iframe url as malware and then load the parent page.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResultWithMetadata(iframe_url, &malware_full_hash);
  EXPECT_CALL(observer_, OnSafeBrowsingMatch(IsUnsafeResourceFor(iframe_url)))
      .Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(iframe_url)))
      .Times(1);
  SetupResponseForUrl(iframe_url, malware_full_hash);
  ui_test_utils::NavigateToURL(browser(), main_url);
  // All types should show the interstitial.
  EXPECT_TRUE(ShowingInterstitialPage());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingServiceMetadataTest, MalwareImg) {
  GURL main_url = embedded_test_server()->GetURL(kMalwarePage);
  GURL img_url = embedded_test_server()->GetURL(kMalwareImg);

  // Add the img url as malware and then load the parent page.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResultWithMetadata(img_url, &malware_full_hash);
  switch (GetParam()) {
    case METADATA_NONE:  // Falls through.
    case METADATA_DISTRIBUTION:
      EXPECT_CALL(observer_, OnSafeBrowsingMatch(IsUnsafeResourceFor(img_url)))
          .Times(1);
      EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(img_url)))
          .Times(1);
      break;
    case METADATA_LANDING:
      // No interstitial shown, so no notifications expected.
      break;
  }
  SetupResponseForUrl(img_url, malware_full_hash);
  ui_test_utils::NavigateToURL(browser(), main_url);
  // Subresource which is tagged as a landing page should not show an
  // interstitial, the other types should.
  switch (GetParam()) {
    case METADATA_NONE:
    case METADATA_DISTRIBUTION:
      EXPECT_TRUE(ShowingInterstitialPage());
      break;
    case METADATA_LANDING:
      EXPECT_FALSE(ShowingInterstitialPage());
      break;
  }
}

INSTANTIATE_TEST_CASE_P(MaybeSetMetadata,
                        SafeBrowsingServiceMetadataTest,
                        testing::Values(METADATA_NONE,
                                        METADATA_LANDING,
                                        METADATA_DISTRIBUTION));

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, UnwantedImgIgnored) {
  GURL main_url = embedded_test_server()->GetURL(kMalwarePage);
  GURL img_url = embedded_test_server()->GetURL(kMalwareImg);

  // Add the img url as coming from a site serving UwS and then load the parent
  // page.
  SBFullHashResult uws_full_hash;
  GenUrlFullhashResult(img_url, safe_browsing_util::UNWANTEDURL,
                       &uws_full_hash);
  SetupResponseForUrl(img_url, uws_full_hash);

  ui_test_utils::NavigateToURL(browser(), main_url);

  EXPECT_FALSE(ShowingInterstitialPage());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, DISABLED_MalwareWithWhitelist) {
  GURL url = embedded_test_server()->GetURL(kEmptyPage);

  // After adding the url to safebrowsing database and getfullhash result,
  // we should see the interstitial page.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResult(url, safe_browsing_util::MALWARE, &malware_full_hash);
  EXPECT_CALL(observer_,
              OnSafeBrowsingMatch(IsUnsafeResourceFor(url))).Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(1)
      .WillOnce(testing::Invoke(
          this, &SafeBrowsingServiceTest::ProceedAndWhitelist));
  SetupResponseForUrl(url, malware_full_hash);

  ui_test_utils::NavigateToURL(browser(), url);
  // Mock calls OnBlockingPageDone set to proceed, so the interstitial
  // is removed.
  EXPECT_FALSE(ShowingInterstitialPage());
  Mock::VerifyAndClearExpectations(&observer_);

  // Navigate back to kEmptyPage -- should hit the whitelist, and send a match
  // call, but no hit call.
  EXPECT_CALL(observer_,
              OnSafeBrowsingMatch(IsUnsafeResourceFor(url))).Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(url))).Times(0);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(ShowingInterstitialPage());
}

const char kPrefetchMalwarePage[] = "/safe_browsing/prefetch_malware.html";

// This test confirms that prefetches don't themselves get the
// interstitial treatment.
IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, Prefetch) {
  GURL url = embedded_test_server()->GetURL(kPrefetchMalwarePage);
  GURL malware_url = embedded_test_server()->GetURL(kMalwarePage);

  class SetPrefetchForTest {
   public:
    explicit SetPrefetchForTest(bool prefetch)
        : old_prerender_mode_(prerender::PrerenderManager::GetMode()) {
      std::string exp_group = prefetch ? "ExperimentYes" : "ExperimentNo";
      base::FieldTrialList::CreateFieldTrial("Prefetch", exp_group);

      prerender::PrerenderManager::SetMode(
          prerender::PrerenderManager::PRERENDER_MODE_DISABLED);
    }

    ~SetPrefetchForTest() {
      prerender::PrerenderManager::SetMode(old_prerender_mode_);
    }

   private:
    prerender::PrerenderManager::PrerenderManagerMode old_prerender_mode_;
  } set_prefetch_for_test(true);

  // Even though we have added this uri to the safebrowsing database and
  // getfullhash result, we should not see the interstitial page since the
  // only malware was a prefetch target.
  SBFullHashResult malware_full_hash;
  GenUrlFullhashResult(malware_url, safe_browsing_util::MALWARE,
                       &malware_full_hash);
  SetupResponseForUrl(malware_url, malware_full_hash);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_FALSE(ShowingInterstitialPage());
  Mock::VerifyAndClear(&observer_);

  // However, when we navigate to the malware page, we should still get
  // the interstitial.
  EXPECT_CALL(observer_, OnSafeBrowsingMatch(IsUnsafeResourceFor(malware_url)))
      .Times(1);
  EXPECT_CALL(observer_, OnSafeBrowsingHit(IsUnsafeResourceFor(malware_url)))
      .Times(1);
  ui_test_utils::NavigateToURL(browser(), malware_url);
  EXPECT_TRUE(ShowingInterstitialPage());
  Mock::VerifyAndClear(&observer_);
}

}  // namespace

class TestSBClient
    : public base::RefCountedThreadSafe<TestSBClient>,
      public SafeBrowsingDatabaseManager::Client {
 public:
  TestSBClient()
    : threat_type_(SB_THREAT_TYPE_SAFE),
      safe_browsing_service_(g_browser_process->safe_browsing_service()) {
  }

  SBThreatType GetThreatType() const {
    return threat_type_;
  }

  void CheckDownloadUrl(const std::vector<GURL>& url_chain) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&TestSBClient::CheckDownloadUrlOnIOThread,
                   this, url_chain));
    content::RunMessageLoop();  // Will stop in OnCheckDownloadUrlResult.
  }

  void CheckBrowseUrl(const GURL& url) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&TestSBClient::CheckBrowseUrlOnIOThread, this, url));
    content::RunMessageLoop();  // Will stop in OnCheckBrowseUrlResult.
  }

 private:
  friend class base::RefCountedThreadSafe<TestSBClient>;
  ~TestSBClient() override {}

  void CheckDownloadUrlOnIOThread(const std::vector<GURL>& url_chain) {
    bool synchronous_safe_signal =
        safe_browsing_service_->database_manager()->CheckDownloadUrl(url_chain,
                                                                     this);
    if (synchronous_safe_signal) {
      threat_type_ = SB_THREAT_TYPE_SAFE;
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(&TestSBClient::CheckDone, this));
    }
  }

  void CheckBrowseUrlOnIOThread(const GURL& url) {
    // The async CheckDone() hook will not be called when we have a synchronous
    // safe signal, handle it right away.
    bool synchronous_safe_signal =
        safe_browsing_service_->database_manager()->CheckBrowseUrl(url, this);
    if (synchronous_safe_signal) {
      threat_type_ = SB_THREAT_TYPE_SAFE;
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(&TestSBClient::CheckDone, this));
    }
  }

  // Called when the result of checking a download URL is known.
  void OnCheckDownloadUrlResult(const std::vector<GURL>& /* url_chain */,
                                SBThreatType threat_type) override {
    threat_type_ = threat_type;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&TestSBClient::CheckDone, this));
  }

  // Called when the result of checking a browse URL is known.
  void OnCheckBrowseUrlResult(const GURL& /* url */,
                              SBThreatType threat_type,
                              const std::string& /* metadata */) override {
    threat_type_ = threat_type;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&TestSBClient::CheckDone, this));
  }

  void CheckDone() {
    base::MessageLoopForUI::current()->Quit();
  }

  SBThreatType threat_type_;
  SafeBrowsingService* safe_browsing_service_;

  DISALLOW_COPY_AND_ASSIGN(TestSBClient);
};

// These tests use SafeBrowsingService::Client to directly interact with
// SafeBrowsingService.
namespace {

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, CheckDownloadUrl) {
  GURL badbin_url = embedded_test_server()->GetURL(kMalwareFile);
  std::vector<GURL> badbin_urls(1, badbin_url);

  scoped_refptr<TestSBClient> client(new TestSBClient);
  client->CheckDownloadUrl(badbin_urls);

  // Since badbin_url is not in database, it is considered to be safe.
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

  SBFullHashResult full_hash_result;
  GenUrlFullhashResult(badbin_url, safe_browsing_util::BINURL,
                       &full_hash_result);
  SetupResponseForUrl(badbin_url, full_hash_result);

  client->CheckDownloadUrl(badbin_urls);

  // Now, the badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SB_THREAT_TYPE_BINARY_MALWARE_URL, client->GetThreatType());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, CheckUnwantedSoftwareUrl) {
  const GURL bad_url = embedded_test_server()->GetURL(kMalwareFile);
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);

    // Since bad_url is not in database, it is considered to be
    // safe.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

    SBFullHashResult full_hash_result;
    GenUrlFullhashResult(
        bad_url, safe_browsing_util::UNWANTEDURL, &full_hash_result);
    SetupResponseForUrl(bad_url, full_hash_result);

    // Now, the bad_url is not safe since it is added to download
    // database.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED, client->GetThreatType());
  }

  // The unwantedness should survive across multiple clients.
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_UNWANTED, client->GetThreatType());
  }

  // An unwanted URL also marked as malware should be flagged as malware.
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);

    SBFullHashResult full_hash_result;
    GenUrlFullhashResult(
        bad_url, safe_browsing_util::MALWARE, &full_hash_result);
    SetupResponseForUrl(bad_url, full_hash_result);

    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, client->GetThreatType());
  }
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, CheckBrowseUrl) {
  const GURL bad_url = embedded_test_server()->GetURL(kMalwareFile);
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);

    // Since bad_url is not in database, it is considered to be
    // safe.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

    SBFullHashResult full_hash_result;
    GenUrlFullhashResult(
        bad_url, safe_browsing_util::MALWARE, &full_hash_result);
    SetupResponseForUrl(bad_url, full_hash_result);

    // Now, the bad_url is not safe since it is added to download
    // database.
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, client->GetThreatType());
  }

  // The unwantedness should survive across multiple clients.
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);
    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, client->GetThreatType());
  }

  // Adding the unwanted state to an existing malware URL should have no impact
  // (i.e. a malware hit should still prevail).
  {
    scoped_refptr<TestSBClient> client(new TestSBClient);

    SBFullHashResult full_hash_result;
    GenUrlFullhashResult(
        bad_url, safe_browsing_util::UNWANTEDURL, &full_hash_result);
    SetupResponseForUrl(bad_url, full_hash_result);

    client->CheckBrowseUrl(bad_url);
    EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, client->GetThreatType());
  }
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, CheckDownloadUrlRedirects) {
  GURL original_url = embedded_test_server()->GetURL(kEmptyPage);
  GURL badbin_url = embedded_test_server()->GetURL(kMalwareFile);
  GURL final_url = embedded_test_server()->GetURL(kEmptyPage);
  std::vector<GURL> badbin_urls;
  badbin_urls.push_back(original_url);
  badbin_urls.push_back(badbin_url);
  badbin_urls.push_back(final_url);

  scoped_refptr<TestSBClient> client(new TestSBClient);
  client->CheckDownloadUrl(badbin_urls);

  // Since badbin_url is not in database, it is considered to be safe.
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

  SBFullHashResult full_hash_result;
  GenUrlFullhashResult(badbin_url, safe_browsing_util::BINURL,
                       &full_hash_result);
  SetupResponseForUrl(badbin_url, full_hash_result);

  client->CheckDownloadUrl(badbin_urls);

  // Now, the badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SB_THREAT_TYPE_BINARY_MALWARE_URL, client->GetThreatType());
}

#if defined(OS_WIN)
// http://crbug.com/396409
#define MAYBE_CheckDownloadUrlTimedOut DISABLED_CheckDownloadUrlTimedOut
#else
#define MAYBE_CheckDownloadUrlTimedOut CheckDownloadUrlTimedOut
#endif
IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest,
                       MAYBE_CheckDownloadUrlTimedOut) {
  GURL badbin_url = embedded_test_server()->GetURL(kMalwareFile);
  std::vector<GURL> badbin_urls(1, badbin_url);

  scoped_refptr<TestSBClient> client(new TestSBClient);
  SBFullHashResult full_hash_result;
  GenUrlFullhashResult(badbin_url, safe_browsing_util::BINURL,
                       &full_hash_result);
  SetupResponseForUrl(badbin_url, full_hash_result);
  client->CheckDownloadUrl(badbin_urls);

  // badbin_url is not safe since it is added to download database.
  EXPECT_EQ(SB_THREAT_TYPE_BINARY_MALWARE_URL, client->GetThreatType());

  //
  // Now introducing delays and we should hit timeout.
  //
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  base::TimeDelta default_urlcheck_timeout = GetCheckTimeout(sb_service);
  IntroduceGetHashDelay(base::TimeDelta::FromSeconds(1));
  SetCheckTimeout(sb_service, base::TimeDelta::FromMilliseconds(1));
  client->CheckDownloadUrl(badbin_urls);

  // There should be a timeout and the hash would be considered as safe.
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, client->GetThreatType());

  // Need to set the timeout back to the default value.
  SetCheckTimeout(sb_service, default_urlcheck_timeout);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceTest, StartAndStop) {
  CreateCSDService();
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  safe_browsing::ClientSideDetectionService* csd_service =
      sb_service->safe_browsing_detection_service();
  PrefService* pref_service = browser()->profile()->GetPrefs();

  ASSERT_TRUE(sb_service != NULL);
  ASSERT_TRUE(csd_service != NULL);
  ASSERT_TRUE(pref_service != NULL);

  EXPECT_TRUE(pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));

  // SBS might still be starting, make sure this doesn't flake.
  EXPECT_TRUE(sb_service->enabled_by_prefs());
  WaitForIOAndCheckEnabled(sb_service, true);
  EXPECT_TRUE(csd_service->enabled());

  // Add a new Profile. SBS should keep running.
  ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
  scoped_ptr<Profile> profile2(Profile::CreateProfile(
      temp_profile_dir_.path(), NULL, Profile::CREATE_MODE_SYNCHRONOUS));
  ASSERT_TRUE(profile2.get() != NULL);
  StartupTaskRunnerServiceFactory::GetForProfile(profile2.get())->
              StartDeferredTaskRunners();
  PrefService* pref_service2 = profile2->GetPrefs();
  EXPECT_TRUE(pref_service2->GetBoolean(prefs::kSafeBrowsingEnabled));
  // We don't expect the state to have changed, but if it did, wait for it.
  EXPECT_TRUE(sb_service->enabled_by_prefs());
  WaitForIOAndCheckEnabled(sb_service, true);
  EXPECT_TRUE(csd_service->enabled());

  // Change one of the prefs. SBS should keep running.
  pref_service->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  EXPECT_TRUE(sb_service->enabled_by_prefs());
  WaitForIOAndCheckEnabled(sb_service, true);
  EXPECT_TRUE(csd_service->enabled());

  // Change the other pref. SBS should stop now.
  pref_service2->SetBoolean(prefs::kSafeBrowsingEnabled, false);

// TODO(mattm): Remove this when crbug.com/461493 is fixed.
#if defined(OS_CHROMEOS)
  // On Chrome OS we should disable safe browsing for signin profile.
  EXPECT_TRUE(sb_service->enabled_by_prefs());
  WaitForIOAndCheckEnabled(sb_service, true);
  EXPECT_TRUE(csd_service->enabled());
  chromeos::ProfileHelper::GetSigninProfile()
      ->GetOriginalProfile()
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  WaitForIOThread();
#endif
  EXPECT_FALSE(sb_service->enabled_by_prefs());
  WaitForIOAndCheckEnabled(sb_service, false);
  EXPECT_FALSE(csd_service->enabled());

  // Turn it back on. SBS comes back.
  pref_service2->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  EXPECT_TRUE(sb_service->enabled_by_prefs());
  WaitForIOAndCheckEnabled(sb_service, true);
  EXPECT_TRUE(csd_service->enabled());

  // Delete the Profile. SBS stops again.
  pref_service2 = NULL;
  profile2.reset();
  EXPECT_FALSE(sb_service->enabled_by_prefs());
  WaitForIOAndCheckEnabled(sb_service, false);
  EXPECT_FALSE(csd_service->enabled());
}

}  // namespace

class SafeBrowsingServiceShutdownTest : public SafeBrowsingServiceTest {
 public:
  void TearDown() override {
    // Browser should be fully torn down by now, so we can safely check these
    // counters.
    EXPECT_EQ(1, TestProtocolManager::create_count());
    EXPECT_EQ(1, TestProtocolManager::delete_count());

    SafeBrowsingServiceTest::TearDown();
  }

  // An observer that returns back to test code after a new profile is
  // initialized.
  void OnUnblockOnProfileCreation(Profile* profile,
                                  Profile::CreateStatus status) {
    if (status == Profile::CREATE_STATUS_INITIALIZED) {
      profile2_ = profile;
      base::MessageLoop::current()->Quit();
    }
  }

 protected:
  Profile* profile2_;
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingServiceShutdownTest,
                       DontStartAfterShutdown) {
  CreateCSDService();
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  safe_browsing::ClientSideDetectionService* csd_service =
      sb_service->safe_browsing_detection_service();
  PrefService* pref_service = browser()->profile()->GetPrefs();

  ASSERT_TRUE(sb_service != NULL);
  ASSERT_TRUE(csd_service != NULL);
  ASSERT_TRUE(pref_service != NULL);

  EXPECT_TRUE(pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));

  // SBS might still be starting, make sure this doesn't flake.
  WaitForIOThread();
  EXPECT_EQ(1, TestProtocolManager::create_count());
  EXPECT_EQ(0, TestProtocolManager::delete_count());

  // Create an additional profile.  We need to use the ProfileManager so that
  // the profile will get destroyed in the normal browser shutdown process.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());
  profile_manager->CreateProfileAsync(
      temp_profile_dir_.path(),
      base::Bind(&SafeBrowsingServiceShutdownTest::OnUnblockOnProfileCreation,
                 this),
      base::string16(), base::string16(), std::string());

  // Spin to allow profile creation to take place, loop is terminated
  // by OnUnblockOnProfileCreation when the profile is created.
  content::RunMessageLoop();

  PrefService* pref_service2 = profile2_->GetPrefs();
  EXPECT_TRUE(pref_service2->GetBoolean(prefs::kSafeBrowsingEnabled));

  // We don't expect the state to have changed, but if it did, wait for it.
  WaitForIOThread();
  EXPECT_EQ(1, TestProtocolManager::create_count());
  EXPECT_EQ(0, TestProtocolManager::delete_count());

  // End the test, shutting down the browser.
  // SafeBrowsingServiceShutdownTest::TearDown will check the create_count and
  // delete_count again.
}

class SafeBrowsingDatabaseManagerCookieTest : public InProcessBrowserTest {
 public:
  SafeBrowsingDatabaseManagerCookieTest() {}

  void SetUp() override {
    // We need to start the test server to get the host&port in the url.
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&SafeBrowsingDatabaseManagerCookieTest::HandleRequest));

    // Point to the testing server for all SafeBrowsing requests.
    GURL url_prefix = embedded_test_server()->GetURL("/testpath");
    sb_factory_.reset(new TestSafeBrowsingServiceFactory(url_prefix.spec()));
    SafeBrowsingService::RegisterFactory(sb_factory_.get());

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();

    SafeBrowsingService::RegisterFactory(NULL);
  }

  bool SetUpUserDataDirectory() override {
    base::FilePath cookie_path(
        SafeBrowsingService::GetCookieFilePathForTesting());
    EXPECT_FALSE(base::PathExists(cookie_path));

    base::FilePath test_dir;
    if (!PathService::Get(chrome::DIR_TEST_DATA, &test_dir)) {
      EXPECT_TRUE(false);
      return false;
    }

    // Initialize the SafeBrowsing cookies with a pre-created cookie store.  It
    // contains a single cookie, for domain 127.0.0.1, with value a=b, and
    // expires in 2038.
    base::FilePath initial_cookies = test_dir.AppendASCII("safe_browsing")
        .AppendASCII("Safe Browsing Cookies");
    if (!base::CopyFile(initial_cookies, cookie_path)) {
      EXPECT_TRUE(false);
      return false;
    }

    sql::Connection db;
    if (!db.Open(cookie_path)) {
      EXPECT_TRUE(false);
      return false;
    }
    // Ensure the host value in the cookie file matches the test server we will
    // be connecting to.
    sql::Statement smt(db.GetUniqueStatement(
        "UPDATE cookies SET host_key = ?"));
    if (!smt.is_valid()) {
      EXPECT_TRUE(false);
      return false;
    }
    if (!smt.BindString(0, embedded_test_server()->base_url().host())) {
      EXPECT_TRUE(false);
      return false;
    }
    if (!smt.Run()) {
      EXPECT_TRUE(false);
      return false;
    }

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

  void TearDownInProcessBrowserTestFixture() override {
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();

    sql::Connection db;
    base::FilePath cookie_path(
        SafeBrowsingService::GetCookieFilePathForTesting());
    ASSERT_TRUE(db.Open(cookie_path));

    sql::Statement smt(db.GetUniqueStatement(
        "SELECT name, value FROM cookies ORDER BY name"));
    ASSERT_TRUE(smt.is_valid());

    ASSERT_TRUE(smt.Step());
    ASSERT_EQ("a", smt.ColumnString(0));
    ASSERT_EQ("b", smt.ColumnString(1));
    ASSERT_TRUE(smt.Step());
    ASSERT_EQ("c", smt.ColumnString(0));
    ASSERT_EQ("d", smt.ColumnString(1));
    EXPECT_FALSE(smt.Step());
  }

  void SetUpOnMainThread() override {
    sb_service_ = g_browser_process->safe_browsing_service();
    ASSERT_TRUE(sb_service_.get() != NULL);
  }

  void TearDownOnMainThread() override { sb_service_ = NULL; }

  void ForceUpdate() {
    sb_service_->protocol_manager()->ForceScheduleNextUpdate(
        base::TimeDelta::FromSeconds(0));
  }

  scoped_refptr<SafeBrowsingService> sb_service_;

 private:
  static scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, "/testpath/",
                          base::CompareCase::SENSITIVE)) {
      ADD_FAILURE() << "bad path";
      return nullptr;
    }

    auto cookie_it = request.headers.find("Cookie");
    if (cookie_it == request.headers.end()) {
      ADD_FAILURE() << "no cookie header";
      return nullptr;
    }

    net::cookie_util::ParsedRequestCookies req_cookies;
    net::cookie_util::ParseRequestCookieLine(cookie_it->second, &req_cookies);
    if (req_cookies.size() != 1) {
      ADD_FAILURE() << "req_cookies.size() = " << req_cookies.size();
      return nullptr;
    }
    const net::cookie_util::ParsedRequestCookie expected_cookie(
        std::make_pair("a", "b"));
    const net::cookie_util::ParsedRequestCookie& cookie = req_cookies.front();
    if (cookie != expected_cookie) {
      ADD_FAILURE() << "bad cookie " << cookie.first << "=" << cookie.second;
      return nullptr;
    }

    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_content("foo");
    http_response->set_content_type("text/plain");
    http_response->AddCustomHeader(
        "Set-Cookie", "c=d; Expires=Fri, 01 Jan 2038 01:01:01 GMT");
    return http_response.Pass();
  }

  scoped_ptr<TestSafeBrowsingServiceFactory> sb_factory_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseManagerCookieTest);
};

// Test that a Local Safe Browsing database update request both sends cookies
// and can save cookies.
IN_PROC_BROWSER_TEST_F(SafeBrowsingDatabaseManagerCookieTest,
                       TestSBUpdateCookies) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE,
      content::Source<SafeBrowsingDatabaseManager>(
          sb_service_->database_manager().get()));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SafeBrowsingDatabaseManagerCookieTest::ForceUpdate, this));
  observer.Wait();
}
