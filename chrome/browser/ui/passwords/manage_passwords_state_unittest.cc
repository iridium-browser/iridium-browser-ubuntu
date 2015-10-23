// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_state.h"

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

namespace {

class ManagePasswordsStateTest : public testing::Test {
 public:
  void SetUp() override {
    test_local_form_.origin = GURL("http://example.com");
    test_local_form_.username_value = base::ASCIIToUTF16("username");
    test_local_form_.password_value = base::ASCIIToUTF16("12345");

    test_submitted_form_ = test_local_form_;
    test_submitted_form_.username_value = base::ASCIIToUTF16("new one");
    test_submitted_form_.password_value = base::ASCIIToUTF16("asdfjkl;");

    test_federated_form_.origin = GURL("https://idp.com");
    test_federated_form_.username_value = base::ASCIIToUTF16("username");

    passwords_data_.set_client(&client_);
  }

  autofill::PasswordForm& test_local_form() { return test_local_form_; }
  autofill::PasswordForm& test_submitted_form() { return test_submitted_form_; }
  autofill::PasswordForm& test_federated_form() { return test_federated_form_; }
  ManagePasswordsState& passwords_data() { return passwords_data_; }

  // Returns a PasswordFormManager containing test_local_form() as a best match.
  scoped_ptr<password_manager::PasswordFormManager> CreateFormManager();

  // Pushes irrelevant updates to |passwords_data_| and checks that they don't
  // affect the state.
  void TestNoisyUpdates();

  // Pushes both relevant and irrelevant updates to |passwords_data_|.
  void TestAllUpdates();

  // Pushes a blacklisted form and checks that it doesn't affect the state.
  void TestBlacklistedUpdates();

  MOCK_METHOD1(OnChooseCredential,
               void(const password_manager::CredentialInfo&));

 private:
  password_manager::StubPasswordManagerClient client_;

  ManagePasswordsState passwords_data_;
  autofill::PasswordForm test_local_form_;
  autofill::PasswordForm test_submitted_form_;
  autofill::PasswordForm test_federated_form_;
};

scoped_ptr<password_manager::PasswordFormManager>
ManagePasswordsStateTest::CreateFormManager() {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          nullptr, &client_,
          base::WeakPtr<password_manager::PasswordManagerDriver>(),
          test_local_form(), false));
  test_form_manager->SimulateFetchMatchingLoginsFromPasswordStore();
  ScopedVector<autofill::PasswordForm> stored_forms;
  stored_forms.push_back(new autofill::PasswordForm(test_local_form()));
  test_form_manager->OnGetPasswordStoreResults(stored_forms.Pass());
  EXPECT_EQ(1u, test_form_manager->best_matches().size());
  EXPECT_EQ(test_local_form(),
            *test_form_manager->best_matches().begin()->second);
  return test_form_manager.Pass();
}

void ManagePasswordsStateTest::TestNoisyUpdates() {
  const std::vector<const autofill::PasswordForm*> forms =
      passwords_data_.GetCurrentForms();
  const std::vector<const autofill::PasswordForm*> federated_forms =
      passwords_data_.federated_credentials_forms();
  const password_manager::ui::State state = passwords_data_.state();
  const GURL origin = passwords_data_.origin();

  // Push "Add".
  autofill::PasswordForm form;
  form.origin = GURL("http://3rdparty.com");
  form.username_value = base::ASCIIToUTF16("username");
  form.password_value = base::ASCIIToUTF16("12345");
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, form);
  password_manager::PasswordStoreChangeList list(1, change);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, passwords_data().GetCurrentForms());
  EXPECT_EQ(federated_forms, passwords_data().federated_credentials_forms());
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Update the form.
  form.password_value = base::ASCIIToUTF16("password");
  list[0] = password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::UPDATE, form);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, passwords_data().GetCurrentForms());
  EXPECT_EQ(federated_forms, passwords_data().federated_credentials_forms());
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Delete the form.
  list[0] = password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, form);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, passwords_data().GetCurrentForms());
  EXPECT_EQ(federated_forms, passwords_data().federated_credentials_forms());
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
}

