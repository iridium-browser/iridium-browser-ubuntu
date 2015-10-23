// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <algorithm>
#include <set>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "google_apis/gaia/gaia_auth_util.h"

using autofill::FormStructure;
using autofill::PasswordForm;
using autofill::PasswordFormMap;
using base::Time;

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace password_manager {

namespace {

PasswordForm CopyAndModifySSLValidity(const PasswordForm& orig,
                                      bool ssl_valid) {
  PasswordForm result(orig);
  result.ssl_valid = ssl_valid;
  return result;
}

// Returns true if user-typed username and password field values match with one
// of the password form within |credentials| map; otherwise false.
bool DoesUsenameAndPasswordMatchCredentials(
    const base::string16& typed_username,
    const base::string16& typed_password,
    const autofill::PasswordFormMap& credentials) {
  for (auto match : credentials) {
    if (match.second->username_value == typed_username &&
        match.second->password_value == typed_password)
      return true;
  }
  return false;
}

std::vector<std::string> SplitPathToSegments(const std::string& path) {
  return base::SplitString(path, "/", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

// Return false iff the strings are neither empty nor equal.
bool AreStringsEqualOrEmpty(const base::string16& s1,
                            const base::string16& s2) {
  return s1.empty() || s2.empty() || s1 == s2;
}

bool DoesStringContainOnlyDigits(const base::string16& s) {
  for (auto c : s) {
    if (!base::IsAsciiDigit(c))
      return false;
  }
  return true;
}

// Heuristics to determine that a string is very unlikely to be a username.
bool IsProbablyNotUsername(const base::string16& s) {
  return !s.empty() && DoesStringContainOnlyDigits(s) && s.size() < 3;
}

}  // namespace

PasswordFormManager::PasswordFormManager(
    PasswordManager* password_manager,
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const PasswordForm& observed_form,
    bool ssl_valid)
    : observed_form_(CopyAndModifySSLValidity(observed_form, ssl_valid)),
      provisionally_saved_form_(nullptr),
      other_possible_username_action_(
          PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES),
      form_path_segments_(
          observed_form_.origin.is_valid()
              ? SplitPathToSegments(observed_form_.origin.path())
              : std::vector<std::string>()),
      is_new_login_(true),
      has_generated_password_(false),
      password_overridden_(false),
      generation_available_(false),
      password_manager_(password_manager),
      preferred_match_(nullptr),
      is_ignorable_change_password_form_(false),
      is_possible_change_password_form_without_username_(
          observed_form.IsPossibleChangePasswordFormWithoutUsername()),
      state_(PRE_MATCHING_PHASE),
      client_(client),
      manager_action_(kManagerActionNone),
      user_action_(kUserActionNone),
      submit_result_(kSubmitResultNotSubmitted),
      form_type_(kFormTypeUnspecified) {
  drivers_.push_back(driver);
}

PasswordFormManager::~PasswordFormManager() {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ActionsTakenV3", GetActionsTaken(), kMaxNumActionsTaken);
  if (submit_result_ == kSubmitResultNotSubmitted) {
    if (has_generated_password_)
      metrics_util::LogPasswordGenerationSubmissionEvent(
          metrics_util::PASSWORD_NOT_SUBMITTED);
    else if (generation_available_)
      metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
          metrics_util::PASSWORD_NOT_SUBMITTED);
  }
  if (form_type_ != kFormTypeUnspecified) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.SubmittedFormType", form_type_,
                              kFormTypeMax);
  }
}

int PasswordFormManager::GetActionsTaken() const {
  return user_action_ +
         kUserActionMax *
             (manager_action_ + kManagerActionMax * submit_result_);
}

// static
base::string16 PasswordFormManager::PasswordToSave(const PasswordForm& form) {
  if (form.new_password_element.empty() || form.new_password_value.empty())
    return form.password_value;
  return form.new_password_value;
}

// TODO(timsteele): use a hash of some sort in the future?
PasswordFormManager::MatchResultMask PasswordFormManager::DoesManage(
    const PasswordForm& form) const {
  // Non-HTML form case.
  if (observed_form_.scheme != PasswordForm::SCHEME_HTML ||
      form.scheme != PasswordForm::SCHEME_HTML) {
    const bool forms_match = observed_form_.signon_realm == form.signon_realm &&
                             observed_form_.scheme == form.scheme;
    return forms_match ? RESULT_COMPLETE_MATCH : RESULT_NO_MATCH;
  }

  // HTML form case.
  MatchResultMask result = RESULT_NO_MATCH;

  // Easiest case of matching origins.
  bool origins_match = form.origin == observed_form_.origin;
  // If this is a replay of the same form in the case a user entered an invalid
  // password, the origin of the new form may equal the action of the "first"
  // form instead.
  origins_match = origins_match || (form.origin == observed_form_.action);
  // Otherwise, if action hosts are the same, the old URL scheme is HTTP while
  // the new one is HTTPS, and the new path equals to or extends the old path,
  // we also consider the actions a match. This is to accommodate cases where
  // the original login form is on an HTTP page, but a failed login attempt
  // redirects to HTTPS (as in http://example.org -> https://example.org/auth).
  if (!origins_match && !observed_form_.origin.SchemeIsCryptographic() &&
      form.origin.SchemeIsCryptographic()) {
    const std::string& old_path = observed_form_.origin.path();
    const std::string& new_path = form.origin.path();
    origins_match =
        observed_form_.origin.host() == form.origin.host() &&
        observed_form_.origin.port() == form.origin.port() &&
        base::StartsWith(new_path, old_path, base::CompareCase::SENSITIVE);
  }

  if (!origins_match)
    return result;

  result |= RESULT_ORIGINS_MATCH;

  // Autofill predictions can overwrite our default username selection so
  // if this form was parsed with autofill predictions then allow the username
  // element to be different.
  if ((form.was_parsed_using_autofill_predictions ||
       form.username_element == observed_form_.username_element) &&
      form.password_element == observed_form_.password_element) {
    result |= RESULT_HTML_ATTRIBUTES_MATCH;
  }

  // Note: although saved password forms might actually have an empty action
  // URL if they were imported (see bug 1107719), the |form| we see here comes
  // never from the password store, and should have an exactly matching action.
  if (form.action == observed_form_.action)
    result |= RESULT_ACTION_MATCH;

  return result;
}

