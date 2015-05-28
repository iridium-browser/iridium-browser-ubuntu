/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/inspector/InspectorCanvasAgent.h"

#include "bindings/core/v8/ScriptProfiler.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/inspector/BindingVisitors.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptCanvasModule.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/DocumentLoader.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"

using blink::TypeBuilder::Array;
using blink::TypeBuilder::Canvas::ResourceId;
using blink::TypeBuilder::Canvas::ResourceState;
using blink::TypeBuilder::Canvas::TraceLog;
using blink::TypeBuilder::Canvas::TraceLogId;
using blink::TypeBuilder::Page::FrameId;
using blink::TypeBuilder::Runtime::RemoteObject;

namespace blink {

namespace CanvasAgentState {
static const char canvasAgentEnabled[] = "canvasAgentEnabled";
};

InspectorCanvasAgent::InspectorCanvasAgent(InspectorPageAgent* pageAgent, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorCanvasAgent, InspectorFrontend::Canvas>("Canvas")
    , m_pageAgent(pageAgent)
    , m_injectedScriptManager(injectedScriptManager)
    , m_enabled(false)
{
}

InspectorCanvasAgent::~InspectorCanvasAgent()
{
}

DEFINE_TRACE(InspectorCanvasAgent)
{
    visitor->trace(m_pageAgent);
    visitor->trace(m_injectedScriptManager);
    InspectorBaseAgent::trace(visitor);
}

void InspectorCanvasAgent::restore()
{
    if (m_state->getBoolean(CanvasAgentState::canvasAgentEnabled)) {
        ErrorString error;
        enable(&error);
    }
}

void InspectorCanvasAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    m_state->setBoolean(CanvasAgentState::canvasAgentEnabled, m_enabled);
    m_instrumentingAgents->setInspectorCanvasAgent(this);
    findFramesWithUninstrumentedCanvases();
}

void InspectorCanvasAgent::disable(ErrorString*)
{
    m_enabled = false;
    m_state->setBoolean(CanvasAgentState::canvasAgentEnabled, m_enabled);
    m_instrumentingAgents->setInspectorCanvasAgent(0);
    m_framesWithUninstrumentedCanvases.clear();
    if (frontend())
        frontend()->traceLogsRemoved(0, 0);
}

void InspectorCanvasAgent::dropTraceLog(ErrorString* errorString, const TraceLogId& traceLogId)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (module.isEmpty())
        return;
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(traceLogId);
    if (injectedScript.isEmpty())
        return;
    injectedScript.releaseObjectGroup(traceLogId);
    module.dropTraceLog(errorString, traceLogId);
}

void InspectorCanvasAgent::hasUninstrumentedCanvases(ErrorString* errorString, bool* result)
{
    if (!checkIsEnabled(errorString))
        return;
    for (const auto& frame : m_framesWithUninstrumentedCanvases) {
        if (frame.value) {
            *result = true;
            return;
        }
    }
    *result = false;
}

void InspectorCanvasAgent::captureFrame(ErrorString* errorString, const FrameId* frameId, TraceLogId* traceLogId)
{
    LocalFrame* frame = frameId ? m_pageAgent->assertFrame(errorString, *frameId) : m_pageAgent->inspectedFrame();
    if (!frame)
        return;
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, ScriptState::forMainWorld(frame));
    if (!module.isEmpty())
        module.captureFrame(errorString, traceLogId);
}

void InspectorCanvasAgent::startCapturing(ErrorString* errorString, const FrameId* frameId, TraceLogId* traceLogId)
{
    LocalFrame* frame = frameId ? m_pageAgent->assertFrame(errorString, *frameId) : m_pageAgent->inspectedFrame();
    if (!frame)
        return;
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, ScriptState::forMainWorld(frame));
    if (!module.isEmpty())
        module.startCapturing(errorString, traceLogId);
}

