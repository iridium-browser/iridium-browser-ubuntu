// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/glue/invalidation_helper.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_invalidation_service.h"
#include "chrome/browser/sync/test/integration/p2p_invalidation_forwarder.h"
#include "chrome/browser/sync/test/integration/p2p_sync_refresher.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/invalidation_switches.h"
#include "components/invalidation/p2p_invalidation_service.h"
#include "components/invalidation/p2p_invalidator.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/os_crypt/os_crypt.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/engine/sync_scheduler_impl.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/fake_server.h"
#include "sync/test/fake_server/fake_server_network_resources.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using content::BrowserThread;

namespace switches {
const char kPasswordFileForTest[] = "password-file-for-test";
const char kSyncUserForTest[] = "sync-user-for-test";
const char kSyncPasswordForTest[] = "sync-password-for-test";
const char kSyncServerCommandLine[] = "sync-server-command-line";
}

namespace {

// Helper class that checks whether a sync test server is running or not.
class SyncServerStatusChecker : public net::URLFetcherDelegate {
 public:
  SyncServerStatusChecker() : running_(false) {}

  void OnURLFetchComplete(const net::URLFetcher* source) override {
    std::string data;
    source->GetResponseAsString(&data);
    running_ =
        (source->GetStatus().status() == net::URLRequestStatus::SUCCESS &&
        source->GetResponseCode() == 200 && data.find("ok") == 0);
    base::MessageLoop::current()->Quit();
  }

  bool running() const { return running_; }

 private:
  bool running_;
};

bool IsEncryptionComplete(const ProfileSyncService* service) {
  return service->EncryptEverythingEnabled() && !service->encryption_pending();
}

// Helper class to wait for encryption to complete.
class EncryptionChecker : public SingleClientStatusChangeChecker {
 public:
  explicit EncryptionChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    return IsEncryptionComplete(service());
  }

  std::string GetDebugMessage() const override { return "Encryption"; }
};

void SetupNetworkCallback(
    base::WaitableEvent* done,
    net::URLRequestContextGetter* url_request_context_getter) {
  url_request_context_getter->GetURLRequestContext()->
      set_cookie_store(new net::CookieMonster(NULL, NULL));
  done->Signal();
}

KeyedService* BuildFakeServerProfileInvalidationProvider(
    content::BrowserContext* context) {
  return new invalidation::ProfileInvalidationProvider(
      scoped_ptr<invalidation::InvalidationService>(
          new fake_server::FakeServerInvalidationService));
}

KeyedService* BuildP2PProfileInvalidationProvider(
    content::BrowserContext* context,
    syncer::P2PNotificationTarget notification_target) {
  Profile* profile = static_cast<Profile*>(context);
  return new invalidation::ProfileInvalidationProvider(
      scoped_ptr<invalidation::InvalidationService>(
          new invalidation::P2PInvalidationService(
              scoped_ptr<IdentityProvider>(new ProfileIdentityProvider(
                  SigninManagerFactory::GetForProfile(profile),
                  ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
                  LoginUIServiceFactory::GetForProfile(profile))),
              profile->GetRequestContext(),
              notification_target)));
}

KeyedService* BuildSelfNotifyingP2PProfileInvalidationProvider(
    content::BrowserContext* context) {
  return BuildP2PProfileInvalidationProvider(context, syncer::NOTIFY_ALL);
}

KeyedService* BuildRealisticP2PProfileInvalidationProvider(
    content::BrowserContext* context) {
  return BuildP2PProfileInvalidationProvider(context, syncer::NOTIFY_OTHERS);
}

}  // namespace

SyncTest::SyncTest(TestType test_type)
    : test_type_(test_type),
      server_type_(SERVER_TYPE_UNDECIDED),
      num_clients_(-1),
      use_verifier_(true),
      notifications_enabled_(true) {
  sync_datatype_helper::AssociateWithTest(this);
  switch (test_type_) {
    case SINGLE_CLIENT:
    case SINGLE_CLIENT_LEGACY: {
      num_clients_ = 1;
      break;
    }
    case TWO_CLIENT:
    case TWO_CLIENT_LEGACY: {
      num_clients_ = 2;
      break;
    }
    case MULTIPLE_CLIENT: {
      num_clients_ = 3;
      break;
    }
  }
}

