  // Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/skcanvas_video_renderer.h"

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "skia/ext/refptr.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrPaint.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "third_party/skia/include/gpu/GrTextureProvider.h"
#include "third_party/skia/include/gpu/SkGr.h"
#include "ui/gfx/geometry/rect_f.h"

// Skia internal format depends on a platform. On Android it is ABGR, on others
// it is ARGB.
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToARGB
#define LIBYUV_I422_TO_ARGB libyuv::I422ToARGB
#elif SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToABGR
#define LIBYUV_I422_TO_ARGB libyuv::I422ToABGR
#else
#error Unexpected Skia ARGB_8888 layout!
#endif

namespace media {

namespace {

// This class keeps the last image drawn.
// We delete the temporary resource if it is not used for 3 seconds.
const int kTemporaryResourceDeletionDelay = 3;  // Seconds;

bool CheckColorSpace(const VideoFrame* video_frame, ColorSpace color_space) {
  int result;
  return video_frame->metadata()->GetInteger(
             VideoFrameMetadata::COLOR_SPACE, &result) &&
         result == color_space;
}

class SyncPointClientImpl : public VideoFrame::SyncPointClient {
 public:
  explicit SyncPointClientImpl(gpu::gles2::GLES2Interface* gl) : gl_(gl) {}
  ~SyncPointClientImpl() override {}
  uint32 InsertSyncPoint() override { return gl_->InsertSyncPointCHROMIUM(); }
  void WaitSyncPoint(uint32 sync_point) override {
    gl_->WaitSyncPointCHROMIUM(sync_point);
  }

 private:
  gpu::gles2::GLES2Interface* gl_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncPointClientImpl);
};

skia::RefPtr<SkImage> NewSkImageFromVideoFrameYUVTextures(
    const VideoFrame* video_frame,
    const Context3D& context_3d) {
  // Support only TEXTURE_YUV_420.
  DCHECK(video_frame->HasTextures());
  DCHECK_EQ(media::PIXEL_FORMAT_I420, video_frame->format());
  DCHECK_EQ(3u, media::VideoFrame::NumPlanes(video_frame->format()));

  gpu::gles2::GLES2Interface* gl = context_3d.gl;
  DCHECK(gl);
  gfx::Size ya_tex_size = video_frame->coded_size();
  gfx::Size uv_tex_size((ya_tex_size.width() + 1) / 2,
                        (ya_tex_size.height() + 1) / 2);

  unsigned source_textures[3] = {0};
  for (size_t i = 0; i < media::VideoFrame::NumPlanes(video_frame->format());
       ++i) {
    // Get the texture from the mailbox and wrap it in a GrTexture.
    const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(i);
    DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
           mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES ||
           mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB);
    gl->WaitSyncPointCHROMIUM(mailbox_holder.sync_point);
    source_textures[i] = gl->CreateAndConsumeTextureCHROMIUM(
        mailbox_holder.texture_target, mailbox_holder.mailbox.name);

    // TODO(dcastagna): avoid this copy once Skia supports native textures
    // with a texture target different than TEXTURE_2D.
    // crbug.com/505026
    if (mailbox_holder.texture_target != GL_TEXTURE_2D) {
      unsigned texture_copy = 0;
      gl->GenTextures(1, &texture_copy);
      DCHECK(texture_copy);
      gl->BindTexture(GL_TEXTURE_2D, texture_copy);
      gl->CopyTextureCHROMIUM(GL_TEXTURE_2D, source_textures[i], texture_copy,
                              GL_RGB, GL_UNSIGNED_BYTE, false, true, false);

      gl->DeleteTextures(1, &source_textures[i]);
      source_textures[i] = texture_copy;
    }
  }
  GrBackendObject handles[3] = {
      source_textures[0], source_textures[1], source_textures[2]};

  SkISize yuvSizes[] = {
      {ya_tex_size.width(), ya_tex_size.height()},
      {uv_tex_size.width(), uv_tex_size.height()},
      {uv_tex_size.width(), uv_tex_size.height()},
  };

  SkYUVColorSpace color_space = kRec601_SkYUVColorSpace;
  if (CheckColorSpace(video_frame, media::COLOR_SPACE_JPEG))
    color_space = kJPEG_SkYUVColorSpace;
  else if (CheckColorSpace(video_frame, media::COLOR_SPACE_HD_REC709))
    color_space = kRec709_SkYUVColorSpace;

