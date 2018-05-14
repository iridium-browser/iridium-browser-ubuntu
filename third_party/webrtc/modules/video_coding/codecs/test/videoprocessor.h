/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_
#define MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/codecs/test/stats.h"
#include "modules/video_coding/codecs/test/test_config.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "test/testsupport/frame_reader.h"
#include "test/testsupport/frame_writer.h"

namespace webrtc {

class VideoBitrateAllocator;

namespace test {

// Handles encoding/decoding of video using the VideoEncoder/VideoDecoder
// interfaces. This is done in a sequential manner in order to be able to
// measure times properly.
// The class processes a frame at the time for the configured input file.
// It maintains state of where in the source input file the processing is at.
//
// Note this class is not thread safe and is meant for simple testing purposes.
class VideoProcessor {
 public:
  using VideoDecoderList = std::vector<std::unique_ptr<VideoDecoder>>;
  using IvfFileWriterList = std::vector<std::unique_ptr<IvfFileWriter>>;
  using FrameWriterList = std::vector<std::unique_ptr<FrameWriter>>;

  VideoProcessor(webrtc::VideoEncoder* encoder,
                 VideoDecoderList* decoders,
                 FrameReader* input_frame_reader,
                 const TestConfig& config,
                 Stats* stats,
                 IvfFileWriterList* encoded_frame_writers,
                 FrameWriterList* decoded_frame_writers);
  ~VideoProcessor();

  // Reads a frame and sends it to the encoder. When the encode callback
  // is received, the encoded frame is buffered. After encoding is finished
  // buffered frame is sent to decoder. Quality evaluation is done in
  // the decode callback.
  void ProcessFrame();

  // Updates the encoder with target rates. Must be called at least once.
  void SetRates(size_t bitrate_kbps, size_t framerate_fps);

 private:
  class VideoProcessorEncodeCompleteCallback
      : public webrtc::EncodedImageCallback {
   public:
    explicit VideoProcessorEncodeCompleteCallback(
        VideoProcessor* video_processor)
        : video_processor_(video_processor),
          task_queue_(rtc::TaskQueue::Current()) {
      RTC_DCHECK(task_queue_);
    }

    Result OnEncodedImage(
        const webrtc::EncodedImage& encoded_image,
        const webrtc::CodecSpecificInfo* codec_specific_info,
        const webrtc::RTPFragmentationHeader* fragmentation) override {
      RTC_CHECK(codec_specific_info);

      // Post the callback to the right task queue, if needed.
      if (!task_queue_->IsCurrent()) {
        task_queue_->PostTask(
            std::unique_ptr<rtc::QueuedTask>(new EncodeCallbackTask(
                video_processor_, encoded_image, codec_specific_info)));
        return Result(Result::OK, 0);
      }

      video_processor_->FrameEncoded(encoded_image, *codec_specific_info);
      return Result(Result::OK, 0);
    }

   private:
    class EncodeCallbackTask : public rtc::QueuedTask {
     public:
      EncodeCallbackTask(VideoProcessor* video_processor,
                         const webrtc::EncodedImage& encoded_image,
                         const webrtc::CodecSpecificInfo* codec_specific_info)
          : video_processor_(video_processor),
            buffer_(encoded_image._buffer, encoded_image._length),
            encoded_image_(encoded_image),
            codec_specific_info_(*codec_specific_info) {
        encoded_image_._buffer = buffer_.data();
      }

      bool Run() override {
        video_processor_->FrameEncoded(encoded_image_, codec_specific_info_);
        return true;
      }

     private:
      VideoProcessor* const video_processor_;
      rtc::Buffer buffer_;
      webrtc::EncodedImage encoded_image_;
      const webrtc::CodecSpecificInfo codec_specific_info_;
    };

    VideoProcessor* const video_processor_;
    rtc::TaskQueue* const task_queue_;
  };

