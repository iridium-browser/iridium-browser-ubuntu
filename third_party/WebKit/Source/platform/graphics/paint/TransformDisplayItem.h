// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransformDisplayItem_h
#define TransformDisplayItem_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginTransformDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginTransformDisplayItem(const DisplayItemClientWrapper& client, const AffineTransform& transform)
        : PairedBeginDisplayItem(client, BeginTransform, sizeof(*this))
        , m_transform(transform) { }

    void replay(GraphicsContext&) override;
    void appendToWebDisplayItemList(WebDisplayItemList*) const override;

    const AffineTransform& transform() const { return m_transform; }

private:
#ifndef NDEBUG
    void dumpPropertiesAsDebugString(WTF::StringBuilder&) const final;
#endif

private:
    const AffineTransform m_transform;
};

class PLATFORM_EXPORT EndTransformDisplayItem final : public PairedEndDisplayItem {
public:
    EndTransformDisplayItem(const DisplayItemClientWrapper& client)
        : PairedEndDisplayItem(client, EndTransform, sizeof(*this)) { }

    void replay(GraphicsContext&) override;
    void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return otherType == BeginTransform; }
#endif
};

} // namespace blink

#endif // TransformDisplayItem_h
