// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_client_impl.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/base/fake_encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/fake_connection_factory.h"
#include "google_apis/gcm/engine/fake_connection_handler.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

enum LastEvent {
  NONE,
  LOADING_COMPLETED,
  REGISTRATION_COMPLETED,
  UNREGISTRATION_COMPLETED,
  MESSAGE_SEND_ERROR,
  MESSAGE_SEND_ACK,
  MESSAGE_RECEIVED,
  MESSAGES_DELETED,
};

const uint64 kDeviceAndroidId = 54321;
const uint64 kDeviceSecurityToken = 12345;
const uint64 kDeviceAndroidId2 = 11111;
const uint64 kDeviceSecurityToken2 = 2222;
const int64 kSettingsCheckinInterval = 16 * 60 * 60;
const char kAppId[] = "app_id";
const char kSender[] = "project_id";
const char kSender2[] = "project_id2";
const char kSender3[] = "project_id3";
const char kRegistrationResponsePrefix[] = "token=";
const char kUnregistrationResponsePrefix[] = "deleted=";

// Helper for building arbitrary data messages.
MCSMessage BuildDownstreamMessage(
    const std::string& project_id,
    const std::string& app_id,
    const std::map<std::string, std::string>& data) {
  mcs_proto::DataMessageStanza data_message;
  data_message.set_from(project_id);
  data_message.set_category(app_id);
  for (std::map<std::string, std::string>::const_iterator iter = data.begin();
       iter != data.end();
       ++iter) {
    mcs_proto::AppData* app_data = data_message.add_app_data();
    app_data->set_key(iter->first);
    app_data->set_value(iter->second);
  }
  return MCSMessage(kDataMessageStanzaTag, data_message);
}

GCMClient::AccountTokenInfo MakeAccountToken(const std::string& email,
                                             const std::string& token) {
  GCMClient::AccountTokenInfo account_token;
  account_token.email = email;
  account_token.access_token = token;
  return account_token;
}

std::map<std::string, std::string> MakeEmailToTokenMap(
    const std::vector<GCMClient::AccountTokenInfo>& account_tokens) {
  std::map<std::string, std::string> email_token_map;
  for (std::vector<GCMClient::AccountTokenInfo>::const_iterator iter =
           account_tokens.begin(); iter != account_tokens.end(); ++iter) {
    email_token_map[iter->email] = iter->access_token;
  }
  return email_token_map;
}

class FakeMCSClient : public MCSClient {
 public:
  FakeMCSClient(base::Clock* clock,
                ConnectionFactory* connection_factory,
                GCMStore* gcm_store,
                GCMStatsRecorder* recorder);
  ~FakeMCSClient() override;
  void Login(uint64 android_id, uint64 security_token) override;
  void SendMessage(const MCSMessage& message) override;

  uint64 last_android_id() const { return last_android_id_; }
  uint64 last_security_token() const { return last_security_token_; }
  uint8 last_message_tag() const { return last_message_tag_; }
  const mcs_proto::DataMessageStanza& last_data_message_stanza() const {
    return last_data_message_stanza_;
  }

 private:
  uint64 last_android_id_;
  uint64 last_security_token_;
  uint8 last_message_tag_;
  mcs_proto::DataMessageStanza last_data_message_stanza_;
};

FakeMCSClient::FakeMCSClient(base::Clock* clock,
                             ConnectionFactory* connection_factory,
                             GCMStore* gcm_store,
                             GCMStatsRecorder* recorder)
    : MCSClient("", clock, connection_factory, gcm_store, recorder),
      last_android_id_(0u),
      last_security_token_(0u),
      last_message_tag_(kNumProtoTypes) {
}

FakeMCSClient::~FakeMCSClient() {
}

void FakeMCSClient::Login(uint64 android_id, uint64 security_token) {
  last_android_id_ = android_id;
  last_security_token_ = security_token;
}

void FakeMCSClient::SendMessage(const MCSMessage& message) {
  last_message_tag_ = message.tag();
  if (last_message_tag_ == kDataMessageStanzaTag) {
    last_data_message_stanza_.CopyFrom(
        reinterpret_cast<const mcs_proto::DataMessageStanza&>(
            message.GetProtobuf()));
  }
}

class AutoAdvancingTestClock : public base::Clock {
 public:
  explicit AutoAdvancingTestClock(base::TimeDelta auto_increment_time_delta);
  ~AutoAdvancingTestClock() override;

  base::Time Now() override;
  void Advance(TimeDelta delta);
  int call_count() const { return call_count_; }

 private:
  int call_count_;
  base::TimeDelta auto_increment_time_delta_;
  base::Time now_;

  DISALLOW_COPY_AND_ASSIGN(AutoAdvancingTestClock);
};

