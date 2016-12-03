// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_settings_migration_experiment.h"

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPasswordManagerSettingMigrationFieldTrialName[] =
    "PasswordManagerSettingsMigration";
const char kEnabledPasswordManagerSettingsMigrationGroupName[] = "Enable";
const char kDisablePasswordManagerSettingsMigrationGroupName[] = "Disable";

const char kPasswordManagerSettingsBehaviourChangeFieldTrialName[] =
    "PasswordManagerSettingsBehaviourChange";
const char kPasswordManagerSettingsBehaviourChangeEnabledGroupName[] =
    "PasswordManagerSettingsBehaviourChange.Active";
const char kPasswordManagerSettingsBehaviourChangeDisabledGroupName[] =
    "PasswordManagerSettingsBehaviourChange.NotActive";

}  // namespace

namespace password_manager {

class PasswordManagerSettingsMigrationExperimentTest : public testing::Test {
 public:
  PasswordManagerSettingsMigrationExperimentTest()
      : field_trial_list_(nullptr) {}

  void EnforcePasswordManagerSettingMigrationExperimentGroup(const char* name) {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        kPasswordManagerSettingMigrationFieldTrialName, name));
  }

  void EnforcePasswordManagerSettingsBehaviourChangeExperimentGroup(
      const char* name) {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        kPasswordManagerSettingsBehaviourChangeFieldTrialName, name));
  }

 protected:
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerSettingsMigrationExperimentTest);
};

TEST_F(PasswordManagerSettingsMigrationExperimentTest, IsSettingsMigrationOn) {
  EnforcePasswordManagerSettingMigrationExperimentGroup(
      kEnabledPasswordManagerSettingsMigrationGroupName);
  EXPECT_TRUE(IsSettingsMigrationActive());
}

TEST_F(PasswordManagerSettingsMigrationExperimentTest, IsSettingsMigrationOff) {
  EnforcePasswordManagerSettingsBehaviourChangeExperimentGroup(
      kDisablePasswordManagerSettingsMigrationGroupName);
  EXPECT_FALSE(IsSettingsMigrationActive());
}

TEST_F(PasswordManagerSettingsMigrationExperimentTest,
       IsBehaviourChangeEnabled) {
  EnforcePasswordManagerSettingsBehaviourChangeExperimentGroup(
      kPasswordManagerSettingsBehaviourChangeEnabledGroupName);
  EXPECT_TRUE(IsSettingsBehaviorChangeActive());
}

TEST_F(PasswordManagerSettingsMigrationExperimentTest,
       IsBehaviourChangeDisabled) {
  EnforcePasswordManagerSettingsBehaviourChangeExperimentGroup(
      kPasswordManagerSettingsBehaviourChangeDisabledGroupName);
  EXPECT_FALSE(IsSettingsBehaviorChangeActive());
}

}  // namespace password_manager
