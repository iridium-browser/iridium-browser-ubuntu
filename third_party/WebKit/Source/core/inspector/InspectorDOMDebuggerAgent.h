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

#include "bindings/core/v8/V8EventListenerInfo.h"
#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/protocol/DOMDebugger.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"

#include <v8-inspector.h>

namespace blink {

class Element;
class InspectorDOMAgent;
class Node;

namespace protocol {
class DictionaryValue;
}

class CORE_EXPORT InspectorDOMDebuggerAgent final
    : public InspectorBaseAgent<protocol::DOMDebugger::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorDOMDebuggerAgent);

 public:
  static void eventListenersInfoForTarget(v8::Isolate*,
                                          v8::Local<v8::Value>,
                                          V8EventListenerInfoList& listeners);

  InspectorDOMDebuggerAgent(v8::Isolate*,
                            InspectorDOMAgent*,
                            v8_inspector::V8InspectorSession*);
  ~InspectorDOMDebuggerAgent() override;
  DECLARE_VIRTUAL_TRACE();

  // DOMDebugger API for frontend
  Response setDOMBreakpoint(int nodeId, const String& type) override;
  Response removeDOMBreakpoint(int nodeId, const String& type) override;
  Response setEventListenerBreakpoint(const String& eventName,
                                      Maybe<String> targetName) override;
  Response removeEventListenerBreakpoint(const String& eventName,
                                         Maybe<String> targetName) override;
  Response setInstrumentationBreakpoint(const String& eventName) override;
  Response removeInstrumentationBreakpoint(const String& eventName) override;
  Response setXHRBreakpoint(const String& url) override;
  Response removeXHRBreakpoint(const String& url) override;
  Response getEventListeners(
      const String& objectId,
      std::unique_ptr<protocol::Array<protocol::DOMDebugger::EventListener>>*
          listeners) override;

  // InspectorInstrumentation API
  void willInsertDOMNode(Node* parent);
  void didInvalidateStyleAttr(Node*);
  void didInsertDOMNode(Node*);
  void willRemoveDOMNode(Node*);
  void didRemoveDOMNode(Node*);
  void willModifyDOMAttr(Element*, const AtomicString&, const AtomicString&);
  void willSendXMLHttpOrFetchNetworkRequest(const String& url);
  void didFireWebGLError(const String& errorName);
  void didFireWebGLWarning();
  void didFireWebGLErrorOrWarning(const String& message);
  void allowNativeBreakpoint(const String& breakpointName,
                             const String* targetName,
                             bool sync);
  void cancelNativeBreakpoint();
  void scriptExecutionBlockedByCSP(const String& directiveText);

  Response disable() override;
  void restore() override;
  void didCommitLoadForLocalFrame(LocalFrame*) override;

 private:
  void pauseOnNativeEventIfNeeded(
      std::unique_ptr<protocol::DictionaryValue> eventData,
      bool synchronous);
  std::unique_ptr<protocol::DictionaryValue> preparePauseOnNativeEventData(
      const String& eventName,
      const String* targetName);

  protocol::DictionaryValue* eventListenerBreakpoints();
  protocol::DictionaryValue* xhrBreakpoints();

  void breakProgramOnDOMEvent(Node* target, int breakpointType, bool insertion);
  void updateSubtreeBreakpoints(Node*, uint32_t rootMask, bool set);
  bool hasBreakpoint(Node*, int type);
  Response setBreakpoint(const String& eventName, const String& targetName);
  Response removeBreakpoint(const String& eventName, const String& targetName);

  void didAddBreakpoint();
  void didRemoveBreakpoint();
  void setEnabled(bool);

  std::unique_ptr<protocol::DOMDebugger::EventListener>
  buildObjectForEventListener(v8::Local<v8::Context>,
                              const V8EventListenerInfo&,
                              const v8_inspector::StringView& objectGroupId);

  v8::Isolate* m_isolate;
  Member<InspectorDOMAgent> m_domAgent;
  v8_inspector::V8InspectorSession* m_v8Session;
  HeapHashMap<Member<Node>, uint32_t> m_domBreakpoints;
};

}  // namespace blink

#endif  // !defined(InspectorDOMDebuggerAgent_h)
