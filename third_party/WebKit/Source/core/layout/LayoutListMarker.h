/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef LayoutListMarker_h
#define LayoutListMarker_h

#include "core/layout/LayoutBox.h"

namespace blink {

class LayoutListItem;

String listMarkerText(EListStyleType, int value);

// Used to layout the list item's marker.
// The LayoutListMarker always has to be a child of a LayoutListItem.
class LayoutListMarker final : public LayoutBox {
public:
    static LayoutListMarker* createAnonymous(LayoutListItem*);
    ~LayoutListMarker() override;

    const String& text() const { return m_text; }

    bool isInside() const;

    void updateMarginsAndContent();

    IntRect getRelativeMarkerRect();
    LayoutRect localSelectionRect();
    bool isImage() const override;
    const StyleImage* image() { return m_image.get(); }
    const LayoutListItem* listItem() { return m_listItem; }

    static UChar listMarkerSuffix(EListStyleType, int value);

    void listItemStyleDidChange();

    const char* name() const override { return "LayoutListMarker"; }

protected:
    void willBeDestroyed() override;

private:
    LayoutListMarker(LayoutListItem*);

    void computePreferredLogicalWidths() override;

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectListMarker || LayoutBox::isOfType(type); }

    void paint(const PaintInfo&, const LayoutPoint&) override;

    void layout() override;

    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) override;

    InlineBox* createInlineBox() override;

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override;

    bool isText() const { return !isImage(); }

    void setSelectionState(SelectionState) override;
    LayoutRect selectionRectForPaintInvalidation(const LayoutBoxModelObject* paintInvalidationContainer) const override;
    bool canBeSelectionLeaf() const override { return true; }

    void updateMargins();
    void updateContent();

    void styleWillChange(StyleDifference, const ComputedStyle& newStyle) override;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    String m_text;
    RefPtrWillBePersistent<StyleImage> m_image;
    LayoutListItem* m_listItem;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutListMarker, isListMarker());

} // namespace blink

#endif // LayoutListMarker_h
