// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_H_

#include "base/memory/scoped_vector.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace password_manager {
enum class CredentialType;
struct CredentialInfo;
class PasswordFormManager;
}

class ManagePasswordsIcon;

// Per-tab class to control the Omnibox password icon and bubble.
class ManagePasswordsUIController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ManagePasswordsUIController>,
      public password_manager::PasswordStore::Observer {
 public:
  ~ManagePasswordsUIController() override;

  // Called when the user submits a form containing login information, so we
  // can handle later requests to save or blacklist that login information.
  // This stores the provided object and triggers the UI to prompt the user
  // about whether they would like to save the password.
  void OnPasswordSubmitted(
      scoped_ptr<password_manager::PasswordFormManager> form_manager);

  // Called when the site asks user to choose from credentials. This triggers
  // the UI to prompt the user. |local_credentials| and |federated_credentials|
  // shouldn't both be empty.
  bool OnChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_credentials,
      ScopedVector<autofill::PasswordForm> federated_credentials,
      const GURL& origin,
      base::Callback<void(const password_manager::CredentialInfo&)> callback);

  // Called when user is auto signed in to the site. |local_forms[0]| contains
  // the credential returned to the site.
  void OnAutoSignin(ScopedVector<autofill::PasswordForm> local_forms);

  // Called when the password will be saved automatically, but we still wish to
  // visually inform the user that the save has occured.
  void OnAutomaticPasswordSave(
      scoped_ptr<password_manager::PasswordFormManager> form_manager);

  // Called when a form is autofilled with login information, so we can manage
  // password credentials for the current site which are stored in
  // |password_form_map|. This stores a copy of |password_form_map| and shows
  // the manage password icon.
  void OnPasswordAutofilled(const autofill::PasswordFormMap& password_form_map);

  // Called when a form is _not_ autofilled due to user blacklisting. This
  // stores a copy of |password_form_map| so that we can offer the user the
  // ability to reenable the manager for this form.
  void OnBlacklistBlockedAutofill(
      const autofill::PasswordFormMap& password_form_map);

  // PasswordStore::Observer implementation.
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // Called from the model when the user chooses to save a password; passes the
  // action off to the FormManager. The controller MUST be in a pending state,
  // and WILL be in MANAGE_STATE after this method executes.
  virtual void SavePassword();

  // Called from the model when the user chooses a credential.
  // The controller MUST be in a pending credentials state.
  virtual void ChooseCredential(
      const autofill::PasswordForm& form,
      password_manager::CredentialType credential_type);

  // Called from the model when the user chooses to never save passwords; passes
  // the action off to the FormManager. The controller MUST be in a pending
  // state, and WILL be in BLACKLIST_STATE after this method executes.
  virtual void NeverSavePassword();

  // Called from the model when the user chooses to unblacklist the site. The
  // controller MUST be in BLACKLIST_STATE, and WILL be in MANAGE_STATE after
  // this method executes. The method removes the first form of
  // GetCurrentForms() which should be the blacklisted one.
  virtual void UnblacklistSite();

  // Called from the model. The controller should switch to MANAGE_STATE and pop
  // up a bubble.
  virtual void ManageAccounts();

  // Open a new tab, pointing to the password manager settings page.
  virtual void NavigateToPasswordManagerSettingsPage();

  // Open a new tab, pointing to passwords.google.com.
  void NavigateToExternalPasswordManager();

  // Open a new tab, pointing to the Smart Lock help article.
  void NavigateToSmartLockHelpArticle();

  virtual const autofill::PasswordForm& PendingPassword() const;

  // Set the state of the Omnibox icon, and possibly show the associated bubble
  // without user interaction.
  virtual void UpdateIconAndBubbleState(ManagePasswordsIcon* icon);

  // Called from the model when the bubble is displayed.
  void OnBubbleShown();

  // Called from the model when the bubble is hidden.
  void OnBubbleHidden();

  password_manager::ui::State state() const { return passwords_data_.state(); }

  // True if a password is sitting around, waiting for a user to decide whether
  // or not to save it.
  bool PasswordPendingUserDecision() const {
    return state() == password_manager::ui::PENDING_PASSWORD_STATE;
  }

  const GURL& origin() const { return passwords_data_.origin(); }

  bool IsAutomaticallyOpeningBubble() const { return should_pop_up_bubble_; }

  // Current local forms.
  const std::vector<const autofill::PasswordForm*>& GetCurrentForms() const {
    return passwords_data_.GetCurrentForms();
  }

  // Current federated forms.
  const std::vector<const autofill::PasswordForm*>& GetFederatedForms() const {
    return passwords_data_.federated_credentials_forms();
  }

 protected:
  explicit ManagePasswordsUIController(
      content::WebContents* web_contents);

  // The pieces of saving and blacklisting passwords that interact with
  // FormManager, split off into internal functions for testing/mocking.
  virtual void SavePasswordInternal();
  virtual void NeverSavePasswordInternal();

  // Called when a PasswordForm is autofilled, when a new PasswordForm is
  // submitted, or when a navigation occurs to update the visibility of the
  // manage passwords icon and bubble.
  virtual void UpdateBubbleAndIconVisibility();

  // Returns the time elapsed since |timer_| was initialized,
  // or base::TimeDelta::Max() if |timer_| was not initialized.
  virtual base::TimeDelta Elapsed() const;

  // Overwrites the client for |passwords_data_|.
  void set_client(password_manager::PasswordManagerClient* client) {
    passwords_data_.set_client(client);
  }

  // content::WebContentsObserver:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void WasHidden() override;

 private:
  friend class content::WebContentsUserData<ManagePasswordsUIController>;

  // Shows the password bubble without user interaction.
  void ShowBubbleWithoutUserInteraction();

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // Shows infobar which allows user to choose credentials. Placing this
  // code to separate method allows mocking.
  virtual void UpdateAndroidAccountChooserInfoBarVisibility();

  // The wrapper around current state and data.
  ManagePasswordsState passwords_data_;

  // Used to measure the amount of time on a page; if it's less than some
  // reasonable limit, then don't close the bubble upon navigation. We create
  // (and destroy) the timer in DidNavigateMainFrame.
  scoped_ptr<base::ElapsedTimer> timer_;

  // Contains true if the bubble is to be popped up in the next call to
  // UpdateBubbleAndIconVisibility().
  bool should_pop_up_bubble_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsUIController);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_H_
