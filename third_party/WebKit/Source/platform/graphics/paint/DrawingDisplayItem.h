// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingDisplayItem_h
#define DrawingDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

class PLATFORM_EXPORT DrawingDisplayItem final : public DisplayItem {
public:
    DrawingDisplayItem(const DisplayItemClient& client, Type type, PassRefPtr<const SkPicture> picture, bool knownToBeOpaque = false)
        : DisplayItem(client, type, sizeof(*this))
        , m_picture(picture && picture->approximateOpCount() ? picture : nullptr)
        , m_knownToBeOpaque(knownToBeOpaque)
    {
        DCHECK(isDrawingType(type));
    }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;
    bool drawsContent() const override;

    const SkPicture* picture() const { return m_picture.get(); }

    bool knownToBeOpaque() const { DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled()); return m_knownToBeOpaque; }

    void analyzeForGpuRasterization(SkPictureGpuAnalyzer&) const override;

#if DCHECK_IS_ON()
    bool equals(const DisplayItem& other) const final;
#endif

private:
#ifndef NDEBUG
    void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif

    RefPtr<const SkPicture> m_picture;

    // True if there are no transparent areas. Only used for SlimmingPaintV2.
    const bool m_knownToBeOpaque;
};

} // namespace blink

#endif // DrawingDisplayItem_h
