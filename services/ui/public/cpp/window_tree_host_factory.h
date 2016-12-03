// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_HOST_FACTORY_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_HOST_FACTORY_H_

#include <memory>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/public/interfaces/window_tree_host.mojom.h"

namespace shell {
class Connector;
}

namespace ui {

class WindowManagerDelegate;
class WindowTreeClientDelegate;

// The following create a new window tree host. Supply a |factory| if you have
// already connected to mus, otherwise supply |shell|, which contacts mus and
// obtains a WindowTreeHostFactory.
void CreateWindowTreeHost(mojom::WindowTreeHostFactory* factory,
                          WindowTreeClientDelegate* delegate,
                          mojom::WindowTreeHostPtr* host,
                          WindowManagerDelegate* window_manager_delegate);
void CreateWindowTreeHost(shell::Connector* connector,
                          WindowTreeClientDelegate* delegate,
                          mojom::WindowTreeHostPtr* host,
                          WindowManagerDelegate* window_manager_delegate);

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_TREE_HOST_FACTORY_H_
