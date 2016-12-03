// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_finder.h"

#include "base/containers/adapters.h"
#include "services/ui/surfaces/surfaces_state.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/server_window_surface.h"
#include "services/ui/ws/server_window_surface_manager.h"
#include "services/ui/ws/window_coordinate_conversions.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/transform.h"

namespace ui {
namespace ws {

bool IsValidWindowForEvents(ServerWindow* window) {
  ServerWindowSurfaceManager* surface_manager = window->surface_manager();
  return surface_manager &&
         surface_manager->HasSurfaceOfType(mojom::SurfaceType::DEFAULT);
}

ServerWindow* FindDeepestVisibleWindowForEvents(ServerWindow* window,
                                                gfx::Point* location) {
  if (!window->can_accept_events())
    return nullptr;

  const ServerWindow::Windows& children = window->children();
  for (ServerWindow* child : base::Reversed(children)) {
    if (!child->visible() || !child->can_accept_events())
      continue;

    // TODO(sky): support transform.
    gfx::Point child_location(location->x() - child->bounds().x(),
                              location->y() - child->bounds().y());
    gfx::Rect child_bounds(child->bounds().size());
    child_bounds.Inset(-child->extended_hit_test_region().left(),
                       -child->extended_hit_test_region().top(),
                       -child->extended_hit_test_region().right(),
                       -child->extended_hit_test_region().bottom());
    if (!child_bounds.Contains(child_location))
      continue;

    if (child->hit_test_mask() &&
        !child->hit_test_mask()->Contains(child_location))
      continue;

    *location = child_location;
    ServerWindow* result = FindDeepestVisibleWindowForEvents(child, location);
    if (IsValidWindowForEvents(result))
      return result;
  }
  return window;
}

gfx::Transform GetTransformToWindow(ServerWindow* window) {
  gfx::Transform transform;
  ServerWindow* current = window;
  while (current->parent()) {
    transform.Translate(-current->bounds().x(), -current->bounds().y());
    current = current->parent();
  }
  return transform;
}

}  // namespace ws
}  // namespace ui
