// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/login_model.h"
#include "components/password_manager/core/browser/password_form_manager.h"

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill {
class FormStructure;
}

namespace password_manager {

class BrowserSavePasswordProgressLogger;
class PasswordManagerClient;
class PasswordManagerDriver;
class PasswordFormManager;

// TODO(melandory): Separate the PasswordFormManager API interface and the
// implementation in two classes http://crbug.com/473184.

// Per-tab password manager. Handles creation and management of UI elements,
// receiving password form data from the renderer and managing the password
// database through the PasswordStore. The PasswordManager is a LoginModel
// for purposes of supporting HTTP authentication dialogs.
class PasswordManager : public LoginModel {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#if defined(OS_WIN)
  static void RegisterLocalPrefs(PrefRegistrySimple* registry);
#endif
  explicit PasswordManager(PasswordManagerClient* client);
  ~PasswordManager() override;

  typedef base::Callback<void(const autofill::PasswordForm&)>
      PasswordSubmittedCallback;

  // There is no corresponding remove function as currently all of the
  // owners of these callbacks have sufficient lifetimes so that the callbacks
  // should always be valid when called.
  void AddSubmissionCallback(const PasswordSubmittedCallback& callback);

  // Called by a PasswordFormManager when it decides a form can be autofilled
  // on the page.
  virtual void Autofill(password_manager::PasswordManagerDriver* driver,
                        const autofill::PasswordForm& form_for_autofill,
                        const autofill::PasswordFormMap& best_matches,
                        const autofill::PasswordForm& preferred_match,
                        bool wait_for_username) const;

  // LoginModel implementation.
  void AddObserver(LoginModelObserver* observer) override;
  void RemoveObserver(LoginModelObserver* observer) override;

  // Update the state of generation for this form.
  void SetHasGeneratedPasswordForForm(
      password_manager::PasswordManagerDriver* driver,
      const autofill::PasswordForm& form,
      bool password_is_generated);

  // TODO(isherman): This should not be public, but is currently being used by
  // the LoginPrompt code.
  // When a form is submitted, we prepare to save the password but wait
  // until we decide the user has successfully logged in. This is step 1
  // of 2 (see SavePassword).
  void ProvisionallySavePassword(const autofill::PasswordForm& form);

  // Should be called when the user navigates the main frame. Not called for
  // in-page navigation.
  void DidNavigateMainFrame();

  // Handles password forms being parsed.
  void OnPasswordFormsParsed(password_manager::PasswordManagerDriver* driver,
                             const std::vector<autofill::PasswordForm>& forms);

  // Handles password forms being rendered.
  void OnPasswordFormsRendered(
      password_manager::PasswordManagerDriver* driver,
      const std::vector<autofill::PasswordForm>& visible_forms,
      bool did_stop_loading);

  // Handles a password form being submitted.
  virtual void OnPasswordFormSubmitted(
      password_manager::PasswordManagerDriver* driver,
      const autofill::PasswordForm& password_form);

  // Called if |password_form| was filled upon in-page navigation. This often
  // means history.pushState being called from JavaScript. If this causes false
  // positive in password saving, update http://crbug.com/357696.
  void OnInPageNavigation(password_manager::PasswordManagerDriver* driver,
                          const autofill::PasswordForm& password_form);

  void ProcessAutofillPredictions(
      password_manager::PasswordManagerDriver* driver,
      const std::vector<autofill::FormStructure*>& forms);

  PasswordManagerClient* client() { return client_; }

 private:
  enum ProvisionalSaveFailure {
    SAVING_DISABLED,
    EMPTY_PASSWORD,
    NO_MATCHING_FORM,
    MATCHING_NOT_COMPLETE,
    FORM_BLACKLISTED,
    INVALID_FORM,
    SYNC_CREDENTIAL,
    MAX_FAILURE_VALUE
  };

  // Log failure for UMA. Logs additional metrics if the |form_origin|
  // corresponds to one of the top, explicitly monitored websites. For some
  // values of |failure| also sends logs to the internals page through |logger|,
  // it |logger| is not NULL.
  void RecordFailure(ProvisionalSaveFailure failure,
                     const GURL& form_origin,
                     BrowserSavePasswordProgressLogger* logger);

  // Returns true if we can show possible usernames to users in cases where
  // the username for the form is ambigious.
  bool OtherPossibleUsernamesEnabled() const;

  // Returns true if |provisional_save_manager_| is ready for saving and
  // non-blacklisted.
  bool CanProvisionalManagerSave();

  // Returns true if the user needs to be prompted before a password can be
  // saved (instead of automatically saving
  // the password), based on inspecting the state of
  // |provisional_save_manager_|.
  bool ShouldPromptUserToSavePassword() const;

  // Called when we already decided that login was correct and we want to save
  // password.
  void AskUserOrSavePassword();

  // Checks for every from in |forms| whether |pending_login_managers_| already
  // contain a manager for that form. If not, adds a manager for each such form.
  void CreatePendingLoginManagers(
      password_manager::PasswordManagerDriver* driver,
      const std::vector<autofill::PasswordForm>& forms);

  // Note about how a PasswordFormManager can transition from
  // pending_login_managers_ to provisional_save_manager_ and the infobar.
  //
  // 1. form "seen"
  //       |                                             new
  //       |                                               ___ Infobar
  // pending_login -- form submit --> provisional_save ___/
  //             ^                            |           \___ (update DB)
  //             |                           fail
  //             |-----------<------<---------|          !new
  //
  // When a form is "seen" on a page, a PasswordFormManager is created
  // and stored in this collection until user navigates away from page.

  ScopedVector<PasswordFormManager> pending_login_managers_;

  // When the user submits a password/credential, this contains the
  // PasswordFormManager for the form in question until we deem the login
  // attempt to have succeeded (as in valid credentials). If it fails, we
  // send the PasswordFormManager back to the pending_login_managers_ set.
  // Scoped in case PasswordManager gets deleted (e.g tab closes) between the
  // time a user submits a login form and gets to the next page.
  scoped_ptr<PasswordFormManager> provisional_save_manager_;

  // The embedder-level client. Must outlive this class.
  PasswordManagerClient* const client_;

  // Observers to be notified of LoginModel events.  This is mutable to allow
  // notification in const member functions.
  mutable ObserverList<LoginModelObserver> observers_;

  // Callbacks to be notified when a password form has been submitted.
  std::vector<PasswordSubmittedCallback> submission_callbacks_;

  // Records all visible forms seen during a page load, in all frames of the
  // page. When the page stops loading, the password manager checks if one of
  // the recorded forms matches the login form from the previous page
  // (to see if the login was a failure), and clears the vector.
  std::vector<autofill::PasswordForm> all_visible_forms_;

  // The user-visible URL from the last time a password was provisionally saved.
  GURL main_frame_url_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_H_