  class VideoProcessorDecodeCompleteCallback
      : public webrtc::DecodedImageCallback {
   public:
    explicit VideoProcessorDecodeCompleteCallback(
        VideoProcessor* video_processor)
        : video_processor_(video_processor),
          task_queue_(rtc::TaskQueue::Current()) {
      RTC_DCHECK(task_queue_);
    }

    int32_t Decoded(webrtc::VideoFrame& image) override {
      // Post the callback to the right task queue, if needed.
      if (!task_queue_->IsCurrent()) {
        task_queue_->PostTask(
            [this, image]() { video_processor_->FrameDecoded(image); });
        return 0;
      }
      video_processor_->FrameDecoded(image);
      return 0;
    }

    int32_t Decoded(webrtc::VideoFrame& image,
                    int64_t decode_time_ms) override {
      return Decoded(image);
    }

    void Decoded(webrtc::VideoFrame& image,
                 rtc::Optional<int32_t> decode_time_ms,
                 rtc::Optional<uint8_t> qp) override {
      Decoded(image);
    }

   private:
    VideoProcessor* const video_processor_;
    rtc::TaskQueue* const task_queue_;
  };

  // Invoked by the callback adapter when a frame has completed encoding.
  void FrameEncoded(const webrtc::EncodedImage& encoded_image,
                    const webrtc::CodecSpecificInfo& codec_specific);

  // Invoked by the callback adapter when a frame has completed decoding.
  void FrameDecoded(const webrtc::VideoFrame& image);

  void CopyEncodedImage(const EncodedImage& encoded_image,
                        const VideoCodecType codec,
                        size_t frame_number,
                        size_t simulcast_svc_idx);

  void CalculateFrameQuality(const VideoFrame& ref_frame,
                             const VideoFrame& dec_frame,
                             FrameStatistics* frame_stat);

  void WriteDecodedFrameToFile(rtc::Buffer* buffer, size_t simulcast_svc_idx);

  TestConfig config_ RTC_GUARDED_BY(sequence_checker_);

  const size_t num_simulcast_or_spatial_layers_;

  webrtc::VideoEncoder* const encoder_;
  VideoDecoderList* const decoders_;
  const std::unique_ptr<VideoBitrateAllocator> bitrate_allocator_;
  BitrateAllocation bitrate_allocation_ RTC_GUARDED_BY(sequence_checker_);

  // Adapters for the codec callbacks.
  VideoProcessorEncodeCompleteCallback encode_callback_;
  VideoProcessorDecodeCompleteCallback decode_callback_;

  // Input frames. Used as reference at frame quality evaluation.
  // Async codecs might queue frames. To handle that we keep input frame
  // and release it after corresponding coded frame is decoded and quality
  // measurement is done.
  std::map<size_t, std::unique_ptr<VideoFrame>> input_frames_
      RTC_GUARDED_BY(sequence_checker_);

  FrameReader* const input_frame_reader_;

  // These (optional) file writers are used to persistently store the encoded
  // and decoded bitstreams. The purpose is to give the experimenter an option
  // to subjectively evaluate the quality of the processing. Each frame writer
  // is enabled by being non-null.
  IvfFileWriterList* const encoded_frame_writers_;
  FrameWriterList* const decoded_frame_writers_;

  // Keep track of inputed/encoded/decoded frames, so we can detect frame drops.
  size_t last_inputed_frame_num_ RTC_GUARDED_BY(sequence_checker_);
  size_t last_encoded_frame_num_ RTC_GUARDED_BY(sequence_checker_);
  size_t last_encoded_simulcast_svc_idx_ RTC_GUARDED_BY(sequence_checker_);
  size_t last_decoded_frame_num_ RTC_GUARDED_BY(sequence_checker_);
  size_t num_encoded_frames_ RTC_GUARDED_BY(sequence_checker_);
  size_t num_decoded_frames_ RTC_GUARDED_BY(sequence_checker_);

  // Map of frame size (in pixels) to simulcast/spatial layer index.
  std::map<size_t, size_t> frame_wxh_to_simulcast_svc_idx_
      RTC_GUARDED_BY(sequence_checker_);

  // Encoder delivers coded frame layer-by-layer. We store coded frames and
  // then, after all layers are encoded, decode them. Such separation of
  // frame processing on superframe level simplifies encoding/decoding time
  // measurement.
  std::map<size_t, EncodedImage> last_encoded_frames_
      RTC_GUARDED_BY(sequence_checker_);

  // Keep track of the last successfully decoded frame, since we write that
  // frame to disk when frame got dropped or decoding fails.
  std::map<size_t, rtc::Buffer> last_decoded_frame_buffers_
      RTC_GUARDED_BY(sequence_checker_);

  // Statistics.
  Stats* const stats_;

  rtc::SequencedTaskChecker sequence_checker_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoProcessor);
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_TEST_VIDEOPROCESSOR_H_
