// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/unlock_manager.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/proximity_auth/client.h"
#include "components/proximity_auth/controller.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/proximity_auth_client.h"
#include "components/proximity_auth/proximity_monitor.h"
#include "components/proximity_auth/remote_status_update.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#endif  // defined(OS_CHROMEOS)

using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::_;

namespace proximity_auth {
namespace {

// Note that the trust agent state is currently ignored by the UnlockManager
// implementation.
RemoteStatusUpdate kRemoteScreenUnlocked = {
    USER_PRESENT, SECURE_SCREEN_LOCK_ENABLED, TRUST_AGENT_UNSUPPORTED};
RemoteStatusUpdate kRemoteScreenLocked = {
    USER_ABSENT, SECURE_SCREEN_LOCK_ENABLED, TRUST_AGENT_UNSUPPORTED};
RemoteStatusUpdate kRemoteScreenlockDisabled = {
    USER_PRESENT, SECURE_SCREEN_LOCK_DISABLED, TRUST_AGENT_UNSUPPORTED};
RemoteStatusUpdate kRemoteScreenlockStateUnknown = {
    USER_PRESENCE_UNKNOWN, SECURE_SCREEN_LOCK_STATE_UNKNOWN,
    TRUST_AGENT_UNSUPPORTED};

class MockController : public Controller {
 public:
  MockController() {}
  ~MockController() override {}

  MOCK_CONST_METHOD0(GetState, State());
  MOCK_METHOD0(GetClient, Client*());
};

class MockClient : public Client {
 public:
  MockClient() {}
  ~MockClient() override {}

  MOCK_METHOD1(AddObserver, void(ClientObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(ClientObserver* observer));
  MOCK_CONST_METHOD0(SupportsSignIn, bool());
  MOCK_METHOD0(DispatchUnlockEvent, void());
  MOCK_METHOD1(RequestDecryption, void(const std::string& challenge));
  MOCK_METHOD0(RequestUnlock, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClient);
};

class MockProximityMonitor : public ProximityMonitor {
 public:
  MockProximityMonitor() {
    ON_CALL(*this, GetStrategy())
        .WillByDefault(Return(ProximityMonitor::Strategy::NONE));
    ON_CALL(*this, IsUnlockAllowed()).WillByDefault(Return(true));
    ON_CALL(*this, IsInRssiRange()).WillByDefault(Return(false));
  }
  ~MockProximityMonitor() override {}

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(GetStrategy, Strategy());
  MOCK_CONST_METHOD0(IsUnlockAllowed, bool());
  MOCK_CONST_METHOD0(IsInRssiRange, bool());
  MOCK_METHOD0(RecordProximityMetricsOnAuthSuccess, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProximityMonitor);
};

class MockProximityAuthClient : public ProximityAuthClient {
 public:
  MockProximityAuthClient() {}
  ~MockProximityAuthClient() override {}

  MOCK_CONST_METHOD0(GetAuthenticatedUsername, std::string());
  MOCK_METHOD1(UpdateScreenlockState,
               void(proximity_auth::ScreenlockState state));
  MOCK_METHOD1(FinalizeUnlock, void(bool success));
  MOCK_METHOD1(FinalizeSignin, void(const std::string& secret));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProximityAuthClient);
};

class FakeLockHandler : public ScreenlockBridge::LockHandler {
 public:
  FakeLockHandler() {}
  ~FakeLockHandler() override {}

  // LockHandler:
  void ShowBannerMessage(const base::string16& message) override {}
  void ShowUserPodCustomIcon(
      const std::string& user_email,
      const ScreenlockBridge::UserPodCustomIconOptions& icon) override {}
  void HideUserPodCustomIcon(const std::string& user_email) override {}
  void EnableInput() override {}
  void SetAuthType(const std::string& user_email,
                   AuthType auth_type,
                   const base::string16& auth_value) override {}
  AuthType GetAuthType(const std::string& user_email) const override {
    return USER_CLICK;
  }
  ScreenType GetScreenType() const override { return LOCK_SCREEN; }
  void Unlock(const std::string& user_email) override {}
  void AttemptEasySignin(const std::string& user_email,
                         const std::string& secret,
                         const std::string& key_label) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeLockHandler);
};

class TestUnlockManager : public UnlockManager {
 public:
  TestUnlockManager(ScreenlockType screenlock_type,
                    scoped_ptr<ProximityMonitor> proximity_monitor,
                    ProximityAuthClient* proximity_auth_client)
      : UnlockManager(screenlock_type,
                      proximity_monitor.Pass(),
                      proximity_auth_client) {}
  ~TestUnlockManager() override {}

