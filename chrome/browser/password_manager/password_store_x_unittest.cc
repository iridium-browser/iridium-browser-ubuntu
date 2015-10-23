// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_store_x.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::UnorderedPasswordFormElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;

namespace {

class MockPasswordStoreConsumer
    : public password_manager::PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<PasswordForm*>&));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(ScopedVector<PasswordForm> results) override {
    OnGetPasswordStoreResultsConstRef(results.get());
  }
};

class MockPasswordStoreObserver
    : public password_manager::PasswordStore::Observer {
 public:
  MOCK_METHOD1(OnLoginsChanged,
               void(const password_manager::PasswordStoreChangeList& changes));
};

class FailingBackend : public PasswordStoreX::NativeBackend {
 public:
  bool Init() override { return true; }

  PasswordStoreChangeList AddLogin(const PasswordForm& form) override {
    return PasswordStoreChangeList();
  }
  bool UpdateLogin(const PasswordForm& form,
                   PasswordStoreChangeList* changes) override {
    return false;
  }
  bool RemoveLogin(const PasswordForm& form,
                   PasswordStoreChangeList* changes) override {
    return false;
  }

  bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override {
    return false;
  }

  bool RemoveLoginsSyncedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override {
    return false;
  }

  // Use this as a landmine to check whether results of failed Get*Logins calls
  // get ignored.
  static ScopedVector<autofill::PasswordForm> CreateTrashForms() {
    ScopedVector<autofill::PasswordForm> forms;
    PasswordForm trash;
    trash.username_element = base::ASCIIToUTF16("trash u. element");
    trash.username_value = base::ASCIIToUTF16("trash u. value");
    trash.password_element = base::ASCIIToUTF16("trash p. element");
    trash.password_value = base::ASCIIToUTF16("trash p. value");
    for (size_t i = 0; i < 3; ++i) {
      trash.origin = GURL(base::StringPrintf("http://trash%zu.com", i));
      forms.push_back(new PasswordForm(trash));
    }
    return forms.Pass();
  }

  bool GetLogins(const PasswordForm& form,
                 ScopedVector<autofill::PasswordForm>* forms) override {
    *forms = CreateTrashForms();
    return false;
  }

  bool GetAutofillableLogins(
      ScopedVector<autofill::PasswordForm>* forms) override {
    *forms = CreateTrashForms();
    return false;
  }
  bool GetBlacklistLogins(
      ScopedVector<autofill::PasswordForm>* forms) override {
    *forms = CreateTrashForms();
    return false;
  }
};

class MockBackend : public PasswordStoreX::NativeBackend {
 public:
  bool Init() override { return true; }

  PasswordStoreChangeList AddLogin(const PasswordForm& form) override {
    all_forms_.push_back(form);
    PasswordStoreChange change(PasswordStoreChange::ADD, form);
    return PasswordStoreChangeList(1, change);
  }

  bool UpdateLogin(const PasswordForm& form,
                   PasswordStoreChangeList* changes) override {
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (ArePasswordFormUniqueKeyEqual(all_forms_[i], form)) {
        all_forms_[i] = form;
        changes->push_back(PasswordStoreChange(PasswordStoreChange::UPDATE,
                                               form));
      }
    }
    return true;
  }

  bool RemoveLogin(const PasswordForm& form,
                   PasswordStoreChangeList* changes) override {
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (ArePasswordFormUniqueKeyEqual(all_forms_[i], form)) {
        changes->push_back(PasswordStoreChange(PasswordStoreChange::REMOVE,
                                               form));
        erase(i--);
      }
    }
    return true;
  }

  bool RemoveLoginsCreatedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override {
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (delete_begin <= all_forms_[i].date_created &&
          (delete_end.is_null() || all_forms_[i].date_created < delete_end))
        erase(i--);
    }
    return true;
  }

  bool RemoveLoginsSyncedBetween(
      base::Time delete_begin,
      base::Time delete_end,
      password_manager::PasswordStoreChangeList* changes) override {
    DCHECK(changes);
    for (size_t i = 0; i < all_forms_.size(); ++i) {
      if (delete_begin <= all_forms_[i].date_synced &&
          (delete_end.is_null() || all_forms_[i].date_synced < delete_end)) {
        changes->push_back(password_manager::PasswordStoreChange(
            password_manager::PasswordStoreChange::REMOVE, all_forms_[i]));
        erase(i--);
      }
    }
    return true;
  }

  bool GetLogins(const PasswordForm& form,
                 ScopedVector<autofill::PasswordForm>* forms) override {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (all_forms_[i].signon_realm == form.signon_realm)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  bool GetAutofillableLogins(
      ScopedVector<autofill::PasswordForm>* forms) override {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (!all_forms_[i].blacklisted_by_user)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

  bool GetBlacklistLogins(
      ScopedVector<autofill::PasswordForm>* forms) override {
    for (size_t i = 0; i < all_forms_.size(); ++i)
      if (all_forms_[i].blacklisted_by_user)
        forms->push_back(new PasswordForm(all_forms_[i]));
    return true;
  }

 private:
  void erase(size_t index) {
    if (index < all_forms_.size() - 1)
      all_forms_[index] = all_forms_[all_forms_.size() - 1];
    all_forms_.pop_back();
  }

  std::vector<PasswordForm> all_forms_;
};

class MockLoginDatabaseReturn {
 public:
  MOCK_METHOD1(OnLoginDatabaseQueryDone,
               void(const std::vector<PasswordForm*>&));
};

void LoginDatabaseQueryCallback(password_manager::LoginDatabase* login_db,
                                bool autofillable,
                                MockLoginDatabaseReturn* mock_return) {
  ScopedVector<autofill::PasswordForm> forms;
  if (autofillable)
    EXPECT_TRUE(login_db->GetAutofillableLogins(&forms));
  else
    EXPECT_TRUE(login_db->GetBlacklistLogins(&forms));
  mock_return->OnLoginDatabaseQueryDone(forms.get());
}

// Generate |count| expected logins, either auto-fillable or blacklisted.
void InitExpectedForms(bool autofillable,
                       size_t count,
                       ScopedVector<autofill::PasswordForm>* forms) {
  const char* domain = autofillable ? "example" : "blacklisted";
  for (size_t i = 0; i < count; ++i) {
    std::string realm = base::StringPrintf("http://%zu.%s.com", i, domain);
    std::string origin = base::StringPrintf("http://%zu.%s.com/origin",
                                            i, domain);
    std::string action = base::StringPrintf("http://%zu.%s.com/action",
                                            i, domain);
    password_manager::PasswordFormData data = {
        PasswordForm::SCHEME_HTML,
        realm.c_str(),
        origin.c_str(),
        action.c_str(),
        L"submit_element",
        L"username_element",
        L"password_element",
        autofillable ? L"username_value" : nullptr,
        autofillable ? L"password_value" : nullptr,
        autofillable,
        false,
        static_cast<double>(i + 1)};
    forms->push_back(CreatePasswordFormFromDataForTesting(data).Pass());
  }
}

PasswordStoreChangeList AddChangeForForm(const PasswordForm& form) {
  return PasswordStoreChangeList(
      1, PasswordStoreChange(PasswordStoreChange::ADD, form));
}

}  // anonymous namespace

