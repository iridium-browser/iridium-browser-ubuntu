/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DENOISER_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DENOISER_H_

#include <memory>

#include "webrtc/modules/video_processing/util/denoiser_filter.h"
#include "webrtc/modules/video_processing/util/noise_estimation.h"
#include "webrtc/modules/video_processing/util/skin_detection.h"

namespace webrtc {

class VideoDenoiser {
 public:
  explicit VideoDenoiser(bool runtime_cpu_detection);

  // TODO(nisse): Let the denoised_frame and denoised_frame_prev be
  // member variables referencing two I420Buffer, and return a refptr
  // to the current one. When we also move the double-buffering logic
  // from the caller.
  void DenoiseFrame(const rtc::scoped_refptr<VideoFrameBuffer>& frame,
                    // Buffers are allocated/replaced when dimensions
                    // change.
                    rtc::scoped_refptr<I420Buffer>* denoised_frame,
                    rtc::scoped_refptr<I420Buffer>* denoised_frame_prev,
                    bool noise_estimation_enabled);

 private:
  void DenoiserReset(const rtc::scoped_refptr<VideoFrameBuffer>& frame,
                     rtc::scoped_refptr<I420Buffer>* denoised_frame,
                     rtc::scoped_refptr<I420Buffer>* denoised_frame_prev);

  // Check the mb position, return 1: close to the frame center (between 1/8
  // and 7/8 of width/height), 3: close to the border (out of 1/16 and 15/16
  // of width/height), 2: in between.
  int PositionCheck(int mb_row, int mb_col, int noise_level);

  // To reduce false detection in moving object detection (MOD).
  void ReduceFalseDetection(const std::unique_ptr<uint8_t[]>& d_status,
                            std::unique_ptr<uint8_t[]>* d_status_red,
                            int noise_level);

  // Return whether a block might cause trailing artifact by checking if one of
  // its neighbor blocks is a moving edge block.
  bool IsTrailingBlock(const std::unique_ptr<uint8_t[]>& d_status,
                       int mb_row,
                       int mb_col);

  // Copy input blocks to dst buffer on moving object blocks (MOB).
  void CopySrcOnMOB(const uint8_t* y_src, uint8_t* y_dst);

  // Copy luma margin blocks when frame width/height not divisible by 16.
  void CopyLumaOnMargin(const uint8_t* y_src, uint8_t* y_dst);

  int width_;
  int height_;
  int mb_rows_;
  int mb_cols_;
  int stride_y_;
  int stride_u_;
  int stride_v_;
  CpuType cpu_type_;
  std::unique_ptr<DenoiserFilter> filter_;
  std::unique_ptr<NoiseEstimation> ne_;
  // 1 for moving edge block, 0 for static block.
  std::unique_ptr<uint8_t[]> moving_edge_;
  // 1 for moving object block, 0 for static block.
  std::unique_ptr<uint8_t[]> moving_object_;
  // x_density_ and y_density_ are used in MOD process.
  std::unique_ptr<uint8_t[]> x_density_;
  std::unique_ptr<uint8_t[]> y_density_;
  // Save the return values by MbDenoise for each block.
  std::unique_ptr<DenoiserDecision[]> mb_filter_decision_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DENOISER_H_
