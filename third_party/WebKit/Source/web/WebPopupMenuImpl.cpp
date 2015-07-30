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

#include "config.h"
#include "web/WebPopupMenuImpl.h"

#include "core/frame/FrameView.h"
#include "platform/Cursor.h"
#include "platform/NotImplemented.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebRect.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebRange.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWidgetClient.h"
#include "web/PopupContainer.h"
#include "web/PopupMenuChromium.h"
#include "web/WebInputEventConversion.h"

namespace blink {

// WebPopupMenu ---------------------------------------------------------------

WebPopupMenu* WebPopupMenu::create(WebWidgetClient* client)
{
    // Pass the WebPopupMenuImpl's self-reference to the caller.
    return adoptRef(new WebPopupMenuImpl(client)).leakRef();
}

// WebWidget ------------------------------------------------------------------

WebPopupMenuImpl::WebPopupMenuImpl(WebWidgetClient* client)
    : m_client(client)
    , m_layerTreeView(0)
    // Set to impossible point so we always get the first mouse position.
    , m_lastMousePosition(WebPoint(-1, -1))
    , m_widget(0)
{
}

WebPopupMenuImpl::~WebPopupMenuImpl()
{
    if (m_widget)
        m_widget->setClient(0);
}

void WebPopupMenuImpl::willCloseLayerTreeView()
{
    m_layerTreeView = 0;
}

void WebPopupMenuImpl::initialize(PopupContainer* widget, const WebRect& bounds)
{
    m_widget = widget;
    m_widget->setClient(this);

    if (!m_client)
        return;
    m_client->setWindowRect(bounds);
    m_client->show(WebNavigationPolicy()); // Policy is ignored.

    m_client->initializeLayerTreeView();
    m_layerTreeView = m_client->layerTreeView();
    if (m_layerTreeView) {
        m_layerTreeView->setVisible(true);
        m_layerTreeView->setDeviceScaleFactor(m_client->deviceScaleFactor());
        m_rootLayer = adoptPtr(Platform::current()->compositorSupport()->createContentLayer(this));
        m_rootLayer->layer()->setBounds(m_size);
        // FIXME: Legacy LCD behavior (http://crbug.com/436821), but are we always guaranteed to be opaque?
        m_rootLayer->layer()->setOpaque(true);
        m_layerTreeView->setRootLayer(*m_rootLayer->layer());
    }
}

void WebPopupMenuImpl::handleMouseMove(const WebMouseEvent& event)
{
    // Don't send mouse move messages if the mouse hasn't moved.
    if (event.x != m_lastMousePosition.x || event.y != m_lastMousePosition.y) {
        m_lastMousePosition = WebPoint(event.x, event.y);
        m_widget->handleMouseMoveEvent(PlatformMouseEventBuilder(m_widget, event));

        // We cannot call setToolTipText() in PopupContainer, because PopupContainer is in WebCore, and we cannot refer to WebKit from Webcore.
        PopupContainer* container = static_cast<PopupContainer*>(m_widget);
        client()->setToolTipText(container->getSelectedItemToolTip(), toWebTextDirection(container->menuStyle().textDirection()));
    }
}

void WebPopupMenuImpl::handleMouseLeave(const WebMouseEvent& event)
{
    m_widget->handleMouseMoveEvent(PlatformMouseEventBuilder(m_widget, event));
}

void WebPopupMenuImpl::handleMouseDown(const WebMouseEvent& event)
{
    m_widget->handleMouseDownEvent(PlatformMouseEventBuilder(m_widget, event));
}

void WebPopupMenuImpl::handleMouseUp(const WebMouseEvent& event)
{
    mouseCaptureLost();
    m_widget->handleMouseReleaseEvent(PlatformMouseEventBuilder(m_widget, event));
}

void WebPopupMenuImpl::handleMouseWheel(const WebMouseWheelEvent& event)
{
    m_widget->handleWheelEvent(PlatformWheelEventBuilder(m_widget, event));
}

bool WebPopupMenuImpl::handleGestureEvent(const WebGestureEvent& event)
{
    return m_widget->handleGestureEvent(PlatformGestureEventBuilder(m_widget, event));
}

bool WebPopupMenuImpl::handleTouchEvent(const WebTouchEvent& event)
{

    PlatformTouchEventBuilder touchEventBuilder(m_widget, event);
    bool defaultPrevented(m_widget->handleTouchEvent(touchEventBuilder));
    return defaultPrevented;
}

bool WebPopupMenuImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    return m_widget->handleKeyEvent(PlatformKeyboardEventBuilder(event));
}

