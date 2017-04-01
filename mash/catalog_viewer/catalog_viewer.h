// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_CATALOG_VIEWER_CATALOG_VIEWER_H_
#define MASH_CATALOG_VIEWER_CATALOG_VIEWER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"

namespace views {
class AuraInit;
class Widget;
}

namespace mash {
namespace catalog_viewer {

class CatalogViewer
    : public service_manager::Service,
      public mojom::Launchable,
      public service_manager::InterfaceFactory<mojom::Launchable> {
 public:
  CatalogViewer();
  ~CatalogViewer() override;

  void RemoveWindow(views::Widget* window);

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // mojom::Launchable:
  void Launch(uint32_t what, mojom::LaunchMode how) override;

  // service_manager::InterfaceFactory<mojom::Launchable>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::LaunchableRequest request) override;

  mojo::BindingSet<mojom::Launchable> bindings_;
  std::vector<views::Widget*> windows_;

  tracing::Provider tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(CatalogViewer);
};

}  // namespace catalog_viewer
}  // namespace mash

#endif  // MASH_CATALOG_VIEWER_CATALOG_VIEWER_H_
