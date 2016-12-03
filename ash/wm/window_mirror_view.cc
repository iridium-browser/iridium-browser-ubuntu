// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/wm/forwarding_layer_delegate.h"
#include "ash/common/wm/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace wm {
namespace {

void EnsureAllChildrenAreVisible(ui::Layer* layer) {
  std::list<ui::Layer*> layers;
  layers.push_back(layer);
  while (!layers.empty()) {
    for (auto* child : layers.front()->children())
      layers.push_back(child);
    layers.front()->SetVisible(true);
    layers.pop_front();
  }
}

}  // namespace

WindowMirrorView::WindowMirrorView(WmWindowAura* window) : target_(window) {
  DCHECK(window);
}

WindowMirrorView::~WindowMirrorView() {
  if (layer_owner_)
    target_->aura_window()->ClearProperty(aura::client::kMirroringEnabledKey);
}

gfx::Size WindowMirrorView::GetPreferredSize() const {
  return GetClientAreaBounds().size();
}

void WindowMirrorView::Layout() {
  // If |layer_owner_| hasn't been initialized (|this| isn't on screen), no-op.
  if (!layer_owner_)
    return;

  // Position at 0, 0.
  GetMirrorLayer()->SetBounds(gfx::Rect(GetMirrorLayer()->bounds().size()));

  gfx::Transform transform;
  gfx::Rect client_area_bounds = GetClientAreaBounds();
  // Scale down if necessary.
  if (size() != target_->GetBounds().size()) {
    const float scale =
        width() / static_cast<float>(client_area_bounds.width());
    transform.Scale(scale, scale);
  }
  // Reposition such that the client area is the only part visible.
  transform.Translate(-client_area_bounds.x(), -client_area_bounds.y());
  GetMirrorLayer()->SetTransform(transform);
}

bool WindowMirrorView::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void WindowMirrorView::OnVisibleBoundsChanged() {
  if (!layer_owner_ && !GetVisibleBounds().IsEmpty())
    InitLayerOwner();
}

ui::LayerDelegate* WindowMirrorView::CreateDelegate(ui::Layer* new_layer,
                                                    ui::Layer* old_layer) {
  if (!old_layer || !old_layer->delegate())
    return nullptr;
  delegates_.push_back(
      base::MakeUnique<ForwardingLayerDelegate>(new_layer, old_layer));
  return delegates_.back().get();
}

void WindowMirrorView::InitLayerOwner() {
  if (!layer_owner_) {
    target_->aura_window()->SetProperty(aura::client::kMirroringEnabledKey,
                                        true);
  }

  layer_owner_ = ::wm::RecreateLayers(target_->aura_window(), this);

  GetMirrorLayer()->parent()->Remove(GetMirrorLayer());
  SetPaintToLayer(true);
  layer()->Add(GetMirrorLayer());
  // This causes us to clip the non-client areas of the window.
  layer()->SetMasksToBounds(true);

  // Some extra work is needed when the target window is minimized.
  if (target_->GetWindowState()->IsMinimized()) {
    GetMirrorLayer()->SetOpacity(1);
    EnsureAllChildrenAreVisible(GetMirrorLayer());
  }

  Layout();
}

ui::Layer* WindowMirrorView::GetMirrorLayer() {
  return layer_owner_->root();
}

gfx::Rect WindowMirrorView::GetClientAreaBounds() const {
  // The target window may not have a widget in unit tests.
  if (!target_->GetInternalWidget())
    return gfx::Rect();
  views::View* client_view = target_->GetInternalWidget()->client_view();
  return client_view->ConvertRectToWidget(client_view->GetLocalBounds());
}

}  // namespace wm
}  // namespace ash
