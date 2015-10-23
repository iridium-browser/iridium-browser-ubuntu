// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_MEDIA_STREAM_RENDERER_FACTORY_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_MEDIA_STREAM_RENDERER_FACTORY_H_

#include "base/callback_forward.h"
#include "content/public/renderer/media_stream_renderer_factory.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

// TestMediaStreamClient is a mock implementation of MediaStreamClient used when
// running layout tests.
class TestMediaStreamRendererFactory : public MediaStreamRendererFactory {
 public:
  TestMediaStreamRendererFactory();
  ~TestMediaStreamRendererFactory() override;

  // MediaStreamRendererFactory implementation.
  scoped_refptr<VideoFrameProvider> GetVideoFrameProvider(
      const GURL& url,
      const base::Closure& error_cb,
      const VideoFrameProvider::RepaintCB& repaint_cb) override;

  scoped_refptr<MediaStreamAudioRenderer> GetAudioRenderer(
      const GURL& url,
      int render_frame_id) override;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_MEDIA_STREAM_RENDERER_FACTORY_H_
