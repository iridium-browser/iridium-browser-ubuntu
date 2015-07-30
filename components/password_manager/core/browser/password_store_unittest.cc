// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The passwords in the tests below are all empty because PasswordStoreDefault
// does not store the actual passwords on OS X (they are stored in the Keychain
// instead). We could special-case it, but it is easier to just have empty
// passwords. This will not be needed anymore if crbug.com/466638 is fixed.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::WaitableEvent;
using testing::_;
using testing::DoAll;
using testing::WithArg;

namespace password_manager {

namespace {

const char kTestWebRealm1[] = "https://one.example.com/";
const char kTestWebOrigin1[] = "https://one.example.com/origin";
const char kTestWebRealm2[] = "https://two.example.com/";
const char kTestWebOrigin2[] = "https://two.example.com/origin";
const char kTestWebRealm3[] = "https://three.example.com/";
const char kTestWebOrigin3[] = "https://three.example.com/origin";
const char kTestWebRealm4[] = "https://four.example.com/";
const char kTestWebOrigin4[] = "https://four.example.com/origin";
const char kTestWebRealm5[] = "https://five.example.com/";
const char kTestWebOrigin5[] = "https://five.example.com/origin";
const char kTestPSLMatchingWebRealm[] = "https://psl.example.com/";
const char kTestPSLMatchingWebOrigin[] = "https://psl.example.com/origin";
const char kTestUnrelatedWebRealm[] = "https://notexample.com/";
const char kTestUnrelatedWebOrigin[] = "https:/notexample.com/origin";
const char kTestInsecureWebRealm[] = "http://one.example.com/";
const char kTestInsecureWebOrigin[] = "http://one.example.com/origin";
const char kTestAndroidRealm1[] = "android://hash@com.example.android/";
const char kTestAndroidRealm2[] = "android://hash@com.example.two.android/";
const char kTestAndroidRealm3[] = "android://hash@com.example.three.android/";
const char kTestUnrelatedAndroidRealm[] =
    "android://hash@com.notexample.android/";

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
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

class MockAffiliatedMatchHelper : public AffiliatedMatchHelper {
 public:
  MockAffiliatedMatchHelper()
      : AffiliatedMatchHelper(nullptr,
                              make_scoped_ptr<AffiliationService>(nullptr)) {}

  // Expects GetAffiliatedAndroidRealms() to be called with the
  // |expected_observed_form|, and will cause the result callback supplied to
  // GetAffiliatedAndroidRealms() to be invoked with |results_to_return|.
  void ExpectCallToGetAffiliatedAndroidRealms(
      const autofill::PasswordForm& expected_observed_form,
      const std::vector<std::string>& results_to_return) {
    EXPECT_CALL(*this,
                OnGetAffiliatedAndroidRealmsCalled(expected_observed_form))
        .WillOnce(testing::Return(results_to_return));
  }

  // Expects GetAffiliatedWebRealms() to be called with the
  // |expected_android_form|, and will cause the result callback supplied to
  // GetAffiliatedWebRealms() to be invoked with |results_to_return|.
  void ExpectCallToGetAffiliatedWebRealms(
      const autofill::PasswordForm& expected_android_form,
      const std::vector<std::string>& results_to_return) {
    EXPECT_CALL(*this, OnGetAffiliatedWebRealmsCalled(expected_android_form))
        .WillOnce(testing::Return(results_to_return));
  }

 private:
  MOCK_METHOD1(OnGetAffiliatedAndroidRealmsCalled,
               std::vector<std::string>(const PasswordForm&));
  MOCK_METHOD1(OnGetAffiliatedWebRealmsCalled,
               std::vector<std::string>(const PasswordForm&));

  void GetAffiliatedAndroidRealms(
      const autofill::PasswordForm& observed_form,
      const AffiliatedRealmsCallback& result_callback) override {
    std::vector<std::string> affiliated_android_realms =
        OnGetAffiliatedAndroidRealmsCalled(observed_form);
    result_callback.Run(affiliated_android_realms);
  }

