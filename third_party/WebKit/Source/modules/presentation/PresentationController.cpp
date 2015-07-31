// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationController.h"

#include "core/frame/LocalFrame.h"
#include "modules/presentation/PresentationSession.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {

PresentationController::PresentationController(LocalFrame& frame, WebPresentationClient* client)
    : LocalFrameLifecycleObserver(&frame)
    , m_client(client)
{
    if (m_client)
        m_client->setController(this);
}

PresentationController::~PresentationController()
{
    if (m_client)
        m_client->setController(nullptr);
}

// static
PassOwnPtrWillBeRawPtr<PresentationController> PresentationController::create(LocalFrame& frame, WebPresentationClient* client)
{
    return adoptPtrWillBeNoop(new PresentationController(frame, client));
}

// static
const char* PresentationController::supplementName()
{
    return "PresentationController";
}

// static
PresentationController* PresentationController::from(LocalFrame& frame)
{
    return static_cast<PresentationController*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));
}

// static
void PresentationController::provideTo(LocalFrame& frame, WebPresentationClient* client)
{
    WillBeHeapSupplement<LocalFrame>::provideTo(frame, PresentationController::supplementName(), PresentationController::create(frame, client));
}

DEFINE_TRACE(PresentationController)
{
    visitor->trace(m_presentation);
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

void PresentationController::didChangeAvailability(bool available)
{
    if (m_presentation)
        m_presentation->didChangeAvailability(available);
}

bool PresentationController::isAvailableChangeWatched() const
{
    if (!m_presentation)
        return false;
    return m_presentation->isAvailableChangeWatched();
}

void PresentationController::updateAvailableChangeWatched(bool watched)
{
    if (m_client)
        m_client->updateAvailableChangeWatched(watched);
}

void PresentationController::didStartDefaultSession(WebPresentationSessionClient* sessionClient)
{
    if (!m_presentation) {
        PresentationSession::dispose(sessionClient);
        return;
    }

    PresentationSession* session = PresentationSession::take(sessionClient, m_presentation);
    m_presentation->didStartDefaultSession(session);
}

void PresentationController::didChangeSessionState(WebPresentationSessionClient* sessionClient, WebPresentationSessionState state)
{
    if (m_presentation)
        m_presentation->didChangeSessionState(sessionClient, state);
    else
        PresentationSession::dispose(sessionClient);
}

void PresentationController::didReceiveSessionTextMessage(WebPresentationSessionClient* sessionClient, const WebString& message)
{
    if (m_presentation)
        m_presentation->didReceiveSessionTextMessage(sessionClient, message);
    else
        PresentationSession::dispose(sessionClient);
}

void PresentationController::startSession(const String& presentationUrl, const String& presentationId, WebPresentationSessionClientCallbacks* callbacks)
{
    if (!m_client) {
        delete callbacks;
        return;
    }
    m_client->startSession(presentationUrl, presentationId, callbacks);
}

void PresentationController::joinSession(const String& presentationUrl, const String& presentationId, WebPresentationSessionClientCallbacks* callbacks)
{
    if (!m_client) {
        delete callbacks;
        return;
    }
    m_client->joinSession(presentationUrl, presentationId, callbacks);
}

void PresentationController::send(const String& presentationUrl, const String& presentationId, const String& message)
{
    if (!m_client)
        return;
    m_client->sendString(presentationUrl, presentationId, message);
}

void PresentationController::send(const String& presentationUrl, const String& presentationId, const uint8_t* data, size_t length)
{
    if (!m_client)
        return;
    m_client->sendArrayBuffer(presentationUrl, presentationId, data, length);
}

void PresentationController::closeSession(const String& url, const String& presentationId)
{
    if (!m_client)
        return;
    m_client->closeSession(url, presentationId);
}

void PresentationController::setPresentation(Presentation* presentation)
{
    m_presentation = presentation;
}

void PresentationController::willDetachFrameHost()
{
    if (m_client) {
        m_client->setController(nullptr);
        m_client = nullptr;
    }
}

} // namespace blink
