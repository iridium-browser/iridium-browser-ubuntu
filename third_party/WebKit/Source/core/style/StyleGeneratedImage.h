/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef StyleGeneratedImage_h
#define StyleGeneratedImage_h

#include "core/CoreExport.h"
#include "core/style/StyleImage.h"

namespace blink {

class CSSValue;
class CSSImageGeneratorValue;

class CORE_EXPORT StyleGeneratedImage final : public StyleImage {
public:
    static PassRefPtrWillBeRawPtr<StyleGeneratedImage> create(CSSImageGeneratorValue* value)
    {
        return adoptRefWillBeNoop(new StyleGeneratedImage(value));
    }

    WrappedImagePtr data() const override { return m_imageGeneratorValue.get(); }

    PassRefPtrWillBeRawPtr<CSSValue> cssValue() const override;

    LayoutSize imageSize(const LayoutObject*, float multiplier) const override;
    bool imageHasRelativeWidth() const override { return !m_fixedSize; }
    bool imageHasRelativeHeight() const override { return !m_fixedSize; }
    void computeIntrinsicDimensions(const LayoutObject*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) override;
    bool usesImageContainerSize() const override { return !m_fixedSize; }
    void setContainerSizeForLayoutObject(const LayoutObject*, const IntSize& containerSize, float) override { m_containerSize = containerSize; }
    void addClient(LayoutObject*) override;
    void removeClient(LayoutObject*) override;
    PassRefPtr<Image> image(LayoutObject*, const IntSize&) const override;
    bool knownToBeOpaque(const LayoutObject*) const override;

    DECLARE_VIRTUAL_TRACE();

private:
    StyleGeneratedImage(PassRefPtrWillBeRawPtr<CSSImageGeneratorValue>);

    RefPtrWillBeMember<CSSImageGeneratorValue> m_imageGeneratorValue;
    IntSize m_containerSize;
    bool m_fixedSize;
};

}
#endif
