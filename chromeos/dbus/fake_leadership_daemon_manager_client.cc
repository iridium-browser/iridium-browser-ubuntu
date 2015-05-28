// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_leadership_daemon_manager_client.h"

#include "base/message_loop/message_loop.h"

namespace chromeos {

namespace {
void StringDBBusMethodCallbackThunk(const StringDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, std::string());
}

void ObjectPathDBBusMethodCallbackThunk(
    const ObjectPathDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS, dbus::ObjectPath());
}

void VoidDBBusMethodCallbackThunk(const VoidDBusMethodCallback& callback) {
  callback.Run(DBUS_METHOD_CALL_SUCCESS);
}
}  // namespace

FakeLeadershipDaemonManagerClient::FakeLeadershipDaemonManagerClient() {
}

FakeLeadershipDaemonManagerClient::~FakeLeadershipDaemonManagerClient() {
}

void FakeLeadershipDaemonManagerClient::Init(dbus::Bus* bus) {
}

void FakeLeadershipDaemonManagerClient::AddObserver(Observer* observer) {
}

void FakeLeadershipDaemonManagerClient::RemoveObserver(Observer* observer) {
}

void FakeLeadershipDaemonManagerClient::JoinGroup(
    const std::string& group,
    const base::DictionaryValue& options,
    const ObjectPathDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ObjectPathDBBusMethodCallbackThunk, callback));
}

void FakeLeadershipDaemonManagerClient::LeaveGroup(
    const std::string& object_path,
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&VoidDBBusMethodCallbackThunk, callback));
}

void FakeLeadershipDaemonManagerClient::SetScore(
    const std::string& object_path,
    int score,
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&VoidDBBusMethodCallbackThunk, callback));
}

void FakeLeadershipDaemonManagerClient::PokeLeader(
    const std::string& object_path,
    const VoidDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&VoidDBBusMethodCallbackThunk, callback));
}

void FakeLeadershipDaemonManagerClient::Ping(
    const StringDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&StringDBBusMethodCallbackThunk, callback));
}

const LeadershipDaemonManagerClient::GroupProperties*
FakeLeadershipDaemonManagerClient::GetGroupProperties(
    const dbus::ObjectPath& object_path) {
  return nullptr;
}

}  // namespace chromeos