  SkImage* img = SkImage::NewFromYUVTexturesCopy(context_3d.gr_context,
                                                 color_space, handles, yuvSizes,
                                                 kTopLeft_GrSurfaceOrigin);
  gl->DeleteTextures(3, source_textures);
  return skia::AdoptRef(img);
}

// Creates a SkImage from a |video_frame| backed by native resources.
// The SkImage will take ownership of the underlying resource.
skia::RefPtr<SkImage> NewSkImageFromVideoFrameNative(
    VideoFrame* video_frame,
    const Context3D& context_3d) {
  DCHECK(PIXEL_FORMAT_ARGB == video_frame->format() ||
         PIXEL_FORMAT_UYVY == video_frame->format());

  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
         mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB ||
         mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES)
      << mailbox_holder.texture_target;

  gpu::gles2::GLES2Interface* gl = context_3d.gl;
  unsigned source_texture = 0;
  if (mailbox_holder.texture_target != GL_TEXTURE_2D) {
    // TODO(dcastagna): At the moment Skia doesn't support targets different
    // than GL_TEXTURE_2D.  Avoid this copy once
    // https://code.google.com/p/skia/issues/detail?id=3868 is addressed.
    gl->GenTextures(1, &source_texture);
    DCHECK(source_texture);
    gl->BindTexture(GL_TEXTURE_2D, source_texture);
    SkCanvasVideoRenderer::CopyVideoFrameSingleTextureToGLTexture(
        gl, video_frame, source_texture, GL_RGBA, GL_UNSIGNED_BYTE, true,
        false);
  } else {
    gl->WaitSyncPointCHROMIUM(mailbox_holder.sync_point);
    source_texture = gl->CreateAndConsumeTextureCHROMIUM(
        mailbox_holder.texture_target, mailbox_holder.mailbox.name);
  }
  GrBackendTextureDesc desc;
  desc.fFlags = kRenderTarget_GrBackendTextureFlag;
  desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  desc.fWidth = video_frame->coded_size().width();
  desc.fHeight = video_frame->coded_size().height();
  desc.fConfig = kRGBA_8888_GrPixelConfig;
  desc.fTextureHandle = source_texture;
  return skia::AdoptRef(
      SkImage::NewFromAdoptedTexture(context_3d.gr_context, desc));
}

}  // anonymous namespace

// Generates an RGB image from a VideoFrame. Convert YUV to RGB plain on GPU.
class VideoImageGenerator : public SkImageGenerator {
 public:
  VideoImageGenerator(const scoped_refptr<VideoFrame>& frame)
      : SkImageGenerator(
            SkImageInfo::MakeN32Premul(frame->visible_rect().width(),
                                       frame->visible_rect().height())),
        frame_(frame) {
    DCHECK(!frame_->HasTextures());
  }
  ~VideoImageGenerator() override {}

 protected:
  bool onGetPixels(const SkImageInfo& info,
                   void* pixels,
                   size_t row_bytes,
                   SkPMColor ctable[],
                   int* ctable_count) override {
    // If skia couldn't do the YUV conversion on GPU, we will on CPU.
    SkCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(frame_.get(), pixels,
                                                        row_bytes);
    return true;
  }

  bool onGetYUV8Planes(SkISize sizes[3],
                       void* planes[3],
                       size_t row_bytes[3],
                       SkYUVColorSpace* color_space) override {
    if (!media::IsYuvPlanar(frame_->format()) ||
        // TODO(rileya): Skia currently doesn't support YUVA conversion. Remove
        // this case once it does. As-is we will fall back on the pure-software
        // path in this case.
        frame_->format() == PIXEL_FORMAT_YV12A) {
      return false;
    }

    if (color_space) {
      if (CheckColorSpace(frame_.get(), COLOR_SPACE_JPEG))
        *color_space = kJPEG_SkYUVColorSpace;
      else if (CheckColorSpace(frame_.get(), COLOR_SPACE_HD_REC709))
        *color_space = kRec709_SkYUVColorSpace;
      else
        *color_space = kRec601_SkYUVColorSpace;
    }

    for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane;
         ++plane) {
      if (sizes) {
        const gfx::Size size =
            VideoFrame::PlaneSize(frame_->format(), plane,
                                  gfx::Size(frame_->visible_rect().width(),
                                            frame_->visible_rect().height()));
        sizes[plane].set(size.width(), size.height());
      }
      if (row_bytes && planes) {
        size_t offset;
        const int y_shift =
            (frame_->format() == media::PIXEL_FORMAT_YV16) ? 0 : 1;
        if (plane == VideoFrame::kYPlane) {
          offset = (frame_->stride(VideoFrame::kYPlane) *
                    frame_->visible_rect().y()) +
                   frame_->visible_rect().x();
        } else {
          offset = (frame_->stride(VideoFrame::kUPlane) *
                    (frame_->visible_rect().y() >> y_shift)) +
                   (frame_->visible_rect().x() >> 1);
        }

        // Copy the frame to the supplied memory.
        // TODO: Find a way (API change?) to avoid this copy.
        char* out_line = static_cast<char*>(planes[plane]);
        int out_line_stride = row_bytes[plane];
        uint8* in_line = frame_->data(plane) + offset;
        int in_line_stride = frame_->stride(plane);
        int plane_height = sizes[plane].height();
        if (in_line_stride == out_line_stride) {
          memcpy(out_line, in_line, plane_height * in_line_stride);
        } else {
          // Different line padding so need to copy one line at a time.
          int bytes_to_copy_per_line = out_line_stride < in_line_stride
                                           ? out_line_stride
                                           : in_line_stride;
          for (int line_no = 0; line_no < plane_height; line_no++) {
            memcpy(out_line, in_line, bytes_to_copy_per_line);
            in_line += in_line_stride;
            out_line += out_line_stride;
          }
        }
      }
    }
    return true;
  }

 private:
  scoped_refptr<VideoFrame> frame_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoImageGenerator);
};

