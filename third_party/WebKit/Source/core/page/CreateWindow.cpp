/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/page/CreateWindow.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/WindowFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

static LocalFrame* createWindow(LocalFrame& openerFrame, LocalFrame& lookupFrame, const FrameLoadRequest& request, const WindowFeatures& features, NavigationPolicy policy, ShouldSendReferrer shouldSendReferrer, bool& created)
{
    ASSERT(!features.dialog || request.frameName().isEmpty());
    ASSERT(request.resourceRequest().requestorOrigin() || openerFrame.document()->url().isEmpty());
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeAuxiliary);

    if (!request.frameName().isEmpty() && request.frameName() != "_blank" && policy == NavigationPolicyIgnore) {
        if (Frame* frame = lookupFrame.findFrameForNavigation(request.frameName(), openerFrame)) {
            if (request.frameName() != "_self") {
                if (FrameHost* host = frame->host()) {
                    if (host == openerFrame.host())
                        frame->page()->focusController().setFocusedFrame(frame);
                    else
                        host->chrome().focus();
                }
            }
            created = false;
            // FIXME: Make this work with RemoteFrames.
            return frame->isLocalFrame() ? toLocalFrame(frame) : nullptr;
        }
    }

    // Sandboxed frames cannot open new auxiliary browsing contexts.
    if (openerFrame.document()->isSandboxed(SandboxPopups)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        openerFrame.document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Blocked opening '" + request.resourceRequest().url().elidedString() + "' in a new window because the request was made in a sandboxed frame whose 'allow-popups' permission is not set."));
        return nullptr;
    }

    if (openerFrame.settings() && !openerFrame.settings()->supportsMultipleWindows()) {
        created = false;
        if (!openerFrame.tree().top()->isLocalFrame())
            return nullptr;
        return toLocalFrame(openerFrame.tree().top());
    }

    Page* oldPage = openerFrame.page();
    if (!oldPage)
        return nullptr;

    Page* page = oldPage->chrome().client().createWindow(&openerFrame, request, features, policy, shouldSendReferrer);
    if (!page || !page->mainFrame()->isLocalFrame())
        return nullptr;
    FrameHost* host = &page->frameHost();

    ASSERT(page->mainFrame());
    LocalFrame& frame = *page->deprecatedLocalMainFrame();

    if (request.frameName() != "_blank")
        frame.tree().setName(request.frameName());

    host->chrome().setWindowFeatures(features);

    // 'x' and 'y' specify the location of the window, while 'width' and 'height'
    // specify the size of the viewport. We can only resize the window, so adjust
    // for the difference between the window size and the viewport size.

    IntRect windowRect = host->chrome().windowRect();
    IntSize viewportSize = host->chrome().pageRect().size();

    if (features.xSet)
        windowRect.setX(features.x);
    if (features.ySet)
        windowRect.setY(features.y);
    if (features.widthSet)
        windowRect.setWidth(features.width + (windowRect.width() - viewportSize.width()));
    if (features.heightSet)
        windowRect.setHeight(features.height + (windowRect.height() - viewportSize.height()));

    host->chrome().setWindowRect(windowRect);
    host->chrome().show(policy);

    if (openerFrame.document()->isSandboxed(SandboxPropagatesToAuxiliaryBrowsingContexts))
        frame.loader().forceSandboxFlags(openerFrame.document()->sandboxFlags());

    created = true;
    return &frame;
}

LocalFrame* createWindow(const String& urlString, const AtomicString& frameName, const WindowFeatures& windowFeatures,
    LocalDOMWindow& callingWindow, LocalFrame& firstFrame, LocalFrame& openerFrame)
{
    LocalFrame* activeFrame = callingWindow.frame();
    ASSERT(activeFrame);

    KURL completedURL = urlString.isEmpty() ? KURL(ParsedURLString, emptyString()) : firstFrame.document()->completeURL(urlString);
    if (!completedURL.isEmpty() && !completedURL.isValid()) {
        // Don't expose client code to invalid URLs.
        callingWindow.printErrorMessage("Unable to open a window with invalid URL '" + completedURL.string() + "'.\n");
        return nullptr;
    }

    FrameLoadRequest frameRequest(callingWindow.document(), completedURL, frameName);
    frameRequest.resourceRequest().setFrameType(WebURLRequest::FrameTypeAuxiliary);
    frameRequest.resourceRequest().setRequestorOrigin(SecurityOrigin::create(activeFrame->document()->url()));

    // Normally, FrameLoader would take care of setting the referrer for a navigation that is
    // triggered from javascript. However, creating a window goes through sufficient processing
    // that it eventually enters FrameLoader as an embedder-initiated navigation. FrameLoader
    // assumes no responsibility for generating an embedder-initiated navigation's referrer,
    // so we need to ensure the proper referrer is set now.
    frameRequest.resourceRequest().setHTTPReferrer(SecurityPolicy::generateReferrer(activeFrame->document()->referrerPolicy(), completedURL, activeFrame->document()->outgoingReferrer()));

    bool hasUserGesture = UserGestureIndicator::processingUserGesture();

    // We pass the opener frame for the lookupFrame in case the active frame is different from
    // the opener frame, and the name references a frame relative to the opener frame.
    bool created;
    LocalFrame* newFrame = createWindow(*activeFrame, openerFrame, frameRequest, windowFeatures, NavigationPolicyIgnore, MaybeSendReferrer, created);
    if (!newFrame)
        return nullptr;

    newFrame->loader().setOpener(&openerFrame);

    if (newFrame->localDOMWindow()->isInsecureScriptAccess(callingWindow, completedURL))
        return newFrame;

    if (created) {
        FrameLoadRequest request(callingWindow.document(), completedURL);
        request.resourceRequest().setHasUserGesture(hasUserGesture);
        newFrame->loader().load(request);
    } else if (!urlString.isEmpty()) {
        newFrame->navigate(*callingWindow.document(), completedURL, false);
    }
    return newFrame;
}

void createWindowForRequest(const FrameLoadRequest& request, LocalFrame& openerFrame, NavigationPolicy policy, ShouldSendReferrer shouldSendReferrer)
{
    ASSERT(request.resourceRequest().requestorOrigin() || (openerFrame.document() && openerFrame.document()->url().isEmpty()));

    if (openerFrame.document()->pageDismissalEventBeingDispatched() != Document::NoDismissal)
        return;

    if (openerFrame.document() && openerFrame.document()->isSandboxed(SandboxPopups))
        return;

    if (!LocalDOMWindow::allowPopUp(openerFrame))
        return;

    if (policy == NavigationPolicyCurrentTab)
        policy = NavigationPolicyNewForegroundTab;

    WindowFeatures features;
    bool created;
    LocalFrame* newFrame = createWindow(openerFrame, openerFrame, request, features, policy, shouldSendReferrer, created);
    if (!newFrame)
        return;
    if (shouldSendReferrer == MaybeSendReferrer) {
        newFrame->loader().setOpener(&openerFrame);
        newFrame->document()->setReferrerPolicy(openerFrame.document()->referrerPolicy());
    }
    FrameLoadRequest newRequest(0, request.resourceRequest());
    newRequest.setForm(request.form());
    newFrame->loader().load(newRequest);
}

} // namespace blink
