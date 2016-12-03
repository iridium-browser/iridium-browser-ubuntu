// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/common_palette_tool.h"

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/system/chromeos/palette/palette_tool_manager.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"

namespace ash {
namespace {

// Size of the icons in DP.
const int kIconSize = 20;

// Distance between the icon and the check from the egdes in DP.
const int kMarginFromEdges = 14;

// Extra distance between the icon and the left edge in DP.
const int kExtraMarginFromLeftEdge = 4;

// Distance between the icon and the name of the tool in DP.
const int kMarginBetweenIconAndText = 18;

void AddHistogramTimes(PaletteToolId id, base::TimeDelta duration) {
  if (id == PaletteToolId::LASER_POINTER) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.Shelf.Palette.InLaserPointerMode", duration,
                               base::TimeDelta::FromMilliseconds(100),
                               base::TimeDelta::FromHours(1), 50);
  } else if (id == PaletteToolId::MAGNIFY) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.Shelf.Palette.InMagnifyMode", duration,
                               base::TimeDelta::FromMilliseconds(100),
                               base::TimeDelta::FromHours(1), 50);
  }
}

}  // namespace

CommonPaletteTool::CommonPaletteTool(Delegate* delegate)
    : PaletteTool(delegate) {}

CommonPaletteTool::~CommonPaletteTool() {}

views::View* CommonPaletteTool::CreateView() {
  // TODO(jdufault): Use real strings.
  return CreateDefaultView(
      base::ASCIIToUTF16("[TODO] " + PaletteToolIdToString(GetToolId())));
}

void CommonPaletteTool::OnViewDestroyed() {
  highlight_view_ = nullptr;
}

void CommonPaletteTool::OnEnable() {
  PaletteTool::OnEnable();
  start_time_ = base::TimeTicks::Now();

  if (highlight_view_) {
    highlight_view_->SetHighlight(true);
    highlight_view_->SetRightIconVisible(true);
  }
}

void CommonPaletteTool::OnDisable() {
  PaletteTool::OnDisable();
  AddHistogramTimes(GetToolId(), base::TimeTicks::Now() - start_time_);

  if (highlight_view_) {
    highlight_view_->SetHighlight(false);
    highlight_view_->SetRightIconVisible(false);
  }
}

void CommonPaletteTool::OnViewClicked(views::View* sender) {
  delegate()->RecordPaletteOptionsUsage(
      PaletteToolIdToPaletteTrayOptions(GetToolId()));
  if (enabled()) {
    delegate()->DisableTool(GetToolId());
    delegate()->RecordPaletteModeCancellation(
        PaletteToolIdToPaletteModeCancelType(GetToolId(),
                                             false /*is_switched*/));
  } else {
    delegate()->EnableTool(GetToolId());
  }
}

views::View* CommonPaletteTool::CreateDefaultView(const base::string16& name) {
  gfx::ImageSkia icon =
      CreateVectorIcon(GetPaletteIconId(), kIconSize, gfx::kChromeIconGrey);
  gfx::ImageSkia check = CreateVectorIcon(gfx::VectorIconId::CHECK_CIRCLE,
                                          kIconSize, gfx::kGoogleGreen700);

  highlight_view_ = new HoverHighlightView(this);
  highlight_view_->SetBorder(
      views::Border::CreateEmptyBorder(0, kExtraMarginFromLeftEdge, 0, 0));
  highlight_view_->AddIconAndLabelCustomSize(icon, name, false, kIconSize,
                                             kMarginFromEdges,
                                             kMarginBetweenIconAndText);
  highlight_view_->AddRightIcon(check, kIconSize);

  if (enabled())
    highlight_view_->SetHighlight(true);
  else
    highlight_view_->SetRightIconVisible(false);

  return highlight_view_;
}

}  // namespace ash
