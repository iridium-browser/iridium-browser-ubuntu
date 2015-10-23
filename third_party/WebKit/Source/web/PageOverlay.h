/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PageOverlay_h
#define PageOverlay_h

#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class GraphicsContext;
class WebPageOverlay;
class WebViewImpl;
class WebGraphicsContext;

// Manages a layer that is overlaid on a WebView's content.
// Clients can paint by implementing WebPageOverlay.
//
// With Slimming Paint, internal clients can extract a GraphicsContext to add
// to the DisplayItemList owned by the GraphicsLayer
class PageOverlay : public GraphicsLayerClient {
public:
    class Delegate : public GarbageCollectedFinalized<Delegate> {
    public:
        DEFINE_INLINE_VIRTUAL_TRACE() { }

        // Paints page overlay contents.
        virtual void paintPageOverlay(WebGraphicsContext*, const WebSize& webViewSize) = 0;
        virtual ~Delegate() { }
    };

    static PassOwnPtr<PageOverlay> create(WebViewImpl*, PageOverlay::Delegate*);

    ~PageOverlay();

    void update();

    GraphicsLayer* graphicsLayer() const { return m_layer.get(); }
    DisplayItemClient displayItemClient() const { return toDisplayItemClient(this); }
    String debugName() const { return "PageOverlay"; }

    // GraphicsLayerClient implementation
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) override;
    String debugName(const GraphicsLayer*) override;

private:
    PageOverlay(WebViewImpl*, PageOverlay::Delegate*);

    WebViewImpl* m_viewImpl;
    Persistent<PageOverlay::Delegate> m_delegate;
    OwnPtr<GraphicsLayer> m_layer;
};

} // namespace blink

#endif // PageOverlay_h
