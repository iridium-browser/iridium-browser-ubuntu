/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LinkHighlightImpl_h
#define LinkHighlightImpl_h

#include "platform/graphics/LinkHighlight.h"
#include "platform/graphics/Path.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCompositorAnimationDelegate.h"
#include "public/platform/WebCompositorAnimationPlayer.h"
#include "public/platform/WebCompositorAnimationPlayerClient.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebContentLayerClient.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"

namespace blink {

class GraphicsLayer;
class LayoutBoxModelObject;
class Node;
class WebContentLayer;
class WebLayer;
class WebViewImpl;

class LinkHighlightImpl final : public LinkHighlight
    , public WebContentLayerClient
    , public WebCompositorAnimationDelegate
    , public WebCompositorAnimationPlayerClient {
public:
    static PassOwnPtr<LinkHighlightImpl> create(Node*, WebViewImpl*);
    ~LinkHighlightImpl() override;

    WebContentLayer* contentLayer();
    WebLayer* clipLayer();
    void startHighlightAnimationIfNeeded();
    void updateGeometry();

    // WebContentLayerClient implementation.
    void paintContents(WebCanvas*, const WebRect& clipRect, WebContentLayerClient::PaintingControlSetting) override;
    void paintContents(WebDisplayItemList*, const WebRect& clipRect, WebContentLayerClient::PaintingControlSetting) override;

    // WebCompositorAnimationDelegate implementation.
    void notifyAnimationStarted(double monotonicTime, int group) override;
    void notifyAnimationFinished(double monotonicTime, int group) override;

    // LinkHighlight implementation.
    void invalidate() override;
    WebLayer* layer() override;
    void clearCurrentGraphicsLayer() override;

    // WebCompositorAnimationPlayerClient implementation.
    WebCompositorAnimationPlayer* compositorPlayer() const override;

    GraphicsLayer* currentGraphicsLayerForTesting() const { return m_currentGraphicsLayer; }

private:
    LinkHighlightImpl(Node*, WebViewImpl*);

    void releaseResources();
    void computeQuads(const Node&, Vector<FloatQuad>&) const;

    void attachLinkHighlightToCompositingLayer(const LayoutBoxModelObject* paintInvalidationContainer);
    void clearGraphicsLayerLinkHighlightPointer();
    // This function computes the highlight path, and returns true if it has changed
    // size since the last call to this function.
    bool computeHighlightLayerPathAndPosition(const LayoutBoxModelObject*);

    OwnPtr<WebContentLayer> m_contentLayer;
    OwnPtr<WebLayer> m_clipLayer;
    Path m_path;

    RefPtrWillBePersistent<Node> m_node;
    WebViewImpl* m_owningWebViewImpl;
    GraphicsLayer* m_currentGraphicsLayer;
    OwnPtr<WebCompositorAnimationPlayer> m_compositorPlayer;

    bool m_geometryNeedsUpdate;
    bool m_isAnimating;
    double m_startTime;
};

} // namespace blink

#endif // LinkHighlightImpl_h
