// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/tracked_preferences_migration.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/user_prefs/tracked/dictionary_hash_store_contents.h"
#include "components/user_prefs/tracked/hash_store_contents.h"
#include "components/user_prefs/tracked/interceptable_pref_filter.h"
#include "components/user_prefs/tracked/pref_hash_store.h"
#include "components/user_prefs/tracked/pref_hash_store_impl.h"
#include "components/user_prefs/tracked/pref_hash_store_transaction.h"
#include "components/user_prefs/tracked/pref_service_hash_store_contents.h"
#include "components/user_prefs/tracked/tracked_preferences_migration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// An unprotected pref.
const char kUnprotectedPref[] = "unprotected";
// A protected pref.
const char kProtectedPref[] = "protected";
// A protected pref which is initially stored in the unprotected store.
const char kPreviouslyUnprotectedPref[] = "previously.unprotected";
// An unprotected pref which is initially stored in the protected store.
const char kPreviouslyProtectedPref[] = "previously.protected";

const char kUnprotectedPrefValue[] = "unprotected_value";
const char kProtectedPrefValue[] = "protected_value";
const char kPreviouslyUnprotectedPrefValue[] = "previously_unprotected_value";
const char kPreviouslyProtectedPrefValue[] = "previously_protected_value";

// A simple InterceptablePrefFilter which doesn't do anything but hand the prefs
// back downstream in FinalizeFilterOnLoad.
class SimpleInterceptablePrefFilter : public InterceptablePrefFilter {
 public:
  // PrefFilter remaining implementation.
  void FilterUpdate(const std::string& path) override { ADD_FAILURE(); }
  void FilterSerializeData(
      base::DictionaryValue* pref_store_contents) override {
    ADD_FAILURE();
  }

 private:
  // InterceptablePrefFilter implementation.
  void FinalizeFilterOnLoad(
      const PostFilterOnLoadCallback& post_filter_on_load_callback,
      scoped_ptr<base::DictionaryValue> pref_store_contents,
      bool prefs_altered) override {
    post_filter_on_load_callback.Run(pref_store_contents.Pass(), prefs_altered);
  }
};

// A test fixture designed to be used like this:
//  1) Set up initial store prefs with PresetStoreValue().
//  2) Hand both sets of prefs to the migrator via HandPrefsToMigrator().
//  3) Migration completes synchronously when the second store hands its prefs
//     over.
//  4) Verifications can be made via various methods of this fixture.
// Call Reset() to perform a second migration.
class TrackedPreferencesMigrationTest : public testing::Test {
 public:
  enum MockPrefStoreID {
    MOCK_UNPROTECTED_PREF_STORE,
    MOCK_PROTECTED_PREF_STORE,
  };

  TrackedPreferencesMigrationTest()
      : unprotected_prefs_(new base::DictionaryValue),
        protected_prefs_(new base::DictionaryValue),
        migration_modified_unprotected_store_(false),
        migration_modified_protected_store_(false),
        unprotected_store_migration_complete_(false),
        protected_store_migration_complete_(false) {}

  void SetUp() override {
    PrefServiceHashStoreContents::RegisterPrefs(local_state_.registry());
    Reset();
  }

  void Reset() {
    std::set<std::string> unprotected_pref_names;
    std::set<std::string> protected_pref_names;
    unprotected_pref_names.insert(kUnprotectedPref);
    unprotected_pref_names.insert(kPreviouslyProtectedPref);
    protected_pref_names.insert(kProtectedPref);
    protected_pref_names.insert(kPreviouslyUnprotectedPref);

    migration_modified_unprotected_store_ = false;
    migration_modified_protected_store_ = false;
    unprotected_store_migration_complete_ = false;
    protected_store_migration_complete_ = false;

    unprotected_store_successful_write_callback_.Reset();
    protected_store_successful_write_callback_.Reset();

    SetupTrackedPreferencesMigration(
        unprotected_pref_names,
        protected_pref_names,
        base::Bind(&TrackedPreferencesMigrationTest::RemovePathFromStore,
                   base::Unretained(this),
                   MOCK_UNPROTECTED_PREF_STORE),
        base::Bind(&TrackedPreferencesMigrationTest::RemovePathFromStore,
                   base::Unretained(this),
                   MOCK_PROTECTED_PREF_STORE),
        base::Bind(
            &TrackedPreferencesMigrationTest::RegisterSuccessfulWriteClosure,
            base::Unretained(this),
            MOCK_UNPROTECTED_PREF_STORE),
        base::Bind(
            &TrackedPreferencesMigrationTest::RegisterSuccessfulWriteClosure,
            base::Unretained(this),
            MOCK_PROTECTED_PREF_STORE),
        scoped_ptr<PrefHashStore>(
            new PrefHashStoreImpl(kSeed, kDeviceId, false)),
        scoped_ptr<PrefHashStore>(
            new PrefHashStoreImpl(kSeed, kDeviceId, true)),
        scoped_ptr<HashStoreContents>(
            new PrefServiceHashStoreContents(kHashStoreId, &local_state_)),

        &mock_unprotected_pref_filter_,
        &mock_protected_pref_filter_);

    // Verify initial expectations are met.
    EXPECT_TRUE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
    EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
    EXPECT_FALSE(
        WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
    EXPECT_FALSE(
        WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));
  }

