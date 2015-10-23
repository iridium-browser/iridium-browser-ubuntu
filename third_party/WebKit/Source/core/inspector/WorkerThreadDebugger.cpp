/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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
#include "core/inspector/WorkerThreadDebugger.h"

#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/inspector/WorkerDebuggerAgent.h"
#include "core/inspector/v8/V8DebuggerListener.h"
#include "core/workers/WorkerThread.h"
#include "wtf/MessageQueue.h"
#include <v8.h>

namespace blink {

static const int workerContextGroupId = 1;

WorkerThreadDebugger::WorkerThreadDebugger(WorkerThread* workerThread)
    : ScriptDebuggerBase(v8::Isolate::GetCurrent())
    , m_listener(nullptr)
    , m_workerThread(workerThread)
{
}

WorkerThreadDebugger::~WorkerThreadDebugger()
{
}

void WorkerThreadDebugger::setContextDebugData(v8::Local<v8::Context> context)
{
    V8Debugger::setContextDebugData(context, "worker", workerContextGroupId);
}

int WorkerThreadDebugger::contextGroupId()
{
    return workerContextGroupId;
}

void WorkerThreadDebugger::runMessageLoopOnPause(v8::Local<v8::Context>)
{
    MessageQueueWaitResult result;
    m_workerThread->willEnterNestedLoop();
    do {
        result = m_workerThread->runDebuggerTask();
    // Keep waiting until execution is resumed.
    } while (result == MessageQueueMessageReceived && debugger()->isPaused());
    m_workerThread->didLeaveNestedLoop();
}

void WorkerThreadDebugger::quitMessageLoopOnPause()
{
    // Nothing to do here in case of workers since runMessageLoopOnPause will check for paused state after each debugger command.
}

} // namespace blink
