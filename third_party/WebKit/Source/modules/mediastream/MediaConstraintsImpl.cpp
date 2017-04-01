/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "modules/mediastream/MediaConstraintsImpl.h"

#include "bindings/core/v8/ArrayValue.h"
#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/mediastream/MediaTrackConstraints.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"

namespace blink {

namespace MediaConstraintsImpl {

// A naked value is treated as an "ideal" value in the basic constraints,
// but as an exact value in "advanced" constraints.
// https://w3c.github.io/mediacapture-main/#constrainable-interface
enum class NakedValueDisposition { kTreatAsIdeal, kTreatAsExact };

// Old type/value form of constraint. Used in parsing old-style constraints.
struct NameValueStringConstraint {
  NameValueStringConstraint() {}

  NameValueStringConstraint(WebString name, WebString value)
      : m_name(name), m_value(value) {}

  WebString m_name;
  WebString m_value;
};

// Legal constraint names.
// Temporary Note: Comments about source are where they are copied from.
// Once the chrome parts use the new-style constraint values, they will
// be deleted from the files mentioned.
// TODO(hta): remove comments before https://crbug.com/543997 is closed.

// From content/renderer/media/media_stream_video_source.cc
const char kMinAspectRatio[] = "minAspectRatio";
const char kMaxAspectRatio[] = "maxAspectRatio";
const char kMaxWidth[] = "maxWidth";
const char kMinWidth[] = "minWidth";
const char kMaxHeight[] = "maxHeight";
const char kMinHeight[] = "minHeight";
const char kMaxFrameRate[] = "maxFrameRate";
const char kMinFrameRate[] = "minFrameRate";
// From content/common/media/media_stream_options.cc
const char kMediaStreamSource[] = "chromeMediaSource";
const char kMediaStreamSourceId[] =
    "chromeMediaSourceId";                           // mapped to deviceId
const char kMediaStreamSourceInfoId[] = "sourceId";  // mapped to deviceId
const char kMediaStreamRenderToAssociatedSink[] =
    "chromeRenderToAssociatedSink";
// RenderToAssociatedSink will be going away in M50-M60 some time.
const char kMediaStreamAudioHotword[] = "googHotword";
// TODO(hta): googHotword should go away. https://crbug.com/577627
// From content/renderer/media/media_stream_audio_processor_options.cc
const char kEchoCancellation[] = "echoCancellation";
const char kDisableLocalEcho[] = "disableLocalEcho";
const char kGoogEchoCancellation[] = "googEchoCancellation";
const char kGoogExperimentalEchoCancellation[] = "googEchoCancellation2";
const char kGoogAutoGainControl[] = "googAutoGainControl";
const char kGoogExperimentalAutoGainControl[] = "googAutoGainControl2";
const char kGoogNoiseSuppression[] = "googNoiseSuppression";
const char kGoogExperimentalNoiseSuppression[] = "googNoiseSuppression2";
const char kGoogBeamforming[] = "googBeamforming";
const char kGoogArrayGeometry[] = "googArrayGeometry";
const char kGoogHighpassFilter[] = "googHighpassFilter";
const char kGoogTypingNoiseDetection[] = "googTypingNoiseDetection";
const char kGoogAudioMirroring[] = "googAudioMirroring";

// From
// third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.cc

// Audio constraints.
const char kDAEchoCancellation[] = "googDAEchoCancellation";

// Google-specific constraint keys for a local video source (getUserMedia).
const char kNoiseReduction[] = "googNoiseReduction";

// Constraint keys for CreateOffer / CreateAnswer defined in W3C specification.
const char kOfferToReceiveAudio[] = "OfferToReceiveAudio";
const char kOfferToReceiveVideo[] = "OfferToReceiveVideo";
const char kVoiceActivityDetection[] = "VoiceActivityDetection";
const char kIceRestart[] = "IceRestart";
// Google specific constraint for BUNDLE enable/disable.
const char kUseRtpMux[] = "googUseRtpMUX";
// Below constraints should be used during PeerConnection construction.
const char kEnableDtlsSrtp[] = "DtlsSrtpKeyAgreement";
const char kEnableRtpDataChannels[] = "RtpDataChannels";
// Google-specific constraint keys.
// TODO(hta): These need to be made standard or deleted. crbug.com/605673
const char kEnableDscp[] = "googDscp";
const char kEnableIPv6[] = "googIPv6";
const char kEnableVideoSuspendBelowMinBitrate[] = "googSuspendBelowMinBitrate";
const char kNumUnsignalledRecvStreams[] = "googNumUnsignalledRecvStreams";
const char kCombinedAudioVideoBwe[] = "googCombinedAudioVideoBwe";
const char kScreencastMinBitrate[] = "googScreencastMinBitrate";
const char kCpuOveruseDetection[] = "googCpuOveruseDetection";
const char kCpuUnderuseThreshold[] = "googCpuUnderuseThreshold";
const char kCpuOveruseThreshold[] = "googCpuOveruseThreshold";
const char kCpuUnderuseEncodeRsdThreshold[] =
    "googCpuUnderuseEncodeRsdThreshold";
const char kCpuOveruseEncodeRsdThreshold[] = "googCpuOveruseEncodeRsdThreshold";
const char kCpuOveruseEncodeUsage[] = "googCpuOveruseEncodeUsage";
const char kHighStartBitrate[] = "googHighStartBitrate";
const char kPayloadPadding[] = "googPayloadPadding";
// From webrtc_audio_capturer
const char kAudioLatency[] = "latencyMs";
// From media_stream_video_capturer_source

// End of names from libjingle
// Names that have been used in the past, but should now be ignored.
// Kept around for backwards compatibility.
// https://crbug.com/579729
const char kGoogLeakyBucket[] = "googLeakyBucket";
const char kPowerLineFrequency[] = "googPowerLineFrequency";
// Names used for testing.
const char kTestConstraint1[] = "valid_and_supported_1";
const char kTestConstraint2[] = "valid_and_supported_2";

static bool parseMandatoryConstraintsDictionary(
    const Dictionary& mandatoryConstraintsDictionary,
    Vector<NameValueStringConstraint>& mandatory) {
  DummyExceptionStateForTesting exceptionState;
  const HashMap<String, String>& mandatoryConstraintsHashMap =
      mandatoryConstraintsDictionary.getOwnPropertiesAsStringHashMap(
          exceptionState);
  if (exceptionState.hadException())
    return false;

  for (const auto& iter : mandatoryConstraintsHashMap)
    mandatory.push_back(NameValueStringConstraint(iter.key, iter.value));
  return true;
}

static bool parseOptionalConstraintsVectorElement(
    const Dictionary& constraint,
    Vector<NameValueStringConstraint>& optionalConstraintsVector) {
  DummyExceptionStateForTesting exceptionState;
  const Vector<String>& localNames =
      constraint.getPropertyNames(exceptionState);
  if (exceptionState.hadException())
    return false;
  if (localNames.size() != 1)
    return false;
  const String& key = localNames[0];
  String value;
  bool ok = DictionaryHelper::get(constraint, key, value);
  if (!ok)
    return false;
  optionalConstraintsVector.push_back(NameValueStringConstraint(key, value));
  return true;
}

// Old style parser. Deprecated.
static bool parse(const Dictionary& constraintsDictionary,
                  Vector<NameValueStringConstraint>& optional,
                  Vector<NameValueStringConstraint>& mandatory) {
  if (constraintsDictionary.isUndefinedOrNull())
    return true;

  DummyExceptionStateForTesting exceptionState;
  const Vector<String>& names =
      constraintsDictionary.getPropertyNames(exceptionState);
  if (exceptionState.hadException())
    return false;

  String mandatoryName("mandatory");
  String optionalName("optional");

  for (const auto& name : names) {
    if (name != mandatoryName && name != optionalName)
      return false;
  }

  if (names.contains(mandatoryName)) {
    Dictionary mandatoryConstraintsDictionary;
    bool ok = constraintsDictionary.get(mandatoryName,
                                        mandatoryConstraintsDictionary);
    if (!ok || mandatoryConstraintsDictionary.isUndefinedOrNull())
      return false;
    ok = parseMandatoryConstraintsDictionary(mandatoryConstraintsDictionary,
                                             mandatory);
    if (!ok)
      return false;
  }

  if (names.contains(optionalName)) {
    ArrayValue optionalConstraints;
    bool ok = DictionaryHelper::get(constraintsDictionary, optionalName,
                                    optionalConstraints);
    if (!ok || optionalConstraints.isUndefinedOrNull())
      return false;

    size_t numberOfConstraints;
    ok = optionalConstraints.length(numberOfConstraints);
    if (!ok)
      return false;

    for (size_t i = 0; i < numberOfConstraints; ++i) {
      Dictionary constraint;
      ok = optionalConstraints.get(i, constraint);
      if (!ok || constraint.isUndefinedOrNull())
        return false;
      ok = parseOptionalConstraintsVectorElement(constraint, optional);
      if (!ok)
        return false;
    }
  }

  return true;
}

static bool parse(const MediaTrackConstraints& constraintsIn,
                  Vector<NameValueStringConstraint>& optional,
                  Vector<NameValueStringConstraint>& mandatory) {
  Vector<NameValueStringConstraint> mandatoryConstraintsVector;
  if (constraintsIn.hasMandatory()) {
    bool ok = parseMandatoryConstraintsDictionary(constraintsIn.mandatory(),
                                                  mandatory);
    if (!ok)
      return false;
  }

  if (constraintsIn.hasOptional()) {
    const Vector<Dictionary>& optionalConstraints = constraintsIn.optional();

    for (const auto& constraint : optionalConstraints) {
      if (constraint.isUndefinedOrNull())
        return false;
      bool ok = parseOptionalConstraintsVectorElement(constraint, optional);
      if (!ok)
        return false;
    }
  }
  return true;
}

static bool toBoolean(const WebString& asWebString) {
  return asWebString.equals("true");
  // TODO(hta): Check against "false" and return error if it's neither.
  // https://crbug.com/576582
}

static void parseOldStyleNames(
    ExecutionContext* context,
    const Vector<NameValueStringConstraint>& oldNames,
    bool reportUnknownNames,
    WebMediaTrackConstraintSet& result,
    MediaErrorState& errorState) {
  for (const NameValueStringConstraint& constraint : oldNames) {
    if (constraint.m_name.equals(kMinAspectRatio)) {
      result.aspectRatio.setMin(atof(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kMaxAspectRatio)) {
      result.aspectRatio.setMax(atof(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kMaxWidth)) {
      result.width.setMax(atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kMinWidth)) {
      result.width.setMin(atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kMaxHeight)) {
      result.height.setMax(atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kMinHeight)) {
      result.height.setMin(atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kMinFrameRate)) {
      result.frameRate.setMin(atof(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kMaxFrameRate)) {
      result.frameRate.setMax(atof(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kEchoCancellation)) {
      result.echoCancellation.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kMediaStreamSource)) {
      // TODO(hta): This has only a few legal values. Should be
      // represented as an enum, and cause type errors.
      // https://crbug.com/576582
      result.mediaStreamSource.setExact(constraint.m_value);
    } else if (constraint.m_name.equals(kDisableLocalEcho) &&
               RuntimeEnabledFeatures::
                   desktopCaptureDisableLocalEchoControlEnabled()) {
      result.disableLocalEcho.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kMediaStreamSourceId) ||
               constraint.m_name.equals(kMediaStreamSourceInfoId)) {
      result.deviceId.setExact(constraint.m_value);
    } else if (constraint.m_name.equals(kMediaStreamRenderToAssociatedSink)) {
      // TODO(hta): This is a boolean represented as string.
      // Should give TypeError when it's not parseable.
      // https://crbug.com/576582
      result.renderToAssociatedSink.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kMediaStreamAudioHotword)) {
      result.hotwordEnabled.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogEchoCancellation)) {
      result.googEchoCancellation.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogExperimentalEchoCancellation)) {
      result.googExperimentalEchoCancellation.setExact(
          toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogAutoGainControl)) {
      result.googAutoGainControl.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogExperimentalAutoGainControl)) {
      result.googExperimentalAutoGainControl.setExact(
          toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogNoiseSuppression)) {
      result.googNoiseSuppression.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogExperimentalNoiseSuppression)) {
      result.googExperimentalNoiseSuppression.setExact(
          toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogBeamforming)) {
      result.googBeamforming.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogArrayGeometry)) {
      result.googArrayGeometry.setExact(constraint.m_value);
    } else if (constraint.m_name.equals(kGoogHighpassFilter)) {
      result.googHighpassFilter.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogTypingNoiseDetection)) {
      result.googTypingNoiseDetection.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kGoogAudioMirroring)) {
      result.googAudioMirroring.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kDAEchoCancellation)) {
      result.googDAEchoCancellation.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kNoiseReduction)) {
      result.googNoiseReduction.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kOfferToReceiveAudio)) {
      // This constraint has formerly been defined both as a boolean
      // and as an integer. Allow both forms.
      if (constraint.m_value.equals("true"))
        result.offerToReceiveAudio.setExact(1);
      else if (constraint.m_value.equals("false"))
        result.offerToReceiveAudio.setExact(0);
      else
        result.offerToReceiveAudio.setExact(
            atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kOfferToReceiveVideo)) {
      // This constraint has formerly been defined both as a boolean
      // and as an integer. Allow both forms.
      if (constraint.m_value.equals("true"))
        result.offerToReceiveVideo.setExact(1);
      else if (constraint.m_value.equals("false"))
        result.offerToReceiveVideo.setExact(0);
      else
        result.offerToReceiveVideo.setExact(
            atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kVoiceActivityDetection)) {
      result.voiceActivityDetection.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kIceRestart)) {
      result.iceRestart.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kUseRtpMux)) {
      result.googUseRtpMux.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kEnableDtlsSrtp)) {
      result.enableDtlsSrtp.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kEnableRtpDataChannels)) {
      result.enableRtpDataChannels.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kEnableDscp)) {
      result.enableDscp.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kEnableIPv6)) {
      result.enableIPv6.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kEnableVideoSuspendBelowMinBitrate)) {
      result.googEnableVideoSuspendBelowMinBitrate.setExact(
          toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kNumUnsignalledRecvStreams)) {
      result.googNumUnsignalledRecvStreams.setExact(
          atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kCombinedAudioVideoBwe)) {
      result.googCombinedAudioVideoBwe.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kScreencastMinBitrate)) {
      result.googScreencastMinBitrate.setExact(
          atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kCpuOveruseDetection)) {
      result.googCpuOveruseDetection.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kCpuUnderuseThreshold)) {
      result.googCpuUnderuseThreshold.setExact(
          atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kCpuOveruseThreshold)) {
      result.googCpuOveruseThreshold.setExact(
          atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kCpuUnderuseEncodeRsdThreshold)) {
      result.googCpuUnderuseEncodeRsdThreshold.setExact(
          atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kCpuOveruseEncodeRsdThreshold)) {
      result.googCpuOveruseEncodeRsdThreshold.setExact(
          atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kCpuOveruseEncodeUsage)) {
      result.googCpuOveruseEncodeUsage.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kHighStartBitrate)) {
      result.googHighStartBitrate.setExact(
          atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kPayloadPadding)) {
      result.googPayloadPadding.setExact(toBoolean(constraint.m_value));
    } else if (constraint.m_name.equals(kAudioLatency)) {
      result.googLatencyMs.setExact(atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kPowerLineFrequency)) {
      result.googPowerLineFrequency.setExact(
          atoi(constraint.m_value.utf8().c_str()));
    } else if (constraint.m_name.equals(kGoogLeakyBucket)) {
      context->addConsoleMessage(ConsoleMessage::create(
          DeprecationMessageSource, WarningMessageLevel,
          "Obsolete constraint named " + String(constraint.m_name) +
              " is ignored. Please stop using it."));
    } else if (constraint.m_name.equals(kTestConstraint1) ||
               constraint.m_name.equals(kTestConstraint2)) {
      // These constraints are only for testing parsing.
      // Values 0 and 1 are legal, all others are a ConstraintError.
      if (!constraint.m_value.equals("0") && !constraint.m_value.equals("1")) {
        errorState.throwConstraintError("Illegal value for constraint",
                                        constraint.m_name);
      }
    } else {
      if (reportUnknownNames) {
        // TODO(hta): UMA stats for unknown constraints passed.
        // https://crbug.com/576613
        context->addConsoleMessage(ConsoleMessage::create(
            DeprecationMessageSource, WarningMessageLevel,
            "Unknown constraint named " + String(constraint.m_name) +
                " rejected"));
        errorState.throwConstraintError("Unknown name of constraint detected",
                                        constraint.m_name);
      }
    }
  }
}

