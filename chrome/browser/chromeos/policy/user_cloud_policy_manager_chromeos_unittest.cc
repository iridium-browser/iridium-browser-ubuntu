// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::_;

namespace policy {

const char kOAuthCodeCookie[] = "oauth_code=1234; Secure; HttpOnly";

const char kOAuth2TokenPairData[] =
    "{"
    "  \"refresh_token\": \"1234\","
    "  \"access_token\": \"5678\","
    "  \"expires_in\": 3600"
    "}";

const char kOAuth2AccessTokenData[] =
    "{"
    "  \"access_token\": \"5678\","
    "  \"expires_in\": 3600"
    "}";

class UserCloudPolicyManagerChromeOSTest : public testing::Test {
 protected:
  UserCloudPolicyManagerChromeOSTest()
      : store_(NULL),
        external_data_manager_(NULL),
        task_runner_(new base::TestSimpleTaskRunner()),
        profile_(NULL),
        signin_profile_(NULL),
        user_manager_(new user_manager::FakeUserManager()),
        user_manager_enabler_(user_manager_) {}

  void SetUp() override {
    // The initialization path that blocks on the initial policy fetch requires
    // a signin Profile to use its URLRequestContext.
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingProfile::TestingFactories factories;
    factories.push_back(
        std::make_pair(ProfileOAuth2TokenServiceFactory::GetInstance(),
                       BuildFakeProfileOAuth2TokenService));
    profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile,
        std::unique_ptr<syncable_prefs::PrefServiceSyncable>(),
        base::UTF8ToUTF16(""), 0, std::string(), factories);
    // Usually the signin Profile and the main Profile are separate, but since
    // the signin Profile is an OTR Profile then for this test it suffices to
    // attach it to the main Profile.
    signin_profile_ = TestingProfile::Builder().BuildIncognito(profile_);
    ASSERT_EQ(signin_profile_, chromeos::ProfileHelper::GetSigninProfile());

    chrome::RegisterLocalState(prefs_.registry());

    // Set up a policy map for testing.
    policy_map_.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    base::MakeUnique<base::StringValue>("http://chromium.org"),
                    nullptr);
    policy_map_.Set(
        key::kChromeOsMultiProfileUserBehavior, POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_USER, POLICY_SOURCE_ENTERPRISE_DEFAULT,
        base::MakeUnique<base::StringValue>("primary-only"), nullptr);
    policy_map_.Set(key::kEasyUnlockAllowed, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    base::MakeUnique<base::FundamentalValue>(false), nullptr);
    policy_map_.Set(key::kCaptivePortalAuthenticationIgnoresProxy,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    POLICY_SOURCE_CLOUD,
                    base::MakeUnique<base::FundamentalValue>(false), nullptr);
    policy_map_.Set(key::kAllowDinosaurEasterEgg, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                    base::MakeUnique<base::FundamentalValue>(false), nullptr);
    expected_bundle_.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .CopyFrom(policy_map_);

    // Create fake policy blobs to deliver to the client.
    em::DeviceRegisterResponse* register_response =
        register_blob_.mutable_register_response();
    register_response->set_device_management_token("dmtoken123");

