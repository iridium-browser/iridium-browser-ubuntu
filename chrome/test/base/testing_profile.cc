// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/prefs/testing_pref_store.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/chrome_fallback_icon_client_factory.h"
#include "chrome/browser/favicon/fallback_icon_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/history_index_restore_observer.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/favicon/core/fallback_icon_service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/content/browser/content_visit_delegate.h"
#include "components/history/content/browser/history_database_helper.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/policy/core/common/policy_service.h"
#include "components/ui/zoom/zoom_event_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/zoom_level_delegate.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema.h"
#else
#include "components/policy/core/common/policy_service_stub.h"
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_system.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

using base::Time;
using bookmarks::BookmarkModel;
using content::BrowserThread;
using content::DownloadManagerDelegate;
using testing::NiceMock;
using testing::Return;

namespace {

// Task used to make sure history has finished processing a request. Intended
// for use with BlockUntilHistoryProcessesPendingRequests.

class QuittingHistoryDBTask : public history::HistoryDBTask {
 public:
  QuittingHistoryDBTask() {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    return true;
  }

  void DoneRunOnMainThread() override { base::MessageLoop::current()->Quit(); }

 private:
  ~QuittingHistoryDBTask() override {}

  DISALLOW_COPY_AND_ASSIGN(QuittingHistoryDBTask);
};

class TestExtensionURLRequestContext : public net::URLRequestContext {
 public:
  TestExtensionURLRequestContext() {
    net::CookieMonster* cookie_monster =
        content::CreateCookieStore(content::CookieStoreConfig())->
            GetCookieMonster();
    const char* const schemes[] = {extensions::kExtensionScheme};
    cookie_monster->SetCookieableSchemes(schemes, arraysize(schemes));
    set_cookie_store(cookie_monster);
  }

  ~TestExtensionURLRequestContext() override { AssertNoURLRequests(); }
};

class TestExtensionURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  net::URLRequestContext* GetURLRequestContext() override {
    if (!context_.get())
      context_.reset(new TestExtensionURLRequestContext());
    return context_.get();
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  }

 protected:
  ~TestExtensionURLRequestContextGetter() override {}

 private:
  scoped_ptr<net::URLRequestContext> context_;
};

scoped_ptr<KeyedService> BuildHistoryService(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return make_scoped_ptr(new history::HistoryService(
      make_scoped_ptr(new ChromeHistoryClient(
          BookmarkModelFactory::GetForProfile(profile))),
      make_scoped_ptr(new history::ContentVisitDelegate(profile))));
}

scoped_ptr<KeyedService> BuildInMemoryURLIndex(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_ptr<InMemoryURLIndex> in_memory_url_index(new InMemoryURLIndex(
      BookmarkModelFactory::GetForProfile(profile),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS),
      content::BrowserThread::GetBlockingPool(), profile->GetPath(),
      profile->GetPrefs()->GetString(prefs::kAcceptLanguages),
      SchemeSet()));
  in_memory_url_index->Init();
  return in_memory_url_index.Pass();
}

scoped_ptr<KeyedService> BuildBookmarkModel(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  ChromeBookmarkClient* bookmark_client =
      ChromeBookmarkClientFactory::GetForProfile(profile);
  scoped_ptr<BookmarkModel> bookmark_model(new BookmarkModel(bookmark_client));
  bookmark_client->Init(bookmark_model.get());
  bookmark_model->Load(profile->GetPrefs(),
                       profile->GetPrefs()->GetString(prefs::kAcceptLanguages),
                       profile->GetPath(),
                       profile->GetIOTaskRunner(),
                       content::BrowserThread::GetMessageLoopProxyForThread(
                           content::BrowserThread::UI));
  return bookmark_model.Pass();
}

void TestProfileErrorCallback(WebDataServiceWrapper::ErrorType error_type,
                              sql::InitStatus status) {
  NOTREACHED();
}

scoped_ptr<KeyedService> BuildWebDataService(content::BrowserContext* context) {
  const base::FilePath& context_path = context->GetPath();
  return make_scoped_ptr(new WebDataServiceWrapper(
      context_path, g_browser_process->GetApplicationLocale(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
      sync_start_util::GetFlareForSyncableService(context_path),
      &TestProfileErrorCallback));
}

}  // namespace

