// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/RootInlineBoxPainter.h"

#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/EllipsisBox.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/PaintInfo.h"

namespace blink {

void RootInlineBoxPainter::paintEllipsisBox(const PaintInfo& paintInfo,
                                            const LayoutPoint& paintOffset,
                                            LayoutUnit lineTop,
                                            LayoutUnit lineBottom) const {
  if (m_rootInlineBox.hasEllipsisBox() &&
      m_rootInlineBox.getLineLayoutItem().style()->visibility() ==
          EVisibility::kVisible &&
      paintInfo.phase == PaintPhaseForeground)
    m_rootInlineBox.ellipsisBox()->paint(paintInfo, paintOffset, lineTop,
                                         lineBottom);
}

void RootInlineBoxPainter::paint(const PaintInfo& paintInfo,
                                 const LayoutPoint& paintOffset,
                                 LayoutUnit lineTop,
                                 LayoutUnit lineBottom) {
  m_rootInlineBox.InlineFlowBox::paint(paintInfo, paintOffset, lineTop,
                                       lineBottom);
  paintEllipsisBox(paintInfo, paintOffset, lineTop, lineBottom);
}

}  // namespace blink