SkCanvasVideoRenderer::SkCanvasVideoRenderer()
    : last_image_deleting_timer_(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kTemporaryResourceDeletionDelay),
          this,
          &SkCanvasVideoRenderer::ResetCache) {}

SkCanvasVideoRenderer::~SkCanvasVideoRenderer() {
  ResetCache();
}

void SkCanvasVideoRenderer::Paint(const scoped_refptr<VideoFrame>& video_frame,
                                  SkCanvas* canvas,
                                  const gfx::RectF& dest_rect,
                                  uint8 alpha,
                                  SkXfermode::Mode mode,
                                  VideoRotation video_rotation,
                                  const Context3D& context_3d) {
  if (alpha == 0) {
    return;
  }

  SkRect dest;
  dest.set(dest_rect.x(), dest_rect.y(), dest_rect.right(), dest_rect.bottom());

  SkPaint paint;
  paint.setAlpha(alpha);

  // Paint black rectangle if there isn't a frame available or the
  // frame has an unexpected format.
  if (!video_frame.get() || video_frame->natural_size().IsEmpty() ||
      !(media::IsYuvPlanar(video_frame->format()) ||
        video_frame->HasTextures())) {
    canvas->drawRect(dest, paint);
    canvas->flush();
    return;
  }

  gpu::gles2::GLES2Interface* gl = context_3d.gl;

  if (!last_image_ || video_frame->timestamp() != last_timestamp_) {
    ResetCache();
    // Generate a new image.
    // Note: Skia will hold onto |video_frame| via |video_generator| only when
    // |video_frame| is software.
    // Holding |video_frame| longer than this call when using GPUVideoDecoder
    // could cause problems since the pool of VideoFrames has a fixed size.
    if (video_frame->HasTextures()) {
      DCHECK(context_3d.gr_context);
      DCHECK(gl);
      if (media::VideoFrame::NumPlanes(video_frame->format()) == 1) {
        last_image_ =
            NewSkImageFromVideoFrameNative(video_frame.get(), context_3d);
      } else {
        last_image_ =
            NewSkImageFromVideoFrameYUVTextures(video_frame.get(), context_3d);
      }
    } else {
      auto video_generator = new VideoImageGenerator(video_frame);
      last_image_ = skia::AdoptRef(SkImage::NewFromGenerator(video_generator));
    }
    if (!last_image_)  // Couldn't create the SkImage.
      return;
    last_timestamp_ = video_frame->timestamp();
  }
  last_image_deleting_timer_.Reset();

  paint.setXfermodeMode(mode);
  paint.setFilterQuality(kLow_SkFilterQuality);

  const bool need_transform =
      video_rotation != VIDEO_ROTATION_0 ||
      dest_rect.size() != video_frame->visible_rect().size() ||
      !dest_rect.origin().IsOrigin();
  if (need_transform) {
    canvas->save();
    canvas->translate(
        SkFloatToScalar(dest_rect.x() + (dest_rect.width() * 0.5f)),
        SkFloatToScalar(dest_rect.y() + (dest_rect.height() * 0.5f)));
    SkScalar angle = SkFloatToScalar(0.0f);
    switch (video_rotation) {
      case VIDEO_ROTATION_0:
        break;
      case VIDEO_ROTATION_90:
        angle = SkFloatToScalar(90.0f);
        break;
      case VIDEO_ROTATION_180:
        angle = SkFloatToScalar(180.0f);
        break;
      case VIDEO_ROTATION_270:
        angle = SkFloatToScalar(270.0f);
        break;
    }
    canvas->rotate(angle);

    gfx::SizeF rotated_dest_size = dest_rect.size();
    if (video_rotation == VIDEO_ROTATION_90 ||
        video_rotation == VIDEO_ROTATION_270) {
      rotated_dest_size =
          gfx::SizeF(rotated_dest_size.height(), rotated_dest_size.width());
    }
    canvas->scale(
        SkFloatToScalar(rotated_dest_size.width() / last_image_->width()),
        SkFloatToScalar(rotated_dest_size.height() / last_image_->height()));
    canvas->translate(-SkFloatToScalar(last_image_->width() * 0.5f),
                      -SkFloatToScalar(last_image_->height() * 0.5f));
  }

  // This is a workaround for crbug.com/524717. SkBitmaps are read back before a
  // SkPicture is sent to multiple threads while SkImages are not. The long term
  // solution is for Skia to provide a SkPicture filter that makes a picture
  // safe for multiple CPU raster threads (skbug.com/4321). We limit the
  // workaround to cases where the src frame is a texture and the canvas is
  // recording.
  if (last_image_.get()->getTexture() &&
      canvas->imageInfo().colorType() == kUnknown_SkColorType) {
    SkBitmap bmp;
    GrWrapTextureInBitmap(last_image_.get()->getTexture(),
        last_image_.get()->width(), last_image_.get()->height(), true, &bmp);
    // Even though the bitmap is logically immutable we do not mark it as such
    // because doing so would defer readback until rasterization, which will be
    // on another thread and is therefore unsafe.
    canvas->drawBitmap(bmp, 0, 0, &paint);
  } else {
    canvas->drawImage(last_image_.get(), 0, 0, &paint);
  }

  if (need_transform)
    canvas->restore();
  // Make sure to flush so we can remove the videoframe from the generator.
  canvas->flush();

  if (video_frame->HasTextures()) {
    DCHECK(gl);
    SyncPointClientImpl client(gl);
    video_frame->UpdateReleaseSyncPoint(&client);
  }
}

