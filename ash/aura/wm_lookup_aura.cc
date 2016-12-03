// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_lookup_aura.h"

#include "ash/aura/wm_root_window_controller_aura.h"
#include "ash/aura/wm_window_aura.h"
#include "ash/common/wm_layout_manager.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"

namespace ash {

WmLookupAura::WmLookupAura() {
  WmLookup::Set(this);
}

WmLookupAura::~WmLookupAura() {
  WmLookup::Set(nullptr);
}

WmRootWindowController* WmLookupAura::GetRootWindowControllerWithDisplayId(
    int64_t id) {
  return WmRootWindowControllerAura::Get(Shell::GetInstance()
                                             ->window_tree_host_manager()
                                             ->GetRootWindowForDisplayId(id));
}

WmWindow* WmLookupAura::GetWindowForWidget(views::Widget* widget) {
  return WmWindowAura::Get(widget->GetNativeWindow());
}

}  // namespace ash