static WebMediaConstraints createFromNamedConstraints(
    ExecutionContext* context,
    Vector<NameValueStringConstraint>& mandatory,
    const Vector<NameValueStringConstraint>& optional,
    MediaErrorState& errorState) {
  WebMediaTrackConstraintSet basic;
  WebMediaTrackConstraintSet advanced;
  WebMediaConstraints constraints;
  parseOldStyleNames(context, mandatory, true, basic, errorState);
  if (errorState.hadException())
    return constraints;
  // We ignore unknow names and syntax errors in optional constraints.
  MediaErrorState ignoredErrorState;
  Vector<WebMediaTrackConstraintSet> advancedVector;
  for (const auto& optionalConstraint : optional) {
    WebMediaTrackConstraintSet advancedElement;
    Vector<NameValueStringConstraint> elementAsList(1, optionalConstraint);
    parseOldStyleNames(context, elementAsList, false, advancedElement,
                       ignoredErrorState);
    if (!advancedElement.isEmpty())
      advancedVector.push_back(advancedElement);
  }
  constraints.initialize(basic, advancedVector);
  return constraints;
}

// Deprecated.
WebMediaConstraints create(ExecutionContext* context,
                           const Dictionary& constraintsDictionary,
                           MediaErrorState& errorState) {
  Vector<NameValueStringConstraint> optional;
  Vector<NameValueStringConstraint> mandatory;
  if (!parse(constraintsDictionary, optional, mandatory)) {
    errorState.throwTypeError("Malformed constraints object.");
    return WebMediaConstraints();
  }
  UseCounter::count(context, UseCounter::MediaStreamConstraintsFromDictionary);
  return createFromNamedConstraints(context, mandatory, optional, errorState);
}

