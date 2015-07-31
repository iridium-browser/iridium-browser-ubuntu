/*
 * Copyright (C) 2006, 2007, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/page/Chrome.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/ColorChooser.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/HitTestResult.h"
#include "core/page/ChromeClient.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PopupOpeningObserver.h"
#include "core/page/ScopedPageLoadDeferrer.h"
#include "core/page/WindowFeatures.h"
#include "platform/FileChooser.h"
#include "platform/Logging.h"
#include "platform/geometry/IntRect.h"
#include "platform/network/NetworkHints.h"
#include "public/platform/WebScreenInfo.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include <algorithm>

namespace blink {

using namespace HTMLNames;

Chrome::Chrome(Page* page, ChromeClient* client)
    : m_page(page)
    , m_client(client)
{
    ASSERT(m_client);
}

Chrome::~Chrome()
{
}

PassOwnPtr<Chrome> Chrome::create(Page* page, ChromeClient* client)
{
    return adoptPtr(new Chrome(page, client));
}

void Chrome::invalidateRect(const IntRect& updateRect)
{
    m_client->invalidateRect(updateRect);
}

IntRect Chrome::viewportToScreen(const IntRect& rect) const
{
    return m_client->viewportToScreen(rect);
}

WebScreenInfo Chrome::screenInfo() const
{
    return m_client->screenInfo();
}

void Chrome::contentsSizeChanged(LocalFrame* frame, const IntSize& size) const
{
    m_client->contentsSizeChanged(frame, size);
}

void Chrome::setWindowRect(const IntRect& pendingRect) const
{
    IntRect screen = screenInfo().availableRect;
    IntRect window = pendingRect;

    IntSize minimumSize = m_client->minimumWindowSize();
    // Let size 0 pass through, since that indicates default size, not minimum size.
    if (window.width())
        window.setWidth(std::min(std::max(minimumSize.width(), window.width()), screen.width()));
    if (window.height())
        window.setHeight(std::min(std::max(minimumSize.height(), window.height()), screen.height()));

    // Constrain the window position within the valid screen area.
    window.setX(std::max(screen.x(), std::min(window.x(), screen.maxX() - window.width())));
    window.setY(std::max(screen.y(), std::min(window.y(), screen.maxY() - window.height())));
    m_client->setWindowRect(window);
}

IntRect Chrome::windowRect() const
{
    return m_client->windowRect();
}

IntRect Chrome::pageRect() const
{
    return m_client->pageRect();
}

void Chrome::focus() const
{
    m_client->focus();
}

bool Chrome::canTakeFocus(WebFocusType type) const
{
    return m_client->canTakeFocus(type);
}

void Chrome::takeFocus(WebFocusType type) const
{
    m_client->takeFocus(type);
}

void Chrome::focusedNodeChanged(Node* fromNode, Node* toNode) const
{
    m_client->focusedNodeChanged(fromNode, toNode);
}

void Chrome::show(NavigationPolicy policy) const
{
    m_client->show(policy);
}

static bool canRunModalIfDuringPageDismissal(Page* page, ChromeClient::DialogType dialog, const String& message)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        Document::PageDismissalType dismissal = toLocalFrame(frame)->document()->pageDismissalEventBeingDispatched();
        if (dismissal != Document::NoDismissal)
            return page->chrome().client().shouldRunModalDialogDuringPageDismissal(dialog, message, dismissal);
    }
    return true;
}

void Chrome::setWindowFeatures(const WindowFeatures& features) const
{
    m_client->setToolbarsVisible(features.toolBarVisible || features.locationBarVisible);
    m_client->setStatusbarVisible(features.statusBarVisible);
    m_client->setScrollbarsVisible(features.scrollbarsVisible);
    m_client->setMenubarVisible(features.menuBarVisible);
    m_client->setResizable(features.resizable);
}

bool Chrome::toolbarsVisible() const
{
    return m_client->toolbarsVisible();
}

bool Chrome::statusbarVisible() const
{
    return m_client->statusbarVisible();
}

bool Chrome::scrollbarsVisible() const
{
    return m_client->scrollbarsVisible();
}

bool Chrome::menubarVisible() const
{
    return m_client->menubarVisible();
}

bool Chrome::canRunBeforeUnloadConfirmPanel()
{
    return m_client->canRunBeforeUnloadConfirmPanel();
}

bool Chrome::runBeforeUnloadConfirmPanel(const String& message, LocalFrame* frame)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    ScopedPageLoadDeferrer deferrer;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willRunJavaScriptDialog(frame, message);
    bool ok = m_client->runBeforeUnloadConfirmPanel(message, frame);
    InspectorInstrumentation::didRunJavaScriptDialog(cookie);
    return ok;
}

void Chrome::closeWindowSoon()
{
    m_client->closeWindowSoon();
}

void Chrome::runJavaScriptAlert(LocalFrame* frame, const String& message)
{
    if (!canRunModalIfDuringPageDismissal(m_page, ChromeClient::AlertDialog, message))
        return;

    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    ScopedPageLoadDeferrer deferrer;

    ASSERT(frame);
    notifyPopupOpeningObservers();

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willRunJavaScriptDialog(frame, message);
    m_client->runJavaScriptAlert(frame, message);
    InspectorInstrumentation::didRunJavaScriptDialog(cookie);
}

bool Chrome::runJavaScriptConfirm(LocalFrame* frame, const String& message)
{
    if (!canRunModalIfDuringPageDismissal(m_page, ChromeClient::ConfirmDialog, message))
        return false;

    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    ScopedPageLoadDeferrer deferrer;

    ASSERT(frame);
    notifyPopupOpeningObservers();

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willRunJavaScriptDialog(frame, message);
    bool ok = m_client->runJavaScriptConfirm(frame, message);
    InspectorInstrumentation::didRunJavaScriptDialog(cookie);
    return ok;
}

bool Chrome::runJavaScriptPrompt(LocalFrame* frame, const String& prompt, const String& defaultValue, String& result)
{
    if (!canRunModalIfDuringPageDismissal(m_page, ChromeClient::PromptDialog, prompt))
        return false;

    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    ScopedPageLoadDeferrer deferrer;

    ASSERT(frame);
    notifyPopupOpeningObservers();

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willRunJavaScriptDialog(frame, prompt);
    bool ok = m_client->runJavaScriptPrompt(frame, prompt, defaultValue, result);
    InspectorInstrumentation::didRunJavaScriptDialog(cookie);

    return ok;
}

void Chrome::setStatusbarText(LocalFrame* frame, const String& status)
{
    ASSERT(frame);
    m_client->setStatusbarText(status);
}

IntRect Chrome::windowResizerRect() const
{
    return m_client->windowResizerRect();
}

void Chrome::mouseDidMoveOverElement(const HitTestResult& result)
{
    if (result.innerNode()) {
        if (result.innerNode()->document().isDNSPrefetchEnabled())
            prefetchDNS(result.absoluteLinkURL().host());
    }
    m_client->mouseDidMoveOverElement(result);
}

void Chrome::setToolTip(const HitTestResult& result)
{
    // First priority is a potential toolTip representing a spelling or grammar error
    TextDirection toolTipDirection;
    String toolTip = result.spellingToolTip(toolTipDirection);

    // Next we'll consider a tooltip for element with "title" attribute
    if (toolTip.isEmpty())
        toolTip = result.title(toolTipDirection);

    // Lastly, for <input type="file"> that allow multiple files, we'll consider a tooltip for the selected filenames
    if (toolTip.isEmpty()) {
        if (Node* node = result.innerNode()) {
            if (isHTMLInputElement(*node)) {
                HTMLInputElement* input = toHTMLInputElement(node);
                toolTip = input->defaultToolTip();

                // FIXME: We should obtain text direction of tooltip from
                // ChromeClient or platform. As of October 2011, all client
                // implementations don't use text direction information for
                // ChromeClient::setToolTip. We'll work on tooltip text
                // direction during bidi cleanup in form inputs.
                toolTipDirection = LTR;
            }
        }
    }

    m_client->setToolTip(toolTip, toolTipDirection);
}

void Chrome::print(LocalFrame* frame)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of executing JavaScript.
    ScopedPageLoadDeferrer deferrer;

    m_client->print(frame);
}

void Chrome::enumerateChosenDirectory(FileChooser* fileChooser)
{
    m_client->enumerateChosenDirectory(fileChooser);
}

PassOwnPtrWillBeRawPtr<ColorChooser> Chrome::createColorChooser(LocalFrame* frame, ColorChooserClient* client, const Color& initialColor)
{
    notifyPopupOpeningObservers();
    return m_client->createColorChooser(frame, client, initialColor);
}

PassRefPtr<DateTimeChooser> Chrome::openDateTimeChooser(DateTimeChooserClient* client, const DateTimeChooserParameters& parameters)
{
    notifyPopupOpeningObservers();
    return m_client->openDateTimeChooser(client, parameters);
}

void Chrome::openTextDataListChooser(HTMLInputElement& input)
{
    notifyPopupOpeningObservers();
    m_client->openTextDataListChooser(input);
}

void Chrome::runOpenPanel(LocalFrame* frame, PassRefPtr<FileChooser> fileChooser)
{
    notifyPopupOpeningObservers();
    m_client->runOpenPanel(frame, fileChooser);
}

void Chrome::dispatchViewportPropertiesDidChange(const ViewportDescription& description) const
{
    m_client->dispatchViewportPropertiesDidChange(description);
}

void Chrome::setCursor(const Cursor& cursor)
{
    m_lastSetMouseCursorForTesting = cursor;
    m_client->setCursor(cursor);
}

Cursor Chrome::getLastSetCursorForTesting() const
{
    return m_lastSetMouseCursorForTesting;
}

void Chrome::scheduleAnimation()
{
    m_page->animator().setAnimationFramePending();
    m_client->scheduleAnimation();
}

// --------

void Chrome::scheduleAnimationForFrame(LocalFrame* localRoot)
{
    m_page->animator().setAnimationFramePending();
    m_client->scheduleAnimationForFrame(localRoot);
}

bool Chrome::hasOpenedPopup() const
{
    return m_client->hasOpenedPopup();
}

PassRefPtrWillBeRawPtr<PopupMenu> Chrome::createPopupMenu(LocalFrame& frame, PopupMenuClient* client)
{
    notifyPopupOpeningObservers();
    return m_client->createPopupMenu(frame, client);
}

void Chrome::registerPopupOpeningObserver(PopupOpeningObserver* observer)
{
    ASSERT(observer);
    m_popupOpeningObservers.append(observer);
}

void Chrome::unregisterPopupOpeningObserver(PopupOpeningObserver* observer)
{
    size_t index = m_popupOpeningObservers.find(observer);
    ASSERT(index != kNotFound);
    m_popupOpeningObservers.remove(index);
}

void Chrome::notifyPopupOpeningObservers() const
{
    const Vector<PopupOpeningObserver*> observers(m_popupOpeningObservers);
    for (size_t i = 0; i < observers.size(); ++i)
        observers[i]->willOpenPopup();
}

void Chrome::registerViewportLayers() const
{
    m_client->registerViewportLayers();
}

void Chrome::willBeDestroyed()
{
    m_client->chromeDestroyed();
}

} // namespace blink
