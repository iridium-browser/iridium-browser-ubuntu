// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_GAIA_COOKIE_MANAGER_SERVICE_H
#define COMPONENTS_SIGNIN_CORE_BROWSER_GAIA_COOKIE_MANAGER_SERVICE_H

#include <deque>

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/signin/core/browser/signin_client.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/ubertoken_fetcher.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"

class GaiaAuthFetcher;
class GaiaCookieRequest;
class GoogleServiceAuthError;
class OAuth2TokenService;

namespace net {
class URLFetcher;
}

// Merges a Google account known to Chrome into the cookie jar.  When merging
// multiple accounts, one instance of the helper is better than multiple
// instances if there is the possibility that they run concurrently, since
// changes to the cookie must be serialized.
//
// Also checks the External CC result to ensure no services that consume the
// GAIA cookie are blocked (such as youtube). This is executed once for the
// lifetime of this object, when the first call is made to AddAccountToCookie.
class GaiaCookieManagerService : public KeyedService,
                                 public GaiaAuthConsumer,
                                 public UbertokenConsumer {
 public:
  enum GaiaCookieRequestType {
    ADD_ACCOUNT,
    LOG_OUT,
    LIST_ACCOUNTS
  };

  // Contains the information and parameters for any request.
  class GaiaCookieRequest {
   public:
    ~GaiaCookieRequest();

    GaiaCookieRequestType request_type() const { return request_type_; }
    const std::string& account_id() const {return account_id_; }

    static GaiaCookieRequest CreateAddAccountRequest(
        const std::string& account_id);
    static GaiaCookieRequest CreateLogOutRequest();
    static GaiaCookieRequest CreateListAccountsRequest();

   private:
    GaiaCookieRequest(
        GaiaCookieRequestType request_type,
        const std::string& account_id);

    GaiaCookieRequestType request_type_;
    std::string account_id_;
  };

  class Observer {
   public:
    // Called whenever a merge session is completed.  The account that was
    // merged is given by |account_id|.  If |error| is equal to
    // GoogleServiceAuthError::AuthErrorNone() then the merge succeeeded.
    virtual void OnAddAccountToCookieCompleted(
        const std::string& account_id,
        const GoogleServiceAuthError& error) {}

    // Called whenever the GaiaCookieManagerService's list of GAIA accounts is
    // updated. The GCMS monitors the APISID cookie and triggers a /ListAccounts
    // call on change. The GCMS will also call ListAccounts upon the first call
    // to ListAccounts(). The GCMS will delay calling ListAccounts if other
    // requests are in queue that would modify the APISID cookie.
    // If the ListAccounts call fails and the GCMS cannot recover, the reason
    // is passed in |error|.
    virtual void OnGaiaAccountsInCookieUpdated(
        const std::vector<gaia::ListedAccount>& accounts,
        const GoogleServiceAuthError& error) {}

   protected:
    virtual ~Observer() {}
  };

  // Class to retrieve the external connection check results from gaia.
  // Declared publicly for unit tests.
  class ExternalCcResultFetcher : public GaiaAuthConsumer,
                                  public net::URLFetcherDelegate {
   public:
    // Maps connection URLs, as returned by StartGetCheckConnectionInfo() to
    // token and URLFetcher used to fetch the URL.
    typedef std::map<GURL, std::pair<std::string, net::URLFetcher*>>
        URLToTokenAndFetcher;

    // Maps tokens to the fetched result for that token.
    typedef std::map<std::string, std::string> ResultMap;

    ExternalCcResultFetcher(GaiaCookieManagerService* helper);
    ~ExternalCcResultFetcher() override;

    // Gets the current value of the external connection check result string.
    std::string GetExternalCcResult();

    // Start fetching the external CC result.  If a fetch is already in progress
    // it is canceled.
    void Start();

    // Are external URLs still being checked?
    bool IsRunning();

    // Returns a copy of the internal token to fetcher map.
    URLToTokenAndFetcher get_fetcher_map_for_testing() { return fetchers_; }

    // Simulate a timeout for tests.
    void TimeoutForTests();

   private:
    // Overridden from GaiaAuthConsumer.
    void OnGetCheckConnectionInfoSuccess(const std::string& data) override;
    void OnGetCheckConnectionInfoError(
        const GoogleServiceAuthError& error) override;

    // Creates and initializes a URL fetcher for doing a connection check.
    scoped_ptr<net::URLFetcher> CreateFetcher(const GURL& url);

    // Overridden from URLFetcherDelgate.
    void OnURLFetchComplete(const net::URLFetcher* source) override;

    // Any fetches still ongoing after this call are considered timed out.
    void Timeout();

    void CleanupTransientState();

    void GetCheckConnectionInfoCompleted(bool succeeded);

    GaiaCookieManagerService* helper_;
    base::OneShotTimer<ExternalCcResultFetcher> timer_;
    URLToTokenAndFetcher fetchers_;
    ResultMap results_;
    base::Time m_external_cc_result_start_time_;

    base::OneShotTimer<ExternalCcResultFetcher> gaia_auth_fetcher_timer_;

    DISALLOW_COPY_AND_ASSIGN(ExternalCcResultFetcher);
  };

  GaiaCookieManagerService(OAuth2TokenService* token_service,
                           const std::string& source,
                           SigninClient* signin_client);
  ~GaiaCookieManagerService() override;

  void Init();
  void Shutdown() override;

  void AddAccountToCookie(const std::string& account_id);
  void AddAccountToCookieWithToken(const std::string& account_id,
                                   const std::string& access_token);

  // Returns if the listed accounts are up to date or not (ignore the out
  // parameter if return is false). The parameter will be assigned the current
  // cached accounts. If the accounts are not up to date, a ListAccounts fetch
  // is sent GAIA and Observer::OnGaiaAccountsInCookieUpdated will be called.
  bool ListAccounts(std::vector<gaia::ListedAccount>* accounts);

  // Add or remove observers of this helper.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Cancel all login requests.
  void CancelAll();

  // Signout all accounts.
  void LogOutAllAccounts();

  // Call observers when merge session completes.  This public so that callers
  // that know that a given account is already in the cookie jar can simply
  // inform the observers.
  void SignalComplete(const std::string& account_id,
                      const GoogleServiceAuthError& error);

  // Returns true of there are pending log ins or outs.
  bool is_running() const { return requests_.size() > 0; }

  // Access the internal object during tests.
  ExternalCcResultFetcher* external_cc_result_fetcher_for_testing() {
    return &external_cc_result_fetcher_;
  }

  void set_list_accounts_stale_for_testing(bool stale) {
    list_accounts_stale_ = stale;
  }

 private:
  net::URLRequestContextGetter* request_context() {
    return signin_client_->GetURLRequestContext();
  }

  // Called when a cookie changes. If the cookie relates to a GAIA APISID
  // cookie, then we call ListAccounts and fire OnGaiaAccountsInCookieUpdated.
  void OnCookieChanged(const net::CanonicalCookie& cookie, bool removed);

  // Overridden from UbertokenConsumer.
  void OnUbertokenSuccess(const std::string& token) override;
  void OnUbertokenFailure(const GoogleServiceAuthError& error) override;

  // Overridden from GaiaAuthConsumer.
  void OnMergeSessionSuccess(const std::string& data) override;
  void OnMergeSessionFailure(const GoogleServiceAuthError& error) override;
  void OnListAccountsSuccess(const std::string& data) override;
  void OnListAccountsFailure(const GoogleServiceAuthError& error) override;
  void OnLogOutSuccess() override;
  void OnLogOutFailure(const GoogleServiceAuthError& error) override;

  // Helper method for AddAccountToCookie* methods.
  void AddAccountToCookieInternal(const std::string& account_id);

  // Starts the proess of fetching the uber token and performing a merge session
  // for the next account.  Virtual so that it can be overriden in tests.
  virtual void StartFetchingUbertoken();

  // Virtual for testing purposes.
  virtual void StartFetchingMergeSession();

  // Virtual for testing purposes.
  virtual void StartFetchingListAccounts();

  // Virtual for testing purpose.
  virtual void StartFetchingLogOut();

  // Start the next request, if needed.
  void HandleNextRequest();

  OAuth2TokenService* token_service_;
  SigninClient* signin_client_;
  scoped_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  scoped_ptr<UbertokenFetcher> uber_token_fetcher_;
  ExternalCcResultFetcher external_cc_result_fetcher_;

  // If the GaiaAuthFetcher or URLFetcher fails, retry with exponential backoff
  // and network delay.
  net::BackoffEntry fetcher_backoff_;
  // We can safely depend on the SigninClient here because there is an explicit
  // dependency, as noted in the GaiaCookieManagerServiceFactory.
  base::OneShotTimer<SigninClient> fetcher_timer_;
  int fetcher_retries_;

  // The last fetched ubertoken, for use in MergeSession retries.
  std::string uber_token_;

  // The access token that can be used to prime the UberToken fetch.
  std::string access_token_;

  // Subscription to be called whenever the GAIA cookies change.
  scoped_ptr<SigninClient::CookieChangedSubscription>
      cookie_changed_subscription_;

  // A worklist for this class. Stores any pending requests that couldn't be
  // executed right away, since this class only permits one request to be
  // executed at a time.
  std::deque<GaiaCookieRequest> requests_;

  // List of observers to notify when merge session completes.
  // Makes sure list is empty on destruction.
  base::ObserverList<Observer, true> observer_list_;

  // Source to use with GAIA endpoints for accounting.
  std::string source_;

  // True once the ExternalCCResultFetcher has completed once.
  bool external_cc_result_fetched_;

  std::vector<gaia::ListedAccount> listed_accounts_;

  bool list_accounts_stale_;

  DISALLOW_COPY_AND_ASSIGN(GaiaCookieManagerService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_GAIA_COOKIE_MANAGER_SERVICE_H