void copyLongConstraint(const LongOrConstrainLongRange& blinkUnionForm,
                        NakedValueDisposition nakedTreatment,
                        LongConstraint& webForm) {
  if (blinkUnionForm.isLong()) {
    switch (nakedTreatment) {
      case NakedValueDisposition::kTreatAsIdeal:
        webForm.setIdeal(blinkUnionForm.getAsLong());
        break;
      case NakedValueDisposition::kTreatAsExact:
        webForm.setExact(blinkUnionForm.getAsLong());
        break;
    }
    return;
  }
  const auto& blinkForm = blinkUnionForm.getAsConstrainLongRange();
  if (blinkForm.hasMin()) {
    webForm.setMin(blinkForm.min());
  }
  if (blinkForm.hasMax()) {
    webForm.setMax(blinkForm.max());
  }
  if (blinkForm.hasIdeal()) {
    webForm.setIdeal(blinkForm.ideal());
  }
  if (blinkForm.hasExact()) {
    webForm.setExact(blinkForm.exact());
  }
}

void copyDoubleConstraint(const DoubleOrConstrainDoubleRange& blinkUnionForm,
                          NakedValueDisposition nakedTreatment,
                          DoubleConstraint& webForm) {
  if (blinkUnionForm.isDouble()) {
    switch (nakedTreatment) {
      case NakedValueDisposition::kTreatAsIdeal:
        webForm.setIdeal(blinkUnionForm.getAsDouble());
        break;
      case NakedValueDisposition::kTreatAsExact:
        webForm.setExact(blinkUnionForm.getAsDouble());
        break;
    }
    return;
  }
  const auto& blinkForm = blinkUnionForm.getAsConstrainDoubleRange();
  if (blinkForm.hasMin()) {
    webForm.setMin(blinkForm.min());
  }
  if (blinkForm.hasMax()) {
    webForm.setMax(blinkForm.max());
  }
  if (blinkForm.hasIdeal()) {
    webForm.setIdeal(blinkForm.ideal());
  }
  if (blinkForm.hasExact()) {
    webForm.setExact(blinkForm.exact());
  }
}