SyncTest::~SyncTest() {}

void SyncTest::SetUp() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kPasswordFileForTest)) {
    ReadPasswordFile();
  } else if (cl->HasSwitch(switches::kSyncUserForTest) &&
             cl->HasSwitch(switches::kSyncPasswordForTest)) {
    username_ = cl->GetSwitchValueASCII(switches::kSyncUserForTest);
    password_ = cl->GetSwitchValueASCII(switches::kSyncPasswordForTest);
  } else {
    username_ = "user@gmail.com";
    password_ = "password";
  }

  if (username_.empty() || password_.empty())
    LOG(FATAL) << "Cannot run sync tests without GAIA credentials.";

  // Sets |server_type_| if it wasn't specified by the test.
  DecideServerType();

  // Mock the Mac Keychain service.  The real Keychain can block on user input.
#if defined(OS_MACOSX)
  OSCrypt::UseMockKeychain(true);
#endif

  // Start up a sync test server if one is needed and setup mock gaia responses.
  // Note: This must be done prior to the call to SetupClients() because we want
  // the mock gaia responses to be available before GaiaUrls is initialized.
  SetUpTestServerIfRequired();

  // Yield control back to the InProcessBrowserTest framework.
  InProcessBrowserTest::SetUp();
}

void SyncTest::TearDown() {
  // Clear any mock gaia responses that might have been set.
  ClearMockGaiaResponses();

  // Allow the InProcessBrowserTest framework to perform its tear down.
  InProcessBrowserTest::TearDown();

  // Stop the local python test server. This is a no-op if one wasn't started.
  TearDownLocalPythonTestServer();

  // Stop the local sync test server. This is a no-op if one wasn't started.
  TearDownLocalTestServer();

  fake_server_.reset();
}

