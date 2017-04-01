// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/auth_policy_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/core/account_id/account_id.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// The first device policy fetch after joining Active Directory can be very slow
// because machine credentials need to propagate through the AD deployment.
const int kRefreshDevicePolicyTimeoutMilliseconds = 90000;

authpolicy::ErrorType GetErrorFromReader(dbus::MessageReader* reader) {
  int32_t int_error;
  if (!reader->PopInt32(&int_error)) {
    DLOG(ERROR) << "AuthPolicyClient: Failed to get an error from the response";
    return authpolicy::ERROR_DBUS_FAILURE;
  }
  if (int_error < 0 || int_error >= authpolicy::ERROR_COUNT)
    return authpolicy::ERROR_UNKNOWN;
  return static_cast<authpolicy::ErrorType>(int_error);
}

class AuthPolicyClientImpl : public AuthPolicyClient {
 public:
  AuthPolicyClientImpl() : weak_ptr_factory_(this) {}

  ~AuthPolicyClientImpl() override {}

  // AuthPolicyClient override.
  void JoinAdDomain(const std::string& machine_name,
                    const std::string& user_principal_name,
                    int password_fd,
                    const JoinCallback& callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthPolicyJoinADDomain);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(machine_name);
    writer.AppendString(user_principal_name);
    writer.AppendFileDescriptor(password_fd);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&AuthPolicyClientImpl::HandleJoinCallback,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void AuthenticateUser(const std::string& user_principal_name,
                        int password_fd,
                        const AuthCallback& callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthPolicyAuthenticateUser);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(user_principal_name);
    writer.AppendFileDescriptor(password_fd);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&AuthPolicyClientImpl::HandleAuthCallback,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RefreshDevicePolicy(const RefreshPolicyCallback& callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthPolicyRefreshDevicePolicy);
    proxy_->CallMethod(
        &method_call, kRefreshDevicePolicyTimeoutMilliseconds,
        base::Bind(&AuthPolicyClientImpl::HandleRefreshPolicyCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RefreshUserPolicy(const AccountId& account_id,
                         const RefreshPolicyCallback& callback) override {
    DCHECK(account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY);
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthPolicyRefreshUserPolicy);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_id.GetAccountIdKey());
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&AuthPolicyClientImpl::HandleRefreshPolicyCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    bus_ = bus;
    proxy_ = bus_->GetObjectProxy(
        authpolicy::kAuthPolicyServiceName,
        dbus::ObjectPath(authpolicy::kAuthPolicyServicePath));
  }

 private:
  void HandleRefreshPolicyCallback(const RefreshPolicyCallback& callback,
                                   dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "RefreshDevicePolicy: failed to call to authpolicy";
      callback.Run(false);
      return;
    }
    dbus::MessageReader reader(response);
    callback.Run(GetErrorFromReader(&reader) == authpolicy::ERROR_NONE);
  }

  void HandleJoinCallback(const JoinCallback& callback,
                          dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "Join: Couldn't call to authpolicy";
      callback.Run(authpolicy::ERROR_DBUS_FAILURE);
      return;
    }

    dbus::MessageReader reader(response);
    callback.Run(GetErrorFromReader(&reader));
  }

  void HandleAuthCallback(const AuthCallback& callback,
                          dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "Auth: Failed to  call to authpolicy";
      callback.Run(authpolicy::ERROR_DBUS_FAILURE, std::string());
      return;
    }
    dbus::MessageReader reader(response);
    const authpolicy::ErrorType error(GetErrorFromReader(&reader));
    std::string user_id;
    if (!reader.PopString(&user_id))
      DLOG(ERROR) << "Auth: Failed to get user_id from the response";
    callback.Run(error, user_id);
  }

  dbus::Bus* bus_ = nullptr;
  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AuthPolicyClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthPolicyClientImpl);
};

}  // namespace

AuthPolicyClient::AuthPolicyClient() {}

AuthPolicyClient::~AuthPolicyClient() {}

// static
AuthPolicyClient* AuthPolicyClient::Create() {
  return new AuthPolicyClientImpl();
}

}  // namespace chromeos
