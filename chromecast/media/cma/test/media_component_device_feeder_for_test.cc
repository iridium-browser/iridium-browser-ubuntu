// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/test/media_component_device_feeder_for_test.h"

#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/cast_decoder_buffer_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/pipeline/frame_status_cb_impl.h"
#include "chromecast/media/cma/pipeline/media_component_device_client_impl.h"
#include "chromecast/media/cma/test/frame_segmenter_for_test.h"
#include "chromecast/public/media/audio_pipeline_device.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decrypt_context.h"
#include "chromecast/public/media/media_clock_device.h"
#include "chromecast/public/media/video_pipeline_device.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

MediaComponentDeviceFeederForTest::MediaComponentDeviceFeederForTest(
    MediaComponentDevice *device,
    const BufferList& frames)
    : media_component_device_(device),
      rendering_frame_idx_(1),
      clock_frame_idx_(1),
      feeding_completed_(false) {
  frames_ = frames;
}

MediaComponentDeviceFeederForTest::~MediaComponentDeviceFeederForTest() {
}

void MediaComponentDeviceFeederForTest::Initialize(
    const base::Closure& eos_cb) {
  eos_cb_ = eos_cb;

  media_component_device_->SetClient(
      new MediaComponentDeviceClientImpl(base::Bind(
          &MediaComponentDeviceFeederForTest::OnEos, base::Unretained(this))));

  bool success =
      media_component_device_->SetState(MediaComponentDevice::kStateIdle);
  ASSERT_TRUE(success);
  success = media_component_device_->SetStartPts(0);
  ASSERT_TRUE(success);
  success =
      media_component_device_->SetState(MediaComponentDevice::kStatePaused);
  ASSERT_TRUE(success);
}

void MediaComponentDeviceFeederForTest::Feed() {
  // Start rendering if needed.
  if (rendering_frame_idx_ == 0) {
    media_component_device_->SetState(MediaComponentDevice::kStateRunning);
  } else {
    rendering_frame_idx_--;
  }

  // Possibly feed one frame
  DCHECK(!frames_.empty());
  scoped_refptr<DecoderBufferBase> buffer = frames_.front();

  MediaComponentDevice::FrameStatus status = media_component_device_->PushFrame(
      nullptr, // decrypt_context
      new CastDecoderBufferImpl(buffer),
      new FrameStatusCBImpl(
          base::Bind(&MediaComponentDeviceFeederForTest::OnFramePushed,
                     base::Unretained(this))));
  EXPECT_NE(status, MediaComponentDevice::kFrameFailed);
  frames_.pop_front();

  // Feeding is done, just wait for the end of stream callback.
  if (buffer->end_of_stream() || frames_.empty()) {
    if (frames_.empty() && !buffer->end_of_stream()) {
      LOG(WARNING) << "Stream emptied without feeding EOS frame";
    }

    feeding_completed_ = true;
    return;
  }

  if (status == MediaComponentDevice::kFramePending)
    return;

  OnFramePushed(MediaComponentDevice::kFrameSuccess);
}

void MediaComponentDeviceFeederForTest::OnFramePushed(
    MediaComponentDevice::FrameStatus status) {
  EXPECT_NE(status, MediaComponentDevice::kFrameFailed);
  if (feeding_completed_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&MediaComponentDeviceFeederForTest::Feed,
                            base::Unretained(this)));
}

void MediaComponentDeviceFeederForTest::OnEos() {
  bool success = media_component_device_->SetState(
      MediaComponentDevice::kStateIdle);
  ASSERT_TRUE(success);
  success = media_component_device_->SetState(
      MediaComponentDevice::kStateUninitialized);
  ASSERT_TRUE(success);

  if (!eos_cb_.is_null()) {
    eos_cb_.Run();
  }
}

}  // namespace media
}  // namespace chromecast
