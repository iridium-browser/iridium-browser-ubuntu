// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/keychain_migration_status_mac.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

#if defined(OS_WIN)
#include "base/prefs/pref_registry_simple.h"
#endif

using autofill::PasswordForm;
using autofill::PasswordFormMap;

namespace password_manager {

namespace {

const char kSpdyProxyRealm[] = "/SpdyProxy";

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

bool ShouldDropSyncCredential() {
  std::string group_name =
      base::FieldTrialList::FindFullName("PasswordManagerDropSyncCredential");

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableDropSyncCredential))
    return true;

  if (command_line->HasSwitch(switches::kDisableDropSyncCredential))
    return false;

  // Default to not saving.
  return group_name != "Disabled";
}

bool URLsEqualUpToScheme(const GURL& a, const GURL& b) {
  return (a.GetContent() == b.GetContent());
}

bool URLsEqualUpToHttpHttpsSubstitution(const GURL& a, const GURL& b) {
  if (a == b)
    return true;

  // The first-time and retry login forms action URLs sometimes differ in
  // switching from HTTP to HTTPS, see http://crbug.com/400769.
  if (a.SchemeIsHTTPOrHTTPS() && b.SchemeIsHTTPOrHTTPS())
    return URLsEqualUpToScheme(a, b);

  return false;
}

// Helper UMA reporting function for differences in URLs during form submission.
void RecordWhetherTargetDomainDiffers(const GURL& src, const GURL& target) {
  bool target_domain_differs =
      !net::registry_controlled_domains::SameDomainOrHost(
          src, target,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.SubmitNavigatesToDifferentDomain",
                        target_domain_differs);
}

bool IsSignupForm(const PasswordForm& form) {
  return !form.new_password_element.empty() && form.password_element.empty();
}

// Tries to convert the |server_field_type| to a PasswordFormFieldPredictionType
// stored in |type|. Returns true if the conversion was made.
bool ServerTypeToPrediction(autofill::ServerFieldType server_field_type,
                            autofill::PasswordFormFieldPredictionType* type) {
  switch (server_field_type) {
    case autofill::USERNAME:
    case autofill::USERNAME_AND_EMAIL_ADDRESS:
      *type = autofill::PREDICTION_USERNAME;
      return true;

    case autofill::PASSWORD:
      *type = autofill::PREDICTION_CURRENT_PASSWORD;
      return true;

    case autofill::ACCOUNT_CREATION_PASSWORD:
      *type = autofill::PREDICTION_NEW_PASSWORD;
      return true;

    default:
      return false;
  }
}

// Returns true if the |field_type| is known to be possibly
// misinterpreted as a password by the Password Manager.
bool IsPredictedTypeNotPasswordPrediction(
    autofill::ServerFieldType field_type) {
  return field_type == autofill::CREDIT_CARD_NUMBER ||
         field_type == autofill::CREDIT_CARD_VERIFICATION_CODE;
}

bool PreferredRealmIsFromAndroid(
    const autofill::PasswordFormFillData& fill_data) {
  return FacetURI::FromPotentiallyInvalidSpec(
             fill_data.preferred_realm).IsValidAndroidFacetURI();
}

bool ContainsAndroidCredentials(
    const autofill::PasswordFormFillData& fill_data) {
  for (const auto& login : fill_data.additional_logins) {
    if (FacetURI::FromPotentiallyInvalidSpec(
            login.second.realm).IsValidAndroidFacetURI()) {
      return true;
    }
  }

  return PreferredRealmIsFromAndroid(fill_data);
}

}  // namespace

// static
void PasswordManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kPasswordManagerSavingEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kPasswordManagerAutoSignin, true);
  registry->RegisterBooleanPref(prefs::kPasswordManagerAllowShowPasswords,
                                true);
  registry->RegisterListPref(prefs::kPasswordManagerGroupsForDomains);