AutoAdvancingTestClock::AutoAdvancingTestClock(
    base::TimeDelta auto_increment_time_delta)
    : call_count_(0), auto_increment_time_delta_(auto_increment_time_delta) {
}

AutoAdvancingTestClock::~AutoAdvancingTestClock() {
}

base::Time AutoAdvancingTestClock::Now() {
  call_count_++;
  now_ += auto_increment_time_delta_;
  return now_;
}

void AutoAdvancingTestClock::Advance(base::TimeDelta delta) {
  now_ += delta;
}

class FakeGCMInternalsBuilder : public GCMInternalsBuilder {
 public:
  FakeGCMInternalsBuilder(base::TimeDelta clock_step);
  ~FakeGCMInternalsBuilder() override;

  scoped_ptr<base::Clock> BuildClock() override;
  scoped_ptr<MCSClient> BuildMCSClient(const std::string& version,
                                       base::Clock* clock,
                                       ConnectionFactory* connection_factory,
                                       GCMStore* gcm_store,
                                       GCMStatsRecorder* recorder) override;
  scoped_ptr<ConnectionFactory> BuildConnectionFactory(
      const std::vector<GURL>& endpoints,
      const net::BackoffEntry::Policy& backoff_policy,
      const scoped_refptr<net::HttpNetworkSession>& gcm_network_session,
      const scoped_refptr<net::HttpNetworkSession>& http_network_session,
      net::NetLog* net_log,
      GCMStatsRecorder* recorder) override;

 private:
  base::TimeDelta clock_step_;
};

FakeGCMInternalsBuilder::FakeGCMInternalsBuilder(base::TimeDelta clock_step)
    : clock_step_(clock_step) {
}

FakeGCMInternalsBuilder::~FakeGCMInternalsBuilder() {}

scoped_ptr<base::Clock> FakeGCMInternalsBuilder::BuildClock() {
  return make_scoped_ptr<base::Clock>(new AutoAdvancingTestClock(clock_step_));
}

scoped_ptr<MCSClient> FakeGCMInternalsBuilder::BuildMCSClient(
    const std::string& version,
    base::Clock* clock,
    ConnectionFactory* connection_factory,
    GCMStore* gcm_store,
    GCMStatsRecorder* recorder) {
  return make_scoped_ptr<MCSClient>(new FakeMCSClient(clock,
                                                      connection_factory,
                                                      gcm_store,
                                                      recorder));
}

scoped_ptr<ConnectionFactory> FakeGCMInternalsBuilder::BuildConnectionFactory(
    const std::vector<GURL>& endpoints,
    const net::BackoffEntry::Policy& backoff_policy,
    const scoped_refptr<net::HttpNetworkSession>& gcm_network_session,
    const scoped_refptr<net::HttpNetworkSession>& http_network_session,
    net::NetLog* net_log,
    GCMStatsRecorder* recorder) {
  return make_scoped_ptr<ConnectionFactory>(new FakeConnectionFactory());
}

}  // namespace

