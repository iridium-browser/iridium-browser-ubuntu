// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPathDisplayItem_h
#define ClipPathDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/graphics/Path.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "third_party/skia/include/core/SkPath.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class PLATFORM_EXPORT BeginClipPathDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginClipPathDisplayItem(const DisplayItemClientWrapper& client, const Path& clipPath)
        : PairedBeginDisplayItem(client, BeginClipPath, sizeof(*this))
        , m_clipPath(clipPath.skPath()) { }

    void replay(GraphicsContext&) override;
    void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
    const SkPath m_clipPath;
#ifndef NDEBUG
    void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
};

class PLATFORM_EXPORT EndClipPathDisplayItem final : public PairedEndDisplayItem {
public:
    EndClipPathDisplayItem(const DisplayItemClientWrapper& client)
        : PairedEndDisplayItem(client, EndClipPath, sizeof(*this)) { }

    void replay(GraphicsContext&) override;
    void appendToWebDisplayItemList(WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return otherType == BeginClipPath; }
#endif
};

} // namespace blink

#endif // ClipPathDisplayItem_h
