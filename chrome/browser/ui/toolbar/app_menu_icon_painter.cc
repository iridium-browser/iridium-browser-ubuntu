// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_icon_painter.h"

#include <algorithm>

#include "grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// The app menu icon is made up of this many bars stacked vertically.
const int kBarCount = 3;

// |value| is the animation progress from 0 to 1. |index| is the index of the
// bar being drawn. This function returns a new progress value (from 0 to 1)
// such that bars appear staggered.
double GetStaggeredValue(double value, int index) {
  // When animating the app menu icon's bars the bars are staggered by this
  // factor.
  const double kStaggerFactor = 0.15;
  double maxStaggeredValue = 1.0 - (kBarCount - 1) * kStaggerFactor;
  double staggeredValue = (value - kStaggerFactor * index) / maxStaggeredValue;
  return std::min(1.0, std::max(0.0, staggeredValue));
}

}  // namespace

AppMenuIconPainter::AppMenuIconPainter(Delegate* delegate)
    : delegate_(delegate), severity_(SEVERITY_NONE) {}

AppMenuIconPainter::~AppMenuIconPainter() {}

void AppMenuIconPainter::SetSeverity(Severity severity, bool animate) {
  if (severity_ == severity)
    return;

  severity_ = severity;
  delegate_->ScheduleAppMenuIconPaint();
  animation_.reset();
  if (severity_ == SEVERITY_NONE || !animate)
    return;

  gfx::MultiAnimation::Parts parts;
  // Animate the bars left to right.
  parts.push_back(gfx::MultiAnimation::Part(1300, gfx::Tween::LINEAR));
  // Fade out animation.
  parts.push_back(gfx::MultiAnimation::Part(1000, gfx::Tween::EASE_IN));
  // Again, animate the bars left to right.
  parts.push_back(gfx::MultiAnimation::Part(1300, gfx::Tween::LINEAR));

  animation_.reset(
      new gfx::MultiAnimation(parts, base::TimeDelta::FromMilliseconds(40)));
  animation_->set_delegate(this);
  animation_->set_continuous(false);
  animation_->Start();
}

void AppMenuIconPainter::Paint(gfx::Canvas* canvas,
                               const ui::ThemeProvider* theme_provider,
                               const gfx::Rect& rect,
                               BezelType bezel_type) {
  gfx::Point center = rect.CenterPoint();

  // Bezel.
  int resource_id = 0;
  switch (bezel_type) {
    case BEZEL_NONE:
      break;
    case BEZEL_HOVER:
      resource_id = IDR_TOOLBAR_BEZEL_HOVER;
      break;
    case BEZEL_PRESSED:
      resource_id = IDR_TOOLBAR_BEZEL_PRESSED;
      break;
  }

  if (resource_id) {
    gfx::ImageSkia* image = theme_provider->GetImageSkiaNamed(resource_id);
    canvas->DrawImageInt(*image, center.x() - image->width() / 2,
                         center.y() - image->height() / 2);
  }

  // The bars with no color.
  {
    gfx::ImageSkia* image = theme_provider->GetImageSkiaNamed(IDR_TOOLS_BAR);
    int x = center.x() - image->width() / 2;
    int y = center.y() - image->height() * kBarCount / 2;
    for (int i = 0; i < kBarCount; ++i) {
      canvas->DrawImageInt(*image, x, y);
      y += image->height();
    }
  }

  // The bars with color based on severity.
  int severity_image_id = GetCurrentSeverityImageID();
  if (severity_image_id) {
    gfx::ImageSkia* image =
        theme_provider->GetImageSkiaNamed(severity_image_id);
    int x = center.x() - image->width() / 2;
    int y = center.y() - image->height() * kBarCount / 2;
    for (int i = 0; i < kBarCount; ++i) {
      SkPaint paint;
      int width = image->width();

      if (animation_ && animation_->is_animating()) {
        if (animation_->current_part_index() % 2 == 1) {
          // Fade out.
          int alpha = animation_->CurrentValueBetween(0xFF, 0);
          if (alpha == 0)
            continue;
          paint.setAlpha(alpha);
        } else {
          // Stagger the widths.
          width = image->width() *
                  GetStaggeredValue(animation_->GetCurrentValue(), i);
          if (width == 0)
            continue;
        }
      }

      canvas->DrawImageInt(*image, 0, 0, width, image->height(), x, y, width,
                           image->height(), false, paint);
      y += image->height();
    }
  }

  if (!badge_.isNull())
    canvas->DrawImageInt(badge_, 0, 0);
}

void AppMenuIconPainter::AnimationProgressed(const gfx::Animation* animation) {
  delegate_->ScheduleAppMenuIconPaint();
}

int AppMenuIconPainter::GetCurrentSeverityImageID() const {
  switch (severity_) {
    case SEVERITY_NONE:
      return 0;
    case SEVERITY_LOW:
      return IDR_TOOLS_BAR_LOW;
    case SEVERITY_MEDIUM:
      return IDR_TOOLS_BAR_MEDIUM;
    case SEVERITY_HIGH:
      return IDR_TOOLS_BAR_HIGH;
  }
  NOTREACHED();
  return 0;
}
