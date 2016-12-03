// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ProfilerAgentImpl_h
#define V8ProfilerAgentImpl_h

#include "platform/inspector_protocol/InspectorProtocol.h"
#include "platform/v8_inspector/protocol/Profiler.h"

#include <vector>

namespace v8 {
class CpuProfiler;
class Isolate;
}

namespace v8_inspector {

class V8InspectorSessionImpl;

namespace protocol = blink::protocol;

class V8ProfilerAgentImpl : public protocol::Profiler::Backend {
    PROTOCOL_DISALLOW_COPY(V8ProfilerAgentImpl);
public:
    V8ProfilerAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*, protocol::DictionaryValue* state);
    ~V8ProfilerAgentImpl() override;

    bool enabled() const { return m_enabled; }
    void restore();

    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void setSamplingInterval(ErrorString*, int) override;
    void start(ErrorString*) override;
    void stop(ErrorString*, std::unique_ptr<protocol::Profiler::Profile>*) override;

    void consoleProfile(const String16& title);
    void consoleProfileEnd(const String16& title);

private:
    String16 nextProfileId();
    v8::CpuProfiler* profiler();

    void startProfiling(const String16& title);
    std::unique_ptr<protocol::Profiler::Profile> stopProfiling(const String16& title, bool serialize);

    bool isRecording() const;

    V8InspectorSessionImpl* m_session;
    v8::Isolate* m_isolate;
    v8::CpuProfiler* m_profiler;
    protocol::DictionaryValue* m_state;
    protocol::Profiler::Frontend m_frontend;
    bool m_enabled;
    bool m_recordingCPUProfile;
    class ProfileDescriptor;
    std::vector<ProfileDescriptor> m_startedProfiles;
    String16 m_frontendInitiatedProfileId;
};

} // namespace v8_inspector

#endif // !defined(V8ProfilerAgentImpl_h)