 protected:
  // Sets |key| to |value| in the test store identified by |store_id| before
  // migration begins. Also sets the corresponding hash in the same store.
  void PresetStoreValue(MockPrefStoreID store_id,
                        const std::string& key,
                        const std::string value) {
    PresetStoreValueOnly(store_id, key, value);
    PresetStoreValueHash(store_id, key, value);
  }

  // Sets |key| to |value| in the test store identified by |store_id| before
  // migration begins. Stores the value hash in Local State as in M36 and
  // earlier.
  void PresetLegacyStoreValue(MockPrefStoreID store_id,
                              const std::string& key,
                              const std::string value) {
    PresetStoreValueOnly(store_id, key, value);
    PresetLegacyValueHash(key, value);
  }

  // Stores a hash for |key| and |value| in the hash store identified by
  // |store_id| before migration begins.
  void PresetStoreValueHash(MockPrefStoreID store_id,
                            const std::string& key,
                            const std::string value) {
    base::DictionaryValue* store = NULL;
    scoped_ptr<PrefHashStore> pref_hash_store;
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        store = unprotected_prefs_.get();
        pref_hash_store.reset(new PrefHashStoreImpl(kSeed, kDeviceId, false));
        break;
      case MOCK_PROTECTED_PREF_STORE:
        store = protected_prefs_.get();
        pref_hash_store.reset(new PrefHashStoreImpl(kSeed, kDeviceId, true));
        break;
    }
    DCHECK(store);