void SyncTest::SetUpCommandLine(base::CommandLine* cl) {
  AddTestSwitches(cl);
  AddOptionalTypesToCommandLine(cl);

#if defined(OS_CHROMEOS)
  cl->AppendSwitch(chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
}

void SyncTest::AddTestSwitches(base::CommandLine* cl) {
  // Disable non-essential access of external network resources.
  if (!cl->HasSwitch(switches::kDisableBackgroundNetworking))
    cl->AppendSwitch(switches::kDisableBackgroundNetworking);

  if (!cl->HasSwitch(switches::kSyncShortInitialRetryOverride))
    cl->AppendSwitch(switches::kSyncShortInitialRetryOverride);
}

void SyncTest::AddOptionalTypesToCommandLine(base::CommandLine* cl) {}

// Called when the ProfileManager has created a profile.
// static
void SyncTest::CreateProfileCallback(const base::Closure& quit_closure,
                                     Profile* profile,
                                     Profile::CreateStatus status) {
  EXPECT_TRUE(profile);
  EXPECT_NE(Profile::CREATE_STATUS_LOCAL_FAIL, status);
  EXPECT_NE(Profile::CREATE_STATUS_REMOTE_FAIL, status);
  // This will be called multiple times. Wait until the profile is initialized
  // fully to quit the loop.
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    quit_closure.Run();
}

// TODO(shadi): Ideally creating a new profile should not depend on signin
// process. We should try to consolidate MakeProfileForUISignin() and
// MakeProfile(). Major differences are profile paths and creation methods. For
// UI signin we need profiles in unique user data dir's and we need to use
// ProfileManager::CreateProfileAsync() for proper profile creation.
// static
Profile* SyncTest::MakeProfileForUISignin(
    const base::FilePath::StringType name) {
  // For multi profile UI signin, profile paths should be outside user data dir.
  // Otherwise, we get an error that the profile has already signed in on this
  // device.
  // Note that prefix |name| is implemented only on Win. On other platforms the
  // com.google.Chrome.XXXXXX prefix is used.
  base::FilePath profile_path;
  CHECK(base::CreateNewTempDirectory(name, &profile_path));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::RunLoop run_loop;
  ProfileManager::CreateCallback create_callback = base::Bind(
      &CreateProfileCallback, run_loop.QuitClosure());
  profile_manager->CreateProfileAsync(profile_path,
                                      create_callback,
                                      base::string16(),
                                      base::string16(),
                                      std::string());
  run_loop.Run();
  return profile_manager->GetProfileByPath(profile_path);
}

Profile* SyncTest::MakeProfile(const base::FilePath::StringType name) {
  base::FilePath path;
  // Create new profiles in user data dir so that other profiles can know about
  // it. This is needed in tests such as supervised user cases which assume
  // browser->profile() as the custodian profile.
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.Append(name);

  if (!base::PathExists(path))
    CHECK(base::CreateDirectory(path));

  if (!preexisting_preferences_file_contents_.empty()) {
    base::FilePath pref_path(path.Append(chrome::kPreferencesFilename));
    const char* contents = preexisting_preferences_file_contents_.c_str();
    size_t contents_length = preexisting_preferences_file_contents_.size();
    if (!base::WriteFile(pref_path, contents, contents_length)) {
      LOG(FATAL) << "Preexisting Preferences file could not be written.";
    }
  }

  Profile* profile =
      Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
  g_browser_process->profile_manager()->RegisterTestingProfile(profile,
                                                               true,
                                                               true);
  return profile;
}

Profile* SyncTest::GetProfile(int index) {
  if (profiles_.empty())
    LOG(FATAL) << "SetupClients() has not yet been called.";
  if (index < 0 || index >= static_cast<int>(profiles_.size()))
    LOG(FATAL) << "GetProfile(): Index is out of bounds.";
  return profiles_[index];
}

Browser* SyncTest::GetBrowser(int index) {
  if (browsers_.empty())
    LOG(FATAL) << "SetupClients() has not yet been called.";
  if (index < 0 || index >= static_cast<int>(browsers_.size()))
    LOG(FATAL) << "GetBrowser(): Index is out of bounds.";
  return browsers_[index];
}

ProfileSyncServiceHarness* SyncTest::GetClient(int index) {
  if (clients_.empty())
    LOG(FATAL) << "SetupClients() has not yet been called.";
  if (index < 0 || index >= static_cast<int>(clients_.size()))
    LOG(FATAL) << "GetClient(): Index is out of bounds.";
  return clients_[index];
}

ProfileSyncService* SyncTest::GetSyncService(int index) {
  return ProfileSyncServiceFactory::GetForProfile(GetProfile(index));
}

std::vector<ProfileSyncService*> SyncTest::GetSyncServices() {
  std::vector<ProfileSyncService*> services;
  for (int i = 0; i < num_clients(); ++i) {
    services.push_back(GetSyncService(i));
  }
  return services;
}

Profile* SyncTest::verifier() {
  if (verifier_ == NULL)
    LOG(FATAL) << "SetupClients() has not yet been called.";
  return verifier_;
}

void SyncTest::DisableVerifier() {
  use_verifier_ = false;
}

bool SyncTest::SetupClients() {
  if (num_clients_ <= 0)
    LOG(FATAL) << "num_clients_ incorrectly initialized.";
  if (!profiles_.empty() || !browsers_.empty() || !clients_.empty())
    LOG(FATAL) << "SetupClients() has already been called.";

  // Create the required number of sync profiles, browsers and clients.
  profiles_.resize(num_clients_);
  browsers_.resize(num_clients_);
  clients_.resize(num_clients_);
  invalidation_forwarders_.resize(num_clients_);
  sync_refreshers_.resize(num_clients_);
  fake_server_invalidation_services_.resize(num_clients_);
  for (int i = 0; i < num_clients_; ++i) {
    InitializeInstance(i);
  }

  // Create the verifier profile.
  verifier_ = MakeProfile(FILE_PATH_LITERAL("Verifier"));
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForProfile(verifier()));
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      verifier(), ServiceAccessType::EXPLICIT_ACCESS));
  ui_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(verifier()));
  return (verifier_ != NULL);
}

