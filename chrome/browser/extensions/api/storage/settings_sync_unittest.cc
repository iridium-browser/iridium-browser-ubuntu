// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "chrome/browser/extensions/api/storage/syncable_settings_storage.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/api/storage/leveldb_settings_storage_factory.h"
#include "extensions/browser/api/storage/settings_storage_factory.h"
#include "extensions/browser/api/storage/settings_test_util.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/browser/value_store/testing_value_store.h"
#include "extensions/common/manifest.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using content::BrowserThread;

namespace extensions {

namespace util = settings_test_util;

namespace {

// To save typing ValueStore::DEFAULTS everywhere.
const ValueStore::WriteOptions DEFAULTS = ValueStore::DEFAULTS;

// More saving typing. Maps extension IDs to a list of sync changes for that
// extension.
using SettingSyncDataMultimap =
    std::map<std::string, linked_ptr<SettingSyncDataList>>;

// Gets the pretty-printed JSON for a value.
static std::string GetJson(const base::Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

// Returns whether two Values are equal.
testing::AssertionResult ValuesEq(
    const char* _1, const char* _2,
    const base::Value* expected,
    const base::Value* actual) {
  if (expected == actual) {
    return testing::AssertionSuccess();
  }
  if (!expected && actual) {
    return testing::AssertionFailure() <<
        "Expected NULL, actual: " << GetJson(*actual);
  }
  if (expected && !actual) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJson(*expected) << ", actual NULL";
  }
  if (!expected->Equals(actual)) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJson(*expected) << ", actual: " << GetJson(*actual);
  }
  return testing::AssertionSuccess();
}

// Returns whether the result of a storage operation is an expected value.
// Logs when different.
testing::AssertionResult SettingsEq(
    const char* _1, const char* _2,
    const base::DictionaryValue& expected,
    ValueStore::ReadResult actual) {
  if (actual->HasError()) {
    return testing::AssertionFailure() <<
        "Expected: " << expected <<
        ", actual has error: " << actual->error().message;
  }
  return ValuesEq(_1, _2, &expected, &actual->settings());
}

// SyncChangeProcessor which just records the changes made, accessed after
// being converted to the more useful SettingSyncData via changes().
class MockSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() : fail_all_requests_(false) {}

  // syncer::SyncChangeProcessor implementation.
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override {
    if (fail_all_requests_) {
      return syncer::SyncError(
          FROM_HERE,
          syncer::SyncError::DATATYPE_ERROR,
          "MockSyncChangeProcessor: configured to fail",
          change_list[0].sync_data().GetDataType());
    }
    for (syncer::SyncChangeList::const_iterator it = change_list.begin();
        it != change_list.end(); ++it) {
      changes_.push_back(new SettingSyncData(*it));
    }
    return syncer::SyncError();
  }

  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return syncer::SyncDataList();
  }

  // Mock methods.

  const SettingSyncDataList& changes() { return changes_; }

  void ClearChanges() {
    changes_.clear();
  }

  void set_fail_all_requests(bool fail_all_requests) {
    fail_all_requests_ = fail_all_requests;
  }

  // Returns the only change for a given extension setting.  If there is not
  // exactly 1 change for that key, a test assertion will fail.
  SettingSyncData* GetOnlyChange(const std::string& extension_id,
                                 const std::string& key) {
    std::vector<SettingSyncData*> matching_changes;
    for (SettingSyncDataList::iterator it = changes_.begin();
        it != changes_.end(); ++it) {
      if ((*it)->extension_id() == extension_id && (*it)->key() == key) {
        matching_changes.push_back(*it);
      }
    }
    if (matching_changes.empty()) {
      ADD_FAILURE() << "No matching changes for " << extension_id << "/" <<
          key << " (out of " << changes_.size() << ")";
      return nullptr;
    }
    if (matching_changes.size() != 1u) {
      ADD_FAILURE() << matching_changes.size() << " matching changes for " <<
           extension_id << "/" << key << " (out of " << changes_.size() << ")";
    }
    return matching_changes[0];
  }

 private:
  SettingSyncDataList changes_;
  bool fail_all_requests_;
};

// SettingsStorageFactory which always returns TestingValueStore objects,
// and allows individually created objects to be returned.
class TestingValueStoreFactory : public SettingsStorageFactory {
 public:
  TestingValueStore* GetExisting(const std::string& extension_id) {
    DCHECK(created_.count(extension_id));
    return created_[extension_id];
  }

  // SettingsStorageFactory implementation.
  ValueStore* Create(const base::FilePath& base_path,
                     const std::string& extension_id) override {
    TestingValueStore* new_storage = new TestingValueStore();
    DCHECK(!created_.count(extension_id));
    created_[extension_id] = new_storage;
    return new_storage;
  }

