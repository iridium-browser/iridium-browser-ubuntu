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
  AudioParamTimeline() {}

  void setValueAtTime(float value, double time, ExceptionState&);
  void linearRampToValueAtTime(float value,
                               double time,
                               float initialValue,
                               double callTime,
                               ExceptionState&);
  void exponentialRampToValueAtTime(float value,
                                    double time,
                                    float initialValue,
                                    double callTime,
                                    ExceptionState&);
  void setTargetAtTime(float target,
                       double time,
                       double timeConstant,
                       ExceptionState&);
  void setValueCurveAtTime(DOMFloat32Array* curve,
                           double time,
                           double duration,
                           ExceptionState&);
  void cancelScheduledValues(double startTime, ExceptionState&);
  void cancelAndHoldAtTime(double cancelTime, ExceptionState&);

  // hasValue is set to true if a valid timeline value is returned.
  // otherwise defaultValue is returned.
  float valueForContextTime(AudioDestinationHandler&,
                            float defaultValue,
                            bool& hasValue,
                            float minValue,
                            float maxValue);

  // Given the time range in frames, calculates parameter values into the values
  // buffer and returns the last parameter value calculated for "values" or the
  // defaultValue if none were calculated.  controlRate is the rate (number per
  // second) at which parameter values will be calculated.  It should equal
  // sampleRate for sample-accurate parameter changes, and otherwise will
  // usually match the render quantum size such that the parameter value changes
  // once per render quantum.
  float valuesForFrameRange(size_t startFrame,
                            size_t endFrame,
                            float defaultValue,
                            float* values,
                            unsigned numberOfValues,
                            double sampleRate,
                            double controlRate,
                            float minValue,
                            float maxValue);

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
      // For cancelValuesAndHold
      CancelValues,
      LastType
    };

    static std::unique_ptr<ParamEvent> createLinearRampEvent(float value,
                                                             double time,
                                                             float initialValue,
                                                             double callTime);
    static std::unique_ptr<ParamEvent> createExponentialRampEvent(
        float value,
        double time,
        float initialValue,
        double callTime);
    static std::unique_ptr<ParamEvent> createSetValueEvent(float value,
                                                           double time);
    static std::unique_ptr<ParamEvent>
    createSetTargetEvent(float value, double time, double timeConstant);
    static std::unique_ptr<ParamEvent> createSetValueCurveEvent(
        const DOMFloat32Array* curve,
        double time,
        double duration);
    static std::unique_ptr<ParamEvent> createCancelValuesEvent(
        double time,
        std::unique_ptr<ParamEvent> savedEvent);
    // Needed for creating a saved event where we want to supply all
    // the possible parameters because we're mostly copying an
    // existing event.
    static std::unique_ptr<ParamEvent> createGeneralEvent(
        Type,
        float value,
        double time,
        float initialValue,
        double callTime,
        double timeConstant,
        double duration,
        Vector<float>& curve,
        double curvePointsPerSecond,
        float curveEndValue,
        std::unique_ptr<ParamEvent> savedEvent);

    static bool eventPreceeds(const std::unique_ptr<ParamEvent>& a,
                              const std::unique_ptr<ParamEvent>& b) {
      return a->time() < b->time();
    }

    Type getType() const { return m_type; }
    float value() const { return m_value; }
    double time() const { return m_time; }
    void setTime(double newTime) { m_time = newTime; }
    double timeConstant() const { return m_timeConstant; }
    double duration() const { return m_duration; }
    Vector<float>& curve() { return m_curve; }
    float initialValue() const { return m_initialValue; }
    double callTime() const { return m_callTime; }
    bool needsTimeClampCheck() const { return m_needsTimeClampCheck; }
    void clearTimeClampCheck() { m_needsTimeClampCheck = false; }

    double curvePointsPerSecond() const { return m_curvePointsPerSecond; }
    float curveEndValue() const { return m_curveEndValue; }

    // For CancelValues events. Not valid for any other event.
    ParamEvent* savedEvent() const;
    bool hasDefaultCancelledValue() const;
    void setCancelledValue(float);

   private:
    // General event
    ParamEvent(Type type,
               float value,
               double time,
               float initialValue,
               double callTime,
               double timeConstant,
               double duration,
               Vector<float>& curve,
               double curvePointsPerSecond,
               float curveEndValue,
               std::unique_ptr<ParamEvent> savedEvent);

    // Create simplest event needing just a value and time, like
    // setValueAtTime.
    ParamEvent(Type, float value, double time);

    // Create a linear or exponential ramp that requires an initial
    // value and time in case there is no actual event that preceeds
    // this event.
    ParamEvent(Type,
               float value,
               double time,
               float initialValue,
               double callTime);

    // Create an event needing a time constant (setTargetAtTime)
    ParamEvent(Type, float value, double time, double timeConstant);

    // Create a setValueCurve event
    ParamEvent(Type,
               double time,
               double duration,
               const DOMFloat32Array* curve,
               double curvePointsPerSecond,
               float curveEndValue);

    // Create CancelValues event
    ParamEvent(Type, double time, std::unique_ptr<ParamEvent> savedEvent);

    Type m_type;

    // The value for the event.  The interpretation of this depends on
    // the event type. Not used for SetValueCurve. For CancelValues,
    // it is the end value to use when cancelling a LinearRampToValue
    // or ExponentialRampToValue event.
    float m_value;

    // The time for the event. The interpretation of this depends on
    // the event type.
    double m_time;

    // Initial value and time to use for linear and exponential ramps that don't
    // have a preceding event.
    float m_initialValue;
    double m_callTime;

    // Only used for SetTarget events
    double m_timeConstant;

    // The following items are only used for SetValueCurve events.
    //
    // The duration of the curve.
    double m_duration;
    // The array of curve points.
    Vector<float> m_curve;
    // The number of curve points per second. it is used to compute
    // the curve index step when running the automation.
    double m_curvePointsPerSecond;
    // The default value to use at the end of the curve.  Normally
    // it's the last entry in m_curve, but cancelling a SetValueCurve
    // will set this to a new value.
    float m_curveEndValue;

    // For CancelValues. If CancelValues is in the middle of an event, this
    // holds the event that is being cancelled, so that processing can
    // continue as if the event still existed up until we reach the actual
    // scheduled cancel time.
    std::unique_ptr<ParamEvent> m_savedEvent;

    // True if the start time needs to be checked against current time
    // to implement clamping.
    bool m_needsTimeClampCheck;

    // True if a default value has been assigned to the CancelValues event.
    bool m_hasDefaultCancelledValue;
  };

  void insertEvent(std::unique_ptr<ParamEvent>, ExceptionState&);
  float valuesForFrameRangeImpl(size_t startFrame,
                                size_t endFrame,
                                float defaultValue,
                                float* values,
                                unsigned numberOfValues,
                                double sampleRate,
                                double controlRate);

  // Produce a nice string describing the event in human-readable form.
  String eventToString(const ParamEvent&);

  // Automation functions that compute the vlaue of the specified
  // automation at the specified time.
  float linearRampAtTime(double t,
                         float value1,
                         double time1,
                         float value2,
                         double time2);
  float exponentialRampAtTime(double t,
                              float value1,
                              double time1,
                              float value2,
                              double time2);
  float targetValueAtTime(double t,
                          float value1,
                          double time1,
                          float value2,
                          float timeConstant);
  float valueCurveAtTime(double t,
                         double time1,
                         double duration,
                         const float* curveData,
                         unsigned curveLength);

  // Vector of all automation events for the AudioParam.  Access must
  // be locked via m_eventsLock.
  Vector<std::unique_ptr<ParamEvent>> m_events;

  mutable Mutex m_eventsLock;

  // Smoothing (de-zippering)
  float m_smoothedValue;
};

}  // namespace blink

#endif  // AudioParamTimeline_h
