// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TextPainter.h"

#include "core/CSSPropertyNames.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTextCombine.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ShadowList.h"
#include "platform/fonts/Font.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/text/TextRun.h"
#include "wtf/Assertions.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

TextPainter::TextPainter(GraphicsContext& context,
                         const Font& font,
                         const TextRun& run,
                         const LayoutPoint& textOrigin,
                         const LayoutRect& textBounds,
                         bool horizontal)
    : m_graphicsContext(context),
      m_font(font),
      m_run(run),
      m_textOrigin(textOrigin),
      m_textBounds(textBounds),
      m_horizontal(horizontal),
      m_emphasisMarkOffset(0),
      m_combinedText(0),
      m_ellipsisOffset(0) {}

TextPainter::~TextPainter() {}

void TextPainter::setEmphasisMark(const AtomicString& emphasisMark,
                                  TextEmphasisPosition position) {
  m_emphasisMark = emphasisMark;
  const SimpleFontData* fontData = m_font.primaryFont();
  DCHECK(fontData);

  if (!fontData || emphasisMark.isNull()) {
    m_emphasisMarkOffset = 0;
  } else if (position == TextEmphasisPositionOver) {
    m_emphasisMarkOffset = -fontData->getFontMetrics().ascent() -
                           m_font.emphasisMarkDescent(emphasisMark);
  } else {
    DCHECK(position == TextEmphasisPositionUnder);
    m_emphasisMarkOffset = fontData->getFontMetrics().descent() +
                           m_font.emphasisMarkAscent(emphasisMark);
  }
}

void TextPainter::paint(unsigned startOffset,
                        unsigned endOffset,
                        unsigned length,
                        const Style& textStyle,
                        TextBlobPtr* cachedTextBlob) {
  GraphicsContextStateSaver stateSaver(m_graphicsContext, false);
  updateGraphicsContext(textStyle, stateSaver);
  if (m_combinedText) {
    m_graphicsContext.save();
    m_combinedText->transformToInlineCoordinates(m_graphicsContext,
                                                 m_textBounds);
    paintInternal<PaintText>(startOffset, endOffset, length, cachedTextBlob);
    m_graphicsContext.restore();
  } else {
    paintInternal<PaintText>(startOffset, endOffset, length, cachedTextBlob);
  }

  if (!m_emphasisMark.isEmpty()) {
    if (textStyle.emphasisMarkColor != textStyle.fillColor)
      m_graphicsContext.setFillColor(textStyle.emphasisMarkColor);

    if (m_combinedText)
      paintEmphasisMarkForCombinedText();
    else
      paintInternal<PaintEmphasisMark>(startOffset, endOffset, length);
  }
}

// static
void TextPainter::updateGraphicsContext(GraphicsContext& context,
                                        const Style& textStyle,
                                        bool horizontal,
                                        GraphicsContextStateSaver& stateSaver) {
  TextDrawingModeFlags mode = context.textDrawingMode();
  if (textStyle.strokeWidth > 0) {
    TextDrawingModeFlags newMode = mode | TextModeStroke;
    if (mode != newMode) {
      if (!stateSaver.saved())
        stateSaver.save();
      context.setTextDrawingMode(newMode);
      mode = newMode;
    }
  }

  if (mode & TextModeFill && textStyle.fillColor != context.fillColor())
    context.setFillColor(textStyle.fillColor);

  if (mode & TextModeStroke) {
    if (textStyle.strokeColor != context.strokeColor())
      context.setStrokeColor(textStyle.strokeColor);
    if (textStyle.strokeWidth != context.strokeThickness())
      context.setStrokeThickness(textStyle.strokeWidth);
  }

  if (textStyle.shadow) {
    if (!stateSaver.saved())
      stateSaver.save();
    context.setDrawLooper(textStyle.shadow->createDrawLooper(
        DrawLooperBuilder::ShadowIgnoresAlpha, textStyle.currentColor,
        horizontal));
  }
}

