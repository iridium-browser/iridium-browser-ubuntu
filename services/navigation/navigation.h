// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_NAVIGATION_H_
#define SERVICES_NAVIGATION_NAVIGATION_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "content/public/common/connection_filter.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context_ref.h"

namespace content {
class BrowserContext;
}

namespace navigation {

class Navigation : public content::ConnectionFilter, public mojom::ViewFactory {
 public:
  Navigation();
  ~Navigation() override;

 private:
  // content::ConnectionFilter:
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry,
                 shell::Connector* connector) override;

  // mojom::ViewFactory:
  void CreateView(mojom::ViewClientPtr client,
                  mojom::ViewRequest request) override;

  void CreateViewFactory(mojom::ViewFactoryRequest request);
  void ViewFactoryLost();

  scoped_refptr<base::SequencedTaskRunner> view_task_runner_;

  shell::Connector* connector_ = nullptr;
  std::string client_user_id_;

  shell::ServiceContextRefFactory ref_factory_;
  std::set<std::unique_ptr<shell::ServiceContextRef>> refs_;

  mojo::BindingSet<mojom::ViewFactory> bindings_;

  base::WeakPtrFactory<Navigation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Navigation);
};

}  // navigation

#endif  // SERVICES_NAVIGATION_NAVIGATION_H_
