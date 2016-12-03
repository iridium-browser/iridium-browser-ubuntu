// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/window_tree_host_factory.h"

#include "services/shell/public/cpp/connector.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"

namespace ui {

void CreateWindowTreeHost(mojom::WindowTreeHostFactory* factory,
                          WindowTreeClientDelegate* delegate,
                          mojom::WindowTreeHostPtr* host,
                          WindowManagerDelegate* window_manager_delegate) {
  mojom::WindowTreeClientPtr tree_client;
  new WindowTreeClient(delegate, window_manager_delegate,
                       GetProxy(&tree_client));
  factory->CreateWindowTreeHost(GetProxy(host), std::move(tree_client));
}

void CreateWindowTreeHost(shell::Connector* connector,
                          WindowTreeClientDelegate* delegate,
                          mojom::WindowTreeHostPtr* host,
                          WindowManagerDelegate* window_manager_delegate) {
  mojom::WindowTreeHostFactoryPtr factory;
  connector->ConnectToInterface("mojo:ui", &factory);
  CreateWindowTreeHost(factory.get(), delegate, host, window_manager_delegate);
}

}  // namespace ui
