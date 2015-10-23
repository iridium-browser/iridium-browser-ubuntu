// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/audio_recorder_impl.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "components/audio_modem/public/audio_modem_types.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"

namespace audio_modem {

namespace {

const float kProcessIntervalMs = 500.0f;  // milliseconds.

void AudioBusToString(scoped_ptr<media::AudioBus> source, std::string* buffer) {
  buffer->resize(source->frames() * source->channels() * sizeof(float));
  float* buffer_view = reinterpret_cast<float*>(string_as_array(buffer));

  const int channels = source->channels();
  for (int ch = 0; ch < channels; ++ch) {
    for (int si = 0, di = ch; si < source->frames(); ++si, di += channels)
      buffer_view[di] = source->channel(ch)[si];
  }
}

// Called every kProcessIntervalMs to process the recorded audio. This
// converts our samples to the required sample rate, interleaves the samples
// and sends them to the whispernet decoder to process.
void ProcessSamples(
    scoped_ptr<media::AudioBus> bus,
    const AudioRecorderImpl::RecordedSamplesCallback& callback) {
  std::string samples;
  AudioBusToString(bus.Pass(), &samples);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, base::Bind(callback, samples));
}

}  // namespace

// Public methods.

AudioRecorderImpl::AudioRecorderImpl()
    : is_recording_(false),
      stream_(nullptr),
      total_buffer_frames_(0),
      buffer_frame_index_(0) {
}

void AudioRecorderImpl::Initialize(
    const RecordedSamplesCallback& decode_callback) {
  decode_callback_ = decode_callback;
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioRecorderImpl::InitializeOnAudioThread,
                 base::Unretained(this)));
}

AudioRecorderImpl::~AudioRecorderImpl() {
}

void AudioRecorderImpl::Record() {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioRecorderImpl::RecordOnAudioThread,
                 base::Unretained(this)));
}

void AudioRecorderImpl::Stop() {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioRecorderImpl::StopOnAudioThread,
                 base::Unretained(this)));
}

void AudioRecorderImpl::Finalize() {
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioRecorderImpl::FinalizeOnAudioThread,
                 base::Unretained(this)));
}

// Private methods.

void AudioRecorderImpl::InitializeOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());

  media::AudioParameters params;
  if (params_for_testing_) {
    params = *params_for_testing_;
  } else {
    params = media::AudioManager::Get()->GetInputStreamParameters(
        media::AudioManagerBase::kDefaultDeviceId);
    params = media::AudioParameters(params.format(),
                                    params.channel_layout(),
                                    params.sample_rate(),
                                    params.bits_per_sample(),
                                    params.frames_per_buffer(),
                                    media::AudioParameters::NO_EFFECTS);
  }

  total_buffer_frames_ = kProcessIntervalMs * params.sample_rate() / 1000;
  buffer_ = media::AudioBus::Create(params.channels(), total_buffer_frames_);
  buffer_frame_index_ = 0;

  stream_ = input_stream_for_testing_
                ? input_stream_for_testing_.get()
                : media::AudioManager::Get()->MakeAudioInputStream(
                      params, media::AudioManagerBase::kDefaultDeviceId);

  if (!stream_ || !stream_->Open()) {
    LOG(ERROR) << "Failed to open an input stream.";
    if (stream_) {
      stream_->Close();
      stream_ = nullptr;
    }
    return;
  }
  stream_->SetVolume(stream_->GetMaxVolume());
}

void AudioRecorderImpl::RecordOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  if (!stream_ || is_recording_)
    return;

  VLOG(3) << "Starting recording.";
  stream_->Start(this);
  is_recording_ = true;
}

void AudioRecorderImpl::StopOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  if (!stream_ || !is_recording_)
    return;

  VLOG(3) << "Stopping recording.";
  stream_->Stop();
  is_recording_ = false;
}

void AudioRecorderImpl::StopAndCloseOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  if (!stream_)
    return;

  StopOnAudioThread();
  stream_->Close();
  stream_ = nullptr;
}

void AudioRecorderImpl::FinalizeOnAudioThread() {
  DCHECK(media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
  StopAndCloseOnAudioThread();
  delete this;
}

void AudioRecorderImpl::OnData(media::AudioInputStream* stream,
                               const media::AudioBus* source,
                               uint32 /* hardware_delay_bytes */,
                               double /* volume */) {
  // source->frames() == source_params.frames_per_buffer(), so we only have
  // one chunk of data in the source; correspondingly set the destination
  // size to one chunk.

  int remaining_buffer_frames = buffer_->frames() - buffer_frame_index_;
  int frames_to_copy = std::min(remaining_buffer_frames, source->frames());
  source->CopyPartialFramesTo(0, frames_to_copy, buffer_frame_index_,
                              buffer_.get());
  buffer_frame_index_ += frames_to_copy;

  // Buffer full, send it for processing.
  if (buffer_->frames() == buffer_frame_index_) {
    ProcessSamples(buffer_.Pass(), decode_callback_);
    buffer_ = media::AudioBus::Create(source->channels(), total_buffer_frames_);
    buffer_frame_index_ = 0;

    // Copy any remaining frames in the source to our buffer.
    int remaining_source_frames = source->frames() - frames_to_copy;
    source->CopyPartialFramesTo(frames_to_copy, remaining_source_frames,
                                buffer_frame_index_, buffer_.get());
    buffer_frame_index_ += remaining_source_frames;
  }
}

void AudioRecorderImpl::OnError(media::AudioInputStream* /* stream */) {
  LOG(ERROR) << "Error during sound recording.";
  media::AudioManager::Get()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&AudioRecorderImpl::StopAndCloseOnAudioThread,
                 base::Unretained(this)));
}

void AudioRecorderImpl::FlushAudioLoopForTesting() {
  if (media::AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread())
    return;

  // Queue task on the audio thread, when it is executed, that means we've
  // successfully executed all the tasks before us.
  base::RunLoop rl;
  media::AudioManager::Get()->GetTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&AudioRecorderImpl::FlushAudioLoopForTesting),
          base::Unretained(this)),
      rl.QuitClosure());
  rl.Run();
}

}  // namespace audio_modem