bool PasswordFormManager::IsBlacklisted() const {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  return !blacklisted_matches_.empty();
}

void PasswordFormManager::PermanentlyBlacklist() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(!client_->IsOffTheRecord());

  // Configure the form about to be saved for blacklist status.
  blacklisted_matches_.push_back(
      new autofill::PasswordForm(pending_credentials_));
  blacklisted_matches_.back()->preferred = false;
  blacklisted_matches_.back()->blacklisted_by_user = true;
  blacklisted_matches_.back()->username_value.clear();
  blacklisted_matches_.back()->password_value.clear();
  blacklisted_matches_.back()->other_possible_usernames.clear();
  blacklisted_matches_.back()->date_created = Time::Now();

  PasswordStore* password_store = client_->GetPasswordStore();
  DCHECK(password_store);
  password_store->AddLogin(*blacklisted_matches_.back());
}

bool PasswordFormManager::IsNewLogin() const {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  return is_new_login_;
}

bool PasswordFormManager::IsPendingCredentialsPublicSuffixMatch() const {
  return pending_credentials_.IsPublicSuffixMatch();
}

bool PasswordFormManager::HasValidPasswordForm() const {
  DCHECK(state_ == MATCHING_PHASE || state_ == POST_MATCHING_PHASE) << state_;
  // Non-HTML password forms (primarily HTTP and FTP autentication)
  // do not contain username_element and password_element values.
  if (observed_form_.scheme != PasswordForm::SCHEME_HTML)
    return true;
  return !observed_form_.password_element.empty() ||
         !observed_form_.new_password_element.empty();
}

void PasswordFormManager::ProvisionallySave(
    const PasswordForm& credentials,
    OtherPossibleUsernamesAction action) {
  DCHECK(state_ == MATCHING_PHASE || state_ == POST_MATCHING_PHASE) << state_;
  DCHECK_NE(RESULT_NO_MATCH, DoesManage(credentials));

  scoped_ptr<autofill::PasswordForm> mutable_provisionally_saved_form(
      new PasswordForm(credentials));
  if (credentials.IsPossibleChangePasswordForm() &&
      !credentials.username_value.empty() &&
      IsProbablyNotUsername(credentials.username_value)) {
    mutable_provisionally_saved_form->username_value.clear();
    mutable_provisionally_saved_form->username_element.clear();
    is_possible_change_password_form_without_username_ = true;
  }
  provisionally_saved_form_ = mutable_provisionally_saved_form.Pass();
  other_possible_username_action_ = action;

  if (HasCompletedMatching())
    CreatePendingCredentials();
}

void PasswordFormManager::Save() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(!client_->IsOffTheRecord());

  if (IsNewLogin()) {
    SaveAsNewLogin();
    DeleteEmptyUsernameCredentials();
  } else {
    UpdateLogin();
  }

  // This is not in UpdateLogin() to catch PSL matched credentials.
  if (pending_credentials_.times_used != 0 &&
      pending_credentials_.type == PasswordForm::TYPE_GENERATED) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_USED);
  }

  password_manager_->UpdateFormManagers();
}

void PasswordFormManager::Update(
    const autofill::PasswordForm& credentials_to_update) {
  base::string16 password_to_save = pending_credentials_.password_value;
  pending_credentials_ = credentials_to_update;
  pending_credentials_.password_value = password_to_save;
  pending_credentials_.preferred = true;
  is_new_login_ = false;
  UpdateLogin();
}

void PasswordFormManager::FetchMatchingLoginsFromPasswordStore(
    PasswordStore::AuthorizationPromptPolicy prompt_policy) {
  if (state_ == MATCHING_PHASE) {
    // There is currently a password store query in progress. Remember the
    // prompt policy for when the store results are back and another store query
    // might be issued.
    next_prompt_policy_.reset(
        new PasswordStore::AuthorizationPromptPolicy(prompt_policy));
    return;
  }

  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_FETCH_LOGINS_METHOD);
    logger->LogNumber(Logger::STRING_FORM_MANAGER_STATE, state_);
  }

  state_ = MATCHING_PHASE;

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    if (logger)
      logger->LogMessage(Logger::STRING_NO_STORE);
    NOTREACHED();
    return;
  }
  password_store->GetLogins(observed_form_, prompt_policy, this);
}