void SyncTest::InitializeInstance(int index) {
  base::FilePath::StringType profile_name =
      base::StringPrintf(FILE_PATH_LITERAL("Profile%d"), index);
  // If running against an EXTERNAL_LIVE_SERVER, we need to signin profiles
  // using real GAIA server. This requires creating profiles with no test hooks.
  if (server_type_ == EXTERNAL_LIVE_SERVER) {
    profiles_[index] = MakeProfileForUISignin(profile_name);
  } else {
    // Without need of real GAIA authentication, we create new test profiles.
    profiles_[index] = MakeProfile(profile_name);
  }

  EXPECT_FALSE(GetProfile(index) == NULL) << "Could not create Profile "
                                          << index << ".";

  // CheckInitialState() assumes that no windows are open at startup.
  browsers_[index] = new Browser(Browser::CreateParams(
      GetProfile(index), chrome::GetActiveDesktop()));

  EXPECT_FALSE(GetBrowser(index) == NULL) << "Could not create Browser "
                                          << index << ".";

  // Make sure the ProfileSyncService has been created before creating the
  // ProfileSyncServiceHarness - some tests expect the ProfileSyncService to
  // already exist.
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile(index));

  SetupNetwork(GetProfile(index)->GetRequestContext());

  if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    // TODO(pvalenzuela): Run the fake server via EmbeddedTestServer.
    profile_sync_service->OverrideNetworkResourcesForTest(
        make_scoped_ptr<syncer::NetworkResources>(
            new fake_server::FakeServerNetworkResources(fake_server_.get())));
  }

  ProfileSyncServiceHarness::SigninType singin_type =
      (server_type_ == EXTERNAL_LIVE_SERVER)
          ? ProfileSyncServiceHarness::SigninType::UI_SIGNIN
          : ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN;

  clients_[index] =
      ProfileSyncServiceHarness::Create(GetProfile(index),
                                        username_,
                                        password_,
                                        singin_type);
  EXPECT_FALSE(GetClient(index) == NULL) << "Could not create Client "
                                         << index << ".";
  InitializeInvalidations(index);

  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForProfile(GetProfile(index)));
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      GetProfile(index), ServiceAccessType::EXPLICIT_ACCESS));
  ui_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(GetProfile(index)));
}

void SyncTest::InitializeInvalidations(int index) {
  if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    CHECK(fake_server_.get());
    fake_server::FakeServerInvalidationService* invalidation_service =
        static_cast<fake_server::FakeServerInvalidationService*>(
            static_cast<invalidation::ProfileInvalidationProvider*>(
                invalidation::ProfileInvalidationProviderFactory::
                    GetInstance()->SetTestingFactoryAndUse(
                        GetProfile(index),
                        BuildFakeServerProfileInvalidationProvider))->
                            GetInvalidationService());
    fake_server_->AddObserver(invalidation_service);
    if (TestUsesSelfNotifications()) {
      invalidation_service->EnableSelfNotifications();
    } else {
      invalidation_service->DisableSelfNotifications();
    }
    fake_server_invalidation_services_[index] = invalidation_service;
  } else if (server_type_ == EXTERNAL_LIVE_SERVER) {
    // DO NOTHING. External live sync servers use GCM to notify profiles of any
    // invalidations in sync'ed data. In this case, to notify other profiles of
    // invalidations, we use sync refresh notifications instead.
  } else {
    invalidation::P2PInvalidationService* p2p_invalidation_service =
        static_cast<invalidation::P2PInvalidationService*>(
            static_cast<invalidation::ProfileInvalidationProvider*>(
                invalidation::ProfileInvalidationProviderFactory::
                    GetInstance()->SetTestingFactoryAndUse(
                        GetProfile(index),
                        TestUsesSelfNotifications() ?
                            BuildSelfNotifyingP2PProfileInvalidationProvider :
                            BuildRealisticP2PProfileInvalidationProvider))->
                                GetInvalidationService());
    p2p_invalidation_service->UpdateCredentials(username_, password_);
    // Start listening for and emitting notifications of commits.
    invalidation_forwarders_[index] =
        new P2PInvalidationForwarder(clients_[index]->service(),
                                     p2p_invalidation_service);
  }
}