  // Testing value stores don't actually create a real database. Don't delete
  // any files.
  void DeleteDatabaseIfExists(const base::FilePath& base_path,
                              const std::string& extension_id) override {}

 private:
  // SettingsStorageFactory is refcounted.
  ~TestingValueStoreFactory() override {}

  // None of these storage areas are owned by this factory, so care must be
  // taken when calling GetExisting.
  std::map<std::string, TestingValueStore*> created_;
};

scoped_ptr<KeyedService> MockExtensionSystemFactoryFunction(
    content::BrowserContext* context) {
  return make_scoped_ptr(new MockExtensionSystem(context));
}

scoped_ptr<KeyedService> BuildEventRouter(content::BrowserContext* profile) {
  return make_scoped_ptr(new extensions::EventRouter(profile, nullptr));
}

}  // namespace

class ExtensionSettingsSyncTest : public testing::Test {
 public:
  ExtensionSettingsSyncTest()
      : ui_thread_(BrowserThread::UI, base::MessageLoop::current()),
        file_thread_(BrowserThread::FILE, base::MessageLoop::current()),
        storage_factory_(new util::ScopedSettingsStorageFactory()),
        sync_processor_(new MockSyncChangeProcessor),
        sync_processor_wrapper_(new syncer::SyncChangeProcessorWrapperForTest(
            sync_processor_.get())) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new TestingProfile(temp_dir_.path()));
    storage_factory_->Reset(new LeveldbSettingsStorageFactory());
    frontend_ = StorageFrontend::CreateForTesting(storage_factory_,
                                                  profile_.get()).Pass();

    ExtensionsBrowserClient::Get()
        ->GetExtensionSystemFactory()
        ->SetTestingFactoryAndUse(profile_.get(),
                                  &MockExtensionSystemFactoryFunction);

    EventRouterFactory::GetInstance()->SetTestingFactory(profile_.get(),
                                                         &BuildEventRouter);
  }

  void TearDown() override {
    frontend_.reset();
    profile_.reset();
    // Execute any pending deletion tasks.
    message_loop_.RunUntilIdle();
  }

 protected:
  // Adds a record of an extension or app to the extension service, then returns
  // its storage area.
  ValueStore* AddExtensionAndGetStorage(
      const std::string& id, Manifest::Type type) {
    scoped_refptr<const Extension> extension =
        util::AddExtensionWithId(profile_.get(), id, type);
    return util::GetStorage(extension, frontend_.get());
  }

  // Gets the syncer::SyncableService for the given sync type.
  syncer::SyncableService* GetSyncableService(syncer::ModelType model_type) {
    base::MessageLoop::current()->RunUntilIdle();
    SyncValueStoreCache* sync_cache = static_cast<SyncValueStoreCache*>(
        frontend_->GetValueStoreCache(settings_namespace::SYNC));
    return sync_cache->GetSyncableService(model_type);
  }

  // Gets all the sync data from the SyncableService for a sync type as a map
  // from extension id to its sync data.
  SettingSyncDataMultimap GetAllSyncData(syncer::ModelType model_type) {
    syncer::SyncDataList as_list =
        GetSyncableService(model_type)->GetAllSyncData(model_type);
    SettingSyncDataMultimap as_map;
    for (syncer::SyncDataList::iterator it = as_list.begin();
        it != as_list.end(); ++it) {
      SettingSyncData* sync_data = new SettingSyncData(*it);
      linked_ptr<SettingSyncDataList>& list_for_extension =
          as_map[sync_data->extension_id()];
      if (!list_for_extension.get())
        list_for_extension.reset(new SettingSyncDataList());
      list_for_extension->push_back(sync_data);
    }
    return as_map;
  }

  // Need these so that the DCHECKs for running on FILE or UI threads pass.
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<StorageFrontend> frontend_;
  scoped_refptr<util::ScopedSettingsStorageFactory> storage_factory_;
  scoped_ptr<MockSyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncChangeProcessorWrapperForTest> sync_processor_wrapper_;
};

// Get a semblance of coverage for both EXTENSION_SETTINGS and APP_SETTINGS
// sync by roughly alternative which one to test.

TEST_F(ExtensionSettingsSyncTest, NoDataDoesNotInvokeSync) {
  syncer::ModelType model_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  EXPECT_EQ(0u, GetAllSyncData(model_type).size());

  // Have one extension created before sync is set up, the other created after.
  AddExtensionAndGetStorage("s1", type);
  EXPECT_EQ(0u, GetAllSyncData(model_type).size());

  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  AddExtensionAndGetStorage("s2", type);
  EXPECT_EQ(0u, GetAllSyncData(model_type).size());

  GetSyncableService(model_type)->StopSyncing(model_type);

  EXPECT_EQ(0u, sync_processor_->changes().size());
  EXPECT_EQ(0u, GetAllSyncData(model_type).size());
}