void copyStringConstraint(
    const StringOrStringSequenceOrConstrainDOMStringParameters& blinkUnionForm,
    NakedValueDisposition nakedTreatment,
    StringConstraint& webForm) {
  if (blinkUnionForm.isString()) {
    switch (nakedTreatment) {
      case NakedValueDisposition::kTreatAsIdeal:
        webForm.setIdeal(Vector<String>(1, blinkUnionForm.getAsString()));
        break;
      case NakedValueDisposition::kTreatAsExact:
        webForm.setExact(Vector<String>(1, blinkUnionForm.getAsString()));

        break;
    }
    return;
  }
  if (blinkUnionForm.isStringSequence()) {
    switch (nakedTreatment) {
      case NakedValueDisposition::kTreatAsIdeal:
        webForm.setIdeal(blinkUnionForm.getAsStringSequence());
        break;
      case NakedValueDisposition::kTreatAsExact:
        webForm.setExact(blinkUnionForm.getAsStringSequence());
        break;
    }
    return;
  }
  const auto& blinkForm = blinkUnionForm.getAsConstrainDOMStringParameters();
  if (blinkForm.hasIdeal()) {
    if (blinkForm.ideal().isStringSequence()) {
      webForm.setIdeal(blinkForm.ideal().getAsStringSequence());
    } else if (blinkForm.ideal().isString()) {
      webForm.setIdeal(Vector<String>(1, blinkForm.ideal().getAsString()));
    }
  }
  if (blinkForm.hasExact()) {
    if (blinkForm.exact().isStringSequence()) {
      webForm.setExact(blinkForm.exact().getAsStringSequence());
    } else if (blinkForm.exact().isString()) {
      webForm.setExact(Vector<String>(1, blinkForm.exact().getAsString()));
    }
  }
}