#if defined(OS_MACOSX)
  registry->RegisterIntegerPref(prefs::kKeychainMigrationStatus,
                                static_cast<int>(MigrationStatus::NOT_STARTED));
#endif
}

#if defined(OS_WIN)
// static
void PasswordManager::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kOsPasswordLastChanged, 0);
  registry->RegisterBooleanPref(prefs::kOsPasswordBlank, false);
}
#endif

PasswordManager::PasswordManager(PasswordManagerClient* client)
    : client_(client) {
  DCHECK(client_);
}

PasswordManager::~PasswordManager() {
  FOR_EACH_OBSERVER(LoginModelObserver, observers_, OnLoginModelDestroying());
}

void PasswordManager::GenerationAvailableForForm(const PasswordForm& form) {
  DCHECK(client_->IsSavingEnabledForCurrentPage());

  PasswordFormManager* form_manager = GetMatchingPendingManager(form);
  if (form_manager) {
    form_manager->MarkGenerationAvailable();
    return;
  }
}

void PasswordManager::SetHasGeneratedPasswordForForm(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& form,
    bool password_is_generated) {
  DCHECK(client_->IsSavingEnabledForCurrentPage());

  PasswordFormManager* form_manager = GetMatchingPendingManager(form);
  if (form_manager) {
    form_manager->set_has_generated_password(password_is_generated);
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.GeneratedFormHasNoFormManager",
                        password_is_generated);

  if (!password_is_generated)
    return;

  // If there is no corresponding PasswordFormManager, we create one. This is
  // not the common case, and should only happen when there is a bug in our
  // ability to detect forms.
  bool ssl_valid = form.origin.SchemeIsCryptographic();
  PasswordFormManager* manager = new PasswordFormManager(
      this, client_, driver->AsWeakPtr(), form, ssl_valid);
  pending_login_managers_.push_back(manager);
  manager->set_has_generated_password(true);
}