Color TextPainter::textColorForWhiteBackground(Color textColor) {
  int distanceFromWhite = differenceSquared(textColor, Color::white);
  // semi-arbitrarily chose 65025 (255^2) value here after a few tests;
  return distanceFromWhite > 65025 ? textColor : textColor.dark();
}

// static
TextPainter::Style TextPainter::textPaintingStyle(LineLayoutItem lineLayoutItem,
                                                  const ComputedStyle& style,
                                                  const PaintInfo& paintInfo) {
  TextPainter::Style textStyle;
  bool isPrinting = paintInfo.isPrinting();

  if (paintInfo.phase == PaintPhaseTextClip) {
    // When we use the text as a clip, we only care about the alpha, thus we
    // make all the colors black.
    textStyle.currentColor = Color::black;
    textStyle.fillColor = Color::black;
    textStyle.strokeColor = Color::black;
    textStyle.emphasisMarkColor = Color::black;
    textStyle.strokeWidth = style.textStrokeWidth();
    textStyle.shadow = 0;
  } else {
    textStyle.currentColor = style.visitedDependentColor(CSSPropertyColor);
    textStyle.fillColor =
        lineLayoutItem.resolveColor(style, CSSPropertyWebkitTextFillColor);
    textStyle.strokeColor =
        lineLayoutItem.resolveColor(style, CSSPropertyWebkitTextStrokeColor);
    textStyle.emphasisMarkColor =
        lineLayoutItem.resolveColor(style, CSSPropertyWebkitTextEmphasisColor);
    textStyle.strokeWidth = style.textStrokeWidth();
    textStyle.shadow = style.textShadow();

    // Adjust text color when printing with a white background.
    DCHECK(lineLayoutItem.document().printing() == isPrinting);
    bool forceBackgroundToWhite =
        BoxPainter::shouldForceWhiteBackgroundForPrintEconomy(
            style, lineLayoutItem.document());
    if (forceBackgroundToWhite) {
      textStyle.fillColor = textColorForWhiteBackground(textStyle.fillColor);
      textStyle.strokeColor =
          textColorForWhiteBackground(textStyle.strokeColor);
      textStyle.emphasisMarkColor =
          textColorForWhiteBackground(textStyle.emphasisMarkColor);
    }

    // Text shadows are disabled when printing. http://crbug.com/258321
    if (isPrinting)
      textStyle.shadow = 0;
  }

  return textStyle;
}

TextPainter::Style TextPainter::selectionPaintingStyle(
    LineLayoutItem lineLayoutItem,
    bool haveSelection,
    const PaintInfo& paintInfo,
    const TextPainter::Style& textStyle) {
  const LayoutObject& layoutObject =
      *LineLayoutAPIShim::constLayoutObjectFrom(lineLayoutItem);
  TextPainter::Style selectionStyle = textStyle;
  bool usesTextAsClip = paintInfo.phase == PaintPhaseTextClip;
  bool isPrinting = paintInfo.isPrinting();

  if (haveSelection) {
    if (!usesTextAsClip) {
      selectionStyle.fillColor = layoutObject.selectionForegroundColor(
          paintInfo.getGlobalPaintFlags());
      selectionStyle.emphasisMarkColor =
          layoutObject.selectionEmphasisMarkColor(
              paintInfo.getGlobalPaintFlags());
    }

    if (const ComputedStyle* pseudoStyle =
            layoutObject.getCachedPseudoStyle(PseudoIdSelection)) {
      selectionStyle.strokeColor =
          usesTextAsClip ? Color::black
                         : layoutObject.resolveColor(
                               *pseudoStyle, CSSPropertyWebkitTextStrokeColor);
      selectionStyle.strokeWidth = pseudoStyle->textStrokeWidth();
      selectionStyle.shadow = usesTextAsClip ? 0 : pseudoStyle->textShadow();
    }

    // Text shadows are disabled when printing. http://crbug.com/258321
    if (isPrinting)
      selectionStyle.shadow = 0;
  }

  return selectionStyle;
}

