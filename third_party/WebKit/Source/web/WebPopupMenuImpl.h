/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPopupMenuImpl_h
#define WebPopupMenuImpl_h

#include "public/platform/WebContentLayerClient.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebSize.h"
#include "public/web/WebPopupMenu.h"
#include "web/PopupContainerClient.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class DisplayItemList;
class WebContentLayer;
class WebGestureEvent;
class WebKeyboardEvent;
class WebLayerTreeView;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebTouchEvent;
struct WebRect;

class WebPopupMenuImpl : public WebPopupMenu, public PopupContainerClient, public WebContentLayerClient, public RefCounted<WebPopupMenuImpl> {
    WTF_MAKE_FAST_ALLOCATED(WebPopupMenuImpl);
public:
    // WebWidget functions:
    virtual void close() override final;
    virtual WebSize size() override final { return m_size; }
    virtual void willStartLiveResize() override final;
    virtual void resize(const WebSize&) override final;
    virtual void willEndLiveResize() override final;
    virtual void beginFrame(const WebBeginFrameArgs&) override final;
    virtual void layout() override final;
    virtual void paint(WebCanvas*, const WebRect&) override final;
    virtual void themeChanged() override final;
    virtual bool handleInputEvent(const WebInputEvent&) override final;
    virtual void mouseCaptureLost() override final;
    virtual void setFocus(bool enable) override final;
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart, int selectionEnd) override final;
    virtual bool confirmComposition() override final;
    virtual bool confirmComposition(ConfirmCompositionBehavior selectionBehavior) override final;
    virtual bool confirmComposition(const WebString& text) override final;
    virtual bool compositionRange(size_t* location, size_t* length) override final;
    virtual bool caretOrSelectionRange(size_t* location, size_t* length) override final;
    virtual void setTextDirection(WebTextDirection) override final;
    virtual bool isAcceleratedCompositingActive() const override final { return false; }
    virtual bool isPopupMenu() const override final { return true; }
    virtual void willCloseLayerTreeView() override final;

    // WebContentLayerClient
    virtual void paintContents(WebCanvas*, const WebRect& clip, WebContentLayerClient::PaintingControlSetting = PaintDefaultBehavior) override final;
    virtual void paintContents(WebDisplayItemList*, const WebRect& clip, WebContentLayerClient::PaintingControlSetting = PaintDefaultBehavior) override final;

    // WebPopupMenuImpl
    void initialize(PopupContainer* widget, const WebRect& bounds);

    WebWidgetClient* client() { return m_client; }

    void handleMouseMove(const WebMouseEvent&);
    void handleMouseLeave(const WebMouseEvent&);
    void handleMouseDown(const WebMouseEvent&);
    void handleMouseUp(const WebMouseEvent&);
    void handleMouseDoubleClick(const WebMouseEvent&);
    void handleMouseWheel(const WebMouseWheelEvent&);
    bool handleGestureEvent(const WebGestureEvent&);
    bool handleTouchEvent(const WebTouchEvent&);
    bool handleKeyEvent(const WebKeyboardEvent&);

   protected:
    friend class WebPopupMenu; // For WebPopupMenu::create.
    friend class WTF::RefCounted<WebPopupMenuImpl>;

    explicit WebPopupMenuImpl(WebWidgetClient*);
    ~WebPopupMenuImpl();

    // HostWindow methods:
    virtual void invalidateRect(const IntRect&) override final;
    virtual void scheduleAnimation() override final;
    virtual IntRect viewportToScreen(const IntRect&) const override final;

    // PopupContainerClient methods:
    virtual void popupClosed(PopupContainer*) override final;
    void invalidateDisplayItemClient(DisplayItemClient) override final;
    void invalidateAllDisplayItems() override final;

    DisplayItemList* displayItemList();

    WebWidgetClient* m_client;
    WebSize m_size;

    WebLayerTreeView* m_layerTreeView;
    OwnPtr<WebContentLayer> m_rootLayer;

    WebPoint m_lastMousePosition;

    // This is a non-owning ref. The popup will notify us via popupClosed()
    // before it is destroyed.
    PopupContainer* m_widget;

    OwnPtr<DisplayItemList> m_displayItemList;
};

DEFINE_TYPE_CASTS(WebPopupMenuImpl, WebWidget, widget, widget->isPopupMenu(), widget.isPopupMenu());
// WebPopupMenuImpl is the only implementation of PopupContainerClient, so
// no need for further checking.
DEFINE_TYPE_CASTS(WebPopupMenuImpl, PopupContainerClient, client, true, true);

} // namespace blink

#endif
