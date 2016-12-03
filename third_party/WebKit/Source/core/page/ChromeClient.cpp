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

#include "core/page/ChromeClient.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/HitTestResult.h"
#include "core/page/FrameTree.h"
#include "core/page/ScopedPageLoadDeferrer.h"
#include "core/page/WindowFeatures.h"
#include "platform/geometry/IntRect.h"
#include "platform/network/NetworkHints.h"
#include "public/platform/WebScreenInfo.h"
#include <algorithm>

namespace blink {

void ChromeClient::setWindowRectWithAdjustment(const IntRect& pendingRect, LocalFrame& frame)
{
    IntRect screen = screenInfo().availableRect;
    IntRect window = pendingRect;

    IntSize minimumSize = minimumWindowSize();
    // Let size 0 pass through, since that indicates default size, not minimum
    // size.
    if (window.width())
        window.setWidth(std::min(std::max(minimumSize.width(), window.width()), screen.width()));
    if (window.height())
        window.setHeight(std::min(std::max(minimumSize.height(), window.height()), screen.height()));

    // Constrain the window position within the valid screen area.
    window.setX(std::max(screen.x(), std::min(window.x(), screen.maxX() - window.width())));
    window.setY(std::max(screen.y(), std::min(window.y(), screen.maxY() - window.height())));
    setWindowRect(window, frame);
}

bool ChromeClient::canOpenModalIfDuringPageDismissal(Frame* mainFrame, ChromeClient::DialogType dialog, const String& message)
{
    for (Frame* frame = mainFrame; frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        Document::PageDismissalType dismissal = toLocalFrame(frame)->document()->pageDismissalEventBeingDispatched();
        if (dismissal != Document::NoDismissal)
            return shouldOpenModalDialogDuringPageDismissal(dialog, message, dismissal);
    }
    return true;
}

void ChromeClient::setWindowFeatures(const WindowFeatures& features)
{
    setToolbarsVisible(features.toolBarVisible || features.locationBarVisible);
    setStatusbarVisible(features.statusBarVisible);
    setScrollbarsVisible(features.scrollbarsVisible);
    setMenubarVisible(features.menuBarVisible);
    setResizable(features.resizable);
}

template <typename Delegate>
static bool openJavaScriptDialog(LocalFrame* frame, const String& message, ChromeClient::DialogType dialogType, const Delegate& delegate)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of
    // executing JavaScript.
    ScopedPageLoadDeferrer deferrer;
    InspectorInstrumentation::willRunJavaScriptDialog(frame, message, dialogType);
    bool result = delegate();
    InspectorInstrumentation::didRunJavaScriptDialog(frame, result);
    return result;
}

bool ChromeClient::openBeforeUnloadConfirmPanel(const String& message, LocalFrame* frame, bool isReload)
{
    ASSERT(frame);
    return openJavaScriptDialog(frame, message, ChromeClient::HTMLDialog, [this, frame, isReload]() {
        return openBeforeUnloadConfirmPanelDelegate(frame, isReload);
    });
}

bool ChromeClient::openJavaScriptAlert(LocalFrame* frame, const String& message)
{
    ASSERT(frame);
    if (!canOpenModalIfDuringPageDismissal(frame->tree().top(), ChromeClient::AlertDialog, message))
        return false;
    return openJavaScriptDialog(frame, message, ChromeClient::AlertDialog, [this, frame, &message]() {
        return openJavaScriptAlertDelegate(frame, message);
    });
}

bool ChromeClient::openJavaScriptConfirm(LocalFrame* frame, const String& message)
{
    ASSERT(frame);
    if (!canOpenModalIfDuringPageDismissal(frame->tree().top(), ChromeClient::ConfirmDialog, message))
        return false;
    return openJavaScriptDialog(frame, message, ChromeClient::ConfirmDialog, [this, frame, &message]() {
        return openJavaScriptConfirmDelegate(frame, message);
    });
}

bool ChromeClient::openJavaScriptPrompt(LocalFrame* frame, const String& prompt, const String& defaultValue, String& result)
{
    ASSERT(frame);
    if (!canOpenModalIfDuringPageDismissal(frame->tree().top(), ChromeClient::PromptDialog, prompt))
        return false;
    return openJavaScriptDialog(frame, prompt, ChromeClient::PromptDialog, [this, frame, &prompt, &defaultValue, &result]() {
        return openJavaScriptPromptDelegate(frame, prompt, defaultValue, result);
    });
}

void ChromeClient::mouseDidMoveOverElement(const HitTestResult& result)
{
    if (result.innerNode() && result.innerNode()->document().isDNSPrefetchEnabled())
        prefetchDNS(result.absoluteLinkURL().host());

    showMouseOverURL(result);

    setToolTip(result);
}

void ChromeClient::setToolTip(const HitTestResult& result)
{
    // First priority is a tooltip for element with "title" attribute.
    TextDirection toolTipDirection;
    String toolTip = result.title(toolTipDirection);

    // Lastly, some elements provide default tooltip strings.  e.g. <input
    // type="file" multiple> shows a tooltip for the selected filenames.
    if (toolTip.isEmpty()) {
        if (Node* node = result.innerNode()) {
            if (node->isElementNode()) {
                toolTip = toElement(node)->defaultToolTip();

                // FIXME: We should obtain text direction of tooltip from
                // ChromeClient or platform. As of October 2011, all client
                // implementations don't use text direction information for
                // ChromeClient::setToolTip. We'll work on tooltip text
                // direction during bidi cleanup in form inputs.
                toolTipDirection = LTR;
            }
        }
    }

    if (m_lastToolTipPoint == result.hitTestLocation().point() && m_lastToolTipText == toolTip)
        return;
    m_lastToolTipPoint = result.hitTestLocation().point();
    m_lastToolTipText = toolTip;
    setToolTip(toolTip, toolTipDirection);
}

void ChromeClient::clearToolTip()
{
    // Do not check m_lastToolTip* and do not update them intentionally.
    // We don't want to show tooltips with same content after clearToolTip().
    setToolTip(String(), LTR);
}

void ChromeClient::print(LocalFrame* frame)
{
    // Defer loads in case the client method runs a new event loop that would
    // otherwise cause the load to continue while we're in the middle of
    // executing JavaScript.
    ScopedPageLoadDeferrer deferrer;

    printDelegate(frame);
}

} // namespace blink