void InspectorCanvasAgent::stopCapturing(ErrorString* errorString, const TraceLogId& traceLogId)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (!module.isEmpty())
        module.stopCapturing(errorString, traceLogId);
}

void InspectorCanvasAgent::getTraceLog(ErrorString* errorString, const TraceLogId& traceLogId, const int* startOffset, const int* maxLength, RefPtr<TraceLog>& traceLog)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (!module.isEmpty())
        module.traceLog(errorString, traceLogId, startOffset, maxLength, &traceLog);
}

void InspectorCanvasAgent::replayTraceLog(ErrorString* errorString, const TraceLogId& traceLogId, int stepNo, RefPtr<ResourceState>& result, double* replayTime)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (module.isEmpty())
        return;
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(traceLogId);
    if (injectedScript.isEmpty())
        return;
    injectedScript.releaseObjectGroup(traceLogId);
    module.replayTraceLog(errorString, traceLogId, stepNo, &result, replayTime);
}

void InspectorCanvasAgent::getResourceState(ErrorString* errorString, const TraceLogId& traceLogId, const ResourceId& resourceId, RefPtr<ResourceState>& result)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (!module.isEmpty())
        module.resourceState(errorString, traceLogId, resourceId, &result);
}

void InspectorCanvasAgent::evaluateTraceLogCallArgument(ErrorString* errorString, const TraceLogId& traceLogId, int callIndex, int argumentIndex, const String* objectGroup, RefPtr<RemoteObject>& result, RefPtr<ResourceState>& resourceState)
{
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(errorString, traceLogId);
    if (!module.isEmpty())
        module.evaluateTraceLogCallArgument(errorString, traceLogId, callIndex, argumentIndex, objectGroup ? *objectGroup : String(), &result, &resourceState);
}

ScriptValue InspectorCanvasAgent::wrapCanvas2DRenderingContextForInstrumentation(const ScriptValue& context)
{
    ErrorString error;
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(&error, context);
    if (module.isEmpty())
        return ScriptValue();
    return notifyRenderingContextWasWrapped(module.wrapCanvas2DContext(context));
}

ScriptValue InspectorCanvasAgent::wrapWebGLRenderingContextForInstrumentation(const ScriptValue& glContext)
{
    ErrorString error;
    InjectedScriptCanvasModule module = injectedScriptCanvasModule(&error, glContext);
    if (module.isEmpty())
        return ScriptValue();
    return notifyRenderingContextWasWrapped(module.wrapWebGLContext(glContext));
}

ScriptValue InspectorCanvasAgent::notifyRenderingContextWasWrapped(const ScriptValue& wrappedContext)
{
    ASSERT(frontend());
    ScriptState* scriptState = wrappedContext.scriptState();
    LocalDOMWindow* domWindow = 0;
    if (scriptState)
        domWindow = scriptState->domWindow();
    LocalFrame* frame = domWindow ? domWindow->frame() : 0;
    if (frame && !m_framesWithUninstrumentedCanvases.contains(frame))
        m_framesWithUninstrumentedCanvases.set(frame, false);
    String frameId = m_pageAgent->frameId(frame);
    if (!frameId.isEmpty())
        frontend()->contextCreated(frameId);
    return wrappedContext;
}

InjectedScriptCanvasModule InspectorCanvasAgent::injectedScriptCanvasModule(ErrorString* errorString, ScriptState* scriptState)
{
    if (!checkIsEnabled(errorString))
        return InjectedScriptCanvasModule();
    InjectedScriptCanvasModule module = InjectedScriptCanvasModule::moduleForState(m_injectedScriptManager, scriptState);
    if (module.isEmpty()) {
        ASSERT_NOT_REACHED();
        *errorString = "Internal error: no Canvas module";
    }
    return module;
}