bool PasswordFormManager::HasCompletedMatching() const {
  return state_ == POST_MATCHING_PHASE;
}

void PasswordFormManager::SetSubmittedForm(const autofill::PasswordForm& form) {
  bool is_change_password_form =
      !form.new_password_value.empty() && !form.password_value.empty();
  is_ignorable_change_password_form_ =
      is_change_password_form && !form.username_marked_by_site &&
      !DoesUsenameAndPasswordMatchCredentials(
          form.username_value, form.password_value, best_matches_) &&
      !client_->IsUpdatePasswordUIEnabled();
  bool is_signup_form =
      !form.new_password_value.empty() && form.password_value.empty();
  bool no_username = form.username_element.empty();

  if (form.layout == PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP) {
    form_type_ = kFormTypeLoginAndSignup;
  } else if (is_ignorable_change_password_form_) {
    if (no_username)
      form_type_ = kFormTypeChangePasswordNoUsername;
    else
      form_type_ = kFormTypeChangePasswordDisabled;
  } else if (is_change_password_form) {
    form_type_ = kFormTypeChangePasswordEnabled;
  } else if (is_signup_form) {
    if (no_username)
      form_type_ = kFormTypeSignupNoUsername;
    else
      form_type_ = kFormTypeSignup;
  } else if (no_username) {
    form_type_ = kFormTypeLoginNoUsername;
  } else {
    form_type_ = kFormTypeLogin;
  }
}

void PasswordFormManager::OnRequestDone(
    ScopedVector<PasswordForm> logins_result) {
  preferred_match_ = nullptr;
  best_matches_.clear();
  blacklisted_matches_.clear();
  const size_t logins_result_size = logins_result.size();

  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_ON_REQUEST_DONE_METHOD);
  }

  // Remove credentials which need to be ignored from |logins_result|.
  if (!observed_form_.ssl_valid) {
    logins_result.erase(
        std::partition(logins_result.begin(), logins_result.end(),
                       [](PasswordForm* form) { return !form->ssl_valid; }),
        logins_result.end());
  }
  logins_result =
      client_->CreateStoreResultFilter()->FilterResults(logins_result.Pass());

  // Deal with blacklisted forms.
  auto begin_blacklisted = std::partition(
      logins_result.begin(), logins_result.end(),
      [](PasswordForm* form) { return !form->blacklisted_by_user; });
  for (auto it = begin_blacklisted; it != logins_result.end(); ++it) {
    if (IsBlacklistMatch(**it)) {
      blacklisted_matches_.push_back(*it);
      *it = nullptr;
    }
  }
  logins_result.erase(begin_blacklisted, logins_result.end());

  // Now compute scores for the remaining credentials in |login_result|.
  std::vector<uint32_t> credential_scores;
  credential_scores.reserve(logins_result.size());
  uint32_t best_score = 0;
  for (const PasswordForm* login : logins_result) {
    uint32_t current_score = ScoreResult(*login);
    if (current_score > best_score)
      best_score = current_score;
    credential_scores.push_back(current_score);
  }

  if (best_score == 0) {
    if (logger) {
      logger->LogNumber(Logger::STRING_BEST_SCORE,
                        static_cast<size_t>(best_score));
    }
    return;
  }

  // Start the |best_matches_| with the best-scoring normal credentials and save
  // the worse-scoring "protected" ones for later.
  ScopedVector<PasswordForm> protected_credentials;
  for (size_t i = 0; i < logins_result.size(); ++i) {
    // Take ownership of the PasswordForm from the ScopedVector.
    scoped_ptr<PasswordForm> login(logins_result[i]);
    logins_result[i] = nullptr;
    DCHECK(!login->blacklisted_by_user);

    if (credential_scores[i] < best_score) {
      // Empty path matches are most commonly imports from Firefox, and
      // generally useful to autofill. Blacklisted entries are only meaningful
      // in the absence of non-blacklisted entries, in which case they need no
      // protection to become |best_matches_|. TODO(timsteele): Bug 1269400. We
      // probably should do something more elegant for any shorter-path match
      // instead of explicitly handling empty path matches.
      bool is_credential_protected =
          observed_form_.scheme == PasswordForm::SCHEME_HTML &&
          base::StartsWith("/", login->origin.path(),
                           base::CompareCase::SENSITIVE) &&
          credential_scores[i] > 0;
      // Passwords generated on a signup form must show on a login form even if
      // there are better-matching saved credentials. TODO(gcasto): We don't
      // want to cut credentials that were saved on signup forms even if they
      // weren't generated, but currently it's hard to distinguish between those
      // forms and two different login forms on the same domain. Filed
      // http://crbug.com/294468 to look into this.
      is_credential_protected |= login->type == PasswordForm::TYPE_GENERATED;

      if (is_credential_protected)
        protected_credentials.push_back(login.Pass());
      continue;
    }

    // If there is another best-score match for the same username, replace it.
    // TODO(vabr): Spare the replacing and keep the first instead of the last
    // candidate.
    const base::string16& username = login->username_value;
    auto best_match_username = best_matches_.find(username);
    if (best_match_username != best_matches_.end() &&
        best_match_username->second == preferred_match_) {
      preferred_match_ = nullptr;
    }
    // Transfer ownership into the map.
    const PasswordForm* best_match = login.get();
    // TODO(mgiuca): Directly assign to |best_match_username|, instead of doing
    // a second map traversal. This will only be possible once we have C++11
    // library support (then |best_matches_| can be a map of scoped_ptrs instead
    // of a ScopedPtrMap).
    best_matches_.set(username, login.Pass());
    if (best_match->preferred)
      preferred_match_ = best_match;
  }

  // Add the protected results if we don't already have a result with the same
  // username.
  for (ScopedVector<PasswordForm>::iterator it = protected_credentials.begin();
       it != protected_credentials.end(); ++it) {
    // Take ownership of the PasswordForm from the ScopedVector.
    scoped_ptr<PasswordForm> protege(*it);
    *it = nullptr;
    const base::string16& username = protege->username_value;
    best_matches_.insert(username, protege.Pass());
  }

  UMA_HISTOGRAM_COUNTS("PasswordManager.NumPasswordsNotShown",
                       logins_result_size - best_matches_.size());

  if (!best_matches_.empty()) {
    // It is possible we have at least one match but have no preferred_match_,
    // because a user may have chosen to 'Forget' the preferred match. So we
    // just pick the first one and whichever the user selects for submit will
    // be saved as preferred.
    if (!preferred_match_)
      preferred_match_ = best_matches_.begin()->second;
  }
}

