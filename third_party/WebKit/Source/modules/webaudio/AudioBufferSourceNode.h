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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioBufferSourceNode_h
#define AudioBufferSourceNode_h

#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioParam.h"
#include "modules/webaudio/AudioScheduledSourceNode.h"
#include "modules/webaudio/PannerNode.h"
#include "platform/audio/AudioBus.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"

namespace blink {

class AudioContext;

// AudioBufferSourceNode is an AudioNode representing an audio source from an in-memory audio asset represented by an AudioBuffer.
// It generally will be used for short sounds which require a high degree of scheduling flexibility (can playback in rhythmically perfect ways).

class AudioBufferSourceHandler final : public AudioScheduledSourceHandler {
public:
    AudioBufferSourceHandler(AudioNode&, float sampleRate);
    virtual ~AudioBufferSourceHandler();

    // AudioHandler
    virtual void dispose() override;
    virtual void process(size_t framesToProcess) override;

    // setBuffer() is called on the main thread. This is the buffer we use for playback.
    void setBuffer(AudioBuffer*, ExceptionState&);
    AudioBuffer* buffer() { return m_buffer.get(); }

    // numberOfChannels() returns the number of output channels.  This value equals the number of channels from the buffer.
    // If a new buffer is set with a different number of channels, then this value will dynamically change.
    unsigned numberOfChannels();

    // Play-state
    void start(ExceptionState& exceptionState) { start(0, exceptionState); }
    void start(double when, ExceptionState&);
    void start(double when, double grainOffset, ExceptionState&);
    void start(double when, double grainOffset, double grainDuration, ExceptionState&);

    // Note: the attribute was originally exposed as .looping, but to be more consistent in naming with <audio>
    // and with how it's described in the specification, the proper attribute name is .loop
    // The old attribute is kept for backwards compatibility.
    bool loop() const { return m_isLooping; }
    void setLoop(bool looping) { m_isLooping = looping; }

    // Loop times in seconds.
    double loopStart() const { return m_loopStart; }
    double loopEnd() const { return m_loopEnd; }
    void setLoopStart(double loopStart) { m_loopStart = loopStart; }
    void setLoopEnd(double loopEnd) { m_loopEnd = loopEnd; }

    AudioParam* playbackRate() { return m_playbackRate.get(); }

    // If a panner node is set, then we can incorporate doppler shift into the playback pitch rate.
    void setPannerNode(PannerNode*);
    void clearPannerNode();

    // If we are no longer playing, propogate silence ahead to downstream nodes.
    virtual bool propagatesSilence() const override;

    // AudioScheduledSourceNode
    virtual void finish() override;

    void handleStoppableSourceNode();

    DECLARE_VIRTUAL_TRACE();

private:
    void startSource(double when, double grainOffset, double grainDuration, bool isDurationGiven, ExceptionState&);

    // Returns true on success.
    bool renderFromBuffer(AudioBus*, unsigned destinationFrameOffset, size_t numberOfFrames);

    // Render silence starting from "index" frame in AudioBus.
    inline bool renderSilenceAndFinishIfNotLooping(AudioBus*, unsigned index, size_t framesToProcess);

    // Clamps grain parameters to the duration of the given AudioBuffer.
    void clampGrainParameters(const AudioBuffer*);

    // m_buffer holds the sample data which this node outputs.
    Member<AudioBuffer> m_buffer;

    // Pointers for the buffer and destination.
    OwnPtr<const float*[]> m_sourceChannels;
    OwnPtr<float*[]> m_destinationChannels;

    // Used for the "playbackRate" attributes.
    Member<AudioParam> m_playbackRate;

    // If m_isLooping is false, then this node will be done playing and become inactive after it reaches the end of the sample data in the buffer.
    // If true, it will wrap around to the start of the buffer each time it reaches the end.
    bool m_isLooping;

    double m_loopStart;
    double m_loopEnd;

    // m_virtualReadIndex is a sample-frame index into our buffer representing the current playback position.
    // Since it's floating-point, it has sub-sample accuracy.
    double m_virtualReadIndex;

    // Granular playback
    bool m_isGrain;
    double m_grainOffset; // in seconds
    double m_grainDuration; // in seconds
    // True if grainDuration is given explicitly (via 3 arg start method).
    bool m_isDurationGiven;
    // totalPitchRate() returns the instantaneous pitch rate (non-time preserving).
    // It incorporates the base pitch rate, any sample-rate conversion factor from the buffer, and any doppler shift from an associated panner node.
    double totalPitchRate();

    // We optionally keep track of a panner node which has a doppler shift that
    // is incorporated into the pitch rate.
    // This RefPtr is connection reference. We must call AudioNode::
    // makeConnection() after ref(), and call AudioNode::breakConnection()
    // before deref().
    // Oilpan: This holds connection references. We must call
    // AudioNode::makeConnection when we add an AudioNode to this, and must call
    // AudioNode::breakConnection() when we remove an AudioNode from this.
    Member<PannerNode> m_pannerNode;

    // This synchronizes process() with setBuffer() which can cause dynamic channel count changes.
    mutable Mutex m_processLock;
};

class AudioBufferSourceNode final : public AudioScheduledSourceNode {
    DEFINE_WRAPPERTYPEINFO();
public:
    static AudioBufferSourceNode* create(AudioContext*, float sampleRate);
    AudioBufferSourceHandler& audioBufferSourceHandler() const;

    AudioBuffer* buffer() const;
    void setBuffer(AudioBuffer*, ExceptionState&);
    AudioParam* playbackRate() const;
    bool loop() const;
    void setLoop(bool);
    double loopStart() const;
    void setLoopStart(double);
    double loopEnd() const;
    void setLoopEnd(double);

    void start(ExceptionState&);
    void start(double when, ExceptionState&);
    void start(double when, double grainOffset, ExceptionState&);
    void start(double when, double grainOffset, double grainDuration, ExceptionState&);

private:
    AudioBufferSourceNode(AudioContext&, float sampleRate);
};

} // namespace blink

#endif // AudioBufferSourceNode_h
