// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Property;
using base::ASCIIToUTF16;

namespace {

// Some guids for testing.
const char kGuid1[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
const char kGuid2[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";
const char kGuid3[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44D";
const char kGuid4[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44E";
const char kHttpOrigin[] = "http://www.example.com/";
const char kHttpsOrigin[] = "https://www.example.com/";
const char kSettingsOrigin[] = "Chrome settings";

class MockAutofillProfileSyncableService
    : public AutofillProfileSyncableService {
 public:
  MockAutofillProfileSyncableService() {}
  virtual ~MockAutofillProfileSyncableService() {}

  using AutofillProfileSyncableService::DataBundle;
  using AutofillProfileSyncableService::set_sync_processor;
  using AutofillProfileSyncableService::CreateData;

  MOCK_METHOD1(LoadAutofillData, bool(std::vector<AutofillProfile*>*));
  MOCK_METHOD1(SaveChangesToWebData,
               bool(const AutofillProfileSyncableService::DataBundle&));
};

ACTION_P(CopyData, data) {
  arg0->resize(data->size());
  std::copy(data->begin(), data->end(), arg0->begin());
}

MATCHER_P(CheckSyncChanges, n_sync_changes_list, "") {
  if (arg.size() != n_sync_changes_list.size())
    return false;
  syncer::SyncChangeList::const_iterator passed, expected;
  for (passed = arg.begin(), expected = n_sync_changes_list.begin();
       passed != arg.end() && expected != n_sync_changes_list.end();
       ++passed, ++expected) {
    DCHECK(passed->IsValid());
    if (passed->change_type() != expected->change_type())
      return false;
    if (passed->sync_data().GetSpecifics().SerializeAsString() !=
            expected->sync_data().GetSpecifics().SerializeAsString()) {
      return false;
    }
  }
  return true;
}

MATCHER_P(DataBundleCheck, n_bundle, "") {
  if ((arg.profiles_to_delete.size() != n_bundle.profiles_to_delete.size()) ||
      (arg.profiles_to_update.size() != n_bundle.profiles_to_update.size()) ||
      (arg.profiles_to_add.size() != n_bundle.profiles_to_add.size()))
    return false;
  for (size_t i = 0; i < arg.profiles_to_delete.size(); ++i) {
    if (arg.profiles_to_delete[i] != n_bundle.profiles_to_delete[i])
      return false;
  }
  for (size_t i = 0; i < arg.profiles_to_update.size(); ++i) {
    if (*arg.profiles_to_update[i] != *n_bundle.profiles_to_update[i])
      return false;
  }
  for (size_t i = 0; i < arg.profiles_to_add.size(); ++i) {
    if (*arg.profiles_to_add[i] != *n_bundle.profiles_to_add[i])
      return false;
  }
  return true;
}

class MockSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() {}
  ~MockSyncChangeProcessor() override {}

  MOCK_METHOD2(ProcessSyncChanges,
               syncer::SyncError(const tracked_objects::Location&,
                                 const syncer::SyncChangeList&));
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return syncer::SyncDataList();
  }
};

class TestSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestSyncChangeProcessor() {}
  ~TestSyncChangeProcessor() override {}

  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& location,
      const syncer::SyncChangeList& changes) override {
    changes_ = changes;
    return syncer::SyncError();
  }

  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return syncer::SyncDataList();
  }

  const syncer::SyncChangeList& changes() { return changes_; }

 private:
  syncer::SyncChangeList changes_;
};

