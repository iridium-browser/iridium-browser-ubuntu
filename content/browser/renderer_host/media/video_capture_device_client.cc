// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_device_client.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv.h"

using media::VideoCaptureFormat;
using media::VideoFrame;

namespace content {

// Class combining a Client::Buffer interface implementation and a pool buffer
// implementation to guarantee proper cleanup on destruction on our side.
class AutoReleaseBuffer : public media::VideoCaptureDevice::Client::Buffer {
 public:
  AutoReleaseBuffer(const scoped_refptr<VideoCaptureBufferPool>& pool,
                    int buffer_id,
                    void* data,
                    size_t size)
      : pool_(pool),
        id_(buffer_id),
        data_(data),
        size_(size) {
    DCHECK(pool_.get());
  }
  int id() const override { return id_; }
  void* data() const override { return data_; }
  size_t size() const override { return size_; }

 private:
  ~AutoReleaseBuffer() override { pool_->RelinquishProducerReservation(id_); }

  const scoped_refptr<VideoCaptureBufferPool> pool_;
  const int id_;
  void* const data_;
  const size_t size_;
};

VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    const base::WeakPtr<VideoCaptureController>& controller,
    const scoped_refptr<VideoCaptureBufferPool>& buffer_pool)
    : controller_(controller),
      buffer_pool_(buffer_pool),
      last_captured_pixel_format_(media::PIXEL_FORMAT_UNKNOWN) {}

VideoCaptureDeviceClient::~VideoCaptureDeviceClient() {}

