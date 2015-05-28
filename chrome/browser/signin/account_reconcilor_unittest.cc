// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/signin/core/common/signin_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestEmail[] = "user@gmail.com";

class MockAccountReconcilor : public testing::StrictMock<AccountReconcilor> {
 public:
  static KeyedService* Build(content::BrowserContext* context);

  MockAccountReconcilor(ProfileOAuth2TokenService* token_service,
                        SigninManagerBase* signin_manager,
                        SigninClient* client,
                        GaiaCookieManagerService* cookie_manager_service);
  ~MockAccountReconcilor() override {}

  MOCK_METHOD1(PerformMergeAction, void(const std::string& account_id));
  MOCK_METHOD0(PerformLogoutAllAccountsAction, void());
};

// static
KeyedService* MockAccountReconcilor::Build(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  AccountReconcilor* reconcilor = new MockAccountReconcilor(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile),
      GaiaCookieManagerServiceFactory::GetForProfile(profile));
  reconcilor->Initialize(false /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

MockAccountReconcilor::MockAccountReconcilor(
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    SigninClient* client,
    GaiaCookieManagerService* cookie_manager_service)
    : testing::StrictMock<AccountReconcilor>(token_service,
                                             signin_manager,
                                             client,
                                             cookie_manager_service) {}

}  // namespace

class AccountReconcilorTest : public ::testing::TestWithParam<bool> {
 public:
  AccountReconcilorTest();
  void SetUp() override;

  TestingProfile* profile() { return profile_; }
  FakeSigninManagerForTesting* signin_manager() { return signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return token_service_; }
  TestSigninClient* test_signin_client() { return test_signin_client_; }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  void SetFakeResponse(const std::string& url,
                       const std::string& data,
                       net::HttpStatusCode code,
                       net::URLRequestStatus::Status status) {
    url_fetcher_factory_.SetFakeResponse(GURL(url), data, code, status);
  }

  MockAccountReconcilor* GetMockReconcilor();

  void SimulateAddAccountToCookieCompleted(
      GaiaCookieManagerService::Observer* observer,
      const std::string& account_id,
      const GoogleServiceAuthError& error);

  void SimulateCookieContentSettingsChanged(
      content_settings::Observer* observer,
      const ContentSettingsPattern& primary_pattern);

  GURL list_accounts_url() { return list_accounts_url_; }
  GURL get_check_connection_info_url() {
    return get_check_connection_info_url_;
  }

 private:
  content::TestBrowserThreadBundle bundle_;
  TestingProfile* profile_;
  FakeSigninManagerForTesting* signin_manager_;
  FakeProfileOAuth2TokenService* token_service_;
  TestSigninClient* test_signin_client_;
  MockAccountReconcilor* mock_reconcilor_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;
  base::HistogramTester histogram_tester_;
  GURL list_accounts_url_;
  GURL get_check_connection_info_url_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilorTest);
};

AccountReconcilorTest::AccountReconcilorTest()
    : signin_manager_(NULL),
      token_service_(NULL),
      test_signin_client_(NULL),
      mock_reconcilor_(NULL),
      url_fetcher_factory_(NULL) {}

void AccountReconcilorTest::SetUp() {
  // If it's a non-parameterized test, or we have a parameter of true, set flag.
  if (!::testing::UnitTest::GetInstance()->current_test_info()->value_param() ||
      GetParam()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableNewProfileManagement);
  }

  list_accounts_url_ = GaiaUrls::GetInstance()->ListAccountsURLWithSource(
      GaiaConstants::kReconcilorSource);
  get_check_connection_info_url_ =
      GaiaUrls::GetInstance()->GetCheckConnectionInfoURLWithSource(
          GaiaConstants::kChromeSource);

  SetFakeResponse(get_check_connection_info_url().spec(), "[]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  testing_profile_manager_.reset(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  ASSERT_TRUE(testing_profile_manager_.get()->SetUp());

  TestingProfile::TestingFactories factories;
  factories.push_back(std::make_pair(ChromeSigninClientFactory::GetInstance(),
      signin::BuildTestSigninClient));
  factories.push_back(std::make_pair(
      ProfileOAuth2TokenServiceFactory::GetInstance(),
      BuildFakeProfileOAuth2TokenService));
  factories.push_back(std::make_pair(SigninManagerFactory::GetInstance(),
      FakeSigninManagerBase::Build));
  factories.push_back(std::make_pair(AccountReconcilorFactory::GetInstance(),
      MockAccountReconcilor::Build));

  profile_ = testing_profile_manager_.get()->CreateTestingProfile("name",
                              scoped_ptr<PrefServiceSyncable>(),
                              base::UTF8ToUTF16("name"), 0, std::string(),
                              factories);

  signin_manager_ =
      static_cast<FakeSigninManagerForTesting*>(
          SigninManagerFactory::GetForProfile(profile()));

  token_service_ =
      static_cast<FakeProfileOAuth2TokenService*>(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));

  test_signin_client_ =
      static_cast<TestSigninClient*>(
          ChromeSigninClientFactory::GetForProfile(profile()));
}

