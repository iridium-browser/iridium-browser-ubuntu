/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008, 2009 Google Inc.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/layout/LayoutThemeDefault.h"

#include "core/CSSValueKeywords.h"
#include "core/layout/LayoutThemeFontProvider.h"
#include "core/paint/MediaControlsPainter.h"
#include "core/style/ComputedStyle.h"
#include "platform/HostWindow.h"
#include "platform/LayoutTestSupport.h"
#include "platform/PlatformResourceLoader.h"
#include "platform/graphics/Color.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThemeEngine.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

// These values all match Safari/Win.
static const float defaultControlFontPixelSize = 13;
static const float defaultCancelButtonSize = 9;
static const float minCancelButtonSize = 5;
static const float maxCancelButtonSize = 21;

static bool useMockTheme() {
  return LayoutTestSupport::isMockThemeEnabledForTest();
}

unsigned LayoutThemeDefault::m_activeSelectionBackgroundColor = 0xff1e90ff;
unsigned LayoutThemeDefault::m_activeSelectionForegroundColor = Color::black;
unsigned LayoutThemeDefault::m_inactiveSelectionBackgroundColor = 0xffc8c8c8;
unsigned LayoutThemeDefault::m_inactiveSelectionForegroundColor = 0xff323232;

double LayoutThemeDefault::m_caretBlinkInterval;

LayoutThemeDefault::LayoutThemeDefault()
    : LayoutTheme(nullptr), m_painter(*this) {
  m_caretBlinkInterval = LayoutTheme::caretBlinkInterval();
}

LayoutThemeDefault::~LayoutThemeDefault() {}

bool LayoutThemeDefault::themeDrawsFocusRing(const ComputedStyle& style) const {
  if (useMockTheme()) {
    // Don't use focus rings for buttons when mocking controls.
    return style.appearance() == ButtonPart ||
           style.appearance() == PushButtonPart ||
           style.appearance() == SquareButtonPart;
  }

  // This causes Blink to draw the focus rings for us.
  return false;
}

Color LayoutThemeDefault::systemColor(CSSValueID cssValueId) const {
  static const Color defaultButtonGrayColor(0xffdddddd);
  static const Color defaultMenuColor(0xfff7f7f7);

  if (cssValueId == CSSValueButtonface) {
    if (useMockTheme())
      return Color(0xc0, 0xc0, 0xc0);
    return defaultButtonGrayColor;
  }
  if (cssValueId == CSSValueMenu)
    return defaultMenuColor;
  return LayoutTheme::systemColor(cssValueId);
}

// Use the Windows style sheets to match their metrics.
String LayoutThemeDefault::extraDefaultStyleSheet() {
  String extraStyleSheet = LayoutTheme::extraDefaultStyleSheet();
  String multipleFieldsStyleSheet =
      RuntimeEnabledFeatures::inputMultipleFieldsUIEnabled()
          ? loadResourceAsASCIIString("themeInputMultipleFields.css")
          : String();
  String windowsStyleSheet = loadResourceAsASCIIString("themeWin.css");
  StringBuilder builder;
  builder.reserveCapacity(extraStyleSheet.length() +
                          multipleFieldsStyleSheet.length() +
                          windowsStyleSheet.length());
  builder.append(extraStyleSheet);
  builder.append(multipleFieldsStyleSheet);
  builder.append(windowsStyleSheet);
  return builder.toString();
}

String LayoutThemeDefault::extraQuirksStyleSheet() {
  return loadResourceAsASCIIString("themeWinQuirks.css");
}

Color LayoutThemeDefault::activeListBoxSelectionBackgroundColor() const {
  return Color(0x28, 0x28, 0x28);
}

Color LayoutThemeDefault::activeListBoxSelectionForegroundColor() const {
  return Color::black;
}

Color LayoutThemeDefault::inactiveListBoxSelectionBackgroundColor() const {
  return Color(0xc8, 0xc8, 0xc8);
}

Color LayoutThemeDefault::inactiveListBoxSelectionForegroundColor() const {
  return Color(0x32, 0x32, 0x32);
}

Color LayoutThemeDefault::platformActiveSelectionBackgroundColor() const {
  if (useMockTheme())
    return Color(0x00, 0x00, 0xff);  // Royal blue.
  return m_activeSelectionBackgroundColor;
}

Color LayoutThemeDefault::platformInactiveSelectionBackgroundColor() const {
  if (useMockTheme())
    return Color(0x99, 0x99, 0x99);  // Medium gray.
  return m_inactiveSelectionBackgroundColor;
}

Color LayoutThemeDefault::platformActiveSelectionForegroundColor() const {
  if (useMockTheme())
    return Color(0xff, 0xff, 0xcc);  // Pale yellow.
  return m_activeSelectionForegroundColor;
}