void PasswordManager::ProvisionallySavePassword(const PasswordForm& form) {
  bool is_saving_enabled = client_->IsSavingEnabledForCurrentPage();

  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_PROVISIONALLY_SAVE_PASSWORD_METHOD);
    logger->LogPasswordForm(Logger::STRING_PROVISIONALLY_SAVE_PASSWORD_FORM,
                            form);
  }

  if (!is_saving_enabled) {
    RecordFailure(SAVING_DISABLED, form.origin, logger.get());
    return;
  }

  // No password to save? Then don't.
  if (PasswordFormManager::PasswordToSave(form).empty()) {
    RecordFailure(EMPTY_PASSWORD, form.origin, logger.get());
    return;
  }

  scoped_ptr<PasswordFormManager> manager;
  ScopedVector<PasswordFormManager>::iterator matched_manager_it =
      pending_login_managers_.end();
  PasswordFormManager::MatchResultMask current_match_result =
      PasswordFormManager::RESULT_NO_MATCH;
  // Below, "matching" is in DoesManage-sense and "not ready" in
  // !HasCompletedMatching sense. We keep track of such PasswordFormManager
  // instances for UMA.
  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end(); ++iter) {
    PasswordFormManager::MatchResultMask result = (*iter)->DoesManage(form);

    if (result == PasswordFormManager::RESULT_NO_MATCH)
      continue;

    (*iter)->SetSubmittedForm(form);

    if ((*iter)->is_ignorable_change_password_form()) {
      if (logger)
        logger->LogMessage(Logger::STRING_CHANGE_PASSWORD_FORM);
      continue;
    }

    if (result == PasswordFormManager::RESULT_COMPLETE_MATCH) {
      // If we find a manager that exactly matches the submitted form including
      // the action URL, exit the loop.
      if (logger)
        logger->LogMessage(Logger::STRING_EXACT_MATCH);
      matched_manager_it = iter;
      break;
    } else if (result == (PasswordFormManager::RESULT_COMPLETE_MATCH &
                          ~PasswordFormManager::RESULT_ACTION_MATCH) &&
               result > current_match_result) {
      // If the current manager matches the submitted form excluding the action
      // URL, remember it as a candidate and continue searching for an exact
      // match. See http://crbug.com/27246 for an example where actions can
      // change.
      if (logger)
        logger->LogMessage(Logger::STRING_MATCH_WITHOUT_ACTION);
      matched_manager_it = iter;
      current_match_result = result;
    } else if (IsSignupForm(form) && result > current_match_result) {
      // Signup forms don't require HTML attributes to match because we don't
      // need to fill these saved passwords on the same form in the future.
      // Prefer the best possible match (e.g. action and origins match instead
      // or just origin matching). Don't break in case there exists a better
      // match.
      // TODO(gcasto): Matching in this way is very imprecise. Having some
      // better way to match the same form when the HTML elements change (e.g.
      // text element changed to password element) would be useful.
      if (logger)
        logger->LogMessage(Logger::STRING_ORIGINS_MATCH);
      matched_manager_it = iter;
      current_match_result = result;
    }
  }
  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (matched_manager_it != pending_login_managers_.end()) {
    // Transfer ownership of the manager from |pending_login_managers_| to
    // |manager|.
    manager.reset(*matched_manager_it);
    pending_login_managers_.weak_erase(matched_manager_it);
  } else {
    RecordFailure(NO_MATCHING_FORM, form.origin, logger.get());
    return;
  }

  // Bail if we're missing any of the necessary form components.
  if (!manager->HasValidPasswordForm()) {
    RecordFailure(INVALID_FORM, form.origin, logger.get());
    return;
  }

  PasswordForm provisionally_saved_form(form);
  provisionally_saved_form.ssl_valid =
      form.origin.SchemeIsCryptographic() &&
      !client_->DidLastPageLoadEncounterSSLErrors();
  provisionally_saved_form.preferred = true;
  if (logger) {
    logger->LogPasswordForm(Logger::STRING_PROVISIONALLY_SAVED_FORM,
                            provisionally_saved_form);
  }
  PasswordFormManager::OtherPossibleUsernamesAction action =
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES;
  if (OtherPossibleUsernamesEnabled())
    action = PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES;
  if (logger) {
    logger->LogBoolean(
        Logger::STRING_IGNORE_POSSIBLE_USERNAMES,
        action == PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  }
  manager->ProvisionallySave(provisionally_saved_form, action);
  provisional_save_manager_.swap(manager);

  // Cache the user-visible URL (i.e., the one seen in the omnibox). Once the
  // post-submit navigation concludes, we compare the landing URL against the
  // cached and report the difference through UMA.
  main_frame_url_ = client_->GetMainFrameURL();
}

void PasswordManager::UpdateFormManagers() {
  for (PasswordFormManager* form_manager : pending_login_managers_) {
    form_manager->FetchMatchingLoginsFromPasswordStore(
        client_->GetAuthorizationPromptPolicy(form_manager->observed_form()));
  }
}