void copyBooleanConstraint(
    const BooleanOrConstrainBooleanParameters& blinkUnionForm,
    NakedValueDisposition nakedTreatment,
    BooleanConstraint& webForm) {
  if (blinkUnionForm.isBoolean()) {
    switch (nakedTreatment) {
      case NakedValueDisposition::kTreatAsIdeal:
        webForm.setIdeal(blinkUnionForm.getAsBoolean());
        break;
      case NakedValueDisposition::kTreatAsExact:
        webForm.setExact(blinkUnionForm.getAsBoolean());
        break;
    }
    return;
  }
  const auto& blinkForm = blinkUnionForm.getAsConstrainBooleanParameters();
  if (blinkForm.hasIdeal()) {
    webForm.setIdeal(blinkForm.ideal());
  }
  if (blinkForm.hasExact()) {
    webForm.setExact(blinkForm.exact());
  }
}

void copyConstraintSet(const MediaTrackConstraintSet& constraintsIn,
                       NakedValueDisposition nakedTreatment,
                       WebMediaTrackConstraintSet& constraintBuffer) {
  if (constraintsIn.hasWidth()) {
    copyLongConstraint(constraintsIn.width(), nakedTreatment,
                       constraintBuffer.width);
  }
  if (constraintsIn.hasHeight()) {
    copyLongConstraint(constraintsIn.height(), nakedTreatment,
                       constraintBuffer.height);
  }
  if (constraintsIn.hasAspectRatio()) {
    copyDoubleConstraint(constraintsIn.aspectRatio(), nakedTreatment,
                         constraintBuffer.aspectRatio);
  }
  if (constraintsIn.hasFrameRate()) {
    copyDoubleConstraint(constraintsIn.frameRate(), nakedTreatment,
                         constraintBuffer.frameRate);
  }
  if (constraintsIn.hasFacingMode()) {
    copyStringConstraint(constraintsIn.facingMode(), nakedTreatment,
                         constraintBuffer.facingMode);
  }
  if (constraintsIn.hasVolume()) {
    copyDoubleConstraint(constraintsIn.volume(), nakedTreatment,
                         constraintBuffer.volume);
  }
  if (constraintsIn.hasSampleRate()) {
    copyLongConstraint(constraintsIn.sampleRate(), nakedTreatment,
                       constraintBuffer.sampleRate);
  }
  if (constraintsIn.hasSampleSize()) {
    copyLongConstraint(constraintsIn.sampleSize(), nakedTreatment,
                       constraintBuffer.sampleSize);
  }
  if (constraintsIn.hasEchoCancellation()) {
    copyBooleanConstraint(constraintsIn.echoCancellation(), nakedTreatment,
                          constraintBuffer.echoCancellation);
  }
  if (constraintsIn.hasLatency()) {
    copyDoubleConstraint(constraintsIn.latency(), nakedTreatment,
                         constraintBuffer.latency);
  }
  if (constraintsIn.hasChannelCount()) {
    copyLongConstraint(constraintsIn.channelCount(), nakedTreatment,
                       constraintBuffer.channelCount);
  }
  if (constraintsIn.hasDeviceId()) {
    copyStringConstraint(constraintsIn.deviceId(), nakedTreatment,
                         constraintBuffer.deviceId);
  }
  if (constraintsIn.hasGroupId()) {
    copyStringConstraint(constraintsIn.groupId(), nakedTreatment,
                         constraintBuffer.groupId);
  }
}

