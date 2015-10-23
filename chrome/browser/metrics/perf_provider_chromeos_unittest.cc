// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf_provider_chromeos.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/metrics/windowed_incognito_observer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/login_state.h"
#include "components/metrics/proto/sampled_profile.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

// Return values for perf.
const int kPerfSuccess = 0;
const int kPerfFailure = 1;

// Converts a protobuf to serialized format as a byte vector.
std::vector<uint8_t> SerializeMessageToVector(
    const google::protobuf::MessageLite& message) {
  std::vector<uint8_t> result(message.ByteSize());
  message.SerializeToArray(result.data(), result.size());
  return result;
}

// Returns an example PerfDataProto. The contents don't have to make sense. They
// just need to constitute a semantically valid protobuf.
// |proto| is an output parameter that will contain the created protobuf.
PerfDataProto GetExamplePerfDataProto() {
  PerfDataProto proto;
  proto.set_timestamp_sec(1435604013);  // Time since epoch in seconds->

  PerfDataProto_PerfFileAttr* file_attr = proto.add_file_attrs();
  file_attr->add_ids(61);
  file_attr->add_ids(62);
  file_attr->add_ids(63);

  PerfDataProto_PerfEventAttr* attr = file_attr->mutable_attr();
  attr->set_type(1);
  attr->set_size(2);
  attr->set_config(3);
  attr->set_sample_period(4);
  attr->set_sample_freq(5);

  PerfDataProto_PerfEventStats* stats = proto.mutable_stats();
  stats->set_num_events_read(100);
  stats->set_num_sample_events(200);
  stats->set_num_mmap_events(300);
  stats->set_num_fork_events(400);
  stats->set_num_exit_events(500);

  return proto;
}

// Returns an example PerfStatProto. The contents don't have to make sense. They
// just need to constitute a semantically valid protobuf.
// |result| is an output parameter that will contain the created protobuf.
PerfStatProto GetExamplePerfStatProto() {
  PerfStatProto proto;
  proto.set_command_line(
      "perf stat -a -e cycles -e instructions -e branches -- sleep 2");

  PerfStatProto_PerfStatLine* line1 = proto.add_line();
  line1->set_time_ms(1000);
  line1->set_count(2000);
  line1->set_event("cycles");

  PerfStatProto_PerfStatLine* line2 = proto.add_line();
  line2->set_time_ms(2000);
  line2->set_count(5678);
  line2->set_event("instructions");

  PerfStatProto_PerfStatLine* line3 = proto.add_line();
  line3->set_time_ms(3000);
  line3->set_count(9999);
  line3->set_event("branches");

  return proto;
}

// Allows testing of PerfProvider behavior when an incognito window is opened.
class TestIncognitoObserver : public WindowedIncognitoObserver {
 public:
  // Factory function to create a TestIncognitoObserver object contained in a
  // scoped_ptr<WindowedIncognitoObserver> object. |incognito_launched|
  // simulates the presence of an open incognito window, or the lack thereof.
  // Used for passing observers to ParseOutputProtoIfValid().
  static scoped_ptr<WindowedIncognitoObserver> CreateWithIncognitoLaunched(
      bool incognito_launched) {
    scoped_ptr<TestIncognitoObserver> observer(new TestIncognitoObserver);
    observer->set_incognito_launched(incognito_launched);
    return observer.Pass();
  }

 private:
  TestIncognitoObserver() {}

  DISALLOW_COPY_AND_ASSIGN(TestIncognitoObserver);
};

// Allows access to PerfProvider::ParseOutputProtoIfValid() for testing.
class TestPerfProvider : public PerfProvider {
 public:
  TestPerfProvider() {}

  using PerfProvider::ParseOutputProtoIfValid;

 private:
  std::vector<SampledProfile> stored_profiles_;

  DISALLOW_COPY_AND_ASSIGN(TestPerfProvider);
};

}  // namespace

class PerfProviderTest : public testing::Test {
 public:
  PerfProviderTest() : task_runner_(new base::TestSimpleTaskRunner),
                       task_runner_handle_(task_runner_),
                       perf_data_proto_(GetExamplePerfDataProto()),
                       perf_stat_proto_(GetExamplePerfStatProto()) {}

  void SetUp() override {
    // PerfProvider requires chromeos::LoginState and
    // chromeos::DBusThreadManagerto be initialized.
    chromeos::LoginState::Initialize();
    chromeos::DBusThreadManager::Initialize();

    perf_provider_.reset(new TestPerfProvider);

    // PerfProvider requires the user to be logged in.
    chromeos::LoginState::Get()->SetLoggedInState(
        chromeos::LoginState::LOGGED_IN_ACTIVE,
        chromeos::LoginState::LOGGED_IN_USER_REGULAR);
  }

  void TearDown() override {
    perf_provider_.reset();
    chromeos::DBusThreadManager::Shutdown();
    chromeos::LoginState::Shutdown();
  }

 protected:
  scoped_ptr<TestPerfProvider> perf_provider_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  // These store example perf data/stat protobufs for testing.
  PerfDataProto perf_data_proto_;
  PerfStatProto perf_stat_proto_;

