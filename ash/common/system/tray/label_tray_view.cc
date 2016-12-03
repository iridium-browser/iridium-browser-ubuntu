// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/label_tray_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Maps a non-MD PNG resource id to its corresponding MD vector icon id.
// TODO(tdanderson): Remove this once material design is enabled by
// default. See crbug.com/614453.
gfx::VectorIconId ResourceIdToVectorIconId(int resource_id) {
  gfx::VectorIconId vector_id = gfx::VectorIconId::VECTOR_ICON_NONE;
#if defined(OS_CHROMEOS)
  switch (resource_id) {
    case IDR_AURA_UBER_TRAY_ENTERPRISE:
      return gfx::VectorIconId::SYSTEM_MENU_BUSINESS;
    case IDR_AURA_UBER_TRAY_BUBBLE_SESSION_LENGTH_LIMIT:
      return gfx::VectorIconId::SYSTEM_MENU_TIMER;
    case IDR_AURA_UBER_TRAY_CHILD_USER:
      return gfx::VectorIconId::SYSTEM_MENU_CHILD_USER;
    case IDR_AURA_UBER_TRAY_SUPERVISED_USER:
      return gfx::VectorIconId::SYSTEM_MENU_SUPERVISED_USER;
    default:
      NOTREACHED();
      break;
  }
#endif  // defined(OS_CHROMEOS)

  return vector_id;
}

}  // namespace

LabelTrayView::LabelTrayView(ViewClickListener* click_listener,
                             int icon_resource_id)
    : click_listener_(click_listener), icon_resource_id_(icon_resource_id) {
  SetLayoutManager(new views::FillLayout());
  SetVisible(false);
}

LabelTrayView::~LabelTrayView() {}

void LabelTrayView::SetMessage(const base::string16& message) {
  if (message_ == message)
    return;

  message_ = message;
  RemoveAllChildViews(true);

  if (!message_.empty()) {
    AddChildView(CreateChildView(message_));
    SetVisible(true);
  } else {
    SetVisible(false);
  }
}

views::View* LabelTrayView::CreateChildView(
    const base::string16& message) const {
  HoverHighlightView* child = new HoverHighlightView(click_listener_);
  if (icon_resource_id_) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::ImageSkia icon =
        MaterialDesignController::IsSystemTrayMenuMaterial()
            ? gfx::CreateVectorIcon(ResourceIdToVectorIconId(icon_resource_id_),
                                    kMenuIconColor)
            : *rb.GetImageSkiaNamed(icon_resource_id_);
    child->AddIconAndLabel(icon, message, false /* highlight */);
    child->SetBorder(views::Border::CreateEmptyBorder(
        0, kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingHorizontal));
    child->text_label()->SetMultiLine(true);
    child->text_label()->SizeToFit(kTrayNotificationContentsWidth);
  } else {
    child->AddLabel(message, gfx::ALIGN_LEFT, false /* highlight */);
    child->text_label()->SetMultiLine(true);
    child->text_label()->SizeToFit(kTrayNotificationContentsWidth +
                                   kNotificationIconWidth);
  }
  child->text_label()->SetAllowCharacterBreak(true);
  child->SetExpandable(true);
  child->SetVisible(true);
  return child;
}

}  // namespace ash
