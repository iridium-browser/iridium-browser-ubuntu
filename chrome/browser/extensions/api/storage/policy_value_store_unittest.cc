// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/policy_value_store.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/value_store/leveldb_value_store.h"
#include "extensions/browser/value_store/value_store_unittest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace extensions {

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kDatabaseUMAClientName[] = "Test";

class MockSettingsObserver : public SettingsObserver {
 public:
  MOCK_METHOD3(OnSettingsChanged, void(
      const std::string& extension_id,
      settings_namespace::Namespace settings_namespace,
      const std::string& changes_json));
};

// Extends PolicyValueStore by overriding the mutating methods, so that the
// Get() base implementation can be tested with the ValueStoreTest parameterized
// tests.
class MutablePolicyValueStore : public PolicyValueStore {
 public:
  explicit MutablePolicyValueStore(const base::FilePath& path)
      : PolicyValueStore(
            kTestExtensionId,
            make_scoped_refptr(new SettingsObserverList()),
            make_scoped_ptr(
                new LeveldbValueStore(kDatabaseUMAClientName, path))) {}
  ~MutablePolicyValueStore() override {}

  WriteResult Set(WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override {
    return delegate()->Set(options, key, value);
  }

  WriteResult Set(WriteOptions options,
                  const base::DictionaryValue& values) override {
    return delegate()->Set(options, values);
  }

  WriteResult Remove(const std::string& key) override {
    return delegate()->Remove(key);
  }

  WriteResult Remove(const std::vector<std::string>& keys) override {
    return delegate()->Remove(keys);
  }

  WriteResult Clear() override { return delegate()->Clear(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MutablePolicyValueStore);
};

ValueStore* Param(const base::FilePath& file_path) {
  return new MutablePolicyValueStore(file_path);
}

}  // namespace

INSTANTIATE_TEST_CASE_P(
    PolicyValueStoreTest,
    ValueStoreTest,
    testing::Values(&Param));

class PolicyValueStoreTest : public testing::Test {
 public:
  PolicyValueStoreTest()
      : file_thread_(content::BrowserThread::FILE, &loop_) {}
  ~PolicyValueStoreTest() override {}

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    observers_ = new SettingsObserverList();
    observers_->AddObserver(&observer_);
    store_.reset(new PolicyValueStore(
        kTestExtensionId, observers_,
        make_scoped_ptr(new LeveldbValueStore(kDatabaseUMAClientName,
                                              scoped_temp_dir_.path()))));
  }

  void TearDown() override {
    observers_->RemoveObserver(&observer_);
    store_.reset();
  }

 protected:
  base::ScopedTempDir scoped_temp_dir_;
  base::MessageLoop loop_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<PolicyValueStore> store_;
  MockSettingsObserver observer_;
  scoped_refptr<SettingsObserverList> observers_;
};

TEST_F(PolicyValueStoreTest, DontProvideRecommendedPolicies) {
  policy::PolicyMap policies;
  base::FundamentalValue expected(123);
  policies.Set("must", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, expected.DeepCopy(), NULL);
  policies.Set("may", policy::POLICY_LEVEL_RECOMMENDED,
               policy::POLICY_SCOPE_USER,
               new base::FundamentalValue(456), NULL);
  store_->SetCurrentPolicy(policies);
  ValueStore::ReadResult result = store_->Get();
  ASSERT_FALSE(result->HasError());
  EXPECT_EQ(1u, result->settings().size());
  base::Value* value = NULL;
  EXPECT_FALSE(result->settings().Get("may", &value));
  EXPECT_TRUE(result->settings().Get("must", &value));
  EXPECT_TRUE(base::Value::Equals(&expected, value));
}

TEST_F(PolicyValueStoreTest, ReadOnly) {
  ValueStore::WriteOptions options = ValueStore::DEFAULTS;

  base::StringValue string_value("value");
  EXPECT_TRUE(store_->Set(options, "key", string_value)->HasError());

  base::DictionaryValue dict;
  dict.SetString("key", "value");
  EXPECT_TRUE(store_->Set(options, dict)->HasError());

  EXPECT_TRUE(store_->Remove("key")->HasError());
  std::vector<std::string> keys;
  keys.push_back("key");
  EXPECT_TRUE(store_->Remove(keys)->HasError());
  EXPECT_TRUE(store_->Clear()->HasError());
}

TEST_F(PolicyValueStoreTest, NotifyOnChanges) {
  // Notify when setting the initial policy.
  const base::StringValue value("111");
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange("aaa", NULL, value.DeepCopy()));
    EXPECT_CALL(observer_,
                OnSettingsChanged(kTestExtensionId,
                                  settings_namespace::MANAGED,
                                  ValueStoreChange::ToJson(changes)));
  }

  policy::PolicyMap policies;
  policies.Set("aaa", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               value.DeepCopy(), NULL);
  store_->SetCurrentPolicy(policies);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);

  // Notify when new policies are added.
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange("bbb", NULL, value.DeepCopy()));
    EXPECT_CALL(observer_,
                OnSettingsChanged(kTestExtensionId,
                                  settings_namespace::MANAGED,
                                  ValueStoreChange::ToJson(changes)));
  }

  policies.Set("bbb", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               value.DeepCopy(), NULL);
  store_->SetCurrentPolicy(policies);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);

  // Notify when policies change.
  const base::StringValue new_value("222");
  {
    ValueStoreChangeList changes;
    changes.push_back(
        ValueStoreChange("bbb", value.DeepCopy(), new_value.DeepCopy()));
    EXPECT_CALL(observer_,
                OnSettingsChanged(kTestExtensionId,
                                  settings_namespace::MANAGED,
                                  ValueStoreChange::ToJson(changes)));
  }

  policies.Set("bbb", policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               new_value.DeepCopy(), NULL);
  store_->SetCurrentPolicy(policies);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);

  // Notify when policies are removed.
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange("bbb", new_value.DeepCopy(), NULL));
    EXPECT_CALL(observer_,
                OnSettingsChanged(kTestExtensionId,
                                  settings_namespace::MANAGED,
                                  ValueStoreChange::ToJson(changes)));
  }

  policies.Erase("bbb");
  store_->SetCurrentPolicy(policies);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);

  // Don't notify when there aren't any changes.
  EXPECT_CALL(observer_, OnSettingsChanged(_, _, _)).Times(0);
  store_->SetCurrentPolicy(policies);
  loop_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace extensions
