// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_MANAGER_CLIENT_FACTORY_H_
#define COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_MANAGER_CLIENT_FACTORY_H_

#include "components/view_manager/public/interfaces/view_manager.mojom.h"
#include "mojo/application/public/cpp/interface_factory.h"

namespace mojo {

class ViewManagerDelegate;
class Shell;

// Add an instance of this class to an incoming connection to allow it to
// instantiate ViewManagerClient implementations in response to
// ViewManagerClient requests.
class ViewManagerClientFactory : public InterfaceFactory<ViewManagerClient> {
 public:
  ViewManagerClientFactory(Shell* shell, ViewManagerDelegate* delegate);
  ~ViewManagerClientFactory() override;

  // InterfaceFactory<ViewManagerClient> implementation.
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ViewManagerClient> request) override;

 private:
  Shell* shell_;
  ViewManagerDelegate* delegate_;
};

}  // namespace mojo

#endif  // COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_VIEW_MANAGER_CLIENT_FACTORY_H_
