// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/panels/panel_window_resizer.h"

#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm/panels/panel_layout_manager.h"
#include "ash/common/wm/window_parenting_utils.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

const int kPanelSnapToLauncherDistance = 30;

}  // namespace

PanelWindowResizer::~PanelWindowResizer() {}

// static
PanelWindowResizer* PanelWindowResizer::Create(
    WindowResizer* next_window_resizer,
    wm::WindowState* window_state) {
  return new PanelWindowResizer(next_window_resizer, window_state);
}

void PanelWindowResizer::Drag(const gfx::Point& location, int event_flags) {
  last_location_ = GetTarget()->GetParent()->ConvertPointToScreen(location);
  if (!did_move_or_resize_) {
    did_move_or_resize_ = true;
    StartedDragging();
  }

  // Check if the destination has changed displays.
  display::Screen* screen = display::Screen::GetScreen();
  const display::Display dst_display =
      screen->GetDisplayNearestPoint(last_location_);
  if (dst_display.id() !=
      panel_container_->GetRootWindow()->GetDisplayNearestWindow().id()) {
    // The panel is being dragged to a new display. If the previous container is
    // the current parent of the panel it will be informed of the end of drag
    // when the panel is reparented, otherwise let the previous container know
    // the drag is complete. If we told the panel's parent that the drag was
    // complete it would begin positioning the panel.
    if (GetTarget()->GetParent() != panel_container_)
      PanelLayoutManager::Get(panel_container_)->FinishDragging();
    WmWindow* dst_root =
        WmLookup::Get()
            ->GetRootWindowControllerWithDisplayId(dst_display.id())
            ->GetWindow();
    panel_container_ =
        dst_root->GetChildByShellWindowId(kShellWindowId_PanelContainer);

    // The panel's parent already knows that the drag is in progress for this
    // panel.
    if (panel_container_ && GetTarget()->GetParent() != panel_container_)
      PanelLayoutManager::Get(panel_container_)->StartDragging(GetTarget());
  }
  gfx::Point offset;
  gfx::Rect bounds(CalculateBoundsForDrag(location));
  if (!(details().bounds_change & WindowResizer::kBoundsChange_Resizes)) {
    window_state_->drag_details()->should_attach_to_shelf =
        AttachToLauncher(bounds, &offset);
  }
  gfx::Point modified_location(location.x() + offset.x(),
                               location.y() + offset.y());

  base::WeakPtr<PanelWindowResizer> resizer(weak_ptr_factory_.GetWeakPtr());
  next_window_resizer_->Drag(modified_location, event_flags);
  if (!resizer)
    return;

  if (details().should_attach_to_shelf &&
      !(details().bounds_change & WindowResizer::kBoundsChange_Resizes)) {
    UpdateLauncherPosition();
  }
}

void PanelWindowResizer::CompleteDrag() {
  // The root window can change when dragging into a different screen.
  next_window_resizer_->CompleteDrag();
  FinishDragging();
}

void PanelWindowResizer::RevertDrag() {
  next_window_resizer_->RevertDrag();
  window_state_->drag_details()->should_attach_to_shelf = was_attached_;
  FinishDragging();
}

PanelWindowResizer::PanelWindowResizer(WindowResizer* next_window_resizer,
                                       wm::WindowState* window_state)
    : WindowResizer(window_state),
      next_window_resizer_(next_window_resizer),
      panel_container_(NULL),
      initial_panel_container_(NULL),
      did_move_or_resize_(false),
      was_attached_(
          GetTarget()->GetBoolProperty(WmWindowProperty::PANEL_ATTACHED)),
      weak_ptr_factory_(this) {
  DCHECK(details().is_resizable);
  panel_container_ = GetTarget()->GetRootWindow()->GetChildByShellWindowId(
      kShellWindowId_PanelContainer);
  initial_panel_container_ = panel_container_;
}

bool PanelWindowResizer::AttachToLauncher(const gfx::Rect& bounds,
                                          gfx::Point* offset) {
  bool should_attach = false;
  if (panel_container_) {
    PanelLayoutManager* panel_layout_manager =
        PanelLayoutManager::Get(panel_container_);
    gfx::Rect launcher_bounds = GetTarget()->GetParent()->ConvertRectFromScreen(
        panel_layout_manager->shelf()->GetWindow()->GetBoundsInScreen());
    switch (panel_layout_manager->shelf()->GetAlignment()) {
      case SHELF_ALIGNMENT_BOTTOM:
      case SHELF_ALIGNMENT_BOTTOM_LOCKED:
        if (bounds.bottom() >=
            (launcher_bounds.y() - kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_y(launcher_bounds.y() - bounds.height() - bounds.y());
        }
        break;
      case SHELF_ALIGNMENT_LEFT:
        if (bounds.x() <=
            (launcher_bounds.right() + kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_x(launcher_bounds.right() - bounds.x());
        }
        break;
      case SHELF_ALIGNMENT_RIGHT:
        if (bounds.right() >=
            (launcher_bounds.x() - kPanelSnapToLauncherDistance)) {
          should_attach = true;
          offset->set_x(launcher_bounds.x() - bounds.width() - bounds.x());
        }
        break;
    }
  }
  return should_attach;
}

void PanelWindowResizer::StartedDragging() {
  // Tell the panel layout manager that we are dragging this panel before
  // attaching it so that it does not get repositioned.
  if (panel_container_)
    PanelLayoutManager::Get(panel_container_)->StartDragging(GetTarget());
  if (!was_attached_) {
    // Attach the panel while dragging, placing it in front of other panels.
    WmWindow* target = GetTarget();
    target->SetBoolProperty(WmWindowProperty::PANEL_ATTACHED, true);
    // We use root window coordinates to ensure that during the drag the panel
    // is reparented to a container in the root window that has that window.
    WmWindow* target_root = target->GetRootWindow();
    WmWindow* old_parent = target->GetParent();
    target->SetParentUsingContext(target_root,
                                  target_root->GetBoundsInScreen());
    wm::ReparentTransientChildrenOfChild(target, old_parent,
                                         target->GetParent());
  }
}

void PanelWindowResizer::FinishDragging() {
  if (!did_move_or_resize_)
    return;
  if (GetTarget()->GetBoolProperty(WmWindowProperty::PANEL_ATTACHED) !=
      details().should_attach_to_shelf) {
    GetTarget()->SetBoolProperty(WmWindowProperty::PANEL_ATTACHED,
                                 details().should_attach_to_shelf);
    // We use last known location to ensure that after the drag the panel
    // is reparented to a container in the root window that has that location.
    WmWindow* target = GetTarget();
    WmWindow* target_root = target->GetRootWindow();
    WmWindow* old_parent = target->GetParent();
    target->SetParentUsingContext(target_root,
                                  gfx::Rect(last_location_, gfx::Size()));
    wm::ReparentTransientChildrenOfChild(target, old_parent,
                                         target->GetParent());
  }

  // If we started the drag in one root window and moved into another root
  // but then canceled the drag we may need to inform the original layout
  // manager that the drag is finished.
  if (initial_panel_container_ != panel_container_)
    PanelLayoutManager::Get(initial_panel_container_)->FinishDragging();
  if (panel_container_)
    PanelLayoutManager::Get(panel_container_)->FinishDragging();
}

void PanelWindowResizer::UpdateLauncherPosition() {
  if (panel_container_) {
    PanelLayoutManager::Get(panel_container_)
        ->shelf()
        ->UpdateIconPositionForPanel(GetTarget());
  }
}

}  // namespace ash