MockAccountReconcilor* AccountReconcilorTest::GetMockReconcilor() {
  if (!mock_reconcilor_) {
    mock_reconcilor_ =
        static_cast<MockAccountReconcilor*>(
            AccountReconcilorFactory::GetForProfile(profile()));
  }

  return mock_reconcilor_;
}

void AccountReconcilorTest::SimulateAddAccountToCookieCompleted(
    GaiaCookieManagerService::Observer* observer,
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  observer->OnAddAccountToCookieCompleted(account_id, error);
}

void AccountReconcilorTest::SimulateCookieContentSettingsChanged(
    content_settings::Observer* observer,
    const ContentSettingsPattern& primary_pattern) {
  observer->OnContentSettingChanged(
      primary_pattern,
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES,
      std::string());
}

TEST_F(AccountReconcilorTest, Basic) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
}

#if !defined(OS_CHROMEOS)

// This method requires the use of the |TestSigninClient| to be created from the
// |ChromeSigninClientFactory| because it overrides the |GoogleSigninSucceeded|
// method with an empty implementation. On MacOS, the normal implementation
// causes the try_bots to time out.
TEST_F(AccountReconcilorTest, SigninManagerRegistration) {
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());

  signin_manager()->set_password("password");
  signin_manager()->OnExternalSigninCompleted(kTestEmail);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST);
  ASSERT_FALSE(reconcilor->IsRegisteredWithTokenService());
}

// This method requires the use of the |TestSigninClient| to be created from the
// |ChromeSigninClientFactory| because it overrides the |GoogleSigninSucceeded|
// method with an empty implementation. On MacOS, the normal implementation
// causes the try_bots to time out.
TEST_F(AccountReconcilorTest, Reauth) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  signin_manager()->set_password("password");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());

  // Simulate reauth.  The state of the reconcilor should not change.
  signin_manager()->OnExternalSigninCompleted(kTestEmail);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(AccountReconcilorTest, ProfileAlreadyConnected) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);
  ASSERT_TRUE(reconcilor->IsRegisteredWithTokenService());
}

TEST_F(AccountReconcilorTest, GetAccountsFromCookieSuccess) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction(kTestEmail));
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 0]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->AreGaiaAccountsSet());
  const std::vector<std::pair<std::string, bool> >& accounts =
      reconcilor->GetGaiaAccountsForTesting();
  ASSERT_EQ(1u, accounts.size());
  ASSERT_EQ("user@gmail.com", accounts[0].first);
}

TEST_F(AccountReconcilorTest, GetAccountsFromCookieFailure) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");
  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(list_accounts_url().spec(), "",
      net::HTTP_NOT_FOUND, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());
}

TEST_P(AccountReconcilorTest, StartReconcileNoop) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectTotalCount(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun", 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
}

TEST_P(AccountReconcilorTest, StartReconcileCookiesDisabled) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");
  test_signin_client()->set_are_signin_cookies_allowed(false);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileContentSettings) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  test_signin_client()->set_are_signin_cookies_allowed(false);
  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  test_signin_client()->set_are_signin_cookies_allowed(true);
  SimulateCookieContentSettingsChanged(reconcilor,
                                       ContentSettingsPattern::Wildcard());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileContentSettingsGaiaUrl) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GaiaUrls::GetInstance()->gaia_url()));
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileContentSettingsNonGaiaUrl) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SimulateCookieContentSettingsChanged(
      reconcilor,
      ContentSettingsPattern::FromURL(GURL("http://www.example.com")));
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileContentSettingsInvalidPattern) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  scoped_ptr<ContentSettingsPattern::BuilderInterface>
      builder(ContentSettingsPattern::CreateBuilder(false));
  builder->Invalid();

  SimulateCookieContentSettingsChanged(reconcilor, builder->Build());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
}

