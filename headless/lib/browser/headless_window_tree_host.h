// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_TREE_HOST_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_TREE_HOST_H_

#include "base/macros.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"

namespace headless {

class HeadlessWindowTreeHost : public aura::WindowTreeHost,
                               public ui::PlatformEventDispatcher {
 public:
  explicit HeadlessWindowTreeHost(const gfx::Rect& bounds);
  ~HeadlessWindowTreeHost() override;

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Point GetLocationOnScreenInPixels() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorNative(gfx::NativeCursor cursor_type) override;
  void MoveCursorToScreenLocationInPixels(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;
  gfx::ICCProfile GetICCProfileForCurrentDisplay() override;

 private:
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessWindowTreeHost);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_TREE_HOST_H_
