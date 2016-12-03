// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/session/session.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mash/login/public/interfaces/login.mojom.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"

namespace {

void LogAndCallServiceRestartCallback(const std::string& url,
                                      const base::Closure& callback) {
  LOG(ERROR) << "Restarting service: " << url;
  callback.Run();
}

}  // namespace

namespace mash {
namespace session {

Session::Session() : screen_locked_(false) {}
Session::~Session() {}

void Session::OnStart(const shell::Identity& identity) {
  StartAppDriver();
  StartWindowManager();
  StartQuickLaunch();
  // Launch a chrome window for dev convience; don't do this in the long term.
  connector()->Connect("exe:chrome");
}

bool Session::OnConnect(const shell::Identity& remote_identity,
                        shell::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::Session>(this);
  return true;
}

void Session::Logout() {
  // TODO(beng): Notify connected listeners that login is happening, potentially
  // give them the option to stop it.
  mash::login::mojom::LoginPtr login;
  connector()->ConnectToInterface("mojo:login", &login);
  login->ShowLoginUI();
  // This kills the user environment.
  base::MessageLoop::current()->QuitWhenIdle();
}

void Session::SwitchUser() {
  mash::login::mojom::LoginPtr login;
  connector()->ConnectToInterface("mojo:login", &login);
  login->SwitchUser();
}

void Session::AddScreenlockStateListener(
    mojom::ScreenlockStateListenerPtr listener) {
  listener->ScreenlockStateChanged(screen_locked_);
  screenlock_listeners_.AddPtr(std::move(listener));
}

void Session::LockScreen() {
  if (screen_locked_)
    return;
  screen_locked_ = true;
  screenlock_listeners_.ForAllPtrs(
      [](mojom::ScreenlockStateListener* listener) {
        listener->ScreenlockStateChanged(true);
      });
  StartScreenlock();
}
void Session::UnlockScreen() {
  if (!screen_locked_)
    return;
  screen_locked_ = false;
  screenlock_listeners_.ForAllPtrs(
      [](mojom::ScreenlockStateListener* listener) {
        listener->ScreenlockStateChanged(false);
      });
  StopScreenlock();
}

void Session::Create(const shell::Identity& remote_identity,
                     mojom::SessionRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void Session::StartWindowManager() {
  StartRestartableService(
      "mojo:ash",
      base::Bind(&Session::StartWindowManager,
                 base::Unretained(this)));
}

void Session::StartAppDriver() {
  StartRestartableService(
      "mojo:app_driver",
      base::Bind(&Session::StartAppDriver, base::Unretained(this)));
}

void Session::StartQuickLaunch() {
  StartRestartableService(
      "mojo:quick_launch",
      base::Bind(&Session::StartQuickLaunch,
                 base::Unretained(this)));
}

void Session::StartScreenlock() {
  StartRestartableService(
      "mojo:screenlock",
      base::Bind(&Session::StartScreenlock,
                 base::Unretained(this)));
}

void Session::StopScreenlock() {
  auto connection = connections_.find("mojo:screenlock");
  DCHECK(connections_.end() != connection);
  connections_.erase(connection);
}

void Session::StartRestartableService(
    const std::string& url,
    const base::Closure& restart_callback) {
  // TODO(beng): This would be the place to insert logic that counted restarts
  //             to avoid infinite crash-restart loops.
  std::unique_ptr<shell::Connection> connection =
      connector()->Connect(url);
  // Note: |connection| may be null if we've lost our connection to the shell.
  if (connection) {
    connection->SetConnectionLostClosure(
        base::Bind(&LogAndCallServiceRestartCallback, url, restart_callback));
    connections_[url] = std::move(connection);
  }
}

}  // namespace session
}  // namespace main
