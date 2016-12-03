// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterDisplayItem_h
#define FilterDisplayItem_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/PassRefPtr.h"
#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif
#include <memory>

namespace blink {

class PLATFORM_EXPORT BeginFilterDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginFilterDisplayItem(const DisplayItemClient& client, sk_sp<SkImageFilter> imageFilter, const FloatRect& bounds, const FloatPoint& origin, CompositorFilterOperations filterOperations)
        : PairedBeginDisplayItem(client, BeginFilter, sizeof(*this))
        , m_imageFilter(std::move(imageFilter))
        , m_compositorFilterOperations(std::move(filterOperations))
        , m_bounds(bounds)
        , m_origin(origin) { }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;
    bool drawsContent() const override;

private:
#ifndef NDEBUG
    void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
#if ENABLE(ASSERT)
    bool equals(const DisplayItem& other) const final
    {
        return DisplayItem::equals(other)
            // TODO(wangxianzhu): compare m_imageFilter and m_webFilterOperations.
            && m_bounds == static_cast<const BeginFilterDisplayItem&>(other).m_bounds
            && m_origin == static_cast<const BeginFilterDisplayItem&>(other).m_origin;
    }
#endif

    // FIXME: m_imageFilter should be replaced with m_webFilterOperations when copying data to the compositor.
    sk_sp<SkImageFilter> m_imageFilter;
    CompositorFilterOperations m_compositorFilterOperations;
    const FloatRect m_bounds;
    const FloatPoint m_origin;
};

class PLATFORM_EXPORT EndFilterDisplayItem final : public PairedEndDisplayItem {
public:
    EndFilterDisplayItem(const DisplayItemClient& client)
        : PairedEndDisplayItem(client, EndFilter, sizeof(*this)) { }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return otherType == BeginFilter; }
#endif
};

} // namespace blink

#endif // FilterDisplayItem_h
