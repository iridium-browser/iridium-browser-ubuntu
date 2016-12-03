// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SESSION_SESSION_H_
#define MASH_SESSION_SESSION_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"

namespace mojo {
class Connection;
}

namespace mash {
namespace session {

class Session : public shell::Service,
                public mojom::Session,
                public shell::InterfaceFactory<mojom::Session> {
 public:
  Session();
  ~Session() override;

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // mojom::Session:
  void Logout() override;
  void SwitchUser() override;
  void AddScreenlockStateListener(
      mojom::ScreenlockStateListenerPtr listener) override;
  void LockScreen() override;
  void UnlockScreen() override;

  // shell::InterfaceFactory<mojom::Session>:
  void Create(const shell::Identity& remote_identity,
              mojom::SessionRequest request) override;

  void StartWindowManager();
  void StartAppDriver();
  void StartQuickLaunch();

  void StartScreenlock();
  void StopScreenlock();

  // Starts the application at |url|, running |restart_callback| if the
  // connection to the application is closed.
  void StartRestartableService(const std::string& url,
                               const base::Closure& restart_callback);

  std::map<std::string, std::unique_ptr<shell::Connection>> connections_;
  bool screen_locked_;
  mojo::BindingSet<mojom::Session> bindings_;
  mojo::InterfacePtrSet<mojom::ScreenlockStateListener> screenlock_listeners_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace session
}  // namespace mash

#endif  // MASH_SESSION_SESSION_H_
