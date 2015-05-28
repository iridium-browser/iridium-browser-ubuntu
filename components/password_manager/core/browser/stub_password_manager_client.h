// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_

#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

// Use this class as a base for mock or test clients to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs.  Do NOT use in production, only use in tests.
class StubPasswordManagerClient : public PasswordManagerClient {
 public:
  StubPasswordManagerClient();
  ~StubPasswordManagerClient() override;

  // PasswordManagerClient:
  std::string GetSyncUsername() const override;
  bool IsSyncAccountCredential(const std::string& username,
                               const std::string& origin) const override;
  bool ShouldFilterAutofillResult(const autofill::PasswordForm& form) override;
  bool PromptUserToSavePassword(
      scoped_ptr<PasswordFormManager> form_to_save,
      password_manager::CredentialSourceType type) override;
  bool PromptUserToChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_forms,
      ScopedVector<autofill::PasswordForm> federated_forms,
      const GURL& origin,
      base::Callback<void(const password_manager::CredentialInfo&)> callback)
      override;
  void NotifyUserAutoSignin(
      ScopedVector<autofill::PasswordForm> local_forms) override;
  void AutomaticPasswordSave(
      scoped_ptr<PasswordFormManager> saved_manager) override;
  PrefService* GetPrefs() override;
  PasswordStore* GetPasswordStore() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StubPasswordManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_CLIENT_H_