enum BackendType {
  NO_BACKEND,
  FAILING_BACKEND,
  WORKING_BACKEND
};

class PasswordStoreXTest : public testing::TestWithParam<BackendType> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.path().Append(FILE_PATH_LITERAL("login_test"));
  }

  PasswordStoreX::NativeBackend* GetBackend() {
    switch (GetParam()) {
      case FAILING_BACKEND:
        return new FailingBackend();
      case WORKING_BACKEND:
        return new MockBackend();
      default:
        return nullptr;
    }
  }

  content::TestBrowserThreadBundle thread_bundle_;

  base::ScopedTempDir temp_dir_;
};

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

TEST_P(PasswordStoreXTest, Notifications) {
  scoped_ptr<password_manager::LoginDatabase> login_db(
      new password_manager::LoginDatabase(test_login_db_file_path()));
  scoped_refptr<PasswordStoreX> store(new PasswordStoreX(
      base::ThreadTaskRunnerHandle::Get(), base::ThreadTaskRunnerHandle::Get(),
      login_db.Pass(), GetBackend()));
  store->Init(syncer::SyncableService::StartSyncFlare());

  password_manager::PasswordFormData form_data = {
      PasswordForm::SCHEME_HTML,       "http://bar.example.com",
      "http://bar.example.com/origin", "http://bar.example.com/action",
      L"submit_element",               L"username_element",
      L"password_element",             L"username_value",
      L"password_value",               true,
      false,                           1};
  scoped_ptr<PasswordForm> form =
      CreatePasswordFormFromDataForTesting(form_data);

  MockPasswordStoreObserver observer;
  store->AddObserver(&observer);

  const PasswordStoreChange expected_add_changes[] = {
    PasswordStoreChange(PasswordStoreChange::ADD, *form),
  };

  EXPECT_CALL(
      observer,
      OnLoginsChanged(ElementsAreArray(expected_add_changes)));

  // Adding a login should trigger a notification.
  store->AddLogin(*form);

  // The PasswordStore schedules tasks to run on the DB thread. Wait for them
  // to complete.
  base::RunLoop().RunUntilIdle();

  // Change the password.
  form->password_value = base::ASCIIToUTF16("a different password");

  const PasswordStoreChange expected_update_changes[] = {
    PasswordStoreChange(PasswordStoreChange::UPDATE, *form),
  };

  EXPECT_CALL(
      observer,
      OnLoginsChanged(ElementsAreArray(expected_update_changes)));

  // Updating the login with the new password should trigger a notification.
  store->UpdateLogin(*form);

  // Wait for PasswordStore to send execute.
  base::RunLoop().RunUntilIdle();

  const PasswordStoreChange expected_delete_changes[] = {
    PasswordStoreChange(PasswordStoreChange::REMOVE, *form),
  };

  EXPECT_CALL(
      observer,
      OnLoginsChanged(ElementsAreArray(expected_delete_changes)));

  // Deleting the login should trigger a notification.
  store->RemoveLogin(*form);

  // Wait for PasswordStore to execute.
  base::RunLoop().RunUntilIdle();

  store->RemoveObserver(&observer);

  store->Shutdown();
}

