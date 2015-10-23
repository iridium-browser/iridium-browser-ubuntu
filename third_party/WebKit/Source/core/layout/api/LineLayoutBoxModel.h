// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutBoxModel_h
#define LineLayoutBoxModel_h

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LineLayoutItem.h"
#include "platform/LayoutUnit.h"

namespace blink {

class LayoutBoxModelObject;

class LineLayoutBoxModel : public LineLayoutItem {
public:
    explicit LineLayoutBoxModel(LayoutBoxModelObject* layoutBox)
        : LineLayoutItem(layoutBox)
    {
    }

    explicit LineLayoutBoxModel(const LineLayoutItem& item)
        : LineLayoutItem(item)
    {
        ASSERT(!item || item.isBoxModelObject());
    }

    LineLayoutBoxModel() { }

    DeprecatedPaintLayer* layer() const
    {
        return toBoxModel()->layer();
    }

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode lineDirectionMode, LinePositionMode linePositionMode = PositionOnContainingLine) const
    {
        return toBoxModel()->lineHeight(firstLine, lineDirectionMode, linePositionMode);
    }

    int baselinePosition(FontBaseline fontBaseline, bool firstLine, LineDirectionMode lineDirectionMode, LinePositionMode linePositionMode = PositionOnContainingLine) const
    {
        return toBoxModel()->baselinePosition(fontBaseline, firstLine, lineDirectionMode, linePositionMode);
    }

    bool hasSelfPaintingLayer() const
    {
        return toBoxModel()->hasSelfPaintingLayer();
    }

    LayoutUnit marginTop() const
    {
        return toBoxModel()->marginTop();
    }

    LayoutUnit marginBottom() const
    {
        return toBoxModel()->marginBottom();
    }

    LayoutUnit marginLeft() const
    {
        return toBoxModel()->marginLeft();
    }

    LayoutUnit marginRight() const
    {
        return toBoxModel()->marginRight();
    }

    LayoutUnit marginBefore(const ComputedStyle* otherStyle = nullptr) const
    {
        return toBoxModel()->marginBefore(otherStyle);
    }

    LayoutUnit marginAfter(const ComputedStyle* otherStyle = nullptr) const
    {
        return toBoxModel()->marginAfter(otherStyle);
    }

    LayoutUnit paddingTop() const
    {
        return toBoxModel()->paddingTop();
    }

    LayoutUnit paddingBottom() const
    {
        return toBoxModel()->paddingBottom();
    }

    LayoutUnit paddingLeft() const
    {
        return toBoxModel()->paddingLeft();
    }

    LayoutUnit paddingRight() const
    {
        return toBoxModel()->paddingRight();
    }

    LayoutUnit paddingBefore() const
    {
        return toBoxModel()->paddingBefore();
    }

    LayoutUnit paddingAfter() const
    {
        return toBoxModel()->paddingAfter();
    }

    int borderBefore() const
    {
        return toBoxModel()->borderBefore();
    }

    int borderAfter() const
    {
        return toBoxModel()->borderAfter();
    }

    LayoutSize relativePositionLogicalOffset() const
    {
        return toBoxModel()->relativePositionLogicalOffset();
    }

    bool hasInlineDirectionBordersOrPadding() const
    {
        return toBoxModel()->hasInlineDirectionBordersOrPadding();
    }

    LayoutUnit borderAndPaddingLogicalHeight() const
    {
        return toBoxModel()->borderAndPaddingLogicalHeight();
    }

    bool boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* inlineFlowBox = nullptr) const
    {
        return toBoxModel()->boxShadowShouldBeAppliedToBackground(bleedAvoidance, inlineFlowBox);
    }

private:
    LayoutBoxModelObject* toBoxModel() { return toLayoutBoxModelObject(layoutObject()); }
    const LayoutBoxModelObject* toBoxModel() const { return toLayoutBoxModelObject(layoutObject()); }
};

inline LineLayoutBoxModel LineLayoutItem::enclosingBoxModelObject() const
{
    return LineLayoutBoxModel(layoutObject()->enclosingBoxModelObject());
}

} // namespace blink

#endif // LineLayoutBoxModel_h