Color LayoutThemeDefault::platformInactiveSelectionForegroundColor() const {
  if (useMockTheme())
    return Color::white;
  return m_inactiveSelectionForegroundColor;
}

IntSize LayoutThemeDefault::sliderTickSize() const {
  if (useMockTheme())
    return IntSize(1, 3);
  return IntSize(1, 6);
}

int LayoutThemeDefault::sliderTickOffsetFromTrackCenter() const {
  if (useMockTheme())
    return 11;
  return -16;
}

void LayoutThemeDefault::adjustSliderThumbSize(ComputedStyle& style) const {
  IntSize size = Platform::current()->themeEngine()->getSize(
      WebThemeEngine::PartSliderThumb);

  // FIXME: Mock theme doesn't handle zoomed sliders.
  float zoomLevel = useMockTheme() ? 1 : style.effectiveZoom();
  if (style.appearance() == SliderThumbHorizontalPart) {
    style.setWidth(Length(size.width() * zoomLevel, Fixed));
    style.setHeight(Length(size.height() * zoomLevel, Fixed));
  } else if (style.appearance() == SliderThumbVerticalPart) {
    style.setWidth(Length(size.height() * zoomLevel, Fixed));
    style.setHeight(Length(size.width() * zoomLevel, Fixed));
  } else {
    MediaControlsPainter::adjustMediaSliderThumbSize(style);
  }
}

void LayoutThemeDefault::setSelectionColors(unsigned activeBackgroundColor,
                                            unsigned activeForegroundColor,
                                            unsigned inactiveBackgroundColor,
                                            unsigned inactiveForegroundColor) {
  m_activeSelectionBackgroundColor = activeBackgroundColor;
  m_activeSelectionForegroundColor = activeForegroundColor;
  m_inactiveSelectionBackgroundColor = inactiveBackgroundColor;
  m_inactiveSelectionForegroundColor = inactiveForegroundColor;
}

void LayoutThemeDefault::setCheckboxSize(ComputedStyle& style) const {
  // If the width and height are both specified, then we have nothing to do.
  if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
    return;

  IntSize size =
      Platform::current()->themeEngine()->getSize(WebThemeEngine::PartCheckbox);
  float zoomLevel = style.effectiveZoom();
  size.setWidth(size.width() * zoomLevel);
  size.setHeight(size.height() * zoomLevel);
  setSizeIfAuto(style, size);
}

void LayoutThemeDefault::setRadioSize(ComputedStyle& style) const {
  // If the width and height are both specified, then we have nothing to do.
  if (!style.width().isIntrinsicOrAuto() && !style.height().isAuto())
    return;

  IntSize size =
      Platform::current()->themeEngine()->getSize(WebThemeEngine::PartRadio);
  float zoomLevel = style.effectiveZoom();
  size.setWidth(size.width() * zoomLevel);
  size.setHeight(size.height() * zoomLevel);
  setSizeIfAuto(style, size);
}

void LayoutThemeDefault::adjustInnerSpinButtonStyle(
    ComputedStyle& style) const {
  IntSize size = Platform::current()->themeEngine()->getSize(
      WebThemeEngine::PartInnerSpinButton);

  float zoomLevel = style.effectiveZoom();
  style.setWidth(Length(size.width() * zoomLevel, Fixed));
  style.setMinWidth(Length(size.width() * zoomLevel, Fixed));
}

bool LayoutThemeDefault::shouldOpenPickerWithF4Key() const {
  return true;
}

bool LayoutThemeDefault::shouldUseFallbackTheme(
    const ComputedStyle& style) const {
  if (useMockTheme()) {
    // The mock theme can't handle zoomed controls, so we fall back to the
    // "fallback" theme.
    ControlPart part = style.appearance();
    if (part == CheckboxPart || part == RadioPart)
      return style.effectiveZoom() != 1;
  }
  return LayoutTheme::shouldUseFallbackTheme(style);
}

bool LayoutThemeDefault::supportsHover(const ComputedStyle& style) const {
  return true;
}

Color LayoutThemeDefault::platformFocusRingColor() const {
  static Color focusRingColor(229, 151, 0, 255);
  return focusRingColor;
}

void LayoutThemeDefault::systemFont(CSSValueID systemFontID,
                                    FontStyle& fontStyle,
                                    FontWeight& fontWeight,
                                    float& fontSize,
                                    AtomicString& fontFamily) const {
  LayoutThemeFontProvider::systemFont(systemFontID, fontStyle, fontWeight,
                                      fontSize, fontFamily);
}

int LayoutThemeDefault::minimumMenuListSize(const ComputedStyle& style) const {
  return 0;
}

// Return a rectangle that has the same center point as |original|, but with a
// size capped at |width| by |height|.
IntRect center(const IntRect& original, int width, int height) {
  width = std::min(original.width(), width);
  height = std::min(original.height(), height);
  int x = original.x() + (original.width() - width) / 2;
  int y = original.y() + (original.height() - height) / 2;

  return IntRect(x, y, width, height);
}