// static
#if defined(OS_CHROMEOS)
// Must be kept in sync with
// ChromeBrowserMainPartsChromeos::PreEarlyInitialization.
const char TestingProfile::kTestUserProfileDir[] = "test-user";
#else
const char TestingProfile::kTestUserProfileDir[] = "Default";
#endif

TestingProfile::TestingProfile()
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(false),
      last_session_exited_cleanly_(true),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(NULL) {
  CreateTempProfileDir();
  profile_path_ = temp_dir_.path();

  Init();
  FinishInit();
}

TestingProfile::TestingProfile(const base::FilePath& path)
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(false),
      last_session_exited_cleanly_(true),
      profile_path_(path),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(NULL) {
  Init();
  FinishInit();
}

TestingProfile::TestingProfile(const base::FilePath& path,
                               Delegate* delegate)
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(false),
      last_session_exited_cleanly_(true),
      profile_path_(path),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(delegate) {
  Init();
  if (delegate_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&TestingProfile::FinishInit, base::Unretained(this)));
  } else {
    FinishInit();
  }
}

TestingProfile::TestingProfile(
    const base::FilePath& path,
    Delegate* delegate,
#if defined(ENABLE_EXTENSIONS)
    scoped_refptr<ExtensionSpecialStoragePolicy> extension_policy,
#endif
    scoped_ptr<PrefServiceSyncable> prefs,
    TestingProfile* parent,
    bool guest_session,
    const std::string& supervised_user_id,
    scoped_ptr<policy::PolicyService> policy_service,
    const TestingFactories& factories)
    : start_time_(Time::Now()),
      prefs_(prefs.release()),
      testing_prefs_(NULL),
      force_incognito_(false),
      original_profile_(parent),
      guest_session_(guest_session),
      last_session_exited_cleanly_(true),
#if defined(ENABLE_EXTENSIONS)
      extension_special_storage_policy_(extension_policy),
#endif
      profile_path_(path),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(delegate),
      policy_service_(policy_service.release()) {
  if (parent)
    parent->SetOffTheRecordProfile(scoped_ptr<Profile>(this));

  // If no profile path was supplied, create one.
  if (profile_path_.empty()) {
    CreateTempProfileDir();
    profile_path_ = temp_dir_.path();
  }

  // Set any testing factories prior to initializing the services.
  for (TestingFactories::const_iterator it = factories.begin();
       it != factories.end(); ++it) {
    it->first->SetTestingFactory(this, it->second);
  }

  Init();
  // If caller supplied a delegate, delay the FinishInit invocation until other
  // tasks have run.
  // TODO(atwilson): See if this is still required once we convert the current
  // users of the constructor that takes a Delegate* param.
  if (delegate_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&TestingProfile::FinishInit, base::Unretained(this)));
  } else {
    FinishInit();
  }

  SetSupervisedUserId(supervised_user_id);
}

void TestingProfile::CreateTempProfileDir() {
  if (!temp_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create unique temporary directory.";

    // Fallback logic in case we fail to create unique temporary directory.
    base::FilePath system_tmp_dir;
    bool success = PathService::Get(base::DIR_TEMP, &system_tmp_dir);

    // We're severly screwed if we can't get the system temporary
    // directory. Die now to avoid writing to the filesystem root
    // or other bad places.
    CHECK(success);

    base::FilePath fallback_dir(
        system_tmp_dir.AppendASCII("TestingProfilePath"));
    base::DeleteFile(fallback_dir, true);
    base::CreateDirectory(fallback_dir);
    if (!temp_dir_.Set(fallback_dir)) {
      // That shouldn't happen, but if it does, try to recover.
      LOG(ERROR) << "Failed to use a fallback temporary directory.";

      // We're screwed if this fails, see CHECK above.
      CHECK(temp_dir_.Set(system_tmp_dir));
    }
  }
}

