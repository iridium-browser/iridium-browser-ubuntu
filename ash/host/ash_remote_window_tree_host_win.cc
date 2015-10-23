// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_remote_window_tree_host_win.h"

#include "ash/host/root_window_transformer.h"
#include "ash/ime/input_method_event_handler.h"
#include "ui/events/event_processor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/transform.h"

namespace ash {

AshRemoteWindowTreeHostWin::AshRemoteWindowTreeHostWin(HWND remote_hwnd)
    : aura::RemoteWindowTreeHostWin(),
      transformer_helper_(this) {
  SetRemoteWindowHandle(remote_hwnd);
  transformer_helper_.Init();
}

AshRemoteWindowTreeHostWin::~AshRemoteWindowTreeHostWin() {}

void AshRemoteWindowTreeHostWin::ToggleFullScreen() {}

bool AshRemoteWindowTreeHostWin::ConfineCursorToRootWindow() { return false; }

void AshRemoteWindowTreeHostWin::UnConfineCursor() {}

void AshRemoteWindowTreeHostWin::SetRootWindowTransformer(
    scoped_ptr<RootWindowTransformer> transformer) {
  transformer_helper_.SetRootWindowTransformer(transformer.Pass());
}

gfx::Insets AshRemoteWindowTreeHostWin::GetHostInsets() const {
  return gfx::Insets();
}

aura::WindowTreeHost* AshRemoteWindowTreeHostWin::AsWindowTreeHost() {
  return this;
}

gfx::Transform AshRemoteWindowTreeHostWin::GetRootTransform() const {
  return transformer_helper_.GetTransform();
}

void AshRemoteWindowTreeHostWin::SetRootTransform(
    const gfx::Transform& transform) {
  transformer_helper_.SetTransform(transform);
}

gfx::Transform AshRemoteWindowTreeHostWin::GetInverseRootTransform() const {
  return transformer_helper_.GetInverseTransform();
}

void AshRemoteWindowTreeHostWin::UpdateRootWindowSize(
    const gfx::Size& host_size) {
  transformer_helper_.UpdateWindowSize(host_size);
}

ui::EventDispatchDetails AshRemoteWindowTreeHostWin::DispatchKeyEventPostIME(
    ui::KeyEvent* event) {
  input_method_handler()->SetPostIME(true);
  ui::EventDispatchDetails details =
      event_processor()->OnEventFromSource(event);
  if (!details.dispatcher_destroyed)
    input_method_handler()->SetPostIME(false);
  return details;
}

}  // namespace ash