InjectedScriptCanvasModule InspectorCanvasAgent::injectedScriptCanvasModule(ErrorString* errorString, const ScriptValue& scriptValue)
{
    if (!checkIsEnabled(errorString))
        return InjectedScriptCanvasModule();
    if (scriptValue.isEmpty()) {
        ASSERT_NOT_REACHED();
        *errorString = "Internal error: original ScriptValue has no value";
        return InjectedScriptCanvasModule();
    }
    return injectedScriptCanvasModule(errorString, scriptValue.scriptState());
}

InjectedScriptCanvasModule InspectorCanvasAgent::injectedScriptCanvasModule(ErrorString* errorString, const String& objectId)
{
    if (!checkIsEnabled(errorString))
        return InjectedScriptCanvasModule();
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return InjectedScriptCanvasModule();
    }
    return injectedScriptCanvasModule(errorString, injectedScript.scriptState());
}

void InspectorCanvasAgent::findFramesWithUninstrumentedCanvases()
{
    class NodeVisitor final : public WrappedNodeVisitor {
    public:
        NodeVisitor(LocalFrame* frame, FramesWithUninstrumentedCanvases& result)
            : m_frame(frame)
            , m_framesWithUninstrumentedCanvases(result)
        {
        }

        virtual void visitNode(Node* node) override
        {
            ASSERT(node);
            if (!isHTMLCanvasElement(*node) || !node->document().frame())
                return;

            LocalFrame* frame = node->document().frame();
            if (frame->localFrameRoot() != m_frame)
                return;

            if (toHTMLCanvasElement(node)->renderingContext())
                m_framesWithUninstrumentedCanvases.set(frame, true);
        }

    private:
        LocalFrame* m_frame;
        FramesWithUninstrumentedCanvases& m_framesWithUninstrumentedCanvases;
    } nodeVisitor(m_pageAgent->inspectedFrame(), m_framesWithUninstrumentedCanvases);

    m_framesWithUninstrumentedCanvases.clear();
    ScriptProfiler::visitNodeWrappers(&nodeVisitor);

    if (frontend()) {
        for (const auto& frame : m_framesWithUninstrumentedCanvases) {
            String frameId = m_pageAgent->frameId(frame.key);
            if (!frameId.isEmpty())
                frontend()->contextCreated(frameId);
        }
    }
}

bool InspectorCanvasAgent::checkIsEnabled(ErrorString* errorString) const
{
    if (m_enabled)
        return true;
    *errorString = "Canvas agent is not enabled";
    return false;
}

void InspectorCanvasAgent::didCommitLoad(LocalFrame*, DocumentLoader* loader)
{
    if (!m_enabled)
        return;
    Frame* frame = loader->frame();
    if (frame == m_pageAgent->inspectedFrame()) {
        for (auto& frame : m_framesWithUninstrumentedCanvases)
            frame.value = false;
        frontend()->traceLogsRemoved(0, 0);
    } else {
        while (frame) {
            if (frame->isLocalFrame()) {
                LocalFrame* localFrame = toLocalFrame(frame);
                if (m_framesWithUninstrumentedCanvases.contains(localFrame))
                    m_framesWithUninstrumentedCanvases.set(localFrame, false);
                if (m_pageAgent->hasIdForFrame(localFrame)) {
                    String frameId = m_pageAgent->frameId(localFrame);
                    frontend()->traceLogsRemoved(&frameId, 0);
                }
            }
            frame = frame->tree().traverseNext();
        }
    }
}

void InspectorCanvasAgent::frameDetachedFromParent(LocalFrame* frame)
{
    m_framesWithUninstrumentedCanvases.remove(frame);
}

void InspectorCanvasAgent::didProcessTask()
{
    ErrorString error;
    for (const auto& frame : m_framesWithUninstrumentedCanvases) {
        InjectedScriptCanvasModule module = injectedScriptCanvasModule(&error, ScriptState::forMainWorld(frame.key));
        if (!module.isEmpty())
            module.markFrameEnd();
    }
}

} // namespace blink