void TestingProfile::Init() {
  // If threads have been initialized, we should be on the UI thread.
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  set_is_guest_profile(guest_session_);

#if defined(OS_ANDROID)
  // Make sure token service knows its running in tests.
  OAuth2TokenServiceDelegateAndroid::set_is_testing_profile();
#endif

  // Normally this would happen during browser startup, but for tests
  // we need to trigger creation of Profile-related services.
  ChromeBrowserMainExtraPartsProfiles::
      EnsureBrowserContextKeyedServiceFactoriesBuilt();

  if (prefs_.get())
    user_prefs::UserPrefs::Set(this, prefs_.get());
  else if (IsOffTheRecord())
    CreateIncognitoPrefService();
  else
    CreateTestingPrefService();

  if (!base::PathExists(profile_path_))
    base::CreateDirectory(profile_path_);

  // TODO(joaodasilva): remove this once this PKS isn't created in ProfileImpl
  // anymore, after converting the PrefService to a PKS. Until then it must
  // be associated with a TestingProfile too.
  if (!IsOffTheRecord())
    CreateProfilePolicyConnector();

  extensions_path_ = profile_path_.AppendASCII("Extensions");

#if defined(ENABLE_EXTENSIONS)
  // Note that the GetPrefs() creates a TestingPrefService, therefore
  // the extension controlled pref values set in ExtensionPrefs
  // are not reflected in the pref service. One would need to
  // inject a new ExtensionPrefStore(extension_pref_value_map, false).
  bool extensions_disabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableExtensions);
  scoped_ptr<extensions::ExtensionPrefs> extension_prefs(
      extensions::ExtensionPrefs::Create(
          this, GetPrefs(), extensions_path_,
          ExtensionPrefValueMapFactory::GetForBrowserContext(this),
          extensions_disabled,
          std::vector<extensions::ExtensionPrefsObserver*>()));
  extensions::ExtensionPrefsFactory::GetInstance()->SetInstanceForTesting(
      this, extension_prefs.Pass());

  extensions::ExtensionSystemFactory::GetInstance()->SetTestingFactory(
      this, extensions::TestExtensionSystem::Build);

  extensions::EventRouterFactory::GetInstance()->SetTestingFactory(this,
                                                                   nullptr);
#endif

  // Prefs for incognito profiles are set in CreateIncognitoPrefService() by
  // simulating ProfileImpl::GetOffTheRecordPrefs().
  if (!IsOffTheRecord()) {
    DCHECK(!original_profile_);
    user_prefs::PrefRegistrySyncable* pref_registry =
        static_cast<user_prefs::PrefRegistrySyncable*>(
            prefs_->DeprecatedGetPrefRegistry());
    browser_context_dependency_manager_->
        RegisterProfilePrefsForServices(this, pref_registry);
  }

  browser_context_dependency_manager_->CreateBrowserContextServicesForTest(
      this);

#if defined(ENABLE_SUPERVISED_USERS)
  if (!IsOffTheRecord()) {
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(this);
    TestingPrefStore* store = new TestingPrefStore();
    settings_service->Init(store);
    store->SetInitializationCompleted();
  }
#endif

  profile_name_ = "testing_profile";
}

void TestingProfile::FinishInit() {
  DCHECK(content::NotificationService::current());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(static_cast<Profile*>(this)),
      content::NotificationService::NoDetails());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->InitProfileUserPrefs(this);

  if (delegate_)
    delegate_->OnProfileCreated(this, true, false);
}

TestingProfile::~TestingProfile() {
  // Revert to non-incognito mode before shutdown.
  force_incognito_ = false;

  // If this profile owns an incognito profile, tear it down first.
  incognito_profile_.reset();

  // Any objects holding live URLFetchers should be deleted before teardown.
  TemplateURLFetcherFactory::ShutdownForProfile(this);

  MaybeSendDestroyedNotification();

  browser_context_dependency_manager_->DestroyBrowserContextServices(this);

  if (host_content_settings_map_.get())
    host_content_settings_map_->ShutdownOnUIThread();

  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();
  // Failing a post == leaks == heapcheck failure. Make that an immediate test
  // failure.
  if (resource_context_) {
    CHECK(BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
                                    resource_context_));
    resource_context_ = NULL;
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }
}

