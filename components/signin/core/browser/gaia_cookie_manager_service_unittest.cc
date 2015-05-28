// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockObserver : public GaiaCookieManagerService::Observer {
 public:
  explicit MockObserver(GaiaCookieManagerService* helper) : helper_(helper) {
    helper_->AddObserver(this);
  }

  ~MockObserver() { helper_->RemoveObserver(this); }

  MOCK_METHOD2(OnAddAccountToCookieCompleted,
               void(const std::string&, const GoogleServiceAuthError&));
  MOCK_METHOD1(GetCheckConnectionInfoCompleted, void(bool));

 private:
  GaiaCookieManagerService* helper_;

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

// Counts number of InstrumentedGaiaCookieManagerService created.
// We can EXPECT_* to be zero at the end of our unit tests
// to make sure everything is properly deleted.

int total = 0;

class InstrumentedGaiaCookieManagerService : public GaiaCookieManagerService {
 public:
  InstrumentedGaiaCookieManagerService(
      OAuth2TokenService* token_service,
      SigninClient* signin_client)
      : GaiaCookieManagerService(token_service,
                                 GaiaConstants::kChromeSource,
                                 signin_client) {
    total++;
  }

  virtual ~InstrumentedGaiaCookieManagerService() { total--; }

  MOCK_METHOD0(StartFetching, void());
  MOCK_METHOD0(StartFetchingMergeSession, void());
  MOCK_METHOD0(StartLogOutUrlFetch, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(InstrumentedGaiaCookieManagerService);
};

class GaiaCookieManagerServiceTest : public testing::Test {
 public:
  GaiaCookieManagerServiceTest()
      : no_error_(GoogleServiceAuthError::NONE),
        error_(GoogleServiceAuthError::SERVICE_ERROR),
        canceled_(GoogleServiceAuthError::REQUEST_CANCELED) {}

  OAuth2TokenService* token_service() { return &token_service_; }
  SigninClient* signin_client() { return &signin_client_; }

  void SimulateUbertokenFailure(UbertokenConsumer* consumer,
                                const GoogleServiceAuthError& error) {
    consumer->OnUbertokenFailure(error);
  }

  void SimulateMergeSessionSuccess(GaiaAuthConsumer* consumer,
                                   const std::string& data) {
    consumer->OnMergeSessionSuccess(data);
  }

  void SimulateMergeSessionFailure(GaiaAuthConsumer* consumer,
                                   const GoogleServiceAuthError& error) {
    consumer->OnMergeSessionFailure(error);
  }

  void SimulateLogoutSuccess(net::URLFetcherDelegate* consumer) {
    consumer->OnURLFetchComplete(NULL);
  }

  void SimulateGetCheckConnctionInfoSuccess(net::TestURLFetcher* fetcher,
                                            const std::string& data) {
    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(200);
    fetcher->SetResponseString(data);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SimulateGetCheckConnctionInfoResult(net::URLFetcher* fetcher,
                                           const std::string& result) {
    net::TestURLFetcher* test_fetcher =
        static_cast<net::TestURLFetcher*>(fetcher);
    test_fetcher->set_status(net::URLRequestStatus());
    test_fetcher->set_response_code(200);
    test_fetcher->SetResponseString(result);
    test_fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  const GoogleServiceAuthError& no_error() { return no_error_; }
  const GoogleServiceAuthError& error() { return error_; }
  const GoogleServiceAuthError& canceled() { return canceled_; }

  net::TestURLFetcherFactory* factory() { return &factory_; }

 private:
  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory factory_;
  FakeOAuth2TokenService token_service_;
  GoogleServiceAuthError no_error_;
  GoogleServiceAuthError error_;
  GoogleServiceAuthError canceled_;
  TestSigninClient signin_client_;
};

}  // namespace

using ::testing::_;

TEST_F(GaiaCookieManagerServiceTest, Success) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com");
  SimulateMergeSessionSuccess(&helper, "token");
}

TEST_F(GaiaCookieManagerServiceTest, FailedMergeSession) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      error()));

  helper.AddAccountToCookie("acc1@gmail.com");
  SimulateMergeSessionFailure(&helper, error());
  // Persistent error incurs no further retries.
  DCHECK(!helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, MergeSessionRetried) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(helper, StartFetchingMergeSession());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com");
  SimulateMergeSessionFailure(&helper, canceled());
  DCHECK(helper.is_running());
  // Transient error incurs a retry after 1 second.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(1100));
  base::MessageLoop::current()->Run();
  SimulateMergeSessionSuccess(&helper, "token");
  DCHECK(!helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, MergeSessionRetriedTwice) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(helper, StartFetchingMergeSession()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com");
  SimulateMergeSessionFailure(&helper, canceled());
  DCHECK(helper.is_running());
  // Transient error incurs a retry after 1 second.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(1100));
  base::MessageLoop::current()->Run();
  SimulateMergeSessionFailure(&helper, canceled());
  DCHECK(helper.is_running());
  // Next transient error incurs a retry after 3 seconds.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      base::TimeDelta::FromMilliseconds(3100));
  base::MessageLoop::current()->Run();
  SimulateMergeSessionSuccess(&helper, "token");
  DCHECK(!helper.is_running());
}

