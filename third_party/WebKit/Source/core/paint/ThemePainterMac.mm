/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
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
 */

#import "core/paint/ThemePainterMac.h"

#import "core/frame/FrameView.h"
#import "core/layout/LayoutProgress.h"
#import "core/layout/LayoutThemeMac.h"
#import "core/layout/LayoutView.h"
#import "core/paint/PaintInfo.h"
#import "platform/geometry/FloatRoundedRect.h"
#import "platform/graphics/BitmapImage.h"
#import "platform/graphics/GraphicsContextStateSaver.h"
#import "platform/graphics/Image.h"
#import "platform/graphics/ImageBuffer.h"
#import "platform/mac/BlockExceptions.h"
#import "platform/mac/ColorMac.h"
#import "platform/mac/LocalCurrentGraphicsContext.h"
#import "platform/mac/ThemeMac.h"
#import "platform/mac/WebCoreNSCellExtras.h"
#import <AvailabilityMacros.h>
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <math.h>

// The methods in this file are specific to the Mac OS X platform.

// Forward declare Mac SPIs.
extern "C" {
void _NSDrawCarbonThemeBezel(NSRect frame, BOOL enabled, BOOL flipped);
// Request for public API: rdar://13787640
void _NSDrawCarbonThemeListBox(NSRect frame,
                               BOOL enabled,
                               BOOL flipped,
                               BOOL always_yes);
}

