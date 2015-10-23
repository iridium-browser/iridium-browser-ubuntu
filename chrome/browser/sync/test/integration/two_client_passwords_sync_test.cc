// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(shadi): create a common macro for end-to-end tests that need to be
// disabled in regular bots.
#define E2E_ONLY(x) DISABLED_E2ETest##x

#include "base/guid.h"
#include "base/hash.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

using passwords_helper::AddLogin;
using passwords_helper::AllProfilesContainSamePasswordForms;
using passwords_helper::AllProfilesContainSamePasswordFormsAsVerifier;
using passwords_helper::AwaitAllProfilesContainSamePasswordForms;
using passwords_helper::AwaitProfileContainsSamePasswordFormsAsVerifier;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;
using passwords_helper::RemoveLogin;
using passwords_helper::RemoveLogins;
using passwords_helper::SetDecryptionPassphrase;
using passwords_helper::SetEncryptionPassphrase;
using passwords_helper::UpdateLogin;
using sync_integration_test_util::AwaitPassphraseAccepted;
using sync_integration_test_util::AwaitPassphraseRequired;

using autofill::PasswordForm;

static const char* kValidPassphrase = "passphrase!";

class TwoClientPasswordsSyncTest : public SyncTest {
 public:
  TwoClientPasswordsSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientPasswordsSyncTest() override {}

  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientPasswordsSyncTest);
};

// TCM ID - 3732277
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Add) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);

  PasswordForm form1 = form0;
  form1.password_value = base::ASCIIToUTF16("new_password");
  AddLogin(GetPasswordStore(1), form1);

  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       SetPassphraseAndAddPassword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetEncryptionPassphrase(0, kValidPassphrase, ProfileSyncService::EXPLICIT);
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService((0))));

  ASSERT_TRUE(AwaitPassphraseRequired(GetSyncService((1))));
  ASSERT_TRUE(SetDecryptionPassphrase(1, kValidPassphrase));
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService((1))));

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
}

// TCM ID - 4603879
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Update) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(AwaitProfileContainsSamePasswordFormsAsVerifier(1));

  form.password_value = base::ASCIIToUTF16("new_password");
  UpdateLogin(GetVerifierPasswordStore(), form);
  UpdateLogin(GetPasswordStore(1), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());

  // Wait for client 1 to commit and client 0 to receive the update.
  ASSERT_TRUE(AwaitProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

// TCM ID - 3719309
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetVerifierPasswordStore(), form1);
  AddLogin(GetPasswordStore(0), form1);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(AwaitProfileContainsSamePasswordFormsAsVerifier(1));

  RemoveLogin(GetPasswordStore(1), form0);
  RemoveLogin(GetVerifierPasswordStore(), form0);
  ASSERT_EQ(1, GetVerifierPasswordCount());

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(AwaitProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, E2E_ONLY(Delete)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(base::Hash(base::GenerateGUID()));
  PasswordForm form1 = CreateTestPasswordForm(base::Hash(base::GenerateGUID()));
  AddLogin(GetPasswordStore(0), form0);
  AddLogin(GetPasswordStore(0), form1);

  const int init_password_count = GetPasswordCount(0);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
  ASSERT_EQ(init_password_count, GetPasswordCount(1));

  RemoveLogin(GetPasswordStore(1), form0);

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
  ASSERT_EQ(init_password_count - 1, GetPasswordCount(0));

  RemoveLogin(GetPasswordStore(1), form1);

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
  ASSERT_EQ(init_password_count - 2, GetPasswordCount(0));
}

// TCM ID - 7573511
// Flaky on Mac and Windows: http://crbug.com/111399
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_DeleteAll DISABLED_DeleteAll
#else
#define MAYBE_DeleteAll DeleteAll
#endif
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, MAYBE_DeleteAll) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetVerifierPasswordStore(), form1);
  AddLogin(GetPasswordStore(0), form1);
  ASSERT_TRUE(AwaitProfileContainsSamePasswordFormsAsVerifier(1));
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  RemoveLogins(GetPasswordStore(1));
  RemoveLogins(GetVerifierPasswordStore());
  ASSERT_TRUE(AwaitProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  ASSERT_EQ(0, GetVerifierPasswordCount());
}

// TCM ID - 3694311
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Merge) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetPasswordStore(1), form1);
  PasswordForm form2 = CreateTestPasswordForm(2);
  AddLogin(GetPasswordStore(1), form2);

  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
  ASSERT_EQ(3, GetPasswordCount(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, E2E_ONLY(TwoClientAddPass)) {
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
  // All profiles should sync same passwords.
  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms()) <<
      "Initial password forms did not match for all profiles";
  const int init_password_count = GetPasswordCount(0);

  // Add one new password per profile. A unique form is created for each to
  // prevent them from overwriting each other.
  for (int i = 0; i < num_clients(); ++i) {
    AddLogin(GetPasswordStore(i),
             CreateTestPasswordForm(base::RandInt(0, kint32max)));
  }

  // Blocks and waits for password forms in all profiles to match.
  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());

  // Check that total number of passwords is as expected.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(GetPasswordCount(i), init_password_count + num_clients()) <<
        "Total password count is wrong.";
  }
}
