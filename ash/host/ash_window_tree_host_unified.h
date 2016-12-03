// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_UNIFIED_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_UNIFIED_H_

#include <vector>

#include "ash/host/ash_window_tree_host_platform.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"

namespace ui {
class Reflector;
}

namespace ash {
class DisplayInfo;

// A WTH used for unified desktop mode. This creates an offscreen
// compositor whose texture will be copied into each displays'
// compositor.
class AshWindowTreeHostUnified : public AshWindowTreeHostPlatform,
                                 public aura::WindowObserver {
 public:
  explicit AshWindowTreeHostUnified(const gfx::Rect& initial_bounds);
  ~AshWindowTreeHostUnified() override;

 private:
  // AshWindowTreeHost:
  void PrepareForShutdown() override;
  void RegisterMirroringHost(AshWindowTreeHost* mirroring_ash_host) override;

  // aura::WindowTreeHost:
  void SetBounds(const gfx::Rect& bounds) override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  // ui::PlatformWindow:
  void OnBoundsChanged(const gfx::Rect& bounds) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  std::vector<AshWindowTreeHost*> mirroring_hosts_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostUnified);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_UNIFIED_H_
