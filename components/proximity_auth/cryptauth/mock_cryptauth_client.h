// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_MOCK_CRYPTAUTH_CLIENT_H
#define COMPONENTS_PROXIMITY_AUTH_MOCK_CRYPTAUTH_CLIENT_H

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace proximity_auth {

class MockCryptAuthClient : public CryptAuthClient {
 public:
  MockCryptAuthClient();
  ~MockCryptAuthClient() override;

  // CryptAuthClient:
  MOCK_METHOD3(GetMyDevices,
               void(const cryptauth::GetMyDevicesRequest& request,
                    const GetMyDevicesCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(FindEligibleUnlockDevices,
               void(const cryptauth::FindEligibleUnlockDevicesRequest& request,
                    const FindEligibleUnlockDevicesCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(SendDeviceSyncTickle,
               void(const cryptauth::SendDeviceSyncTickleRequest& request,
                    const SendDeviceSyncTickleCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(ToggleEasyUnlock,
               void(const cryptauth::ToggleEasyUnlockRequest& request,
                    const ToggleEasyUnlockCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(SetupEnrollment,
               void(const cryptauth::SetupEnrollmentRequest& request,
                    const SetupEnrollmentCallback& callback,
                    const ErrorCallback& error_callback));
  MOCK_METHOD3(FinishEnrollment,
               void(const cryptauth::FinishEnrollmentRequest& request,
                    const FinishEnrollmentCallback& callback,
                    const ErrorCallback& error_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCryptAuthClient);
};

class MockCryptAuthClientFactory : public CryptAuthClientFactory {
 public:
  class Observer {
   public:
    // Called with the new instance when it is requested from the factory,
    // allowing expectations to be set. Ownership of |client| will be taken by
    // the caller of CreateInstance().
    virtual void OnCryptAuthClientCreated(MockCryptAuthClient* client) = 0;
  };

  // If |is_strict| is true, then StrictMocks will be created. Otherwise,
  // NiceMocks will be created.
  explicit MockCryptAuthClientFactory(bool is_strict);
  ~MockCryptAuthClientFactory() override;

  // CryptAuthClientFactory:
  scoped_ptr<CryptAuthClient> CreateInstance() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Whether to create StrictMocks or NiceMocks.
  bool is_strict_;

  // Observers of the factory.
  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(MockCryptAuthClientFactory);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_MOCK_CRYPTAUTH_CLIENT_H
