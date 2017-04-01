// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_view.h"

#include "base/i18n/number_formatting.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/toolbar/toolbar_model.h"
#include "components/zoom/zoom_controller.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icons_public.h"

ZoomView::ZoomView(LocationBarView::Delegate* location_bar_delegate)
    : BubbleIconView(nullptr, 0),
      location_bar_delegate_(location_bar_delegate),
      image_id_(gfx::VectorIconId::VECTOR_ICON_NONE) {
  Update(NULL);
}

ZoomView::~ZoomView() {
}

void ZoomView::Update(zoom::ZoomController* zoom_controller) {
  if (!zoom_controller || zoom_controller->IsAtDefaultZoom() ||
      location_bar_delegate_->GetToolbarModel()->input_in_progress()) {
    SetVisible(false);
    ZoomBubbleView::CloseCurrentBubble();
    return;
  }

  SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_TOOLTIP_ZOOM,
      base::FormatPercent(zoom_controller->GetZoomPercent())));

  // The icon is hidden when the zoom level is default.
  image_id_ = zoom_controller->GetZoomRelativeToDefault() ==
                      zoom::ZoomController::ZOOM_BELOW_DEFAULT_ZOOM
                  ? gfx::VectorIconId::ZOOM_MINUS
                  : gfx::VectorIconId::ZOOM_PLUS;
  if (GetNativeTheme())
    UpdateIcon();

  SetVisible(true);
}

void ZoomView::OnExecuting(BubbleIconView::ExecuteSource source) {
  ZoomBubbleView::ShowBubble(location_bar_delegate_->GetWebContents(),
                             ZoomBubbleView::USER_GESTURE);
}

void ZoomView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  BubbleIconView::GetAccessibleNodeData(node_data);
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_ZOOM));
}

views::BubbleDialogDelegateView* ZoomView::GetBubble() const {
  return ZoomBubbleView::GetZoomBubble();
}

gfx::VectorIconId ZoomView::GetVectorIcon() const {
  return image_id_;
}