// Returns a profile with all fields set.  Contains identical data to the data
// returned from ConstructCompleteSyncData().
scoped_ptr<AutofillProfile> ConstructCompleteProfile() {
  scoped_ptr<AutofillProfile> profile(
      new AutofillProfile(kGuid1, kHttpsOrigin));

  profile->set_use_count(7);
  profile->set_use_date(base::Time::FromTimeT(1423182152));

  profile->SetRawInfo(NAME_FULL, ASCIIToUTF16("John K. Doe, Jr."));
  profile->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile->SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("K."));
  profile->SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));

  profile->SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("user@example.com"));
  profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1.800.555.1234"));

  profile->SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                      ASCIIToUTF16("123 Fake St.\n"
                                   "Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Fake St."),
            profile->GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), profile->GetRawInfo(ADDRESS_HOME_LINE2));

  profile->SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Google, Inc."));
  profile->SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"));
  profile->SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"));
  profile->SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"));
  profile->SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  profile->SetRawInfo(ADDRESS_HOME_SORTING_CODE, ASCIIToUTF16("CEDEX"));
  profile->SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                      ASCIIToUTF16("Santa Clara"));
  profile->set_language_code("en");
  return profile.Pass();
}

// Returns SyncData with all Autofill profile fields set.  Contains identical
// data to the data returned from ConstructCompleteProfile().
syncer::SyncData ConstructCompleteSyncData() {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::AutofillProfileSpecifics* specifics =
      entity_specifics.mutable_autofill_profile();

  specifics->set_guid(kGuid1);
  specifics->set_origin(kHttpsOrigin);
  specifics->set_use_count(7);
  specifics->set_use_date(1423182152);

  specifics->add_name_first("John");
  specifics->add_name_middle("K.");
  specifics->add_name_last("Doe");
  specifics->add_name_full("John K. Doe, Jr.");

  specifics->add_email_address("user@example.com");

  specifics->add_phone_home_whole_number("1.800.555.1234");

  specifics->set_address_home_line1("123 Fake St.");
  specifics->set_address_home_line2("Apt. 42");
  specifics->set_address_home_street_address("123 Fake St.\n"
                                             "Apt. 42");

  specifics->set_company_name("Google, Inc.");
  specifics->set_address_home_city("Mountain View");
  specifics->set_address_home_state("California");
  specifics->set_address_home_zip("94043");
  specifics->set_address_home_country("US");
  specifics->set_address_home_sorting_code("CEDEX");
  specifics->set_address_home_dependent_locality("Santa Clara");
  specifics->set_address_home_language_code("en");

  return syncer::SyncData::CreateLocalData(kGuid1, kGuid1, entity_specifics);
}

}  // namespace

class AutofillProfileSyncableServiceTest : public testing::Test {
 public:
  AutofillProfileSyncableServiceTest() {}

  void SetUp() override { sync_processor_.reset(new MockSyncChangeProcessor); }

