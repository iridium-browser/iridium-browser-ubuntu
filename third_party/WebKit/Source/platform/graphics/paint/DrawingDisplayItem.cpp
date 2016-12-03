// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DrawingDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkPictureAnalyzer.h"

#if ENABLE(ASSERT)
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#endif

namespace blink {

void DrawingDisplayItem::replay(GraphicsContext& context) const
{
    if (m_picture)
        context.drawPicture(m_picture.get());
}

void DrawingDisplayItem::appendToWebDisplayItemList(const IntRect& visualRect, WebDisplayItemList* list) const
{
    if (m_picture)
        list->appendDrawingItem(visualRect, toSkSp(m_picture));
}

bool DrawingDisplayItem::drawsContent() const
{
    return m_picture.get();
}

void DrawingDisplayItem::analyzeForGpuRasterization(SkPictureGpuAnalyzer& analyzer) const
{
    analyzer.analyzePicture(m_picture.get());
}

#ifndef NDEBUG
void DrawingDisplayItem::dumpPropertiesAsDebugString(StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    if (m_picture) {
        stringBuilder.append(String::format(", rect: [%f,%f %fx%f]",
            m_picture->cullRect().x(), m_picture->cullRect().y(),
            m_picture->cullRect().width(), m_picture->cullRect().height()));
    }
}
#endif

#if ENABLE(ASSERT)
static bool picturesEqual(const SkPicture* picture1, const SkPicture* picture2)
{
    if (picture1->approximateOpCount() != picture2->approximateOpCount())
        return false;

    SkDynamicMemoryWStream picture1Serialized;
    picture1->serialize(&picture1Serialized);
    SkDynamicMemoryWStream picture2Serialized;
    picture2->serialize(&picture2Serialized);
    if (picture1Serialized.bytesWritten() != picture2Serialized.bytesWritten())
        return false;

    RefPtr<SkData> data1 = adoptRef(picture1Serialized.copyToData());
    RefPtr<SkData> data2 = adoptRef(picture2Serialized.copyToData());
    return data1->equals(data2.get());
}

static SkBitmap pictureToBitmap(const SkPicture* picture)
{
    SkBitmap bitmap;
    SkRect rect = picture->cullRect();
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
    SkCanvas canvas(bitmap);
    canvas.translate(-rect.x(), -rect.y());
    canvas.drawPicture(picture);
    return bitmap;
}

static bool bitmapsEqual(const SkPicture* picture1, const SkPicture* picture2)
{
    SkRect rect = picture1->cullRect();
    if (rect != picture2->cullRect())
        return false;

    SkBitmap bitmap1 = pictureToBitmap(picture1);
    SkBitmap bitmap2 = pictureToBitmap(picture2);
    bitmap1.lockPixels();
    bitmap2.lockPixels();
    int mismatchCount = 0;
    const int maxMismatches = 10;
    for (int y = 0; y < rect.height() && mismatchCount < maxMismatches; ++y) {
        for (int x = 0; x < rect.width() && mismatchCount < maxMismatches; ++x) {
            SkColor pixel1 = bitmap1.getColor(x, y);
            SkColor pixel2 = bitmap2.getColor(x, y);
            if (pixel1 != pixel2) {
                LOG(ERROR) << "x=" << x << " y=" << y << " " << std::hex << pixel1 << " vs " << std::hex << pixel2;
                ++mismatchCount;
            }
        }
    }
    bitmap1.unlockPixels();
    bitmap2.unlockPixels();
    return !mismatchCount;
}

bool DrawingDisplayItem::equals(const DisplayItem& other) const
{
    if (!DisplayItem::equals(other))
        return false;

    const SkPicture* picture = this->picture();
    const SkPicture* otherPicture = static_cast<const DrawingDisplayItem&>(other).picture();

    if (!picture && !otherPicture)
        return true;
    if (!picture || !otherPicture)
        return false;

    if (picturesEqual(picture, otherPicture))
        return true;

    // Sometimes the client may produce different pictures for the same visual result
    // which should be treated as equal.
    return bitmapsEqual(picture, otherPicture);
}
#endif // ENABLE(ASSERT)

} // namespace blink