void TestingProfile::CreateFaviconService() {
  // It is up to the caller to create the history service if one is needed.
  FaviconServiceFactory::GetInstance()->SetTestingFactory(
      this, FaviconServiceFactory::GetDefaultFactory());
}

bool TestingProfile::CreateHistoryService(bool delete_file, bool no_db) {
  DestroyHistoryService();
  if (delete_file) {
    base::FilePath path = GetPath();
    path = path.Append(history::kHistoryFilename);
    if (!base::DeleteFile(path, false) || base::PathExists(path))
      return false;
  }
  // This will create and init the history service.
  history::HistoryService* history_service =
      static_cast<history::HistoryService*>(
          HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              this, BuildHistoryService));
  if (!history_service->Init(
          no_db, GetPrefs()->GetString(prefs::kAcceptLanguages),
          history::HistoryDatabaseParamsForPath(GetPath()))) {
    HistoryServiceFactory::GetInstance()->SetTestingFactory(this, nullptr);
    return false;
  }
  // Some tests expect that CreateHistoryService() will also make the
  // InMemoryURLIndex available.
  InMemoryURLIndexFactory::GetInstance()->SetTestingFactory(
      this, BuildInMemoryURLIndex);
  // Disable WebHistoryService by default, since it makes network requests.
  WebHistoryServiceFactory::GetInstance()->SetTestingFactory(this, nullptr);
  return true;
}

void TestingProfile::DestroyHistoryService() {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(this);
  if (!history_service)
    return;

  history_service->ClearCachedDataForContextID(0);
  history_service->SetOnBackendDestroyTask(base::MessageLoop::QuitClosure());
  history_service->Cleanup();
  HistoryServiceFactory::ShutdownForProfile(this);

  // Wait for the backend class to terminate before deleting the files and
  // moving to the next test. Note: if this never terminates, somebody is
  // probably leaking a reference to the history backend, so it never calls
  // our destroy task.
  base::MessageLoop::current()->Run();

  // Make sure we don't have any event pending that could disrupt the next
  // test.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::MessageLoop::QuitClosure());
  base::MessageLoop::current()->Run();
}

void TestingProfile::CreateBookmarkModel(bool delete_file) {
  if (delete_file) {
    base::FilePath path = GetPath().Append(bookmarks::kBookmarksFileName);
    base::DeleteFile(path, false);
  }
  ManagedBookmarkServiceFactory::GetInstance()->SetTestingFactory(
      this, ManagedBookmarkServiceFactory::GetDefaultFactory());
  ChromeBookmarkClientFactory::GetInstance()->SetTestingFactory(
      this, ChromeBookmarkClientFactory::GetDefaultFactory());
  // This creates the BookmarkModel.
  ignore_result(BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
      this, BuildBookmarkModel));
}

void TestingProfile::CreateWebDataService() {
  WebDataServiceFactory::GetInstance()->SetTestingFactory(
      this, BuildWebDataService);
}

void TestingProfile::BlockUntilHistoryIndexIsRefreshed() {
  // Only get the history service if it actually exists since the caller of the
  // test should explicitly call CreateHistoryService to build it.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(this);
  DCHECK(history_service);
  InMemoryURLIndex* index = InMemoryURLIndexFactory::GetForProfile(this);
  if (!index || index->restored())
    return;
  base::RunLoop run_loop;
  HistoryIndexRestoreObserver observer(
      content::GetQuitTaskForRunLoop(&run_loop));
  index->set_restore_cache_observer(&observer);
  run_loop.Run();
  index->set_restore_cache_observer(NULL);
  DCHECK(index->restored());
}

void TestingProfile::SetGuestSession(bool guest) {
  guest_session_ = guest;
}

base::FilePath TestingProfile::GetPath() const {
  return profile_path_;
}

scoped_ptr<content::ZoomLevelDelegate> TestingProfile::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return make_scoped_ptr(new chrome::ChromeZoomLevelPrefs(
      GetPrefs(), GetPath(), partition_path,
      ui_zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr()));
}

scoped_refptr<base::SequencedTaskRunner> TestingProfile::GetIOTaskRunner() {
  return base::MessageLoop::current()->task_runner();
}