void PasswordManager::RecordFailure(ProvisionalSaveFailure failure,
                                    const GURL& form_origin,
                                    BrowserSavePasswordProgressLogger* logger) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ProvisionalSaveFailure", failure, MAX_FAILURE_VALUE);

  std::string group_name =
      metrics_util::GroupIdToString(metrics_util::MonitoredDomainGroupId(
          form_origin.host(), client_->GetPrefs()));
  if (!group_name.empty()) {
    metrics_util::LogUMAHistogramEnumeration(
        "PasswordManager.ProvisionalSaveFailure_" + group_name,
        failure,
        MAX_FAILURE_VALUE);
  }

  if (logger) {
    switch (failure) {
      case SAVING_DISABLED:
        logger->LogMessage(Logger::STRING_SAVING_DISABLED);
        break;
      case EMPTY_PASSWORD:
        logger->LogMessage(Logger::STRING_EMPTY_PASSWORD);
        break;
      case MATCHING_NOT_COMPLETE:
        logger->LogMessage(Logger::STRING_MATCHING_NOT_COMPLETE);
        break;
      case NO_MATCHING_FORM:
        logger->LogMessage(Logger::STRING_NO_MATCHING_FORM);
        break;
      case FORM_BLACKLISTED:
        logger->LogMessage(Logger::STRING_FORM_BLACKLISTED);
        break;
      case INVALID_FORM:
        logger->LogMessage(Logger::STRING_INVALID_FORM);
        break;
      case SYNC_CREDENTIAL:
        logger->LogMessage(Logger::STRING_SYNC_CREDENTIAL);
        break;
      case MAX_FAILURE_VALUE:
        NOTREACHED();
        return;
    }
    logger->LogMessage(Logger::STRING_DECISION_DROP);
  }
}

void PasswordManager::AddSubmissionCallback(
    const PasswordSubmittedCallback& callback) {
  submission_callbacks_.push_back(callback);
}

void PasswordManager::AddObserver(LoginModelObserver* observer) {
  observers_.AddObserver(observer);
}

void PasswordManager::RemoveObserver(LoginModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PasswordManager::DidNavigateMainFrame() {
  pending_login_managers_.clear();
}

void PasswordManager::OnPasswordFormSubmitted(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  ProvisionallySavePassword(password_form);
  for (size_t i = 0; i < submission_callbacks_.size(); ++i) {
    submission_callbacks_[i].Run(password_form);
  }

  pending_login_managers_.clear();
}

void PasswordManager::OnPasswordFormForceSaveRequested(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  // TODO(msramek): This is just a sketch. We will need to show a custom bubble,
  // mark the form as force saved, and recreate the pending login managers,
  // because the password store might have changed.
  ProvisionallySavePassword(password_form);
  if (provisional_save_manager_)
    OnLoginSuccessful();
}

void PasswordManager::OnPasswordFormsParsed(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& forms) {
  CreatePendingLoginManagers(driver, forms);
}

void PasswordManager::CreatePendingLoginManagers(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& forms) {
  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_CREATE_LOGIN_MANAGERS_METHOD);
  }

  if (client_->DidLastPageLoadEncounterSSLErrors() ||
      !client_->IsPasswordManagementEnabledForCurrentPage())
    return;

  if (logger) {
    logger->LogNumber(Logger::STRING_OLD_NUMBER_LOGIN_MANAGERS,
                      pending_login_managers_.size());
  }

  // Copy the weak pointers to the currently known login managers for comparison
  // against the newly added.
  std::vector<PasswordFormManager*> old_login_managers(
      pending_login_managers_.get());
  for (std::vector<PasswordForm>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    // Don't involve the password manager if this form corresponds to
    // SpdyProxy authentication, as indicated by the realm.
    if (base::EndsWith(iter->signon_realm, kSpdyProxyRealm,
                       base::CompareCase::SENSITIVE))
      continue;
    bool old_manager_found = false;
    for (const auto& old_manager : old_login_managers) {
      if (old_manager->DoesManage(*iter) !=
          PasswordFormManager::RESULT_COMPLETE_MATCH) {
        continue;
      }
      old_manager_found = true;
      if (old_manager->HasCompletedMatching())
        old_manager->ProcessFrame(driver->AsWeakPtr());
      break;
    }
    if (old_manager_found)
      continue;  // The current form is already managed.

    UMA_HISTOGRAM_BOOLEAN("PasswordManager.EmptyUsernames.ParsedUsernameField",
                          iter->username_element.empty());

    // Out of the forms not containing a username field, determine how many
    // are password change forms.
    if (iter->username_element.empty()) {
      UMA_HISTOGRAM_BOOLEAN(
          "PasswordManager.EmptyUsernames."
          "FormWithoutUsernameFieldIsPasswordChangeForm",
          !iter->new_password_element.empty());
    }

    if (logger)
      logger->LogFormSignatures(Logger::STRING_ADDING_SIGNATURE, *iter);
    bool ssl_valid = iter->origin.SchemeIsCryptographic();
    PasswordFormManager* manager = new PasswordFormManager(
        this, client_, driver->AsWeakPtr(), *iter, ssl_valid);
    pending_login_managers_.push_back(manager);

    PasswordStore::AuthorizationPromptPolicy prompt_policy =
        client_->GetAuthorizationPromptPolicy(*iter);

    manager->FetchMatchingLoginsFromPasswordStore(prompt_policy);
  }

  if (logger) {
    logger->LogNumber(Logger::STRING_NEW_NUMBER_LOGIN_MANAGERS,
                      pending_login_managers_.size());
  }
}

