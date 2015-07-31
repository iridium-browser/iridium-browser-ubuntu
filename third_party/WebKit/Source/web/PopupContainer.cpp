/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
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
#include "web/PopupContainer.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/forms/PopupMenuClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/paint/TransformRecorder.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformTouchEvent.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "public/web/WebPopupMenuInfo.h"
#include "public/web/WebPopupType.h"
#include "public/web/WebViewClient.h"
#include "web/PopupContainerClient.h"
#include "web/WebPopupMenuImpl.h"
#include "web/WebViewImpl.h"
#include <algorithm>
#include <limits>

namespace blink {

static const int borderSize = 1;

static PlatformMouseEvent constructRelativeMouseEvent(const PlatformMouseEvent& e, PopupContainer* parent, PopupListBox* child)
{
    IntPoint pos = parent->convertSelfToChild(child, e.position());

    // FIXME: This is a horrible hack since PlatformMouseEvent has no setters for x/y.
    PlatformMouseEvent relativeEvent = e;
    IntPoint& relativePos = const_cast<IntPoint&>(relativeEvent.position());
    relativePos.setX(pos.x());
    relativePos.setY(pos.y());
    return relativeEvent;
}

static PlatformWheelEvent constructRelativeWheelEvent(const PlatformWheelEvent& e, PopupContainer* parent, PopupListBox* child)
{
    IntPoint pos = parent->convertSelfToChild(child, e.position());

    // FIXME: This is a horrible hack since PlatformWheelEvent has no setters for x/y.
    PlatformWheelEvent relativeEvent = e;
    IntPoint& relativePos = const_cast<IntPoint&>(relativeEvent.position());
    relativePos.setX(pos.x());
    relativePos.setY(pos.y());
    return relativeEvent;
}

// static
PassRefPtrWillBeRawPtr<PopupContainer> PopupContainer::create(PopupMenuClient* client, bool deviceSupportsTouch)
{
    return adoptRefWillBeNoop(new PopupContainer(client, deviceSupportsTouch));
}

PopupContainer::PopupContainer(PopupMenuClient* client, bool deviceSupportsTouch)
    : m_listBox(PopupListBox::create(client, deviceSupportsTouch, this))
    , m_popupOpen(false)
    , m_client(0)
{
}

PopupContainer::~PopupContainer()
{
#if !ENABLE(OILPAN)
    if (m_listBox->parent())
        m_listBox->setParent(0);
#endif
}

DEFINE_TRACE(PopupContainer)
{
    visitor->trace(m_frameView);
    visitor->trace(m_listBox);
    Widget::trace(visitor);
}

IntRect PopupContainer::layoutAndCalculateWidgetRectInternal(IntRect widgetRectInScreen, int targetControlHeight, const IntRect& windowRect, const IntRect& screen, bool isRTL, const int rtlOffset, const int verticalOffset, const IntSize& transformOffset, PopupContent* listBox, bool& needToResizeView)
{
    ASSERT(listBox);
    if (windowRect.x() >= screen.x() && windowRect.maxX() <= screen.maxX() && (widgetRectInScreen.x() < screen.x() || widgetRectInScreen.maxX() > screen.maxX())) {
        // First, inverse the popup alignment if it does not fit the screen -
        // this might fix things (or make them better).
        IntRect inverseWidgetRectInScreen = widgetRectInScreen;
        inverseWidgetRectInScreen.setX(inverseWidgetRectInScreen.x() + (isRTL ? -rtlOffset : rtlOffset));
        inverseWidgetRectInScreen.setY(inverseWidgetRectInScreen.y() + (isRTL ? -verticalOffset : verticalOffset));
        unsigned originalCutoff = std::max(screen.x() - widgetRectInScreen.x(), 0) + std::max(widgetRectInScreen.maxX() - screen.maxX(), 0);
        unsigned inverseCutoff = std::max(screen.x() - inverseWidgetRectInScreen.x(), 0) + std::max(inverseWidgetRectInScreen.maxX() - screen.maxX(), 0);

        // Accept the inverse popup alignment if the trimmed content gets
        // shorter than that in the original alignment case.
        if (inverseCutoff < originalCutoff)
            widgetRectInScreen = inverseWidgetRectInScreen;

        if (widgetRectInScreen.x() < screen.x()) {
            widgetRectInScreen.setWidth(widgetRectInScreen.maxX() - screen.x());
            widgetRectInScreen.setX(screen.x());
            listBox->setMaxWidthAndLayout(std::max(widgetRectInScreen.width() - borderSize * 2, 0));
        } else if (widgetRectInScreen.maxX() > screen.maxX()) {
            widgetRectInScreen.setWidth(screen.maxX() - widgetRectInScreen.x());
            listBox->setMaxWidthAndLayout(std::max(widgetRectInScreen.width() - borderSize * 2, 0));
        }
    }

    // Calculate Y axis size.
    if (widgetRectInScreen.maxY() > static_cast<int>(screen.maxY())) {
        if (widgetRectInScreen.y() - widgetRectInScreen.height() - targetControlHeight - transformOffset.height() > 0) {
            // There is enough room to open upwards.
            widgetRectInScreen.move(-transformOffset.width(), -(widgetRectInScreen.height() + targetControlHeight + transformOffset.height()));
        } else {
            // Figure whether upwards or downwards has more room and set the
            // maximum number of items.
            int spaceAbove = widgetRectInScreen.y() - targetControlHeight + transformOffset.height();
            int spaceBelow = screen.maxY() - widgetRectInScreen.y();
            if (spaceAbove > spaceBelow)
                listBox->setMaxHeight(spaceAbove);
            else
                listBox->setMaxHeight(spaceBelow);
            listBox->layout();
            needToResizeView = true;
            widgetRectInScreen.setHeight(listBox->popupContentHeight() + borderSize * 2);
            // Move WebWidget upwards if necessary.
            if (spaceAbove > spaceBelow)
                widgetRectInScreen.move(-transformOffset.width(), -(widgetRectInScreen.height() + targetControlHeight + transformOffset.height()));
        }
    }
    return widgetRectInScreen;
}

IntRect PopupContainer::layoutAndCalculateWidgetRect(int targetControlHeight, const IntSize& transformOffset, const IntPoint& popupInitialCoordinate)
{
    // Reset the max width and height to their default values, they will be
    // recomputed below if necessary.
    m_listBox->setMaxHeight(PopupListBox::defaultMaxHeight);
    m_listBox->setMaxWidth(std::numeric_limits<int>::max());

    // Lay everything out to figure out our preferred size, then tell the view's
    // WidgetClient about it. It should assign us a client.
    m_listBox->layout();
    fitToListBox();
    bool isRTL = this->isRTL();

    // Compute the starting x-axis for a normal RTL or right-aligned LTR
    // dropdown. For those, the right edge of dropdown box should be aligned
    // with the right edge of <select>/<input> element box, and the dropdown box
    // should be expanded to the left if more space is needed.
    // m_originalFrameRect.width() is the width of the target <select>/<input>
    // element.
    int rtlOffset = m_controlPosition.p2().x() - m_controlPosition.p1().x() - (m_listBox->width() + borderSize * 2);
    int rightOffset = isRTL ? rtlOffset : 0;

    // Compute the y-axis offset between the bottom left and bottom right
    // points. If the <select>/<input> is transformed, they are not the same.
    int verticalOffset = - m_controlPosition.p4().y() + m_controlPosition.p3().y();
    int verticalForRTLOffset = isRTL ? verticalOffset : 0;

    // Assume m_listBox size is already calculated.
    IntSize targetSize(m_listBox->width() + borderSize * 2, m_listBox->height() + borderSize * 2);

    IntRect widgetRectInScreen;
    // If the popup would extend past the bottom of the screen, open upwards
    // instead.
    IntRect screen = chromeClient().screenInfo().availableRect;
    // Use popupInitialCoordinate.x() + rightOffset because RTL position
    // needs to be considered.
    float pageScaleFactor = m_frameView->frame().page()->pageScaleFactor();
    int popupX = round((popupInitialCoordinate.x() + rightOffset) * pageScaleFactor);
    int popupY = round((popupInitialCoordinate.y() + verticalForRTLOffset) * pageScaleFactor);
    widgetRectInScreen = chromeClient().viewportToScreen(IntRect(popupX, popupY, targetSize.width(), targetSize.height()));

    // If we have multiple screens and the browser rect is in one screen, we
    // have to clip the window width to the screen width.
    // When clipping, we also need to set a maximum width for the list box.
    IntRect windowRect = chromeClient().windowRect();

    bool needToResizeView = false;
    widgetRectInScreen = layoutAndCalculateWidgetRectInternal(widgetRectInScreen, targetControlHeight, windowRect, screen, isRTL, rtlOffset, verticalOffset, transformOffset, m_listBox.get(), needToResizeView);
    if (needToResizeView)
        fitToListBox();

    return widgetRectInScreen;
}

void PopupContainer::showPopup(FrameView* view)
{
    m_frameView = view;
    m_listBox->m_focusedElement = m_frameView->frame().document()->focusedElement();

    IntSize transformOffset(m_controlPosition.p4().x() - m_controlPosition.p1().x(), m_controlPosition.p4().y() - m_controlPosition.p1().y() - m_controlSize.height());
    popupOpened(layoutAndCalculateWidgetRect(m_controlSize.height(), transformOffset, roundedIntPoint(m_controlPosition.p4())));
    m_popupOpen = true;

    if (!m_listBox->parent())
        m_listBox->setParent(this);

    m_listBox->scrollToRevealSelection();

    invalidate();
}

void PopupContainer::hidePopup()
{
    m_listBox->cancel();
}

void PopupContainer::notifyPopupHidden()
{
    if (!m_popupOpen)
        return;
    m_popupOpen = false;

    // With Oilpan, we cannot assume that the FrameView's LocalFrame's
    // page is still available, as the LocalFrame itself may have been
    // detached from its FrameHost by now.
    //
    // So, if a popup menu is left in an open/shown state when
    // finalized, the PopupMenu implementation of this container's
    // listbox will hide itself when destructed, delivering the
    // notifyPopupHidden() notification in the process & ending up here.
    // If the LocalFrame has been detached already -- done when its
    // HTMLFrameOwnerElement frame owner is detached as part of being
    // torn down -- the connection to the FrameHost has been snipped &
    // there's no page. Hence the null check.
    //
    // In a non-Oilpan setting, the LayoutMenuList that controls/owns
    // the PopupMenuChromium object and this PopupContainer is torn
    // down and destructed before the frame and frame owner, hence the
    // page will always be available in that setting and this will
    // not be an issue.
    if (WebViewImpl* webView = WebViewImpl::fromPage(m_frameView->frame().page()))
        webView->popupClosed(this);
}

void PopupContainer::fitToListBox()
{
    // Place the listbox within our border.
    m_listBox->move(borderSize, borderSize);

    // Size ourselves to contain listbox + border.
    resize(m_listBox->width() + borderSize * 2, m_listBox->height() + borderSize * 2);
    invalidate();
}

bool PopupContainer::handleMouseDownEvent(const PlatformMouseEvent& event)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    return m_listBox->handleMouseDownEvent(
        constructRelativeMouseEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleMouseMoveEvent(const PlatformMouseEvent& event)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    return m_listBox->handleMouseMoveEvent(
        constructRelativeMouseEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleMouseReleaseEvent(const PlatformMouseEvent& event)
{
    RefPtrWillBeRawPtr<PopupContainer> protect(this);
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    return m_listBox->handleMouseReleaseEvent(
        constructRelativeMouseEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleWheelEvent(const PlatformWheelEvent& event)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    return m_listBox->handleWheelEvent(
        constructRelativeWheelEvent(event, this, m_listBox.get()));
}

bool PopupContainer::handleTouchEvent(const PlatformTouchEvent&)
{
    return false;
}

// FIXME: Refactor this code to share functionality with
// EventHandler::handleGestureEvent.
bool PopupContainer::handleGestureEvent(const PlatformGestureEvent& gestureEvent)
{
    switch (gestureEvent.type()) {
    case PlatformEvent::GestureTap: {
        PlatformMouseEvent fakeMouseMove(gestureEvent.position(), gestureEvent.globalPosition(), NoButton, PlatformEvent::MouseMoved, /* clickCount */ 1, gestureEvent.shiftKey(), gestureEvent.ctrlKey(), gestureEvent.altKey(), gestureEvent.metaKey(), PlatformMouseEvent::FromTouch, gestureEvent.timestamp());
        PlatformMouseEvent fakeMouseDown(gestureEvent.position(), gestureEvent.globalPosition(), LeftButton, PlatformEvent::MousePressed, /* clickCount */ 1, gestureEvent.shiftKey(), gestureEvent.ctrlKey(), gestureEvent.altKey(), gestureEvent.metaKey(), PlatformMouseEvent::FromTouch, gestureEvent.timestamp());
        PlatformMouseEvent fakeMouseUp(gestureEvent.position(), gestureEvent.globalPosition(), LeftButton, PlatformEvent::MouseReleased, /* clickCount */ 1, gestureEvent.shiftKey(), gestureEvent.ctrlKey(), gestureEvent.altKey(), gestureEvent.metaKey(), PlatformMouseEvent::FromTouch, gestureEvent.timestamp());
        // handleMouseMoveEvent(fakeMouseMove);
        handleMouseDownEvent(fakeMouseDown);
        handleMouseReleaseEvent(fakeMouseUp);
        return true;
    }
    case PlatformEvent::GestureScrollUpdate: {
        PlatformWheelEvent syntheticWheelEvent(gestureEvent.position(), gestureEvent.globalPosition(), gestureEvent.deltaX(), gestureEvent.deltaY(), gestureEvent.deltaX() / 120.0f, gestureEvent.deltaY() / 120.0f, ScrollByPixelWheelEvent, gestureEvent.shiftKey(), gestureEvent.ctrlKey(), gestureEvent.altKey(), gestureEvent.metaKey());
        handleWheelEvent(syntheticWheelEvent);
        return true;
    }
    case PlatformEvent::GestureScrollBegin:
    case PlatformEvent::GestureScrollEnd:
    case PlatformEvent::GestureTapDown:
    case PlatformEvent::GestureShowPress:
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

bool PopupContainer::handleKeyEvent(const PlatformKeyboardEvent& event)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    return m_listBox->handleKeyEvent(event);
}

void PopupContainer::hide()
{
    m_listBox->cancel();
}

void PopupContainer::paint(GraphicsContext* gc, const IntRect& paintRect)
{
    TransformRecorder transformRecorder(*gc, *this, AffineTransform::translation(x(),  y()));
    IntRect adjustedPaintRect = intersection(paintRect, frameRect());
    adjustedPaintRect.moveBy(-location());

    m_listBox->paint(gc, adjustedPaintRect);
    paintBorder(gc, adjustedPaintRect);
}

void PopupContainer::paintBorder(GraphicsContext* gc, const IntRect& rect)
{
    DrawingRecorder drawingRecorder(*gc, *this, DisplayItem::PopupContainerBorder, boundsRect());
    if (drawingRecorder.canUseCachedDrawing())
        return;

    // FIXME: Where do we get the border color from?
    Color borderColor(127, 157, 185);

    gc->setStrokeStyle(SolidStroke);
    gc->setStrokeThickness(borderSize);
    gc->setStrokeColor(borderColor);

    FloatRect borderRect(boundsRect());
    borderRect.inflate(-gc->strokeThickness() / 2);
    gc->strokeRect(borderRect);
}

ChromeClient& PopupContainer::chromeClient()
{
    return m_frameView->frame().page()->chrome().client();
}

void PopupContainer::showInRect(const FloatQuad& controlPosition, const IntSize& controlSize, FrameView* v, int index)
{
    // The controlSize is the size of the select box. It's usually larger than
    // we need. Subtract border size so that usually the container will be
    // displayed exactly the same width as the select box.
    m_listBox->setBaseWidth(std::max(controlSize.width() - borderSize * 2, 0));
    m_listBox->setSelectedIndex(m_listBox->m_popupClient->selectedIndex());
    m_listBox->updateFromElement();

    // We set the selected item in updateFromElement(), and disregard the
    // index passed into this function (same as Webkit's PopupMenuWin.cpp)
    // FIXME: make sure this is correct, and add an assertion.
    // ASSERT(popupWindow(popup)->listBox()->selectedIndex() == index);

    // Save and convert the controlPosition to main window coords. Each point is converted separately
    // to window coordinates because the control could be in a transformed webview and then each point
    // would be transformed by a different delta.
    m_controlPosition.setP1(v->contentsToRootFrame(IntPoint(controlPosition.p1().x(), controlPosition.p1().y())));
    m_controlPosition.setP2(v->contentsToRootFrame(IntPoint(controlPosition.p2().x(), controlPosition.p2().y())));
    m_controlPosition.setP3(v->contentsToRootFrame(IntPoint(controlPosition.p3().x(), controlPosition.p3().y())));
    m_controlPosition.setP4(v->contentsToRootFrame(IntPoint(controlPosition.p4().x(), controlPosition.p4().y())));

    FloatRect controlBounds = m_controlPosition.boundingBox();
    controlBounds = v->page()->frameHost().pinchViewport().mainViewToViewportCSSPixels(controlBounds);
    m_controlPosition = controlBounds;

    m_controlSize = controlSize;

    // Position at (0, 0) since the frameRect().location() is relative to the
    // parent WebWidget.
    setFrameRect(IntRect(IntPoint(), controlSize));
    showPopup(v);
}

inline bool PopupContainer::isRTL() const
{
    return m_listBox->m_popupClient->menuStyle().textDirection() == RTL;
}

int PopupContainer::selectedIndex() const
{
    return m_listBox->selectedIndex();
}

int PopupContainer::menuItemHeight() const
{
    return m_listBox->getRowHeight(0);
}

int PopupContainer::menuItemFontSize() const
{
    return m_listBox->getRowFont(0).fontDescription().computedSize();
}

PopupMenuStyle PopupContainer::menuStyle() const
{
    return m_listBox->m_popupClient->menuStyle();
}

String PopupContainer::getSelectedItemToolTip()
{
    // We cannot use m_popupClient->selectedIndex() to choose tooltip message,
    // because the selectedIndex() might return final selected index, not
    // hovering selection.
    return m_listBox->m_popupClient->itemToolTip(m_listBox->m_selectedIndex);
}

void PopupContainer::popupOpened(const IntRect& bounds)
{
    WebViewImpl* webView = WebViewImpl::fromPage(m_frameView->frame().page());
    if (!webView->client())
        return;

    WebWidget* webwidget = webView->client()->createPopupMenu(WebPopupTypeSelect);
    if (!webwidget)
        return;
    // We only notify when the WebView has to handle the popup, as when
    // the popup is handled externally, the fact that a popup is showing is
    // transparent to the WebView.
    webView->popupOpened(this);
    toWebPopupMenuImpl(webwidget)->initialize(this, bounds);
}

void PopupContainer::invalidateRect(const IntRect& rect)
{
    if (HostWindow* h = hostWindow())
        h->invalidateRect(rect);
    if (m_client)
        m_client->invalidateDisplayItemClient(displayItemClient());
}

HostWindow* PopupContainer::hostWindow() const
{
    return m_client;
}

IntPoint PopupContainer::convertChildToSelf(const Widget* child, const IntPoint& point) const
{
    IntPoint newPoint = point;
    newPoint.moveBy(child->location());
    return newPoint;
}

IntPoint PopupContainer::convertSelfToChild(const Widget* child, const IntPoint& point) const
{
    IntPoint newPoint = point;
    newPoint.moveBy(-child->location());
    return newPoint;
}

} // namespace blink