TEST_F(GaiaCookieManagerServiceTest, FailedUbertoken) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      error()));

  helper.AddAccountToCookie("acc1@gmail.com");
  SimulateUbertokenFailure(&helper, error());
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterSuccess) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com");
  helper.AddAccountToCookie("acc2@gmail.com");
  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterFailure1) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      error()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com");
  helper.AddAccountToCookie("acc2@gmail.com");
  SimulateMergeSessionFailure(&helper, error());
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, ContinueAfterFailure2) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      error()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      no_error()));

  helper.AddAccountToCookie("acc1@gmail.com");
  helper.AddAccountToCookie("acc2@gmail.com");
  SimulateUbertokenFailure(&helper, error());
  SimulateMergeSessionSuccess(&helper, "token2");
}

TEST_F(GaiaCookieManagerServiceTest, AllRequestsInMultipleGoes) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  EXPECT_CALL(helper, StartFetching()).Times(4);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted(_, no_error())).Times(4);

  helper.AddAccountToCookie("acc1@gmail.com");
  helper.AddAccountToCookie("acc2@gmail.com");

  SimulateMergeSessionSuccess(&helper, "token1");

  helper.AddAccountToCookie("acc3@gmail.com");

  SimulateMergeSessionSuccess(&helper, "token2");
  SimulateMergeSessionSuccess(&helper, "token3");

  helper.AddAccountToCookie("acc4@gmail.com");

  SimulateMergeSessionSuccess(&helper, "token4");
}

TEST_F(GaiaCookieManagerServiceTest, LogOut) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<std::string> current_accounts;
  current_accounts.push_back("acc1@gmail.com");
  current_accounts.push_back("acc2@gmail.com");
  current_accounts.push_back("acc3@gmail.com");

  EXPECT_CALL(helper, StartLogOutUrlFetch());
  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc3@gmail.com",
                                                      no_error()));

  helper.LogOut("acc2@gmail.com", current_accounts);
  SimulateLogoutSuccess(&helper);
  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateMergeSessionSuccess(&helper, "token3");
}

TEST_F(GaiaCookieManagerServiceTest, PendingSigninThenSignout) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<std::string> current_accounts;
  current_accounts.push_back("acc2@gmail.com");
  current_accounts.push_back("acc3@gmail.com");

  // From the first Signin.
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));

  // From the sign out and then re-sign in.
  EXPECT_CALL(helper, StartLogOutUrlFetch());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc3@gmail.com",
                                                      no_error()));

  // Total sign in 2 times, not enforcing ordered sequences.
  EXPECT_CALL(helper, StartFetching()).Times(2);

  helper.AddAccountToCookie("acc1@gmail.com");
  helper.LogOut("acc2@gmail.com", current_accounts);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogoutSuccess(&helper);
  SimulateMergeSessionSuccess(&helper, "token3");
}

TEST_F(GaiaCookieManagerServiceTest, CancelSignIn) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<std::string> current_accounts;

  EXPECT_CALL(helper, StartFetching());
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc2@gmail.com",
                                                      canceled()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()));
  EXPECT_CALL(helper, StartLogOutUrlFetch());

  helper.AddAccountToCookie("acc1@gmail.com");
  helper.AddAccountToCookie("acc2@gmail.com");
  helper.LogOut("acc2@gmail.com", current_accounts);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogoutSuccess(&helper);
}