void VideoCaptureDeviceClient::OnIncomingCapturedData(
    const uint8* data,
    int length,
    const VideoCaptureFormat& frame_format,
    int rotation,
    const base::TimeTicks& timestamp) {
  TRACE_EVENT0("video", "VideoCaptureController::OnIncomingCapturedData");

  if (last_captured_pixel_format_ != frame_format.pixel_format) {
    OnLog("Pixel format: " + media::VideoCaptureFormat::PixelFormatToString(
                                 frame_format.pixel_format));
    last_captured_pixel_format_ = frame_format.pixel_format;
  }

  if (!frame_format.IsValid())
    return;

  // |chopped_{width,height} and |new_unrotated_{width,height}| are the lowest
  // bit decomposition of {width, height}, grabbing the odd and even parts.
  const int chopped_width = frame_format.frame_size.width() & 1;
  const int chopped_height = frame_format.frame_size.height() & 1;
  const int new_unrotated_width = frame_format.frame_size.width() & ~1;
  const int new_unrotated_height = frame_format.frame_size.height() & ~1;

  int destination_width = new_unrotated_width;
  int destination_height = new_unrotated_height;
  if (rotation == 90 || rotation == 270) {
    destination_width = new_unrotated_height;
    destination_height = new_unrotated_width;
  }

  DCHECK_EQ(rotation % 90, 0)
      << " Rotation must be a multiple of 90, now: " << rotation;
  libyuv::RotationMode rotation_mode = libyuv::kRotate0;
  if (rotation == 90)
    rotation_mode = libyuv::kRotate90;
  else if (rotation == 180)
    rotation_mode = libyuv::kRotate180;
  else if (rotation == 270)
    rotation_mode = libyuv::kRotate270;

  const gfx::Size dimensions(destination_width, destination_height);
  if (!VideoFrame::IsValidConfig(VideoFrame::I420,
                                 dimensions,
                                 gfx::Rect(dimensions),
                                 dimensions)) {
    return;
  }

  scoped_refptr<Buffer> buffer = ReserveOutputBuffer(VideoFrame::I420,
                                                     dimensions);
  if (!buffer.get())
    return;

  uint8* const yplane = reinterpret_cast<uint8*>(buffer->data());
  uint8* const uplane =
      yplane + VideoFrame::PlaneAllocationSize(VideoFrame::I420,
                                               VideoFrame::kYPlane, dimensions);
  uint8* const vplane =
      uplane + VideoFrame::PlaneAllocationSize(VideoFrame::I420,
                                               VideoFrame::kUPlane, dimensions);
  int yplane_stride = dimensions.width();
  int uv_plane_stride = yplane_stride / 2;
  int crop_x = 0;
  int crop_y = 0;
  libyuv::FourCC origin_colorspace = libyuv::FOURCC_ANY;

  bool flip = false;
  switch (frame_format.pixel_format) {
    case media::PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      break;
    case media::PIXEL_FORMAT_I420:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_I420;
      break;
    case media::PIXEL_FORMAT_YV12:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YV12;
      break;
    case media::PIXEL_FORMAT_NV12:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_NV12;
      break;
    case media::PIXEL_FORMAT_NV21:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_NV21;
      break;
    case media::PIXEL_FORMAT_YUY2:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_YUY2;
      break;
    case media::PIXEL_FORMAT_UYVY:
      DCHECK(!chopped_width && !chopped_height);
      origin_colorspace = libyuv::FOURCC_UYVY;
      break;
    case media::PIXEL_FORMAT_RGB24:
      origin_colorspace = libyuv::FOURCC_24BG;
#if defined(OS_WIN)
      // TODO(wjia): Currently, for RGB24 on WIN, capture device always
      // passes in positive src_width and src_height. Remove this hardcoded
      // value when nagative src_height is supported. The negative src_height
      // indicates that vertical flipping is needed.
      flip = true;
#endif
      break;
    case media::PIXEL_FORMAT_RGB32:
// Fallback to PIXEL_FORMAT_ARGB setting |flip| in Windows platforms.
#if defined(OS_WIN)
      flip = true;
#endif
    case media::PIXEL_FORMAT_ARGB:
      origin_colorspace = libyuv::FOURCC_ARGB;
      break;
    case media::PIXEL_FORMAT_MJPEG:
      origin_colorspace = libyuv::FOURCC_MJPG;
      break;
    default:
      NOTREACHED();
  }

  // The input |length| can be greater than the required buffer size because of
  // paddings and/or alignments, but it cannot be smaller.
  DCHECK_GE(static_cast<size_t>(length), frame_format.ImageAllocationSize());

  if (libyuv::ConvertToI420(data,
                            length,
                            yplane,
                            yplane_stride,
                            uplane,
                            uv_plane_stride,
                            vplane,
                            uv_plane_stride,
                            crop_x,
                            crop_y,
                            frame_format.frame_size.width(),
                            (flip ? -1 : 1) * frame_format.frame_size.height(),
                            new_unrotated_width,
                            new_unrotated_height,
                            rotation_mode,
                            origin_colorspace) != 0) {
    DLOG(WARNING) << "Failed to convert buffer's pixel format to I420 from "
                  << media::VideoCaptureFormat::PixelFormatToString(
                         frame_format.pixel_format);
    return;
  }
  scoped_refptr<VideoFrame> frame =
      VideoFrame::WrapExternalPackedMemory(
          VideoFrame::I420,
          dimensions,
          gfx::Rect(dimensions),
          dimensions,
          yplane,
          VideoFrame::AllocationSize(VideoFrame::I420, dimensions),
          base::SharedMemory::NULLHandle(),
          0,
          base::TimeDelta(),
          base::Closure());
  DCHECK(frame.get());
  frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                               frame_format.frame_rate);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_,
          buffer,
          frame,
          timestamp));
}