  // Wrapper around AutofillProfileSyncableService::MergeDataAndStartSyncing()
  // that also verifies expectations.
  void MergeDataAndStartSyncing(
      const std::vector<AutofillProfile*>& profiles_from_web_db,
      const syncer::SyncDataList& data_list,
      const MockAutofillProfileSyncableService::DataBundle& expected_bundle,
      const syncer::SyncChangeList& expected_change_list) {
    EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
        .Times(1)
        .WillOnce(DoAll(CopyData(&profiles_from_web_db), Return(true)));
    EXPECT_CALL(autofill_syncable_service_,
                SaveChangesToWebData(DataBundleCheck(expected_bundle)))
        .Times(1)
        .WillOnce(Return(true));
    if (expected_change_list.empty()) {
      EXPECT_CALL(*sync_processor_, ProcessSyncChanges(_, _)).Times(0);
    } else {
      ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
          .WillByDefault(Return(syncer::SyncError()));
      EXPECT_CALL(*sync_processor_,
                  ProcessSyncChanges(_, CheckSyncChanges(expected_change_list)))
          .Times(1)
          .WillOnce(Return(syncer::SyncError()));
    }

    // Takes ownership of sync_processor_.
    autofill_syncable_service_.MergeDataAndStartSyncing(
        syncer::AUTOFILL_PROFILE,
        data_list,
        sync_processor_.Pass(),
        scoped_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
  }

 protected:
  base::MessageLoop message_loop_;
  MockAutofillProfileSyncableService autofill_syncable_service_;
  scoped_ptr<MockSyncChangeProcessor> sync_processor_;
};

TEST_F(AutofillProfileSyncableServiceTest, MergeDataAndStartSyncing) {
  std::vector<AutofillProfile*> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;
  std::string origin_present1 = kHttpOrigin;
  std::string origin_present2 = std::string();
  std::string origin_synced1 = kHttpsOrigin;
  std::string origin_synced2 = kSettingsOrigin;

  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present1, origin_present1));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("1 1st st"));
  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present2, origin_present2));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("2 2nd st"));

  syncer::SyncDataList data_list;
  AutofillProfile profile1(guid_synced1, origin_synced1);
  profile1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2, origin_synced2);
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Harry"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));
  // This one will have the name and origin updated.
  AutofillProfile profile3(guid_present2, origin_synced2);
  profile3.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom Doe"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile3));

  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.front())));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_add.push_back(&profile1);
  expected_bundle.profiles_to_add.push_back(&profile2);
  expected_bundle.profiles_to_update.push_back(&profile3);

  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, MergeIdenticalProfiles) {
  std::vector<AutofillProfile*> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;
  std::string origin_present1 = kHttpOrigin;
  std::string origin_present2 = kSettingsOrigin;
  std::string origin_synced1 = kHttpsOrigin;
  std::string origin_synced2 = kHttpsOrigin;

  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present1, origin_present1));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("1 1st st"));
  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present2, origin_present2));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("2 2nd st"));

  // The synced profiles are identical to the local ones, except that the guids
  // are different.
  syncer::SyncDataList data_list;
  AutofillProfile profile1(guid_synced1, origin_synced1);
  profile1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile1.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1 1st st"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2, origin_synced2);
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("2 2nd st"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));

  AutofillProfile expected_profile(profile2);
  expected_profile.set_origin(kSettingsOrigin);
  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_UPDATE,
                         MockAutofillProfileSyncableService::CreateData(
                             expected_profile)));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present1);
  expected_bundle.profiles_to_delete.push_back(guid_present2);
  expected_bundle.profiles_to_add.push_back(&profile1);
  expected_bundle.profiles_to_add.push_back(&expected_profile);

  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, MergeSimilarProfiles) {
  std::vector<AutofillProfile*> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;
  std::string guid_synced1 = kGuid3;
  std::string guid_synced2 = kGuid4;
  std::string origin_present1 = kHttpOrigin;
  std::string origin_present2 = kSettingsOrigin;
  std::string origin_synced1 = kHttpsOrigin;
  std::string origin_synced2 = kHttpsOrigin;

  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present1, origin_present1));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("1 1st st"));
  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present2, origin_present2));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profiles_from_web_db.back()->SetRawInfo(ADDRESS_HOME_LINE1,
                                          ASCIIToUTF16("2 2nd st"));

  // The synced profiles are identical to the local ones, except that the guids
  // are different.
  syncer::SyncDataList data_list;
  AutofillProfile profile1(guid_synced1, origin_synced1);
  profile1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile1.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1 1st st"));
  profile1.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Frobbers, Inc."));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2, origin_synced2);
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Tom"));
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("2 2nd st"));
  profile2.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Fizzbang, LLC."));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));

  // The first profile should have its origin updated.
  // The second profile should remain as-is, because an unverified profile
  // should never overwrite a verified one.
  AutofillProfile expected_profile(profile1);
  expected_profile.set_origin(origin_present1);
  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.back())));
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_UPDATE,
                         MockAutofillProfileSyncableService::CreateData(
                             expected_profile)));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present1);
  expected_bundle.profiles_to_add.push_back(&expected_profile);
  expected_bundle.profiles_to_add.push_back(&profile2);

  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Ensure that no Sync events are generated to fill in missing origins from Sync