TEST_F(ExtensionSettingsSyncTest, InSyncDataDoesNotInvokeSync) {
  syncer::ModelType model_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::StringValue value1("fooValue");
  base::ListValue value2;
  value2.Append(new base::StringValue("barValue"));

  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);

  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "bar", value2);

  SettingSyncDataMultimap all_sync_data = GetAllSyncData(model_type);
  EXPECT_EQ(2u, all_sync_data.size());
  EXPECT_EQ(1u, all_sync_data["s1"]->size());
  EXPECT_PRED_FORMAT2(ValuesEq, &value1, &(*all_sync_data["s1"])[0]->value());
  EXPECT_EQ(1u, all_sync_data["s2"]->size());
  EXPECT_PRED_FORMAT2(ValuesEq, &value2, &(*all_sync_data["s2"])[0]->value());

  syncer::SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s1", "foo", value1, model_type));
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2, model_type));

  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          sync_data,
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  // Already in sync, so no changes.
  EXPECT_EQ(0u, sync_processor_->changes().size());

  // Regression test: not-changing the synced value shouldn't result in a sync
  // change, and changing the synced value should result in an update.
  storage1->Set(DEFAULTS, "foo", value1);
  EXPECT_EQ(0u, sync_processor_->changes().size());

  storage1->Set(DEFAULTS, "foo", value2);
  EXPECT_EQ(1u, sync_processor_->changes().size());
  SettingSyncData* change = sync_processor_->GetOnlyChange("s1", "foo");
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
  EXPECT_TRUE(value2.Equals(&change->value()));

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, LocalDataWithNoSyncDataIsPushedToSync) {
  syncer::ModelType model_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::StringValue value1("fooValue");
  base::ListValue value2;
  value2.Append(new base::StringValue("barValue"));

  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);

  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "bar", value2);

  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  // All settings should have been pushed to sync.
  EXPECT_EQ(2u, sync_processor_->changes().size());
  SettingSyncData* change = sync_processor_->GetOnlyChange("s1", "foo");
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
  EXPECT_TRUE(value1.Equals(&change->value()));
  change = sync_processor_->GetOnlyChange("s2", "bar");
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
  EXPECT_TRUE(value2.Equals(&change->value()));

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, AnySyncDataOverwritesLocalData) {
  syncer::ModelType model_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::StringValue value1("fooValue");
  base::ListValue value2;
  value2.Append(new base::StringValue("barValue"));

  // Maintain dictionaries mirrored to the expected values of the settings in
  // each storage area.
  base::DictionaryValue expected1, expected2;

  // Pre-populate one of the storage areas.
  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  storage1->Set(DEFAULTS, "overwriteMe", value1);

  syncer::SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s1", "foo", value1, model_type));
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2, model_type));
  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          sync_data,
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  expected1.Set("foo", value1.DeepCopy());
  expected2.Set("bar", value2.DeepCopy());

  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);

  // All changes should be local, so no sync changes.
  EXPECT_EQ(0u, sync_processor_->changes().size());

  // Sync settings should have been pushed to local settings.
  EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, ProcessSyncChanges) {
  syncer::ModelType model_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::StringValue value1("fooValue");
  base::ListValue value2;
  value2.Append(new base::StringValue("barValue"));

  // Maintain dictionaries mirrored to the expected values of the settings in
  // each storage area.
  base::DictionaryValue expected1, expected2;

  // Make storage1 initialised from local data, storage2 initialised from sync.
  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);

  storage1->Set(DEFAULTS, "foo", value1);
  expected1.Set("foo", value1.DeepCopy());

  syncer::SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2, model_type));

  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          sync_data,
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  expected2.Set("bar", value2.DeepCopy());

  // Make sync add some settings.
  syncer::SyncChangeList change_list;
  change_list.push_back(settings_sync_util::CreateAdd(
      "s1", "bar", value2, model_type));
  change_list.push_back(settings_sync_util::CreateAdd(
      "s2", "foo", value1, model_type));
  GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Set("bar", value2.DeepCopy());
  expected2.Set("foo", value1.DeepCopy());

  EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  // Make sync update some settings, storage1 the new setting, storage2 the
  // initial setting.
  change_list.clear();
  change_list.push_back(settings_sync_util::CreateUpdate(
      "s1", "bar", value2, model_type));
  change_list.push_back(settings_sync_util::CreateUpdate(
      "s2", "bar", value1, model_type));
  GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Set("bar", value2.DeepCopy());
  expected2.Set("bar", value1.DeepCopy());

  EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  // Make sync remove some settings, storage1 the initial setting, storage2 the
  // new setting.
  change_list.clear();
  change_list.push_back(settings_sync_util::CreateDelete(
      "s1", "foo", model_type));
  change_list.push_back(settings_sync_util::CreateDelete(
      "s2", "foo", model_type));
  GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  expected1.Remove("foo", NULL);
  expected2.Remove("foo", NULL);

  EXPECT_PRED_FORMAT2(SettingsEq, expected1, storage1->Get());
  EXPECT_PRED_FORMAT2(SettingsEq, expected2, storage2->Get());

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, PushToSync) {
  syncer::ModelType model_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::StringValue value1("fooValue");
  base::ListValue value2;
  value2.Append(new base::StringValue("barValue"));

  // Make storage1/2 initialised from local data, storage3/4 initialised from
  // sync.
  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);
  ValueStore* storage3 = AddExtensionAndGetStorage("s3", type);
  ValueStore* storage4 = AddExtensionAndGetStorage("s4", type);

  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "foo", value1);

  syncer::SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s3", "bar", value2, model_type));
  sync_data.push_back(settings_sync_util::CreateData(
      "s4", "bar", value2, model_type));

  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          sync_data,
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  // Add something locally.
  storage1->Set(DEFAULTS, "bar", value2);
  storage2->Set(DEFAULTS, "bar", value2);
  storage3->Set(DEFAULTS, "foo", value1);
  storage4->Set(DEFAULTS, "foo", value1);

  SettingSyncData* change = sync_processor_->GetOnlyChange("s1", "bar");
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
  EXPECT_TRUE(value2.Equals(&change->value()));
  sync_processor_->GetOnlyChange("s2", "bar");
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
  EXPECT_TRUE(value2.Equals(&change->value()));
  change = sync_processor_->GetOnlyChange("s3", "foo");
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
  EXPECT_TRUE(value1.Equals(&change->value()));
  change = sync_processor_->GetOnlyChange("s4", "foo");
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change->change_type());
  EXPECT_TRUE(value1.Equals(&change->value()));

  // Change something locally, storage1/3 the new setting and storage2/4 the
  // initial setting, for all combinations of local vs sync intialisation and
  // new vs initial.
  sync_processor_->ClearChanges();
  storage1->Set(DEFAULTS, "bar", value1);
  storage2->Set(DEFAULTS, "foo", value2);
  storage3->Set(DEFAULTS, "bar", value1);
  storage4->Set(DEFAULTS, "foo", value2);

  change = sync_processor_->GetOnlyChange("s1", "bar");
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
  EXPECT_TRUE(value1.Equals(&change->value()));
  change = sync_processor_->GetOnlyChange("s2", "foo");
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
  EXPECT_TRUE(value2.Equals(&change->value()));
  change = sync_processor_->GetOnlyChange("s3", "bar");
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
  EXPECT_TRUE(value1.Equals(&change->value()));
  change = sync_processor_->GetOnlyChange("s4", "foo");
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change->change_type());
  EXPECT_TRUE(value2.Equals(&change->value()));

  // Remove something locally, storage1/3 the new setting and storage2/4 the
  // initial setting, for all combinations of local vs sync intialisation and
  // new vs initial.
  sync_processor_->ClearChanges();
  storage1->Remove("foo");
  storage2->Remove("bar");
  storage3->Remove("foo");
  storage4->Remove("bar");

  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s1", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s2", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s3", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s4", "bar")->change_type());

  // Remove some nonexistent settings.
  sync_processor_->ClearChanges();
  storage1->Remove("foo");
  storage2->Remove("bar");
  storage3->Remove("foo");
  storage4->Remove("bar");

  EXPECT_EQ(0u, sync_processor_->changes().size());

  // Clear the rest of the settings.  Add the removed ones back first so that
  // more than one setting is cleared.
  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "bar", value2);
  storage3->Set(DEFAULTS, "foo", value1);
  storage4->Set(DEFAULTS, "bar", value2);

  sync_processor_->ClearChanges();
  storage1->Clear();
  storage2->Clear();
  storage3->Clear();
  storage4->Clear();

  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s1", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s1", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s2", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s2", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s3", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s3", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s4", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            sync_processor_->GetOnlyChange("s4", "bar")->change_type());

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, ExtensionAndAppSettingsSyncSeparately) {
  base::StringValue value1("fooValue");
  base::ListValue value2;
  value2.Append(new base::StringValue("barValue"));

  // storage1 is an extension, storage2 is an app.
  ValueStore* storage1 = AddExtensionAndGetStorage(
      "s1", Manifest::TYPE_EXTENSION);
  ValueStore* storage2 = AddExtensionAndGetStorage(
      "s2", Manifest::TYPE_LEGACY_PACKAGED_APP);

  storage1->Set(DEFAULTS, "foo", value1);
  storage2->Set(DEFAULTS, "bar", value2);

  SettingSyncDataMultimap extension_sync_data =
      GetAllSyncData(syncer::EXTENSION_SETTINGS);
  EXPECT_EQ(1u, extension_sync_data.size());
  EXPECT_EQ(1u, extension_sync_data["s1"]->size());
  EXPECT_PRED_FORMAT2(ValuesEq, &value1,
                      &(*extension_sync_data["s1"])[0]->value());

  SettingSyncDataMultimap app_sync_data = GetAllSyncData(syncer::APP_SETTINGS);
  EXPECT_EQ(1u, app_sync_data.size());
  EXPECT_EQ(1u, app_sync_data["s2"]->size());
  EXPECT_PRED_FORMAT2(ValuesEq, &value2, &(*app_sync_data["s2"])[0]->value());

  // Stop each separately, there should be no changes either time.
  syncer::SyncDataList sync_data;
  sync_data.push_back(settings_sync_util::CreateData(
      "s1", "foo", value1, syncer::EXTENSION_SETTINGS));

  GetSyncableService(syncer::EXTENSION_SETTINGS)
      ->MergeDataAndStartSyncing(
          syncer::EXTENSION_SETTINGS,
          sync_data,
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  GetSyncableService(syncer::EXTENSION_SETTINGS)->
      StopSyncing(syncer::EXTENSION_SETTINGS);
  EXPECT_EQ(0u, sync_processor_->changes().size());

  sync_data.clear();
  sync_data.push_back(settings_sync_util::CreateData(
      "s2", "bar", value2, syncer::APP_SETTINGS));

  scoped_ptr<syncer::SyncChangeProcessorWrapperForTest> app_settings_delegate_(
      new syncer::SyncChangeProcessorWrapperForTest(sync_processor_.get()));
  GetSyncableService(syncer::APP_SETTINGS)
      ->MergeDataAndStartSyncing(
          syncer::APP_SETTINGS,
          sync_data,
          app_settings_delegate_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  GetSyncableService(syncer::APP_SETTINGS)->
      StopSyncing(syncer::APP_SETTINGS);
  EXPECT_EQ(0u, sync_processor_->changes().size());
}

TEST_F(ExtensionSettingsSyncTest, FailingStartSyncingDisablesSync) {
  syncer::ModelType model_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::StringValue fooValue("fooValue");
  base::StringValue barValue("barValue");

  // There is a bit of a convoluted method to get storage areas that can fail;
  // hand out TestingValueStore object then toggle them failing/succeeding
  // as necessary.
  TestingValueStoreFactory* testing_factory = new TestingValueStoreFactory();
  storage_factory_->Reset(testing_factory);

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  // Make bad fail for incoming sync changes.
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::CORRUPTION);
  {
    syncer::SyncDataList sync_data;
    sync_data.push_back(settings_sync_util::CreateData(
          "good", "foo", fooValue, model_type));
    sync_data.push_back(settings_sync_util::CreateData(
          "bad", "foo", fooValue, model_type));
    GetSyncableService(model_type)
        ->MergeDataAndStartSyncing(
            model_type,
            sync_data,
            sync_processor_wrapper_.Pass(),
            make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  }
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::OK);

  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Changes made to good should be sent to sync, changes from bad shouldn't.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", barValue);
  bad->Set(DEFAULTS, "bar", barValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Changes received from sync should go to good but not bad (even when it's
  // not failing).
  {
    syncer::SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateUpdate(
          "good", "foo", barValue, model_type));
    // (Sending UPDATE here even though it's adding, since that's what the state
    // of sync is.  In any case, it won't work.)
    change_list.push_back(settings_sync_util::CreateUpdate(
          "bad", "foo", barValue, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    base::DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Changes made to bad still shouldn't go to sync, even though it didn't fail
  // last time.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", fooValue);
  bad->Set(DEFAULTS, "bar", fooValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  {
    base::DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Failing ProcessSyncChanges shouldn't go to the storage.
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::CORRUPTION);
  {
    syncer::SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateUpdate(
          "good", "foo", fooValue, model_type));
    // (Ditto.)
    change_list.push_back(settings_sync_util::CreateUpdate(
          "bad", "foo", fooValue, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::OK);

  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Restarting sync should make bad start syncing again.
  sync_processor_->ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  sync_processor_wrapper_.reset(
      new syncer::SyncChangeProcessorWrapperForTest(sync_processor_.get()));
  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  // Local settings will have been pushed to sync, since it's empty (in this
  // test; presumably it wouldn't be live, since we've been getting changes).
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("bad", "bar")->change_type());
  EXPECT_EQ(3u, sync_processor_->changes().size());

  // Live local changes now get pushed, too.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", barValue);
  bad->Set(DEFAULTS, "bar", barValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("bad", "bar")->change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());

  // And ProcessSyncChanges work, too.
  {
    syncer::SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateUpdate(
          "good", "bar", fooValue, model_type));
    change_list.push_back(settings_sync_util::CreateUpdate(
          "bad", "bar", fooValue, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("bar", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }
}

TEST_F(ExtensionSettingsSyncTest, FailingProcessChangesDisablesSync) {
  // The test above tests a failing ProcessSyncChanges too, but here test with
  // an initially passing MergeDataAndStartSyncing.
  syncer::ModelType model_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::StringValue fooValue("fooValue");
  base::StringValue barValue("barValue");

  TestingValueStoreFactory* testing_factory = new TestingValueStoreFactory();
  storage_factory_->Reset(testing_factory);

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  // Unlike before, initially succeeding MergeDataAndStartSyncing.
  {
    syncer::SyncDataList sync_data;
    sync_data.push_back(settings_sync_util::CreateData(
          "good", "foo", fooValue, model_type));
    sync_data.push_back(settings_sync_util::CreateData(
          "bad", "foo", fooValue, model_type));
    GetSyncableService(model_type)
        ->MergeDataAndStartSyncing(
            model_type,
            sync_data,
            sync_processor_wrapper_.Pass(),
            make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  }

  EXPECT_EQ(0u, sync_processor_->changes().size());

  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Now fail ProcessSyncChanges for bad.
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::CORRUPTION);
  {
    syncer::SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateAdd(
          "good", "bar", barValue, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "bar", barValue, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::OK);

  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // No more changes sent to sync for bad.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", barValue);
  bad->Set(DEFAULTS, "foo", barValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // No more changes received from sync should go to bad.
  {
    syncer::SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateAdd(
          "good", "foo", fooValue, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "foo", fooValue, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }
}

TEST_F(ExtensionSettingsSyncTest, FailingGetAllSyncDataDoesntStopSync) {
  syncer::ModelType model_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::StringValue fooValue("fooValue");
  base::StringValue barValue("barValue");

  TestingValueStoreFactory* testing_factory = new TestingValueStoreFactory();
  storage_factory_->Reset(testing_factory);

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  good->Set(DEFAULTS, "foo", fooValue);
  bad->Set(DEFAULTS, "foo", fooValue);

  // Even though bad will fail to get all sync data, sync data should still
  // include that from good.
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::CORRUPTION);
  {
    syncer::SyncDataList all_sync_data =
        GetSyncableService(model_type)->GetAllSyncData(model_type);
    EXPECT_EQ(1u, all_sync_data.size());
    EXPECT_EQ("good/foo", syncer::SyncDataLocal(all_sync_data[0]).GetTag());
  }
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::OK);

  // Sync shouldn't be disabled for good (nor bad -- but this is unimportant).
  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("bad", "foo")->change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());

  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", barValue);
  bad->Set(DEFAULTS, "bar", barValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("bad", "bar")->change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());
}

TEST_F(ExtensionSettingsSyncTest, FailureToReadChangesToPushDisablesSync) {
  syncer::ModelType model_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  base::StringValue fooValue("fooValue");
  base::StringValue barValue("barValue");

  TestingValueStoreFactory* testing_factory = new TestingValueStoreFactory();
  storage_factory_->Reset(testing_factory);

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  good->Set(DEFAULTS, "foo", fooValue);
  bad->Set(DEFAULTS, "foo", fooValue);

  // good will successfully push foo:fooValue to sync, but bad will fail to
  // get them so won't.
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::CORRUPTION);
  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  testing_factory->GetExisting("bad")->set_error_code(ValueStore::OK);

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // bad should now be disabled for sync.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", barValue);
  bad->Set(DEFAULTS, "bar", barValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  {
    syncer::SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateUpdate(
          "good", "foo", barValue, model_type));
    // (Sending ADD here even though it's updating, since that's what the state
    // of sync is.  In any case, it won't work.)
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "foo", barValue, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    base::DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("foo", fooValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Re-enabling sync without failing should cause the local changes from bad
  // to be pushed to sync successfully, as should future changes to bad.
  sync_processor_->ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  sync_processor_wrapper_.reset(
      new syncer::SyncChangeProcessorWrapperForTest(sync_processor_.get()));
  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("bad", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("bad", "bar")->change_type());
  EXPECT_EQ(4u, sync_processor_->changes().size());

  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "bar", fooValue);
  bad->Set(DEFAULTS, "bar", fooValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());
}

TEST_F(ExtensionSettingsSyncTest, FailureToPushLocalStateDisablesSync) {
  syncer::ModelType model_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::StringValue fooValue("fooValue");
  base::StringValue barValue("barValue");

  TestingValueStoreFactory* testing_factory = new TestingValueStoreFactory();
  storage_factory_->Reset(testing_factory);

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  // Only set bad; setting good will cause it to fail below.
  bad->Set(DEFAULTS, "foo", fooValue);

  sync_processor_->set_fail_all_requests(true);
  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  sync_processor_->set_fail_all_requests(false);

  // Changes from good will be send to sync, changes from bad won't.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", barValue);
  bad->Set(DEFAULTS, "foo", barValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // Changes from sync will be sent to good, not to bad.
  {
    syncer::SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateAdd(
          "good", "bar", barValue, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "bar", barValue, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    base::DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Restarting sync makes everything work again.
  sync_processor_->ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  sync_processor_wrapper_.reset(
      new syncer::SyncChangeProcessorWrapperForTest(sync_processor_.get()));
  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("bad", "foo")->change_type());
  EXPECT_EQ(3u, sync_processor_->changes().size());

  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", fooValue);
  bad->Set(DEFAULTS, "foo", fooValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());
}

TEST_F(ExtensionSettingsSyncTest, FailureToPushLocalChangeDisablesSync) {
  syncer::ModelType model_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  base::StringValue fooValue("fooValue");
  base::StringValue barValue("barValue");

  TestingValueStoreFactory* testing_factory = new TestingValueStoreFactory();
  storage_factory_->Reset(testing_factory);

  ValueStore* good = AddExtensionAndGetStorage("good", type);
  ValueStore* bad = AddExtensionAndGetStorage("bad", type);

  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  // bad will fail to send changes.
  good->Set(DEFAULTS, "foo", fooValue);
  sync_processor_->set_fail_all_requests(true);
  bad->Set(DEFAULTS, "foo", fooValue);
  sync_processor_->set_fail_all_requests(false);

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // No further changes should be sent from bad.
  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", barValue);
  bad->Set(DEFAULTS, "foo", barValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(1u, sync_processor_->changes().size());

  // Changes from sync will be sent to good, not to bad.
  {
    syncer::SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateAdd(
          "good", "bar", barValue, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "bad", "bar", barValue, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }

  {
    base::DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    dict.Set("bar", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, good->Get());
  }
  {
    base::DictionaryValue dict;
    dict.Set("foo", barValue.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, dict, bad->Get());
  }

  // Restarting sync makes everything work again.
  sync_processor_->ClearChanges();
  GetSyncableService(model_type)->StopSyncing(model_type);
  sync_processor_wrapper_.reset(
      new syncer::SyncChangeProcessorWrapperForTest(sync_processor_.get()));
  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("good", "bar")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            sync_processor_->GetOnlyChange("bad", "foo")->change_type());
  EXPECT_EQ(3u, sync_processor_->changes().size());

  sync_processor_->ClearChanges();
  good->Set(DEFAULTS, "foo", fooValue);
  bad->Set(DEFAULTS, "foo", fooValue);

  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            sync_processor_->GetOnlyChange("good", "foo")->change_type());
  EXPECT_EQ(2u, sync_processor_->changes().size());
}

TEST_F(ExtensionSettingsSyncTest,
       LargeOutgoingChangeRejectedButIncomingAccepted) {
  syncer::ModelType model_type = syncer::APP_SETTINGS;
  Manifest::Type type = Manifest::TYPE_LEGACY_PACKAGED_APP;

  // This value should be larger than the limit in sync_storage_backend.cc.
  std::string string_10k;
  for (size_t i = 0; i < 10000; ++i) {
    string_10k.append("a");
  }
  base::StringValue large_value(string_10k);

  GetSyncableService(model_type)
      ->MergeDataAndStartSyncing(
          model_type,
          syncer::SyncDataList(),
          sync_processor_wrapper_.Pass(),
          make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  // Large local change rejected and doesn't get sent out.
  ValueStore* storage1 = AddExtensionAndGetStorage("s1", type);
  EXPECT_TRUE(storage1->Set(DEFAULTS, "large_value", large_value)->HasError());
  EXPECT_EQ(0u, sync_processor_->changes().size());

  // Large incoming change should still get accepted.
  ValueStore* storage2 = AddExtensionAndGetStorage("s2", type);
  {
    syncer::SyncChangeList change_list;
    change_list.push_back(settings_sync_util::CreateAdd(
          "s1", "large_value", large_value, model_type));
    change_list.push_back(settings_sync_util::CreateAdd(
          "s2", "large_value", large_value, model_type));
    GetSyncableService(model_type)->ProcessSyncChanges(FROM_HERE, change_list);
  }
  {
    base::DictionaryValue expected;
    expected.Set("large_value", large_value.DeepCopy());
    EXPECT_PRED_FORMAT2(SettingsEq, expected, storage1->Get());
    EXPECT_PRED_FORMAT2(SettingsEq, expected, storage2->Get());
  }

  GetSyncableService(model_type)->StopSyncing(model_type);
}

TEST_F(ExtensionSettingsSyncTest, Dots) {
  syncer::ModelType model_type = syncer::EXTENSION_SETTINGS;
  Manifest::Type type = Manifest::TYPE_EXTENSION;

  ValueStore* storage = AddExtensionAndGetStorage("ext", type);

  {
    syncer::SyncDataList sync_data_list;
    scoped_ptr<base::Value> string_value(new base::StringValue("value"));
    sync_data_list.push_back(settings_sync_util::CreateData(
        "ext", "key.with.dot", *string_value, model_type));

    GetSyncableService(model_type)
        ->MergeDataAndStartSyncing(
            model_type,
            sync_data_list,
            sync_processor_wrapper_.Pass(),
            make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  }

  // Test dots in keys that come from sync.
  {
    ValueStore::ReadResult data = storage->Get();
    ASSERT_FALSE(data->HasError());

    base::DictionaryValue expected_data;
    expected_data.SetWithoutPathExpansion(
        "key.with.dot",
        new base::StringValue("value"));
    EXPECT_TRUE(base::Value::Equals(&expected_data, &data->settings()));
  }

  // Test dots in keys going to sync.
  {
    scoped_ptr<base::Value> string_value(new base::StringValue("spot"));
    storage->Set(DEFAULTS, "key.with.spot", *string_value);

    ASSERT_EQ(1u, sync_processor_->changes().size());
    SettingSyncData* sync_data = sync_processor_->changes()[0];
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, sync_data->change_type());
    EXPECT_EQ("ext", sync_data->extension_id());
    EXPECT_EQ("key.with.spot", sync_data->key());
    EXPECT_TRUE(sync_data->value().Equals(string_value.get()));
  }
}

// In other (frontend) tests, we assume that the result of GetStorage
// is a pointer to the a Storage owned by a Frontend object, but for
// the unlimitedStorage case, this might not be true. So, write the
// tests in a "callback" style.  We should really rewrite all tests to
// be asynchronous in this way.

namespace {

static void UnlimitedSyncStorageTestCallback(ValueStore* sync_storage) {
  // Sync storage should still run out after ~100K; the unlimitedStorage
  // permission can't apply to sync.
  scoped_ptr<base::Value> kilobyte = util::CreateKilobyte();
  for (int i = 0; i < 100; ++i) {
    sync_storage->Set(ValueStore::DEFAULTS, base::IntToString(i), *kilobyte);
  }

  EXPECT_TRUE(sync_storage->Set(ValueStore::DEFAULTS, "WillError", *kilobyte)
                  ->HasError());
}

static void UnlimitedLocalStorageTestCallback(ValueStore* local_storage) {
  // Local storage should never run out.
  scoped_ptr<base::Value> megabyte = util::CreateMegabyte();
  for (int i = 0; i < 7; ++i) {
    local_storage->Set(ValueStore::DEFAULTS, base::IntToString(i), *megabyte);
  }

  EXPECT_FALSE(local_storage->Set(ValueStore::DEFAULTS, "WontError", *megabyte)
                   ->HasError());
}

}  // namespace

#if defined(OS_WIN)
// See: http://crbug.com/227296
#define MAYBE_UnlimitedStorageForLocalButNotSync \
  DISABLED_UnlimitedStorageForLocalButNotSync
#else
#define MAYBE_UnlimitedStorageForLocalButNotSync \
  UnlimitedStorageForLocalButNotSync
#endif
TEST_F(ExtensionSettingsSyncTest, MAYBE_UnlimitedStorageForLocalButNotSync) {
  const std::string id = "ext";
  std::set<std::string> permissions;
  permissions.insert("unlimitedStorage");
  scoped_refptr<const Extension> extension =
      util::AddExtensionWithIdAndPermissions(
          profile_.get(), id, Manifest::TYPE_EXTENSION, permissions);

  frontend_->RunWithStorage(extension,
                            settings_namespace::SYNC,
                            base::Bind(&UnlimitedSyncStorageTestCallback));
  frontend_->RunWithStorage(extension,
                            settings_namespace::LOCAL,
                            base::Bind(&UnlimitedLocalStorageTestCallback));

  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace extensions