    em::CloudPolicySettings policy_proto;
    policy_proto.mutable_homepagelocation()->set_value("http://chromium.org");
    ASSERT_TRUE(
        policy_proto.SerializeToString(policy_data_.mutable_policy_value()));
    policy_data_.set_policy_type(dm_protocol::kChromeUserPolicyType);
    policy_data_.set_request_token("dmtoken123");
    policy_data_.set_device_id("id987");
    em::PolicyFetchResponse* policy_response =
        policy_blob_.mutable_policy_response()->add_response();
    ASSERT_TRUE(policy_data_.SerializeToString(
        policy_response->mutable_policy_data()));

    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _))
        .Times(AnyNumber());
  }

  void TearDown() override {
    if (token_forwarder_)
      token_forwarder_->Shutdown();
    if (manager_) {
      manager_->RemoveObserver(&observer_);
      manager_->Shutdown();
    }
    signin_profile_ = NULL;
    profile_ = NULL;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  void CreateManager(bool wait_for_fetch, int fetch_timeout) {
    store_ = new MockCloudPolicyStore();
    external_data_manager_ = new MockCloudExternalDataManager;
    external_data_manager_->SetPolicyStore(store_);
    EXPECT_CALL(*store_, Load());
    manager_.reset(new UserCloudPolicyManagerChromeOS(
        std::unique_ptr<CloudPolicyStore>(store_),
        std::unique_ptr<CloudExternalDataManager>(external_data_manager_),
        base::FilePath(), wait_for_fetch,
        base::TimeDelta::FromSeconds(fetch_timeout), task_runner_, task_runner_,
        task_runner_));
    manager_->Init(&schema_registry_);
    manager_->AddObserver(&observer_);
    manager_->Connect(&prefs_, &device_management_service_, NULL);
    Mock::VerifyAndClearExpectations(store_);
    EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());

    if (!wait_for_fetch) {
      // Create the UserCloudPolicyTokenForwarder, which fetches the access
      // token using the OAuth2PolicyFetcher and forwards it to the
      // UserCloudPolicyManagerChromeOS. This service is automatically created
      // for regular Profiles but not for testing Profiles.
      ProfileOAuth2TokenService* token_service =
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
      ASSERT_TRUE(token_service);
      SigninManagerBase* signin_manager =
          SigninManagerFactory::GetForProfile(profile_);
      ASSERT_TRUE(signin_manager);
      token_forwarder_.reset(
          new UserCloudPolicyTokenForwarder(manager_.get(), token_service,
                                            signin_manager));
    }
  }

  // Expects a pending URLFetcher for the |expected_url|, and returns it with
  // prepared to deliver a response to its delegate.
  net::TestURLFetcher* PrepareOAuthFetcher(const GURL& expected_url) {
    net::TestURLFetcher* fetcher = test_url_fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);
    if (!fetcher)
      return NULL;
    EXPECT_TRUE(fetcher->delegate());
    EXPECT_TRUE(base::StartsWith(fetcher->GetOriginalURL().spec(),
                                 expected_url.spec(),
                                 base::CompareCase::SENSITIVE));
    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_response_code(200);
    fetcher->set_status(net::URLRequestStatus());
    return fetcher;
  }

  // Issues the OAuth2 tokens and returns the device management register job
  // if the flow succeeded.
  MockDeviceManagementJob* IssueOAuthToken(bool has_request_token) {
    EXPECT_FALSE(manager_->core()->client()->is_registered());

    // Issuing this token triggers the callback of the OAuth2PolicyFetcher,
    // which triggers the registration request.
    MockDeviceManagementJob* register_request = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION, _))
        .WillOnce(device_management_service_.CreateAsyncJob(&register_request));

    if (!has_request_token) {
      GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
      net::TestURLFetcher* fetcher = NULL;

      // Issue the oauth_token cookie first.
      fetcher = PrepareOAuthFetcher(gaia_urls->client_login_to_oauth2_url());
      if (!fetcher)
        return NULL;

      scoped_refptr<net::HttpResponseHeaders> reponse_headers =
          new net::HttpResponseHeaders("");
      reponse_headers->AddCookie(kOAuthCodeCookie);
      fetcher->set_response_headers(reponse_headers);
      fetcher->delegate()->OnURLFetchComplete(fetcher);

      // Issue the refresh token.
      fetcher = PrepareOAuthFetcher(gaia_urls->oauth2_token_url());
      if (!fetcher)
        return NULL;
      fetcher->SetResponseString(kOAuth2TokenPairData);
      fetcher->delegate()->OnURLFetchComplete(fetcher);

      // Issue the access token.
      fetcher = PrepareOAuthFetcher(gaia_urls->oauth2_token_url());
      if (!fetcher)
        return NULL;
      fetcher->SetResponseString(kOAuth2AccessTokenData);
      fetcher->delegate()->OnURLFetchComplete(fetcher);
    } else {
      // Since the refresh token is available, OAuth2TokenService was used
      // to request the access token and not UserCloudPolicyTokenForwarder.
      // Issue the access token with the former.
      FakeProfileOAuth2TokenService* token_service =
        static_cast<FakeProfileOAuth2TokenService*>(
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile_));
      EXPECT_TRUE(token_service);
      OAuth2TokenService::ScopeSet scopes;
      scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
      scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
      token_service->IssueTokenForScope(
          scopes, "5678",
          base::Time::Now() + base::TimeDelta::FromSeconds(3600));
    }

    EXPECT_TRUE(register_request);
    EXPECT_FALSE(manager_->core()->client()->is_registered());

    Mock::VerifyAndClearExpectations(&device_management_service_);
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _))
        .Times(AnyNumber());

    return register_request;
  }

  // Expects a policy fetch request to be issued after invoking |trigger_fetch|.
  // This method replies to that fetch request and verifies that the manager
  // handled the response.
  void FetchPolicy(const base::Closure& trigger_fetch) {
    MockDeviceManagementJob* policy_request = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
        .WillOnce(device_management_service_.CreateAsyncJob(&policy_request));
    trigger_fetch.Run();
    ASSERT_TRUE(policy_request);
    EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
    EXPECT_TRUE(manager_->core()->client()->is_registered());

    Mock::VerifyAndClearExpectations(&device_management_service_);
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _))
        .Times(AnyNumber());

    // Send the initial policy back. This completes the initialization flow.
    EXPECT_CALL(*store_, Store(_));
    policy_request->SendResponse(DM_STATUS_SUCCESS, policy_blob_);
    Mock::VerifyAndClearExpectations(store_);

    // Notifying that the store is has cached the fetched policy completes the
    // process, and initializes the manager.
    EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
    store_->policy_map_.CopyFrom(policy_map_);
    store_->NotifyStoreLoaded();
    EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    Mock::VerifyAndClearExpectations(&observer_);
    EXPECT_TRUE(manager_->policies().Equals(expected_bundle_));
  }

  // Required by the refresh scheduler that's created by the manager and
  // for the cleanup of URLRequestContextGetter in the |signin_profile_|.
  content::TestBrowserThreadBundle thread_bundle_;

  // Convenience policy objects.
  em::PolicyData policy_data_;
  em::DeviceManagementResponse register_blob_;
  em::DeviceManagementResponse policy_blob_;
  PolicyMap policy_map_;
  PolicyBundle expected_bundle_;

  // Policy infrastructure.
  net::TestURLFetcherFactory test_url_fetcher_factory_;
  TestingPrefServiceSimple prefs_;
  MockConfigurationPolicyObserver observer_;
  MockDeviceManagementService device_management_service_;
  MockCloudPolicyStore* store_;  // Not owned.
  MockCloudExternalDataManager* external_data_manager_;  // Not owned.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  SchemaRegistry schema_registry_;
  std::unique_ptr<UserCloudPolicyManagerChromeOS> manager_;
  std::unique_ptr<UserCloudPolicyTokenForwarder> token_forwarder_;

  // Required by ProfileHelper to get the signin Profile context.
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  TestingProfile* signin_profile_;

  user_manager::FakeUserManager* user_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerChromeOSTest);
};