    base::StringValue string_value(value);
    pref_hash_store->BeginTransaction(
        scoped_ptr<HashStoreContents>(
            new DictionaryHashStoreContents(store)))->StoreHash(
                key, &string_value);
  }

  // Stores a hash for |key| and |value| in the legacy hash store in
  // local_state.
  void PresetLegacyValueHash(const std::string& key,
                             const std::string value) {
    scoped_ptr<PrefHashStore> pref_hash_store(
        new PrefHashStoreImpl(kSeed, kDeviceId, true));

    base::StringValue string_value(value);
    PrefHashStoreImpl(kSeed, kDeviceId, true)
        .BeginTransaction(scoped_ptr<HashStoreContents>(
            new PrefServiceHashStoreContents(kHashStoreId, &local_state_)))
        ->StoreHash(key, &string_value);
  }

  // Returns true if the store opposite to |store_id| is observed for its next
  // successful write.
  bool WasOnSuccessfulWriteCallbackRegistered(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        return !protected_store_successful_write_callback_.is_null();
      case MOCK_PROTECTED_PREF_STORE:
        return !unprotected_store_successful_write_callback_.is_null();
    }
    NOTREACHED();
    return false;
  }

  // Verifies that the (key, value) pairs in |expected_prefs_in_store| are found
  // in the store identified by |store_id|.
  void VerifyValuesStored(
      MockPrefStoreID store_id,
      const base::StringPairs& expected_prefs_in_store) {
    base::DictionaryValue* store = NULL;
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        store = unprotected_prefs_.get();
        break;
      case MOCK_PROTECTED_PREF_STORE:
        store = protected_prefs_.get();
        break;
    }
    DCHECK(store);

    for (base::StringPairs::const_iterator it = expected_prefs_in_store.begin();
         it != expected_prefs_in_store.end(); ++it) {
      std::string val;
      EXPECT_TRUE(store->GetString(it->first, &val));
      EXPECT_EQ(it->second, val);
    }
  }

  // Determines whether |expected_pref_in_hash_store| has a hash in the hash
  // store identified by |store_id|.
  bool ContainsHash(MockPrefStoreID store_id,
                    std::string expected_pref_in_hash_store) {
    base::DictionaryValue* store = NULL;
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        store = unprotected_prefs_.get();
        break;
      case MOCK_PROTECTED_PREF_STORE:
        store = protected_prefs_.get();
        break;
    }
    DCHECK(store);
    const base::DictionaryValue* hash_store_contents =
        DictionaryHashStoreContents(store).GetContents();
    return hash_store_contents &&
           hash_store_contents->GetString(expected_pref_in_hash_store,
                                          static_cast<std::string*>(NULL));
  }

  // Determines whether |expected_pref_in_hash_store| has a hash in the Local
  // State hash store.
  bool ContainsLegacyHash(std::string expected_pref_in_hash_store) {
    const base::DictionaryValue* hash_store_contents =
        PrefServiceHashStoreContents(kHashStoreId, &local_state_).GetContents();
    return hash_store_contents &&
           hash_store_contents->GetString(expected_pref_in_hash_store,
                                          static_cast<std::string*>(NULL));
  }

  // Both stores need to hand their prefs over in order for migration to kick
  // in.
  void HandPrefsToMigrator(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        mock_unprotected_pref_filter_.FilterOnLoad(
            base::Bind(&TrackedPreferencesMigrationTest::GetPrefsBack,
                       base::Unretained(this),
                       MOCK_UNPROTECTED_PREF_STORE),
            unprotected_prefs_.Pass());
        break;
      case MOCK_PROTECTED_PREF_STORE:
        mock_protected_pref_filter_.FilterOnLoad(
            base::Bind(&TrackedPreferencesMigrationTest::GetPrefsBack,
                       base::Unretained(this),
                       MOCK_PROTECTED_PREF_STORE),
            protected_prefs_.Pass());
        break;
    }
  }

  bool HasPrefs(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        return unprotected_prefs_;
      case MOCK_PROTECTED_PREF_STORE:
        return protected_prefs_;
    }
    NOTREACHED();
    return false;
  }

  bool StoreModifiedByMigration(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        return migration_modified_unprotected_store_;
      case MOCK_PROTECTED_PREF_STORE:
        return migration_modified_protected_store_;
    }
    NOTREACHED();
    return false;
  }

  bool MigrationCompleted() {
    return unprotected_store_migration_complete_ &&
        protected_store_migration_complete_;
  }

  void SimulateSuccessfulWrite(MockPrefStoreID store_id) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        EXPECT_FALSE(unprotected_store_successful_write_callback_.is_null());
        unprotected_store_successful_write_callback_.Run();
        unprotected_store_successful_write_callback_.Reset();
        break;
      case MOCK_PROTECTED_PREF_STORE:
        EXPECT_FALSE(protected_store_successful_write_callback_.is_null());
        protected_store_successful_write_callback_.Run();
        protected_store_successful_write_callback_.Reset();
        break;
    }
  }

 private:
  void RegisterSuccessfulWriteClosure(
      MockPrefStoreID store_id,
      const base::Closure& successful_write_closure) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        EXPECT_TRUE(unprotected_store_successful_write_callback_.is_null());
        unprotected_store_successful_write_callback_ = successful_write_closure;
        break;
      case MOCK_PROTECTED_PREF_STORE:
        EXPECT_TRUE(protected_store_successful_write_callback_.is_null());
        protected_store_successful_write_callback_ = successful_write_closure;
        break;
    }
  }

  // Helper given as an InterceptablePrefFilter::FinalizeFilterOnLoadCallback
  // to the migrator to be invoked when it's done.
  void GetPrefsBack(MockPrefStoreID store_id,
                    scoped_ptr<base::DictionaryValue> prefs,
                    bool prefs_altered) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        EXPECT_FALSE(unprotected_prefs_);
        unprotected_prefs_ = prefs.Pass();
        migration_modified_unprotected_store_ = prefs_altered;
        unprotected_store_migration_complete_ = true;
        break;
      case MOCK_PROTECTED_PREF_STORE:
        EXPECT_FALSE(protected_prefs_);
        protected_prefs_ = prefs.Pass();
        migration_modified_protected_store_ = prefs_altered;
        protected_store_migration_complete_ = true;
        break;
    }
  }

  // Helper given as a cleaning callback to the migrator.
  void RemovePathFromStore(MockPrefStoreID store_id, const std::string& key) {
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        ASSERT_TRUE(unprotected_prefs_);
        unprotected_prefs_->RemovePath(key, NULL);
        break;
      case MOCK_PROTECTED_PREF_STORE:
        ASSERT_TRUE(protected_prefs_);
        protected_prefs_->RemovePath(key, NULL);
        break;
    }
  }

  // Sets |key| to |value| in the test store identified by |store_id| before
  // migration begins. Does not store a preference hash.
  void PresetStoreValueOnly(MockPrefStoreID store_id,
                            const std::string& key,
                            const std::string value) {
    base::DictionaryValue* store = NULL;
    switch (store_id) {
      case MOCK_UNPROTECTED_PREF_STORE:
        store = unprotected_prefs_.get();
        break;
      case MOCK_PROTECTED_PREF_STORE:
        store = protected_prefs_.get();
        break;
    }
    DCHECK(store);

    store->SetString(key, value);
  }

  static const char kHashStoreId[];
  static const char kSeed[];
  static const char kDeviceId[];

  scoped_ptr<base::DictionaryValue> unprotected_prefs_;
  scoped_ptr<base::DictionaryValue> protected_prefs_;

  SimpleInterceptablePrefFilter mock_unprotected_pref_filter_;
  SimpleInterceptablePrefFilter mock_protected_pref_filter_;

  base::Closure unprotected_store_successful_write_callback_;
  base::Closure protected_store_successful_write_callback_;

  bool migration_modified_unprotected_store_;
  bool migration_modified_protected_store_;

  bool unprotected_store_migration_complete_;
  bool protected_store_migration_complete_;

  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(TrackedPreferencesMigrationTest);
};

