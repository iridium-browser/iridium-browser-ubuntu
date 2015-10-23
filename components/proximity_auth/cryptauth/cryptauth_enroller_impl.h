// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_ENROLLER_IMPL_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_ENROLLER_IMPL_H

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/cryptauth/cryptauth_enroller.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"

namespace proximity_auth {

class CryptAuthClient;
class CryptAuthClientFactory;
class CryptAuthClientFactoryImpl;
class SecureMessageDelegate;

// Implementation of CryptAuthEnroller to perform enrollment in two steps:
// 1. SetupEnrollment:
//     Obtain a session public key from CryptAuth used to encrypt enrollment
//     data. Generate an ephemeral public key and derive a session symmetric
//     key.
// 2. FinishEnrollment:
//     Encrypt the enrollment data with the session symmetric key, and send the
//     payload and device's public key to CryptAuth.
class CryptAuthEnrollerImpl : public CryptAuthEnroller {
 public:
  // |client_factory| creates CryptAuthClient instances for making API calls.
  // |crypto_delegate| is responsible for SecureMessage operations.
  CryptAuthEnrollerImpl(
      scoped_ptr<CryptAuthClientFactory> client_factory,
      scoped_ptr<SecureMessageDelegate> secure_message_delegate);
  ~CryptAuthEnrollerImpl() override;

  // CryptAuthEnroller:
  void Enroll(const std::string& user_public_key,
              const std::string& user_private_key,
              const cryptauth::GcmDeviceInfo& device_info,
              cryptauth::InvocationReason invocation_reason,
              const EnrollmentFinishedCallback& callback) override;

 private:
  // Callbacks for SetupEnrollment.
  void OnSetupEnrollmentSuccess(
      const cryptauth::SetupEnrollmentResponse& response);
  void OnSetupEnrollmentFailure(const std::string& error);

  // Callbacks for FinishEnrollment.
  void OnFinishEnrollmentSuccess(
      const cryptauth::FinishEnrollmentResponse& response);
  void OnFinishEnrollmentFailure(const std::string& error);

  // Callbacks for SecureMessageDelegate operations.
  void OnKeyPairGenerated(const std::string& public_key,
                          const std::string& private_key);
  void OnKeyDerived(const std::string& symmetric_key);
  void OnInnerSecureMessageCreated(const std::string& inner_message);
  void OnOuterSecureMessageCreated(const std::string& outer_message);

  // Creates the CryptAuthClient instances to make API requests.
  scoped_ptr<CryptAuthClientFactory> client_factory_;

  // Handles SecureMessage operations.
  scoped_ptr<SecureMessageDelegate> secure_message_delegate_;

  // The CryptAuthClient for the latest request.
  scoped_ptr<CryptAuthClient> cryptauth_client_;

  // The ephemeral key-pair generated for a single enrollment.
  std::string session_public_key_;
  std::string session_private_key_;

  // The user's persistent key-pair identifying the local device.
  std::string user_public_key_;
  std::string user_private_key_;

  // Contains information of the device to enroll.
  cryptauth::GcmDeviceInfo device_info_;

  // The reason telling the server why the enrollment happened.
  cryptauth::InvocationReason invocation_reason_;

  // The setup information returned from the SetupEnrollment API call.
  cryptauth::SetupEnrollmentInfo setup_info_;

  // Callback invoked when the enrollment is done.
  EnrollmentFinishedCallback callback_;

  // The derived ephemeral symmetric key.
  std::string symmetric_key_;

  base::WeakPtrFactory<CryptAuthEnrollerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthEnrollerImpl);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_ENROLLER_IMPL_H