void LayoutThemeDefault::adjustButtonStyle(ComputedStyle& style) const {
  if (style.appearance() == PushButtonPart) {
    // Ignore line-height.
    style.setLineHeight(ComputedStyle::initialLineHeight());
  }
}

void LayoutThemeDefault::adjustSearchFieldStyle(ComputedStyle& style) const {
  // Ignore line-height.
  style.setLineHeight(ComputedStyle::initialLineHeight());
}

void LayoutThemeDefault::adjustSearchFieldCancelButtonStyle(
    ComputedStyle& style) const {
  // Scale the button size based on the font size
  float fontScale = style.fontSize() / defaultControlFontPixelSize;
  int cancelButtonSize = lroundf(std::min(
      std::max(minCancelButtonSize, defaultCancelButtonSize * fontScale),
      maxCancelButtonSize));
  style.setWidth(Length(cancelButtonSize, Fixed));
  style.setHeight(Length(cancelButtonSize, Fixed));
}

void LayoutThemeDefault::adjustMenuListStyle(ComputedStyle& style,
                                             Element*) const {
  // Height is locked to auto on all browsers.
  style.setLineHeight(ComputedStyle::initialLineHeight());
}

void LayoutThemeDefault::adjustMenuListButtonStyle(ComputedStyle& style,
                                                   Element* e) const {
  adjustMenuListStyle(style, e);
}

// The following internal paddings are in addition to the user-supplied padding.
// Matches the Firefox behavior.

int LayoutThemeDefault::popupInternalPaddingStart(
    const ComputedStyle& style) const {
  return menuListInternalPadding(style, 4);
}

int LayoutThemeDefault::popupInternalPaddingEnd(
    const HostWindow* host,
    const ComputedStyle& style) const {
  if (style.appearance() == NoControlPart)
    return 0;
  return 1 * style.effectiveZoom() +
         clampedMenuListArrowPaddingSize(host, style);
}

int LayoutThemeDefault::popupInternalPaddingTop(
    const ComputedStyle& style) const {
  return menuListInternalPadding(style, 1);
}

int LayoutThemeDefault::popupInternalPaddingBottom(
    const ComputedStyle& style) const {
  return menuListInternalPadding(style, 1);
}

int LayoutThemeDefault::menuListArrowWidthInDIP() const {
  int width = Platform::current()
                  ->themeEngine()
                  ->getSize(WebThemeEngine::PartScrollbarUpArrow)
                  .width;
  return width > 0 ? width : 15;
}

float LayoutThemeDefault::clampedMenuListArrowPaddingSize(
    const HostWindow* host,
    const ComputedStyle& style) const {
  if (m_cachedMenuListArrowPaddingSize > 0 &&
      style.effectiveZoom() == m_cachedMenuListArrowZoomLevel)
    return m_cachedMenuListArrowPaddingSize;
  m_cachedMenuListArrowZoomLevel = style.effectiveZoom();
  int originalSize = menuListArrowWidthInDIP();
  int scaledSize =
      host ? host->windowToViewportScalar(originalSize) : originalSize;
  // The result should not be samller than the scrollbar thickness in order to
  // secure space for scrollbar in popup.
  float deviceScale = 1.0f * scaledSize / originalSize;
  float size;
  if (m_cachedMenuListArrowZoomLevel < deviceScale) {
    size = scaledSize;
  } else {
    // The value should be zoomed though scrollbars aren't scaled by zoom.
    // crbug.com/432795.
    size = originalSize * m_cachedMenuListArrowZoomLevel;
  }
  m_cachedMenuListArrowPaddingSize = size;
  return size;
}

void LayoutThemeDefault::didChangeThemeEngine() {
  m_cachedMenuListArrowZoomLevel = 0;
  m_cachedMenuListArrowPaddingSize = 0;
}

// static
void LayoutThemeDefault::setDefaultFontSize(int fontSize) {
  LayoutThemeFontProvider::setDefaultFontSize(fontSize);
}

int LayoutThemeDefault::menuListInternalPadding(const ComputedStyle& style,
                                                int padding) const {
  if (style.appearance() == NoControlPart)
    return 0;
  return padding * style.effectiveZoom();
}

//
// Following values are come from default of GTK+
//
static const int progressAnimationFrames = 10;
static const double progressAnimationInterval = 0.125;

double LayoutThemeDefault::animationRepeatIntervalForProgressBar() const {
  return progressAnimationInterval;
}

double LayoutThemeDefault::animationDurationForProgressBar() const {
  return progressAnimationInterval * progressAnimationFrames *
         2;  // "2" for back and forth
}

}  // namespace blink