// static
const char TrackedPreferencesMigrationTest::kHashStoreId[] = "hash-store-id";

// static
const char TrackedPreferencesMigrationTest::kSeed[] = "seed";

// static
const char TrackedPreferencesMigrationTest::kDeviceId[] = "device-id";

}  // namespace

TEST_F(TrackedPreferencesMigrationTest, NoMigrationRequired) {
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref,
                   kUnprotectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE, kProtectedPref,
                   kProtectedPrefValue);

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));

  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));

  // Hand unprotected prefs to the migrator which should wait for the protected
  // prefs.
  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  EXPECT_FALSE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(MigrationCompleted());

  // Hand protected prefs to the migrator which should proceed with the
  // migration synchronously.
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  // Prefs should have been handed back over.
  EXPECT_TRUE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_PROTECTED_PREF_STORE));

  base::StringPairs expected_unprotected_values;
  expected_unprotected_values.push_back(
      std::make_pair(kUnprotectedPref, kUnprotectedPrefValue));
  VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE, expected_unprotected_values);

  base::StringPairs expected_protected_values;
  expected_protected_values.push_back(
      std::make_pair(kProtectedPref, kProtectedPrefValue));
  VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));

  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
}

TEST_F(TrackedPreferencesMigrationTest, LegacyHashMigrationOnly) {
  PresetLegacyStoreValue(
      MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref, kUnprotectedPrefValue);
  PresetLegacyStoreValue(
      MOCK_PROTECTED_PREF_STORE, kProtectedPref, kProtectedPrefValue);

  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));

  EXPECT_TRUE(ContainsLegacyHash(kProtectedPref));
  EXPECT_TRUE(ContainsLegacyHash(kUnprotectedPref));

  // Hand unprotected prefs to the migrator which should wait for the protected
  // prefs.
  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  EXPECT_FALSE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(MigrationCompleted());

  // Hand protected prefs to the migrator which should proceed with the
  // migration synchronously.
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  // Prefs should have been handed back over.
  EXPECT_TRUE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));

  // There is no pending cleanup task for the modern hash stores.
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));

  // Both stores were modified as hashes were moved from Local State.
  EXPECT_TRUE(StoreModifiedByMigration(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(StoreModifiedByMigration(MOCK_PROTECTED_PREF_STORE));

  base::StringPairs expected_unprotected_values;
  expected_unprotected_values.push_back(
      std::make_pair(kUnprotectedPref, kUnprotectedPrefValue));
  VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE, expected_unprotected_values);

  base::StringPairs expected_protected_values;
  expected_protected_values.push_back(
      std::make_pair(kProtectedPref, kProtectedPrefValue));
  VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));

  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));

  // The Local State hash store will not be reset until the next run.
  EXPECT_TRUE(ContainsLegacyHash(kProtectedPref));
  EXPECT_TRUE(ContainsLegacyHash(kUnprotectedPref));

  Reset();

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  // Neither store was modified.
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_PROTECTED_PREF_STORE));

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));

  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));

  EXPECT_FALSE(ContainsLegacyHash(kProtectedPref));
  EXPECT_FALSE(ContainsLegacyHash(kUnprotectedPref));
}