bool SyncTest::SetupSync() {
  // Create sync profiles and clients if they haven't already been created.
  if (profiles_.empty()) {
    if (!SetupClients())
      LOG(FATAL) << "SetupClients() failed.";
  }

  // Sync each of the profiles.
  for (int i = 0; i < num_clients_; ++i) {
    if (!GetClient(i)->SetupSync())
      LOG(FATAL) << "SetupSync() failed.";
  }

  // Because clients may modify sync data as part of startup (for example local
  // session-releated data is rewritten), we need to ensure all startup-based
  // changes have propagated between the clients.
  //
  // Tests that don't use self-notifications can't await quiescense.  They'll
  // have to find their own way of waiting for an initial state if they really
  // need such guarantees.
  if (TestUsesSelfNotifications()) {
    AwaitQuiescence();
  }

  // SyncRefresher is used instead of invalidations to notify other profiles to
  // do a sync refresh on committed data sets. This is only needed when running
  // tests against external live server, otherwise invalidation service is used.
  // With external live servers, the profiles commit data on first sync cycle
  // automatically after signing in. To avoid misleading sync commit
  // notifications at start up, we start the SyncRefresher observers post
  // client set up.
  if (server_type_ == EXTERNAL_LIVE_SERVER) {
    for (int i = 0; i < num_clients_; ++i) {
      sync_refreshers_[i] = new P2PSyncRefresher(clients_[i]->service());
    }
  }

  return true;
}

void SyncTest::TearDownOnMainThread() {
  for (size_t i = 0; i < clients_.size(); ++i) {
    clients_[i]->service()->DisableForUser();
  }

  // Some of the pending messages might rely on browser windows still being
  // around, so run messages both before and after closing all browsers.
  content::RunAllPendingInMessageLoop();
  // Close all browser windows.
  chrome::CloseAllBrowsers();
  content::RunAllPendingInMessageLoop();

  if (fake_server_.get()) {
    std::vector<fake_server::FakeServerInvalidationService*>::const_iterator it;
    for (it = fake_server_invalidation_services_.begin();
         it != fake_server_invalidation_services_.end(); ++it) {
      fake_server_->RemoveObserver(*it);
    }
  }

  // All browsers should be closed at this point, or else we could see memory
  // corruption in QuitBrowser().
  CHECK_EQ(0U, chrome::GetTotalBrowserCount());
  invalidation_forwarders_.clear();
  sync_refreshers_.clear();
  fake_server_invalidation_services_.clear();
  clients_.clear();
}

void SyncTest::SetUpInProcessBrowserTestFixture() {
  // We don't take a reference to |resolver|, but mock_host_resolver_override_
  // does, so effectively assumes ownership.
  net::RuleBasedHostResolverProc* resolver =
      new net::RuleBasedHostResolverProc(host_resolver());
  resolver->AllowDirectLookup("*.google.com");
  // On Linux, we use Chromium's NSS implementation which uses the following
  // hosts for certificate verification. Without these overrides, running the
  // integration tests on Linux causes error as we make external DNS lookups.
  resolver->AllowDirectLookup("*.thawte.com");
  resolver->AllowDirectLookup("*.geotrust.com");
  resolver->AllowDirectLookup("*.gstatic.com");
  mock_host_resolver_override_.reset(
      new net::ScopedDefaultHostResolverProc(resolver));
}

void SyncTest::TearDownInProcessBrowserTestFixture() {
  mock_host_resolver_override_.reset();
}

void SyncTest::ReadPasswordFile() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  password_file_ = cl->GetSwitchValuePath(switches::kPasswordFileForTest);
  if (password_file_.empty())
    LOG(FATAL) << "Can't run live server test without specifying --"
               << switches::kPasswordFileForTest << "=<filename>";
  std::string file_contents;
  base::ReadFileToString(password_file_, &file_contents);
  ASSERT_NE(file_contents, "") << "Password file \""
      << password_file_.value() << "\" does not exist.";
  std::vector<std::string> tokens;
  std::string delimiters = "\r\n";
  Tokenize(file_contents, delimiters, &tokens);
  ASSERT_EQ(2U, tokens.size()) << "Password file \""
      << password_file_.value()
      << "\" must contain exactly two lines of text.";
  username_ = tokens[0];
  password_ = tokens[1];
}

