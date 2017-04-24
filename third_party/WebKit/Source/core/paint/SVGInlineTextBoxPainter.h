// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInlineTextBoxPainter_h
#define SVGInlineTextBoxPainter_h

#include "core/style/ComputedStyleConstants.h"
#include "core/layout/svg/LayoutSVGResourcePaintServer.h"
#include "wtf/Allocator.h"

namespace blink {

class Font;
struct PaintInfo;
class LayoutPoint;
class LayoutSVGInlineText;
class ComputedStyle;
class SVGInlineTextBox;
struct SVGTextFragment;
class TextRun;
class DocumentMarker;

struct SVGTextFragmentWithRange {
  SVGTextFragmentWithRange(const SVGTextFragment& fragment,
                           int startPosition,
                           int endPosition)
      : fragment(fragment),
        startPosition(startPosition),
        endPosition(endPosition) {}
  const SVGTextFragment& fragment;
  int startPosition;
  int endPosition;
};

class SVGInlineTextBoxPainter {
  STACK_ALLOCATED();

 public:
  SVGInlineTextBoxPainter(const SVGInlineTextBox& svgInlineTextBox)
      : m_svgInlineTextBox(svgInlineTextBox) {}
  void paint(const PaintInfo&, const LayoutPoint&);
  void paintSelectionBackground(const PaintInfo&);
  void paintTextMatchMarkerForeground(const PaintInfo&,
                                      const LayoutPoint&,
                                      const DocumentMarker&,
                                      const ComputedStyle&,
                                      const Font&);
  void paintTextMatchMarkerBackground(const PaintInfo&,
                                      const LayoutPoint&,
                                      const DocumentMarker&,
                                      const ComputedStyle&,
                                      const Font&);

 private:
  bool shouldPaintSelection(const PaintInfo&) const;
  FloatRect boundsForDrawingRecorder(const PaintInfo&,
                                     const ComputedStyle&,
                                     const LayoutPoint&,
                                     bool includeSelectionRect) const;
  void paintTextFragments(const PaintInfo&, LayoutObject&);
  void paintDecoration(const PaintInfo&,
                       TextDecoration,
                       const SVGTextFragment&);
  bool setupTextPaint(const PaintInfo&,
                      const ComputedStyle&,
                      LayoutSVGResourceMode,
                      PaintFlags&);
  void paintText(const PaintInfo&,
                 TextRun&,
                 const SVGTextFragment&,
                 int startPosition,
                 int endPosition,
                 const PaintFlags&);
  void paintText(const PaintInfo&,
                 const ComputedStyle&,
                 const ComputedStyle& selectionStyle,
                 const SVGTextFragment&,
                 LayoutSVGResourceMode,
                 bool shouldPaintSelection);
  Vector<SVGTextFragmentWithRange> collectTextMatches(
      const DocumentMarker&) const;
  Vector<SVGTextFragmentWithRange> collectFragmentsInRange(
      int startPosition,
      int endPosition) const;
  LayoutObject& inlineLayoutObject() const;
  LayoutObject& parentInlineLayoutObject() const;
  LayoutSVGInlineText& inlineText() const;

  const SVGInlineTextBox& m_svgInlineTextBox;
};

}  // namespace blink

#endif  // SVGInlineTextBoxPainter_h
