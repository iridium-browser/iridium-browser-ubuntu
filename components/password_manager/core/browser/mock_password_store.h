// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordStore : public PasswordStore {
 public:
  MockPasswordStore();

  MOCK_METHOD1(RemoveLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD3(GetLogins,
               void(const autofill::PasswordForm&,
                    PasswordStore::AuthorizationPromptPolicy prompt_policy,
                    PasswordStoreConsumer*));
  MOCK_METHOD1(AddLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD1(UpdateLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD2(UpdateLoginWithPrimaryKey,
               void(const autofill::PasswordForm&,
                    const autofill::PasswordForm&));
  MOCK_METHOD2(ReportMetrics, void(const std::string&, bool));
  MOCK_METHOD2(ReportMetricsImpl, void(const std::string&, bool));
  MOCK_METHOD1(AddLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(UpdateLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(RemoveLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD2(RemoveLoginsCreatedBetweenImpl,
               PasswordStoreChangeList(base::Time, base::Time));
  MOCK_METHOD2(RemoveLoginsSyncedBetweenImpl,
               PasswordStoreChangeList(base::Time, base::Time));
  ScopedVector<autofill::PasswordForm> FillMatchingLogins(
      const autofill::PasswordForm& form,
      PasswordStore::AuthorizationPromptPolicy prompt_policy) override {
    return ScopedVector<autofill::PasswordForm>();
  }
  MOCK_METHOD1(FillAutofillableLogins,
               bool(ScopedVector<autofill::PasswordForm>*));
  MOCK_METHOD1(FillBlacklistLogins,
               bool(ScopedVector<autofill::PasswordForm>*));
  MOCK_METHOD1(NotifyLoginsChanged, void(const PasswordStoreChangeList&));
  void AddSiteStatsImpl(const InteractionsStats& stats) override {}
  void RemoveSiteStatsImpl(const GURL& origin_domain) override {}
  scoped_ptr<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) override {
    return scoped_ptr<InteractionsStats>();
  }

  PasswordStoreSync* GetSyncInterface() { return this; }

 protected:
  virtual ~MockPasswordStore();
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