bool PasswordManager::CanProvisionalManagerSave() {
  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_CAN_PROVISIONAL_MANAGER_SAVE_METHOD);
  }

  if (!provisional_save_manager_) {
    if (logger) {
      logger->LogMessage(Logger::STRING_NO_PROVISIONAL_SAVE_MANAGER);
    }
    return false;
  }

  if (!provisional_save_manager_->HasCompletedMatching()) {
    // We have a provisional save manager, but it didn't finish matching yet.
    // We just give up.
    RecordFailure(MATCHING_NOT_COMPLETE,
                  provisional_save_manager_->observed_form().origin,
                  logger.get());
    provisional_save_manager_.reset();
    return false;
  }
  return true;
}

bool PasswordManager::ShouldPromptUserToSavePassword() const {
  return !client_->IsAutomaticPasswordSavingEnabled() &&
         (provisional_save_manager_->IsNewLogin() ||
          provisional_save_manager_
              ->is_possible_change_password_form_without_username() ||
          (provisional_save_manager_->password_overridden() &&
           client_->IsUpdatePasswordUIEnabled())) &&
         !provisional_save_manager_->has_generated_password() &&
         !provisional_save_manager_->IsPendingCredentialsPublicSuffixMatch();
}

void PasswordManager::OnPasswordFormsRendered(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<PasswordForm>& visible_forms,
    bool did_stop_loading) {
  CreatePendingLoginManagers(driver, visible_forms);
  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_ON_PASSWORD_FORMS_RENDERED_METHOD);
  }

  if (!CanProvisionalManagerSave())
    return;

  // If the server throws an internal error, access denied page, page not
  // found etc. after a login attempt, we do not save the credentials.
  if (client_->WasLastNavigationHTTPError()) {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_DROP);
    provisional_save_manager_->LogSubmitFailed();
    provisional_save_manager_.reset();
    return;
  }

  if (logger) {
    logger->LogNumber(Logger::STRING_NUMBER_OF_VISIBLE_FORMS,
                      visible_forms.size());
  }

  // Record all visible forms from the frame.
  all_visible_forms_.insert(all_visible_forms_.end(),
                            visible_forms.begin(),
                            visible_forms.end());

  // If we see the login form again, then the login failed.
  if (did_stop_loading) {
    if (provisional_save_manager_->pending_credentials().scheme ==
        PasswordForm::SCHEME_HTML) {
      for (size_t i = 0; i < all_visible_forms_.size(); ++i) {
        // TODO(vabr): The similarity check is just action equality up to
        // HTTP<->HTTPS substitution for now. If it becomes more complex, it may
        // make sense to consider modifying and using
        // PasswordFormManager::DoesManage for it.
        if (all_visible_forms_[i].action.is_valid() &&
            URLsEqualUpToHttpHttpsSubstitution(
                provisional_save_manager_->pending_credentials().action,
                all_visible_forms_[i].action)) {
          provisional_save_manager_->LogSubmitFailed();
          // Generated passwords should always be saved, but we do want to
          // record that the submission normally would have failed for UMA.
          if (!provisional_save_manager_->has_generated_password()) {
            if (logger) {
              logger->LogPasswordForm(Logger::STRING_PASSWORD_FORM_REAPPEARED,
                                      visible_forms[i]);
              logger->LogMessage(Logger::STRING_DECISION_DROP);
            }
            provisional_save_manager_.reset();
            // Clear all_visible_forms_ once we found the match.
            all_visible_forms_.clear();
            return;
          }
        }
      }
    } else {
      if (logger)
        logger->LogMessage(Logger::STRING_PROVISIONALLY_SAVED_FORM_IS_NOT_HTML);
    }

    // Clear all_visible_forms_ after checking all the visible forms.
    all_visible_forms_.clear();

    // Looks like a successful login attempt. Either show an infobar or
    // automatically save the login data. We prompt when the user hasn't
    // already given consent, either through previously accepting the infobar
    // or by having the browser generate the password.
    OnLoginSuccessful();
  }
}