  DISALLOW_COPY_AND_ASSIGN(PerfProviderTest);
};

TEST_F(PerfProviderTest, CheckSetup) {
  EXPECT_GT(perf_data_proto_.ByteSize(), 0);
  EXPECT_GT(perf_stat_proto_.ByteSize(), 0);

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(perf_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());

  EXPECT_FALSE(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false)->
          incognito_launched());
  EXPECT_TRUE(
      TestIncognitoObserver::CreateWithIncognitoLaunched(true)->
          incognito_launched());
}

TEST_F(PerfProviderTest, NoPerfData) {
  scoped_ptr<SampledProfile> sampled_profile(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      std::vector<uint8_t>(),
      std::vector<uint8_t>());

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(perf_provider_->GetSampledProfiles(&stored_profiles));
}

TEST_F(PerfProviderTest, PerfDataProtoOnly) {
  scoped_ptr<SampledProfile> sampled_profile(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      SerializeMessageToVector(perf_data_proto_),
      std::vector<uint8_t>());

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(perf_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_ms_after_login());

  ASSERT_TRUE(profile.has_perf_data());
  EXPECT_FALSE(profile.has_perf_stat());
  EXPECT_EQ(SerializeMessageToVector(perf_data_proto_),
            SerializeMessageToVector(profile.perf_data()));
}

TEST_F(PerfProviderTest, PerfStatProtoOnly) {
  scoped_ptr<SampledProfile> sampled_profile(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      std::vector<uint8_t>(),
      SerializeMessageToVector(perf_stat_proto_));

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(perf_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(1U, stored_profiles.size());

  const SampledProfile& profile = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile.trigger_event());
  EXPECT_TRUE(profile.has_ms_after_login());

  EXPECT_FALSE(profile.has_perf_data());
  ASSERT_TRUE(profile.has_perf_stat());
  EXPECT_EQ(SerializeMessageToVector(perf_stat_proto_),
            SerializeMessageToVector(profile.perf_stat()));
}

TEST_F(PerfProviderTest, BothPerfDataProtoAndPerfStatProto) {
  scoped_ptr<SampledProfile> sampled_profile(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      SerializeMessageToVector(perf_data_proto_),
      SerializeMessageToVector(perf_stat_proto_));

  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(perf_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

TEST_F(PerfProviderTest, InvalidPerfOutputResult) {
  scoped_ptr<SampledProfile> sampled_profile(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfFailure,
      SerializeMessageToVector(perf_data_proto_),
      std::vector<uint8_t>());

  // Should not have been stored.
  std::vector<SampledProfile> stored_profiles;
  EXPECT_FALSE(perf_provider_->GetSampledProfiles(&stored_profiles));
  EXPECT_TRUE(stored_profiles.empty());
}

// Change |sampled_profile| between calls to ParseOutputProtoIfValid().
TEST_F(PerfProviderTest, MultipleCalls) {
  scoped_ptr<SampledProfile> sampled_profile(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      SerializeMessageToVector(perf_data_proto_),
      std::vector<uint8_t>());

  sampled_profile.reset(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::RESTORE_SESSION);
  sampled_profile->set_ms_after_restore(3000);
  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      std::vector<uint8_t>(),
      SerializeMessageToVector(perf_stat_proto_));

  sampled_profile.reset(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::RESUME_FROM_SUSPEND);
  sampled_profile->set_suspend_duration_ms(60000);
  sampled_profile->set_ms_after_resume(1500);
  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      SerializeMessageToVector(perf_data_proto_),
      std::vector<uint8_t>());

  sampled_profile.reset(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      std::vector<uint8_t>(),
      SerializeMessageToVector(perf_stat_proto_));

  std::vector<SampledProfile> stored_profiles;
  EXPECT_TRUE(perf_provider_->GetSampledProfiles(&stored_profiles));
  ASSERT_EQ(4U, stored_profiles.size());

  const SampledProfile& profile1 = stored_profiles[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile1.trigger_event());
  EXPECT_TRUE(profile1.has_ms_after_login());
  ASSERT_TRUE(profile1.has_perf_data());
  EXPECT_FALSE(profile1.has_perf_stat());
  EXPECT_EQ(SerializeMessageToVector(perf_data_proto_),
            SerializeMessageToVector(profile1.perf_data()));

  const SampledProfile& profile2 = stored_profiles[1];
  EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile2.trigger_event());
  EXPECT_TRUE(profile2.has_ms_after_login());
  EXPECT_EQ(3000, profile2.ms_after_restore());
  EXPECT_FALSE(profile2.has_perf_data());
  ASSERT_TRUE(profile2.has_perf_stat());
  EXPECT_EQ(SerializeMessageToVector(perf_stat_proto_),
            SerializeMessageToVector(profile2.perf_stat()));

  const SampledProfile& profile3 = stored_profiles[2];
  EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile3.trigger_event());
  EXPECT_TRUE(profile3.has_ms_after_login());
  EXPECT_EQ(60000, profile3.suspend_duration_ms());
  EXPECT_EQ(1500, profile3.ms_after_resume());
  ASSERT_TRUE(profile3.has_perf_data());
  EXPECT_FALSE(profile3.has_perf_stat());
  EXPECT_EQ(SerializeMessageToVector(perf_data_proto_),
            SerializeMessageToVector(profile3.perf_data()));

  const SampledProfile& profile4 = stored_profiles[3];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile4.trigger_event());
  EXPECT_TRUE(profile4.has_ms_after_login());
  EXPECT_FALSE(profile4.has_perf_data());
  ASSERT_TRUE(profile4.has_perf_stat());
  EXPECT_EQ(SerializeMessageToVector(perf_stat_proto_),
            SerializeMessageToVector(profile4.perf_stat()));
}