  void GetAffiliatedWebRealms(
      const autofill::PasswordForm& android_form,
      const AffiliatedRealmsCallback& result_callback) override {
    std::vector<std::string> affiliated_web_realms =
        OnGetAffiliatedWebRealmsCalled(android_form);
    result_callback.Run(affiliated_web_realms);
  }

  DISALLOW_COPY_AND_ASSIGN(MockAffiliatedMatchHelper);
};

class StartSyncFlareMock {
 public:
  StartSyncFlareMock() {}
  ~StartSyncFlareMock() {}

  MOCK_METHOD1(StartSyncFlare, void(syncer::ModelType));
};

}  // namespace

class PasswordStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.path().Append(FILE_PATH_LITERAL("login_test"));
  }

  base::MessageLoopForUI message_loop_;
  base::ScopedTempDir temp_dir_;
};

ACTION(STLDeleteElements0) {
  STLDeleteContainerPointers(arg0.begin(), arg0.end());
}

TEST_F(PasswordStoreTest, IgnoreOldWwwGoogleLogins) {
  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(), base::MessageLoopProxy::current(),
      make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
  store->Init(syncer::SyncableService::StartSyncFlare());

  const time_t cutoff = 1325376000;  // 00:00 Jan 1 2012 UTC
  static const PasswordFormData form_data[] = {
    // A form on https://www.google.com/ older than the cutoff. Will be ignored.
    { PasswordForm::SCHEME_HTML,
      "https://www.google.com",
      "https://www.google.com/origin",
      "https://www.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value_1",
      L"",
      true, true, cutoff - 1 },
    // A form on https://www.google.com/ older than the cutoff. Will be ignored.
    { PasswordForm::SCHEME_HTML,
      "https://www.google.com",
      "https://www.google.com/origin",
      "https://www.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value_2",
      L"",
      true, true, cutoff - 1 },
    // A form on https://www.google.com/ newer than the cutoff.
    { PasswordForm::SCHEME_HTML,
      "https://www.google.com",
      "https://www.google.com/origin",
      "https://www.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value_3",
      L"",
      true, true, cutoff + 1 },
    // A form on https://accounts.google.com/ older than the cutoff.
    { PasswordForm::SCHEME_HTML,
      "https://accounts.google.com",
      "https://accounts.google.com/origin",
      "https://accounts.google.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"",
      true, true, cutoff - 1 },
    // A form on http://bar.example.com/ older than the cutoff.
    { PasswordForm::SCHEME_HTML,
      "http://bar.example.com",
      "http://bar.example.com/origin",
      "http://bar.example.com/action",
      L"submit_element",
      L"username_element",
      L"password_element",
      L"username_value",
      L"",
      true, false, cutoff - 1 },
  };

  // Build the forms vector and add the forms to the store.
  ScopedVector<PasswordForm> all_forms;
  for (size_t i = 0; i < arraysize(form_data); ++i) {
    all_forms.push_back(
        CreatePasswordFormFromDataForTesting(form_data[i]).Pass());
    store->AddLogin(*all_forms.back());
  }
  base::MessageLoop::current()->RunUntilIdle();

  // We expect to get back only the "recent" www.google.com login.
  // Theoretically these should never actually exist since there are no longer
  // any login forms on www.google.com to save, but we technically allow them.
  // We should not get back the older saved password though.
  PasswordForm www_google;
  www_google.scheme = PasswordForm::SCHEME_HTML;
  www_google.signon_realm = "https://www.google.com";
  std::vector<PasswordForm*> www_google_expected;
  www_google_expected.push_back(all_forms[2]);

  // We should still get the accounts.google.com login even though it's older
  // than our cutoff - this is the new location of all Google login forms.
  PasswordForm accounts_google;
  accounts_google.scheme = PasswordForm::SCHEME_HTML;
  accounts_google.signon_realm = "https://accounts.google.com";
  std::vector<PasswordForm*> accounts_google_expected;
  accounts_google_expected.push_back(all_forms[3]);

  // Same thing for a generic saved login.
  PasswordForm bar_example;
  bar_example.scheme = PasswordForm::SCHEME_HTML;
  bar_example.signon_realm = "http://bar.example.com";
  std::vector<PasswordForm*> bar_example_expected;
  bar_example_expected.push_back(all_forms[4]);

  MockPasswordStoreConsumer consumer;
  testing::InSequence s;
  EXPECT_CALL(consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(www_google_expected)))
      .RetiresOnSaturation();
  EXPECT_CALL(consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(accounts_google_expected)))
      .RetiresOnSaturation();
  EXPECT_CALL(consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(bar_example_expected)))
      .RetiresOnSaturation();

  store->GetLogins(www_google, PasswordStore::ALLOW_PROMPT, &consumer);
  store->GetLogins(accounts_google, PasswordStore::ALLOW_PROMPT, &consumer);
  store->GetLogins(bar_example, PasswordStore::ALLOW_PROMPT, &consumer);

  base::MessageLoop::current()->RunUntilIdle();

  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(PasswordStoreTest, StartSyncFlare) {
  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(), base::MessageLoopProxy::current(),
      make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
  StartSyncFlareMock mock;
  store->Init(
      base::Bind(&StartSyncFlareMock::StartSyncFlare, base::Unretained(&mock)));
  {
    PasswordForm form;
    form.origin = GURL("http://accounts.google.com/LoginAuth");
    form.signon_realm = "http://accounts.google.com/";
    EXPECT_CALL(mock, StartSyncFlare(syncer::PASSWORDS));
    store->AddLogin(form);
    base::MessageLoop::current()->RunUntilIdle();
  }
  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(PasswordStoreTest, GetLoginImpl) {
  /* clang-format off */
  static const PasswordFormData kTestCredential = {
      PasswordForm::SCHEME_HTML,
      kTestWebRealm1,
      kTestWebOrigin1,
      "", L"", L"username_element",  L"password_element",
      L"username_value",
      L"", true, true, 1};
  /* clang-format on */

  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::ThreadTaskRunnerHandle::Get(), base::ThreadTaskRunnerHandle::Get(),
      make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
  store->Init(syncer::SyncableService::StartSyncFlare());

  // For each attribute in the primary key, create one form that mismatches on
  // that attribute.
  scoped_ptr<PasswordForm> test_form(
      CreatePasswordFormFromDataForTesting(kTestCredential));
  scoped_ptr<PasswordForm> mismatching_form_1(new PasswordForm(*test_form));
  mismatching_form_1->signon_realm = kTestPSLMatchingWebRealm;
  scoped_ptr<PasswordForm> mismatching_form_2(new PasswordForm(*test_form));
  mismatching_form_2->origin = GURL(kTestPSLMatchingWebOrigin);
  scoped_ptr<PasswordForm> mismatching_form_3(new PasswordForm(*test_form));
  mismatching_form_3->username_element = base::ASCIIToUTF16("other_element");
  scoped_ptr<PasswordForm> mismatching_form_4(new PasswordForm(*test_form));
  mismatching_form_4->password_element = base::ASCIIToUTF16("other_element");
  scoped_ptr<PasswordForm> mismatching_form_5(new PasswordForm(*test_form));
  mismatching_form_5->username_value =
      base::ASCIIToUTF16("other_username_value");

  store->AddLogin(*mismatching_form_1);
  store->AddLogin(*mismatching_form_2);
  store->AddLogin(*mismatching_form_3);
  store->AddLogin(*mismatching_form_4);
  store->AddLogin(*mismatching_form_5);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(store->GetLoginImpl(*test_form));

  store->AddLogin(*test_form);
  base::MessageLoop::current()->RunUntilIdle();
  scoped_ptr<PasswordForm> returned_form = store->GetLoginImpl(*test_form);
  ASSERT_TRUE(returned_form);
  EXPECT_EQ(*test_form, *returned_form);

  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

// When no Android applications are actually affiliated with the realm of the
// observed form, GetLoginsWithAffiliations() should still return the exact and
// PSL matching results, but not any stored Android credentials.
TEST_F(PasswordStoreTest, GetLoginsWithoutAffiliations) {
  /* clang-format off */
  static const PasswordFormData kTestCredentials[] = {
      // Credential that is an exact match of the observed form.
      {PasswordForm::SCHEME_HTML,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", L"", L"",  L"",
       L"username_value_1",
       L"", true, true, 1},
      // Credential that is a PSL match of the observed form.
      {PasswordForm::SCHEME_HTML,
       kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin,
       "", L"", L"",  L"",
       L"username_value_2",
       L"", true, true, 1},
      // Credential for an unrelated Android application.
      {PasswordForm::SCHEME_HTML,
       kTestUnrelatedAndroidRealm,
       "", "", L"", L"", L"",
       L"username_value_3",
       L"", true, true, 1}};
  /* clang-format on */

  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(), base::MessageLoopProxy::current(),
      make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
  store->Init(syncer::SyncableService::StartSyncFlare());

  MockAffiliatedMatchHelper* mock_helper = new MockAffiliatedMatchHelper;
  store->SetAffiliatedMatchHelper(make_scoped_ptr(mock_helper));

  ScopedVector<PasswordForm> all_credentials;
  for (size_t i = 0; i < arraysize(kTestCredentials); ++i) {
    all_credentials.push_back(
        CreatePasswordFormFromDataForTesting(kTestCredentials[i]));
    store->AddLogin(*all_credentials.back());
    base::MessageLoop::current()->RunUntilIdle();
  }

  PasswordForm observed_form;
  observed_form.scheme = PasswordForm::SCHEME_HTML;
  observed_form.origin = GURL(kTestWebOrigin1);
  observed_form.ssl_valid = true;
  observed_form.signon_realm = kTestWebRealm1;

  MockPasswordStoreConsumer mock_consumer;
  ScopedVector<PasswordForm> expected_results;
  expected_results.push_back(new PasswordForm(*all_credentials[0]));
  expected_results.push_back(new PasswordForm(*all_credentials[1]));
  for (PasswordForm* result : expected_results) {
    if (result->signon_realm == observed_form.signon_realm)
      continue;
    result->original_signon_realm = result->signon_realm;
    result->origin = observed_form.origin;
    result->signon_realm = observed_form.signon_realm;
  }

  std::vector<std::string> no_affiliated_android_realms;
  mock_helper->ExpectCallToGetAffiliatedAndroidRealms(
      observed_form, no_affiliated_android_realms);

  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(expected_results.get())));
  store->GetLogins(observed_form, PasswordStore::ALLOW_PROMPT, &mock_consumer);
  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

// There are 3 Android applications affiliated with the realm of the observed
// form, with the PasswordStore having credentials for two of these (even two
// credentials for one). GetLoginsWithAffiliations() should return the exact,
// and PSL matching credentials, and the credentials for these two Android
// applications, but not for the unaffiliated Android application.
TEST_F(PasswordStoreTest, GetLoginsWithAffiliations) {
  /* clang-format off */
  static const PasswordFormData kTestCredentials[] = {
      // Credential that is an exact match of the observed form.
      {PasswordForm::SCHEME_HTML,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", L"", L"",  L"",
       L"username_value_1",
       L"", true, true, 1},
      // Credential that is a PSL match of the observed form.
      {PasswordForm::SCHEME_HTML,
       kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin,
       "", L"", L"",  L"",
       L"username_value_2",
       L"", true, true, 1},
      // Credential for an Android application affiliated with the realm of the
      // observed from.
      {PasswordForm::SCHEME_HTML,
       kTestAndroidRealm1,
       "", "", L"", L"", L"",
       L"username_value_3",
       L"", true, true, 1},
      // Second credential for the same Android application.
      {PasswordForm::SCHEME_HTML,
       kTestAndroidRealm1,
       "", "", L"", L"", L"",
       L"username_value_3b",
       L"", true, true, 1},
      // Credential for another Android application affiliated with the realm
      // of the observed from.
      {PasswordForm::SCHEME_HTML,
       kTestAndroidRealm2,
       "", "", L"", L"", L"",
       L"username_value_4",
       L"", true, true, 1},
      // Credential for an unrelated Android application.
      {PasswordForm::SCHEME_HTML,
       kTestUnrelatedAndroidRealm,
       "", "", L"", L"", L"",
       L"username_value_5",
       L"", true, true, 1}};
  /* clang-format on */

  scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
      base::MessageLoopProxy::current(), base::MessageLoopProxy::current(),
      make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
  store->Init(syncer::SyncableService::StartSyncFlare());

  MockAffiliatedMatchHelper* mock_helper = new MockAffiliatedMatchHelper;
  store->SetAffiliatedMatchHelper(make_scoped_ptr(mock_helper));

  ScopedVector<PasswordForm> all_credentials;
  for (size_t i = 0; i < arraysize(kTestCredentials); ++i) {
    all_credentials.push_back(
        CreatePasswordFormFromDataForTesting(kTestCredentials[i]));
    store->AddLogin(*all_credentials.back());
    base::MessageLoop::current()->RunUntilIdle();
  }

  PasswordForm observed_form;
  observed_form.scheme = PasswordForm::SCHEME_HTML;
  observed_form.origin = GURL(kTestWebOrigin1);
  observed_form.ssl_valid = true;
  observed_form.signon_realm = kTestWebRealm1;

  MockPasswordStoreConsumer mock_consumer;
  ScopedVector<PasswordForm> expected_results;
  expected_results.push_back(new PasswordForm(*all_credentials[0]));
  expected_results.push_back(new PasswordForm(*all_credentials[1]));
  expected_results.push_back(new PasswordForm(*all_credentials[2]));
  expected_results.push_back(new PasswordForm(*all_credentials[3]));
  expected_results.push_back(new PasswordForm(*all_credentials[4]));
  for (PasswordForm* result : expected_results) {
    if (result->signon_realm == observed_form.signon_realm)
      continue;
    result->original_signon_realm = result->signon_realm;
    result->signon_realm = observed_form.signon_realm;
    result->origin = observed_form.origin;
  }

  std::vector<std::string> affiliated_android_realms;
  affiliated_android_realms.push_back(kTestAndroidRealm1);
  affiliated_android_realms.push_back(kTestAndroidRealm2);
  affiliated_android_realms.push_back(kTestAndroidRealm3);
  mock_helper->ExpectCallToGetAffiliatedAndroidRealms(
      observed_form, affiliated_android_realms);

  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsConstRef(
                  UnorderedPasswordFormElementsAre(expected_results.get())));

  store->GetLogins(observed_form, PasswordStore::ALLOW_PROMPT, &mock_consumer);
  store->Shutdown();
  base::MessageLoop::current()->RunUntilIdle();
}