void PasswordFormManager::ProcessFrame(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  if (state_ == POST_MATCHING_PHASE)
    ProcessFrameInternal(driver);

  for (auto const& old_driver : drivers_) {
    // |drivers_| is not a set because WeakPtr has no good candidate for a key
    // (the address may change to null). So let's weed out duplicates in O(N).
    if (old_driver.get() == driver.get())
      return;
  }

  drivers_.push_back(driver);
}

void PasswordFormManager::ProcessFrameInternal(
    const base::WeakPtr<PasswordManagerDriver>& driver) {
  if (!driver || manager_action_ == kManagerActionBlacklisted)
    return;

  // Allow generation for any non-blacklisted form.
  driver->AllowPasswordGenerationForForm(observed_form_);

  if (best_matches_.empty())
    return;

  // Proceed to autofill.
  // Note that we provide the choices but don't actually prefill a value if:
  // (1) we are in Incognito mode, (2) the ACTION paths don't match,
  // (3) if it matched using public suffix domain matching, or
  // (4) the form is change password form.
  // However, 2 and 3 should not apply to Android-based credentials found
  // via affiliation-based matching (we want to autofill them).
  // TODO(engedy): Clean this up. See: https://crbug.com/476519.
  bool wait_for_username =
      client_->IsOffTheRecord() ||
      (!IsValidAndroidFacetURI(preferred_match_->original_signon_realm) &&
       (observed_form_.action.GetWithEmptyPath() !=
            preferred_match_->action.GetWithEmptyPath() ||
        preferred_match_->IsPublicSuffixMatch() ||
        observed_form_.IsPossibleChangePasswordForm()));
  if (wait_for_username)
    manager_action_ = kManagerActionNone;
  else
    manager_action_ = kManagerActionAutofilled;
  password_manager_->Autofill(driver.get(), observed_form_, best_matches_,
                              *preferred_match_, wait_for_username);
}

void PasswordFormManager::OnGetPasswordStoreResults(
    ScopedVector<PasswordForm> results) {
  DCHECK_EQ(state_, MATCHING_PHASE);

  if (next_prompt_policy_) {
    // The received results are no longer up-to-date, need to re-request.
    state_ = PRE_MATCHING_PHASE;
    FetchMatchingLoginsFromPasswordStore(*next_prompt_policy_);
    next_prompt_policy_.reset();
    return;
  }

  scoped_ptr<BrowserSavePasswordProgressLogger> logger;
  if (client_->IsLoggingActive()) {
    logger.reset(new BrowserSavePasswordProgressLogger(client_));
    logger->LogMessage(Logger::STRING_ON_GET_STORE_RESULTS_METHOD);
    logger->LogNumber(Logger::STRING_NUMBER_RESULTS, results.size());
  }

  if (!results.empty())
    OnRequestDone(results.Pass());
  state_ = POST_MATCHING_PHASE;

  // If password store was slow and provisionally saved form is already here
  // then create pending credentials (see http://crbug.com/470322).
  if (provisionally_saved_form_)
    CreatePendingCredentials();

  if (manager_action_ != kManagerActionBlacklisted) {
    for (auto const& driver : drivers_)
      ProcessFrameInternal(driver);
  }
}