// WebWidget -------------------------------------------------------------------

void WebPopupMenuImpl::close()
{
    if (m_widget)
        m_widget->hide();

    m_client = 0;

    deref(); // Balances ref() from WebPopupMenu::create.
}

void WebPopupMenuImpl::willStartLiveResize()
{
}

void WebPopupMenuImpl::resize(const WebSize& newSize)
{
    if (m_size == newSize)
        return;
    m_size = newSize;

    if (m_widget) {
        IntRect newGeometry(0, 0, m_size.width, m_size.height);
        m_widget->setFrameRect(newGeometry);
    }

    if (m_client) {
        WebRect damagedRect(0, 0, m_size.width, m_size.height);
        m_client->didInvalidateRect(damagedRect);
    }

    if (m_rootLayer)
        m_rootLayer->layer()->setBounds(newSize);
}

void WebPopupMenuImpl::willEndLiveResize()
{
}

void WebPopupMenuImpl::beginFrame(const WebBeginFrameArgs&)
{
}

void WebPopupMenuImpl::layout()
{
}

void WebPopupMenuImpl::paintContents(WebCanvas* canvas, const WebRect& rect, WebContentLayerClient::PaintingControlSetting paintingControl)
{
    if (!m_widget)
        return;

    OwnPtr<GraphicsContext> context;
    GraphicsContext::DisabledMode disabledMode = GraphicsContext::NothingDisabled;
    if (paintingControl == PaintingControlSetting::DisplayListPaintingDisabled
        || paintingControl == PaintingControlSetting::DisplayListConstructionDisabled)
        disabledMode = GraphicsContext::FullyDisabled;

    DisplayItemList* itemList = displayItemList();
    if (itemList) {
        context = adoptPtr(new GraphicsContext(itemList, disabledMode));
        itemList->setDisplayItemConstructionIsDisabled(paintingControl == PaintingControlSetting::DisplayListConstructionDisabled);
    } else {
        context = GraphicsContext::deprecatedCreateWithCanvas(canvas, disabledMode);
    }
    m_widget->paint(context.get(), rect);

    if (itemList)
        itemList->commitNewDisplayItems();
}

void WebPopupMenuImpl::paintContents(WebDisplayItemList* webDisplayItemList, const WebRect& clip, WebContentLayerClient::PaintingControlSetting paintingControl)
{
    if (!m_widget)
        return;

    if (paintingControl != WebContentLayerClient::PaintDefaultBehavior && m_displayItemList)
        m_displayItemList->invalidateAll();

    paintContents(static_cast<WebCanvas*>(nullptr), clip, paintingControl);

    RELEASE_ASSERT(m_displayItemList);
    for (const auto& item : m_displayItemList->displayItems())
        item->appendToWebDisplayItemList(webDisplayItemList);
}

void WebPopupMenuImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    if (!m_widget)
        return;

    if (!rect.isEmpty()) {
        OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
        float scaleFactor = m_client->deviceScaleFactor();
        context->scale(scaleFactor, scaleFactor);
        m_widget->paint(context.get(), rect);
    }
}

void WebPopupMenuImpl::themeChanged()
{
    notImplemented();
}

