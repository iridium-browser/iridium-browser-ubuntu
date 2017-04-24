// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGInlineTextBoxPainter.h"

#include "core/editing/Editor.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/markers/RenderedDocumentMarker.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "core/paint/InlineTextBoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGPaintContext.h"
#include "core/style/AppliedTextDecoration.h"
#include "core/style/ShadowList.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include <memory>

namespace blink {

static inline bool textShouldBePainted(
    const LayoutSVGInlineText& textLayoutObject) {
  // Font::pixelSize(), returns FontDescription::computedPixelSize(), which
  // returns "int(x + 0.5)".  If the absolute font size on screen is below
  // x=0.5, don't render anything.
  return textLayoutObject.scaledFont().getFontDescription().computedPixelSize();
}

bool SVGInlineTextBoxPainter::shouldPaintSelection(
    const PaintInfo& paintInfo) const {
  // Don't paint selections when printing.
  if (paintInfo.isPrinting())
    return false;
  // Don't paint selections when rendering a mask, clip-path (as a mask),
  // pattern or feImage (element reference.)
  if (paintInfo.isRenderingResourceSubtree())
    return false;
  return m_svgInlineTextBox.getSelectionState() != SelectionNone;
}

static bool hasShadow(const PaintInfo& paintInfo, const ComputedStyle& style) {
  // Text shadows are disabled when printing. http://crbug.com/258321
  return style.textShadow() && !paintInfo.isPrinting();
}

FloatRect SVGInlineTextBoxPainter::boundsForDrawingRecorder(
    const PaintInfo& paintInfo,
    const ComputedStyle& style,
    const LayoutPoint& paintOffset,
    bool includeSelectionRect) const {
  LayoutRect bounds(m_svgInlineTextBox.location() + paintOffset,
                    m_svgInlineTextBox.size());
  if (hasShadow(paintInfo, style))
    bounds.expand(style.textShadow()->rectOutsetsIncludingOriginal());
  if (includeSelectionRect) {
    bounds.unite(m_svgInlineTextBox.localSelectionRect(
        m_svgInlineTextBox.start(),
        m_svgInlineTextBox.start() + m_svgInlineTextBox.len()));
  }
  return FloatRect(bounds);
}

LayoutObject& SVGInlineTextBoxPainter::inlineLayoutObject() const {
  return *LineLayoutAPIShim::layoutObjectFrom(
      m_svgInlineTextBox.getLineLayoutItem());
}

LayoutObject& SVGInlineTextBoxPainter::parentInlineLayoutObject() const {
  return *LineLayoutAPIShim::layoutObjectFrom(
      m_svgInlineTextBox.parent()->getLineLayoutItem());
}

LayoutSVGInlineText& SVGInlineTextBoxPainter::inlineText() const {
  return toLayoutSVGInlineText(inlineLayoutObject());
}

void SVGInlineTextBoxPainter::paint(const PaintInfo& paintInfo,
                                    const LayoutPoint& paintOffset) {
  DCHECK(paintInfo.phase == PaintPhaseForeground ||
         paintInfo.phase == PaintPhaseSelection);
  DCHECK(m_svgInlineTextBox.truncation() == cNoTruncation);

  if (m_svgInlineTextBox.getLineLayoutItem().style()->visibility() !=
          EVisibility::kVisible ||
      !m_svgInlineTextBox.len())
    return;

  // We're explicitly not supporting composition & custom underlines and custom
  // highlighters -- unlike InlineTextBox.  If we ever need that for SVG, it's
  // very easy to refactor and reuse the code.

  bool haveSelection = shouldPaintSelection(paintInfo);
  if (!haveSelection && paintInfo.phase == PaintPhaseSelection)
    return;

  LayoutSVGInlineText& textLayoutObject = inlineText();
  if (!textShouldBePainted(textLayoutObject))
    return;

  DisplayItem::Type displayItemType =
      DisplayItem::paintPhaseToDrawingType(paintInfo.phase);
  if (!DrawingRecorder::useCachedDrawingIfPossible(
          paintInfo.context, m_svgInlineTextBox, displayItemType)) {
    LayoutObject& parentLayoutObject = parentInlineLayoutObject();
    const ComputedStyle& style = parentLayoutObject.styleRef();

    bool includeSelectionRect =
        paintInfo.phase != PaintPhaseSelection &&
        (haveSelection ||
         InlineTextBoxPainter::paintsMarkerHighlights(textLayoutObject));
    DrawingRecorder recorder(
        paintInfo.context, m_svgInlineTextBox, displayItemType,
        boundsForDrawingRecorder(paintInfo, style, paintOffset,
                                 includeSelectionRect));
    InlineTextBoxPainter textPainter(m_svgInlineTextBox);
    textPainter.paintDocumentMarkers(paintInfo, paintOffset, style,
                                     textLayoutObject.scaledFont(),
                                     DocumentMarkerPaintPhase::Background);

    if (!m_svgInlineTextBox.textFragments().isEmpty())
      paintTextFragments(paintInfo, parentLayoutObject);

    textPainter.paintDocumentMarkers(paintInfo, paintOffset, style,
                                     textLayoutObject.scaledFont(),
                                     DocumentMarkerPaintPhase::Foreground);
  }
}

void SVGInlineTextBoxPainter::paintTextFragments(
    const PaintInfo& paintInfo,
    LayoutObject& parentLayoutObject) {
  const ComputedStyle& style = parentLayoutObject.styleRef();
  const SVGComputedStyle& svgStyle = style.svgStyle();

  bool hasFill = svgStyle.hasFill();
  bool hasVisibleStroke = svgStyle.hasVisibleStroke();

  const ComputedStyle* selectionStyle = &style;
  bool shouldPaintSelection = this->shouldPaintSelection(paintInfo);
  if (shouldPaintSelection) {
    selectionStyle = parentLayoutObject.getCachedPseudoStyle(PseudoIdSelection);
    if (selectionStyle) {
      const SVGComputedStyle& svgSelectionStyle = selectionStyle->svgStyle();

      if (!hasFill)
        hasFill = svgSelectionStyle.hasFill();
      if (!hasVisibleStroke)
        hasVisibleStroke = svgSelectionStyle.hasVisibleStroke();
    } else {
      selectionStyle = &style;
    }
  }

  if (paintInfo.isRenderingClipPathAsMaskImage()) {
    hasFill = true;
    hasVisibleStroke = false;
  }

  unsigned textFragmentsSize = m_svgInlineTextBox.textFragments().size();
  for (unsigned i = 0; i < textFragmentsSize; ++i) {
    const SVGTextFragment& fragment = m_svgInlineTextBox.textFragments().at(i);

    GraphicsContextStateSaver stateSaver(paintInfo.context, false);
    if (fragment.isTransformed()) {
      stateSaver.save();
      paintInfo.context.concatCTM(fragment.buildFragmentTransform());
    }

    // Spec: All text decorations except line-through should be drawn before the
    // text is filled and stroked; thus, the text is rendered on top of these
    // decorations.
    const Vector<AppliedTextDecoration>& decorations =
        style.appliedTextDecorations();
    for (const AppliedTextDecoration& decoration : decorations) {
      if (decoration.lines() & TextDecorationUnderline)
        paintDecoration(paintInfo, TextDecorationUnderline, fragment);
      if (decoration.lines() & TextDecorationOverline)
        paintDecoration(paintInfo, TextDecorationOverline, fragment);
    }

    for (int i = 0; i < 3; i++) {
      switch (svgStyle.paintOrderType(i)) {
        case PT_FILL:
          if (hasFill)
            paintText(paintInfo, style, *selectionStyle, fragment,
                      ApplyToFillMode, shouldPaintSelection);
          break;
        case PT_STROKE:
          if (hasVisibleStroke)
            paintText(paintInfo, style, *selectionStyle, fragment,
                      ApplyToStrokeMode, shouldPaintSelection);
          break;
        case PT_MARKERS:
          // Markers don't apply to text
          break;
        default:
          ASSERT_NOT_REACHED();
          break;
      }
    }

    // Spec: Line-through should be drawn after the text is filled and stroked;
    // thus, the line-through is rendered on top of the text.
    for (const AppliedTextDecoration& decoration : decorations) {
      if (decoration.lines() & TextDecorationLineThrough)
        paintDecoration(paintInfo, TextDecorationLineThrough, fragment);
    }
  }
}

void SVGInlineTextBoxPainter::paintSelectionBackground(
    const PaintInfo& paintInfo) {
  if (m_svgInlineTextBox.getLineLayoutItem().style()->visibility() !=
      EVisibility::kVisible)
    return;

  DCHECK(!paintInfo.isPrinting());

  if (paintInfo.phase == PaintPhaseSelection ||
      !shouldPaintSelection(paintInfo))
    return;

  Color backgroundColor =
      m_svgInlineTextBox.getLineLayoutItem().selectionBackgroundColor();
  if (!backgroundColor.alpha())
    return;

  LayoutSVGInlineText& textLayoutObject = inlineText();
  if (!textShouldBePainted(textLayoutObject))
    return;

  const ComputedStyle& style =
      m_svgInlineTextBox.parent()->getLineLayoutItem().styleRef();

  int startPosition, endPosition;
  m_svgInlineTextBox.selectionStartEnd(startPosition, endPosition);

  const Vector<SVGTextFragmentWithRange> fragmentInfoList =
      collectFragmentsInRange(startPosition, endPosition);
  for (const SVGTextFragmentWithRange& fragmentWithRange : fragmentInfoList) {
    const SVGTextFragment& fragment = fragmentWithRange.fragment;
    GraphicsContextStateSaver stateSaver(paintInfo.context);
    if (fragment.isTransformed())
      paintInfo.context.concatCTM(fragment.buildFragmentTransform());

    paintInfo.context.setFillColor(backgroundColor);
    paintInfo.context.fillRect(m_svgInlineTextBox.selectionRectForTextFragment(
                                   fragment, fragmentWithRange.startPosition,
                                   fragmentWithRange.endPosition, style),
                               backgroundColor);
  }
}

static inline LayoutObject* findLayoutObjectDefininingTextDecoration(
    InlineFlowBox* parentBox) {
  // Lookup first layout object in parent hierarchy which has text-decoration
  // set.
  LayoutObject* layoutObject = nullptr;
  while (parentBox) {
    layoutObject =
        LineLayoutAPIShim::layoutObjectFrom(parentBox->getLineLayoutItem());

    if (layoutObject->style() &&
        layoutObject->style()->getTextDecoration() != TextDecorationNone)
      break;

    parentBox = parentBox->parent();
  }

  DCHECK(layoutObject);
  return layoutObject;
}

// Offset from the baseline for |decoration|. Positive offsets are above the
// baseline.
static inline float baselineOffsetForDecoration(TextDecoration decoration,
                                                const FontMetrics& fontMetrics,
                                                float thickness) {
  // FIXME: For SVG Fonts we need to use the attributes defined in the
  // <font-face> if specified.
  // Compatible with Batik/Presto.
  if (decoration == TextDecorationUnderline)
    return -thickness * 1.5f;
  if (decoration == TextDecorationOverline)
    return fontMetrics.floatAscent() - thickness;
  if (decoration == TextDecorationLineThrough)
    return fontMetrics.floatAscent() * 3 / 8.0f;

  ASSERT_NOT_REACHED();
  return 0.0f;
}

static inline float thicknessForDecoration(TextDecoration, const Font& font) {
  // FIXME: For SVG Fonts we need to use the attributes defined in the
  // <font-face> if specified.
  // Compatible with Batik/Presto
  return font.getFontDescription().computedSize() / 20.0f;
}

void SVGInlineTextBoxPainter::paintDecoration(const PaintInfo& paintInfo,
                                              TextDecoration decoration,
                                              const SVGTextFragment& fragment) {
  if (m_svgInlineTextBox.getLineLayoutItem()
          .style()
          ->textDecorationsInEffect() == TextDecorationNone)
    return;

  if (fragment.width <= 0)
    return;

  // Find out which style defined the text-decoration, as its fill/stroke
  // properties have to be used for drawing instead of ours.
  LayoutObject* decorationLayoutObject =
      findLayoutObjectDefininingTextDecoration(m_svgInlineTextBox.parent());
  const ComputedStyle& decorationStyle = decorationLayoutObject->styleRef();

  if (decorationStyle.visibility() != EVisibility::kVisible)
    return;

  float scalingFactor = 1;
  Font scaledFont;
  LayoutSVGInlineText::computeNewScaledFontForStyle(*decorationLayoutObject,
                                                    scalingFactor, scaledFont);
  DCHECK(scalingFactor);

  float thickness = thicknessForDecoration(decoration, scaledFont);
  if (thickness <= 0)
    return;

  const SimpleFontData* fontData = scaledFont.primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return;

  float decorationOffset = baselineOffsetForDecoration(
      decoration, fontData->getFontMetrics(), thickness);
  FloatPoint decorationOrigin(fragment.x,
                              fragment.y - decorationOffset / scalingFactor);

  Path path;
  path.addRect(FloatRect(decorationOrigin,
                         FloatSize(fragment.width, thickness / scalingFactor)));

  const SVGComputedStyle& svgDecorationStyle = decorationStyle.svgStyle();

  for (int i = 0; i < 3; i++) {
    switch (svgDecorationStyle.paintOrderType(i)) {
      case PT_FILL:
        if (svgDecorationStyle.hasFill()) {
          PaintFlags fillFlags;
          if (!SVGPaintContext::paintForLayoutObject(
                  paintInfo, decorationStyle, *decorationLayoutObject,
                  ApplyToFillMode, fillFlags))
            break;
          fillFlags.setAntiAlias(true);
          paintInfo.context.drawPath(path.getSkPath(), fillFlags);
        }
        break;
      case PT_STROKE:
        if (svgDecorationStyle.hasVisibleStroke()) {
          PaintFlags strokeFlags;
          if (!SVGPaintContext::paintForLayoutObject(
                  paintInfo, decorationStyle, *decorationLayoutObject,
                  ApplyToStrokeMode, strokeFlags))
            break;
          strokeFlags.setAntiAlias(true);
          float strokeScaleFactor =
              svgDecorationStyle.vectorEffect() == VE_NON_SCALING_STROKE
                  ? 1 / scalingFactor
                  : 1;
          StrokeData strokeData;
          SVGLayoutSupport::applyStrokeStyleToStrokeData(
              strokeData, decorationStyle, *decorationLayoutObject,
              strokeScaleFactor);
          if (strokeScaleFactor != 1)
            strokeData.setThickness(strokeData.thickness() * strokeScaleFactor);
          strokeData.setupPaint(&strokeFlags);
          paintInfo.context.drawPath(path.getSkPath(), strokeFlags);
        }
        break;
      case PT_MARKERS:
        break;
      default:
        ASSERT_NOT_REACHED();
    }
  }
}

bool SVGInlineTextBoxPainter::setupTextPaint(const PaintInfo& paintInfo,
                                             const ComputedStyle& style,
                                             LayoutSVGResourceMode resourceMode,
                                             PaintFlags& flags) {
  LayoutSVGInlineText& textLayoutObject = inlineText();

  float scalingFactor = textLayoutObject.scalingFactor();
  DCHECK(scalingFactor);

  AffineTransform paintServerTransform;
  const AffineTransform* additionalPaintServerTransform = nullptr;

  if (scalingFactor != 1) {
    // Adjust the paint-server coordinate space.
    paintServerTransform.scale(scalingFactor);
    additionalPaintServerTransform = &paintServerTransform;
  }

  if (!SVGPaintContext::paintForLayoutObject(
          paintInfo, style, parentInlineLayoutObject(), resourceMode, flags,
          additionalPaintServerTransform))
    return false;
  flags.setAntiAlias(true);

  if (hasShadow(paintInfo, style)) {
    flags.setLooper(style.textShadow()->createDrawLooper(
        DrawLooperBuilder::ShadowRespectsAlpha,
        style.visitedDependentColor(CSSPropertyColor)));
  }

  if (resourceMode == ApplyToStrokeMode) {
    // The stroke geometry needs be generated based on the scaled font.
    float strokeScaleFactor =
        style.svgStyle().vectorEffect() != VE_NON_SCALING_STROKE ? scalingFactor
                                                                 : 1;
    StrokeData strokeData;
    SVGLayoutSupport::applyStrokeStyleToStrokeData(
        strokeData, style, parentInlineLayoutObject(), strokeScaleFactor);
    if (strokeScaleFactor != 1)
      strokeData.setThickness(strokeData.thickness() * strokeScaleFactor);
    strokeData.setupPaint(&flags);
  }
  return true;
}

void SVGInlineTextBoxPainter::paintText(const PaintInfo& paintInfo,
                                        TextRun& textRun,
                                        const SVGTextFragment& fragment,
                                        int startPosition,
                                        int endPosition,
                                        const PaintFlags& flags) {
  LayoutSVGInlineText& textLayoutObject = inlineText();
  const Font& scaledFont = textLayoutObject.scaledFont();

  float scalingFactor = textLayoutObject.scalingFactor();
  DCHECK(scalingFactor);

  FloatPoint textOrigin(fragment.x, fragment.y);
  FloatSize textSize(fragment.width, fragment.height);

  GraphicsContext& context = paintInfo.context;
  GraphicsContextStateSaver stateSaver(context, false);
  if (scalingFactor != 1) {
    textOrigin.scale(scalingFactor, scalingFactor);
    textSize.scale(scalingFactor);
    stateSaver.save();
    context.scale(1 / scalingFactor, 1 / scalingFactor);
  }

  TextRunPaintInfo textRunPaintInfo(textRun);
  textRunPaintInfo.from = startPosition;
  textRunPaintInfo.to = endPosition;

  const SimpleFontData* fontData = scaledFont.primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return;
  float baseline = fontData->getFontMetrics().floatAscent();
  textRunPaintInfo.bounds = FloatRect(textOrigin.x(), textOrigin.y() - baseline,
                                      textSize.width(), textSize.height());

  context.drawText(scaledFont, textRunPaintInfo, textOrigin, flags);
}

void SVGInlineTextBoxPainter::paintText(const PaintInfo& paintInfo,
                                        const ComputedStyle& style,
                                        const ComputedStyle& selectionStyle,
                                        const SVGTextFragment& fragment,
                                        LayoutSVGResourceMode resourceMode,
                                        bool shouldPaintSelection) {
  int startPosition = 0;
  int endPosition = 0;
  if (shouldPaintSelection) {
    m_svgInlineTextBox.selectionStartEnd(startPosition, endPosition);
    shouldPaintSelection =
        m_svgInlineTextBox.mapStartEndPositionsIntoFragmentCoordinates(
            fragment, startPosition, endPosition);
  }

  // Fast path if there is no selection, just draw the whole chunk part using
  // the regular style.
  TextRun textRun = m_svgInlineTextBox.constructTextRun(style, fragment);
  if (!shouldPaintSelection || startPosition >= endPosition) {
    PaintFlags flags;
    if (setupTextPaint(paintInfo, style, resourceMode, flags))
      paintText(paintInfo, textRun, fragment, 0, fragment.length, flags);
    return;
  }

  // Eventually draw text using regular style until the start position of the
  // selection.
  bool paintSelectedTextOnly = paintInfo.phase == PaintPhaseSelection;
  if (startPosition > 0 && !paintSelectedTextOnly) {
    PaintFlags flags;
    if (setupTextPaint(paintInfo, style, resourceMode, flags))
      paintText(paintInfo, textRun, fragment, 0, startPosition, flags);
  }

  // Draw text using selection style from the start to the end position of the
  // selection.
  if (style != selectionStyle) {
    StyleDifference diff;
    diff.setNeedsPaintInvalidationObject();
    SVGResourcesCache::clientStyleChanged(&parentInlineLayoutObject(), diff,
                                          selectionStyle);
  }

  PaintFlags flags;
  if (setupTextPaint(paintInfo, selectionStyle, resourceMode, flags))
    paintText(paintInfo, textRun, fragment, startPosition, endPosition, flags);

  if (style != selectionStyle) {
    StyleDifference diff;
    diff.setNeedsPaintInvalidationObject();
    SVGResourcesCache::clientStyleChanged(&parentInlineLayoutObject(), diff,
                                          style);
  }

  // Eventually draw text using regular style from the end position of the
  // selection to the end of the current chunk part.
  if (endPosition < static_cast<int>(fragment.length) &&
      !paintSelectedTextOnly) {
    PaintFlags flags;
    if (setupTextPaint(paintInfo, style, resourceMode, flags)) {
      paintText(paintInfo, textRun, fragment, endPosition, fragment.length,
                flags);
    }
  }
}

Vector<SVGTextFragmentWithRange> SVGInlineTextBoxPainter::collectTextMatches(
    const DocumentMarker& marker) const {
  const Vector<SVGTextFragmentWithRange> emptyTextMatchList;

  // SVG does not support grammar or spellcheck markers, so skip anything but
  // TextMatch.
  if (marker.type() != DocumentMarker::TextMatch)
    return emptyTextMatchList;

  if (!inlineLayoutObject().frame()->editor().markedTextMatchesAreHighlighted())
    return emptyTextMatchList;

  int markerStartPosition =
      std::max<int>(marker.startOffset() - m_svgInlineTextBox.start(), 0);
  int markerEndPosition =
      std::min<int>(marker.endOffset() - m_svgInlineTextBox.start(),
                    m_svgInlineTextBox.len());

  if (markerStartPosition >= markerEndPosition)
    return emptyTextMatchList;

  return collectFragmentsInRange(markerStartPosition, markerEndPosition);
}

Vector<SVGTextFragmentWithRange>
SVGInlineTextBoxPainter::collectFragmentsInRange(int startPosition,
                                                 int endPosition) const {
  Vector<SVGTextFragmentWithRange> fragmentInfoList;
  const Vector<SVGTextFragment>& fragments = m_svgInlineTextBox.textFragments();
  for (const SVGTextFragment& fragment : fragments) {
    // TODO(ramya.v): If these can't be negative we should use unsigned.
    int fragmentStartPosition = startPosition;
    int fragmentEndPosition = endPosition;
    if (!m_svgInlineTextBox.mapStartEndPositionsIntoFragmentCoordinates(
            fragment, fragmentStartPosition, fragmentEndPosition))
      continue;

    fragmentInfoList.push_back(SVGTextFragmentWithRange(
        fragment, fragmentStartPosition, fragmentEndPosition));
  }
  return fragmentInfoList;
}

void SVGInlineTextBoxPainter::paintTextMatchMarkerForeground(
    const PaintInfo& paintInfo,
    const LayoutPoint& point,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) {
  const Vector<SVGTextFragmentWithRange> textMatchInfoList =
      collectTextMatches(marker);
  if (textMatchInfoList.isEmpty())
    return;

  Color textColor =
      LayoutTheme::theme().platformTextSearchColor(marker.activeMatch());

  PaintFlags fillFlags;
  fillFlags.setColor(textColor.rgb());
  fillFlags.setAntiAlias(true);

  PaintFlags strokeFlags;
  bool shouldPaintStroke = false;
  if (setupTextPaint(paintInfo, style, ApplyToStrokeMode, strokeFlags)) {
    shouldPaintStroke = true;
    strokeFlags.setLooper(nullptr);
    strokeFlags.setColor(textColor.rgb());
  }

  for (const SVGTextFragmentWithRange& textMatchInfo : textMatchInfoList) {
    const SVGTextFragment& fragment = textMatchInfo.fragment;
    GraphicsContextStateSaver stateSaver(paintInfo.context);
    if (fragment.isTransformed())
      paintInfo.context.concatCTM(fragment.buildFragmentTransform());

    TextRun textRun = m_svgInlineTextBox.constructTextRun(style, fragment);
    paintText(paintInfo, textRun, fragment, textMatchInfo.startPosition,
              textMatchInfo.endPosition, fillFlags);
    if (shouldPaintStroke) {
      paintText(paintInfo, textRun, fragment, textMatchInfo.startPosition,
                textMatchInfo.endPosition, strokeFlags);
    }
  }
}

void SVGInlineTextBoxPainter::paintTextMatchMarkerBackground(
    const PaintInfo& paintInfo,
    const LayoutPoint& point,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) {
  const Vector<SVGTextFragmentWithRange> textMatchInfoList =
      collectTextMatches(marker);
  if (textMatchInfoList.isEmpty())
    return;

  Color color = LayoutTheme::theme().platformTextSearchHighlightColor(
      marker.activeMatch());
  for (const SVGTextFragmentWithRange& textMatchInfo : textMatchInfoList) {
    const SVGTextFragment& fragment = textMatchInfo.fragment;

    GraphicsContextStateSaver stateSaver(paintInfo.context, false);
    if (fragment.isTransformed()) {
      stateSaver.save();
      paintInfo.context.concatCTM(fragment.buildFragmentTransform());
    }
    FloatRect fragmentRect = m_svgInlineTextBox.selectionRectForTextFragment(
        fragment, textMatchInfo.startPosition, textMatchInfo.endPosition,
        style);
    paintInfo.context.setFillColor(color);
    paintInfo.context.fillRect(fragmentRect);
  }
}

}  // namespace blink
