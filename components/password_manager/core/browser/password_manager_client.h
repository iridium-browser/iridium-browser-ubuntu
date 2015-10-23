// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_

#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_store.h"

class PrefService;

namespace autofill {
class AutofillManager;
}

namespace password_manager {

struct CredentialInfo;
class PasswordFormManager;
class PasswordManager;
class PasswordManagerDriver;
class PasswordStore;

enum PasswordSyncState {
  NOT_SYNCING_PASSWORDS,
  SYNCING_NORMAL_ENCRYPTION,
  SYNCING_WITH_CUSTOM_PASSPHRASE
};

enum class CredentialSourceType {
  CREDENTIAL_SOURCE_PASSWORD_MANAGER = 0,
  CREDENTIAL_SOURCE_API,
  CREDENTIAL_SOURCE_LAST = CREDENTIAL_SOURCE_API
};

// An abstraction of operations that depend on the embedders (e.g. Chrome)
// environment.
class PasswordManagerClient {
 public:
  PasswordManagerClient() {}
  virtual ~PasswordManagerClient() {}

  // For automated testing, the save password prompt should sometimes not be
  // shown, and password immediately saved instead. That can be enforced by
  // a command-line flag. If auto-saving is enforced, this method returns true.
  // The default return value is false.
  virtual bool IsAutomaticPasswordSavingEnabled() const;

  // If the password manager should work for the current page. Default
  // always returns true.
  virtual bool IsPasswordManagementEnabledForCurrentPage() const;

  // Is saving new data for password autofill enabled for the current profile
  // and page? For example, saving new data is disabled in Incognito mode,
  // whereas filling data is not. Also, saving data is disabled in the presence
  // of SSL errors on a page.
  virtual bool IsSavingEnabledForCurrentPage() const;

  // Return the username that the user is syncing with. Should return an empty
  // string if sync is not enabled for passwords.
  virtual std::string GetSyncUsername() const = 0;

  // Returns true if |username| and signon |realm| correspond to the account
  // which is syncing.
  virtual bool IsSyncAccountCredential(const std::string& username,
                                       const std::string& realm) const = 0;

  // Informs the embedder of a password form that can be saved or updated in
  // password store if the user allows it. The embedder is not required to
  // prompt the user if it decides that this form doesn't need to be saved or
  // updated. Returns true if the prompt was indeed displayed.
  // There are 3 different cases when |update_password| == true:
  // 1.A change password form was submitted and the user has only one stored
  // credential. Then form_to_save.pending_credentials() should correspond to
  // the unique element from |form_to_save.best_matches_|.
  // 2.A change password form was submitted and the user has more than one
  // stored credential. Then we shouldn't expect anything from
  // form_to_save.pending_credentials() except correct origin, since we don't
  // know which credentials should be updated.
  // 3.A sign-in password form was submitted with a password different from
  // the stored one. In this case form_to_save.password_overridden() == true
  // and form_to_save.pending_credentials() should correspond to the credential
  // that was overidden.
  virtual bool PromptUserToSaveOrUpdatePassword(
      scoped_ptr<PasswordFormManager> form_to_save,
      CredentialSourceType type,
      bool update_password) = 0;

  // Informs the embedder of a password forms that the user should choose from.
  // Returns true if the prompt is indeed displayed. If the prompt is not
  // displayed, returns false and does not call |callback|.
  // |callback| should be invoked with the chosen form.
  virtual bool PromptUserToChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_forms,
      ScopedVector<autofill::PasswordForm> federated_forms,
      const GURL& origin,
      base::Callback<void(const CredentialInfo&)> callback) = 0;

  // Informs the embedder that the user has manually requested to save the
  // password in the focused password field.
  virtual void ForceSavePassword();

  // Informs the embedder that automatic signing in just happened. The form
  // returned to the site is |local_forms[0]|. |local_forms| and
  // |federated_forms| contain all the local and federated credentials for the
  // site.
  virtual void NotifyUserAutoSignin(
      ScopedVector<autofill::PasswordForm> local_forms) = 0;

  // Called when a password is saved in an automated fashion. Embedder may
  // inform the user that this save has occured.
  virtual void AutomaticPasswordSave(
      scoped_ptr<PasswordFormManager> saved_form_manager) = 0;

  // Called when a password is autofilled. |best_matches| contains the
  // PasswordForm into which a password was filled: the client may choose to
  // save this to the PasswordStore, for example. Default implementation is a
  // noop.
  virtual void PasswordWasAutofilled(
      const autofill::PasswordFormMap& best_matches) const;

  // Called when password autofill is blocked by the blacklist. |best_matches|
  // contains the PasswordForm that flags the current site as being on the
  // blacklist. The client may choose to remove this from the PasswordStore in
  // order to unblacklist a site, for example. Default implementation is a noop.
  virtual void PasswordAutofillWasBlocked(
      const autofill::PasswordFormMap& best_matches) const;

  // Gets prefs associated with this embedder.
  virtual PrefService* GetPrefs() = 0;

  // Returns the PasswordStore associated with this instance.
  virtual PasswordStore* GetPasswordStore() const = 0;

  // Reports whether and how passwords are synced in the embedder. The default
  // implementation always returns NOT_SYNCING_PASSWORDS.
  virtual PasswordSyncState GetPasswordSyncState() const;

  // Only for clients which registered with a LogRouter: If called with
  // |router_can_be_used| set to false, the client may no longer use the
  // LogRouter. If |router_can_be_used| is true, the LogRouter can be used after
  // the return from OnLogRouterAvailabilityChanged.
  virtual void OnLogRouterAvailabilityChanged(bool router_can_be_used);

  // Forward |text| for display to the LogRouter (if registered with one).
  virtual void LogSavePasswordProgress(const std::string& text) const;

  // Returns true if logs recorded via LogSavePasswordProgress will be
  // displayed, and false otherwise.
  virtual bool IsLoggingActive() const;

  // Returns true if last navigation page had HTTP error i.e 5XX or 4XX
  virtual bool WasLastNavigationHTTPError() const;

  // Returns the authorization prompt policy to be used with the given form.
  // Only relevant on OSX.
  virtual PasswordStore::AuthorizationPromptPolicy GetAuthorizationPromptPolicy(
      const autofill::PasswordForm& form);

  // Returns whether any SSL certificate errors were encountered as a result of
  // the last page load.
  virtual bool DidLastPageLoadEncounterSSLErrors() const;

  // If this browsing session should not be persisted.
  virtual bool IsOffTheRecord() const;

  // Returns the PasswordManager associated with this client.
  virtual PasswordManager* GetPasswordManager();

  // Returns the AutofillManager for the main frame.
  virtual autofill::AutofillManager* GetAutofillManagerForMainFrame();

  // Returns the main frame URL.
  virtual const GURL& GetMainFrameURL() const;

  // Returns true if the UI for confirmation of update password is enabled.
  virtual bool IsUpdatePasswordUIEnabled() const;

  virtual const GURL& GetLastCommittedEntryURL() const = 0;

  // Creates a filter for PasswordFormManager to process password store
  // response. One filter should be created for every batch of store results for
  // a single observed form. The filter results should not be cached.
  virtual scoped_ptr<password_manager::CredentialsFilter>
  CreateStoreResultFilter() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
