/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
#include "web/PopupMenuChromium.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "web/PopupContainer.h"

namespace blink {

PopupMenuChromium::PopupMenuChromium(LocalFrame& frame, PopupMenuClient* client)
    : m_popupClient(client)
    , m_frameView(frame.view())
{
}

PopupMenuChromium::~PopupMenuChromium()
{
#if ENABLE(OILPAN)
    // Verify that dispose() has been called when disconnecting.
    ASSERT(!m_popupClient);
#else
    dispose();
#endif
}

void PopupMenuChromium::dispose()
{
    // When the PopupMenuChromium is destroyed, the client could already have been deleted.
    if (m_popup)
        m_popup->disconnectClient();
    hide();
}

DEFINE_TRACE(PopupMenuChromium)
{
    visitor->trace(m_frameView);
    visitor->trace(m_popup);
    PopupMenu::trace(visitor);
}

void PopupMenuChromium::show(const FloatQuad& controlPosition, const IntSize& controlSize, int index)
{
    if (!m_popup) {
        bool deviceSupportsTouch = m_frameView->frame().settings()->deviceSupportsTouch();
        m_popup = PopupContainer::create(m_popupClient, deviceSupportsTouch);
    }
    m_popup->showInRect(controlPosition, controlSize, m_frameView.get(), index);
}

void PopupMenuChromium::hide()
{
    if (m_popup)
        m_popup->hide();
}

void PopupMenuChromium::updateFromElement()
{
    m_popup->updateFromElement();
}


void PopupMenuChromium::disconnectClient()
{
    m_popupClient = 0;
#if ENABLE(OILPAN)
    // Cannot be done during finalization, so instead done when the
    // layout object is destroyed and disconnected.
    //
    // FIXME: do this always, regardless of ENABLE(OILPAN).
    dispose();
#endif
}

} // namespace blink
