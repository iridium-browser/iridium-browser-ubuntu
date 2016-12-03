// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_STATUS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_STATUS_SERVICE_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/core/browser/signin_manager.h"

class PrefRegistrySimple;
class PrefService;

namespace ntp_snippets {

enum class DisabledReason : int {
  // Snippets are enabled
  NONE,
  // Snippets have been disabled as part of the service configuration.
  EXPLICITLY_DISABLED,
  // The user is not signed in, and the service requires it to be enabled.
  SIGNED_OUT,
};

// Aggregates data from preferences and signin to notify the snippet service of
// relevant changes in their states.
class NTPSnippetsStatusService : public SigninManagerBase::Observer {
 public:
  typedef base::Callback<void(DisabledReason)> DisabledReasonChangeCallback;

  NTPSnippetsStatusService(SigninManagerBase* signin_manager,
                           PrefService* pref_service);

  ~NTPSnippetsStatusService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Starts listening for changes from the dependencies. |callback| will be
  // called when a significant change in state is detected.
  void Init(const DisabledReasonChangeCallback& callback);

  DisabledReason disabled_reason() const { return disabled_reason_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(NTPSnippetsStatusServiceTest, DisabledViaPref);

  // SigninManagerBase::Observer implementation
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // Callback for the PrefChangeRegistrar.
  void OnStateChanged();

  DisabledReason GetDisabledReasonFromDeps() const;

  DisabledReason disabled_reason_;
  DisabledReasonChangeCallback disabled_reason_change_callback_;

  bool require_signin_;
  SigninManagerBase* signin_manager_;
  PrefService* pref_service_;

  PrefChangeRegistrar pref_change_registrar_;

  // The observer for the SigninManager.
  ScopedObserver<SigninManagerBase, SigninManagerBase::Observer>
      signin_observer_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsStatusService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_STATUS_SERVICE_H_
