// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service.h"

using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace arc {

namespace {

bool AllProfilesHaveSameArcPackageDetails() {
  return SyncArcPackageHelper::GetInstance()
      ->AllProfilesHaveSamePackageDetails();
}

}  // namespace

class SingleClientArcPackageSyncTest : public SyncTest {
 public:
  SingleClientArcPackageSyncTest()
      : SyncTest(SINGLE_CLIENT), sync_helper_(nullptr) {}

  ~SingleClientArcPackageSyncTest() override {}

  bool SetupClients() override {
    if (!SyncTest::SetupClients())
      return false;

    // Init SyncArcPackageHelper to ensure that the arc services are initialized
    // for each Profile.
    sync_helper_ = SyncArcPackageHelper::GetInstance();
    return sync_helper_ != nullptr;
  }

  void TearDownOnMainThread() override {
    sync_helper_->CleanUp();
    sync_helper_ = nullptr;
    SyncTest::TearDownOnMainThread();
  }

  SyncArcPackageHelper* sync_helper() { return sync_helper_; }

 private:
  SyncArcPackageHelper* sync_helper_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientArcPackageSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientArcPackageSyncTest, ArcPackageEmpty) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());

}

IN_PROC_BROWSER_TEST_F(SingleClientArcPackageSyncTest,
                       ArcPackageInstallSomePackages) {
  ASSERT_TRUE(SetupSync());

  constexpr size_t kNumPackages = 5;
  for (size_t i = 0; i < kNumPackages; ++i) {
    sync_helper()->InstallPackageWithIndex(GetProfile(0), i);
    sync_helper()->InstallPackageWithIndex(verifier(), i);
  }

  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(AllProfilesHaveSameArcPackageDetails());
}

}  // namespace arc