void SkCanvasVideoRenderer::Copy(const scoped_refptr<VideoFrame>& video_frame,
                                 SkCanvas* canvas,
                                 const Context3D& context_3d) {
  Paint(video_frame, canvas, video_frame->visible_rect(), 0xff,
        SkXfermode::kSrc_Mode, media::VIDEO_ROTATION_0, context_3d);
}

// static
void SkCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
    const VideoFrame* video_frame,
    void* rgb_pixels,
    size_t row_bytes) {
  if (!video_frame->IsMappable()) {
    NOTREACHED() << "Cannot extract pixels from non-CPU frame formats.";
    return;
  }
  if (!media::IsYuvPlanar(video_frame->format())) {
    NOTREACHED() << "Non YUV formats are not supported";
    return;
  }

  DCHECK_EQ(video_frame->stride(VideoFrame::kUPlane),
            video_frame->stride(VideoFrame::kVPlane));

  const int y_shift =
      (video_frame->format() == media::PIXEL_FORMAT_YV16) ? 0 : 1;
  // Use the "left" and "top" of the destination rect to locate the offset
  // in Y, U and V planes.
  const size_t y_offset = (video_frame->stride(VideoFrame::kYPlane) *
                           video_frame->visible_rect().y()) +
                          video_frame->visible_rect().x();
  // For format YV12, there is one U, V value per 2x2 block.
  // For format YV16, there is one U, V value per 2x1 block.
  const size_t uv_offset = (video_frame->stride(VideoFrame::kUPlane) *
                            (video_frame->visible_rect().y() >> y_shift)) +
                           (video_frame->visible_rect().x() >> 1);

  switch (video_frame->format()) {
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
      if (CheckColorSpace(video_frame, COLOR_SPACE_JPEG)) {
        ConvertYUVToRGB32(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          static_cast<uint8*>(rgb_pixels),
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->stride(VideoFrame::kUPlane),
          row_bytes,
          YV12J);
      } else if (CheckColorSpace(video_frame, COLOR_SPACE_HD_REC709)) {
        ConvertYUVToRGB32(video_frame->data(VideoFrame::kYPlane) + y_offset,
                          video_frame->data(VideoFrame::kUPlane) + uv_offset,
                          video_frame->data(VideoFrame::kVPlane) + uv_offset,
                          static_cast<uint8*>(rgb_pixels),
                          video_frame->visible_rect().width(),
                          video_frame->visible_rect().height(),
                          video_frame->stride(VideoFrame::kYPlane),
                          video_frame->stride(VideoFrame::kUPlane), row_bytes,
                          YV12HD);
      } else {
        LIBYUV_I420_TO_ARGB(
            video_frame->data(VideoFrame::kYPlane) + y_offset,
            video_frame->stride(VideoFrame::kYPlane),
            video_frame->data(VideoFrame::kUPlane) + uv_offset,
            video_frame->stride(VideoFrame::kUPlane),
            video_frame->data(VideoFrame::kVPlane) + uv_offset,
            video_frame->stride(VideoFrame::kVPlane),
            static_cast<uint8*>(rgb_pixels),
            row_bytes,
            video_frame->visible_rect().width(),
            video_frame->visible_rect().height());
      }
      break;
    case PIXEL_FORMAT_YV16:
      LIBYUV_I422_TO_ARGB(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(VideoFrame::kUPlane),
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(VideoFrame::kVPlane),
          static_cast<uint8*>(rgb_pixels),
          row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
      break;

  case PIXEL_FORMAT_YV12A:
      libyuv::I420AlphaToARGB(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(VideoFrame::kUPlane),
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(VideoFrame::kVPlane),
          video_frame->data(VideoFrame::kAPlane) + y_offset,
          video_frame->stride(VideoFrame::kAPlane),
          static_cast<uint8*>(rgb_pixels),
          row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
    // TODO(fbarchard): Implement I420AlphaToABGR and remove swizzle below.
#if SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
      libyuv::ARGBToABGR(static_cast<uint8*>(rgb_pixels),
                         row_bytes,
                         static_cast<uint8*>(rgb_pixels),
                         row_bytes,
                         video_frame->visible_rect().width(),
                         video_frame->visible_rect().height());
#endif
      break;

    case PIXEL_FORMAT_YV24:
      libyuv::I444ToARGB(
          video_frame->data(VideoFrame::kYPlane) + y_offset,
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->data(VideoFrame::kUPlane) + uv_offset,
          video_frame->stride(VideoFrame::kUPlane),
          video_frame->data(VideoFrame::kVPlane) + uv_offset,
          video_frame->stride(VideoFrame::kVPlane),
          static_cast<uint8*>(rgb_pixels),
          row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height());
#if SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
      libyuv::ARGBToABGR(static_cast<uint8*>(rgb_pixels),
                         row_bytes,
                         static_cast<uint8*>(rgb_pixels),
                         row_bytes,
                         video_frame->visible_rect().width(),
                         video_frame->visible_rect().height());
#endif
      break;
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_UNKNOWN:
      NOTREACHED();
  }
}

// static
void SkCanvasVideoRenderer::CopyVideoFrameSingleTextureToGLTexture(
    gpu::gles2::GLES2Interface* gl,
    VideoFrame* video_frame,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(video_frame);
  DCHECK(video_frame->HasTextures());
  DCHECK_EQ(1u, VideoFrame::NumPlanes(video_frame->format()));

  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
         mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB ||
         mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES)
      << mailbox_holder.texture_target;

  gl->WaitSyncPointCHROMIUM(mailbox_holder.sync_point);
  uint32 source_texture = gl->CreateAndConsumeTextureCHROMIUM(
      mailbox_holder.texture_target, mailbox_holder.mailbox.name);

  // The video is stored in a unmultiplied format, so premultiply
  // if necessary.
  // Application itself needs to take care of setting the right |flip_y|
  // value down to get the expected result.
  // "flip_y == true" means to reverse the video orientation while
  // "flip_y == false" means to keep the intrinsic orientation.
  gl->CopyTextureCHROMIUM(GL_TEXTURE_2D, source_texture, texture,
                          internal_format, type,
                          flip_y, premultiply_alpha, false);

  gl->DeleteTextures(1, &source_texture);
  gl->Flush();

  SyncPointClientImpl client(gl);
  video_frame->UpdateReleaseSyncPoint(&client);
}

void SkCanvasVideoRenderer::ResetCache() {
  // Clear cached values.
  last_image_ = nullptr;
  last_timestamp_ = kNoTimestamp();
}

}  // namespace media
