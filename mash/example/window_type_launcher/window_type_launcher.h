// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_
#define MASH_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_

#include <memory>

#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/service.h"

namespace views {
class AuraInit;
class Widget;
class WindowManagerConnection;
}

class WindowTypeLauncher
    : public shell::Service,
      public mash::mojom::Launchable,
      public shell::InterfaceFactory<mash::mojom::Launchable> {
 public:
  WindowTypeLauncher();
  ~WindowTypeLauncher() override;

  void RemoveWindow(views::Widget* window);

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // mash::mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override;

  // shell::InterfaceFactory<mash::mojom::Launchable>:
  void Create(const shell::Identity& remote_identity,
              mash::mojom::LaunchableRequest request) override;

  mojo::BindingSet<mash::mojom::Launchable> bindings_;
  std::vector<views::Widget*> windows_;

  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncher);
};

#endif  // MASH_EXAMPLE_WINDOW_TYPE_LAUNCHER_WINDOW_TYPE_LAUNCHER_H_