void PasswordFormManager::SaveAsNewLogin() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(IsNewLogin());
  // The new_form is being used to sign in, so it is preferred.
  DCHECK(pending_credentials_.preferred);
  DCHECK(!pending_credentials_.blacklisted_by_user);
  // new_form contains the same basic data as observed_form_ (because its the
  // same form), but with the newly added credentials.

  DCHECK(!client_->IsOffTheRecord());

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    NOTREACHED();
    return;
  }

  // Upload credentials the first time they are saved. This data is used
  // by password generation to help determine account creation sites.
  // Credentials that have been previously used (e.g. PSL matches) are checked
  // to see if they are valid account creation forms.
    if (pending_credentials_.times_used == 0) {
      UploadPasswordForm(pending_credentials_.form_data, base::string16(),
                         autofill::PASSWORD, std::string());
    } else {
      SendAutofillVotes(observed_form_, &pending_credentials_);
    }

  pending_credentials_.date_created = Time::Now();
  SanitizePossibleUsernames(&pending_credentials_);
  password_store->AddLogin(pending_credentials_);

  UpdatePreferredLoginState(password_store);
}

void PasswordFormManager::SanitizePossibleUsernames(PasswordForm* form) {
  // Remove any possible usernames that could be credit cards or SSN for privacy
  // reasons. Also remove duplicates, both in other_possible_usernames and
  // between other_possible_usernames and username_value.
  std::set<base::string16> set;
  for (std::vector<base::string16>::const_iterator it =
           form->other_possible_usernames.begin();
       it != form->other_possible_usernames.end(); ++it) {
    if (!autofill::IsValidCreditCardNumber(*it) && !autofill::IsSSN(*it))
      set.insert(*it);
  }
  set.erase(form->username_value);
  std::vector<base::string16> temp(set.begin(), set.end());
  form->other_possible_usernames.swap(temp);
}

void PasswordFormManager::UpdatePreferredLoginState(
    PasswordStore* password_store) {
  DCHECK(password_store);
  PasswordFormMap::const_iterator iter;
  for (iter = best_matches_.begin(); iter != best_matches_.end(); iter++) {
    if (iter->second->username_value != pending_credentials_.username_value &&
        iter->second->preferred) {
      // This wasn't the selected login but it used to be preferred.
      iter->second->preferred = false;
      if (user_action_ == kUserActionNone)
        user_action_ = kUserActionChoose;
      password_store->UpdateLogin(*iter->second);
    }
  }
}

void PasswordFormManager::UpdateLogin() {
  DCHECK_EQ(state_, POST_MATCHING_PHASE);
  DCHECK(preferred_match_);
  // If we're doing an Update, we either autofilled correctly and need to
  // update the stats, or the user typed in a new password for autofilled
  // username, or the user selected one of the non-preferred matches,
  // thus requiring a swap of preferred bits.
  DCHECK(!IsNewLogin() && pending_credentials_.preferred);
  DCHECK(!client_->IsOffTheRecord());

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    NOTREACHED();
    return;
  }

  UpdateMetadataForUsage(pending_credentials_);

  if (client_->IsSyncAccountCredential(
          base::UTF16ToUTF8(pending_credentials_.username_value),
          pending_credentials_.signon_realm)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialUsed"));
  }

  // Check to see if this form is a candidate for password generation.
  SendAutofillVotes(observed_form_, &pending_credentials_);

  UpdatePreferredLoginState(password_store);

  // Update the new preferred login.
  if (!selected_username_.empty()) {
    // Username has changed. We set this selected username as the real
    // username. Given that |username_value| is part of the Sync and
    // PasswordStore primary key, the old primary key must be supplied.
    PasswordForm old_primary_key(pending_credentials_);
    pending_credentials_.username_value = selected_username_;
    password_store->UpdateLoginWithPrimaryKey(pending_credentials_,
                                              old_primary_key);
  } else if (observed_form_.new_password_element.empty() &&
             (pending_credentials_.password_element.empty() ||
              pending_credentials_.username_element.empty() ||
              pending_credentials_.submit_element.empty())) {
    // If |observed_form_| was a sign-up or change password form, there is no
    // point in trying to update element names: they are likely going to be
    // different than those on a login form.
    // Otherwise, given that |password_element| and |username_element| are part
    // of Sync and PasswordStore primary key, the old primary key must be
    // supplied to UpdateLogin().
    PasswordForm old_primary_key(pending_credentials_);
    pending_credentials_.password_element = observed_form_.password_element;
    pending_credentials_.username_element = observed_form_.username_element;
    pending_credentials_.submit_element = observed_form_.submit_element;
    password_store->UpdateLoginWithPrimaryKey(pending_credentials_,
                                              old_primary_key);
  } else {
    password_store->UpdateLogin(pending_credentials_);
  }
}

void PasswordFormManager::UpdateMetadataForUsage(
    const PasswordForm& credential) {
  ++pending_credentials_.times_used;

  // Remove alternate usernames. At this point we assume that we have found
  // the right username.
  pending_credentials_.other_possible_usernames.clear();
}

bool PasswordFormManager::UpdatePendingCredentialsIfOtherPossibleUsername(
    const base::string16& username) {
  for (PasswordFormMap::const_iterator it = best_matches_.begin();
       it != best_matches_.end(); ++it) {
    for (size_t i = 0; i < it->second->other_possible_usernames.size(); ++i) {
      if (it->second->other_possible_usernames[i] == username) {
        pending_credentials_ = *it->second;
        return true;
      }
    }
  }
  return false;
}