// This is test is needed until chrome changes to use gaia obfuscated id.
// The signin manager and token service use the gaia "email" property, which
// preserves dots in usernames and preserves case. gaia::ParseListAccountsData()
// however uses gaia "displayEmail" which does not preserve case, and then
// passes the string through gaia::CanonicalizeEmail() which removes dots.  This
// tests makes sure that an email like "Dot.S@hmail.com", as seen by the
// token service, will be considered the same as "dots@gmail.com" as returned
// by gaia::ParseListAccountsData().
TEST_P(AccountReconcilorTest, StartReconcileNoopWithDots) {
  signin_manager()->SetAuthenticatedUsername("Dot.S@gmail.com");
  token_service()->UpdateCredentials("Dot.S@gmail.com", "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"dot.s@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
}

TEST_P(AccountReconcilorTest, StartReconcileNoopMultiple) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");
  token_service()->UpdateCredentials("other@gmail.com", "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1], "
               "[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  reconcilor->StartReconcile();
  ASSERT_FALSE(reconcilor->AreGaiaAccountsSet());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectTotalCount(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun", 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
}

TEST_P(AccountReconcilorTest, StartReconcileAddToCookie) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");
  token_service()->UpdateCredentials("other@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("other@gmail.com"));

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, "other@gmail.com",
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
}

TEST_P(AccountReconcilorTest, StartReconcileRemoveFromCookie) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("user@gmail.com"));

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1], "
               "[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  SimulateAddAccountToCookieCompleted(reconcilor, "user@gmail.com",
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 0, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 1, 1);
}

TEST_P(AccountReconcilorTest, StartReconcileAddToCookieTwice) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");
  token_service()->UpdateCredentials("other@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("other@gmail.com"));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("third@gmail.com"));

  SetFakeResponse(
      list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(
      reconcilor, "other@gmail.com", GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);

  // Do another pass after I've added a third account to the token service

  SetFakeResponse(
      list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1], "
      "[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
  // This will cause the reconcilor to fire.
  token_service()->UpdateCredentials("third@gmail.com", "refresh_token");

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(
      reconcilor, "third@gmail.com", GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.SubsequentRun",
      signin_metrics::ACCOUNTS_SAME,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.SubsequentRun", 1, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.SubsequentRun", 0, 1);
}

TEST_P(AccountReconcilorTest, StartReconcileBadPrimary) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");
  token_service()->UpdateCredentials("other@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformLogoutAllAccountsAction());
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("user@gmail.com"));
  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("other@gmail.com"));

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0, 0, 1], "
               "[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor = GetMockReconcilor();
  reconcilor->StartReconcile();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, "other@gmail.com",
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);
  SimulateAddAccountToCookieCompleted(reconcilor, "user@gmail.com",
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);

  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.DifferentPrimaryAccounts.FirstRun",
      signin_metrics::COOKIE_AND_TOKEN_PRIMARIES_DIFFERENT,
      1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.AddedToCookieJar.FirstRun", 0, 1);
  histogram_tester()->ExpectUniqueSample(
      "Signin.Reconciler.RemovedFromCookieJar.FirstRun", 0, 1);
}

TEST_P(AccountReconcilorTest, StartReconcileOnlyOnce) {
  signin_manager()->SetAuthenticatedUsername(kTestEmail);
  token_service()->UpdateCredentials(kTestEmail, "refresh_token");

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_P(AccountReconcilorTest, StartReconcileWithSessionInfoExpiredDefault) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");
  token_service()->UpdateCredentials("other@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("user@gmail.com"));

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 0],"
               "[\"b\", 0, \"n\", \"other@gmail.com\", \"p\", 0, 0, 0, 0, 1]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();
  SimulateAddAccountToCookieCompleted(reconcilor, "user@gmail.com",
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

TEST_F(AccountReconcilorTest, AddAccountToCookieCompletedWithBogusAccount) {
  signin_manager()->SetAuthenticatedUsername("user@gmail.com");
  token_service()->UpdateCredentials("user@gmail.com", "refresh_token");

  EXPECT_CALL(*GetMockReconcilor(), PerformMergeAction("user@gmail.com"));

  SetFakeResponse(list_accounts_url().spec(),
      "[\"f\", [[\"b\", 0, \"n\", \"user@gmail.com\", \"p\", 0, 0, 0, 0, 0]]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  AccountReconcilor* reconcilor =
      AccountReconcilorFactory::GetForProfile(profile());
  ASSERT_TRUE(reconcilor);

  ASSERT_FALSE(reconcilor->is_reconcile_started_);
  reconcilor->StartReconcile();
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  base::RunLoop().RunUntilIdle();

  // If an unknown account id is sent, it should not upset the state.
  SimulateAddAccountToCookieCompleted(reconcilor, "bogus@gmail.com",
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_TRUE(reconcilor->is_reconcile_started_);

  SimulateAddAccountToCookieCompleted(reconcilor, "user@gmail.com",
                                      GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(reconcilor->is_reconcile_started_);
}

INSTANTIATE_TEST_CASE_P(AccountReconcilorMaybeEnabled,
                        AccountReconcilorTest,
                        testing::Bool());
