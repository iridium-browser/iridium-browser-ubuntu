// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_WINDOW_FINDER_H_
#define SERVICES_UI_WS_WINDOW_FINDER_H_

namespace gfx {
class Point;
class Transform;
}

namespace ui {
namespace ws {

class ServerWindow;

// Find the deepest visible child of |root| that should receive an event at
// |location|. |location| is initially in the coordinate space of
// |root_window|, on return it is converted to the coordinates of the return
// value. Returns null if there is no valid event target window over |location|.
ServerWindow* FindDeepestVisibleWindowForEvents(
    ServerWindow* root_window,
    gfx::Point* location);

// Retrieve the transform to the provided |window|'s coordinate space from the
// root.
gfx::Transform GetTransformToWindow(ServerWindow* window);

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_WINDOW_FINDER_H_
