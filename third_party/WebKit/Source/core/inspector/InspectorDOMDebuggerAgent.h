/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorDOMDebuggerAgent_h
#define InspectorDOMDebuggerAgent_h


#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Element;
class Event;
class EventListener;
class EventTarget;
class InjectedScriptManager;
class InspectorDOMAgent;
class JSONObject;
class Node;
class RegisteredEventListener;
class V8DebuggerAgent;

typedef String ErrorString;

class CORE_EXPORT InspectorDOMDebuggerAgent final
    : public InspectorBaseAgent<InspectorDOMDebuggerAgent, InspectorFrontend::DOMDebugger>
    , public InspectorBackendDispatcher::DOMDebuggerCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDOMDebuggerAgent);
public:
    static PassOwnPtrWillBeRawPtr<InspectorDOMDebuggerAgent> create(InjectedScriptManager*, InspectorDOMAgent*, V8DebuggerAgent*);

    ~InspectorDOMDebuggerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    // DOMDebugger API for InspectorFrontend
    void setXHRBreakpoint(ErrorString*, const String& url) override;
    void removeXHRBreakpoint(ErrorString*, const String& url) override;
    void setEventListenerBreakpoint(ErrorString*, const String& eventName, const String* targetName) override;
    void removeEventListenerBreakpoint(ErrorString*, const String& eventName, const String* targetName) override;
    void setInstrumentationBreakpoint(ErrorString*, const String& eventName) override;
    void removeInstrumentationBreakpoint(ErrorString*, const String& eventName) override;
    void setDOMBreakpoint(ErrorString*, int nodeId, const String& type) override;
    void removeDOMBreakpoint(ErrorString*, int nodeId, const String& type) override;
    void getEventListeners(ErrorString*, const String& objectId, RefPtr<TypeBuilder::Array<TypeBuilder::DOMDebugger::EventListener>>& result) override;
    // InspectorInstrumentation API
    void willInsertDOMNode(Node* parent);
    void didInvalidateStyleAttr(Node*);
    void didInsertDOMNode(Node*);
    void willRemoveDOMNode(Node*);
    void didRemoveDOMNode(Node*);
    void willModifyDOMAttr(Element*, const AtomicString&, const AtomicString&);
    void willSetInnerHTML();
    void willSendXMLHttpRequest(const String& url);
    void didInstallTimer(ExecutionContext*, int timerId, int timeout, bool singleShot);
    void didRemoveTimer(ExecutionContext*, int timerId);
    void willFireTimer(ExecutionContext*, int timerId);
    void didRequestAnimationFrame(ExecutionContext*, int callbackId);
    void didCancelAnimationFrame(ExecutionContext*, int callbackId);
    void willFireAnimationFrame(ExecutionContext*, int callbackId);
    void willHandleEvent(EventTarget*, Event*, EventListener*, bool useCapture);
    void willEvaluateScript();
    void didFireWebGLError(const String& errorName);
    void didFireWebGLWarning();
    void didFireWebGLErrorOrWarning(const String& message);
    void willCloseWindow();

    void disable(ErrorString*) override;
    void restore() override;
    void didCommitLoadForLocalFrame(LocalFrame*) override;

private:
    InspectorDOMDebuggerAgent(InjectedScriptManager*, InspectorDOMAgent*, V8DebuggerAgent*);

    void pauseOnNativeEventIfNeeded(PassRefPtr<JSONObject> eventData, bool synchronous);
    PassRefPtr<JSONObject> preparePauseOnNativeEventData(const String& eventName, const String* targetName);

    void descriptionForDOMEvent(Node* target, int breakpointType, bool insertion, JSONObject* description);
    void updateSubtreeBreakpoints(Node*, uint32_t rootMask, bool set);
    bool hasBreakpoint(Node*, int type);
    void setBreakpoint(ErrorString*, const String& eventName, const String* targetName);
    void removeBreakpoint(ErrorString*, const String& eventName, const String* targetName);

    void didAddBreakpoint();
    void didRemoveBreakpoint();
    void setEnabled(bool);

    void eventListeners(InjectedScript&, v8::Local<v8::Value>, const String& objectGroup, RefPtr<TypeBuilder::Array<TypeBuilder::DOMDebugger::EventListener>>& listenersArray);
    PassRefPtr<TypeBuilder::DOMDebugger::EventListener> buildObjectForEventListener(InjectedScript&, v8::Local<v8::Object> handler, bool useCapture, const String& type, const String& objectGroupId);

    RawPtrWillBeMember<InjectedScriptManager> m_injectedScriptManager;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    RawPtrWillBeMember<V8DebuggerAgent> m_debuggerAgent;
    WillBeHeapHashMap<RawPtrWillBeMember<Node>, uint32_t> m_domBreakpoints;
};

} // namespace blink


#endif // !defined(InspectorDOMDebuggerAgent_h)
