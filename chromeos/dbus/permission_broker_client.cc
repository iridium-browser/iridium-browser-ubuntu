// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/permission_broker_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using permission_broker::kCheckPathAccess;
using permission_broker::kOpenPath;
using permission_broker::kPermissionBrokerInterface;
using permission_broker::kPermissionBrokerServiceName;
using permission_broker::kPermissionBrokerServicePath;
using permission_broker::kReleaseTcpPort;
using permission_broker::kReleaseUdpPort;
using permission_broker::kRequestPathAccess;
using permission_broker::kRequestTcpPortAccess;
using permission_broker::kRequestUdpPortAccess;

namespace chromeos {

class PermissionBrokerClientImpl : public PermissionBrokerClient {
 public:
  PermissionBrokerClientImpl() : proxy_(NULL), weak_ptr_factory_(this) {}

  void CheckPathAccess(const std::string& path,
                       const ResultCallback& callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kCheckPathAccess);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&PermissionBrokerClientImpl::OnResponse,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RequestPathAccess(const std::string& path,
                         const int interface_id,
                         const ResultCallback& callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kRequestPathAccess);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);
    writer.AppendInt32(interface_id);
    proxy_->CallMethod(&method_call,
                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&PermissionBrokerClientImpl::OnResponse,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void OpenPath(const std::string& path,
                const OpenPathCallback& callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kOpenPath);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&PermissionBrokerClientImpl::OnOpenPathResponse,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RequestTcpPortAccess(uint16 port,
                            const std::string& interface,
                            const dbus::FileDescriptor& lifeline_fd,
                            const ResultCallback& callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kRequestTcpPortAccess);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(port);
    writer.AppendString(interface);
    writer.AppendFileDescriptor(lifeline_fd);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&PermissionBrokerClientImpl::OnResponse,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RequestUdpPortAccess(uint16 port,
                            const std::string& interface,
                            const dbus::FileDescriptor& lifeline_fd,
                            const ResultCallback& callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kRequestUdpPortAccess);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(port);
    writer.AppendString(interface);
    writer.AppendFileDescriptor(lifeline_fd);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&PermissionBrokerClientImpl::OnResponse,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void ReleaseTcpPort(uint16 port,
                      const std::string& interface,
                      const ResultCallback& callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kReleaseTcpPort);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(port);
    writer.AppendString(interface);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&PermissionBrokerClientImpl::OnResponse,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void ReleaseUdpPort(uint16 port,
                      const std::string& interface,
                      const ResultCallback& callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kReleaseUdpPort);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(port);
    writer.AppendString(interface);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&PermissionBrokerClientImpl::OnResponse,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ =
        bus->GetObjectProxy(kPermissionBrokerServiceName,
                            dbus::ObjectPath(kPermissionBrokerServicePath));
  }

 private:
  // Handle a DBus response from the permission broker, invoking the callback
  // that the method was originally called with with the success response.
  void OnResponse(const ResultCallback& callback, dbus::Response* response) {
    if (!response) {
      LOG(WARNING) << "Access request method call failed.";
      callback.Run(false);
      return;
    }

    bool result = false;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&result))
      LOG(WARNING) << "Could not parse response: " << response->ToString();
    callback.Run(result);
  }

  void OnOpenPathResponse(const OpenPathCallback& callback,
                          dbus::Response* response) {
    dbus::FileDescriptor fd;
    if (response) {
      dbus::MessageReader reader(response);
      if (!reader.PopFileDescriptor(&fd))
        LOG(WARNING) << "Could not parse response: " << response->ToString();
    } else {
      LOG(WARNING) << "Access request method call failed.";
    }

    callback.Run(fd.Pass());
  }

  dbus::ObjectProxy* proxy_;

  // Note: This should remain the last member so that it will be destroyed
  // first, invalidating its weak pointers, before the other members are
  // destroyed.
  base::WeakPtrFactory<PermissionBrokerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBrokerClientImpl);
};

PermissionBrokerClient::PermissionBrokerClient() {}

PermissionBrokerClient::~PermissionBrokerClient() {}

PermissionBrokerClient* PermissionBrokerClient::Create() {
  return new PermissionBrokerClientImpl();
}

}  // namespace chromeos
