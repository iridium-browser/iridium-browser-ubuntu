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

#ifndef StyleFetchedImage_h
#define StyleFetchedImage_h

#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/ResourcePtr.h"
#include "core/style/StyleImage.h"

namespace blink {

class Document;
class ImageResource;

class StyleFetchedImage final : public StyleImage, private ImageResourceClient {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(StyleFetchedImage);
public:
    static PassRefPtrWillBeRawPtr<StyleFetchedImage> create(ImageResource* image, Document* document)
    {
        return adoptRefWillBeNoop(new StyleFetchedImage(image, document));
    }
    ~StyleFetchedImage() override;

    WrappedImagePtr data() const override { return m_image.get(); }

    PassRefPtrWillBeRawPtr<CSSValue> cssValue() const override;

    bool canRender(const LayoutObject&, float multiplier) const override;
    bool isLoaded() const override;
    bool errorOccurred() const override;
    LayoutSize imageSize(const LayoutObject*, float multiplier) const override;
    bool imageHasRelativeWidth() const override;
    bool imageHasRelativeHeight() const override;
    void computeIntrinsicDimensions(const LayoutObject*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) override;
    bool usesImageContainerSize() const override;
    void setContainerSizeForLayoutObject(const LayoutObject*, const IntSize&, float) override;
    void addClient(LayoutObject*) override;
    void removeClient(LayoutObject*) override;
    void notifyFinished(Resource*) override;
    PassRefPtr<Image> image(LayoutObject*, const IntSize&) const override;
    bool knownToBeOpaque(const LayoutObject*) const override;
    ImageResource* cachedImage() const override { return m_image.get(); }

    DECLARE_VIRTUAL_TRACE();

private:
    StyleFetchedImage(ImageResource*, Document*);

    ResourcePtr<ImageResource> m_image;
    RawPtrWillBeMember<Document> m_document;
};

DEFINE_STYLE_IMAGE_TYPE_CASTS(StyleFetchedImage, isImageResource());

}
#endif