void ManagePasswordsStateTest::TestAllUpdates() {
  const std::vector<const autofill::PasswordForm*> forms =
      passwords_data_.GetCurrentForms();
  const std::vector<const autofill::PasswordForm*> federated_forms =
      passwords_data_.federated_credentials_forms();
  const password_manager::ui::State state = passwords_data_.state();
  const GURL origin = passwords_data_.origin();
  EXPECT_NE(GURL::EmptyGURL(), origin);

  // Push "Add".
  autofill::PasswordForm form;
  form.origin = origin;
  form.username_value = base::ASCIIToUTF16("user15");
  form.password_value = base::ASCIIToUTF16("12345");
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, form);
  password_manager::PasswordStoreChangeList list(1, change);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_THAT(passwords_data().GetCurrentForms(), Contains(Pointee(form)));
  EXPECT_EQ(federated_forms, passwords_data().federated_credentials_forms());
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Update the form.
  form.password_value = base::ASCIIToUTF16("password");
  list[0] = password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::UPDATE, form);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_THAT(passwords_data().GetCurrentForms(), Contains(Pointee(form)));
  EXPECT_EQ(federated_forms, passwords_data().federated_credentials_forms());
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Delete the form.
  list[0] = password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, form);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, passwords_data().GetCurrentForms());
  EXPECT_EQ(federated_forms, passwords_data().federated_credentials_forms());
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  TestNoisyUpdates();
}

void ManagePasswordsStateTest::TestBlacklistedUpdates() {
  const std::vector<const autofill::PasswordForm*> forms =
      passwords_data_.GetCurrentForms();
  const std::vector<const autofill::PasswordForm*> federated_forms =
      passwords_data_.federated_credentials_forms();
  const password_manager::ui::State state = passwords_data_.state();
  const GURL origin = passwords_data_.origin();
  EXPECT_NE(GURL::EmptyGURL(), origin);

  // Process the blacklisted form.
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.origin = origin;
  password_manager::PasswordStoreChangeList list;
  list.push_back(password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::ADD, blacklisted));
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, passwords_data().GetCurrentForms());
  EXPECT_EQ(federated_forms, passwords_data().federated_credentials_forms());
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());

  // Delete the blacklisted form.
  list[0] = password_manager::PasswordStoreChange(
      password_manager::PasswordStoreChange::REMOVE, blacklisted);
  passwords_data().ProcessLoginsChanged(list);
  EXPECT_EQ(forms, passwords_data().GetCurrentForms());
  EXPECT_EQ(federated_forms, passwords_data().federated_credentials_forms());
  EXPECT_EQ(state, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
}

TEST_F(ManagePasswordsStateTest, DefaultState) {
  EXPECT_THAT(passwords_data().GetCurrentForms(), IsEmpty());
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, passwords_data().state());
  EXPECT_EQ(GURL::EmptyGURL(), passwords_data().origin());
  EXPECT_FALSE(passwords_data().form_manager());

  TestNoisyUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordSubmitted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_submitted_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  passwords_data().OnPendingPassword(test_form_manager.Pass());

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());
  EXPECT_EQ(test_submitted_form().origin, passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  EXPECT_EQ(test_submitted_form(),
            passwords_data().form_manager()->pending_credentials());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordSaved) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_submitted_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  passwords_data().OnPendingPassword(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());

  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::MANAGE_STATE,
            passwords_data().state());
  EXPECT_EQ(test_submitted_form().origin, passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, OnRequestCredentials) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  federated_credentials.push_back(
      new autofill::PasswordForm(test_federated_form()));
  const GURL origin = test_local_form().origin;
  passwords_data().OnRequestCredentials(local_credentials.Pass(),
                                        federated_credentials.Pass(), origin);
  passwords_data().set_credentials_callback(
      base::Bind(&ManagePasswordsStateTest::OnChooseCredential,
                 base::Unretained(this)));
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));
  EXPECT_THAT(passwords_data().federated_credentials_forms(),
              ElementsAre(Pointee(test_federated_form())));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  TestAllUpdates();

  password_manager::CredentialInfo credential_info(
      test_local_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_CALL(*this, OnChooseCredential(_))
      .WillOnce(testing::SaveArg<0>(&credential_info));
  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY,
            credential_info.type);
  EXPECT_TRUE(passwords_data().credentials_callback().is_null());
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(origin, passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AutoSignin) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  passwords_data().OnAutoSignin(local_credentials.Pass());
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, passwords_data().state());
  EXPECT_EQ(test_local_form().origin, passwords_data().origin());
  TestAllUpdates();

  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(test_local_form().origin, passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, AutomaticPasswordSave) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_submitted_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);

  passwords_data().OnAutomaticPasswordSave(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::CONFIRMATION_STATE, passwords_data().state());
  EXPECT_EQ(test_submitted_form().origin, passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  EXPECT_EQ(test_submitted_form(),
            passwords_data().form_manager()->pending_credentials());
  TestAllUpdates();

  passwords_data().TransitionToState(password_manager::ui::MANAGE_STATE);
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              UnorderedElementsAre(Pointee(test_local_form()),
                                   Pointee(test_submitted_form())));
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(test_submitted_form().origin, passwords_data().origin());
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordAutofilled) {
  autofill::PasswordFormMap password_form_map;
  password_form_map.insert(
      test_local_form().username_value,
      make_scoped_ptr(new autofill::PasswordForm(test_local_form())));
  passwords_data().OnPasswordAutofilled(password_form_map);

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());
  EXPECT_EQ(test_local_form().origin, passwords_data().origin());

  // |passwords_data| should hold a separate copy of test_local_form().
  EXPECT_THAT(passwords_data().GetCurrentForms(),
              Not(Contains(&test_local_form())));
  TestAllUpdates();
}