namespace blink {

ThemePainterMac::ThemePainterMac(LayoutThemeMac& layoutTheme)
    : ThemePainter(), m_layoutTheme(layoutTheme) {}

bool ThemePainterMac::paintTextField(const LayoutObject& o,
                                     const PaintInfo& paintInfo,
                                     const IntRect& r) {
  LocalCurrentGraphicsContext localContext(paintInfo.context, r);

  bool useNSTextFieldCell = o.styleRef().hasAppearance() &&
                            o.styleRef().visitedDependentColor(
                                CSSPropertyBackgroundColor) == Color::white &&
                            !o.styleRef().hasBackgroundImage();

  // We do not use NSTextFieldCell to draw styled text fields since it induces a
  // behavior change while remaining a fragile solution.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=658085#c3
  if (!useNSTextFieldCell) {
    _NSDrawCarbonThemeBezel(
        r, LayoutTheme::isEnabled(o) && !LayoutTheme::isReadOnlyControl(o),
        YES);
    return false;
  }

  NSTextFieldCell* textField = m_layoutTheme.textField();

  GraphicsContextStateSaver stateSaver(paintInfo.context);

  [textField setEnabled:(LayoutTheme::isEnabled(o) &&
                         !LayoutTheme::isReadOnlyControl(o))];
  [textField drawWithFrame:NSRect(r) inView:m_layoutTheme.documentViewFor(o)];

  [textField setControlView:nil];

  return false;
}

bool ThemePainterMac::paintCapsLockIndicator(const LayoutObject& o,
                                             const PaintInfo& paintInfo,
                                             const IntRect& r) {
  // This draws the caps lock indicator as it was done by
  // WKDrawCapsLockIndicator.
  LocalCurrentGraphicsContext localContext(paintInfo.context, r);
  CGContextRef c = localContext.cgContext();
  CGMutablePathRef shape = CGPathCreateMutable();

  // To draw the caps lock indicator, draw the shape into a small
  // square that is then scaled to the size of r.
  const CGFloat kSquareSize = 17;

  // Create a rounted square shape.
  CGPathMoveToPoint(shape, NULL, 16.5, 4.5);
  CGPathAddArc(shape, NULL, 12.5, 12.5, 4, 0, M_PI_2, false);
  CGPathAddArc(shape, NULL, 4.5, 12.5, 4, M_PI_2, M_PI, false);
  CGPathAddArc(shape, NULL, 4.5, 4.5, 4, M_PI, 3 * M_PI / 2, false);
  CGPathAddArc(shape, NULL, 12.5, 4.5, 4, 3 * M_PI / 2, 0, false);

  // Draw the arrow - note this is drawing in a flipped coordinate system, so
  // the arrow is pointing down.
  CGPathMoveToPoint(shape, NULL, 8.5, 2);  // Tip point.
  CGPathAddLineToPoint(shape, NULL, 4, 7);
  CGPathAddLineToPoint(shape, NULL, 6.25, 7);
  CGPathAddLineToPoint(shape, NULL, 6.25, 10.25);
  CGPathAddLineToPoint(shape, NULL, 10.75, 10.25);
  CGPathAddLineToPoint(shape, NULL, 10.75, 7);
  CGPathAddLineToPoint(shape, NULL, 13, 7);
  CGPathAddLineToPoint(shape, NULL, 8.5, 2);

  // Draw the rectangle that underneath (or above in the flipped system) the
  // arrow.
  CGPathAddLineToPoint(shape, NULL, 10.75, 12);
  CGPathAddLineToPoint(shape, NULL, 6.25, 12);
  CGPathAddLineToPoint(shape, NULL, 6.25, 14.25);
  CGPathAddLineToPoint(shape, NULL, 10.75, 14.25);
  CGPathAddLineToPoint(shape, NULL, 10.75, 12);

  // Scale and translate the shape.
  CGRect cgr = r;
  CGFloat maxX = CGRectGetMaxX(cgr);
  CGFloat minX = CGRectGetMinX(cgr);
  CGFloat minY = CGRectGetMinY(cgr);
  CGFloat heightScale = r.height() / kSquareSize;
  const bool isRTL = o.styleRef().direction() == TextDirection::kRtl;
  CGAffineTransform transform =
      CGAffineTransformMake(heightScale, 0,                           // A  B
                            0, heightScale,                           // C  D
                            isRTL ? minX : maxX - r.height(), minY);  // Tx Ty

  CGMutablePathRef paintPath = CGPathCreateMutable();
  CGPathAddPath(paintPath, &transform, shape);
  CGPathRelease(shape);

  CGContextSetRGBFillColor(c, 0, 0, 0, 0.4);
  CGContextBeginPath(c);
  CGContextAddPath(c, paintPath);
  CGContextFillPath(c);
  CGPathRelease(paintPath);

  return false;
}

bool ThemePainterMac::paintTextArea(const LayoutObject& o,
                                    const PaintInfo& paintInfo,
                                    const IntRect& r) {
  LocalCurrentGraphicsContext localContext(paintInfo.context, r);
  _NSDrawCarbonThemeListBox(
      r, LayoutTheme::isEnabled(o) && !LayoutTheme::isReadOnlyControl(o), YES,
      YES);
  return false;
}

bool ThemePainterMac::paintMenuList(const LayoutObject& o,
                                    const PaintInfo& paintInfo,
                                    const IntRect& r) {
  m_layoutTheme.setPopupButtonCellState(o, r);

  NSPopUpButtonCell* popupButton = m_layoutTheme.popupButton();

  float zoomLevel = o.styleRef().effectiveZoom();
  IntSize size = m_layoutTheme.popupButtonSizes()[[popupButton controlSize]];
  size.setHeight(size.height() * zoomLevel);
  size.setWidth(r.width());

  // Now inflate it to account for the shadow.
  IntRect inflatedRect = r;
  if (r.width() >= m_layoutTheme.minimumMenuListSize(o.styleRef()))
    inflatedRect = ThemeMac::inflateRect(
        inflatedRect, size, m_layoutTheme.popupButtonMargins(), zoomLevel);

  LocalCurrentGraphicsContext localContext(
      paintInfo.context, ThemeMac::inflateRectForFocusRing(inflatedRect));

  if (zoomLevel != 1.0f) {
    inflatedRect.setWidth(inflatedRect.width() / zoomLevel);
    inflatedRect.setHeight(inflatedRect.height() / zoomLevel);
    paintInfo.context.translate(inflatedRect.x(), inflatedRect.y());
    paintInfo.context.scale(zoomLevel, zoomLevel);
    paintInfo.context.translate(-inflatedRect.x(), -inflatedRect.y());
  }

  NSView* view = m_layoutTheme.documentViewFor(o);
  [popupButton drawWithFrame:inflatedRect inView:view];
  if (LayoutTheme::isFocused(o) && o.styleRef().outlineStyleIsAuto())
    [popupButton cr_drawFocusRingWithFrame:inflatedRect inView:view];
  [popupButton setControlView:nil];

  return false;
}

bool ThemePainterMac::paintProgressBar(const LayoutObject& layoutObject,
                                       const PaintInfo& paintInfo,
                                       const IntRect& rect) {
  if (!layoutObject.isProgress())
    return true;

  const LayoutProgress& layoutProgress = toLayoutProgress(layoutObject);
  HIThemeTrackDrawInfo trackInfo;
  trackInfo.version = 0;
  NSControlSize controlSize =
      m_layoutTheme.controlSizeForFont(layoutObject.styleRef());
  if (controlSize == NSRegularControlSize)
    trackInfo.kind = layoutProgress.position() < 0 ? kThemeLargeIndeterminateBar
                                                   : kThemeLargeProgressBar;
  else
    trackInfo.kind = layoutProgress.position() < 0
                         ? kThemeMediumIndeterminateBar
                         : kThemeMediumProgressBar;

  trackInfo.bounds = IntRect(IntPoint(), rect.size());
  trackInfo.min = 0;
  trackInfo.max = std::numeric_limits<SInt32>::max();
  trackInfo.value =
      lround(layoutProgress.position() * nextafter(trackInfo.max, 0));
  trackInfo.trackInfo.progress.phase =
      lround(layoutProgress.animationProgress() *
             nextafter(LayoutThemeMac::progressAnimationNumFrames, 0));
  trackInfo.attributes = kThemeTrackHorizontal;
  trackInfo.enableState = LayoutTheme::isActive(layoutObject)
                              ? kThemeTrackActive
                              : kThemeTrackInactive;
  trackInfo.reserved = 0;
  trackInfo.filler1 = 0;

  std::unique_ptr<ImageBuffer> imageBuffer = ImageBuffer::create(rect.size());
  if (!imageBuffer)
    return true;

  IntRect clipRect = IntRect(IntPoint(), rect.size());
  LocalCurrentGraphicsContext localContext(imageBuffer->canvas(), 1, clipRect);
  CGContextRef cgContext = localContext.cgContext();
  HIThemeDrawTrack(&trackInfo, 0, cgContext, kHIThemeOrientationNormal);

  GraphicsContextStateSaver stateSaver(paintInfo.context);

  if (!layoutProgress.styleRef().isLeftToRightDirection()) {
    paintInfo.context.translate(2 * rect.x() + rect.width(), 0);
    paintInfo.context.scale(-1, 1);
  }

  if (!paintInfo.context.contextDisabled())
    imageBuffer->draw(
        paintInfo.context,
        FloatRect(rect.location(), FloatSize(imageBuffer->size())), nullptr,
        SkBlendMode::kSrcOver);
  return false;
}

bool ThemePainterMac::paintMenuListButton(const LayoutObject& o,
                                          const PaintInfo& paintInfo,
                                          const IntRect& r) {
  IntRect bounds = IntRect(r.x() + o.styleRef().borderLeftWidth(),
                           r.y() + o.styleRef().borderTopWidth(),
                           r.width() - o.styleRef().borderLeftWidth() -
                               o.styleRef().borderRightWidth(),
                           r.height() - o.styleRef().borderTopWidth() -
                               o.styleRef().borderBottomWidth());
  // Since we actually know the size of the control here, we restrict the font
  // scale to make sure the arrows will fit vertically in the bounds
  float fontScale = std::min(
      o.styleRef().fontSize() / LayoutThemeMac::baseFontSize,
      bounds.height() / (LayoutThemeMac::menuListBaseArrowHeight * 2 +
                         LayoutThemeMac::menuListBaseSpaceBetweenArrows));
  float centerY = bounds.y() + bounds.height() / 2.0f;
  float arrowHeight = LayoutThemeMac::menuListBaseArrowHeight * fontScale;
  float arrowWidth = LayoutThemeMac::menuListBaseArrowWidth * fontScale;
  float spaceBetweenArrows =
      LayoutThemeMac::menuListBaseSpaceBetweenArrows * fontScale;
  float scaledPaddingEnd =
      LayoutThemeMac::menuListArrowPaddingEnd * o.styleRef().effectiveZoom();
  float leftEdge;
  if (o.styleRef().direction() == TextDirection::kLtr) {
    leftEdge = bounds.maxX() - scaledPaddingEnd - arrowWidth;
  } else {
    leftEdge = bounds.x() + scaledPaddingEnd;
  }
  if (bounds.width() < arrowWidth + scaledPaddingEnd)
    return false;

  Color color = o.styleRef().visitedDependentColor(CSSPropertyColor);
  SkPaint paint = paintInfo.context.fillPaint();
  paint.setAntiAlias(true);
  paint.setColor(color.rgb());

  SkPath arrow1;
  arrow1.moveTo(leftEdge, centerY - spaceBetweenArrows / 2.0f);
  arrow1.lineTo(leftEdge + arrowWidth, centerY - spaceBetweenArrows / 2.0f);
  arrow1.lineTo(leftEdge + arrowWidth / 2.0f,
                centerY - spaceBetweenArrows / 2.0f - arrowHeight);

  // Draw the top arrow.
  paintInfo.context.drawPath(arrow1, paint);

  SkPath arrow2;
  arrow2.moveTo(leftEdge, centerY + spaceBetweenArrows / 2.0f);
  arrow2.lineTo(leftEdge + arrowWidth, centerY + spaceBetweenArrows / 2.0f);
  arrow2.lineTo(leftEdge + arrowWidth / 2.0f,
                centerY + spaceBetweenArrows / 2.0f + arrowHeight);

  // Draw the bottom arrow.
  paintInfo.context.drawPath(arrow2, paint);
  return false;
}

bool ThemePainterMac::paintSliderTrack(const LayoutObject& o,
                                       const PaintInfo& paintInfo,
                                       const IntRect& r) {
  paintSliderTicks(o, paintInfo, r);

  float zoomLevel = o.styleRef().effectiveZoom();
  FloatRect unzoomedRect = r;

  if (o.styleRef().appearance() == SliderHorizontalPart ||
      o.styleRef().appearance() == MediaSliderPart) {
    unzoomedRect.setY(ceilf(unzoomedRect.y() + unzoomedRect.height() / 2 -
                            zoomLevel * LayoutThemeMac::sliderTrackWidth / 2));
    unzoomedRect.setHeight(zoomLevel * LayoutThemeMac::sliderTrackWidth);
  } else if (o.styleRef().appearance() == SliderVerticalPart) {
    unzoomedRect.setX(ceilf(unzoomedRect.x() + unzoomedRect.width() / 2 -
                            zoomLevel * LayoutThemeMac::sliderTrackWidth / 2));
    unzoomedRect.setWidth(zoomLevel * LayoutThemeMac::sliderTrackWidth);
  }

  if (zoomLevel != 1) {
    unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
    unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
  }

  GraphicsContextStateSaver stateSaver(paintInfo.context);
  if (zoomLevel != 1) {
    paintInfo.context.translate(unzoomedRect.x(), unzoomedRect.y());
    paintInfo.context.scale(zoomLevel, zoomLevel);
    paintInfo.context.translate(-unzoomedRect.x(), -unzoomedRect.y());
  }

  Color fillColor(205, 205, 205);
  Color borderGradientTopColor(109, 109, 109);
  Color borderGradientBottomColor(181, 181, 181);
  Color shadowColor(0, 0, 0, 118);

  if (!LayoutTheme::isEnabled(o)) {
    Color tintColor(255, 255, 255, 128);
    fillColor = fillColor.blend(tintColor);
    borderGradientTopColor = borderGradientTopColor.blend(tintColor);
    borderGradientBottomColor = borderGradientBottomColor.blend(tintColor);
    shadowColor = shadowColor.blend(tintColor);
  }

  Color tintColor;
  if (!LayoutTheme::isEnabled(o))
    tintColor = Color(255, 255, 255, 128);

  bool isVerticalSlider = o.styleRef().appearance() == SliderVerticalPart;

  float fillRadiusSize = (LayoutThemeMac::sliderTrackWidth -
                          LayoutThemeMac::sliderTrackBorderWidth) /
                         2;
  FloatSize fillRadius(fillRadiusSize, fillRadiusSize);
  FloatRect fillBounds(enclosedIntRect(unzoomedRect));
  FloatRoundedRect fillRect(fillBounds, fillRadius, fillRadius, fillRadius,
                            fillRadius);
  paintInfo.context.fillRoundedRect(fillRect, fillColor);

  FloatSize shadowOffset(isVerticalSlider ? 1 : 0, isVerticalSlider ? 0 : 1);
  float shadowBlur = 3;
  float shadowSpread = 0;
  paintInfo.context.save();
  paintInfo.context.drawInnerShadow(fillRect, shadowColor, shadowOffset,
                                    shadowBlur, shadowSpread);
  paintInfo.context.restore();

  RefPtr<Gradient> borderGradient =
      Gradient::create(fillBounds.minXMinYCorner(),
                       isVerticalSlider ? fillBounds.maxXMinYCorner()
                                        : fillBounds.minXMaxYCorner());
  borderGradient->addColorStop(0.0, borderGradientTopColor);
  borderGradient->addColorStop(1.0, borderGradientBottomColor);

  FloatRect borderRect(unzoomedRect);
  borderRect.inflate(-LayoutThemeMac::sliderTrackBorderWidth / 2.0);
  float borderRadiusSize =
      (isVerticalSlider ? borderRect.width() : borderRect.height()) / 2;
  FloatSize borderRadius(borderRadiusSize, borderRadiusSize);
  FloatRoundedRect borderRRect(borderRect, borderRadius, borderRadius,
                               borderRadius, borderRadius);
  paintInfo.context.setStrokeThickness(LayoutThemeMac::sliderTrackBorderWidth);
  SkPaint borderPaint(paintInfo.context.strokePaint());
  borderGradient->applyToPaint(borderPaint, SkMatrix::I());
  paintInfo.context.drawRRect(borderRRect, borderPaint);

  return false;
}

bool ThemePainterMac::paintSliderThumb(const LayoutObject& o,
                                       const PaintInfo& paintInfo,
                                       const IntRect& r) {
  GraphicsContextStateSaver stateSaver(paintInfo.context);
  float zoomLevel = o.styleRef().effectiveZoom();

  FloatRect unzoomedRect(r.x(), r.y(), LayoutThemeMac::sliderThumbWidth,
                         LayoutThemeMac::sliderThumbHeight);
  if (zoomLevel != 1.0f) {
    paintInfo.context.translate(unzoomedRect.x(), unzoomedRect.y());
    paintInfo.context.scale(zoomLevel, zoomLevel);
    paintInfo.context.translate(-unzoomedRect.x(), -unzoomedRect.y());
  }

  Color fillGradientTopColor(250, 250, 250);
  Color fillGradientUpperMiddleColor(244, 244, 244);
  Color fillGradientLowerMiddleColor(236, 236, 236);
  Color fillGradientBottomColor(238, 238, 238);
  Color borderGradientTopColor(151, 151, 151);
  Color borderGradientBottomColor(128, 128, 128);
  Color shadowColor(0, 0, 0, 36);

  if (!LayoutTheme::isEnabled(o)) {
    Color tintColor(255, 255, 255, 128);
    fillGradientTopColor = fillGradientTopColor.blend(tintColor);
    fillGradientUpperMiddleColor =
        fillGradientUpperMiddleColor.blend(tintColor);
    fillGradientLowerMiddleColor =
        fillGradientLowerMiddleColor.blend(tintColor);
    fillGradientBottomColor = fillGradientBottomColor.blend(tintColor);
    borderGradientTopColor = borderGradientTopColor.blend(tintColor);
    borderGradientBottomColor = borderGradientBottomColor.blend(tintColor);
    shadowColor = shadowColor.blend(tintColor);
  } else if (LayoutTheme::isPressed(o)) {
    Color tintColor(0, 0, 0, 32);
    fillGradientTopColor = fillGradientTopColor.blend(tintColor);
    fillGradientUpperMiddleColor =
        fillGradientUpperMiddleColor.blend(tintColor);
    fillGradientLowerMiddleColor =
        fillGradientLowerMiddleColor.blend(tintColor);
    fillGradientBottomColor = fillGradientBottomColor.blend(tintColor);
    borderGradientTopColor = borderGradientTopColor.blend(tintColor);
    borderGradientBottomColor = borderGradientBottomColor.blend(tintColor);
    shadowColor = shadowColor.blend(tintColor);
  }

  FloatRect borderBounds = unzoomedRect;
  borderBounds.inflate(LayoutThemeMac::sliderThumbBorderWidth / 2.0);

  borderBounds.inflate(-LayoutThemeMac::sliderThumbBorderWidth);
  FloatSize shadowOffset(0, 1);
  paintInfo.context.setShadow(
      shadowOffset, LayoutThemeMac::sliderThumbShadowBlur, shadowColor);
  paintInfo.context.setFillColor(Color::black);
  paintInfo.context.fillEllipse(borderBounds);
  paintInfo.context.setDrawLooper(nullptr);

  IntRect fillBounds = enclosedIntRect(unzoomedRect);
  RefPtr<Gradient> fillGradient = Gradient::create(fillBounds.minXMinYCorner(),
                                                   fillBounds.minXMaxYCorner());
  fillGradient->addColorStop(0.0, fillGradientTopColor);
  fillGradient->addColorStop(0.52, fillGradientUpperMiddleColor);
  fillGradient->addColorStop(0.52, fillGradientLowerMiddleColor);
  fillGradient->addColorStop(1.0, fillGradientBottomColor);
  SkPaint fillPaint(paintInfo.context.fillPaint());
  fillGradient->applyToPaint(fillPaint, SkMatrix::I());
  paintInfo.context.drawOval(borderBounds, fillPaint);

  RefPtr<Gradient> borderGradient = Gradient::create(
      fillBounds.minXMinYCorner(), fillBounds.minXMaxYCorner());
  borderGradient->addColorStop(0.0, borderGradientTopColor);
  borderGradient->addColorStop(1.0, borderGradientBottomColor);
  paintInfo.context.setStrokeThickness(LayoutThemeMac::sliderThumbBorderWidth);
  SkPaint borderPaint(paintInfo.context.strokePaint());
  borderGradient->applyToPaint(borderPaint, SkMatrix::I());
  paintInfo.context.drawOval(borderBounds, borderPaint);

  if (LayoutTheme::isFocused(o)) {
    Path borderPath;
    borderPath.addEllipse(borderBounds);
    paintInfo.context.drawFocusRing(borderPath, 5, -2,
                                    m_layoutTheme.focusRingColor());
  }

  return false;
}

// We don't use controlSizeForFont() for search field decorations because it
// needs to fit into the search field. The font size will already be modified by
// setFontFromControlSize() called on the search field.
static NSControlSize searchFieldControlSizeForFont(const ComputedStyle& style) {
  int fontSize = style.fontSize();
  if (fontSize >= 13)
    return NSRegularControlSize;
  if (fontSize >= 11)
    return NSSmallControlSize;
  return NSMiniControlSize;
}

bool ThemePainterMac::paintSearchField(const LayoutObject& o,
                                       const PaintInfo& paintInfo,
                                       const IntRect& r) {
  LocalCurrentGraphicsContext localContext(paintInfo.context, r);

  NSSearchFieldCell* search = m_layoutTheme.search();
  m_layoutTheme.setSearchCellState(o, r);
  [search setControlSize:searchFieldControlSizeForFont(o.styleRef())];

  GraphicsContextStateSaver stateSaver(paintInfo.context);

  float zoomLevel = o.styleRef().effectiveZoom();

  IntRect unzoomedRect = r;

  if (zoomLevel != 1.0f) {
    unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
    unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
    paintInfo.context.translate(unzoomedRect.x(), unzoomedRect.y());
    paintInfo.context.scale(zoomLevel, zoomLevel);
    paintInfo.context.translate(-unzoomedRect.x(), -unzoomedRect.y());
  }

  // Set the search button to nil before drawing. Then reset it so we can
  // draw it later.
  [search setSearchButtonCell:nil];

  [search drawWithFrame:NSRect(unzoomedRect)
                 inView:m_layoutTheme.documentViewFor(o)];

  [search setControlView:nil];
  [search resetSearchButtonCell];

  return false;
}

bool ThemePainterMac::paintSearchFieldCancelButton(const LayoutObject& o,
                                                   const PaintInfo& paintInfo,
                                                   const IntRect& r) {
  if (!o.node())
    return false;
  Element* input = o.node()->ownerShadowHost();
  if (!input)
    input = toElement(o.node());

  if (!input->layoutObject()->isBox())
    return false;

  GraphicsContextStateSaver stateSaver(paintInfo.context);

  float zoomLevel = o.styleRef().effectiveZoom();
  FloatRect unzoomedRect(r);
  if (zoomLevel != 1.0f) {
    unzoomedRect.setWidth(unzoomedRect.width() / zoomLevel);
    unzoomedRect.setHeight(unzoomedRect.height() / zoomLevel);
    paintInfo.context.translate(unzoomedRect.x(), unzoomedRect.y());
    paintInfo.context.scale(zoomLevel, zoomLevel);
    paintInfo.context.translate(-unzoomedRect.x(), -unzoomedRect.y());
  }

  Color fillColor(200, 200, 200);

  if (LayoutTheme::isPressed(o)) {
    Color tintColor(0, 0, 0, 32);
    fillColor = fillColor.blend(tintColor);
  }

  float centerX = unzoomedRect.x() + unzoomedRect.width() / 2;
  float centerY = unzoomedRect.y() + unzoomedRect.height() / 2;
  // The line width is 3px on a regular sized, high DPI NSCancelButtonCell
  // (which is 28px wide).
  float lineWidth = unzoomedRect.width() * 3 / 28;
  // The line length is 16px on a regular sized, high DPI NSCancelButtonCell.
  float lineLength = unzoomedRect.width() * 16 / 28;

  Path xPath;
  FloatSize lineRectRadius(lineWidth / 2, lineWidth / 2);
  xPath.addRoundedRect(
      FloatRect(-lineLength / 2, -lineWidth / 2, lineLength, lineWidth),
      lineRectRadius, lineRectRadius, lineRectRadius, lineRectRadius);
  xPath.addRoundedRect(
      FloatRect(-lineWidth / 2, -lineLength / 2, lineWidth, lineLength),
      lineRectRadius, lineRectRadius, lineRectRadius, lineRectRadius);

  paintInfo.context.translate(centerX, centerY);
  paintInfo.context.rotate(deg2rad(45.0));
  paintInfo.context.clipOut(xPath);
  paintInfo.context.rotate(deg2rad(-45.0));
  paintInfo.context.translate(-centerX, -centerY);

  paintInfo.context.setFillColor(fillColor);
  paintInfo.context.fillEllipse(unzoomedRect);

  return false;
}

// FIXME: Share more code with radio buttons.
bool ThemePainterMac::paintCheckbox(const LayoutObject& object,
                                    const PaintInfo& paintInfo,
                                    const IntRect& zoomedRect) {
  BEGIN_BLOCK_OBJC_EXCEPTIONS

  ControlStates states = LayoutTheme::controlStatesForLayoutObject(object);
  float zoomFactor = object.styleRef().effectiveZoom();

  // Determine the width and height needed for the control and prepare the cell
  // for painting.
  NSButtonCell* checkboxCell =
      ThemeMac::checkbox(states, zoomedRect, zoomFactor);
  GraphicsContextStateSaver stateSaver(paintInfo.context);

  NSControlSize controlSize = [checkboxCell controlSize];
  IntSize zoomedSize = ThemeMac::checkboxSizes()[controlSize];
  zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
  zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
  IntRect inflatedRect =
      ThemeMac::inflateRect(zoomedRect, zoomedSize,
                            ThemeMac::checkboxMargins(controlSize), zoomFactor);

  if (zoomFactor != 1.0f) {
    inflatedRect.setWidth(inflatedRect.width() / zoomFactor);
    inflatedRect.setHeight(inflatedRect.height() / zoomFactor);
    paintInfo.context.translate(inflatedRect.x(), inflatedRect.y());
    paintInfo.context.scale(zoomFactor, zoomFactor);
    paintInfo.context.translate(-inflatedRect.x(), -inflatedRect.y());
  }

  LocalCurrentGraphicsContext localContext(
      paintInfo.context, ThemeMac::inflateRectForFocusRing(inflatedRect));
  NSView* view = ThemeMac::ensuredView(object.view()->frameView());
  [checkboxCell drawWithFrame:NSRect(inflatedRect) inView:view];
  if (states & FocusControlState)
    [checkboxCell cr_drawFocusRingWithFrame:NSRect(inflatedRect) inView:view];
  [checkboxCell setControlView:nil];

  END_BLOCK_OBJC_EXCEPTIONS
  return false;
}

bool ThemePainterMac::paintRadio(const LayoutObject& object,
                                 const PaintInfo& paintInfo,
                                 const IntRect& zoomedRect) {
  ControlStates states = LayoutTheme::controlStatesForLayoutObject(object);
  float zoomFactor = object.styleRef().effectiveZoom();

  // Determine the width and height needed for the control and prepare the cell
  // for painting.
  NSButtonCell* radioCell = ThemeMac::radio(states, zoomedRect, zoomFactor);
  GraphicsContextStateSaver stateSaver(paintInfo.context);

  NSControlSize controlSize = [radioCell controlSize];
  IntSize zoomedSize = ThemeMac::radioSizes()[controlSize];
  zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
  zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
  IntRect inflatedRect = ThemeMac::inflateRect(
      zoomedRect, zoomedSize, ThemeMac::radioMargins(controlSize), zoomFactor);

  if (zoomFactor != 1.0f) {
    inflatedRect.setWidth(inflatedRect.width() / zoomFactor);
    inflatedRect.setHeight(inflatedRect.height() / zoomFactor);
    paintInfo.context.translate(inflatedRect.x(), inflatedRect.y());
    paintInfo.context.scale(zoomFactor, zoomFactor);
    paintInfo.context.translate(-inflatedRect.x(), -inflatedRect.y());
  }

  LocalCurrentGraphicsContext localContext(
      paintInfo.context, ThemeMac::inflateRectForFocusRing(inflatedRect));
  BEGIN_BLOCK_OBJC_EXCEPTIONS
  NSView* view = ThemeMac::ensuredView(object.view()->frameView());
  [radioCell drawWithFrame:NSRect(inflatedRect) inView:view];
  if (states & FocusControlState)
    [radioCell cr_drawFocusRingWithFrame:NSRect(inflatedRect) inView:view];
  [radioCell setControlView:nil];
  END_BLOCK_OBJC_EXCEPTIONS

  return false;
}

bool ThemePainterMac::paintButton(const LayoutObject& object,
                                  const PaintInfo& paintInfo,
                                  const IntRect& zoomedRect) {
  BEGIN_BLOCK_OBJC_EXCEPTIONS

  ControlStates states = LayoutTheme::controlStatesForLayoutObject(object);
  float zoomFactor = object.styleRef().effectiveZoom();

  // Determine the width and height needed for the control and prepare the cell
  // for painting.
  NSButtonCell* buttonCell = ThemeMac::button(object.styleRef().appearance(),
                                              states, zoomedRect, zoomFactor);
  GraphicsContextStateSaver stateSaver(paintInfo.context);

  NSControlSize controlSize = [buttonCell controlSize];
  IntSize zoomedSize = ThemeMac::buttonSizes()[controlSize];
  // Buttons don't ever constrain width, so the zoomed width can just be
  // honored.
  zoomedSize.setWidth(zoomedRect.width());
  zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
  IntRect inflatedRect = zoomedRect;
  if ([buttonCell bezelStyle] == NSRoundedBezelStyle) {
    // Center the button within the available space.
    if (inflatedRect.height() > zoomedSize.height()) {
      inflatedRect.setY(inflatedRect.y() +
                        (inflatedRect.height() - zoomedSize.height()) / 2);
      inflatedRect.setHeight(zoomedSize.height());
    }

    // Now inflate it to account for the shadow.
    inflatedRect =
        ThemeMac::inflateRect(inflatedRect, zoomedSize,
                              ThemeMac::buttonMargins(controlSize), zoomFactor);

    if (zoomFactor != 1.0f) {
      inflatedRect.setWidth(inflatedRect.width() / zoomFactor);
      inflatedRect.setHeight(inflatedRect.height() / zoomFactor);
      paintInfo.context.translate(inflatedRect.x(), inflatedRect.y());
      paintInfo.context.scale(zoomFactor, zoomFactor);
      paintInfo.context.translate(-inflatedRect.x(), -inflatedRect.y());
    }
  }

  LocalCurrentGraphicsContext localContext(
      paintInfo.context, ThemeMac::inflateRectForFocusRing(inflatedRect));
  NSView* view = ThemeMac::ensuredView(object.view()->frameView());

  [buttonCell drawWithFrame:NSRect(inflatedRect) inView:view];
  if (states & FocusControlState)
    [buttonCell cr_drawFocusRingWithFrame:NSRect(inflatedRect) inView:view];
  [buttonCell setControlView:nil];

  END_BLOCK_OBJC_EXCEPTIONS
  return false;
}

static ThemeDrawState convertControlStatesToThemeDrawState(
    ThemeButtonKind kind,
    ControlStates states) {
  if (states & ReadOnlyControlState)
    return kThemeStateUnavailableInactive;
  if (!(states & EnabledControlState))
    return kThemeStateUnavailableInactive;

  // Do not process PressedState if !EnabledControlState or
  // ReadOnlyControlState.
  if (states & PressedControlState) {
    if (kind == kThemeIncDecButton || kind == kThemeIncDecButtonSmall ||
        kind == kThemeIncDecButtonMini)
      return states & SpinUpControlState ? kThemeStatePressedUp
                                         : kThemeStatePressedDown;
    return kThemeStatePressed;
  }
  return kThemeStateActive;
}

bool ThemePainterMac::paintInnerSpinButton(const LayoutObject& object,
                                           const PaintInfo& paintInfo,
                                           const IntRect& zoomedRect) {
  ControlStates states = LayoutTheme::controlStatesForLayoutObject(object);
  float zoomFactor = object.styleRef().effectiveZoom();

  // We don't use NSStepperCell because there are no ways to draw an
  // NSStepperCell with the up button highlighted.

  HIThemeButtonDrawInfo drawInfo;
  drawInfo.version = 0;
  drawInfo.state =
      convertControlStatesToThemeDrawState(kThemeIncDecButton, states);
  drawInfo.adornment = kThemeAdornmentDefault;
  ControlSize controlSize = ThemeMac::controlSizeFromPixelSize(
      ThemeMac::stepperSizes(), zoomedRect.size(), zoomFactor);
  if (controlSize == NSSmallControlSize)
    drawInfo.kind = kThemeIncDecButtonSmall;
  else if (controlSize == NSMiniControlSize)
    drawInfo.kind = kThemeIncDecButtonMini;
  else
    drawInfo.kind = kThemeIncDecButton;

  IntRect rect(zoomedRect);
  GraphicsContextStateSaver stateSaver(paintInfo.context);
  if (zoomFactor != 1.0f) {
    rect.setWidth(rect.width() / zoomFactor);
    rect.setHeight(rect.height() / zoomFactor);
    paintInfo.context.translate(rect.x(), rect.y());
    paintInfo.context.scale(zoomFactor, zoomFactor);
    paintInfo.context.translate(-rect.x(), -rect.y());
  }
  CGRect bounds(rect);
  CGRect backgroundBounds;
  HIThemeGetButtonBackgroundBounds(&bounds, &drawInfo, &backgroundBounds);
  // Center the stepper rectangle in the specified area.
  backgroundBounds.origin.x =
      bounds.origin.x + (bounds.size.width - backgroundBounds.size.width) / 2;
  if (backgroundBounds.size.height < bounds.size.height) {
    int heightDiff =
        clampTo<int>(bounds.size.height - backgroundBounds.size.height);
    backgroundBounds.origin.y = bounds.origin.y + (heightDiff / 2) + 1;
  }

  LocalCurrentGraphicsContext localContext(paintInfo.context, rect);
  HIThemeDrawButton(&backgroundBounds, &drawInfo, localContext.cgContext(),
                    kHIThemeOrientationNormal, 0);
  return false;
}

}  // namespace blink
