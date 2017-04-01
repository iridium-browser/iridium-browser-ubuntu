// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/overview/overview_button_tray.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace {

// Predefined padding for the icon used in this tray. These are to be set to the
// border of the icon, depending on the current shelf_alignment()
const int kHorizontalShelfHorizontalPadding = 8;
const int kHorizontalShelfVerticalPadding = 4;
const int kVerticalShelfHorizontalPadding = 2;
const int kVerticalShelfVerticalPadding = 5;

}  // namespace

namespace ash {

OverviewButtonTray::OverviewButtonTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf), icon_(nullptr) {
  icon_ = new views::ImageView();
  if (MaterialDesignController::IsShelfMaterial()) {
    SetInkDropMode(InkDropMode::ON);
    SetContentsBackground(false);
    gfx::ImageSkia image_md =
        CreateVectorIcon(kShelfOverviewIcon, kShelfIconColor);
    icon_->SetImage(image_md);
  } else {
    SetContentsBackground(true);
    gfx::ImageSkia* image_non_md =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_AURA_UBER_TRAY_OVERVIEW_MODE);
    icon_->SetImage(image_non_md);
  }
  SetIconBorderForShelfAlignment();
  tray_container()->AddChildView(icon_);

  WmShell::Get()->AddShellObserver(this);
  WmShell::Get()->GetSessionStateDelegate()->AddSessionStateObserver(this);
}

OverviewButtonTray::~OverviewButtonTray() {
  WmShell::Get()->RemoveShellObserver(this);
  WmShell::Get()->GetSessionStateDelegate()->RemoveSessionStateObserver(this);
}

void OverviewButtonTray::UpdateAfterLoginStatusChange(LoginStatus status) {
  UpdateIconVisibility();
}

bool OverviewButtonTray::PerformAction(const ui::Event& event) {
  WindowSelectorController* controller =
      WmShell::Get()->window_selector_controller();
  // Toggling overview mode will fail if there is no window to show.
  bool performed = controller->ToggleOverview();
  WmShell::Get()->RecordUserMetricsAction(UMA_TRAY_OVERVIEW);
  return performed;
}

void OverviewButtonTray::SessionStateChanged(
    session_manager::SessionState state) {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnMaximizeModeStarted() {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnMaximizeModeEnded() {
  UpdateIconVisibility();
}

void OverviewButtonTray::OnOverviewModeStarting() {
  SetIsActive(true);
}

void OverviewButtonTray::OnOverviewModeEnded() {
  SetIsActive(false);
}

void OverviewButtonTray::ClickedOutsideBubble() {}

base::string16 OverviewButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_OVERVIEW_BUTTON_ACCESSIBLE_NAME);
}

void OverviewButtonTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
  // This class has no bubbles to hide.
}

void OverviewButtonTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;

  TrayBackgroundView::SetShelfAlignment(alignment);
  SetIconBorderForShelfAlignment();
}

void OverviewButtonTray::SetIconBorderForShelfAlignment() {
  gfx::Insets insets;
  if (ash::MaterialDesignController::IsShelfMaterial()) {
    // Pad button size to align with other controls in the system tray.
    const gfx::ImageSkia image = icon_->GetImage();
    const int vertical_padding = (kTrayItemSize - image.height()) / 2;
    const int horizontal_padding = (kTrayItemSize - image.width()) / 2;
    insets = gfx::Insets(vertical_padding, horizontal_padding);
  } else {
    insets = IsHorizontalAlignment(shelf_alignment())
                 ? gfx::Insets(kHorizontalShelfVerticalPadding,
                               kHorizontalShelfHorizontalPadding)
                 : gfx::Insets(kVerticalShelfVerticalPadding,
                               kVerticalShelfHorizontalPadding);
  }
  icon_->SetBorder(views::CreateEmptyBorder(insets));
}

void OverviewButtonTray::UpdateIconVisibility() {
  // The visibility of the OverviewButtonTray has diverge from
  // WindowSelectorController::CanSelect. The visibility of the button should
  // not change during transient times in which CanSelect is false. Such as when
  // a modal dialog is present.
  WmShell* shell = WmShell::Get();
  SessionStateDelegate* session_state_delegate =
      shell->GetSessionStateDelegate();

  SetVisible(
      shell->maximize_mode_controller()->IsMaximizeModeWindowManagerEnabled() &&
      session_state_delegate->IsActiveUserSessionStarted() &&
      !session_state_delegate->IsScreenLocked() &&
      session_state_delegate->GetSessionState() ==
          session_manager::SessionState::ACTIVE &&
      shell->system_tray_delegate()->GetUserLoginStatus() !=
          LoginStatus::KIOSK_APP &&
      shell->system_tray_delegate()->GetUserLoginStatus() !=
          LoginStatus::ARC_KIOSK_APP);
}

}  // namespace ash