class GCMClientImplTest : public testing::Test,
                          public GCMClient::Delegate {
 public:
  GCMClientImplTest();
  ~GCMClientImplTest() override;

  void SetUp() override;

  void SetUpUrlFetcherFactory();

  void BuildGCMClient(base::TimeDelta clock_step);
  void InitializeGCMClient();
  void StartGCMClient();
  void ReceiveMessageFromMCS(const MCSMessage& message);
  void ReceiveOnMessageSentToMCS(
      const std::string& app_id,
      const std::string& message_id,
      const MCSClient::MessageSendStatus status);
  void CompleteCheckin(uint64 android_id,
                       uint64 security_token,
                       const std::string& digest,
                       const std::map<std::string, std::string>& settings);
  void CompleteRegistration(const std::string& registration_id);
  void CompleteUnregistration(const std::string& app_id);
  void VerifyPendingRequestFetcherDeleted();

  bool ExistsRegistration(const std::string& app_id) const;
  void AddRegistration(const std::string& app_id,
                       const std::vector<std::string>& sender_ids,
                       const std::string& registration_id);

  // GCMClient::Delegate overrides (for verification).
  void OnRegisterFinished(const std::string& app_id,
                          const std::string& registration_id,
                          GCMClient::Result result) override;
  void OnUnregisterFinished(const std::string& app_id,
                            GCMClient::Result result) override;
  void OnSendFinished(const std::string& app_id,
                      const std::string& message_id,
                      GCMClient::Result result) override {}
  void OnMessageReceived(const std::string& registration_id,
                         const GCMClient::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnMessageSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  void OnGCMReady(const std::vector<AccountMapping>& account_mappings,
                  const base::Time& last_token_fetch_time) override;
  void OnActivityRecorded() override {}
  void OnConnected(const net::IPEndPoint& ip_endpoint) override {}
  void OnDisconnected() override {}

  GCMClientImpl* gcm_client() const { return gcm_client_.get(); }
  GCMClientImpl::State gcm_client_state() const {
    return gcm_client_->state_;
  }
  FakeMCSClient* mcs_client() const {
    return reinterpret_cast<FakeMCSClient*>(gcm_client_->mcs_client_.get());
  }
  ConnectionFactory* connection_factory() const {
    return gcm_client_->connection_factory_.get();
  }

  const GCMClientImpl::CheckinInfo& device_checkin_info() const {
    return gcm_client_->device_checkin_info_;
  }

  void reset_last_event() {
    last_event_ = NONE;
    last_app_id_.clear();
    last_registration_id_.clear();
    last_message_id_.clear();
    last_result_ = GCMClient::UNKNOWN_ERROR;
    last_account_mappings_.clear();
    last_token_fetch_time_ = base::Time();
  }

  LastEvent last_event() const { return last_event_; }
  const std::string& last_app_id() const { return last_app_id_; }
  const std::string& last_registration_id() const {
    return last_registration_id_;
  }
  const std::string& last_message_id() const { return last_message_id_; }
  GCMClient::Result last_result() const { return last_result_; }
  const GCMClient::IncomingMessage& last_message() const {
    return last_message_;
  }
  const GCMClient::SendErrorDetails& last_error_details() const {
    return last_error_details_;
  }
  const base::Time& last_token_fetch_time() const {
    return last_token_fetch_time_;
  }
  const std::vector<AccountMapping>& last_account_mappings() {
    return last_account_mappings_;
  }

  const GServicesSettings& gservices_settings() const {
    return gcm_client_->gservices_settings_;
  }

  const base::FilePath& temp_directory_path() const {
    return temp_directory_.path();
  }

  int64 CurrentTime();

  // Tooling.
  void PumpLoop();
  void PumpLoopUntilIdle();
  void QuitLoop();
  void InitializeLoop();
  bool CreateUniqueTempDir();
  AutoAdvancingTestClock* clock() const {
    return reinterpret_cast<AutoAdvancingTestClock*>(gcm_client_->clock_.get());
  }

 private:
  // Variables used for verification.
  LastEvent last_event_;
  std::string last_app_id_;
  std::string last_registration_id_;
  std::string last_message_id_;
  GCMClient::Result last_result_;
  GCMClient::IncomingMessage last_message_;
  GCMClient::SendErrorDetails last_error_details_;
  base::Time last_token_fetch_time_;
  std::vector<AccountMapping> last_account_mappings_;

  scoped_ptr<GCMClientImpl> gcm_client_;

  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;

  // Injected to GCM client:
  base::ScopedTempDir temp_directory_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
};

GCMClientImplTest::GCMClientImplTest()
    : last_event_(NONE),
      last_result_(GCMClient::UNKNOWN_ERROR),
      url_request_context_getter_(new net::TestURLRequestContextGetter(
          message_loop_.message_loop_proxy())) {
}

GCMClientImplTest::~GCMClientImplTest() {}

void GCMClientImplTest::SetUp() {
  testing::Test::SetUp();
  ASSERT_TRUE(CreateUniqueTempDir());
  InitializeLoop();
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  StartGCMClient();
  SetUpUrlFetcherFactory();
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  std::string(),
                  std::map<std::string, std::string>());
}

void GCMClientImplTest::SetUpUrlFetcherFactory() {
  url_fetcher_factory_.set_remove_fetcher_on_delete(true);
}

void GCMClientImplTest::PumpLoop() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void GCMClientImplTest::PumpLoopUntilIdle() {
  run_loop_->RunUntilIdle();
  run_loop_.reset(new base::RunLoop());
}

void GCMClientImplTest::QuitLoop() {
  if (run_loop_ && run_loop_->running())
    run_loop_->Quit();
}

void GCMClientImplTest::InitializeLoop() {
  run_loop_.reset(new base::RunLoop);
}

bool GCMClientImplTest::CreateUniqueTempDir() {
  return temp_directory_.CreateUniqueTempDir();
}

void GCMClientImplTest::BuildGCMClient(base::TimeDelta clock_step) {
  gcm_client_.reset(new GCMClientImpl(make_scoped_ptr<GCMInternalsBuilder>(
      new FakeGCMInternalsBuilder(clock_step))));
}