TestingPrefServiceSyncable* TestingProfile::GetTestingPrefService() {
  DCHECK(prefs_);
  DCHECK(testing_prefs_);
  return testing_prefs_;
}

TestingProfile* TestingProfile::AsTestingProfile() {
  return this;
}

std::string TestingProfile::GetProfileUserName() const {
  return profile_name_;
}

Profile::ProfileType TestingProfile::GetProfileType() const {
  if (guest_session_)
    return GUEST_PROFILE;
  if (force_incognito_ || original_profile_)
    return INCOGNITO_PROFILE;
  return REGULAR_PROFILE;
}

bool TestingProfile::IsOffTheRecord() const {
  return force_incognito_ || original_profile_;
}

void TestingProfile::SetOffTheRecordProfile(scoped_ptr<Profile> profile) {
  DCHECK(!IsOffTheRecord());
  DCHECK_EQ(this, profile->GetOriginalProfile());
  incognito_profile_ = profile.Pass();
}

Profile* TestingProfile::GetOffTheRecordProfile() {
  if (IsOffTheRecord())
    return this;
  if (!incognito_profile_)
    TestingProfile::Builder().BuildIncognito(this);
  return incognito_profile_.get();
}

bool TestingProfile::HasOffTheRecordProfile() {
  return incognito_profile_.get() != NULL;
}

Profile* TestingProfile::GetOriginalProfile() {
  if (original_profile_)
    return original_profile_;
  return this;
}

void TestingProfile::SetSupervisedUserId(const std::string& id) {
  supervised_user_id_ = id;
  if (!id.empty())
    GetPrefs()->SetString(prefs::kSupervisedUserId, id);
  else
    GetPrefs()->ClearPref(prefs::kSupervisedUserId);
}

bool TestingProfile::IsSupervised() const {
  return !supervised_user_id_.empty();
}

bool TestingProfile::IsChild() const {
#if defined(ENABLE_SUPERVISED_USERS)
  return supervised_user_id_ == supervised_users::kChildAccountSUID;
#else
  return false;
#endif
}

bool TestingProfile::IsLegacySupervised() const {
  return IsSupervised() && !IsChild();
}

#if defined(ENABLE_EXTENSIONS)
void TestingProfile::SetExtensionSpecialStoragePolicy(
    ExtensionSpecialStoragePolicy* extension_special_storage_policy) {
  extension_special_storage_policy_ = extension_special_storage_policy;
}
#endif

ExtensionSpecialStoragePolicy*
TestingProfile::GetExtensionSpecialStoragePolicy() {
#if defined(ENABLE_EXTENSIONS)
  if (!extension_special_storage_policy_.get())
    extension_special_storage_policy_ = new ExtensionSpecialStoragePolicy(NULL);
  return extension_special_storage_policy_.get();
#else
  return NULL;
#endif
}

net::CookieMonster* TestingProfile::GetCookieMonster() {
  if (!GetRequestContext())
    return NULL;
  return GetRequestContext()->GetURLRequestContext()->cookie_store()->
      GetCookieMonster();
}

void TestingProfile::CreateTestingPrefService() {
  DCHECK(!prefs_.get());
  testing_prefs_ = new TestingPrefServiceSyncable();
  prefs_.reset(testing_prefs_);
  user_prefs::UserPrefs::Set(this, prefs_.get());
  chrome::RegisterUserProfilePrefs(testing_prefs_->registry());
}

void TestingProfile::CreateIncognitoPrefService() {
  DCHECK(original_profile_);
  DCHECK(!testing_prefs_);
  // Simplified version of ProfileImpl::GetOffTheRecordPrefs(). Note this
  // leaves testing_prefs_ unset.
  prefs_.reset(original_profile_->prefs_->CreateIncognitoPrefService(NULL));
  user_prefs::UserPrefs::Set(this, prefs_.get());
}

