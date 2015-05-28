// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_AUDIO_PLAYER_IMPL_H_
#define COMPONENTS_AUDIO_MODEM_AUDIO_PLAYER_IMPL_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/audio_modem/audio_player.h"
#include "media/audio/audio_io.h"

namespace media {
class AudioBus;
class AudioBusRefCounted;
}

namespace audio_modem {

// The AudioPlayerImpl class will play a set of samples till it is told to stop.
class AudioPlayerImpl final
    : public AudioPlayer,
      public media::AudioOutputStream::AudioSourceCallback {
 public:
  AudioPlayerImpl();

  // AudioPlayer overrides:
  void Initialize() override;
  void Play(const scoped_refptr<media::AudioBusRefCounted>& samples) override;
  void Stop() override;
  void Finalize() override;

  // Takes ownership of the stream.
  void set_output_stream_for_testing(
      media::AudioOutputStream* output_stream_for_testing) {
    output_stream_for_testing_.reset(output_stream_for_testing);
  }

 private:
  friend class AudioPlayerTest;
  FRIEND_TEST_ALL_PREFIXES(AudioPlayerTest, BasicPlayAndStop);
  FRIEND_TEST_ALL_PREFIXES(AudioPlayerTest, OutOfOrderPlayAndStopMultiple);

  ~AudioPlayerImpl() override;

  // Methods to do our various operations; all of these need to be run on the
  // audio thread.
  void InitializeOnAudioThread();
  void PlayOnAudioThread(
      const scoped_refptr<media::AudioBusRefCounted>& samples);
  void StopOnAudioThread();
  void StopAndCloseOnAudioThread();
  void FinalizeOnAudioThread();

  // AudioOutputStream::AudioSourceCallback overrides:
  // Following methods could be called from *ANY* thread.
  int OnMoreData(media::AudioBus* dest, uint32 total_bytes_delay) override;
  void OnError(media::AudioOutputStream* stream) override;

  // Flushes the audio loop, making sure that any queued operations are
  // performed.
  void FlushAudioLoopForTesting();

  bool is_playing_;

  // Self-deleting object.
  media::AudioOutputStream* stream_;

  scoped_ptr<media::AudioOutputStream> output_stream_for_testing_;

  // All fields below here are protected by this lock.
  base::Lock state_lock_;

  scoped_refptr<media::AudioBusRefCounted> samples_;

  // Index to the frame in the samples that we need to play next.
  int frame_index_;

  DISALLOW_COPY_AND_ASSIGN(AudioPlayerImpl);
};

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_AUDIO_PLAYER_IMPL_H_