TEST_F(TrackedPreferencesMigrationTest, FullMigrationWithLegacyHashStore) {
  // Store some values with matching MACs in Local State.
  PresetLegacyStoreValue(
      MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref, kUnprotectedPrefValue);
  PresetLegacyStoreValue(MOCK_UNPROTECTED_PREF_STORE,
                         kPreviouslyUnprotectedPref,
                         kPreviouslyUnprotectedPrefValue);
  PresetLegacyStoreValue(
      MOCK_PROTECTED_PREF_STORE, kProtectedPref, kProtectedPrefValue);
  PresetLegacyStoreValue(MOCK_PROTECTED_PREF_STORE,
                         kPreviouslyProtectedPref,
                         kPreviouslyProtectedPrefValue);

  // Verify that there are no MACs in Preferences or Secure Preferences.
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  // Verify that there are MACs in Local State.
  EXPECT_TRUE(ContainsLegacyHash(kUnprotectedPref));
  EXPECT_TRUE(ContainsLegacyHash(kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsLegacyHash(kProtectedPref));
  EXPECT_TRUE(ContainsLegacyHash(kPreviouslyProtectedPref));

  // Perform a first-pass migration.
  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  // All values should have been moved to their preferred locations, including
  // MACs.
  base::StringPairs expected_unprotected_values;
  expected_unprotected_values.push_back(
      std::make_pair(kUnprotectedPref, kUnprotectedPrefValue));
  expected_unprotected_values.push_back(
      std::make_pair(kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
  base::StringPairs expected_protected_values;
  expected_protected_values.push_back(
      std::make_pair(kProtectedPref, kProtectedPrefValue));
  expected_protected_values.push_back(std::make_pair(
      kPreviouslyUnprotectedPref, kPreviouslyUnprotectedPrefValue));

  VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE, expected_unprotected_values);
  VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));

  EXPECT_FALSE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  // Removing the values from their previous locations is deferred until the new
  // locations are persisted.
  EXPECT_TRUE(ContainsLegacyHash(kUnprotectedPref));
  EXPECT_TRUE(ContainsLegacyHash(kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsLegacyHash(kProtectedPref));
  EXPECT_TRUE(ContainsLegacyHash(kPreviouslyProtectedPref));

  EXPECT_TRUE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));

  SimulateSuccessfulWrite(MOCK_UNPROTECTED_PREF_STORE);
  SimulateSuccessfulWrite(MOCK_PROTECTED_PREF_STORE);

  Reset();

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  // In this run the MACs should have been removed from their previous
  // locations. There is no more pending action.
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  EXPECT_FALSE(ContainsLegacyHash(kUnprotectedPref));
  EXPECT_FALSE(ContainsLegacyHash(kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsLegacyHash(kProtectedPref));
  EXPECT_FALSE(ContainsLegacyHash(kPreviouslyProtectedPref));

  VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE, expected_unprotected_values);
  VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);
}

