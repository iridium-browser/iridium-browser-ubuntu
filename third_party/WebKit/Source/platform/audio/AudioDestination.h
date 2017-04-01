/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioDestination_h
#define AudioDestination_h

#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioIOCallback.h"
#include "platform/audio/AudioSourceProvider.h"
#include "public/platform/WebAudioDevice.h"
#include "public/platform/WebVector.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class AudioPullFIFO;
class SecurityOrigin;

// The AudioDestination class is an audio sink interface between the media
// renderer and the Blink's WebAudio module. It has a FIFO to adapt the
// different processing block sizes of WebAudio renderer and actual hardware
// audio callback.
class PLATFORM_EXPORT AudioDestination : public WebAudioDevice::RenderCallback,
                                         public AudioSourceProvider {
  USING_FAST_MALLOC(AudioDestination);
  WTF_MAKE_NONCOPYABLE(AudioDestination);

 public:
  AudioDestination(AudioIOCallback&,
                   unsigned numberOfOutputChannels,
                   float sampleRate,
                   PassRefPtr<SecurityOrigin>);
  ~AudioDestination() override;

  static std::unique_ptr<AudioDestination> create(
      AudioIOCallback&,
      unsigned numberOfOutputChannels,
      float sampleRate,
      PassRefPtr<SecurityOrigin>);

  // The actual render function (WebAudioDevice::RenderCallback) isochronously
  // invoked by the media renderer.
  void render(const WebVector<float*>& destinationData,
              size_t numberOfFrames,
              double delay,
              double delayTimestamp,
              size_t priorFramesSkipped) override;

  // AudioSourceProvider (FIFO)
  void provideInput(AudioBus* outputBus, size_t framesToProcess) override;

  virtual void start();
  virtual void stop();

  size_t callbackBufferSize() const { return m_callbackBufferSize; }
  float sampleRate() const { return m_sampleRate; }
  bool isPlaying() { return m_isPlaying; }

  // The information from the actual audio hardware. (via Platform::current)
  static float hardwareSampleRate();
  static unsigned long maxChannelCount();

 private:
  std::unique_ptr<WebAudioDevice> m_webAudioDevice;
  unsigned m_numberOfOutputChannels;
  size_t m_callbackBufferSize;
  float m_sampleRate;
  bool m_isPlaying;

  // The render callback function of WebAudio engine. (i.e. DestinationNode)
  AudioIOCallback& m_callback;

  RefPtr<AudioBus> m_outputBus;
  std::unique_ptr<AudioPullFIFO> m_fifo;

  size_t m_framesElapsed;
  AudioIOPosition m_outputPosition;
  base::TimeTicks m_outputPositionReceivedTimestamp;

  // Calculate the optimum buffer size for a given platform. Return false if the
  // buffer size calculation fails.
  bool calculateBufferSize();

  size_t hardwareBufferSize();
};

}  // namespace blink

#endif  // AudioDestination_h