void PasswordManager::OnInPageNavigation(
    password_manager::PasswordManagerDriver* driver,
    const PasswordForm& password_form) {
  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_ON_IN_PAGE_NAVIGATION);
  }

  ProvisionallySavePassword(password_form);

  if (!CanProvisionalManagerSave())
    return;

  OnLoginSuccessful();
}

void PasswordManager::OnLoginSuccessful() {
  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_ON_ASK_USER_OR_SAVE_PASSWORD);
  }

  if (ShouldDropSyncCredential() &&
      client_->IsSyncAccountCredential(
          base::UTF16ToUTF8(
              provisional_save_manager_->pending_credentials().username_value),
          provisional_save_manager_->pending_credentials().signon_realm)) {
    provisional_save_manager_->WipeStoreCopyIfOutdated();
    RecordFailure(SYNC_CREDENTIAL,
                  provisional_save_manager_->observed_form().origin,
                  logger.get());
    provisional_save_manager_.reset();
    return;
  }

  provisional_save_manager_->LogSubmitPassed();

  RecordWhetherTargetDomainDiffers(main_frame_url_, client_->GetMainFrameURL());

  if (ShouldPromptUserToSavePassword()) {
    bool empty_password =
        provisional_save_manager_->pending_credentials().username_value.empty();
    UMA_HISTOGRAM_BOOLEAN("PasswordManager.EmptyUsernames.OfferedToSave",
                          empty_password);
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_ASK);
    bool update_password =
        (!provisional_save_manager_->best_matches().empty() &&
         provisional_save_manager_
             ->is_possible_change_password_form_without_username()) ||
        provisional_save_manager_->password_overridden();
    if (client_->PromptUserToSaveOrUpdatePassword(
            provisional_save_manager_.Pass(),
            CredentialSourceType::CREDENTIAL_SOURCE_PASSWORD_MANAGER,
            update_password)) {
      if (logger)
        logger->LogMessage(Logger::STRING_SHOW_PASSWORD_PROMPT);
    }
  } else {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_SAVE);
    provisional_save_manager_->Save();

    if (provisional_save_manager_->has_generated_password()) {
      client_->AutomaticPasswordSave(provisional_save_manager_.Pass());
    } else {
      provisional_save_manager_.reset();
    }
  }
}

bool PasswordManager::OtherPossibleUsernamesEnabled() const {
  return false;
}