void PasswordFormManager::SendAutofillVotes(
    const PasswordForm& observed,
    PasswordForm* pending) {
  if (pending->form_data.fields.empty())
    return;

  FormStructure pending_structure(pending->form_data);
  FormStructure observed_structure(observed.form_data);

  // Ignore |pending_structure| if its FormData has no fields. This is to
  // weed out those credentials that were saved before FormData was added
  // to PasswordForm. Even without this check, these FormStructure's won't
  // be uploaded, but it makes it hard to see if we are encountering
  // unexpected errors.
  if (pending_structure.FormSignature() != observed_structure.FormSignature()) {
    // Only upload if this is the first time the password has been used.
    // Otherwise the credentials have been used on the same field before so
    // they aren't from an account creation form.
    // Also bypass uploading if the username was edited. Offering generation
    // in cases where we currently save the wrong username isn't great.
    // TODO(gcasto): Determine if generation should be offered in this case.
    if (pending->times_used == 1 && selected_username_.empty()) {
      if (UploadPasswordForm(pending->form_data, pending->username_element,
                             autofill::ACCOUNT_CREATION_PASSWORD,
                             observed_structure.FormSignature())) {
        pending->generation_upload_status =
            autofill::PasswordForm::POSITIVE_SIGNAL_SENT;
      }
    }
  } else if (pending->generation_upload_status ==
             autofill::PasswordForm::POSITIVE_SIGNAL_SENT) {
    // A signal was sent that this was an account creation form, but the
    // credential is now being used on the same form again. This cancels out
    // the previous vote.
    if (UploadPasswordForm(pending->form_data, base::string16(),
                           autofill::NOT_ACCOUNT_CREATION_PASSWORD,
                           std::string())) {
      pending->generation_upload_status =
          autofill::PasswordForm::NEGATIVE_SIGNAL_SENT;
    }
  }
}

bool PasswordFormManager::UploadPasswordForm(
    const autofill::FormData& form_data,
    const base::string16& username_field,
    const autofill::ServerFieldType& password_type,
    const std::string& login_form_signature) {
  autofill::AutofillManager* autofill_manager =
      client_->GetAutofillManagerForMainFrame();
  if (!autofill_manager)
    return false;

  // Note that this doesn't guarantee that the upload succeeded, only that
  // |form_data| is considered uploadable.
  bool success = autofill_manager->UploadPasswordForm(
      form_data, username_field, password_type, login_form_signature);
  UMA_HISTOGRAM_BOOLEAN("PasswordGeneration.UploadStarted", success);
  return success;
}