// This test must use passwords, which are not stored on Mac, therefore the test
// is disabled on Mac. This should not be a huge issue as functionality in the
// platform-independent base class is tested. See also the file-level comment.
#if defined(OS_MACOSX)
#define MAYBE_UpdatePasswordsStoredForAffiliatedWebsites \
  DISABLED_UpdatePasswordsStoredForAffiliatedWebsites
#else
#define MAYBE_UpdatePasswordsStoredForAffiliatedWebsites \
  UpdatePasswordsStoredForAffiliatedWebsites
#endif

// When the password stored for an Android application is updated, credentials
// with the same username stored for affiliated web sites should also be updated
// automatically.
TEST_F(PasswordStoreTest, MAYBE_UpdatePasswordsStoredForAffiliatedWebsites) {
  const wchar_t kTestUsername[] = L"username_value_1";
  const wchar_t kTestOtherUsername[] = L"username_value_2";
  const wchar_t kTestOldPassword[] = L"old_password_value";
  const wchar_t kTestNewPassword[] = L"new_password_value";
  const wchar_t kTestOtherPassword[] = L"other_password_value";

  /* clang-format off */
  static const PasswordFormData kTestCredentials[] = {
      // The credential for the Android application that will be updated
      // explicitly so it can be tested if the changes are correctly propagated
      // to affiliated Web credentials.
      {PasswordForm::SCHEME_HTML,
       kTestAndroidRealm1,
       "", "", L"", L"", L"",
       kTestUsername,
       kTestOldPassword, true, true, 2},

      // --- Positive samples --- Credentials that the password update should be
      // automatically propagated to.

      // Credential for an affiliated web site with the same username.
      {PasswordForm::SCHEME_HTML,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", L"", L"",  L"",
       kTestUsername,
       kTestOldPassword, true, true, 1},
      // Credential for another affiliated web site with the same username.
      // Although the password is different than the current/old password for
      // the Android application, it should be updated regardless.
      {PasswordForm::SCHEME_HTML,
       kTestWebRealm2,
       kTestWebOrigin2,
       "", L"", L"",  L"",
       kTestUsername,
       kTestOtherPassword, true, true, 1},

      // --- Negative samples --- Credentials that the password update should
      // not be propagated to.

      // Credential for another affiliated web site, but one that already has
      // the new password.
      {PasswordForm::SCHEME_HTML,
       kTestWebRealm3,
       kTestWebOrigin3,
       "", L"", L"",  L"",
       kTestUsername,
       kTestNewPassword, true, true, 1},
      // Credential for another affiliated web site, but one that was saved
      // under insecure conditions.
      {PasswordForm::SCHEME_HTML,
       kTestWebRealm4,
       kTestWebOrigin4,
       "", L"", L"",  L"",
       kTestUsername,
       kTestOldPassword, true, false, 1},
      // Credential for the HTTP version of an affiliated web site.
      {PasswordForm::SCHEME_HTML,
       kTestInsecureWebRealm,
       kTestInsecureWebOrigin,
       "", L"", L"",  L"",
       kTestUsername,
       kTestOldPassword, true, false, 1},
      // Credential for an affiliated web site, but with a different username.
      {PasswordForm::SCHEME_HTML,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", L"", L"",  L"",
       kTestOtherUsername,
       kTestOldPassword, true, true, 1},
      // Credential for a web site that is a PSL match to a web sites affiliated
      // with the Android application.
      {PasswordForm::SCHEME_HTML,
       kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin,
       "poisoned", L"poisoned", L"",  L"",
       kTestUsername,
       kTestOldPassword, true, true, 1},
      // Credential for an unrelated web site.
      {PasswordForm::SCHEME_HTML,
       kTestUnrelatedWebRealm,
       kTestUnrelatedWebOrigin,
       "", L"", L"",  L"",
       kTestUsername,
       kTestOldPassword, true, true, 1},
      // Credential for an affiliated Android application.
      {PasswordForm::SCHEME_HTML,
       kTestAndroidRealm2,
       "", "", L"", L"", L"",
       kTestUsername,
       kTestOldPassword, true, true, 1},
      // Credential for an unrelated Android application.
      {PasswordForm::SCHEME_HTML,
       kTestUnrelatedAndroidRealm,
       "", "", L"", L"", L"",
       kTestUsername,
       kTestOldPassword, true, true, 1},
      // Credential for an affiliated web site with the same username, but one
      // that was updated at the same time via Sync as the Android credential.
      {PasswordForm::SCHEME_HTML,
       kTestWebRealm5,
       kTestWebOrigin5,
       "", L"", L"",  L"",
       kTestUsername,
       kTestOtherPassword, true, true, 2}};
  /* clang-format on */

  // The number of positive samples in |kTestCredentials|.
  const size_t kExpectedNumberOfPropagatedUpdates = 2u;

  const bool kFalseTrue[] = {false, true};
  for (bool propagation_enabled : kFalseTrue) {
    for (bool test_remove_and_add_login : kFalseTrue) {
      SCOPED_TRACE(testing::Message("propagation_enabled: ")
                   << propagation_enabled);
      SCOPED_TRACE(testing::Message("test_remove_and_add_login: ")
                   << test_remove_and_add_login);

      scoped_refptr<PasswordStoreDefault> store(new PasswordStoreDefault(
          base::ThreadTaskRunnerHandle::Get(),
          base::ThreadTaskRunnerHandle::Get(),
          make_scoped_ptr(new LoginDatabase(test_login_db_file_path()))));
      store->Init(syncer::SyncableService::StartSyncFlare());
      store->RemoveLoginsCreatedBetween(base::Time(), base::Time::Max());

      // Set up the initial test data set.
      ScopedVector<PasswordForm> all_credentials;
      for (size_t i = 0; i < arraysize(kTestCredentials); ++i) {
        all_credentials.push_back(
            CreatePasswordFormFromDataForTesting(kTestCredentials[i]));
        all_credentials.back()->date_synced =
            all_credentials.back()->date_created;
        store->AddLogin(*all_credentials.back());
        base::MessageLoop::current()->RunUntilIdle();
      }

      // The helper must be injected after the initial test data is set up,
      // otherwise it will already start propagating updates as new Android
      // credentials are added.
      MockAffiliatedMatchHelper* mock_helper = new MockAffiliatedMatchHelper;
      store->SetAffiliatedMatchHelper(make_scoped_ptr(mock_helper));
      store->enable_propagating_password_changes_to_web_credentials(
          propagation_enabled);

      // Calculate how the correctly updated test data set should look like.
      size_t expected_number_of_propageted_updates =
          propagation_enabled ? kExpectedNumberOfPropagatedUpdates : 0u;
      ScopedVector<PasswordForm> expected_credentials_after_update;
      for (size_t i = 0; i < all_credentials.size(); ++i) {
        expected_credentials_after_update.push_back(
            new autofill::PasswordForm(*all_credentials[i]));
        if (i < 1 + expected_number_of_propageted_updates) {
          expected_credentials_after_update.back()->password_value =
              base::WideToUTF16(kTestNewPassword);
        }
      }

      if (propagation_enabled) {
        std::vector<std::string> affiliated_web_realms;
        affiliated_web_realms.push_back(kTestWebRealm1);
        affiliated_web_realms.push_back(kTestWebRealm2);
        affiliated_web_realms.push_back(kTestWebRealm3);
        affiliated_web_realms.push_back(kTestWebRealm4);
        affiliated_web_realms.push_back(kTestWebRealm5);
        mock_helper->ExpectCallToGetAffiliatedWebRealms(
            *expected_credentials_after_update[0], affiliated_web_realms);
      }

      // Explicitly update the Android credential, wait until things calm down,
      // then query all passwords and expect that:
      //   1.) The positive samples in |kTestCredentials| have the new password,
      //       but the negative samples do not.
      //   2.) Change notifications are sent about the updates. Note that as the
      //       test interacts with the (Update|Add)LoginSync methods, only the
      //       derived changes should trigger notifications, the first one would
      //       normally be trigger by Sync.
      MockPasswordStoreObserver mock_observer;
      store->AddObserver(&mock_observer);
      if (propagation_enabled) {
        EXPECT_CALL(mock_observer, OnLoginsChanged(testing::SizeIs(
                                       expected_number_of_propageted_updates)));
      }
      if (test_remove_and_add_login) {
        store->RemoveLoginSync(*all_credentials[0]);
        store->AddLoginSync(*expected_credentials_after_update[0]);
      } else {
        store->UpdateLoginSync(*expected_credentials_after_update[0]);
      }
      base::MessageLoop::current()->RunUntilIdle();
      store->RemoveObserver(&mock_observer);

      MockPasswordStoreConsumer mock_consumer;
      EXPECT_CALL(
          mock_consumer,
          OnGetPasswordStoreResultsConstRef(UnorderedPasswordFormElementsAre(
              expected_credentials_after_update.get())));
      store->GetAutofillableLogins(&mock_consumer);
      store->Shutdown();
      base::MessageLoop::current()->RunUntilIdle();
    }
  }
}

}  // namespace password_manager