void GCMClientImplTest::CompleteCheckin(
    uint64 android_id,
    uint64 security_token,
    const std::string& digest,
    const std::map<std::string, std::string>& settings) {
  checkin_proto::AndroidCheckinResponse response;
  response.set_stats_ok(true);
  response.set_android_id(android_id);
  response.set_security_token(security_token);

  // For testing G-services settings.
  if (!digest.empty()) {
    response.set_digest(digest);
    for (std::map<std::string, std::string>::const_iterator it =
             settings.begin();
         it != settings.end();
         ++it) {
      checkin_proto::GservicesSetting* setting = response.add_setting();
      setting->set_name(it->first);
      setting->set_value(it->second);
    }
    response.set_settings_diff(false);
  }

  std::string response_string;
  response.SerializeToString(&response_string);

  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(response_string);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void GCMClientImplTest::CompleteRegistration(
    const std::string& registration_id) {
  std::string response(kRegistrationResponsePrefix);
  response.append(registration_id);
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(response);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void GCMClientImplTest::CompleteUnregistration(
    const std::string& app_id) {
  std::string response(kUnregistrationResponsePrefix);
  response.append(app_id);
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(response);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void GCMClientImplTest::VerifyPendingRequestFetcherDeleted() {
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  EXPECT_FALSE(fetcher);
}

bool GCMClientImplTest::ExistsRegistration(const std::string& app_id) const {
  return gcm_client_->registrations_.count(app_id) > 0;
}

void GCMClientImplTest::AddRegistration(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& registration_id) {
  linked_ptr<RegistrationInfo> registration(new RegistrationInfo);
  registration->sender_ids = sender_ids;
  registration->registration_id = registration_id;
  gcm_client_->registrations_[app_id] = registration;
}

void GCMClientImplTest::InitializeGCMClient() {
  clock()->Advance(base::TimeDelta::FromMilliseconds(1));

  // Actual initialization.
  GCMClient::ChromeBuildInfo chrome_build_info;
  gcm_client_->Initialize(chrome_build_info,
                          temp_directory_.path(),
                          message_loop_.message_loop_proxy(),
                          url_request_context_getter_,
                          make_scoped_ptr<Encryptor>(new FakeEncryptor),
                          this);
}

void GCMClientImplTest::StartGCMClient() {
  // Start loading and check-in.
  gcm_client_->Start(GCMClient::IMMEDIATE_START);

  PumpLoopUntilIdle();
}

void GCMClientImplTest::ReceiveMessageFromMCS(const MCSMessage& message) {
  gcm_client_->recorder_.RecordConnectionInitiated(std::string());
  gcm_client_->recorder_.RecordConnectionSuccess();
  gcm_client_->OnMessageReceivedFromMCS(message);
}

void GCMClientImplTest::ReceiveOnMessageSentToMCS(
      const std::string& app_id,
      const std::string& message_id,
      const MCSClient::MessageSendStatus status) {
  gcm_client_->OnMessageSentToMCS(0LL, app_id, message_id, status);
}

void GCMClientImplTest::OnGCMReady(
    const std::vector<AccountMapping>& account_mappings,
    const base::Time& last_token_fetch_time) {
  last_event_ = LOADING_COMPLETED;
  last_account_mappings_ = account_mappings;
  last_token_fetch_time_ = last_token_fetch_time;
  QuitLoop();
}

void GCMClientImplTest::OnMessageReceived(
    const std::string& registration_id,
    const GCMClient::IncomingMessage& message) {
  last_event_ = MESSAGE_RECEIVED;
  last_app_id_ = registration_id;
  last_message_ = message;
  QuitLoop();
}

void GCMClientImplTest::OnRegisterFinished(const std::string& app_id,
                                           const std::string& registration_id,
                                           GCMClient::Result result) {
  last_event_ = REGISTRATION_COMPLETED;
  last_app_id_ = app_id;
  last_registration_id_ = registration_id;
  last_result_ = result;
}

void GCMClientImplTest::OnUnregisterFinished(const std::string& app_id,
                                             GCMClient::Result result) {
  last_event_ = UNREGISTRATION_COMPLETED;
  last_app_id_ = app_id;
  last_result_ = result;
}

void GCMClientImplTest::OnMessagesDeleted(const std::string& app_id) {
  last_event_ = MESSAGES_DELETED;
  last_app_id_ = app_id;
}

void GCMClientImplTest::OnMessageSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  last_event_ = MESSAGE_SEND_ERROR;
  last_app_id_ = app_id;
  last_error_details_ = send_error_details;
}

void GCMClientImplTest::OnSendAcknowledged(const std::string& app_id,
                                           const std::string& message_id) {
  last_event_ = MESSAGE_SEND_ACK;
  last_app_id_ = app_id;
  last_message_id_ = message_id;
}

int64 GCMClientImplTest::CurrentTime() {
  return clock()->Now().ToInternalValue() / base::Time::kMicrosecondsPerSecond;
}

TEST_F(GCMClientImplTest, LoadingCompleted) {
  EXPECT_EQ(LOADING_COMPLETED, last_event());
  EXPECT_EQ(kDeviceAndroidId, mcs_client()->last_android_id());
  EXPECT_EQ(kDeviceSecurityToken, mcs_client()->last_security_token());

  // Checking freshly loaded CheckinInfo.
  EXPECT_EQ(kDeviceAndroidId, device_checkin_info().android_id);
  EXPECT_EQ(kDeviceSecurityToken, device_checkin_info().secret);
  EXPECT_TRUE(device_checkin_info().last_checkin_accounts.empty());
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_TRUE(device_checkin_info().account_tokens.empty());
}

TEST_F(GCMClientImplTest, LoadingBusted) {
  // Close the GCM store.
  gcm_client()->Stop();
  PumpLoopUntilIdle();

  // Mess up the store.
  base::FilePath store_file_path =
      temp_directory_path().Append(FILE_PATH_LITERAL("CURRENT"));
  ASSERT_TRUE(base::AppendToFile(store_file_path, "A", 1));

  // Restart GCM client. The store should be reset and the loading should
  // complete successfully.
  reset_last_event();
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  StartGCMClient();
  CompleteCheckin(kDeviceAndroidId2,
                  kDeviceSecurityToken2,
                  std::string(),
                  std::map<std::string, std::string>());

  EXPECT_EQ(LOADING_COMPLETED, last_event());
  EXPECT_EQ(kDeviceAndroidId2, mcs_client()->last_android_id());
  EXPECT_EQ(kDeviceSecurityToken2, mcs_client()->last_security_token());
}

TEST_F(GCMClientImplTest, RegisterApp) {
  EXPECT_FALSE(ExistsRegistration(kAppId));

  std::vector<std::string> senders;
  senders.push_back("sender");
  gcm_client()->Register(kAppId, senders);
  CompleteRegistration("reg_id");

  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kAppId, last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_TRUE(ExistsRegistration(kAppId));
}

TEST_F(GCMClientImplTest, DISABLED_RegisterAppFromCache) {
  EXPECT_FALSE(ExistsRegistration(kAppId));

  std::vector<std::string> senders;
  senders.push_back("sender");
  gcm_client()->Register(kAppId, senders);
  CompleteRegistration("reg_id");
  EXPECT_TRUE(ExistsRegistration(kAppId));

  EXPECT_EQ(kAppId, last_app_id());
  EXPECT_EQ("reg_id", last_registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_EQ(REGISTRATION_COMPLETED, last_event());

  // Recreate GCMClient in order to load from the persistent store.
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  StartGCMClient();

  EXPECT_TRUE(ExistsRegistration(kAppId));
}

TEST_F(GCMClientImplTest, UnregisterApp) {
  EXPECT_FALSE(ExistsRegistration(kAppId));

  std::vector<std::string> senders;
  senders.push_back("sender");
  gcm_client()->Register(kAppId, senders);
  CompleteRegistration("reg_id");
  EXPECT_TRUE(ExistsRegistration(kAppId));

  gcm_client()->Unregister(kAppId);
  CompleteUnregistration(kAppId);

  EXPECT_EQ(UNREGISTRATION_COMPLETED, last_event());
  EXPECT_EQ(kAppId, last_app_id());
  EXPECT_EQ(GCMClient::SUCCESS, last_result());
  EXPECT_FALSE(ExistsRegistration(kAppId));
}

// Tests that stopping the GCMClient also deletes pending registration requests.
// This is tested by checking that url fetcher contained in the request was
// deleted.
TEST_F(GCMClientImplTest, DeletePendingRequestsWhenStopping) {
  std::vector<std::string> senders;
  senders.push_back("sender");
  gcm_client()->Register(kAppId, senders);

  gcm_client()->Stop();
  VerifyPendingRequestFetcherDeleted();
}

TEST_F(GCMClientImplTest, DispatchDownstreamMessage) {
  // Register to receive messages from kSender and kSender2 only.
  std::vector<std::string> senders;
  senders.push_back(kSender);
  senders.push_back(kSender2);
  AddRegistration(kAppId, senders, "reg_id");

  std::map<std::string, std::string> expected_data;
  expected_data["message_type"] = "gcm";
  expected_data["key"] = "value";
  expected_data["key2"] = "value2";

  // Message for kSender will be received.
  MCSMessage message(BuildDownstreamMessage(kSender, kAppId, expected_data));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  expected_data.erase(expected_data.find("message_type"));
  EXPECT_EQ(MESSAGE_RECEIVED, last_event());
  EXPECT_EQ(kAppId, last_app_id());
  EXPECT_EQ(expected_data.size(), last_message().data.size());
  EXPECT_EQ(expected_data, last_message().data);
  EXPECT_EQ(kSender, last_message().sender_id);

  reset_last_event();

  // Message for kSender2 will be received.
  MCSMessage message2(BuildDownstreamMessage(kSender2, kAppId, expected_data));
  EXPECT_TRUE(message2.IsValid());
  ReceiveMessageFromMCS(message2);

  EXPECT_EQ(MESSAGE_RECEIVED, last_event());
  EXPECT_EQ(kAppId, last_app_id());
  EXPECT_EQ(expected_data.size(), last_message().data.size());
  EXPECT_EQ(expected_data, last_message().data);
  EXPECT_EQ(kSender2, last_message().sender_id);

  reset_last_event();

  // Message from kSender3 will be dropped.
  MCSMessage message3(BuildDownstreamMessage(kSender3, kAppId, expected_data));
  EXPECT_TRUE(message3.IsValid());
  ReceiveMessageFromMCS(message3);

  EXPECT_NE(MESSAGE_RECEIVED, last_event());
  EXPECT_NE(kAppId, last_app_id());
}

TEST_F(GCMClientImplTest, DispatchDownstreamMessageSendError) {
  std::map<std::string, std::string> expected_data;
  expected_data["message_type"] = "send_error";
  expected_data["google.message_id"] = "007";
  expected_data["error_details"] = "some details";
  MCSMessage message(BuildDownstreamMessage(
      kSender, kAppId, expected_data));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGE_SEND_ERROR, last_event());
  EXPECT_EQ(kAppId, last_app_id());
  EXPECT_EQ("007", last_error_details().message_id);
  EXPECT_EQ(1UL, last_error_details().additional_data.size());
  GCMClient::MessageData::const_iterator iter =
      last_error_details().additional_data.find("error_details");
  EXPECT_TRUE(iter != last_error_details().additional_data.end());
  EXPECT_EQ("some details", iter->second);
}

TEST_F(GCMClientImplTest, DispatchDownstreamMessgaesDeleted) {
  std::map<std::string, std::string> expected_data;
  expected_data["message_type"] = "deleted_messages";
  MCSMessage message(BuildDownstreamMessage(
      kSender, kAppId, expected_data));
  EXPECT_TRUE(message.IsValid());
  ReceiveMessageFromMCS(message);

  EXPECT_EQ(MESSAGES_DELETED, last_event());
  EXPECT_EQ(kAppId, last_app_id());
}

TEST_F(GCMClientImplTest, SendMessage) {
  GCMClient::OutgoingMessage message;
  message.id = "007";
  message.time_to_live = 500;
  message.data["key"] = "value";
  gcm_client()->Send(kAppId, kSender, message);

  EXPECT_EQ(kDataMessageStanzaTag, mcs_client()->last_message_tag());
  EXPECT_EQ(kAppId, mcs_client()->last_data_message_stanza().category());
  EXPECT_EQ(kSender, mcs_client()->last_data_message_stanza().to());
  EXPECT_EQ(500, mcs_client()->last_data_message_stanza().ttl());
  EXPECT_EQ(CurrentTime(), mcs_client()->last_data_message_stanza().sent());
  EXPECT_EQ("007", mcs_client()->last_data_message_stanza().id());
  EXPECT_EQ("gcm@chrome.com", mcs_client()->last_data_message_stanza().from());
  EXPECT_EQ(kSender, mcs_client()->last_data_message_stanza().to());
  EXPECT_EQ("key", mcs_client()->last_data_message_stanza().app_data(0).key());
  EXPECT_EQ("value",
            mcs_client()->last_data_message_stanza().app_data(0).value());
}

TEST_F(GCMClientImplTest, SendMessageAcknowledged) {
  ReceiveOnMessageSentToMCS(kAppId, "007", MCSClient::SENT);
  EXPECT_EQ(MESSAGE_SEND_ACK, last_event());
  EXPECT_EQ(kAppId, last_app_id());
  EXPECT_EQ("007", last_message_id());
}

class GCMClientImplCheckinTest : public GCMClientImplTest {
 public:
  GCMClientImplCheckinTest();
  ~GCMClientImplCheckinTest() override;

  void SetUp() override;
};

GCMClientImplCheckinTest::GCMClientImplCheckinTest() {
}

GCMClientImplCheckinTest::~GCMClientImplCheckinTest() {
}

void GCMClientImplCheckinTest::SetUp() {
  testing::Test::SetUp();
  // Creating unique temp directory that will be used by GCMStore shared between
  // GCM Client and G-services settings.
  ASSERT_TRUE(CreateUniqueTempDir());
  InitializeLoop();
  // Time will be advancing one hour every time it is checked.
  BuildGCMClient(base::TimeDelta::FromSeconds(kSettingsCheckinInterval));
  InitializeGCMClient();
  StartGCMClient();
}

TEST_F(GCMClientImplCheckinTest, GServicesSettingsAfterInitialCheckin) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::Int64ToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);
  EXPECT_EQ(base::TimeDelta::FromSeconds(kSettingsCheckinInterval),
            gservices_settings().GetCheckinInterval());
  EXPECT_EQ(GURL("http://alternative.url/checkin"),
            gservices_settings().GetCheckinURL());
  EXPECT_EQ(GURL("http://alternative.url/registration"),
            gservices_settings().GetRegistrationURL());
  EXPECT_EQ(GURL("https://alternative.gcm.host:7777"),
            gservices_settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://alternative.gcm.host:443"),
            gservices_settings().GetMCSFallbackEndpoint());
}

