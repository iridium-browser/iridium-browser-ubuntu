// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_VIDEO_FRAME_PROVIDER_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_VIDEO_FRAME_PROVIDER_H_

#include "base/time/time.h"
#include "content/public/renderer/video_frame_provider.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// A simple implementation of VideoFrameProvider generates raw frames and
// passes them to webmediaplayer.
// Since non-black pixel values are required in the layout test, e.g.,
// media/video-capture-canvas.html, this class should generate frame with
// only non-black pixels.
class TestVideoFrameProvider : public VideoFrameProvider {
 public:
  TestVideoFrameProvider(
      const gfx::Size& size,
      const base::TimeDelta& frame_duration,
      const base::Closure& error_cb,
      const RepaintCB& repaint_cb);

  // VideoFrameProvider implementation.
  void Start() override;
  void Stop() override;
  void Play() override;
  void Pause() override;

 protected:
  ~TestVideoFrameProvider() override;

 private:
  enum State {
    kStarted,
    kPaused,
    kStopped,
  };

  void GenerateFrame();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  gfx::Size size_;
  State state_;

  base::TimeDelta current_time_;
  base::TimeDelta frame_duration_;
  base::Closure error_cb_;
  RepaintCB repaint_cb_;

  DISALLOW_COPY_AND_ASSIGN(TestVideoFrameProvider);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_VIDEO_FRAME_PROVIDER_H_