WebMediaConstraints convertConstraintsToWeb(
    const MediaTrackConstraints& constraintsIn) {
  WebMediaConstraints constraints;
  WebMediaTrackConstraintSet constraintBuffer;
  Vector<WebMediaTrackConstraintSet> advancedBuffer;
  copyConstraintSet(constraintsIn, NakedValueDisposition::kTreatAsIdeal,
                    constraintBuffer);
  if (constraintsIn.hasAdvanced()) {
    for (const auto& element : constraintsIn.advanced()) {
      WebMediaTrackConstraintSet advancedElement;
      copyConstraintSet(element, NakedValueDisposition::kTreatAsExact,
                        advancedElement);
      advancedBuffer.push_back(advancedElement);
    }
  }
  constraints.initialize(constraintBuffer, advancedBuffer);
  return constraints;
}

WebMediaConstraints create(ExecutionContext* context,
                           const MediaTrackConstraints& constraintsIn,
                           MediaErrorState& errorState) {
  WebMediaConstraints standardForm = convertConstraintsToWeb(constraintsIn);
  if (constraintsIn.hasOptional() || constraintsIn.hasMandatory()) {
    if (!standardForm.isEmpty()) {
      UseCounter::count(context, UseCounter::MediaStreamConstraintsOldAndNew);
      errorState.throwTypeError(
          "Malformed constraint: Cannot use both optional/mandatory and "
          "specific or advanced constraints.");
      return WebMediaConstraints();
    }
    Vector<NameValueStringConstraint> optional;
    Vector<NameValueStringConstraint> mandatory;
    if (!parse(constraintsIn, optional, mandatory)) {
      errorState.throwTypeError("Malformed constraints object.");
      return WebMediaConstraints();
    }
    UseCounter::count(context, UseCounter::MediaStreamConstraintsNameValue);
    return createFromNamedConstraints(context, mandatory, optional, errorState);
  }
  UseCounter::count(context, UseCounter::MediaStreamConstraintsConformant);
  return standardForm;
}