template <TextPainter::PaintInternalStep step>
void TextPainter::paintInternalRun(TextRunPaintInfo& textRunPaintInfo,
                                   unsigned from,
                                   unsigned to) {
  DCHECK(from <= textRunPaintInfo.run.length());
  DCHECK(to <= textRunPaintInfo.run.length());

  textRunPaintInfo.from = from;
  textRunPaintInfo.to = to;

  if (step == PaintEmphasisMark) {
    m_graphicsContext.drawEmphasisMarks(
        m_font, textRunPaintInfo, m_emphasisMark,
        FloatPoint(m_textOrigin) + IntSize(0, m_emphasisMarkOffset));
  } else {
    DCHECK(step == PaintText);
    m_graphicsContext.drawText(m_font, textRunPaintInfo,
                               FloatPoint(m_textOrigin));
  }
}

template <TextPainter::PaintInternalStep Step>
void TextPainter::paintInternal(unsigned startOffset,
                                unsigned endOffset,
                                unsigned truncationPoint,
                                TextBlobPtr* cachedTextBlob) {
  TextRunPaintInfo textRunPaintInfo(m_run);
  textRunPaintInfo.bounds = FloatRect(m_textBounds);
  if (startOffset <= endOffset) {
    // FIXME: We should be able to use cachedTextBlob in more cases.
    textRunPaintInfo.cachedTextBlob = cachedTextBlob;
    paintInternalRun<Step>(textRunPaintInfo, startOffset, endOffset);
  } else {
    if (endOffset > 0)
      paintInternalRun<Step>(textRunPaintInfo, m_ellipsisOffset, endOffset);
    if (startOffset < truncationPoint)
      paintInternalRun<Step>(textRunPaintInfo, startOffset, truncationPoint);
  }
}

void TextPainter::clipDecorationsStripe(float upper,
                                        float stripeWidth,
                                        float dilation) {
  TextRunPaintInfo textRunPaintInfo(m_run);

  if (!m_run.length())
    return;

  Vector<Font::TextIntercept> textIntercepts;
  m_font.getTextIntercepts(
      textRunPaintInfo, m_graphicsContext.deviceScaleFactor(),
      m_graphicsContext.fillPaint(),
      std::make_tuple(upper, upper + stripeWidth), textIntercepts);

  for (auto intercept : textIntercepts) {
    FloatPoint clipOrigin(m_textOrigin);
    FloatRect clipRect(
        clipOrigin + FloatPoint(intercept.m_begin, upper),
        FloatSize(intercept.m_end - intercept.m_begin, stripeWidth));
    clipRect.inflateX(dilation);
    // We need to ensure the clip rectangle is covering the full underline
    // extent. For horizontal drawing, using enclosingIntRect would be
    // sufficient, since we can clamp to full device pixels that way. However,
    // for vertical drawing, we have a transformation applied, which breaks the
    // integers-equal-device pixels assumption, so vertically inflating by 1
    // pixel makes sure we're always covering. This should only be done on the
    // clipping rectangle, not when computing the glyph intersects.
    clipRect.inflateY(1.0);
    m_graphicsContext.clipOut(clipRect);
  }
}

void TextPainter::paintEmphasisMarkForCombinedText() {
  const SimpleFontData* fontData = m_font.primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return;

  DCHECK(m_combinedText);
  TextRun placeholderTextRun(&ideographicFullStopCharacter, 1);
  FloatPoint emphasisMarkTextOrigin(m_textBounds.x().toFloat(),
                                    m_textBounds.y().toFloat() +
                                        fontData->getFontMetrics().ascent() +
                                        m_emphasisMarkOffset);
  TextRunPaintInfo textRunPaintInfo(placeholderTextRun);
  textRunPaintInfo.bounds = FloatRect(m_textBounds);
  m_graphicsContext.concatCTM(rotation(m_textBounds, Clockwise));
  m_graphicsContext.drawEmphasisMarks(m_combinedText->originalFont(),
                                      textRunPaintInfo, m_emphasisMark,
                                      emphasisMarkTextOrigin);
  m_graphicsContext.concatCTM(rotation(m_textBounds, Counterclockwise));
}

}  // namespace blink
