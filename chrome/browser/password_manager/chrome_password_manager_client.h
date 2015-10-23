// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_member.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/browser/credential_manager_dispatcher.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/rect.h"

class Profile;

namespace autofill {
class PasswordGenerationPopupObserver;
class PasswordGenerationPopupControllerImpl;
}

namespace content {
class WebContents;
}

namespace password_manager {
struct CredentialInfo;
class PasswordGenerationManager;
class PasswordManagerDriver;
}

// ChromePasswordManagerClient implements the PasswordManagerClient interface.
class ChromePasswordManagerClient
    : public password_manager::PasswordManagerClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromePasswordManagerClient> {
 public:
  ~ChromePasswordManagerClient() override;

  // PasswordManagerClient implementation.
  bool IsAutomaticPasswordSavingEnabled() const override;
  bool IsPasswordManagementEnabledForCurrentPage() const override;
  bool IsSavingEnabledForCurrentPage() const override;
  std::string GetSyncUsername() const override;
  bool IsSyncAccountCredential(const std::string& username,
                               const std::string& realm) const override;
  bool PromptUserToSaveOrUpdatePassword(
      scoped_ptr<password_manager::PasswordFormManager> form_to_save,
      password_manager::CredentialSourceType type,
      bool update_password) override;
  bool PromptUserToChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_forms,
      ScopedVector<autofill::PasswordForm> federated_forms,
      const GURL& origin,
      base::Callback<void(const password_manager::CredentialInfo&)> callback)
      override;
  void ForceSavePassword() override;
  void NotifyUserAutoSignin(
      ScopedVector<autofill::PasswordForm> local_forms) override;
  void AutomaticPasswordSave(scoped_ptr<password_manager::PasswordFormManager>
                                 saved_form_manager) override;
  void PasswordWasAutofilled(
      const autofill::PasswordFormMap& best_matches) const override;
  void PasswordAutofillWasBlocked(
      const autofill::PasswordFormMap& best_matches) const override;
  PrefService* GetPrefs() override;
  password_manager::PasswordStore* GetPasswordStore() const override;
  password_manager::PasswordSyncState GetPasswordSyncState() const override;
  void OnLogRouterAvailabilityChanged(bool router_can_be_used) override;
  void LogSavePasswordProgress(const std::string& text) const override;
  bool IsLoggingActive() const override;
  bool WasLastNavigationHTTPError() const override;
  bool DidLastPageLoadEncounterSSLErrors() const override;
  bool IsOffTheRecord() const override;
  password_manager::PasswordManager* GetPasswordManager() override;
  autofill::AutofillManager* GetAutofillManagerForMainFrame() override;
  const GURL& GetMainFrameURL() const override;
  bool IsUpdatePasswordUIEnabled() const override;
  const GURL& GetLastCommittedEntryURL() const override;
  scoped_ptr<password_manager::CredentialsFilter> CreateStoreResultFilter()
      const override;

  // Hides any visible generation UI.
  void HidePasswordGenerationPopup();

  static void CreateForWebContentsWithAutofillClient(
      content::WebContents* contents,
      autofill::AutofillClient* autofill_client);

  // Observer for PasswordGenerationPopup events. Used for testing.
  void SetTestObserver(autofill::PasswordGenerationPopupObserver* observer);

  // Returns true if the bubble UI is enabled, and false if we're still using
  // the sad old Infobar UI.
  static bool IsTheHotNewBubbleUIEnabled();

  // Returns true if the password manager should be enabled during sync signin.
  static bool EnabledForSyncSignin();

 protected:
  // Callable for tests.
  ChromePasswordManagerClient(content::WebContents* web_contents,
                              autofill::AutofillClient* autofill_client);

 private:
  friend class content::WebContentsUserData<ChromePasswordManagerClient>;

  // content::WebContentsObserver overrides.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  // Given |bounds| in the renderers coordinate system, return the same bounds
  // in the screens coordinate system.
  gfx::RectF GetBoundsInScreenSpace(const gfx::RectF& bounds);

  // Causes the password generation UI to be shown for the specified form.
  // The popup will be anchored at |element_bounds|. The generated password
  // will be no longer than |max_length|.
  void ShowPasswordGenerationPopup(content::RenderFrameHost* render_frame_host,
                                   const gfx::RectF& bounds,
                                   int max_length,
                                   const autofill::PasswordForm& form);

  // Causes the password editing UI to be shown anchored at |element_bounds|.
  void ShowPasswordEditingPopup(content::RenderFrameHost* render_frame_host,
                                const gfx::RectF& bounds,
                                const autofill::PasswordForm& form);

  // Notify the PasswordManager that generation is available for |form|. Used
  // for UMA stats.
  void GenerationAvailableForForm(const autofill::PasswordForm& form);

  // Sends a message to the renderer with the current value of
  // |can_use_log_router_|.
  void NotifyRendererOfLoggingAvailability();

  // Returns true if |url| is the reauth page for accessing the password
  // website.
  bool IsURLPasswordWebsiteReauth(const GURL& url) const;

  Profile* const profile_;

  password_manager::PasswordManager password_manager_;

  password_manager::ContentPasswordManagerDriverFactory* driver_factory_;

  password_manager::CredentialManagerDispatcher
      credential_manager_dispatcher_;

  // Observer for password generation popup.
  autofill::PasswordGenerationPopupObserver* observer_;

  // Controls the popup
  base::WeakPtr<
    autofill::PasswordGenerationPopupControllerImpl> popup_controller_;

  // True if |this| is registered with some LogRouter which can accept logs.
  bool can_use_log_router_;

  // Set to false to disable password saving (will no longer ask if you
  // want to save passwords but will continue to fill passwords).
  BooleanPrefMember saving_passwords_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ChromePasswordManagerClient);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