void
VideoCaptureDeviceClient::OnIncomingCapturedYuvData(
    const uint8* y_data,
    const uint8* u_data,
    const uint8* v_data,
    size_t y_stride,
    size_t u_stride,
    size_t v_stride,
    const VideoCaptureFormat& frame_format,
    int clockwise_rotation,
    const base::TimeTicks& timestamp) {
  TRACE_EVENT0("video", "VideoCaptureController::OnIncomingCapturedYuvData");
  DCHECK_EQ(frame_format.pixel_format, media::PIXEL_FORMAT_I420);
  DCHECK_EQ(clockwise_rotation, 0) << "Rotation not supported";

  scoped_refptr<Buffer> buffer = ReserveOutputBuffer(VideoFrame::I420,
                                                     frame_format.frame_size);
  if (!buffer.get())
    return;

  // Blit (copy) here from y,u,v into buffer.data()). Needed so we can return
  // the parameter buffer synchronously to the driver.
  const size_t y_plane_size = VideoFrame::PlaneAllocationSize(VideoFrame::I420,
      VideoFrame::kYPlane, frame_format.frame_size);
  const size_t u_plane_size = VideoFrame::PlaneAllocationSize(
      VideoFrame::I420, VideoFrame::kUPlane, frame_format.frame_size);
  uint8* const dst_y = reinterpret_cast<uint8*>(buffer->data());
  uint8* const dst_u = dst_y + y_plane_size;
  uint8* const dst_v = dst_u + u_plane_size;

  const size_t dst_y_stride = VideoFrame::RowBytes(
      VideoFrame::kYPlane, VideoFrame::I420, frame_format.frame_size.width());
  const size_t dst_u_stride = VideoFrame::RowBytes(
      VideoFrame::kUPlane, VideoFrame::I420, frame_format.frame_size.width());
  const size_t dst_v_stride = VideoFrame::RowBytes(
      VideoFrame::kVPlane, VideoFrame::I420, frame_format.frame_size.width());
  DCHECK_GE(y_stride, dst_y_stride);
  DCHECK_GE(u_stride, dst_u_stride);
  DCHECK_GE(v_stride, dst_v_stride);

  if (libyuv::I420Copy(y_data, y_stride,
                       u_data, u_stride,
                       v_data, v_stride,
                       dst_y, dst_y_stride,
                       dst_u, dst_u_stride,
                       dst_v, dst_v_stride,
                       frame_format.frame_size.width(),
                       frame_format.frame_size.height())) {
    DLOG(WARNING) << "Failed to copy buffer";
    return;
  }

  scoped_refptr<VideoFrame> video_frame = VideoFrame::WrapExternalYuvData(
      VideoFrame::I420, frame_format.frame_size,
      gfx::Rect(frame_format.frame_size), frame_format.frame_size, y_stride,
      u_stride, v_stride, dst_y, dst_u, dst_v, base::TimeDelta(),
      base::Closure());
  DCHECK(video_frame.get());
  video_frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                     frame_format.frame_rate);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_,
          buffer,
          video_frame,
          timestamp));
};

scoped_refptr<media::VideoCaptureDevice::Client::Buffer>
VideoCaptureDeviceClient::ReserveOutputBuffer(VideoFrame::Format format,
                                              const gfx::Size& dimensions) {
  const size_t frame_bytes = VideoFrame::AllocationSize(format, dimensions);
  if (format == VideoFrame::NATIVE_TEXTURE) {
    DCHECK_EQ(dimensions.width(), 0);
    DCHECK_EQ(dimensions.height(), 0);
  } else {
    DLOG_IF(ERROR, frame_bytes == 0) << "Error calculating allocation size";
  }

  int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;
  const int buffer_id =
      buffer_pool_->ReserveForProducer(frame_bytes, &buffer_id_to_drop);
  if (buffer_id == VideoCaptureBufferPool::kInvalidId)
    return NULL;
  void* data;
  size_t size;
  buffer_pool_->GetBufferInfo(buffer_id, &data, &size);

  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> output_buffer(
      new AutoReleaseBuffer(buffer_pool_, buffer_id, data, size));

  if (buffer_id_to_drop != VideoCaptureBufferPool::kInvalidId) {
    BrowserThread::PostTask(BrowserThread::IO,
        FROM_HERE,
        base::Bind(&VideoCaptureController::DoBufferDestroyedOnIOThread,
                   controller_, buffer_id_to_drop));
  }

  return output_buffer;
}

void
VideoCaptureDeviceClient::OnIncomingCapturedVideoFrame(
    const scoped_refptr<Buffer>& buffer,
    const scoped_refptr<VideoFrame>& frame,
    const base::TimeTicks& timestamp) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &VideoCaptureController::DoIncomingCapturedVideoFrameOnIOThread,
          controller_,
          buffer,
          frame,
          timestamp));
}

void VideoCaptureDeviceClient::OnError(
    const std::string& reason) {
  const std::string log_message = base::StringPrintf(
      "Error on video capture: %s, OS message: %s",
      reason.c_str(),
      logging::SystemErrorCodeToString(
          logging::GetLastSystemErrorCode()).c_str());
  DLOG(ERROR) << log_message;
  OnLog(log_message);
  BrowserThread::PostTask(BrowserThread::IO,
      FROM_HERE,
      base::Bind(&VideoCaptureController::DoErrorOnIOThread, controller_));
}

void VideoCaptureDeviceClient::OnLog(
    const std::string& message) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&VideoCaptureController::DoLogOnIOThread,
                                     controller_, message));
}

}  // namespace content
