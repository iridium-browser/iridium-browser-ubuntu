// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/ClipRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

ClipRecorder::ClipRecorder(GraphicsContext& context, const DisplayItemClientWrapper& client, DisplayItem::Type type, const LayoutRect& clipRect, SkRegion::Op operation)
    : m_client(client)
    , m_context(context)
    , m_type(type)
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        if (m_context.displayItemList()->displayItemConstructionIsDisabled())
            return;
        m_context.displayItemList()->add(ClipDisplayItem::create(m_client, type, pixelSnappedIntRect(clipRect), operation));
    } else {
        ClipDisplayItem clipDisplayItem(m_client, type, pixelSnappedIntRect(clipRect), operation);
        clipDisplayItem.replay(m_context);
    }
}

ClipRecorder::~ClipRecorder()
{
    DisplayItem::Type endType = DisplayItem::clipTypeToEndClipType(m_type);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_context.displayItemList());
        if (m_context.displayItemList()->displayItemConstructionIsDisabled())
            return;
        m_context.displayItemList()->add(EndClipDisplayItem::create(m_client, endType));
    } else {
        EndClipDisplayItem endClipDisplayItem(m_client, endType);
        endClipDisplayItem.replay(m_context);
    }
}

} // namespace blink
