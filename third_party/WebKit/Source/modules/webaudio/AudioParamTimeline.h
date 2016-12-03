/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef AudioParamTimeline_h
#define AudioParamTimeline_h

#include "core/dom/DOMTypedArray.h"
#include "modules/webaudio/AudioDestinationNode.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "wtf/Forward.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"

namespace blink {

class AudioParamTimeline {
    DISALLOW_NEW();
public:
    AudioParamTimeline()
    {
    }

    void setValueAtTime(float value, double time, ExceptionState&);
    void linearRampToValueAtTime(float value, double time, float initialValue, double callTime, ExceptionState&);
    void exponentialRampToValueAtTime(float value, double time,  float initialValue, double callTime, ExceptionState&);
    void setTargetAtTime(float target, double time, double timeConstant, ExceptionState&);
    void setValueCurveAtTime(DOMFloat32Array* curve, double time, double duration, ExceptionState&);
    void cancelScheduledValues(double startTime, ExceptionState&);

    // hasValue is set to true if a valid timeline value is returned.
    // otherwise defaultValue is returned.
    float valueForContextTime(AudioDestinationHandler&, float defaultValue, bool& hasValue, float minValue, float maxValue);

    // Given the time range in frames, calculates parameter values into the values buffer and
    // returns the last parameter value calculated for "values" or the defaultValue if none were
    // calculated.  controlRate is the rate (number per second) at which parameter values will be
    // calculated.  It should equal sampleRate for sample-accurate parameter changes, and otherwise
    // will usually match the render quantum size such that the parameter value changes once per
    // render quantum.
    float valuesForFrameRange(size_t startFrame, size_t endFrame, float defaultValue, float* values, unsigned numberOfValues, double sampleRate, double controlRate, float minValue, float maxValue);

    // Returns true if this AudioParam has any events on it.
    bool hasValues() const;

    float smoothedValue() { return m_smoothedValue; }
    void setSmoothedValue(float v) { m_smoothedValue = v; }
private:
    class ParamEvent {
    public:
        enum Type {
            SetValue,
            LinearRampToValue,
            ExponentialRampToValue,
            SetTarget,
            SetValueCurve,
            LastType
        };

        static ParamEvent createLinearRampEvent(float value, double time, float initialValue, double callTime);
        static ParamEvent createExponentialRampEvent(float value, double time, float initialValue, double callTime);
        static ParamEvent createSetValueEvent(float value, double time);
        static ParamEvent createSetTargetEvent(float value, double time, double timeConstant);
        static ParamEvent createSetValueCurveEvent(const DOMFloat32Array* curve, double time, double duration);

        Type getType() const { return m_type; }
        float value() const { return m_value; }
        double time() const { return m_time; }
        double timeConstant() const { return m_timeConstant; }
        double duration() const { return m_duration; }
        Vector<float>& curve() { return m_curve; }
        float initialValue() const { return m_initialValue; }
        double callTime() const { return m_callTime; }

    private:
        ParamEvent(Type type, float value, double time,
            double timeConstant, double duration, const DOMFloat32Array* curve,
            float initialValue = 0, double callTime = 0);

        Type m_type;
        float m_value;
        double m_time;
        // Only used for SetTarget events
        double m_timeConstant;
        // Only used for SetValueCurve events.
        double m_duration;
        Vector<float> m_curve;
        // Initial value and time to use for linear and exponential ramps that don't have a
        // preceding event.
        float m_initialValue;
        double m_callTime;
    };

    void insertEvent(const ParamEvent&, ExceptionState&);
    float valuesForFrameRangeImpl(size_t startFrame, size_t endFrame, float defaultValue, float* values, unsigned numberOfValues, double sampleRate, double controlRate);

    // Produce a nice string describing the event in human-readable form.
    String eventToString(const ParamEvent&);
    Vector<ParamEvent> m_events;

    mutable Mutex m_eventsLock;

    // Smoothing (de-zippering)
    float m_smoothedValue;
};

} // namespace blink

#endif // AudioParamTimeline_h