// Simulate opening and closing of incognito window in between calls to
// ParseOutputProtoIfValid().
TEST_F(PerfProviderTest, IncognitoWindowOpened) {
  scoped_ptr<SampledProfile> sampled_profile(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);

  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      SerializeMessageToVector(perf_data_proto_),
      std::vector<uint8_t>());

  std::vector<SampledProfile> stored_profiles1;
  EXPECT_TRUE(perf_provider_->GetSampledProfiles(&stored_profiles1));
  ASSERT_EQ(1U, stored_profiles1.size());

  const SampledProfile& profile1 = stored_profiles1[0];
  EXPECT_EQ(SampledProfile::PERIODIC_COLLECTION, profile1.trigger_event());
  EXPECT_TRUE(profile1.has_ms_after_login());
  ASSERT_TRUE(profile1.has_perf_data());
  EXPECT_FALSE(profile1.has_perf_stat());
  EXPECT_EQ(SerializeMessageToVector(perf_data_proto_),
            SerializeMessageToVector(profile1.perf_data()));

  sampled_profile.reset(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::RESTORE_SESSION);
  sampled_profile->set_ms_after_restore(3000);
  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      std::vector<uint8_t>(),
      SerializeMessageToVector(perf_stat_proto_));

  std::vector<SampledProfile> stored_profiles2;
  EXPECT_TRUE(perf_provider_->GetSampledProfiles(&stored_profiles2));
  ASSERT_EQ(1U, stored_profiles2.size());

  const SampledProfile& profile2 = stored_profiles2[0];
  EXPECT_EQ(SampledProfile::RESTORE_SESSION, profile2.trigger_event());
  EXPECT_TRUE(profile2.has_ms_after_login());
  EXPECT_EQ(3000, profile2.ms_after_restore());
  EXPECT_FALSE(profile2.has_perf_data());
  ASSERT_TRUE(profile2.has_perf_stat());
  EXPECT_EQ(SerializeMessageToVector(perf_stat_proto_),
            SerializeMessageToVector(profile2.perf_stat()));

  sampled_profile.reset(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::RESUME_FROM_SUSPEND);
  // An incognito window opens.
  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(true),
      sampled_profile.Pass(),
      kPerfSuccess,
      SerializeMessageToVector(perf_data_proto_),
      std::vector<uint8_t>());

  std::vector<SampledProfile> stored_profiles_empty;
  EXPECT_FALSE(perf_provider_->GetSampledProfiles(&stored_profiles_empty));

  sampled_profile.reset(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::PERIODIC_COLLECTION);
  // Incognito window is still open.
  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(true),
      sampled_profile.Pass(),
      kPerfSuccess,
      std::vector<uint8_t>(),
      SerializeMessageToVector(perf_stat_proto_));

  EXPECT_FALSE(perf_provider_->GetSampledProfiles(&stored_profiles_empty));

  sampled_profile.reset(new SampledProfile);
  sampled_profile->set_trigger_event(SampledProfile::RESUME_FROM_SUSPEND);
  sampled_profile->set_suspend_duration_ms(60000);
  sampled_profile->set_ms_after_resume(1500);
  // Incognito window closes.
  perf_provider_->ParseOutputProtoIfValid(
      TestIncognitoObserver::CreateWithIncognitoLaunched(false),
      sampled_profile.Pass(),
      kPerfSuccess,
      SerializeMessageToVector(perf_data_proto_),
      std::vector<uint8_t>());

  std::vector<SampledProfile> stored_profiles3;
  EXPECT_TRUE(perf_provider_->GetSampledProfiles(&stored_profiles3));
  ASSERT_EQ(1U, stored_profiles3.size());

  const SampledProfile& profile3 = stored_profiles3[0];
  EXPECT_EQ(SampledProfile::RESUME_FROM_SUSPEND, profile3.trigger_event());
  EXPECT_TRUE(profile3.has_ms_after_login());
  EXPECT_EQ(60000, profile3.suspend_duration_ms());
  EXPECT_EQ(1500, profile3.ms_after_resume());
  ASSERT_TRUE(profile3.has_perf_data());
  EXPECT_FALSE(profile3.has_perf_stat());
  EXPECT_EQ(SerializeMessageToVector(perf_data_proto_),
            SerializeMessageToVector(profile3.perf_data()));
}

}  // namespace metrics