  using UnlockManager::OnAuthAttempted;
  using ClientObserver::OnUnlockEventSent;
  using ClientObserver::OnRemoteStatusUpdate;
  using ClientObserver::OnDecryptResponse;
  using ClientObserver::OnUnlockResponse;
  using ClientObserver::OnDisconnected;
  using ScreenlockBridge::Observer::OnScreenDidLock;
  using ScreenlockBridge::Observer::OnScreenDidUnlock;
  using ScreenlockBridge::Observer::OnFocusedUserChanged;
};

// Creates a mock Bluetooth adapter and sets it as the global adapter for
// testing.
scoped_refptr<device::MockBluetoothAdapter>
CreateAndRegisterMockBluetoothAdapter() {
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new NiceMock<device::MockBluetoothAdapter>();
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);
  return adapter;
}

}  // namespace

class ProximityAuthUnlockManagerTest : public testing::Test {
 public:
  ProximityAuthUnlockManagerTest()
      : bluetooth_adapter_(CreateAndRegisterMockBluetoothAdapter()),
        proximity_monitor_(nullptr),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {
    ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));
    ON_CALL(controller_, GetClient()).WillByDefault(Return(&client_));
    ON_CALL(client_, SupportsSignIn()).WillByDefault(Return(true));

    ScreenlockBridge::Get()->SetLockHandler(&lock_handler_);

#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Initialize();
#endif
  }

  ~ProximityAuthUnlockManagerTest() override {
    // Make sure to verify the mock prior to the destruction of the unlock
    // manager, as otherwise it's impossible to tell whether calls to Stop()
    // occur as a side-effect of the destruction or from the code intended to be
    // under test.
    if (proximity_monitor_)
      testing::Mock::VerifyAndClearExpectations(proximity_monitor_);

    // The UnlockManager must be destroyed before calling
    // chromeos::DBusThreadManager::Shutdown(), as the UnlockManager's
    // destructor references the DBusThreadManager.
    unlock_manager_.reset();

#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Shutdown();
#endif

    ScreenlockBridge::Get()->SetLockHandler(nullptr);
  }

  void CreateUnlockManager(UnlockManager::ScreenlockType screenlock_type) {
    proximity_monitor_ = new NiceMock<MockProximityMonitor>;
    unlock_manager_.reset(new TestUnlockManager(
        screenlock_type, make_scoped_ptr(proximity_monitor_),
        &proximity_auth_client_));
  }

  void SimulateUserPresentState() {
    ON_CALL(controller_, GetState())
        .WillByDefault(Return(Controller::State::STOPPED));
    unlock_manager_->SetController(&controller_);

    ON_CALL(controller_, GetState())
        .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
    unlock_manager_->OnControllerStateChanged();

    unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);
  }

  void RunPendingTasks() { task_runner_->RunPendingTasks(); }

 protected:
  // Mock used for verifying interactions with the Bluetooth subsystem.
  scoped_refptr<device::MockBluetoothAdapter> bluetooth_adapter_;

  NiceMock<MockProximityAuthClient> proximity_auth_client_;
  NiceMock<MockController> controller_;
  NiceMock<MockClient> client_;
  scoped_ptr<TestUnlockManager> unlock_manager_;
  // Owned by the |unlock_manager_|.
  MockProximityMonitor* proximity_monitor_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  FakeLockHandler lock_handler_;
  ScopedDisableLoggingForTesting disable_logging_;
};