TEST_F(TrackedPreferencesMigrationTest, FullMigration) {
  PresetStoreValue(
      MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref, kUnprotectedPrefValue);
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE,
                   kPreviouslyUnprotectedPref,
                   kPreviouslyUnprotectedPrefValue);
  PresetStoreValue(
      MOCK_PROTECTED_PREF_STORE, kProtectedPref, kProtectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE,
                   kPreviouslyProtectedPref,
                   kPreviouslyProtectedPrefValue);

  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  EXPECT_FALSE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(MigrationCompleted());

  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  EXPECT_TRUE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_TRUE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));
  EXPECT_TRUE(StoreModifiedByMigration(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(StoreModifiedByMigration(MOCK_PROTECTED_PREF_STORE));

  // Values should have been migrated to their store, but migrated values should
  // still remain in the source store until cleanup tasks are later invoked.
  {
    base::StringPairs expected_unprotected_values;
    expected_unprotected_values.push_back(std::make_pair(
        kUnprotectedPref, kUnprotectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyUnprotectedPref, kPreviouslyUnprotectedPrefValue));
    VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE,
                       expected_unprotected_values);

    base::StringPairs expected_protected_values;
    expected_protected_values.push_back(std::make_pair(
        kProtectedPref, kProtectedPrefValue));
    expected_protected_values.push_back(std::make_pair(
        kPreviouslyUnprotectedPref, kPreviouslyUnprotectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);

    EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
    EXPECT_TRUE(
        ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
    EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
    EXPECT_TRUE(
        ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

    EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
    EXPECT_TRUE(
        ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
    EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
    EXPECT_TRUE(
        ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));
  }

  // A successful write of the protected pref store should result in a clean up
  // of the unprotected store.
  SimulateSuccessfulWrite(MOCK_PROTECTED_PREF_STORE);

  {
    base::StringPairs expected_unprotected_values;
    expected_unprotected_values.push_back(std::make_pair(
        kUnprotectedPref, kUnprotectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE,
                       expected_unprotected_values);

    base::StringPairs expected_protected_values;
    expected_protected_values.push_back(std::make_pair(
        kProtectedPref, kProtectedPrefValue));
    expected_protected_values.push_back(std::make_pair(
        kPreviouslyUnprotectedPref, kPreviouslyUnprotectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);
  }

  SimulateSuccessfulWrite(MOCK_UNPROTECTED_PREF_STORE);

  {
    base::StringPairs expected_unprotected_values;
    expected_unprotected_values.push_back(std::make_pair(
        kUnprotectedPref, kUnprotectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE,
                       expected_unprotected_values);

    base::StringPairs expected_protected_values;
    expected_protected_values.push_back(std::make_pair(
        kProtectedPref, kProtectedPrefValue));
    expected_protected_values.push_back(std::make_pair(
        kPreviouslyUnprotectedPref, kPreviouslyUnprotectedPrefValue));
    VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);
  }

  // Hashes are not cleaned up yet.
  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  Reset();

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  // Hashes are cleaned up.
  EXPECT_TRUE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_FALSE(ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_UNPROTECTED_PREF_STORE, kPreviouslyProtectedPref));

  EXPECT_FALSE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kUnprotectedPref));
  EXPECT_TRUE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyUnprotectedPref));
  EXPECT_TRUE(ContainsHash(MOCK_PROTECTED_PREF_STORE, kProtectedPref));
  EXPECT_FALSE(
      ContainsHash(MOCK_PROTECTED_PREF_STORE, kPreviouslyProtectedPref));
}

TEST_F(TrackedPreferencesMigrationTest, CleanupOnly) {
  // Already migrated; only cleanup needed.
  PresetStoreValue(
      MOCK_UNPROTECTED_PREF_STORE, kUnprotectedPref, kUnprotectedPrefValue);
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE,
                   kPreviouslyProtectedPref,
                   kPreviouslyProtectedPrefValue);
  PresetStoreValue(MOCK_UNPROTECTED_PREF_STORE,
                   kPreviouslyUnprotectedPref,
                   kPreviouslyUnprotectedPrefValue);
  PresetStoreValue(
      MOCK_PROTECTED_PREF_STORE, kProtectedPref, kProtectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE,
                   kPreviouslyProtectedPref,
                   kPreviouslyProtectedPrefValue);
  PresetStoreValue(MOCK_PROTECTED_PREF_STORE,
                   kPreviouslyUnprotectedPref,
                   kPreviouslyUnprotectedPrefValue);

  HandPrefsToMigrator(MOCK_UNPROTECTED_PREF_STORE);
  EXPECT_FALSE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(MigrationCompleted());

  HandPrefsToMigrator(MOCK_PROTECTED_PREF_STORE);
  EXPECT_TRUE(MigrationCompleted());

  EXPECT_TRUE(HasPrefs(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_TRUE(HasPrefs(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(
      WasOnSuccessfulWriteCallbackRegistered(MOCK_PROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_UNPROTECTED_PREF_STORE));
  EXPECT_FALSE(StoreModifiedByMigration(MOCK_PROTECTED_PREF_STORE));

  // Cleanup should happen synchronously if the values were already present in
  // their destination stores.
  {
    base::StringPairs expected_unprotected_values;
    expected_unprotected_values.push_back(std::make_pair(
        kUnprotectedPref, kUnprotectedPrefValue));
    expected_unprotected_values.push_back(std::make_pair(
        kPreviouslyProtectedPref, kPreviouslyProtectedPrefValue));
    VerifyValuesStored(MOCK_UNPROTECTED_PREF_STORE,
                       expected_unprotected_values);

    base::StringPairs expected_protected_values;
    expected_protected_values.push_back(std::make_pair(
        kProtectedPref, kProtectedPrefValue));
    expected_protected_values.push_back(std::make_pair(
        kPreviouslyUnprotectedPref, kPreviouslyUnprotectedPrefValue));
    VerifyValuesStored(MOCK_PROTECTED_PREF_STORE, expected_protected_values);
  }
}