bool WebPopupMenuImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    if (!m_widget)
        return false;

    // FIXME: WebKit seems to always return false on mouse events methods. For
    // now we'll assume it has processed them (as we are only interested in
    // whether keyboard events are processed).
    switch (inputEvent.type) {
    case WebInputEvent::MouseMove:
        handleMouseMove(static_cast<const WebMouseEvent&>(inputEvent));
        return true;

    case WebInputEvent::MouseLeave:
        handleMouseLeave(static_cast<const WebMouseEvent&>(inputEvent));
        return true;

    case WebInputEvent::MouseWheel:
        handleMouseWheel(static_cast<const WebMouseWheelEvent&>(inputEvent));
        return true;

    case WebInputEvent::MouseDown:
        handleMouseDown(static_cast<const WebMouseEvent&>(inputEvent));
        return true;

    case WebInputEvent::MouseUp:
        handleMouseUp(static_cast<const WebMouseEvent&>(inputEvent));
        return true;

    // In Windows, RawKeyDown only has information about the physical key, but
    // for "selection", we need the information about the character the key
    // translated into. For English, the physical key value and the character
    // value are the same, hence, "selection" works for English. But for other
    // languages, such as Hebrew, the character value is different from the
    // physical key value. Thus, without accepting Char event type which
    // contains the key's character value, the "selection" won't work for
    // non-English languages, such as Hebrew.
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
    case WebInputEvent::Char:
        return handleKeyEvent(static_cast<const WebKeyboardEvent&>(inputEvent));

    case WebInputEvent::TouchStart:
    case WebInputEvent::TouchMove:
    case WebInputEvent::TouchEnd:
    case WebInputEvent::TouchCancel:
        return handleTouchEvent(static_cast<const WebTouchEvent&>(inputEvent));

    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureFlingStart:
    case WebInputEvent::GestureFlingCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureShowPress:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureDoubleTap:
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap:
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
        return handleGestureEvent(static_cast<const WebGestureEvent&>(inputEvent));

    case WebInputEvent::Undefined:
    case WebInputEvent::MouseEnter:
    case WebInputEvent::ContextMenu:
        return false;
    }
    return false;
}

void WebPopupMenuImpl::mouseCaptureLost()
{
}

void WebPopupMenuImpl::setFocus(bool)
{
}

bool WebPopupMenuImpl::setComposition(const WebString&, const WebVector<WebCompositionUnderline>&, int, int)
{
    return false;
}

bool WebPopupMenuImpl::confirmComposition()
{
    return false;
}

bool WebPopupMenuImpl::confirmComposition(ConfirmCompositionBehavior)
{
    return false;
}

bool WebPopupMenuImpl::confirmComposition(const WebString&)
{
    return false;
}

bool WebPopupMenuImpl::compositionRange(size_t* location, size_t* length)
{
    *location = 0;
    *length = 0;
    return false;
}

bool WebPopupMenuImpl::caretOrSelectionRange(size_t* location, size_t* length)
{
    *location = 0;
    *length = 0;
    return false;
}

void WebPopupMenuImpl::setTextDirection(WebTextDirection)
{
}


//-----------------------------------------------------------------------------
// HostWindow

void WebPopupMenuImpl::invalidateRect(const IntRect& paintRect)
{
    if (paintRect.isEmpty())
        return;
    if (m_client)
        m_client->didInvalidateRect(paintRect);
    if (m_rootLayer)
        m_rootLayer->layer()->invalidateRect(paintRect);
}

void WebPopupMenuImpl::scheduleAnimation()
{
}

IntRect WebPopupMenuImpl::viewportToScreen(const IntRect& rect) const
{
    notImplemented();
    return IntRect();
}

void WebPopupMenuImpl::popupClosed(PopupContainer* widget)
{
    ASSERT(widget == m_widget);
    if (m_widget) {
        m_widget->setClient(0);
        m_widget = 0;
    }
    if (m_client)
        m_client->closeWidgetSoon();
}

void WebPopupMenuImpl::invalidateDisplayItemClient(DisplayItemClient client)
{
    if (m_displayItemList) {
        ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
        m_displayItemList->invalidate(client);
    }
}

void WebPopupMenuImpl::invalidateAllDisplayItems()
{
    if (m_displayItemList) {
        ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
        m_displayItemList->invalidateAll();
    }
}

DisplayItemList* WebPopupMenuImpl::displayItemList()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return nullptr;
    if (!m_displayItemList)
        m_displayItemList = DisplayItemList::create();
    return m_displayItemList.get();
}

} // namespace blink