void PasswordFormManager::CreatePendingCredentials() {
  DCHECK(provisionally_saved_form_);
  base::string16 password_to_save(PasswordToSave(*provisionally_saved_form_));

  // Make sure the important fields stay the same as the initially observed or
  // autofilled ones, as they may have changed if the user experienced a login
  // failure.
  // Look for these credentials in the list containing auto-fill entries.
  PasswordFormMap::const_iterator it =
      best_matches_.find(provisionally_saved_form_->username_value);
  if (it != best_matches_.end()) {
    // The user signed in with a login we autofilled.
    pending_credentials_ = *it->second;
    password_overridden_ =
        pending_credentials_.password_value != password_to_save;
    if (IsPendingCredentialsPublicSuffixMatch()) {
      // If the autofilled credentials were only a PSL match, store a copy with
      // the current origin and signon realm. This ensures that on the next
      // visit, a precise match is found.
      is_new_login_ = true;
      user_action_ = password_overridden_ ? kUserActionChoosePslMatch
                                          : kUserActionOverridePassword;

      // Since this credential will not overwrite a previously saved credential,
      // username_value can be updated now.
      if (!selected_username_.empty())
        pending_credentials_.username_value = selected_username_;

      // Update credential to reflect that it has been used for submission.
      // If this isn't updated, then password generation uploads are off for
      // sites where PSL matching is required to fill the login form, as two
      // PASSWORD votes are uploaded per saved password instead of one.
      //
      // TODO(gcasto): It would be nice if other state were shared such that if
      // say a password was updated on one match it would update on all related
      // passwords. This is a much larger change.
      UpdateMetadataForUsage(pending_credentials_);

      // Normally, the copy of the PSL matched credentials, adapted for the
      // current domain, is saved automatically without asking the user, because
      // the copy likely represents the same account, i.e., the one for which
      // the user already agreed to store a password.
      //
      // However, if the user changes the suggested password, it might indicate
      // that the autofilled credentials and |provisionally_saved_form_|
      // actually correspond  to two different accounts (see
      // http://crbug.com/385619). In that case the user should be asked again
      // before saving the password. This is ensured by clearing
      // |original_signon_realm| on |pending_credentials_|, which unmarks it as
      // a PSL match.
      //
      // There is still the edge case when the autofilled credentials represent
      // the same account as |provisionally_saved_form_| but the stored password
      // was out of date. In that case, the user just had to manually enter the
      // new password, which is now in |provisionally_saved_form_|. The best
      // thing would be to save automatically, and also update the original
      // credentials. However, we have no way to tell if this is the case.
      // This will likely happen infrequently, and the inconvenience put on the
      // user by asking them is not significant, so we are fine with asking
      // here again.
      if (password_overridden_) {
        pending_credentials_.original_signon_realm.clear();
        DCHECK(!IsPendingCredentialsPublicSuffixMatch());
      }
    } else {  // Not a PSL match.
      is_new_login_ = false;
      if (password_overridden_)
        user_action_ = kUserActionOverridePassword;
    }
  } else if (other_possible_username_action_ ==
                 ALLOW_OTHER_POSSIBLE_USERNAMES &&
             UpdatePendingCredentialsIfOtherPossibleUsername(
                 provisionally_saved_form_->username_value)) {
    // |pending_credentials_| is now set. Note we don't update
    // |pending_credentials_.username_value| to |credentials.username_value|
    // yet because we need to keep the original username to modify the stored
    // credential.
    selected_username_ = provisionally_saved_form_->username_value;
    is_new_login_ = false;
  } else if (client_->IsUpdatePasswordUIEnabled() && !best_matches_.empty() &&
             provisionally_saved_form_
                 ->IsPossibleChangePasswordFormWithoutUsername()) {
    PasswordForm* best_update_match = FindBestMatchForUpdatePassword(
        provisionally_saved_form_->password_value);

    if (best_update_match)
      pending_credentials_ = *best_update_match;
    else
      pending_credentials_.origin = provisionally_saved_form_->origin;
    is_new_login_ = false;
    // We don't care about |pending_credentials_| if we didn't find the best
    // match, since the user will select the correct one.
  } else {
    // User typed in a new, unknown username.
    user_action_ = kUserActionOverrideUsernameAndPassword;
    pending_credentials_ = observed_form_;
    if (provisionally_saved_form_->was_parsed_using_autofill_predictions)
      pending_credentials_.username_element =
          provisionally_saved_form_->username_element;
    pending_credentials_.username_value =
        provisionally_saved_form_->username_value;
    pending_credentials_.other_possible_usernames =
        provisionally_saved_form_->other_possible_usernames;

    // The password value will be filled in later, remove any garbage for now.
    pending_credentials_.password_value.clear();
    pending_credentials_.new_password_value.clear();

    // If this was a sign-up or change password form, the names of the elements
    // are likely different than those on a login form, so do not bother saving
    // them. We will fill them with meaningful values in UpdateLogin() when the
    // user goes onto a real login form for the first time.
    if (!provisionally_saved_form_->new_password_element.empty()) {
      pending_credentials_.password_element.clear();
    }
  }

  pending_credentials_.action = provisionally_saved_form_->action;
  // If the user selected credentials we autofilled from a PasswordForm
  // that contained no action URL (IE6/7 imported passwords, for example),
  // bless it with the action URL from the observed form. See bug 1107719.
  if (pending_credentials_.action.is_empty())
    pending_credentials_.action = observed_form_.action;

  pending_credentials_.password_value = password_to_save;
  pending_credentials_.preferred = provisionally_saved_form_->preferred;

  if (user_action_ == kUserActionOverridePassword &&
      pending_credentials_.type == PasswordForm::TYPE_GENERATED &&
      !has_generated_password_) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::PASSWORD_OVERRIDDEN);
    pending_credentials_.type = PasswordForm::TYPE_MANUAL;
  }

  if (has_generated_password_)
    pending_credentials_.type = PasswordForm::TYPE_GENERATED;

  provisionally_saved_form_.reset();
}

uint32_t PasswordFormManager::ScoreResult(const PasswordForm& candidate) const {
  DCHECK_EQ(state_, MATCHING_PHASE);
  DCHECK(!candidate.blacklisted_by_user);
  // For scoring of candidate login data:
  // The most important element that should match is the signon_realm followed
  // by the origin, the action, the password name, the submit button name, and
  // finally the username input field name.
  // If public suffix origin match was not used, it gives an addition of
  // 128 (1 << 7).
  // Exact origin match gives an addition of 64 (1 << 6) + # of matching url
  // dirs.
  // Partial match gives an addition of 32 (1 << 5) + # matching url dirs
  // That way, a partial match cannot trump an exact match even if
  // the partial one matches all other attributes (action, elements) (and
  // regardless of the matching depth in the URL path).

  // When comparing path segments, only consider at most 63 of them, so that the
  // potential gain from shared path prefix is not more than from an exact
  // origin match.
  const size_t kSegmentCountCap = 63;
  const size_t capped_form_path_segment_count =
      std::min(form_path_segments_.size(), kSegmentCountCap);

  uint32_t score = 0u;
  if (!candidate.IsPublicSuffixMatch()) {
    score += 1u << 7;
  }
  if (candidate.origin == observed_form_.origin) {
    // This check is here for the most common case which
    // is we have a single match in the db for the given host,
    // so we don't generally need to walk the entire URL path (the else
    // clause).
    score += (1u << 6) + static_cast<uint32_t>(capped_form_path_segment_count);
  } else {
    // Walk the origin URL paths one directory at a time to see how
    // deep the two match.
    std::vector<std::string> candidate_path_segments =
        SplitPathToSegments(candidate.origin.path());
    size_t depth = 0u;
    const size_t max_dirs = std::min(capped_form_path_segment_count,
                                     candidate_path_segments.size());
    while ((depth < max_dirs) &&
           (form_path_segments_[depth] == candidate_path_segments[depth])) {
      depth++;
      score++;
    }
    // do we have a partial match?
    score += (depth > 0u) ? 1u << 5 : 0u;
  }
  if (observed_form_.scheme == PasswordForm::SCHEME_HTML) {
    if (candidate.action == observed_form_.action)
      score += 1u << 3;
    if (candidate.password_element == observed_form_.password_element)
      score += 1u << 2;
    if (candidate.submit_element == observed_form_.submit_element)
      score += 1u << 1;
    if (candidate.username_element == observed_form_.username_element)
      score += 1u << 0;
  }

  return score;
}

