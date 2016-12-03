/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_H_
#define WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_H_

#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/common_audio/resampler/include/push_resampler.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_mixer/new_audio_conference_mixer.h"
#include "webrtc/modules/audio_mixer/audio_mixer_defines.h"
#include "webrtc/modules/utility/include/file_recorder.h"
#include "webrtc/voice_engine/level_indicator.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc {

class AudioProcessing;
class FileWrapper;
class VoEMediaProcess;

namespace voe {
class Statistics;

// Note: this class is in the process of being rewritten and merged
// with AudioConferenceMixer. Expect inheritance chains to be changed,
// member functions removed or renamed.
class AudioMixer : public FileCallback {
 public:
  static int32_t Create(AudioMixer*& mixer, uint32_t instanceId);  // NOLINT

  static void Destroy(AudioMixer*& mixer);  // NOLINT

  int32_t SetEngineInformation(Statistics& engineStatistics);  // NOLINT

  int32_t SetAudioProcessingModule(AudioProcessing* audioProcessingModule);

  // VoEExternalMedia
  int RegisterExternalMediaProcessing(VoEMediaProcess&  // NOLINT
                                      proccess_object);

  int DeRegisterExternalMediaProcessing();

  int32_t DoOperationsOnCombinedSignal(bool feed_data_to_apm);

  int32_t SetMixabilityStatus(MixerAudioSource& audio_source,  // NOLINT
                              bool mixable);

  int32_t SetAnonymousMixabilityStatus(
      MixerAudioSource& audio_source,  // NOLINT
      bool mixable);

  int GetMixedAudio(int sample_rate_hz,
                    size_t num_channels,
                    AudioFrame* audioFrame);

  // VoEVolumeControl
  int GetSpeechOutputLevel(uint32_t& level);  // NOLINT

  int GetSpeechOutputLevelFullRange(uint32_t& level);  // NOLINT

  int SetOutputVolumePan(float left, float right);

  int GetOutputVolumePan(float& left, float& right);  // NOLINT

  // VoEFile
  int StartRecordingPlayout(const char* fileName, const CodecInst* codecInst);

  int StartRecordingPlayout(OutStream* stream, const CodecInst* codecInst);
  int StopRecordingPlayout();

  virtual ~AudioMixer();

  // For file recording
  void PlayNotification(int32_t id, uint32_t durationMs);

  void RecordNotification(int32_t id, uint32_t durationMs);

  void PlayFileEnded(int32_t id);
  void RecordFileEnded(int32_t id);

 private:
  explicit AudioMixer(uint32_t instanceId);

  // uses
  Statistics* _engineStatisticsPtr;
  AudioProcessing* _audioProcessingModulePtr;

  rtc::CriticalSection _callbackCritSect;
  // protect the _outputFileRecorderPtr and _outputFileRecording
  rtc::CriticalSection _fileCritSect;
  NewAudioConferenceMixer& _mixerModule;
  AudioFrame _audioFrame;
  // Converts mixed audio to the audio processing rate.
  PushResampler<int16_t> audioproc_resampler_;
  AudioLevel _audioLevel;  // measures audio level for the combined signal
  int _instanceId;
  VoEMediaProcess* _externalMediaCallbackPtr;
  bool _externalMedia;
  float _panLeft;
  float _panRight;
  int _mixingFrequencyHz;
  std::unique_ptr<FileRecorder> _outputFileRecorderPtr;
  bool _outputFileRecording;
};

}  // namespace voe

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_H_
