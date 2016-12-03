// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_CLIENT_IMPL_H
#define COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_CLIENT_IMPL_H

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/cryptauth/cryptauth_access_token_fetcher.h"
#include "components/proximity_auth/cryptauth/cryptauth_api_call_flow.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"
#include "net/url_request/url_request_context_getter.h"

class OAuth2TokenService;

namespace proximity_auth {

// Implementation of CryptAuthClient.
// Note: There is no need to set the |device_classifier| field in request
// messages. CryptAuthClient will fill this field for all requests.
class CryptAuthClientImpl : public CryptAuthClient {
 public:
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  // Creates the client using |url_request_context| to make the HTTP request
  // through |api_call_flow|. CryptAuthClientImpl takes ownership of
  // |access_token_fetcher|, which provides the access token authorizing
  // CryptAuth requests. The |device_classifier| argument contains basic device
  // information of the caller (e.g. version and device type).
  CryptAuthClientImpl(
      std::unique_ptr<CryptAuthApiCallFlow> api_call_flow,
      std::unique_ptr<CryptAuthAccessTokenFetcher> access_token_fetcher,
      scoped_refptr<net::URLRequestContextGetter> url_request_context,
      const cryptauth::DeviceClassifier& device_classifier);
  ~CryptAuthClientImpl() override;

  // CryptAuthClient:
  void GetMyDevices(const cryptauth::GetMyDevicesRequest& request,
                    const GetMyDevicesCallback& callback,
                    const ErrorCallback& error_callback) override;
  void FindEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesRequest& request,
      const FindEligibleUnlockDevicesCallback& callback,
      const ErrorCallback& error_callback) override;
  void SendDeviceSyncTickle(
      const cryptauth::SendDeviceSyncTickleRequest& request,
      const SendDeviceSyncTickleCallback& callback,
      const ErrorCallback& error_callback) override;
  void ToggleEasyUnlock(const cryptauth::ToggleEasyUnlockRequest& request,
                        const ToggleEasyUnlockCallback& callback,
                        const ErrorCallback& error_callback) override;
  void SetupEnrollment(const cryptauth::SetupEnrollmentRequest& request,
                       const SetupEnrollmentCallback& callback,
                       const ErrorCallback& error_callback) override;
  void FinishEnrollment(const cryptauth::FinishEnrollmentRequest& request,
                        const FinishEnrollmentCallback& callback,
                        const ErrorCallback& error_callback) override;
  std::string GetAccessTokenUsed() override;

 private:
  // Starts a call to the API given by |request_path|, with the templated
  // request and response types. The client first fetches the access token and
  // then makes the HTTP request.
  template <class RequestProto, class ResponseProto>
  void MakeApiCall(
      const std::string& request_path,
      const RequestProto& request_proto,
      const base::Callback<void(const ResponseProto&)>& response_callback,
      const ErrorCallback& error_callback);

  // Called when the access token is obtained so the API request can be made.
  template <class ResponseProto>
  void OnAccessTokenFetched(
      const std::string& serialized_request,
      const base::Callback<void(const ResponseProto&)>& response_callback,
      const std::string& access_token);

  // Called with CryptAuthApiCallFlow completes successfully to deserialize and
  // return the result.
  template <class ResponseProto>
  void OnFlowSuccess(
      const base::Callback<void(const ResponseProto&)>& result_callback,
      const std::string& serialized_response);

  // Called when the current API call fails at any step.
  void OnApiCallFailed(const std::string& error_message);

  // Constructs and executes the actual HTTP request.
  std::unique_ptr<CryptAuthApiCallFlow> api_call_flow_;

  // Fetches the access token authorizing the API calls.
  std::unique_ptr<CryptAuthAccessTokenFetcher> access_token_fetcher_;

  // The context for network requests.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;

  // Contains basic device info of the client making the request that is sent to
  // CryptAuth with each API call.
  const cryptauth::DeviceClassifier device_classifier_;

  // True if an API call has been started. Remains true even after the API call
  // completes.
  bool has_call_started_;

  // URL path of the current request.
  std::string request_path_;

  // The access token fetched by |access_token_fetcher_|.
  std::string access_token_used_;

  // Called when the current request fails.
  ErrorCallback error_callback_;

  base::WeakPtrFactory<CryptAuthClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthClientImpl);
};

// Implementation of CryptAuthClientFactory.
class CryptAuthClientFactoryImpl : public CryptAuthClientFactory {
 public:
  // |token_service|: Gets the user's access token.
  //     Not owned, so |token_service| needs to outlive this object.
  // |account_id|: The account id of the user.
  // |url_request_context|: The request context to make the HTTP requests.
  // |device_classifier|: Contains basic device information of the client.
  CryptAuthClientFactoryImpl(
      OAuth2TokenService* token_service,
      const std::string& account_id,
      scoped_refptr<net::URLRequestContextGetter> url_request_context,
      const cryptauth::DeviceClassifier& device_classifier);
  ~CryptAuthClientFactoryImpl() override;

  // CryptAuthClientFactory:
  std::unique_ptr<CryptAuthClient> CreateInstance() override;

 private:
  OAuth2TokenService* token_service_;
  const std::string account_id_;
  const scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  const cryptauth::DeviceClassifier device_classifier_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthClientFactoryImpl);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CRYPTAUTH_CLIENT_IMPL_H
