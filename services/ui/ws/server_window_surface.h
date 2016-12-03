// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_SERVER_WINDOW_SURFACE_H_
#define SERVICES_UI_WS_SERVER_WINDOW_SURFACE_H_

#include <set>

#include "base/macros.h"
#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/surface.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws/ids.h"

namespace ui {

class SurfacesState;

namespace ws {

class ServerWindow;
class ServerWindowSurfaceManager;

// Server side representation of a WindowSurface.
class ServerWindowSurface : public mojom::Surface,
                            public cc::SurfaceFactoryClient {
 public:
  ServerWindowSurface(ServerWindowSurfaceManager* manager,
                      mojo::InterfaceRequest<mojom::Surface> request,
                      mojom::SurfaceClientPtr client);

  ~ServerWindowSurface() override;

  const gfx::Size& last_submitted_frame_size() const {
    return last_submitted_frame_size_;
  }

  bool may_contain_video() const { return may_contain_video_; }

  // mojom::Surface:
  void SubmitCompositorFrame(
      cc::CompositorFrame frame,
      const SubmitCompositorFrameCallback& callback) override;

  const cc::SurfaceId& id() const { return surface_id_; }

  // Destroys old surfaces that have been outdated by a new surface.
  void DestroySurfacesScheduledForDestruction();

 private:
  ServerWindow* window();

  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  ServerWindowSurfaceManager* manager_;  // Owns this.

  gfx::Size last_submitted_frame_size_;

  cc::SurfaceId surface_id_;
  cc::SurfaceIdAllocator surface_id_allocator_;
  cc::SurfaceFactory surface_factory_;

  mojom::SurfaceClientPtr client_;
  mojo::Binding<Surface> binding_;

  // Set of surface ids that need to be destroyed.
  std::set<cc::SurfaceId> surfaces_scheduled_for_destruction_;

  bool may_contain_video_ = false;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowSurface);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_SERVER_WINDOW_SURFACE_H_
