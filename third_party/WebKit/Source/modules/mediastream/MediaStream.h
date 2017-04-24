/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStream_h
#define MediaStream_h

#include "core/html/URLRegistry.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "platform/Timer.h"
#include "platform/mediastream/MediaStreamDescriptor.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptState;

class MODULES_EXPORT MediaStream final : public EventTargetWithInlineData,
                                         public ContextClient,
                                         public URLRegistrable,
                                         public MediaStreamDescriptorClient {
  USING_GARBAGE_COLLECTED_MIXIN(MediaStream);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaStream* create(ExecutionContext*);
  static MediaStream* create(ExecutionContext*, MediaStream*);
  static MediaStream* create(ExecutionContext*, const MediaStreamTrackVector&);
  static MediaStream* create(ExecutionContext*, MediaStreamDescriptor*);
  ~MediaStream() override;

  String id() const { return m_descriptor->id(); }

  void addTrack(MediaStreamTrack*, ExceptionState&);
  void removeTrack(MediaStreamTrack*, ExceptionState&);
  MediaStreamTrack* getTrackById(String);
  MediaStream* clone(ScriptState*);

  MediaStreamTrackVector getAudioTracks() const { return m_audioTracks; }
  MediaStreamTrackVector getVideoTracks() const { return m_videoTracks; }
  MediaStreamTrackVector getTracks();

  bool active() const { return m_descriptor->active(); }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(active);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(inactive);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(removetrack);

  void trackEnded();

  // MediaStreamDescriptorClient
  void streamEnded() override;

  MediaStreamDescriptor* descriptor() const { return m_descriptor; }

  // EventTarget
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override {
    return ContextClient::getExecutionContext();
  }

  // URLRegistrable
  URLRegistry& registry() const override;

  DECLARE_VIRTUAL_TRACE();

 protected:
  bool addEventListenerInternal(
      const AtomicString& eventType,
      EventListener*,
      const AddEventListenerOptionsResolved&) override;

 private:
  MediaStream(ExecutionContext*, MediaStreamDescriptor*);
  MediaStream(ExecutionContext*,
              const MediaStreamTrackVector& audioTracks,
              const MediaStreamTrackVector& videoTracks);

  // MediaStreamDescriptorClient
  void addRemoteTrack(MediaStreamComponent*) override;
  void removeRemoteTrack(MediaStreamComponent*) override;

  bool emptyOrOnlyEndedTracks();

  void scheduleDispatchEvent(Event*);
  void scheduledEventTimerFired(TimerBase*);

  MediaStreamTrackVector m_audioTracks;
  MediaStreamTrackVector m_videoTracks;
  Member<MediaStreamDescriptor> m_descriptor;

  TaskRunnerTimer<MediaStream> m_scheduledEventTimer;
  HeapVector<Member<Event>> m_scheduledEvents;
};

using MediaStreamVector = HeapVector<Member<MediaStream>>;

MediaStream* toMediaStream(MediaStreamDescriptor*);

}  // namespace blink

#endif  // MediaStream_h