void TestingProfile::CreateProfilePolicyConnector() {
#if defined(ENABLE_CONFIGURATION_POLICY)
  schema_registry_service_ =
      policy::SchemaRegistryServiceFactory::CreateForContext(
          this, policy::Schema(), NULL);
  CHECK_EQ(schema_registry_service_.get(),
           policy::SchemaRegistryServiceFactory::GetForContext(this));
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

if (!policy_service_) {
#if defined(ENABLE_CONFIGURATION_POLICY)
    std::vector<policy::ConfigurationPolicyProvider*> providers;
    policy_service_.reset(new policy::PolicyServiceImpl(providers));
#else
    policy_service_.reset(new policy::PolicyServiceStub());
#endif
  }
  profile_policy_connector_.reset(new policy::ProfilePolicyConnector());
  profile_policy_connector_->InitForTesting(policy_service_.Pass());
  policy::ProfilePolicyConnectorFactory::GetInstance()->SetServiceForTesting(
      this, profile_policy_connector_.get());
  CHECK_EQ(profile_policy_connector_.get(),
           policy::ProfilePolicyConnectorFactory::GetForBrowserContext(this));
}

PrefService* TestingProfile::GetPrefs() {
  DCHECK(prefs_);
  return prefs_.get();
}

const PrefService* TestingProfile::GetPrefs() const {
  DCHECK(prefs_);
  return prefs_.get();
}

chrome::ChromeZoomLevelPrefs* TestingProfile::GetZoomLevelPrefs() {
  return static_cast<chrome::ChromeZoomLevelPrefs*>(
      GetDefaultStoragePartition(this)->GetZoomLevelDelegate());
}

DownloadManagerDelegate* TestingProfile::GetDownloadManagerDelegate() {
  return NULL;
}

net::URLRequestContextGetter* TestingProfile::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter* TestingProfile::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return new net::TestURLRequestContextGetter(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
}

net::URLRequestContextGetter* TestingProfile::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  content::RenderProcessHost* rph = content::RenderProcessHost::FromID(
      renderer_child_id);
  return rph->GetStoragePartition()->GetURLRequestContext();
}

net::URLRequestContextGetter* TestingProfile::GetMediaRequestContext() {
  return NULL;
}

net::URLRequestContextGetter*
TestingProfile::GetMediaRequestContextForRenderProcess(
    int renderer_child_id) {
  return NULL;
}

net::URLRequestContextGetter*
TestingProfile::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return NULL;
}

net::URLRequestContextGetter* TestingProfile::GetRequestContextForExtensions() {
  if (!extensions_request_context_.get())
    extensions_request_context_ = new TestExtensionURLRequestContextGetter();
  return extensions_request_context_.get();
}

net::SSLConfigService* TestingProfile::GetSSLConfigService() {
  if (!GetRequestContext())
    return NULL;
  return GetRequestContext()->GetURLRequestContext()->ssl_config_service();
}

net::URLRequestContextGetter*
TestingProfile::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  // We don't test storage partitions here yet, so returning the same dummy
  // context is sufficient for now.
  return GetRequestContext();
}

content::ResourceContext* TestingProfile::GetResourceContext() {
  if (!resource_context_)
    resource_context_ = new content::MockResourceContext();
  return resource_context_;
}

HostContentSettingsMap* TestingProfile::GetHostContentSettingsMap() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(GetPrefs(), false);
#if defined(ENABLE_EXTENSIONS)
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(this)->extension_service();
    if (extension_service) {
      extension_service->RegisterContentSettings(
          host_content_settings_map_.get());
    }
#endif
  }
  return host_content_settings_map_.get();
}

content::BrowserPluginGuestManager* TestingProfile::GetGuestManager() {
#if defined(ENABLE_EXTENSIONS)
  return guest_view::GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

content::PushMessagingService* TestingProfile::GetPushMessagingService() {
  return NULL;
}

bool TestingProfile::IsSameProfile(Profile *p) {
  return this == p;
}

base::Time TestingProfile::GetStartTime() const {
  return start_time_;
}

base::FilePath TestingProfile::last_selected_directory() {
  return last_selected_directory_;
}

void TestingProfile::set_last_selected_directory(const base::FilePath& path) {
  last_selected_directory_ = path;
}

PrefProxyConfigTracker* TestingProfile::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_.get()) {
    // TestingProfile is used in unit tests, where local state is not available.
    pref_proxy_config_tracker_.reset(
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(GetPrefs(),
                                                                   NULL));
  }
  return pref_proxy_config_tracker_.get();
}

