// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_VPX_H_
#define REMOTING_CODEC_VIDEO_ENCODER_VPX_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "remoting/codec/scoped_vpx_codec.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_helper.h"

typedef struct vpx_image vpx_image_t;

namespace webrtc {
class DesktopRegion;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

class VideoEncoderVpx : public VideoEncoder {
 public:
  // Create encoder for the specified protocol.
  static scoped_ptr<VideoEncoderVpx> CreateForVP8();
  static scoped_ptr<VideoEncoderVpx> CreateForVP9();

  ~VideoEncoderVpx() override;

  // VideoEncoder interface.
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  scoped_ptr<VideoPacket> Encode(const webrtc::DesktopFrame& frame) override;

 private:
  explicit VideoEncoderVpx(bool use_vp9);

  // (Re)Configures this instance to encode frames of the specified |size|,
  // with the configured lossless color & encoding modes.
  void Configure(const webrtc::DesktopSize& size);

  // Prepares |image_| for encoding. Writes updated rectangles into
  // |updated_region|.
  void PrepareImage(const webrtc::DesktopFrame& frame,
                    webrtc::DesktopRegion* updated_region);

  // Updates the active map according to |updated_region|. Active map is then
  // given to the encoder to speed up encoding.
  void PrepareActiveMap(const webrtc::DesktopRegion& updated_region);

  // True if the encoder is for VP9, false for VP8.
  const bool use_vp9_;

  // Options controlling VP9 encode quantization and color space.
  // These are always off (false) for VP8.
  bool lossless_encode_;
  bool lossless_color_;

  // Holds the initialized & configured codec.
  ScopedVpxCodec codec_;

  // Used to generate zero-based frame timestamps.
  base::TimeTicks timestamp_base_;

  // VPX image and buffer to hold the actual YUV planes.
  scoped_ptr<vpx_image_t> image_;
  scoped_ptr<uint8[]> image_buffer_;

  // Active map used to optimize out processing of un-changed macroblocks.
  scoped_ptr<uint8[]> active_map_;
  int active_map_width_;
  int active_map_height_;

  // Used to help initialize VideoPackets from DesktopFrames.
  VideoEncoderHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderVpx);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_VP8_H_