void SyncTest::SetupMockGaiaResponses() {
  factory_.reset(new net::URLFetcherImplFactory());
  fake_factory_.reset(new net::FakeURLFetcherFactory(factory_.get()));
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->get_user_info_url(),
      "email=user@gmail.com\ndisplayEmail=user@gmail.com",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->issue_auth_token_url(),
      "auth",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
  fake_factory_->SetFakeResponse(
      GURL(GoogleURLTracker::kSearchDomainCheckURL),
      ".google.com",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->client_login_to_oauth2_url(),
      "some_response",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->oauth2_token_url(),
      "{"
      "  \"refresh_token\": \"rt1\","
      "  \"access_token\": \"at1\","
      "  \"expires_in\": 3600,"
      "  \"token_type\": \"Bearer\""
      "}",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->oauth_user_info_url(),
      "{"
      "  \"id\": \"12345\""
      "}",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->oauth1_login_url(),
      "SID=sid\nLSID=lsid\nAuth=auth_token",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
  fake_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->oauth2_revoke_url(),
      "",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
}

void SyncTest::SetOAuth2TokenResponse(const std::string& response_data,
                                      net::HttpStatusCode response_code,
                                      net::URLRequestStatus::Status status) {
  ASSERT_TRUE(NULL != fake_factory_.get());
  fake_factory_->SetFakeResponse(GaiaUrls::GetInstance()->oauth2_token_url(),
                                 response_data, response_code, status);
}

void SyncTest::ClearMockGaiaResponses() {
  // Clear any mock gaia responses that might have been set.
  if (fake_factory_) {
    fake_factory_->ClearFakeResponses();
    fake_factory_.reset();
  }

  // Cancel any outstanding URL fetches and destroy the URLFetcherImplFactory we
  // created.
  net::URLFetcher::CancelAll();
  factory_.reset();
}

void SyncTest::DecideServerType() {
  // Only set |server_type_| if it hasn't already been set. This allows for
  // tests to explicitly set this value in each test class if needed.
  if (server_type_ == SERVER_TYPE_UNDECIDED) {
    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    if (!cl->HasSwitch(switches::kSyncServiceURL) &&
        !cl->HasSwitch(switches::kSyncServerCommandLine)) {
      // If neither a sync server URL nor a sync server command line is
      // provided, start up a local sync test server and point Chrome
      // to its URL.  This is the most common configuration, and the only
      // one that makes sense for most developers. FakeServer is the
      // current solution but some scenarios are only supported by the
      // legacy python server.
      switch (test_type_) {
        case SINGLE_CLIENT:
        case TWO_CLIENT:
        case MULTIPLE_CLIENT:
          server_type_ = IN_PROCESS_FAKE_SERVER;
          break;
        default:
          server_type_ = LOCAL_PYTHON_SERVER;
      }
    } else if (cl->HasSwitch(switches::kSyncServiceURL) &&
               cl->HasSwitch(switches::kSyncServerCommandLine)) {
      // If a sync server URL and a sync server command line are provided,
      // start up a local sync server by running the command line. Chrome
      // will connect to the server at the URL that was provided.
      server_type_ = LOCAL_LIVE_SERVER;
    } else if (cl->HasSwitch(switches::kSyncServiceURL) &&
               !cl->HasSwitch(switches::kSyncServerCommandLine)) {
      // If a sync server URL is provided, but not a server command line,
      // it is assumed that the server is already running. Chrome will
      // automatically connect to it at the URL provided. There is nothing
      // to do here.
      server_type_ = EXTERNAL_LIVE_SERVER;
    } else {
      // If a sync server command line is provided, but not a server URL,
      // we flag an error.
      LOG(FATAL) << "Can't figure out how to run a server.";
    }
  }
}

// Start up a local sync server based on the value of server_type_, which
// was determined from the command line parameters.
void SyncTest::SetUpTestServerIfRequired() {
  if (server_type_ == LOCAL_PYTHON_SERVER) {
    if (!SetUpLocalPythonTestServer())
      LOG(FATAL) << "Failed to set up local python sync and XMPP servers";
    SetupMockGaiaResponses();
  } else if (server_type_ == LOCAL_LIVE_SERVER) {
    // Using mock gaia credentials requires the use of a mock XMPP server.
    if (username_ == "user@gmail.com" && !SetUpLocalPythonTestServer())
      LOG(FATAL) << "Failed to set up local python XMPP server";
    if (!SetUpLocalTestServer())
      LOG(FATAL) << "Failed to set up local test server";
  } else if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    fake_server_.reset(new fake_server::FakeServer());
    SetupMockGaiaResponses();
  } else if (server_type_ == EXTERNAL_LIVE_SERVER) {
    // Nothing to do; we'll just talk to the URL we were given.
  } else {
    LOG(FATAL) << "Don't know which server environment to run test in.";
  }
}