WebMediaConstraints create() {
  WebMediaConstraints constraints;
  constraints.initialize();
  return constraints;
}

template <class T>
bool useNakedNumeric(T input, NakedValueDisposition which) {
  switch (which) {
    case NakedValueDisposition::kTreatAsIdeal:
      return input.hasIdeal() &&
             !(input.hasExact() || input.hasMin() || input.hasMax());
      break;
    case NakedValueDisposition::kTreatAsExact:
      return input.hasExact() &&
             !(input.hasIdeal() || input.hasMin() || input.hasMax());
      break;
  }
  NOTREACHED();
  return false;
};

template <class T>
bool useNakedNonNumeric(T input, NakedValueDisposition which) {
  switch (which) {
    case NakedValueDisposition::kTreatAsIdeal:
      return input.hasIdeal() && !input.hasExact();
      break;
    case NakedValueDisposition::kTreatAsExact:
      return input.hasExact() && !input.hasIdeal();
      break;
  }
  NOTREACHED();
  return false;
};

template <typename U, class T>
U getNakedValue(T input, NakedValueDisposition which) {
  switch (which) {
    case NakedValueDisposition::kTreatAsIdeal:
      return input.ideal();
      break;
    case NakedValueDisposition::kTreatAsExact:
      return input.exact();
      break;
  }
  NOTREACHED();
  return input.exact();
};

LongOrConstrainLongRange convertLong(const LongConstraint& input,
                                     NakedValueDisposition nakedTreatment) {
  LongOrConstrainLongRange outputUnion;
  if (useNakedNumeric(input, nakedTreatment)) {
    outputUnion.setLong(getNakedValue<long>(input, nakedTreatment));
  } else if (!input.isEmpty()) {
    ConstrainLongRange output;
    if (input.hasExact())
      output.setExact(input.exact());
    if (input.hasMin())
      output.setMin(input.min());
    if (input.hasMax())
      output.setMax(input.max());
    if (input.hasIdeal())
      output.setIdeal(input.ideal());
    outputUnion.setConstrainLongRange(output);
  }
  return outputUnion;
}

DoubleOrConstrainDoubleRange convertDouble(
    const DoubleConstraint& input,
    NakedValueDisposition nakedTreatment) {
  DoubleOrConstrainDoubleRange outputUnion;
  if (useNakedNumeric(input, nakedTreatment)) {
    outputUnion.setDouble(getNakedValue<double>(input, nakedTreatment));
  } else if (!input.isEmpty()) {
    ConstrainDoubleRange output;
    if (input.hasExact())
      output.setExact(input.exact());
    if (input.hasIdeal())
      output.setIdeal(input.ideal());
    if (input.hasMin())
      output.setMin(input.min());
    if (input.hasMax())
      output.setMax(input.max());
    outputUnion.setConstrainDoubleRange(output);
  }
  return outputUnion;
}

StringOrStringSequence convertStringSequence(
    const WebVector<WebString>& input) {
  StringOrStringSequence theStrings;
  if (input.size() > 1) {
    Vector<String> buffer;
    for (const auto& scanner : input)
      buffer.push_back(scanner);
    theStrings.setStringSequence(buffer);
  } else if (input.size() > 0) {
    theStrings.setString(input[0]);
  }
  return theStrings;
}