// with explicitly present empty ones.  This ensures that the migration to add
// origins to profiles does not generate lots of needless Sync updates.
TEST_F(AutofillProfileSyncableServiceTest, MergeDataEmptyOrigins) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Create a profile with an empty origin.
  AutofillProfile profile(kGuid1, std::string());
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1 1st st"));

  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Create a Sync profile identical to |profile|, except with no origin set.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_line1("1 1st st");
  EXPECT_FALSE(autofill_specifics->has_origin());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  syncer::SyncChangeList expected_change_list;
  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, GetAllSyncData) {
  std::vector<AutofillProfile*> profiles_from_web_db;
  std::string guid_present1 = kGuid1;
  std::string guid_present2 = kGuid2;

  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present1, kHttpOrigin));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.push_back(
      new AutofillProfile(guid_present2, kHttpsOrigin));
  profiles_from_web_db.back()->SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));

  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.front())));
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         MockAutofillProfileSyncableService::CreateData(
                             *profiles_from_web_db.back())));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  syncer::SyncDataList data_list;
  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);

  syncer::SyncDataList data =
      autofill_syncable_service_.GetAllSyncData(syncer::AUTOFILL_PROFILE);

  ASSERT_EQ(2U, data.size());
  EXPECT_EQ(guid_present1, data[0].GetSpecifics().autofill_profile().guid());
  EXPECT_EQ(guid_present2, data[1].GetSpecifics().autofill_profile().guid());
  EXPECT_EQ(kHttpOrigin, data[0].GetSpecifics().autofill_profile().origin());
  EXPECT_EQ(kHttpsOrigin, data[1].GetSpecifics().autofill_profile().origin());

  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, ProcessSyncChanges) {
  std::vector<AutofillProfile *> profiles_from_web_db;
  std::string guid_present = kGuid1;
  std::string guid_synced = kGuid2;

  syncer::SyncChangeList change_list;
  AutofillProfile profile(guid_synced, kHttpOrigin);
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  change_list.push_back(
      syncer::SyncChange(
          FROM_HERE,
          syncer::SyncChange::ACTION_ADD,
          MockAutofillProfileSyncableService::CreateData(profile)));
  AutofillProfile empty_profile(guid_present, kHttpsOrigin);
  change_list.push_back(
      syncer::SyncChange(
          FROM_HERE,
          syncer::SyncChange::ACTION_DELETE,
          MockAutofillProfileSyncableService::CreateData(empty_profile)));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present);
  expected_bundle.profiles_to_add.push_back(&profile);

  EXPECT_CALL(autofill_syncable_service_, SaveChangesToWebData(
              DataBundleCheck(expected_bundle)))
      .Times(1)
      .WillOnce(Return(true));

  autofill_syncable_service_.set_sync_processor(sync_processor_.release());
  syncer::SyncError error = autofill_syncable_service_.ProcessSyncChanges(
      FROM_HERE, change_list);

  EXPECT_FALSE(error.IsSet());
}

TEST_F(AutofillProfileSyncableServiceTest, AutofillProfileAdded) {
  // Will be owned by the syncable service.  Keep a reference available here for
  // verifying test expectations.
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuid1, &profile);
  autofill_syncable_service_.AutofillProfileChanged(change);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, result.change_type());

  sync_pb::AutofillProfileSpecifics specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(kGuid1, specifics.guid());
  EXPECT_EQ(kHttpsOrigin, specifics.origin());
  EXPECT_THAT(specifics.name_first(), testing::ElementsAre("Jane"));
}

TEST_F(AutofillProfileSyncableServiceTest, AutofillProfileDeleted) {
  // Will be owned by the syncable service.  Keep a reference available here for
  // verifying test expectations.
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  AutofillProfileChange change(AutofillProfileChange::REMOVE, kGuid2, NULL);
  autofill_syncable_service_.AutofillProfileChanged(change);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, result.change_type());
  sync_pb::AutofillProfileSpecifics specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(kGuid2, specifics.guid());
}