bool SyncTest::SetUpLocalPythonTestServer() {
  EXPECT_TRUE(sync_server_.Start())
      << "Could not launch local python test server.";

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (server_type_ == LOCAL_PYTHON_SERVER) {
    std::string sync_service_url = sync_server_.GetURL("chromiumsync").spec();
    cl->AppendSwitchASCII(switches::kSyncServiceURL, sync_service_url);
    DVLOG(1) << "Started local python sync server at " << sync_service_url;
  }

  int xmpp_port = 0;
  if (!sync_server_.server_data().GetInteger("xmpp_port", &xmpp_port)) {
    LOG(ERROR) << "Could not find valid xmpp_port value";
    return false;
  }
  if ((xmpp_port <= 0) || (xmpp_port > kuint16max)) {
    LOG(ERROR) << "Invalid xmpp port: " << xmpp_port;
    return false;
  }

  net::HostPortPair xmpp_host_port_pair(sync_server_.host_port_pair());
  xmpp_host_port_pair.set_port(xmpp_port);
  xmpp_port_.reset(new net::ScopedPortException(xmpp_port));

  if (!cl->HasSwitch(invalidation::switches::kSyncNotificationHostPort)) {
    cl->AppendSwitchASCII(invalidation::switches::kSyncNotificationHostPort,
                          xmpp_host_port_pair.ToString());
    // The local XMPP server only supports insecure connections.
    cl->AppendSwitch(invalidation::switches::kSyncAllowInsecureXmppConnection);
  }
  DVLOG(1) << "Started local python XMPP server at "
           << xmpp_host_port_pair.ToString();

  return true;
}

bool SyncTest::SetUpLocalTestServer() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType server_cmdline_string =
      cl->GetSwitchValueNative(switches::kSyncServerCommandLine);
  base::CommandLine::StringVector server_cmdline_vector;
  base::CommandLine::StringType delimiters(FILE_PATH_LITERAL(" "));
  Tokenize(server_cmdline_string, delimiters, &server_cmdline_vector);
  base::CommandLine server_cmdline(server_cmdline_vector);
  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  test_server_ = base::LaunchProcess(server_cmdline, options);
  if (!test_server_.IsValid())
    LOG(ERROR) << "Could not launch local test server.";

  const base::TimeDelta kMaxWaitTime = TestTimeouts::action_max_timeout();
  const int kNumIntervals = 15;
  if (WaitForTestServerToStart(kMaxWaitTime, kNumIntervals)) {
    DVLOG(1) << "Started local test server at "
             << cl->GetSwitchValueASCII(switches::kSyncServiceURL) << ".";
    return true;
  } else {
    LOG(ERROR) << "Could not start local test server at "
               << cl->GetSwitchValueASCII(switches::kSyncServiceURL) << ".";
    return false;
  }
}

bool SyncTest::TearDownLocalPythonTestServer() {
  if (!sync_server_.Stop()) {
    LOG(ERROR) << "Could not stop local python test server.";
    return false;
  }
  xmpp_port_.reset();
  return true;
}

bool SyncTest::TearDownLocalTestServer() {
  if (test_server_.IsValid()) {
    EXPECT_TRUE(test_server_.Terminate(0, false))
        << "Could not stop local test server.";
    test_server_.Close();
  }
  return true;
}

bool SyncTest::WaitForTestServerToStart(base::TimeDelta wait, int intervals) {
  for (int i = 0; i < intervals; ++i) {
    if (IsTestServerRunning())
      return true;
    base::PlatformThread::Sleep(wait / intervals);
  }
  return false;
}

bool SyncTest::IsTestServerRunning() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  std::string sync_url = cl->GetSwitchValueASCII(switches::kSyncServiceURL);
  GURL sync_url_status(sync_url.append("/healthz"));
  SyncServerStatusChecker delegate;
  scoped_ptr<net::URLFetcher> fetcher(net::URLFetcher::Create(
    sync_url_status, net::URLFetcher::GET, &delegate));
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                        net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher->SetRequestContext(g_browser_process->system_request_context());
  fetcher->Start();
  content::RunMessageLoop();
  return delegate.running();
}