TEST_F(ManagePasswordsStateTest, InactiveOnPSLMatched) {
  autofill::PasswordForm psl_matched_test_form = test_local_form();
  psl_matched_test_form.original_signon_realm = "http://pslmatched.example.com";
  autofill::PasswordFormMap password_form_map;
  password_form_map.insert(
      psl_matched_test_form.username_value,
      make_scoped_ptr(new autofill::PasswordForm(psl_matched_test_form)));
  passwords_data().OnPasswordAutofilled(password_form_map);

  EXPECT_THAT(passwords_data().GetCurrentForms(), IsEmpty());
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, passwords_data().state());
  EXPECT_EQ(GURL::EmptyGURL(), passwords_data().origin());
  EXPECT_FALSE(passwords_data().form_manager());
}

TEST_F(ManagePasswordsStateTest, OnInactive) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_submitted_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  passwords_data().OnPendingPassword(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());
  passwords_data().OnInactive();
  EXPECT_THAT(passwords_data().GetCurrentForms(), IsEmpty());
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, passwords_data().state());
  EXPECT_EQ(GURL::EmptyGURL(), passwords_data().origin());
  EXPECT_FALSE(passwords_data().form_manager());
  TestNoisyUpdates();
}

TEST_F(ManagePasswordsStateTest, PendingPasswordAddBlacklisted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_submitted_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  passwords_data().OnPendingPassword(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, RequestCredentialsAddBlacklisted) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  federated_credentials.push_back(
      new autofill::PasswordForm(test_federated_form()));
  const GURL origin = test_local_form().origin;
  passwords_data().OnRequestCredentials(local_credentials.Pass(),
                                        federated_credentials.Pass(), origin);
  passwords_data().set_credentials_callback(
      base::Bind(&ManagePasswordsStateTest::OnChooseCredential,
                 base::Unretained(this)));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, AutoSigninAddBlacklisted) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  passwords_data().OnAutoSignin(local_credentials.Pass());
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, AutomaticPasswordSaveAddBlacklisted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_submitted_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  passwords_data().OnAutomaticPasswordSave(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::CONFIRMATION_STATE, passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, BackgroundAutofilledAddBlacklisted) {
  autofill::PasswordFormMap password_form_map;
  password_form_map.insert(
      test_local_form().username_value,
      make_scoped_ptr(new autofill::PasswordForm(test_local_form())));
  passwords_data().OnPasswordAutofilled(password_form_map);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordUpdateAddBlacklisted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_submitted_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  passwords_data().OnUpdatePassword(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            passwords_data().state());

  TestBlacklistedUpdates();
}

TEST_F(ManagePasswordsStateTest, PasswordUpdateSubmitted) {
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      CreateFormManager());
  test_form_manager->ProvisionallySave(
      test_submitted_form(),
      password_manager::PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES);
  passwords_data().OnUpdatePassword(test_form_manager.Pass());

  EXPECT_THAT(passwords_data().GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));
  EXPECT_THAT(passwords_data().federated_credentials_forms(), IsEmpty());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
            passwords_data().state());
  EXPECT_EQ(test_submitted_form().origin, passwords_data().origin());
  ASSERT_TRUE(passwords_data().form_manager());
  EXPECT_EQ(test_submitted_form(),
            passwords_data().form_manager()->pending_credentials());
  TestAllUpdates();
}

}  // namespace
