/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp9/svc_rate_allocator.h"

#include <algorithm>
#include <cmath>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {
const float kSpatialLayeringRateScalingFactor = 0.55f;
const float kTemporalLayeringRateScalingFactor = 0.55f;
}  // namespace

SvcRateAllocator::SvcRateAllocator(const VideoCodec& codec) : codec_(codec) {
  RTC_DCHECK_EQ(codec.codecType, kVideoCodecVP9);
}

BitrateAllocation SvcRateAllocator::GetAllocation(uint32_t total_bitrate_bps,
                                                  uint32_t framerate_fps) {
  BitrateAllocation bitrate_allocation;

  size_t num_spatial_layers = codec_.VP9().numberOfSpatialLayers;
  RTC_CHECK(num_spatial_layers > 0);
  size_t num_temporal_layers = codec_.VP9().numberOfTemporalLayers;
  RTC_CHECK(num_temporal_layers > 0);

  if (codec_.maxBitrate != 0) {
    total_bitrate_bps = std::min(total_bitrate_bps, codec_.maxBitrate * 1000);
  }

  if (codec_.mode == kScreensharing) {
    // At screen sharing bitrate allocation is handled by VP9 encoder wrapper.
    bitrate_allocation.SetBitrate(0, 0, total_bitrate_bps);
    return bitrate_allocation;
  }

  std::vector<size_t> spatial_layer_bitrate_bps;

  if (codec_.spatialLayers[0].maxBitrate == 0) {
    // Layers' parameters are not initialized. Do simple split.
    spatial_layer_bitrate_bps =
        SplitBitrate(num_spatial_layers, total_bitrate_bps,
                     kSpatialLayeringRateScalingFactor);
  } else {
    // Distribute total bitrate across spatial layers. If there is not enough
    // bitrate to provide all layers with at least minimum required bitrate
    // then number of layers is reduced by one and distribution is repeated
    // until that condition is met or if number of layers is reduced to one.
    for (;; --num_spatial_layers) {
      spatial_layer_bitrate_bps =
          SplitBitrate(num_spatial_layers, total_bitrate_bps,
                       kSpatialLayeringRateScalingFactor);

      bool enough_bitrate = true;
      size_t excess_rate = 0;
      for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
        RTC_DCHECK_GT(codec_.spatialLayers[sl_idx].maxBitrate, 0);
        RTC_DCHECK_GE(codec_.spatialLayers[sl_idx].maxBitrate,
                      codec_.spatialLayers[sl_idx].minBitrate);

        const size_t min_bitrate_bps =
            codec_.spatialLayers[sl_idx].minBitrate * 1000;
        const size_t max_bitrate_bps =
            codec_.spatialLayers[sl_idx].maxBitrate * 1000;

        spatial_layer_bitrate_bps[sl_idx] += excess_rate;
        if (spatial_layer_bitrate_bps[sl_idx] < max_bitrate_bps) {
          excess_rate = 0;
        } else {
          excess_rate = spatial_layer_bitrate_bps[sl_idx] - max_bitrate_bps;
          spatial_layer_bitrate_bps[sl_idx] = max_bitrate_bps;
        }

        if (spatial_layer_bitrate_bps[sl_idx] < min_bitrate_bps) {
          enough_bitrate = false;
          break;
        }
      }

      if (enough_bitrate || num_spatial_layers == 1) {
        break;
      }
    }
  }

  for (size_t sl_idx = 0; sl_idx < num_spatial_layers; ++sl_idx) {
    std::vector<size_t> temporal_layer_bitrate_bps =
        SplitBitrate(num_temporal_layers, spatial_layer_bitrate_bps[sl_idx],
                     kTemporalLayeringRateScalingFactor);

    // Distribute rate across temporal layers. Allocate more bits to lower
    // layers since they are used for prediction of higher layers and their
    // references are far apart.
    if (num_temporal_layers == 1) {
      bitrate_allocation.SetBitrate(sl_idx, 0, temporal_layer_bitrate_bps[0]);
    } else if (num_temporal_layers == 2) {
      bitrate_allocation.SetBitrate(sl_idx, 0, temporal_layer_bitrate_bps[1]);
      bitrate_allocation.SetBitrate(sl_idx, 1, temporal_layer_bitrate_bps[0]);
    } else {
      RTC_CHECK_EQ(num_temporal_layers, 3);
      // In case of three temporal layers the high layer has two frames and the
      // middle layer has one frame within GOP (in between two consecutive low
      // layer frames). Thus high layer requires more bits (comparing pure
      // bitrate of layer, excluding bitrate of base layers) to keep quality on
      // par with lower layers.
      bitrate_allocation.SetBitrate(sl_idx, 0, temporal_layer_bitrate_bps[2]);
      bitrate_allocation.SetBitrate(sl_idx, 1, temporal_layer_bitrate_bps[0]);
      bitrate_allocation.SetBitrate(sl_idx, 2, temporal_layer_bitrate_bps[1]);
    }
  }

  return bitrate_allocation;
}

uint32_t SvcRateAllocator::GetPreferredBitrateBps(uint32_t framerate) {
  return GetAllocation(codec_.maxBitrate * 1000, framerate).get_sum_bps();
}

std::vector<size_t> SvcRateAllocator::SplitBitrate(size_t num_layers,
                                                   size_t total_bitrate,
                                                   float rate_scaling_factor) {
  std::vector<size_t> bitrates;

  float denominator = 0.0f;
  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    denominator += std::pow(rate_scaling_factor, layer_idx);
  }

  float numerator = std::pow(rate_scaling_factor, num_layers - 1);
  for (size_t layer_idx = 0; layer_idx < num_layers; ++layer_idx) {
    bitrates.push_back(numerator * total_bitrate / denominator);
    numerator /= rate_scaling_factor;
  }

  return bitrates;
}

}  // namespace webrtc