TEST_F(ProximityAuthUnlockManagerTest, IsUnlockAllowed_InitialState) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest, IsUnlockAllowed_SessionLock_AllGood) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetController(&controller_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_TRUE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest, IsUnlockAllowed_SignIn_AllGood) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SIGN_IN);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::STOPPED));
  unlock_manager_->SetController(&controller_);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->OnControllerStateChanged();

  ON_CALL(client_, SupportsSignIn()).WillByDefault(Return(true));
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_TRUE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_SignIn_ClientDoesNotSupportSignIn) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SIGN_IN);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::STOPPED));
  unlock_manager_->SetController(&controller_);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->OnControllerStateChanged();

  ON_CALL(client_, SupportsSignIn()).WillByDefault(Return(false));
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest, IsUnlockAllowed_SignIn_ClientIsNull) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SIGN_IN);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  ON_CALL(controller_, GetClient()).WillByDefault(Return(nullptr));
  unlock_manager_->SetController(&controller_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_DisallowedByProximityMonitor) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetController(&controller_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  ON_CALL(*proximity_monitor_, IsUnlockAllowed()).WillByDefault(Return(false));
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_SecureChannelNotEstablished) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::AUTHENTICATING));
  unlock_manager_->SetController(&controller_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest, IsUnlockAllowed_ControllerIsNull) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  unlock_manager_->SetController(nullptr);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_RemoteScreenlockStateLocked) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetController(&controller_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenLocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_RemoteScreenlockStateUnknown) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetController(&controller_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockStateUnknown);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_RemoteScreenlockStateDisabled) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetController(&controller_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockDisabled);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_RemoteScreenlockStateNotYetReceived) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetController(&controller_);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest, SetController_SetToNull) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::INACTIVE));
  unlock_manager_->SetController(nullptr);
}

TEST_F(ProximityAuthUnlockManagerTest, SetController_ExistingController) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, UpdateScreenlockState(_)).Times(0);
  unlock_manager_->SetController(&controller_);
}

TEST_F(ProximityAuthUnlockManagerTest,
       SetController_NullThenExistingController) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::INACTIVE));
  unlock_manager_->SetController(nullptr);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::AUTHENTICATED));
  unlock_manager_->SetController(&controller_);
}

TEST_F(ProximityAuthUnlockManagerTest, SetController_AuthenticationFailed) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetController(nullptr);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::AUTHENTICATION_FAILED));
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::PHONE_NOT_AUTHENTICATED));
  unlock_manager_->SetController(&controller_);
}

TEST_F(ProximityAuthUnlockManagerTest, SetController_WakingUp) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetController(nullptr);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::FINDING_CONNECTION));
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetController(&controller_);
}

TEST_F(ProximityAuthUnlockManagerTest,
       SetController_NullController_StopsProximityMonitor) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(*proximity_monitor_, Stop()).Times(AtLeast(1));
  unlock_manager_->SetController(nullptr);
}

TEST_F(ProximityAuthUnlockManagerTest,
       SetController_ConnectingController_StopsProximityMonitor) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  NiceMock<MockController> controller;
  ON_CALL(controller, GetState())
      .WillByDefault(Return(Controller::State::FINDING_CONNECTION));

  EXPECT_CALL(*proximity_monitor_, Stop()).Times(AtLeast(1));
  unlock_manager_->SetController(&controller);
}