bool SyncTest::TestUsesSelfNotifications() {
  return true;
}

bool SyncTest::EnableEncryption(int index) {
  ProfileSyncService* service = GetClient(index)->service();

  if (::IsEncryptionComplete(service))
    return true;

  service->EnableEncryptEverything();

  // In order to kick off the encryption we have to reconfigure. Just grab the
  // currently synced types and use them.
  const syncer::ModelTypeSet synced_datatypes =
      service->GetPreferredDataTypes();
  bool sync_everything = synced_datatypes.Equals(syncer::ModelTypeSet::All());
  service->OnUserChoseDatatypes(sync_everything, synced_datatypes);

  return AwaitEncryptionComplete(index);
}

bool SyncTest::IsEncryptionComplete(int index) {
  return ::IsEncryptionComplete(GetClient(index)->service());
}

bool SyncTest::AwaitEncryptionComplete(int index) {
  ProfileSyncService* service = GetClient(index)->service();
  EncryptionChecker checker(service);
  checker.Wait();
  return !checker.TimedOut();
}

bool SyncTest::AwaitQuiescence() {
  return ProfileSyncServiceHarness::AwaitQuiescence(clients());
}

bool SyncTest::ServerSupportsNotificationControl() const {
  EXPECT_NE(SERVER_TYPE_UNDECIDED, server_type_);

  // Supported only if we're using the python testserver.
  return server_type_ == LOCAL_PYTHON_SERVER;
}

void SyncTest::DisableNotificationsImpl() {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  std::string path = "chromiumsync/disablenotifications";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notifications disabled",
            base::UTF16ToASCII(
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetTitle()));
}

void SyncTest::DisableNotifications() {
  DisableNotificationsImpl();
  notifications_enabled_ = false;
}

void SyncTest::EnableNotificationsImpl() {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  std::string path = "chromiumsync/enablenotifications";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notifications enabled",
            base::UTF16ToASCII(
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetTitle()));
}

void SyncTest::EnableNotifications() {
  EnableNotificationsImpl();
  notifications_enabled_ = true;
}

void SyncTest::TriggerNotification(syncer::ModelTypeSet changed_types) {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  const std::string& data =
      syncer::P2PNotificationData(
          "from_server",
          syncer::NOTIFY_ALL,
          syncer::ObjectIdInvalidationMap::InvalidateAll(
              syncer::ModelTypeSetToObjectIdSet(changed_types))).ToString();
  const std::string& path =
      std::string("chromiumsync/sendnotification?channel=") +
      syncer::kSyncP2PNotificationChannel + "&data=" + data;
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notification sent",
            base::UTF16ToASCII(
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetTitle()));
}

bool SyncTest::ServerSupportsErrorTriggering() const {
  EXPECT_NE(SERVER_TYPE_UNDECIDED, server_type_);

  // Supported only if we're using the python testserver.
  return server_type_ == LOCAL_PYTHON_SERVER;
}

void SyncTest::TriggerMigrationDoneError(syncer::ModelTypeSet model_types) {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/migrate";
  char joiner = '?';
  for (syncer::ModelTypeSet::Iterator it = model_types.First();
       it.Good(); it.Inc()) {
    path.append(
        base::StringPrintf(
            "%ctype=%d", joiner,
            syncer::GetSpecificsFieldNumberFromModelType(it.Get())));
    joiner = '&';
  }
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Migration: 200",
            base::UTF16ToASCII(
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetTitle()));
}

void SyncTest::TriggerXmppAuthError() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/xmppcred";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
}

void SyncTest::TriggerCreateSyncedBookmarks() {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/createsyncedbookmarks";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Synced Bookmarks",
            base::UTF16ToASCII(
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetTitle()));
}

void SyncTest::SetupNetwork(net::URLRequestContextGetter* context_getter) {
  base::WaitableEvent done(false, false);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SetupNetworkCallback, &done,
                 make_scoped_refptr(context_getter)));
  done.Wait();
}

fake_server::FakeServer* SyncTest::GetFakeServer() const {
  return fake_server_.get();
}

void SyncTest::SetPreexistingPreferencesFileContents(
    const std::string& contents) {
  preexisting_preferences_file_contents_ = contents;
}