// Test disabled. See crbug.com/534733.
TEST_F(UserCloudPolicyManagerChromeOSTest, DISABLED_BlockingFirstFetch) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the policy cache is empty.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true, 1000));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_request = IssueOAuthToken(false);
  ASSERT_TRUE(register_request);

  // Reply with a valid registration response. This triggers the initial policy
  // fetch.
  FetchPolicy(base::Bind(&MockDeviceManagementJob::SendResponse,
                         base::Unretained(register_request),
                         DM_STATUS_SUCCESS, register_blob_));
}

// Test disabled. See crbug.com/534733.
TEST_F(UserCloudPolicyManagerChromeOSTest, DISABLED_BlockingRefreshFetch) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when a previously cached policy and DMToken already exist.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true, 1000));

  // Set the initially cached data and initialize the CloudPolicyService.
  // The initial policy fetch is issued using the cached DMToken.
  store_->policy_.reset(new em::PolicyData(policy_data_));
  FetchPolicy(base::Bind(&MockCloudPolicyStore::NotifyStoreLoaded,
                         base::Unretained(store_)));
}

// Test disabled. See crbug.com/534733.
TEST_F(UserCloudPolicyManagerChromeOSTest, DISABLED_BlockingFetchStoreError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the initial store load fails.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true, 1000));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreError();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_request = IssueOAuthToken(false);
  ASSERT_TRUE(register_request);

  // Reply with a valid registration response. This triggers the initial policy
  // fetch.
  FetchPolicy(base::Bind(&MockDeviceManagementJob::SendResponse,
                         base::Unretained(register_request),
                         DM_STATUS_SUCCESS, register_blob_));
}

// Test disabled. See crbug.com/534733.
TEST_F(UserCloudPolicyManagerChromeOSTest, DISABLED_BlockingFetchOAuthError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the OAuth2 token fetch fails.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true, 1000));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will initialize with no policy after the token fetcher fails.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));

  // The PolicyOAuth2TokenFetcher posts delayed retries on some errors. This
  // data will make it fail immediately.
  net::TestURLFetcher* fetcher = PrepareOAuthFetcher(
      GaiaUrls::GetInstance()->client_login_to_oauth2_url());
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(400);
  fetcher->SetResponseString("Error=BadAuthentication");
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
  Mock::VerifyAndClearExpectations(&observer_);
}

// Test disabled. See crbug.com/534733.
TEST_F(UserCloudPolicyManagerChromeOSTest,
       DISABLED_BlockingFetchRegisterError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the device management registration fails.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true, 1000));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreError();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_request = IssueOAuthToken(false);
  ASSERT_TRUE(register_request);

  // Now make it fail.
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  register_request->SendResponse(DM_STATUS_TEMPORARY_UNAVAILABLE,
                                 em::DeviceManagementResponse());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
  Mock::VerifyAndClearExpectations(&observer_);
}