// This test only checks that periodic checkin happens.
TEST_F(GCMClientImplCheckinTest, PeriodicCheckin) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::IntToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  EXPECT_EQ(2, clock()->call_count());

  PumpLoopUntilIdle();
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);
}

TEST_F(GCMClientImplCheckinTest, LoadGSettingsFromStore) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::IntToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
  StartGCMClient();

  EXPECT_EQ(base::TimeDelta::FromSeconds(kSettingsCheckinInterval),
            gservices_settings().GetCheckinInterval());
  EXPECT_EQ(GURL("http://alternative.url/checkin"),
            gservices_settings().GetCheckinURL());
  EXPECT_EQ(GURL("http://alternative.url/registration"),
            gservices_settings().GetRegistrationURL());
  EXPECT_EQ(GURL("https://alternative.gcm.host:7777"),
            gservices_settings().GetMCSMainEndpoint());
  EXPECT_EQ(GURL("https://alternative.gcm.host:443"),
            gservices_settings().GetMCSFallbackEndpoint());
}

// This test only checks that periodic checkin happens.
TEST_F(GCMClientImplCheckinTest, CheckinWithAccounts) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::IntToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  account_tokens.push_back(MakeAccountToken("test_user1@gmail.com", "token1"));
  account_tokens.push_back(MakeAccountToken("test_user2@gmail.com", "token2"));
  gcm_client()->SetAccountTokens(account_tokens);

  EXPECT_TRUE(device_checkin_info().last_checkin_accounts.empty());
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);

  PumpLoopUntilIdle();
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  std::set<std::string> accounts;
  accounts.insert("test_user1@gmail.com");
  accounts.insert("test_user2@gmail.com");
  EXPECT_EQ(accounts, device_checkin_info().last_checkin_accounts);
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);
}