StringOrStringSequenceOrConstrainDOMStringParameters convertString(
    const StringConstraint& input,
    NakedValueDisposition nakedTreatment) {
  StringOrStringSequenceOrConstrainDOMStringParameters outputUnion;
  if (useNakedNonNumeric(input, nakedTreatment)) {
    WebVector<WebString> inputBuffer(
        getNakedValue<WebVector<WebString>>(input, nakedTreatment));
    if (inputBuffer.size() > 1) {
      Vector<String> buffer;
      for (const auto& scanner : inputBuffer)
        buffer.push_back(scanner);
      outputUnion.setStringSequence(buffer);
    } else if (inputBuffer.size() > 0) {
      outputUnion.setString(inputBuffer[0]);
    }
  } else if (!input.isEmpty()) {
    ConstrainDOMStringParameters output;
    if (input.hasExact())
      output.setExact(convertStringSequence(input.exact()));
    if (input.hasIdeal())
      output.setIdeal(convertStringSequence(input.ideal()));
    outputUnion.setConstrainDOMStringParameters(output);
  }
  return outputUnion;
}

BooleanOrConstrainBooleanParameters convertBoolean(
    const BooleanConstraint& input,
    NakedValueDisposition nakedTreatment) {
  BooleanOrConstrainBooleanParameters outputUnion;
  if (useNakedNonNumeric(input, nakedTreatment)) {
    outputUnion.setBoolean(getNakedValue<bool>(input, nakedTreatment));
  } else if (!input.isEmpty()) {
    ConstrainBooleanParameters output;
    if (input.hasExact())
      output.setExact(input.exact());
    if (input.hasIdeal())
      output.setIdeal(input.ideal());
    outputUnion.setConstrainBooleanParameters(output);
  }
  return outputUnion;
}

void convertConstraintSet(const WebMediaTrackConstraintSet& input,
                          NakedValueDisposition nakedTreatment,
                          MediaTrackConstraintSet& output) {
  if (!input.width.isEmpty())
    output.setWidth(convertLong(input.width, nakedTreatment));
  if (!input.height.isEmpty())
    output.setHeight(convertLong(input.height, nakedTreatment));
  if (!input.aspectRatio.isEmpty())
    output.setAspectRatio(convertDouble(input.aspectRatio, nakedTreatment));
  if (!input.frameRate.isEmpty())
    output.setFrameRate(convertDouble(input.frameRate, nakedTreatment));
  if (!input.facingMode.isEmpty())
    output.setFacingMode(convertString(input.facingMode, nakedTreatment));
  if (!input.volume.isEmpty())
    output.setVolume(convertDouble(input.volume, nakedTreatment));
  if (!input.sampleRate.isEmpty())
    output.setSampleRate(convertLong(input.sampleRate, nakedTreatment));
  if (!input.sampleSize.isEmpty())
    output.setSampleSize(convertLong(input.sampleSize, nakedTreatment));
  if (!input.echoCancellation.isEmpty()) {
    output.setEchoCancellation(
        convertBoolean(input.echoCancellation, nakedTreatment));
  }
  if (!input.latency.isEmpty())
    output.setLatency(convertDouble(input.latency, nakedTreatment));
  if (!input.channelCount.isEmpty())
    output.setChannelCount(convertLong(input.channelCount, nakedTreatment));
  if (!input.deviceId.isEmpty())
    output.setDeviceId(convertString(input.deviceId, nakedTreatment));
  if (!input.groupId.isEmpty())
    output.setGroupId(convertString(input.groupId, nakedTreatment));
  // TODO(hta): Decide the future of the nonstandard constraints.
  // If they go forward, they need to be added here.
  // https://crbug.com/605673
}

void convertConstraints(const WebMediaConstraints& input,
                        MediaTrackConstraints& output) {
  if (input.isNull())
    return;
  convertConstraintSet(input.basic(), NakedValueDisposition::kTreatAsIdeal,
                       output);
  HeapVector<MediaTrackConstraintSet> advancedVector;
  for (const auto& it : input.advanced()) {
    MediaTrackConstraintSet element;
    convertConstraintSet(it, NakedValueDisposition::kTreatAsExact, element);
    advancedVector.push_back(element);
  }
  if (!advancedVector.isEmpty())
    output.setAdvanced(advancedVector);
}

}  // namespace MediaConstraintsImpl
}  // namespace blink