// Test disabled. See crbug.com/534733.
TEST_F(UserCloudPolicyManagerChromeOSTest,
       DISABLED_BlockingFetchPolicyFetchError) {
  // Tests the initialization of a manager whose Profile is waiting for the
  // initial fetch, when the policy fetch request fails.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true, 1000));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // This starts the OAuth2 policy token fetcher using the signin Profile.
  // The manager will then issue the registration request.
  MockDeviceManagementJob* register_request = IssueOAuthToken(false);
  ASSERT_TRUE(register_request);

  // Reply with a valid registration response. This triggers the initial policy
  // fetch.
  MockDeviceManagementJob* policy_request = NULL;
  EXPECT_CALL(device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .WillOnce(device_management_service_.CreateAsyncJob(&policy_request));
  register_request->SendResponse(DM_STATUS_SUCCESS, register_blob_);
  Mock::VerifyAndClearExpectations(&device_management_service_);
  ASSERT_TRUE(policy_request);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_TRUE(manager_->core()->client()->is_registered());

  // Make the policy fetch fail. The observer gets 2 notifications: one from the
  // RefreshPolicies callback, and another from the OnClientError callback.
  // A single notification suffices for this edge case, but this behavior is
  // also correct and makes the implementation simpler.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get())).Times(AtLeast(1));
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  policy_request->SendResponse(DM_STATUS_TEMPORARY_UNAVAILABLE,
                               em::DeviceManagementResponse());
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
}

// Test disabled. See crbug.com/534733.
TEST_F(UserCloudPolicyManagerChromeOSTest, DISABLED_BlockingFetchTimeout) {
  // The blocking fetch should be abandoned after the timeout.
  ASSERT_NO_FATAL_FAILURE(CreateManager(true, 0));

  // Initialize the CloudPolicyService without any stored data.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // Running the message loop should trigger the timeout.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get())).Times(AtLeast(1));
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(PolicyBundle().Equals(manager_->policies()));
}

// Test disabled. See crbug.com/534733.
TEST_F(UserCloudPolicyManagerChromeOSTest, DISABLED_NonBlockingFirstFetch) {
  // Tests the first policy fetch request by a Profile that isn't managed.
  ASSERT_NO_FATAL_FAILURE(CreateManager(false, 1000));

  // Initialize the CloudPolicyService without any stored data. Since the
  // manager is not waiting for the initial fetch, it will become initialized
  // once the store is ready.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_FALSE(manager_->core()->client()->is_registered());

  // The manager is waiting for the refresh token, and hasn't started any
  // fetchers.
  EXPECT_FALSE(test_url_fetcher_factory_.GetFetcherByID(0));

  // Set a fake refresh token at the OAuth2TokenService.
  FakeProfileOAuth2TokenService* token_service =
    static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_));
  ASSERT_TRUE(token_service);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  ASSERT_TRUE(signin_manager);
  const std::string& account_id = signin_manager->GetAuthenticatedAccountId();
  EXPECT_FALSE(token_service->RefreshTokenIsAvailable(account_id));
  token_service->UpdateCredentials(account_id, "refresh_token");
  EXPECT_TRUE(token_service->RefreshTokenIsAvailable(account_id));

  // That should have notified the manager, which now issues the request for the
  // policy oauth token.
  MockDeviceManagementJob* register_request = IssueOAuthToken(true);
  ASSERT_TRUE(register_request);
  register_request->SendResponse(DM_STATUS_SUCCESS, register_blob_);

  // The refresh scheduler takes care of the initial fetch for unmanaged users.
  // Running the task runner issues the initial fetch.
  FetchPolicy(
      base::Bind(&base::TestSimpleTaskRunner::RunUntilIdle, task_runner_));
}

// Test disabled. See crbug.com/534733.
TEST_F(UserCloudPolicyManagerChromeOSTest, DISABLED_NonBlockingRefreshFetch) {
  // Tests a non-blocking initial policy fetch for a Profile that already has
  // a cached DMToken.
  ASSERT_NO_FATAL_FAILURE(CreateManager(false, 1000));

  // Set the initially cached data and initialize the CloudPolicyService.
  // The initial policy fetch is issued using the cached DMToken.
  EXPECT_FALSE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->policy_.reset(new em::PolicyData(policy_data_));
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->core()->service()->IsInitializationComplete());
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_TRUE(manager_->core()->client()->is_registered());

  // The refresh scheduler takes care of the initial fetch for unmanaged users.
  // Running the task runner issues the initial fetch.
  FetchPolicy(
      base::Bind(&base::TestSimpleTaskRunner::RunUntilIdle, task_runner_));
}

}  // namespace policy
