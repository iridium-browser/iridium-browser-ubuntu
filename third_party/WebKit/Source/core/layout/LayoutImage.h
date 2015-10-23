/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef LayoutImage_h
#define LayoutImage_h

#include "core/CoreExport.h"
#include "core/layout/LayoutImageResource.h"
#include "core/layout/LayoutReplaced.h"

namespace blink {

class HTMLAreaElement;
class HTMLMapElement;

class CORE_EXPORT LayoutImage : public LayoutReplaced {
public:
    // These are the paddings to use when displaying either alt text or an image.
    static const unsigned short paddingWidth = 4;
    static const unsigned short paddingHeight = 4;

    LayoutImage(Element*);
    ~LayoutImage() override;

    static LayoutImage* createAnonymous(Document*);

    void setImageResource(PassOwnPtrWillBeRawPtr<LayoutImageResource>);

    LayoutImageResource* imageResource() { return m_imageResource.get(); }
    const LayoutImageResource* imageResource() const { return m_imageResource.get(); }
    ImageResource* cachedImage() const { return m_imageResource ? m_imageResource->cachedImage() : 0; }

    HTMLMapElement* imageMap() const;
    void areaElementFocusChanged(HTMLAreaElement*);

    void setIsGeneratedContent(bool generated = true) { m_isGeneratedContent = generated; }

    bool isGeneratedContent() const { return m_isGeneratedContent; }

    inline void setImageDevicePixelRatio(float factor) { m_imageDevicePixelRatio = factor; }
    float imageDevicePixelRatio() const { return m_imageDevicePixelRatio; }

    void intrinsicSizeChanged() override
    {
        if (m_imageResource)
            imageChanged(m_imageResource->imagePtr());
    }

    const char* name() const override { return "LayoutImage"; }

protected:
    bool needsPreferredWidthsRecalculation() const final;
    LayoutBox* embeddedContentBox() const final;
    void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const final;

    void imageChanged(WrappedImagePtr, const IntRect* = nullptr) override;

    void paint(const PaintInfo&, const LayoutPoint&) final;

    void layout() override;
    bool updateImageLoadingPriorities() final;

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutImage || LayoutReplaced::isOfType(type); }

    void willBeDestroyed() override;

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

private:
    bool isImage() const override { return true; }

    void paintReplaced(const PaintInfo&, const LayoutPoint&) override;

    bool foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const final;
    bool computeBackgroundIsKnownToBeObscured() final;

    bool backgroundShouldAlwaysBeClipped() const override { return true; }

    LayoutUnit minimumReplacedHeight() const override;

    void notifyFinished(Resource*) final;
    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    bool boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance, InlineFlowBox*) const final;

    void invalidatePaintAndMarkForLayoutIfNeeded();
    void updateIntrinsicSizeIfNeeded(const LayoutSize&);
    // Update the size of the image to be rendered. Object-fit may cause this to be different from the CSS box's content rect.
    void updateInnerContentRect();

    // Text to display as long as the image isn't available.
    OwnPtrWillBePersistent<LayoutImageResource> m_imageResource;
    bool m_didIncrementVisuallyNonEmptyPixelCount;
    bool m_isGeneratedContent;
    float m_imageDevicePixelRatio;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutImage, isLayoutImage());

} // namespace blink

#endif // LayoutImage_h