void PasswordManager::Autofill(password_manager::PasswordManagerDriver* driver,
                               const PasswordForm& form_for_autofill,
                               const PasswordFormMap& best_matches,
                               const PasswordForm& preferred_match,
                               bool wait_for_username) const {
  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_PASSWORDMANAGER_AUTOFILL);
  }
  switch (form_for_autofill.scheme) {
    case PasswordForm::SCHEME_HTML: {
      // Note the check above is required because the observers_ for a non-HTML
      // schemed password form may have been freed, so we need to distinguish.
      autofill::PasswordFormFillData fill_data;
      InitPasswordFormFillData(form_for_autofill,
                               best_matches,
                               &preferred_match,
                               wait_for_username,
                               OtherPossibleUsernamesEnabled(),
                               &fill_data);
      if (logger)
        logger->LogBoolean(Logger::STRING_WAIT_FOR_USERNAME, wait_for_username);
      UMA_HISTOGRAM_BOOLEAN(
          "PasswordManager.FillSuggestionsIncludeAndroidAppCredentials",
          ContainsAndroidCredentials(fill_data));
      metrics_util::LogFilledCredentialIsFromAndroidApp(
          PreferredRealmIsFromAndroid(fill_data));
      driver->FillPasswordForm(fill_data);
      break;
    }
    default:
      if (logger) {
        logger->LogBoolean(Logger::STRING_LOGINMODELOBSERVER_PRESENT,
                           observers_.might_have_observers());
      }
      FOR_EACH_OBSERVER(
          LoginModelObserver,
          observers_,
          OnAutofillDataAvailable(preferred_match.username_value,
                                  preferred_match.password_value));
      break;
  }

  client_->PasswordWasAutofilled(best_matches);
}

void PasswordManager::ProcessAutofillPredictions(
    password_manager::PasswordManagerDriver* driver,
    const std::vector<autofill::FormStructure*>& forms) {
  // Leave only forms that contain fields that are useful for password manager.
  std::map<autofill::FormData, autofill::PasswordFormFieldPredictionMap>
      predictions;
  for (autofill::FormStructure* form : forms) {
    for (std::vector<autofill::AutofillField*>::const_iterator field =
             form->begin();
         field != form->end(); ++field) {
      autofill::PasswordFormFieldPredictionType prediction_type;
      if (ServerTypeToPrediction((*field)->server_type(), &prediction_type)) {
        predictions[form->ToFormData()][*(*field)] = prediction_type;
      }
      // Certain fields are annotated by the browsers as "not passwords" i.e.
      // they should not be treated as passwords by the Password Manager.
      if ((*field)->form_control_type == "password" &&
          IsPredictedTypeNotPasswordPrediction(
              (*field)->Type().GetStorableType())) {
        predictions[form->ToFormData()][*(*field)] =
            autofill::PREDICTION_NOT_PASSWORD;
      }
    }
  }
  if (predictions.empty())
    return;
  driver->AutofillDataReceived(predictions);
}

PasswordFormManager* PasswordManager::GetMatchingPendingManager(
    const PasswordForm& form) {
  auto matched_manager_it = pending_login_managers_.end();
  PasswordFormManager::MatchResultMask current_match_result =
      PasswordFormManager::RESULT_NO_MATCH;

  for (auto iter = pending_login_managers_.begin();
       iter != pending_login_managers_.end(); ++iter) {
    PasswordFormManager::MatchResultMask result = (*iter)->DoesManage(form);

    if (result == PasswordFormManager::RESULT_NO_MATCH)
      continue;

    if (result == PasswordFormManager::RESULT_COMPLETE_MATCH) {
      // If we find a manager that exactly matches the submitted form including
      // the action URL, exit the loop.
      matched_manager_it = iter;
      break;
    } else if (result == (PasswordFormManager::RESULT_COMPLETE_MATCH &
                          ~PasswordFormManager::RESULT_ACTION_MATCH) &&
               result > current_match_result) {
      // If the current manager matches the submitted form excluding the action
      // URL, remember it as a candidate and continue searching for an exact
      // match. See http://crbug.com/27246 for an example where actions can
      // change.
      matched_manager_it = iter;
      current_match_result = result;
    } else if (result > current_match_result) {
      matched_manager_it = iter;
      current_match_result = result;
    }
  }
  if (matched_manager_it != pending_login_managers_.end())
    return *matched_manager_it;
  return nullptr;
}

}  // namespace password_manager
