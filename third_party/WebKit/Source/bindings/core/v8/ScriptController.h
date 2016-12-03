/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#ifndef ScriptController_h
#define ScriptController_h

#include "bindings/core/v8/SharedPersistent.h"
#include "bindings/core/v8/WindowProxyManager.h"
#include "core/CoreExport.h"
#include "core/fetch/AccessControlStatus.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/frame/LocalFrame.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/TextPosition.h"
#include <v8.h>

namespace blink {

class DOMWrapperWorld;
class ExecutionContext;
class HTMLDocument;
class HTMLPlugInElement;
class KURL;
class ScriptState;
class ScriptSourceCode;
class SecurityOrigin;
class WindowProxy;
class Widget;

typedef WTF::Vector<v8::Extension*> V8Extensions;

enum ReasonForCallingCanExecuteScripts {
    AboutToExecuteScript,
    NotAboutToExecuteScript
};

class CORE_EXPORT ScriptController final : public GarbageCollected<ScriptController> {
    WTF_MAKE_NONCOPYABLE(ScriptController);
public:
    enum ExecuteScriptPolicy {
        ExecuteScriptWhenScriptsDisabled,
        DoNotExecuteScriptWhenScriptsDisabled
    };

    static ScriptController* create(LocalFrame* frame)
    {
        return new ScriptController(frame);
    }

    DECLARE_TRACE();

    bool initializeMainWorld();
    WindowProxy* windowProxy(DOMWrapperWorld&);
    WindowProxy* existingWindowProxy(DOMWrapperWorld&);

    // Evaluate JavaScript in the main world.
    void executeScriptInMainWorld(const String&, ExecuteScriptPolicy = DoNotExecuteScriptWhenScriptsDisabled);
    void executeScriptInMainWorld(const ScriptSourceCode&, AccessControlStatus = NotSharableCrossOrigin);
    v8::Local<v8::Value> executeScriptInMainWorldAndReturnValue(const ScriptSourceCode&, ExecuteScriptPolicy = DoNotExecuteScriptWhenScriptsDisabled);
    v8::Local<v8::Value> executeScriptAndReturnValue(v8::Local<v8::Context>, const ScriptSourceCode&, AccessControlStatus = NotSharableCrossOrigin);

    // Executes JavaScript in an isolated world. The script gets its own global scope,
    // its own prototypes for intrinsic JavaScript objects (String, Array, and so-on),
    // and its own wrappers for all DOM nodes and DOM constructors.
    //
    // If an isolated world with the specified ID already exists, it is reused.
    // Otherwise, a new world is created.
    //
    // FIXME: Get rid of extensionGroup here.
    // FIXME: We don't want to support multiple scripts.
    void executeScriptInIsolatedWorld(int worldID, const HeapVector<ScriptSourceCode>& sources, int extensionGroup, Vector<v8::Local<v8::Value>>* results);

    // Returns true if argument is a JavaScript URL.
    bool executeScriptIfJavaScriptURL(const KURL&);

    // Returns true if the current world is isolated, and has its own Content
    // Security Policy. In this case, the policy of the main world should be
    // ignored when evaluating resources injected into the DOM.
    bool shouldBypassMainWorldCSP();

    PassRefPtr<SharedPersistent<v8::Object>> createPluginWrapper(Widget*);

    void enableEval();
    void disableEval(const String& errorMessage);

    static bool canAccessFromCurrentOrigin(v8::Isolate*, Frame*);

    void collectIsolatedContexts(Vector<std::pair<ScriptState*, SecurityOrigin*>>&);

    bool canExecuteScripts(ReasonForCallingCanExecuteScripts);

    TextPosition eventHandlerPosition() const;

    void clearWindowProxy();
    void updateDocument();

    void namedItemAdded(HTMLDocument*, const AtomicString&);
    void namedItemRemoved(HTMLDocument*, const AtomicString&);

    void updateSecurityOrigin(SecurityOrigin*);

    void clearForClose();

    // Registers a v8 extension to be available on webpages. Will only
    // affect v8 contexts initialized after this call. Takes ownership of
    // the v8::Extension object passed.
    static void registerExtensionIfNeeded(v8::Extension*);
    static V8Extensions& registeredExtensions();

    v8::Isolate* isolate() const { return m_windowProxyManager->isolate(); }

    WindowProxyManager* getWindowProxyManager() const { return m_windowProxyManager.get(); }

private:
    explicit ScriptController(LocalFrame*);

    LocalFrame* frame() const { return toLocalFrame(m_windowProxyManager->frame()); }

    v8::Local<v8::Value> evaluateScriptInMainWorld(const ScriptSourceCode&, AccessControlStatus, ExecuteScriptPolicy);

    Member<WindowProxyManager> m_windowProxyManager;
};

} // namespace blink

#endif // ScriptController_h