TEST_F(AutofillProfileSyncableServiceTest, UpdateField) {
  AutofillProfile profile(kGuid1, kSettingsOrigin);
  std::string company1 = "A Company";
  std::string company2 = "Another Company";
  profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16(company1));
  EXPECT_FALSE(AutofillProfileSyncableService::UpdateField(
      COMPANY_NAME, company1, &profile));
  EXPECT_EQ(profile.GetRawInfo(COMPANY_NAME), ASCIIToUTF16(company1));
  EXPECT_TRUE(AutofillProfileSyncableService::UpdateField(
      COMPANY_NAME, company2, &profile));
  EXPECT_EQ(profile.GetRawInfo(COMPANY_NAME), ASCIIToUTF16(company2));
  EXPECT_FALSE(AutofillProfileSyncableService::UpdateField(
      COMPANY_NAME, company2, &profile));
  EXPECT_EQ(profile.GetRawInfo(COMPANY_NAME), ASCIIToUTF16(company2));
}

TEST_F(AutofillProfileSyncableServiceTest, MergeProfile) {
  AutofillProfile profile1(kGuid1, kHttpOrigin);
  profile1.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("111 First St."));

  AutofillProfile profile2(kGuid2, kHttpsOrigin);
  profile2.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("111 First St."));

  profile1.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("1@1.com"));
  profile2.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("1@1.com"));

  profile1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));

  profile1.SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));
  profile2.SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));

  profile2.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("650234567"));

  profile1.set_language_code("en");

  EXPECT_FALSE(AutofillProfileSyncableService::MergeProfile(profile2, &profile1,
                                                            "en-US"));

  // The more recent use_date is maintained and synced back.
  profile2.set_use_date(base::Time::FromTimeT(30));
  profile1.set_use_date(base::Time::FromTimeT(25));
  EXPECT_FALSE(AutofillProfileSyncableService::MergeProfile(profile2, &profile1,
                                                            "en-US"));
  EXPECT_EQ(base::Time::FromTimeT(30), profile1.use_date());
  profile1.set_use_date(base::Time::FromTimeT(35));
  EXPECT_TRUE(AutofillProfileSyncableService::MergeProfile(profile2, &profile1,
                                                           "en-US"));
  EXPECT_EQ(base::Time::FromTimeT(35), profile1.use_date());

  EXPECT_EQ(ASCIIToUTF16("John"), profile1.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Doe"), profile1.GetRawInfo(NAME_LAST));
  EXPECT_EQ(ASCIIToUTF16("1@1.com"), profile1.GetRawInfo(EMAIL_ADDRESS));
  EXPECT_EQ(ASCIIToUTF16("650234567"),
            profile1.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  EXPECT_EQ(profile2.origin(), profile1.origin());

  AutofillProfile profile3(kGuid3, kHttpOrigin);
  profile3.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("111 First St."));
  profile3.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jane"));
  profile3.SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));

  EXPECT_TRUE(AutofillProfileSyncableService::MergeProfile(profile3,
                                                           &profile1,
                                                           "en-US"));

  EXPECT_EQ(ASCIIToUTF16("Jane"), profile1.GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Doe"), profile1.GetRawInfo(NAME_LAST));
  EXPECT_EQ(ASCIIToUTF16("1@1.com"), profile1.GetRawInfo(EMAIL_ADDRESS));
  EXPECT_EQ(ASCIIToUTF16("650234567"),
            profile1.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
}

// Ensure that all profile fields are able to be synced up from the client to
// the server.
TEST_F(AutofillProfileSyncableServiceTest, SyncAllFieldsToServer) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Create a profile with all fields set.
  profiles_from_web_db.push_back(ConstructCompleteProfile().release());

  // Set up expectations: No changes to the WebDB, and all fields correctly
  // copied to Sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  syncer::SyncChangeList expected_change_list;
  expected_change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         ConstructCompleteSyncData()));

  // Verify the expectations.
  syncer::SyncDataList data_list;
  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Ensure that all profile fields are able to be synced down from the server to