// This test only checks that periodic checkin happens.
TEST_F(GCMClientImplCheckinTest, CheckinWhenAccountRemoved) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::IntToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  account_tokens.push_back(MakeAccountToken("test_user1@gmail.com", "token1"));
  account_tokens.push_back(MakeAccountToken("test_user2@gmail.com", "token2"));
  gcm_client()->SetAccountTokens(account_tokens);
  PumpLoopUntilIdle();
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  EXPECT_EQ(2UL, device_checkin_info().last_checkin_accounts.size());
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);

  account_tokens.erase(account_tokens.begin() + 1);
  gcm_client()->SetAccountTokens(account_tokens);

  PumpLoopUntilIdle();
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  std::set<std::string> accounts;
  accounts.insert("test_user1@gmail.com");
  EXPECT_EQ(accounts, device_checkin_info().last_checkin_accounts);
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);
}

// This test only checks that periodic checkin happens.
TEST_F(GCMClientImplCheckinTest, CheckinWhenAccountReplaced) {
  std::map<std::string, std::string> settings;
  settings["checkin_interval"] = base::IntToString(kSettingsCheckinInterval);
  settings["checkin_url"] = "http://alternative.url/checkin";
  settings["gcm_hostname"] = "alternative.gcm.host";
  settings["gcm_secure_port"] = "7777";
  settings["gcm_registration_url"] = "http://alternative.url/registration";
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  account_tokens.push_back(MakeAccountToken("test_user1@gmail.com", "token1"));
  gcm_client()->SetAccountTokens(account_tokens);

  PumpLoopUntilIdle();
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  std::set<std::string> accounts;
  accounts.insert("test_user1@gmail.com");
  EXPECT_EQ(accounts, device_checkin_info().last_checkin_accounts);

  // This should trigger another checkin, because the list of accounts is
  // different.
  account_tokens.clear();
  account_tokens.push_back(MakeAccountToken("test_user2@gmail.com", "token2"));
  gcm_client()->SetAccountTokens(account_tokens);

  PumpLoopUntilIdle();
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  GServicesSettings::CalculateDigest(settings),
                  settings);

  accounts.clear();
  accounts.insert("test_user2@gmail.com");
  EXPECT_EQ(accounts, device_checkin_info().last_checkin_accounts);
  EXPECT_TRUE(device_checkin_info().accounts_set);
  EXPECT_EQ(MakeEmailToTokenMap(account_tokens),
            device_checkin_info().account_tokens);
}