TEST_P(PasswordStoreXTest, NativeMigration) {
  ScopedVector<autofill::PasswordForm> expected_autofillable;
  InitExpectedForms(true, 50, &expected_autofillable);

  ScopedVector<autofill::PasswordForm> expected_blacklisted;
  InitExpectedForms(false, 50, &expected_blacklisted);

  const base::FilePath login_db_file = test_login_db_file_path();
  scoped_ptr<password_manager::LoginDatabase> login_db(
      new password_manager::LoginDatabase(login_db_file));
  ASSERT_TRUE(login_db->Init());

  // Get the initial size of the login DB file, before we populate it.
  // This will be used later to make sure it gets back to this size.
  base::File::Info db_file_start_info;
  ASSERT_TRUE(base::GetFileInfo(login_db_file, &db_file_start_info));

  // Populate the login DB with logins that should be migrated.
  for (const autofill::PasswordForm* form : expected_autofillable) {
    EXPECT_EQ(AddChangeForForm(*form), login_db->AddLogin(*form));
  }
  for (const autofill::PasswordForm* form : expected_blacklisted) {
    EXPECT_EQ(AddChangeForForm(*form), login_db->AddLogin(*form));
  }

  // Get the new size of the login DB file. We expect it to be larger.
  base::File::Info db_file_full_info;
  ASSERT_TRUE(base::GetFileInfo(login_db_file, &db_file_full_info));
  EXPECT_GT(db_file_full_info.size, db_file_start_info.size);

  // Initializing the PasswordStore shouldn't trigger a native migration (yet).
  login_db.reset(new password_manager::LoginDatabase(login_db_file));
  scoped_refptr<PasswordStoreX> store(new PasswordStoreX(
      base::ThreadTaskRunnerHandle::Get(), base::ThreadTaskRunnerHandle::Get(),
      login_db.Pass(), GetBackend()));
  store->Init(syncer::SyncableService::StartSyncFlare());

  MockPasswordStoreConsumer consumer;

  // The autofillable forms should have been migrated to the native backend.
  EXPECT_CALL(
      consumer,
      OnGetPasswordStoreResultsConstRef(
          UnorderedPasswordFormElementsAre(expected_autofillable.get())));

  store->GetAutofillableLogins(&consumer);
  base::RunLoop().RunUntilIdle();

  // The blacklisted forms should have been migrated to the native backend.
  EXPECT_CALL(
      consumer,
      OnGetPasswordStoreResultsConstRef(
          UnorderedPasswordFormElementsAre(expected_blacklisted.get())));

  store->GetBlacklistLogins(&consumer);
  base::RunLoop().RunUntilIdle();

  MockLoginDatabaseReturn ld_return;

  if (GetParam() == WORKING_BACKEND) {
    // No autofillable logins should be left in the login DB.
    EXPECT_CALL(ld_return, OnLoginDatabaseQueryDone(IsEmpty()));
  } else {
    // The autofillable logins should still be in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(UnorderedPasswordFormElementsAre(
                    expected_autofillable.get())));
  }

  LoginDatabaseQueryCallback(store->login_db(), true, &ld_return);

  // Wait for the login DB methods to execute.
  base::RunLoop().RunUntilIdle();

  if (GetParam() == WORKING_BACKEND) {
    // Likewise, no blacklisted logins should be left in the login DB.
    EXPECT_CALL(ld_return, OnLoginDatabaseQueryDone(IsEmpty()));
  } else {
    // The blacklisted logins should still be in the login DB.
    EXPECT_CALL(ld_return,
                OnLoginDatabaseQueryDone(UnorderedPasswordFormElementsAre(
                    expected_blacklisted.get())));
  }

  LoginDatabaseQueryCallback(store->login_db(), false, &ld_return);

  // Wait for the login DB methods to execute.
  base::RunLoop().RunUntilIdle();

  if (GetParam() == WORKING_BACKEND) {
    // If the migration succeeded, then not only should there be no logins left
    // in the login DB, but also the file should have been deleted and then
    // recreated. We approximate checking for this by checking that the file
    // size is equal to the size before we populated it, even though it was
    // larger after populating it.
    base::File::Info db_file_end_info;
    ASSERT_TRUE(base::GetFileInfo(login_db_file, &db_file_end_info));
    EXPECT_EQ(db_file_start_info.size, db_file_end_info.size);
  }

  store->Shutdown();
}

INSTANTIATE_TEST_CASE_P(NoBackend,
                        PasswordStoreXTest,
                        testing::Values(NO_BACKEND));
INSTANTIATE_TEST_CASE_P(FailingBackend,
                        PasswordStoreXTest,
                        testing::Values(FAILING_BACKEND));
INSTANTIATE_TEST_CASE_P(WorkingBackend,
                        PasswordStoreXTest,
                        testing::Values(WORKING_BACKEND));
