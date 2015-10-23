// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/v8/V8DebuggerListener.h"

namespace {
static const unsigned kBlackboxUnknown = 0;
}

namespace blink {

V8DebuggerListener::Script::Script()
    : m_startLine(0)
    , m_startColumn(0)
    , m_endLine(0)
    , m_endColumn(0)
    , m_isContentScript(false)
    , m_isBlackboxedURL(false)
    , m_blackboxGeneration(kBlackboxUnknown)
{
}

String V8DebuggerListener::Script::sourceURL() const
{
    return m_sourceURL.isEmpty() ? m_url : m_sourceURL;
}

bool V8DebuggerListener::Script::getBlackboxedState(unsigned blackboxGeneration, bool* isBlackboxed) const
{
    if (m_blackboxGeneration == kBlackboxUnknown || m_blackboxGeneration != blackboxGeneration)
        return false;
    *isBlackboxed = m_isBlackboxedURL;
    return true;
}

void V8DebuggerListener::Script::setBlackboxedState(unsigned blackboxGeneration, bool isBlackboxed)
{
    ASSERT(blackboxGeneration);
    m_isBlackboxedURL = isBlackboxed;
    m_blackboxGeneration = blackboxGeneration;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setURL(const String& url)
{
    m_url = url;
    m_blackboxGeneration = kBlackboxUnknown;
    return *this;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setSourceURL(const String& sourceURL)
{
    m_sourceURL = sourceURL;
    m_blackboxGeneration = kBlackboxUnknown;
    return *this;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setSourceMappingURL(const String& sourceMappingURL)
{
    m_sourceMappingURL = sourceMappingURL;
    return *this;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setSource(const String& source)
{
    m_source = source;
    return *this;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setStartLine(int startLine)
{
    m_startLine = startLine;
    return *this;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setStartColumn(int startColumn)
{
    m_startColumn = startColumn;
    return *this;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setEndLine(int endLine)
{
    m_endLine = endLine;
    return *this;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setEndColumn(int endColumn)
{
    m_endColumn = endColumn;
    return *this;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setIsContentScript(bool isContentScript)
{
    m_isContentScript = isContentScript;
    return *this;
}

V8DebuggerListener::Script& V8DebuggerListener::Script::setIsInternalScript(bool isInternalScript)
{
    m_isInternalScript = isInternalScript;
    return *this;
}

} // namespace blink
