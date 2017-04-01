// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
#define UI_NATIVE_THEME_NATIVE_THEME_BASE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "skia/ext/platform_canvas.h"
#include "ui/native_theme/native_theme.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {

// Theme support for non-Windows toolkits.
class NATIVE_THEME_EXPORT NativeThemeBase : public NativeTheme {
 public:
  // NativeTheme implementation:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  void Paint(SkCanvas* canvas,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra) const override;

 protected:
  NativeThemeBase();
  ~NativeThemeBase() override;

  // Draw the arrow. Used by scrollbar and inner spin button.
  virtual void PaintArrowButton(
      SkCanvas* gc,
      const gfx::Rect& rect,
      Part direction,
      State state) const;
  // Paint the scrollbar track. Done before the thumb so that it can contain
  // alpha.
  virtual void PaintScrollbarTrack(
      SkCanvas* canvas,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect) const;
  // Draw the scrollbar thumb over the track.
  virtual void PaintScrollbarThumb(
      SkCanvas* canvas,
      Part part,
      State state,
      const gfx::Rect& rect,
      NativeTheme::ScrollbarOverlayColorTheme theme) const;

  virtual void PaintScrollbarCorner(SkCanvas* canvas,
                                    State state,
                                    const gfx::Rect& rect) const;

  virtual void PaintCheckbox(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  virtual void PaintRadio(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  virtual void PaintButton(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  virtual void PaintTextField(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const TextFieldExtraParams& text) const;

  virtual void PaintMenuList(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list) const;

  virtual void PaintMenuPopupBackground(
      SkCanvas* canvas,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background) const;

  virtual void PaintMenuItemBackground(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuItemExtraParams& menu_item) const;

  virtual void PaintSliderTrack(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) const;

  virtual void PaintSliderThumb(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) const;

  virtual void PaintInnerSpinButton(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const InnerSpinButtonExtraParams& spin_button) const;

  virtual void PaintProgressBar(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ProgressBarExtraParams& progress_bar) const;

  // Shrinks checkbox/radio button rect, if necessary, to make room for padding
  // and drop shadow.
  // TODO(mohsen): This is needed because checkboxes/radio buttons on Android
  // have different padding from those on desktop Chrome. Get rid of this when
  // crbug.com/530746 is resolved.
  virtual void AdjustCheckboxRadioRectForPadding(SkRect* rect) const;

  void set_scrollbar_button_length(int length) {
    scrollbar_button_length_ = length;
  }
  int scrollbar_button_length() const { return scrollbar_button_length_; }

  SkColor SaturateAndBrighten(SkScalar* hsv,
                              SkScalar saturate_amount,
                              SkScalar brighten_amount) const;

  // Paints the arrow used on the scrollbar and spinner.
  void PaintArrow(SkCanvas* canvas,
                  const gfx::Rect& rect,
                  Part direction,
                  SkColor color) const;

  // Returns the color used to draw the arrow.
  SkColor GetArrowColor(State state) const;

  int scrollbar_width_;

 private:
  friend class NativeThemeAuraTest;

  SkPath PathForArrow(const gfx::Rect& rect, Part direction) const;
  gfx::Rect BoundingRectForArrow(const gfx::Rect& rect) const;

  void DrawVertLine(SkCanvas* canvas,
                    int x,
                    int y1,
                    int y2,
                    const SkPaint& paint) const;
  void DrawHorizLine(SkCanvas* canvas,
                     int x1,
                     int x2,
                     int y,
                     const SkPaint& paint) const;
  void DrawBox(SkCanvas* canvas,
               const gfx::Rect& rect,
               const SkPaint& paint) const;
  SkScalar Clamp(SkScalar value,
                 SkScalar min,
                 SkScalar max) const;
  SkColor OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const;

  // Paint the common parts of the checkboxes and radio buttons.
  // borderRadius specifies how rounded the corners should be.
  SkRect PaintCheckboxRadioCommon(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SkScalar borderRadius) const;

  // The length of the arrow buttons, 0 means no buttons are drawn.
  int scrollbar_button_length_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeBase);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