// the client.
TEST_F(AutofillProfileSyncableServiceTest, SyncAllFieldsToClient) {
  // Create a profile with all fields set.
  syncer::SyncDataList data_list;
  data_list.push_back(ConstructCompleteSyncData());

  // Set up expectations: All fields correctly copied to the WebDB, and no
  // changes propagated to Sync.
  syncer::SyncChangeList expected_change_list;
  scoped_ptr<AutofillProfile> expected_profile = ConstructCompleteProfile();
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_add.push_back(expected_profile.get());

  // Verify the expectations.
  std::vector<AutofillProfile*> profiles_from_web_db;
  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Ensure that the street address field takes precedence over the address line 1
// and line 2 fields, even though these are expected to always be in sync in
// practice.
TEST_F(AutofillProfileSyncableServiceTest,
       StreetAddressTakesPrecedenceOverAddressLines) {
  // Create a Sync profile with conflicting address data in the street address
  // field vs. the address line 1 and address line 2 fields.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(kGuid1);
  autofill_specifics->set_origin(kHttpsOrigin);
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_line1("123 Example St.");
  autofill_specifics->set_address_home_line2("Apt. 42");
  autofill_specifics->set_address_home_street_address("456 El Camino Real\n"
                                                      "Suite #1337");

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(kGuid1, kGuid1, specifics));

  // Set up expectations: Full street address takes precedence over address
  // lines.
  syncer::SyncChangeList expected_change_list;
  AutofillProfile expected_profile(kGuid1, kHttpsOrigin);
  expected_profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                              ASCIIToUTF16("456 El Camino Real\n"
                                           "Suite #1337"));
  EXPECT_EQ(ASCIIToUTF16("456 El Camino Real"),
            expected_profile.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Suite #1337"),
            expected_profile.GetRawInfo(ADDRESS_HOME_LINE2));
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_add.push_back(&expected_profile);

  // Verify the expectations.
  std::vector<AutofillProfile*> profiles_from_web_db;
  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Ensure that no Sync events are generated to fill in missing street address