bool PasswordFormManager::IsBlacklistMatch(
    const autofill::PasswordForm& blacklisted_form) const {
  DCHECK(blacklisted_form.blacklisted_by_user);

  if (blacklisted_form.IsPublicSuffixMatch())
    return false;
  if (blacklisted_form.origin.GetOrigin() != observed_form_.origin.GetOrigin())
    return false;
  if (observed_form_.scheme == PasswordForm::SCHEME_HTML) {
    if (!AreStringsEqualOrEmpty(blacklisted_form.submit_element,
                                observed_form_.submit_element))
      return false;
    if (!AreStringsEqualOrEmpty(blacklisted_form.password_element,
                                observed_form_.password_element))
      return false;
    if (!AreStringsEqualOrEmpty(blacklisted_form.username_element,
                                observed_form_.username_element))
      return false;
  }
  return true;
}

void PasswordFormManager::DeleteEmptyUsernameCredentials() {
  if (best_matches_.empty() || pending_credentials_.username_value.empty())
    return;
  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store) {
    NOTREACHED();
    return;
  }
  for (auto iter = best_matches_.begin(); iter != best_matches_.end(); ++iter) {
    PasswordForm* form = iter->second;
    if (!form->IsPublicSuffixMatch() && form->username_value.empty() &&
        form->password_value == pending_credentials_.password_value)
      password_store->RemoveLogin(*form);
  }
}

PasswordForm* PasswordFormManager::FindBestMatchForUpdatePassword(
    const base::string16& password) const {
  if (best_matches_.size() == 1) {
    // In case when the user has only one credential, consider it the same as
    // is being saved.
    return best_matches_.begin()->second;
  }
  if (password.empty())
    return nullptr;

  PasswordFormMap::const_iterator best_password_match_it = best_matches_.end();
  for (auto it = best_matches_.begin(); it != best_matches_.end(); ++it) {
    if (it->second->password_value == password) {
      if (best_password_match_it != best_matches_.end()) {
        // Found a second credential with the same password, do nothing.
        return nullptr;
      }
      best_password_match_it = it;
    }
  }
  return best_password_match_it == best_matches_.end()
             ? nullptr
             : best_password_match_it->second;
}

void PasswordFormManager::LogSubmitPassed() {
  if (submit_result_ != kSubmitResultFailed) {
    if (has_generated_password_) {
      metrics_util::LogPasswordGenerationSubmissionEvent(
          metrics_util::PASSWORD_SUBMITTED);
    } else if (generation_available_) {
      metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
          metrics_util::PASSWORD_SUBMITTED);
    }
  }
  submit_result_ = kSubmitResultPassed;
}

void PasswordFormManager::LogSubmitFailed() {
  if (has_generated_password_) {
    metrics_util::LogPasswordGenerationSubmissionEvent(
        metrics_util::GENERATED_PASSWORD_FORCE_SAVED);
  } else if (generation_available_) {
    metrics_util::LogPasswordGenerationAvailableSubmissionEvent(
        metrics_util::PASSWORD_SUBMISSION_FAILED);
  }
  submit_result_ = kSubmitResultFailed;
}

void PasswordFormManager::WipeStoreCopyIfOutdated() {
  DCHECK_NE(PRE_MATCHING_PHASE, state_);

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.StoreReadyWhenWiping",
                        HasCompletedMatching());

  PasswordStore* password_store = client_->GetPasswordStore();
  if (!password_store)
    return;

  for (PasswordFormMap::const_iterator stored_credential =
           best_matches_.begin();
       stored_credential != best_matches_.end();
       /*no increment here*/) {
    // Beware erase() below, keep the cycle iterator valid.
    PasswordFormMap::const_iterator credential_to_delete = stored_credential;
    ++stored_credential;

    if (pending_credentials_.password_value ==
        credential_to_delete->second->password_value) {
      continue;
    }
    if (!gaia::AreEmailsSame(
            base::UTF16ToUTF8(pending_credentials_.username_value),
            base::UTF16ToUTF8(credential_to_delete->first))) {
      continue;
    }
    password_store->RemoveLogin(*credential_to_delete->second);
    if (credential_to_delete->second == preferred_match_)
      preferred_match_ = nullptr;
    best_matches_.erase(credential_to_delete);
  }
}

}  // namespace password_manager
