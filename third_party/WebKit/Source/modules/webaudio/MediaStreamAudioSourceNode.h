/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef MediaStreamAudioSourceNode_h
#define MediaStreamAudioSourceNode_h

#include "modules/mediastream/MediaStream.h"
#include "modules/webaudio/AudioNode.h"
#include "platform/audio/AudioSourceProvider.h"
#include "platform/audio/AudioSourceProviderClient.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Threading.h"
#include <memory>

namespace blink {

class BaseAudioContext;
class MediaStreamAudioSourceOptions;

class MediaStreamAudioSourceHandler final : public AudioHandler {
 public:
  static PassRefPtr<MediaStreamAudioSourceHandler> create(
      AudioNode&,
      std::unique_ptr<AudioSourceProvider>);
  ~MediaStreamAudioSourceHandler() override;

  // AudioHandler
  void process(size_t framesToProcess) override;

  // A helper for AudioSourceProviderClient implementation of
  // MediaStreamAudioSourceNode.
  void setFormat(size_t numberOfChannels, float sampleRate);

 private:
  MediaStreamAudioSourceHandler(AudioNode&,
                                std::unique_ptr<AudioSourceProvider>);

  // As an audio source, we will never propagate silence.
  bool propagatesSilence() const override { return false; }

  AudioSourceProvider* getAudioSourceProvider() const {
    return m_audioSourceProvider.get();
  }

  std::unique_ptr<AudioSourceProvider> m_audioSourceProvider;

  Mutex m_processLock;

  unsigned m_sourceNumberOfChannels;
};

class MediaStreamAudioSourceNode final : public AudioNode,
                                         public AudioSourceProviderClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MediaStreamAudioSourceNode);

 public:
  static MediaStreamAudioSourceNode* create(BaseAudioContext&,
                                            MediaStream&,
                                            ExceptionState&);
  static MediaStreamAudioSourceNode* create(
      BaseAudioContext*,
      const MediaStreamAudioSourceOptions&,
      ExceptionState&);

  DECLARE_VIRTUAL_TRACE();

  MediaStream* getMediaStream() const;

  // AudioSourceProviderClient functions:
  void setFormat(size_t numberOfChannels, float sampleRate) override;

 private:
  MediaStreamAudioSourceNode(BaseAudioContext&,
                             MediaStream&,
                             MediaStreamTrack*,
                             std::unique_ptr<AudioSourceProvider>);

  MediaStreamAudioSourceHandler& mediaStreamAudioSourceHandler() const;

  Member<MediaStreamTrack> m_audioTrack;
  Member<MediaStream> m_mediaStream;
};

}  // namespace blink

#endif  // MediaStreamAudioSourceNode_h