// fields from Sync with explicitly present ones identical to the data stored in
// the line1 and line2 fields.  This ensures that the migration to add the
// street address field to profiles does not generate lots of needless Sync
// updates.
TEST_F(AutofillProfileSyncableServiceTest, MergeDataEmptyStreetAddress) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Create a profile with the street address set.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     ASCIIToUTF16("123 Example St.\n"
                                  "Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Example St."),
            profile.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), profile.GetRawInfo(ADDRESS_HOME_LINE2));

  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Create a Sync profile identical to |profile|, except without street address
  // explicitly set.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_line1("123 Example St.");
  autofill_specifics->set_address_home_line2("Apt. 42");
  EXPECT_FALSE(autofill_specifics->has_address_home_street_address());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  syncer::SyncChangeList expected_change_list;
  MergeDataAndStartSyncing(
      profiles_from_web_db, data_list, expected_bundle, expected_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Sync data without origin should not overwrite existing origin in local
// autofill profile.
TEST_F(AutofillProfileSyncableServiceTest, EmptySyncPreservesOrigin) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Local autofill profile has an origin.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Remote data does not have an origin value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  EXPECT_FALSE(autofill_specifics->has_origin());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to still have an origin after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(profile.guid(), profile.origin());
  expected_profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no sync events to add origin to the remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(profiles_from_web_db, data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Missing language code field should not generate sync events.
TEST_F(AutofillProfileSyncableServiceTest, NoLanguageCodeNoSync) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Local autofill profile has an empty language code.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  EXPECT_TRUE(profile.language_code().empty());
  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Remote data does not have a language code value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  EXPECT_FALSE(autofill_specifics->has_address_home_language_code());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect no changes to local and remote data.
  MockAutofillProfileSyncableService::DataBundle expected_empty_bundle;
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(profiles_from_web_db, data_list,
                           expected_empty_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Empty language code should be overwritten by sync.
TEST_F(AutofillProfileSyncableServiceTest, SyncUpdatesEmptyLanguageCode) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Local autofill profile has an empty language code.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  EXPECT_TRUE(profile.language_code().empty());
  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Remote data has "en" language code.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  EXPECT_TRUE(autofill_specifics->has_address_home_language_code());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to have "en" language code after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(kGuid1, kHttpsOrigin);
  expected_profile.set_language_code("en");
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(profiles_from_web_db, data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Incorrect language code should be overwritten by sync.
TEST_F(AutofillProfileSyncableServiceTest, SyncUpdatesIncorrectLanguageCode) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Local autofill profile has "de" language code.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.set_language_code("de");
  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Remote data has "en" language code.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  EXPECT_TRUE(autofill_specifics->has_address_home_language_code());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to have "en" language code after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(kGuid1, kHttpsOrigin);
  expected_profile.set_language_code("en");
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(profiles_from_web_db, data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Sync data without language code should not overwrite existing language code
// in local autofill profile.
TEST_F(AutofillProfileSyncableServiceTest, EmptySyncPreservesLanguageCode) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Local autofill profile has "en" language code.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.set_language_code("en");
  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Remote data does not have a language code value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first("John");
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  EXPECT_FALSE(autofill_specifics->has_address_home_language_code());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect local autofill profile to still have "en" language code after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(profile.guid(), profile.origin());
  expected_profile.set_language_code("en");
  expected_profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(profiles_from_web_db, data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Language code in autofill profiles should be synced to the server.
TEST_F(AutofillProfileSyncableServiceTest, LanguageCodePropagates) {
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;
  autofill_syncable_service_.set_sync_processor(sync_change_processor);

  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.set_language_code("en");
  AutofillProfileChange change(AutofillProfileChange::ADD, kGuid1, &profile);
  autofill_syncable_service_.AutofillProfileChanged(change);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, result.change_type());

  sync_pb::AutofillProfileSpecifics specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(kGuid1, specifics.guid());
  EXPECT_EQ(kHttpsOrigin, specifics.origin());
  EXPECT_EQ("en", specifics.address_home_language_code());
}

// Missing full name field should not generate sync events.
TEST_F(AutofillProfileSyncableServiceTest, NoFullNameNoSync) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Local autofill profile has an empty full name.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Remote data does not have a full name.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string("John"));
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect no changes to local and remote data.
  MockAutofillProfileSyncableService::DataBundle expected_empty_bundle;
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(profiles_from_web_db, data_list,
                           expected_empty_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, EmptySyncPreservesFullName) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Local autofill profile has a full name.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("John Jacob Smith, Jr"));
  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Remote data does not have a full name value.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string("John"));
  autofill_specifics->add_name_middle(std::string("Jacob"));
  autofill_specifics->add_name_last(std::string("Smith"));

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect local autofill profile to still have the same full name after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile(profile.guid(), profile.origin());
  expected_profile.SetInfo(AutofillType(NAME_FULL),
                           ASCIIToUTF16("John Jacob Smith, Jr"),
                           "en-US");
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(profiles_from_web_db, data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Missing use_count/use_date fields should not generate sync events.
TEST_F(AutofillProfileSyncableServiceTest, NoUsageStatsNoSync) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Local autofill profile has 0 for use_count/use_date.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.set_language_code("en");
  EXPECT_EQ(0U, profile.use_count());
  EXPECT_EQ(base::Time(), profile.use_date());
  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Remote data does not have use_count/use_date.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  EXPECT_FALSE(autofill_specifics->has_use_count());
  EXPECT_FALSE(autofill_specifics->has_use_date());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect no changes to local and remote data.
  MockAutofillProfileSyncableService::DataBundle expected_empty_bundle;
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(profiles_from_web_db, data_list,
                           expected_empty_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Usage stats should be updated by sync.
TEST_F(AutofillProfileSyncableServiceTest, SyncUpdatesEmptyUsageStats) {
  std::vector<AutofillProfile*> profiles_from_web_db;

  // Local autofill profile has 0 for use_count/use_date.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.set_language_code("en");
  EXPECT_EQ(0U, profile.use_count());
  EXPECT_EQ(base::Time(), profile.use_date());
  profiles_from_web_db.push_back(new AutofillProfile(profile));

  // Remote data has usage stats.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(profile.guid());
  autofill_specifics->set_origin(profile.origin());
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  autofill_specifics->set_use_count(9);
  autofill_specifics->set_use_date(1423182153);
  EXPECT_TRUE(autofill_specifics->has_use_count());
  EXPECT_TRUE(autofill_specifics->has_use_date());

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics));

  // Expect the local autofill profile to have usage stats after sync.
  MockAutofillProfileSyncableService::DataBundle expected_bundle;
  AutofillProfile expected_profile = profile;
  expected_profile.set_use_count(9U);
  expected_profile.set_use_date(base::Time::FromTimeT(1423182153));
  expected_bundle.profiles_to_update.push_back(&expected_profile);

  // Expect no changes to remote data.
  syncer::SyncChangeList expected_empty_change_list;

  MergeDataAndStartSyncing(profiles_from_web_db, data_list,
                           expected_bundle, expected_empty_change_list);
  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Usage stats should be updated by the client.
TEST_F(AutofillProfileSyncableServiceTest, ClientOverwritesUsageStats) {
  TestSyncChangeProcessor* sync_change_processor = new TestSyncChangeProcessor;

  // Remote data has a profile with usage stats.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillProfileSpecifics* autofill_specifics =
      specifics.mutable_autofill_profile();
  autofill_specifics->set_guid(kGuid1);
  autofill_specifics->set_origin(kHttpsOrigin);
  autofill_specifics->add_name_first(std::string());
  autofill_specifics->add_name_middle(std::string());
  autofill_specifics->add_name_last(std::string());
  autofill_specifics->add_name_full(std::string());
  autofill_specifics->add_email_address(std::string());
  autofill_specifics->add_phone_home_whole_number(std::string());
  autofill_specifics->set_address_home_language_code("en");
  autofill_specifics->set_use_count(9);
  autofill_specifics->set_use_date(25);

  syncer::SyncDataList data_list;
  data_list.push_back(
      syncer::SyncData::CreateLocalData(
          kGuid1, kHttpsOrigin, specifics));

  EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_syncable_service_,
              SaveChangesToWebData(_))
      .Times(1)
      .WillOnce(Return(true));
  autofill_syncable_service_.MergeDataAndStartSyncing(
      syncer::AUTOFILL_PROFILE,
      data_list,
      make_scoped_ptr(sync_change_processor),
      scoped_ptr<syncer::SyncErrorFactory>(
          new syncer::SyncErrorFactoryMock()));

  // Update to the usage stats for that profile.
  AutofillProfile profile(kGuid1, kHttpsOrigin);
  profile.set_language_code("en");
  profile.set_use_count(10U);
  profile.set_use_date(base::Time::FromTimeT(30));
  AutofillProfileChange change(AutofillProfileChange::UPDATE, kGuid1, &profile);
  autofill_syncable_service_.AutofillProfileChanged(change);
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile);

  ASSERT_EQ(1U, sync_change_processor->changes().size());
  syncer::SyncChange result = sync_change_processor->changes()[0];
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, result.change_type());

  sync_pb::AutofillProfileSpecifics result_specifics =
      result.sync_data().GetSpecifics().autofill_profile();
  EXPECT_EQ(10U, result_specifics.use_count());
  EXPECT_EQ(30, result_specifics.use_date());

  autofill_syncable_service_.StopSyncing(syncer::AUTOFILL_PROFILE);
}

// Server profile updates should be ignored.
TEST_F(AutofillProfileSyncableServiceTest, IgnoreServerProfileUpdate) {
  EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(autofill_syncable_service_, SaveChangesToWebData(_))
      .Times(1)
      .WillOnce(Return(true));
  autofill_syncable_service_.MergeDataAndStartSyncing(
      syncer::AUTOFILL_PROFILE, syncer::SyncDataList(),
      make_scoped_ptr(new TestSyncChangeProcessor),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));
  AutofillProfile server_profile(AutofillProfile::SERVER_PROFILE, "server-id");

  // Should not crash:
  autofill_syncable_service_.AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, server_profile.guid(), &server_profile));
}

}  // namespace autofill