TEST_F(GaiaCookieManagerServiceTest, DoubleSignout) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  MockObserver observer(&helper);

  std::vector<std::string> current_accounts1;
  current_accounts1.push_back("acc1@gmail.com");
  current_accounts1.push_back("acc2@gmail.com");
  current_accounts1.push_back("acc3@gmail.com");

  std::vector<std::string> current_accounts2;
  current_accounts2.push_back("acc1@gmail.com");
  current_accounts2.push_back("acc3@gmail.com");

  EXPECT_CALL(helper, StartFetching()).Times(2);
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc3@gmail.com",
                                                      canceled()));
  EXPECT_CALL(observer, OnAddAccountToCookieCompleted("acc1@gmail.com",
                                                      no_error()))
      .Times(2);
  EXPECT_CALL(helper, StartLogOutUrlFetch());

  helper.AddAccountToCookie("acc1@gmail.com");
  helper.LogOut("acc2@gmail.com", current_accounts1);
  helper.LogOut("acc3@gmail.com", current_accounts2);

  SimulateMergeSessionSuccess(&helper, "token1");
  SimulateLogoutSuccess(&helper);
  SimulateMergeSessionSuccess(&helper, "token1");
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcher) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  MockObserver observer(&helper);
  EXPECT_CALL(observer, GetCheckConnectionInfoCompleted(true));
  result_fetcher.Start();

  // Simulate a successful completion of GetCheckConnctionInfo.
  net::TestURLFetcher* fetcher = factory()->GetFetcherByID(0);
  ASSERT_TRUE(NULL != fetcher);
  SimulateGetCheckConnctionInfoSuccess(
      fetcher,
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"},"
      " {\"carryBackToken\": \"bl\", \"url\": \"http://www.bl.com\"}]");

  // Simulate responses for the two connection URLs.
  GaiaCookieManagerService::ExternalCcResultFetcher::URLToTokenAndFetcher
      fetchers = result_fetcher.get_fetcher_map_for_testing();
  ASSERT_EQ(2u, fetchers.size());
  ASSERT_EQ(1u, fetchers.count(GURL("http://www.yt.com")));
  ASSERT_EQ(1u, fetchers.count(GURL("http://www.bl.com")));

  ASSERT_EQ("bl:null,yt:null", result_fetcher.GetExternalCcResult());
  SimulateGetCheckConnctionInfoResult(
      fetchers[GURL("http://www.yt.com")].second, "yt_result");
  ASSERT_EQ("bl:null,yt:yt_result", result_fetcher.GetExternalCcResult());
  SimulateGetCheckConnctionInfoResult(
      fetchers[GURL("http://www.bl.com")].second, "bl_result");
  ASSERT_EQ("bl:bl_result,yt:yt_result", result_fetcher.GetExternalCcResult());
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcherTimeout) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  MockObserver observer(&helper);
  EXPECT_CALL(observer, GetCheckConnectionInfoCompleted(false));
  result_fetcher.Start();

  // Simulate a successful completion of GetCheckConnctionInfo.
  net::TestURLFetcher* fetcher = factory()->GetFetcherByID(0);
  ASSERT_TRUE(NULL != fetcher);
  SimulateGetCheckConnctionInfoSuccess(
      fetcher,
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"},"
      " {\"carryBackToken\": \"bl\", \"url\": \"http://www.bl.com\"}]");

  GaiaCookieManagerService::ExternalCcResultFetcher::URLToTokenAndFetcher
      fetchers = result_fetcher.get_fetcher_map_for_testing();
  ASSERT_EQ(2u, fetchers.size());
  ASSERT_EQ(1u, fetchers.count(GURL("http://www.yt.com")));
  ASSERT_EQ(1u, fetchers.count(GURL("http://www.bl.com")));

  // Simulate response only for "yt".
  ASSERT_EQ("bl:null,yt:null", result_fetcher.GetExternalCcResult());
  SimulateGetCheckConnctionInfoResult(
      fetchers[GURL("http://www.yt.com")].second, "yt_result");
  ASSERT_EQ("bl:null,yt:yt_result", result_fetcher.GetExternalCcResult());

  // Now timeout.
  result_fetcher.TimeoutForTests();
  ASSERT_EQ("bl:null,yt:yt_result", result_fetcher.GetExternalCcResult());
  fetchers = result_fetcher.get_fetcher_map_for_testing();
  ASSERT_EQ(0u, fetchers.size());
}

TEST_F(GaiaCookieManagerServiceTest, ExternalCcResultFetcherTruncate) {
  InstrumentedGaiaCookieManagerService helper(token_service(), signin_client());
  GaiaCookieManagerService::ExternalCcResultFetcher result_fetcher(&helper);
  result_fetcher.Start();

  // Simulate a successful completion of GetCheckConnctionInfo.
  net::TestURLFetcher* fetcher = factory()->GetFetcherByID(0);
  ASSERT_TRUE(NULL != fetcher);
  SimulateGetCheckConnctionInfoSuccess(
      fetcher,
      "[{\"carryBackToken\": \"yt\", \"url\": \"http://www.yt.com\"}]");

  GaiaCookieManagerService::ExternalCcResultFetcher::URLToTokenAndFetcher
      fetchers = result_fetcher.get_fetcher_map_for_testing();
  ASSERT_EQ(1u, fetchers.size());
  ASSERT_EQ(1u, fetchers.count(GURL("http://www.yt.com")));

  // Simulate response for "yt" with a string that is too long.
  SimulateGetCheckConnctionInfoResult(
      fetchers[GURL("http://www.yt.com")].second, "1234567890123456trunc");
  ASSERT_EQ("yt:1234567890123456", result_fetcher.GetExternalCcResult());
}
