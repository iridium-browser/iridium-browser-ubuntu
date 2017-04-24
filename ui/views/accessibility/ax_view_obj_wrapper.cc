// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_view_obj_wrapper.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event_utils.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

AXViewObjWrapper::AXViewObjWrapper(View* view)  : view_(view) {
  if (view->GetWidget())
    AXAuraObjCache::GetInstance()->GetOrCreate(view->GetWidget());
}

AXViewObjWrapper::~AXViewObjWrapper() {}

AXAuraObjWrapper* AXViewObjWrapper::GetParent() {
  AXAuraObjCache* cache = AXAuraObjCache::GetInstance();
  if (view_->parent())
    return cache->GetOrCreate(view_->parent());

  if (view_->GetWidget())
    return cache->GetOrCreate(view_->GetWidget());

  return NULL;
}

void AXViewObjWrapper::GetChildren(
    std::vector<AXAuraObjWrapper*>* out_children) {
  // TODO(dtseng): Need to handle |Widget| child of |View|.
  for (int i = 0; i < view_->child_count(); ++i) {
    if (!view_->child_at(i)->visible())
      continue;

    AXAuraObjWrapper* child =
        AXAuraObjCache::GetInstance()->GetOrCreate(view_->child_at(i));
    out_children->push_back(child);
  }
}

void AXViewObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->state = 0;
  view_->GetAccessibleNodeData(out_node_data);

  out_node_data->id = GetID();

  if (view_->IsFocusable())
    out_node_data->state |= 1 << ui::AX_STATE_FOCUSABLE;
  if (!view_->visible())
    out_node_data->state |= 1 << ui::AX_STATE_INVISIBLE;

  out_node_data->location = gfx::RectF(view_->GetBoundsInScreen());
}

int32_t AXViewObjWrapper::GetID() {
  return AXAuraObjCache::GetInstance()->GetID(view_);
}

void AXViewObjWrapper::DoDefault() {
  gfx::Rect rect = view_->GetLocalBounds();
  gfx::Point center = rect.CenterPoint();
  view_->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  view_->OnMouseReleased(ui::MouseEvent(
      ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
}

void AXViewObjWrapper::Focus() {
  view_->RequestFocus();
}

void AXViewObjWrapper::MakeVisible() {
  view_->ScrollRectToVisible(view_->GetLocalBounds());
}

void AXViewObjWrapper::SetSelection(int32_t start, int32_t end) {
  // TODO(dtseng): Implement.
}

void AXViewObjWrapper::ShowContextMenu() {
  view_->ShowContextMenu(view_->bounds().CenterPoint(),
                         ui::MENU_SOURCE_KEYBOARD);
}

}  // namespace views
