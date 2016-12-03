/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Motorola Mobility Inc.
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

#ifndef DOMURL_h
#define DOMURL_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/DOMURLUtils.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Forward.h"

namespace blink {

class Blob;
class ExceptionState;
class ExecutionContext;
class URLRegistrable;
class URLSearchParams;

class DOMURL final : public GarbageCollectedFinalized<DOMURL>, public ScriptWrappable, public DOMURLUtils {
    DEFINE_WRAPPERTYPEINFO();
public:
    static DOMURL* create(const String& url, ExceptionState& exceptionState)
    {
        return new DOMURL(url, blankURL(), exceptionState);
    }

    static DOMURL* create(const String& url, const String& base, ExceptionState& exceptionState)
    {
        return new DOMURL(url, KURL(KURL(), base), exceptionState);
    }
    ~DOMURL();

    CORE_EXPORT static String createPublicURL(ExecutionContext*, URLRegistrable*, const String& uuid = String());
    static void revokeObjectUUID(ExecutionContext*, const String&);

    KURL url() const override { return m_url; }
    void setURL(const KURL& url) override { m_url = url; }

    String input() const override { return m_input; }
    void setInput(const String&) override;

    void setSearch(const String&) override;

    URLSearchParams* searchParams();

    DECLARE_VIRTUAL_TRACE();

private:
    friend class URLSearchParams;
    DOMURL(const String& url, const KURL& base, ExceptionState&);

    void update();
    void updateSearchParams(const String&);

    KURL m_url;
    String m_input;
    WeakMember<URLSearchParams> m_searchParams;
};

} // namespace blink

#endif // DOMURL_h