void TestingProfile::BlockUntilHistoryProcessesPendingRequests() {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(this,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(history_service);
  DCHECK(base::MessageLoop::current());

  base::CancelableTaskTracker tracker;
  history_service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new QuittingHistoryDBTask()),
      &tracker);
  base::MessageLoop::current()->Run();
}

chrome_browser_net::Predictor* TestingProfile::GetNetworkPredictor() {
  return NULL;
}

DevToolsNetworkControllerHandle*
TestingProfile::GetDevToolsNetworkControllerHandle() {
  return NULL;
}

void TestingProfile::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  if (!completion.is_null()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, completion);
  }
}

GURL TestingProfile::GetHomePage() {
  return GURL(chrome::kChromeUINewTabURL);
}

PrefService* TestingProfile::GetOffTheRecordPrefs() {
  return NULL;
}

storage::SpecialStoragePolicy* TestingProfile::GetSpecialStoragePolicy() {
#if defined(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

content::SSLHostStateDelegate* TestingProfile::GetSSLHostStateDelegate() {
  return NULL;
}

content::PermissionManager* TestingProfile::GetPermissionManager() {
  return NULL;
}

bool TestingProfile::WasCreatedByVersionOrLater(const std::string& version) {
  return true;
}

bool TestingProfile::IsGuestSession() const {
  return guest_session_;
}

Profile::ExitType TestingProfile::GetLastSessionExitType() {
  return last_session_exited_cleanly_ ? EXIT_NORMAL : EXIT_CRASHED;
}

TestingProfile::Builder::Builder()
    : build_called_(false),
      delegate_(NULL),
      guest_session_(false) {
}

TestingProfile::Builder::~Builder() {
}

void TestingProfile::Builder::SetPath(const base::FilePath& path) {
  path_ = path;
}

void TestingProfile::Builder::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

#if defined(ENABLE_EXTENSIONS)
void TestingProfile::Builder::SetExtensionSpecialStoragePolicy(
    scoped_refptr<ExtensionSpecialStoragePolicy> policy) {
  extension_policy_ = policy;
}
#endif

void TestingProfile::Builder::SetPrefService(
    scoped_ptr<PrefServiceSyncable> prefs) {
  pref_service_ = prefs.Pass();
}

void TestingProfile::Builder::SetGuestSession() {
  guest_session_ = true;
}

void TestingProfile::Builder::SetSupervisedUserId(
    const std::string& supervised_user_id) {
  supervised_user_id_ = supervised_user_id;
}

void TestingProfile::Builder::SetPolicyService(
    scoped_ptr<policy::PolicyService> policy_service) {
  policy_service_ = policy_service.Pass();
}

void TestingProfile::Builder::AddTestingFactory(
    BrowserContextKeyedServiceFactory* service_factory,
    BrowserContextKeyedServiceFactory::TestingFactoryFunction callback) {
  testing_factories_.push_back(std::make_pair(service_factory, callback));
}

scoped_ptr<TestingProfile> TestingProfile::Builder::Build() {
  DCHECK(!build_called_);
  build_called_ = true;

  return scoped_ptr<TestingProfile>(new TestingProfile(path_,
                                                       delegate_,
#if defined(ENABLE_EXTENSIONS)
                                                       extension_policy_,
#endif
                                                       pref_service_.Pass(),
                                                       NULL,
                                                       guest_session_,
                                                       supervised_user_id_,
                                                       policy_service_.Pass(),
                                                       testing_factories_));
}

TestingProfile* TestingProfile::Builder::BuildIncognito(
    TestingProfile* original_profile) {
  DCHECK(!build_called_);
  DCHECK(original_profile);
  build_called_ = true;

  // Note: Owned by |original_profile|.
  return new TestingProfile(path_,
                            delegate_,
#if defined(ENABLE_EXTENSIONS)
                            extension_policy_,
#endif
                            pref_service_.Pass(),
                            original_profile,
                            guest_session_,
                            supervised_user_id_,
                            policy_service_.Pass(),
                            testing_factories_);
}