class GCMClientImplStartAndStopTest : public GCMClientImplTest {
public:
  GCMClientImplStartAndStopTest();
  ~GCMClientImplStartAndStopTest() override;

  void SetUp() override;

  void DefaultCompleteCheckin();
};

GCMClientImplStartAndStopTest::GCMClientImplStartAndStopTest() {
}

GCMClientImplStartAndStopTest::~GCMClientImplStartAndStopTest() {
}

void GCMClientImplStartAndStopTest::SetUp() {
  testing::Test::SetUp();
  ASSERT_TRUE(CreateUniqueTempDir());
  InitializeLoop();
  BuildGCMClient(base::TimeDelta());
  InitializeGCMClient();
}

void GCMClientImplStartAndStopTest::DefaultCompleteCheckin() {
  SetUpUrlFetcherFactory();
  CompleteCheckin(kDeviceAndroidId,
                  kDeviceSecurityToken,
                  std::string(),
                  std::map<std::string, std::string>());
  PumpLoopUntilIdle();
}

TEST_F(GCMClientImplStartAndStopTest, StartStopAndRestart) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM.
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::LOADED, gcm_client_state());

  // Stop the GCM.
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Restart the GCM without delay.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIAL_DEVICE_CHECKIN, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, StartAndStopImmediately) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM and then stop it immediately.
  gcm_client()->Start(GCMClient::DELAYED_START);
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Start the GCM and then stop it immediately.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, StartStopAndRestartImmediately) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM and then stop and restart it immediately.
  gcm_client()->Start(GCMClient::DELAYED_START);
  gcm_client()->Stop();
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::LOADED, gcm_client_state());

  // Start the GCM and then stop and restart it immediately.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  gcm_client()->Stop();
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIAL_DEVICE_CHECKIN, gcm_client_state());
}

