/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#ifndef AudioBufferSourceNode_h
#define AudioBufferSourceNode_h

#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioParam.h"
#include "modules/webaudio/AudioScheduledSourceNode.h"
#include "modules/webaudio/PannerNode.h"
#include "platform/audio/AudioBus.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"
#include <memory>

namespace blink {

class AudioBufferSourceOptions;
class BaseAudioContext;

// AudioBufferSourceNode is an AudioNode representing an audio source from an
// in-memory audio asset represented by an AudioBuffer.  It generally will be
// used for short sounds which require a high degree of scheduling flexibility
// (can playback in rhythmically perfect ways).

class AudioBufferSourceHandler final : public AudioScheduledSourceHandler {
 public:
  static PassRefPtr<AudioBufferSourceHandler> create(
      AudioNode&,
      float sampleRate,
      AudioParamHandler& playbackRate,
      AudioParamHandler& detune);
  ~AudioBufferSourceHandler() override;

  // AudioHandler
  void process(size_t framesToProcess) override;

  // setBuffer() is called on the main thread. This is the buffer we use for
  // playback.
  void setBuffer(AudioBuffer*, ExceptionState&);
  AudioBuffer* buffer() { return m_buffer.get(); }

  // numberOfChannels() returns the number of output channels.  This value
  // equals the number of channels from the buffer.  If a new buffer is set with
  // a different number of channels, then this value will dynamically change.
  unsigned numberOfChannels();

  // Play-state
  void start(double when, ExceptionState&);
  void start(double when, double grainOffset, ExceptionState&);
  void start(double when,
             double grainOffset,
             double grainDuration,
             ExceptionState&);

  // Note: the attribute was originally exposed as |.looping|, but to be more
  // consistent in naming with <audio> and with how it's described in the
  // specification, the proper attribute name is |.loop|. The old attribute is
  // kept for backwards compatibility.
  bool loop() const { return m_isLooping; }
  void setLoop(bool looping) {
    m_isLooping = looping;
    setDidSetLooping(looping);
  }

  // Loop times in seconds.
  double loopStart() const { return m_loopStart; }
  double loopEnd() const { return m_loopEnd; }
  void setLoopStart(double loopStart) { m_loopStart = loopStart; }
  void setLoopEnd(double loopEnd) { m_loopEnd = loopEnd; }

  // If we are no longer playing, propogate silence ahead to downstream nodes.
  bool propagatesSilence() const override;

  void handleStoppableSourceNode();

 private:
  AudioBufferSourceHandler(AudioNode&,
                           float sampleRate,
                           AudioParamHandler& playbackRate,
                           AudioParamHandler& detune);
  void startSource(double when,
                   double grainOffset,
                   double grainDuration,
                   bool isDurationGiven,
                   ExceptionState&);

  // Returns true on success.
  bool renderFromBuffer(AudioBus*,
                        unsigned destinationFrameOffset,
                        size_t numberOfFrames);

  // Render silence starting from "index" frame in AudioBus.
  inline bool renderSilenceAndFinishIfNotLooping(AudioBus*,
                                                 unsigned index,
                                                 size_t framesToProcess);

  // Clamps grain parameters to the duration of the given AudioBuffer.
  void clampGrainParameters(const AudioBuffer*);

  // m_buffer holds the sample data which this node outputs.
  // This Persistent doesn't make a reference cycle including
  // AudioBufferSourceNode.
  // It is cross-thread, as it will be accessed by the audio and main threads.
  CrossThreadPersistent<AudioBuffer> m_buffer;

  // Pointers for the buffer and destination.
  std::unique_ptr<const float* []> m_sourceChannels;
  std::unique_ptr<float* []> m_destinationChannels;

  RefPtr<AudioParamHandler> m_playbackRate;
  RefPtr<AudioParamHandler> m_detune;

  bool didSetLooping() const { return acquireLoad(&m_didSetLooping); }
  void setDidSetLooping(bool loop) {
    bool newLooping = didSetLooping() || loop;
    releaseStore(&m_didSetLooping, newLooping);
  }

  // If m_isLooping is false, then this node will be done playing and become
  // inactive after it reaches the end of the sample data in the buffer.  If
  // true, it will wrap around to the start of the buffer each time it reaches
  // the end.
  bool m_isLooping;

  // True if the source .loop attribute was ever set.
  int m_didSetLooping;

  double m_loopStart;
  double m_loopEnd;

  // m_virtualReadIndex is a sample-frame index into our buffer representing the
  // current playback position.  Since it's floating-point, it has sub-sample
  // accuracy.
  double m_virtualReadIndex;

  // Granular playback
  bool m_isGrain;
  double m_grainOffset;    // in seconds
  double m_grainDuration;  // in seconds
  // True if grainDuration is given explicitly (via 3 arg start method).
  bool m_isDurationGiven;

  // Compute playback rate (k-rate) by incorporating the sample rate
  // conversion factor, and the value of playbackRate and detune AudioParams.
  double computePlaybackRate();

  // The minimum playbackRate value ever used for this source.
  double m_minPlaybackRate;
};

class AudioBufferSourceNode final : public AudioScheduledSourceNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioBufferSourceNode* create(BaseAudioContext&, ExceptionState&);
  static AudioBufferSourceNode* create(BaseAudioContext*,
                                       AudioBufferSourceOptions&,
                                       ExceptionState&);
  DECLARE_VIRTUAL_TRACE();
  AudioBufferSourceHandler& audioBufferSourceHandler() const;

  AudioBuffer* buffer() const;
  void setBuffer(AudioBuffer*, ExceptionState&);
  AudioParam* playbackRate() const;
  AudioParam* detune() const;
  bool loop() const;
  void setLoop(bool);
  double loopStart() const;
  void setLoopStart(double);
  double loopEnd() const;
  void setLoopEnd(double);

  void start(ExceptionState&);
  void start(double when, ExceptionState&);
  void start(double when, double grainOffset, ExceptionState&);
  void start(double when,
             double grainOffset,
             double grainDuration,
             ExceptionState&);

 private:
  AudioBufferSourceNode(BaseAudioContext&);

  Member<AudioParam> m_playbackRate;
  Member<AudioParam> m_detune;
};

}  // namespace blink

#endif  // AudioBufferSourceNode_h
