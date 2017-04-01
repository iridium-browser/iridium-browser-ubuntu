/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef MIDIAccess_h
#define MIDIAccess_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "media/midi/midi_service.mojom-blink.h"
#include "modules/EventTargetModules.h"
#include "modules/webmidi/MIDIAccessInitializer.h"
#include "modules/webmidi/MIDIAccessor.h"
#include "modules/webmidi/MIDIAccessorClient.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class ExecutionContext;
class MIDIInput;
class MIDIInputMap;
class MIDIOutput;
class MIDIOutputMap;

class MIDIAccess final : public EventTargetWithInlineData,
                         public ActiveScriptWrappable<MIDIAccess>,
                         public ContextLifecycleObserver,
                         public MIDIAccessorClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MIDIAccess);
  USING_PRE_FINALIZER(MIDIAccess, dispose);

 public:
  static MIDIAccess* create(
      std::unique_ptr<MIDIAccessor> accessor,
      bool sysexEnabled,
      const Vector<MIDIAccessInitializer::PortDescriptor>& ports,
      ExecutionContext* executionContext) {
    return new MIDIAccess(std::move(accessor), sysexEnabled, ports,
                          executionContext);
  }
  ~MIDIAccess() override;

  MIDIInputMap* inputs() const;
  MIDIOutputMap* outputs() const;

  EventListener* onstatechange();
  void setOnstatechange(EventListener*);

  bool sysexEnabled() const { return m_sysexEnabled; }

  // EventTarget
  const AtomicString& interfaceName() const override {
    return EventTargetNames::MIDIAccess;
  }
  ExecutionContext* getExecutionContext() const override {
    return ContextLifecycleObserver::getExecutionContext();
  }

  // ScriptWrappable
  bool hasPendingActivity() const final;

  // ContextLifecycleObserver
  void contextDestroyed(ExecutionContext*) override;

  // MIDIAccessorClient
  void didAddInputPort(const String& id,
                       const String& manufacturer,
                       const String& name,
                       const String& version,
                       midi::mojom::PortState) override;
  void didAddOutputPort(const String& id,
                        const String& manufacturer,
                        const String& name,
                        const String& version,
                        midi::mojom::PortState) override;
  void didSetInputPortState(unsigned portIndex,
                            midi::mojom::PortState) override;
  void didSetOutputPortState(unsigned portIndex,
                             midi::mojom::PortState) override;
  void didStartSession(midi::mojom::Result) override {
    // This method is for MIDIAccess initialization: MIDIAccessInitializer
    // has the implementation.
    NOTREACHED();
  }
  void didReceiveMIDIData(unsigned portIndex,
                          const unsigned char* data,
                          size_t length,
                          double timeStamp) override;

  // |timeStampInMilliseconds| is in the same time coordinate system as
  // performance.now().
  void sendMIDIData(unsigned portIndex,
                    const unsigned char* data,
                    size_t length,
                    double timeStampInMilliseconds);

  // Eager finalization needed to promptly release m_accessor. Otherwise
  // its client back reference could end up being unsafely used during
  // the lazy sweeping phase.
  DECLARE_VIRTUAL_TRACE();

 private:
  MIDIAccess(std::unique_ptr<MIDIAccessor>,
             bool sysexEnabled,
             const Vector<MIDIAccessInitializer::PortDescriptor>&,
             ExecutionContext*);
  void dispose();

  std::unique_ptr<MIDIAccessor> m_accessor;
  bool m_sysexEnabled;
  bool m_hasPendingActivity;
  HeapVector<Member<MIDIInput>> m_inputs;
  HeapVector<Member<MIDIOutput>> m_outputs;
};

}  // namespace blink

#endif  // MIDIAccess_h