TEST_F(GCMClientImplStartAndStopTest, DelayStart) {
  // GCMClientImpl should be in INITIALIZED state at first.
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM.
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::LOADED, gcm_client_state());

  // Start the GCM immediately and complete the checkin.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIAL_DEVICE_CHECKIN, gcm_client_state());
  DefaultCompleteCheckin();
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());

  // Registration.
  std::vector<std::string> senders;
  senders.push_back("sender");
  gcm_client()->Register(kAppId, senders);
  CompleteRegistration("reg_id");
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());

  // Stop the GCM.
  gcm_client()->Stop();
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::INITIALIZED, gcm_client_state());

  // Delay start the GCM. GCM is indeed started without delay because the
  // registration record has been found.
  gcm_client()->Start(GCMClient::DELAYED_START);
  PumpLoopUntilIdle();
  EXPECT_EQ(GCMClientImpl::READY, gcm_client_state());
}

// Test for known account mappings and last token fetching time being passed
// to OnGCMReady.
TEST_F(GCMClientImplStartAndStopTest, OnGCMReadyAccountsAndTokenFetchingTime) {
  // Start the GCM and wait until it is ready.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();
  DefaultCompleteCheckin();

  base::Time expected_time = base::Time::Now();
  gcm_client()->SetLastTokenFetchTime(expected_time);
  AccountMapping expected_mapping;
  expected_mapping.account_id = "accId";
  expected_mapping.email = "email@gmail.com";
  expected_mapping.status = AccountMapping::MAPPED;
  expected_mapping.status_change_timestamp = expected_time;
  gcm_client()->UpdateAccountMapping(expected_mapping);
  PumpLoopUntilIdle();

  // Stop the GCM.
  gcm_client()->Stop();
  PumpLoopUntilIdle();

  // Restart the GCM.
  gcm_client()->Start(GCMClient::IMMEDIATE_START);
  PumpLoopUntilIdle();

  EXPECT_EQ(LOADING_COMPLETED, last_event());
  EXPECT_EQ(expected_time, last_token_fetch_time());
  ASSERT_EQ(1UL, last_account_mappings().size());
  const AccountMapping& actual_mapping = last_account_mappings()[0];
  EXPECT_EQ(expected_mapping.account_id, actual_mapping.account_id);
  EXPECT_EQ(expected_mapping.email, actual_mapping.email);
  EXPECT_EQ(expected_mapping.status, actual_mapping.status);
  EXPECT_EQ(expected_mapping.status_change_timestamp,
            actual_mapping.status_change_timestamp);
}

}  // namespace gcm