TEST_F(ProximityAuthUnlockManagerTest,
       SetController_ConnectedController_StartsProximityMonitor) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  NiceMock<MockController> controller;
  ON_CALL(controller, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));

  EXPECT_CALL(*proximity_monitor_, Start()).Times(AtLeast(1));
  unlock_manager_->SetController(&controller);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnControllerStateChanged_SecureChannelEstablished_RegistersAsObserver) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  NiceMock<MockController> controller;
  ON_CALL(controller, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));

  EXPECT_CALL(client_, AddObserver(unlock_manager_.get()));
  unlock_manager_->OnControllerStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnControllerStateChanged_StartsProximityMonitor) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  NiceMock<MockController> controller;
  ON_CALL(controller, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));

  EXPECT_CALL(*proximity_monitor_, Start()).Times(AtLeast(1));
  unlock_manager_->OnControllerStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnControllerStateChanged_StopsProximityMonitor) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::AUTHENTICATION_FAILED));

  EXPECT_CALL(*proximity_monitor_, Stop()).Times(AtLeast(1));
  unlock_manager_->OnControllerStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnControllerStateChanged_Stopped_UpdatesScreenlockState) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::STOPPED));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::INACTIVE));
  unlock_manager_->OnControllerStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnControllerStateChanged_AuthenticationFailed_UpdatesScreenlockState) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::AUTHENTICATION_FAILED));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::PHONE_NOT_AUTHENTICATED));
  unlock_manager_->OnControllerStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnControllerStateChanged_FindingConnection_UpdatesScreenlockState) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::STOPPED));
  unlock_manager_->SetController(&controller_);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::FINDING_CONNECTION));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->OnControllerStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnControllerStateChanged_Authenticating_UpdatesScreenlockState) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::STOPPED));
  unlock_manager_->SetController(&controller_);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::AUTHENTICATING));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->OnControllerStateChanged();
}

TEST_F(
    ProximityAuthUnlockManagerTest,
    OnControllerStateChanged_SecureChannelEstablished_UpdatesScreenlockState) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::STOPPED));
  unlock_manager_->SetController(&controller_);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->OnControllerStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest, OnDisconnected_UnregistersAsObserver) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::AUTHENTICATION_FAILED));

  EXPECT_CALL(client_, RemoveObserver(unlock_manager_.get()));
  unlock_manager_.get()->OnDisconnected();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnScreenDidUnlock_StopsProximityMonitor) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(*proximity_monitor_, Stop());
  unlock_manager_.get()->OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);
}

TEST_F(ProximityAuthUnlockManagerTest, OnScreenDidLock_StartsProximityMonitor) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::STOPPED));
  unlock_manager_->SetController(&controller_);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->OnControllerStateChanged();

  EXPECT_CALL(*proximity_monitor_, Start());
  unlock_manager_.get()->OnScreenDidLock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);
}

TEST_F(ProximityAuthUnlockManagerTest, OnScreenDidLock_SetsWakingUpState) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_.get()->OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);

  ON_CALL(controller_, GetState())
      .WillByDefault(Return(Controller::State::FINDING_CONNECTION));
  unlock_manager_->OnControllerStateChanged();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_.get()->OnScreenDidLock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnDecryptResponse_NoAuthAttemptInProgress) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnDecryptResponse(nullptr);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnUnlockEventSent_NoAuthAttemptInProgress) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnUnlockResponse_NoAuthAttemptInProgress) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockResponse(true);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_NoController) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetController(nullptr);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_UnlockNotAllowed) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(*proximity_monitor_, IsUnlockAllowed()).WillByDefault(Return(false));

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_NotUserClick) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_->OnAuthAttempted(
      ScreenlockBridge::LockHandler::EXPAND_THEN_USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_DuplicateCall) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(client_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(client_, RequestUnlock()).Times(0);
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_TimesOut) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_DoesntTimeOutFollowingResponse) {
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_));
  unlock_manager_->OnUnlockResponse(false);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_SupportsSignIn_UnlockRequestFails) {
  ON_CALL(client_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(client_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockResponse(false);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_WithSignIn_RequestSucceeds_EventSendFails) {
  ON_CALL(client_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(client_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(client_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_WithSignIn_RequestSucceeds_EventSendSucceeds) {
  ON_CALL(client_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(client_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(client_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(true));
  unlock_manager_->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_DoesntSupportSignIn_UnlockEventSendFails) {
  ON_CALL(client_, SupportsSignIn()).WillByDefault(Return(false));
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(client_, DispatchUnlockEvent());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_SupportsSignIn_UnlockEventSendSucceeds) {
  ON_CALL(client_, SupportsSignIn()).WillByDefault(Return(false));
  CreateUnlockManager(UnlockManager::ScreenlockType::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(client_, DispatchUnlockEvent());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(true));
  unlock_manager_->OnUnlockEventSent(true);
}

}  // namespace proximity_auth
