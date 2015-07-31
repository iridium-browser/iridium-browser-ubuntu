/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/parser/HTMLResourcePreloader.h"

#include "core/dom/Document.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/ResourceFetcher.h"
#include "public/platform/Platform.h"

namespace blink {

inline HTMLResourcePreloader::HTMLResourcePreloader(Document& document)
    : m_document(document)
{
}

PassOwnPtrWillBeRawPtr<HTMLResourcePreloader> HTMLResourcePreloader::create(Document& document)
{
    return adoptPtrWillBeNoop(new HTMLResourcePreloader(document));
}

DEFINE_TRACE(HTMLResourcePreloader)
{
    visitor->trace(m_document);
}

void HTMLResourcePreloader::preload(PassOwnPtr<PreloadRequest> preload)
{
    FetchRequest request = preload->resourceRequest(m_document);
    Platform::current()->histogramCustomCounts("WebCore.PreloadDelayMs", static_cast<int>(1000 * (monotonicallyIncreasingTime() - preload->discoveryTime())), 0, 2000, 20);
    m_document->fetcher()->preload(preload->resourceType(), request, preload->charset());
}

} // namespace blink
