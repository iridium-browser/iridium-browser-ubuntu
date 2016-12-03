// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/skcanvas_video_renderer.h"

#include <limits>

#include "base/macros.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "skia/ext/texture_handle.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrPaint.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "third_party/skia/include/gpu/GrTextureProvider.h"
#include "third_party/skia/include/gpu/SkGr.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/skia_util.h"

// Skia internal format depends on a platform. On Android it is ABGR, on others
// it is ARGB.
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToARGB
#define LIBYUV_I422_TO_ARGB libyuv::I422ToARGB
#define LIBYUV_I444_TO_ARGB libyuv::I444ToARGB
#define LIBYUV_I420ALPHA_TO_ARGB libyuv::I420AlphaToARGB
#define LIBYUV_J420_TO_ARGB libyuv::J420ToARGB
#define LIBYUV_H420_TO_ARGB libyuv::H420ToARGB
#elif SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToABGR
#define LIBYUV_I422_TO_ARGB libyuv::I422ToABGR
#define LIBYUV_I444_TO_ARGB libyuv::I444ToABGR
#define LIBYUV_I420ALPHA_TO_ARGB libyuv::I420AlphaToABGR
#define LIBYUV_J420_TO_ARGB libyuv::J420ToABGR
#define LIBYUV_H420_TO_ARGB libyuv::H420ToABGR
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
  return video_frame->metadata()->GetInteger(VideoFrameMetadata::COLOR_SPACE,
                                             &result) &&
         result == color_space;
}

class SyncTokenClientImpl : public VideoFrame::SyncTokenClient {
 public:
  explicit SyncTokenClientImpl(gpu::gles2::GLES2Interface* gl) : gl_(gl) {}
  ~SyncTokenClientImpl() override {}
  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    const uint64_t fence_sync = gl_->InsertFenceSyncCHROMIUM();
    gl_->ShallowFlushCHROMIUM();
    gl_->GenSyncTokenCHROMIUM(fence_sync, sync_token->GetData());
  }
  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }

 private:
  gpu::gles2::GLES2Interface* gl_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncTokenClientImpl);
};

sk_sp<SkImage> NewSkImageFromVideoFrameYUVTextures(
    const VideoFrame* video_frame,
    const Context3D& context_3d) {
  DCHECK(video_frame->HasTextures());
  DCHECK(video_frame->format() == PIXEL_FORMAT_I420 ||
         video_frame->format() == PIXEL_FORMAT_NV12);

  gpu::gles2::GLES2Interface* gl = context_3d.gl;
  DCHECK(gl);
  gfx::Size ya_tex_size = video_frame->coded_size();
  gfx::Size uv_tex_size((ya_tex_size.width() + 1) / 2,
                        (ya_tex_size.height() + 1) / 2);

  GrGLTextureInfo source_textures[] = {{0, 0}, {0, 0}, {0, 0}};
  for (size_t i = 0; i < media::VideoFrame::NumPlanes(video_frame->format());
       ++i) {
    // Get the texture from the mailbox and wrap it in a GrTexture.
    const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(i);
    DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
           mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES ||
           mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB);
    gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
    source_textures[i].fID = gl->CreateAndConsumeTextureCHROMIUM(
        mailbox_holder.texture_target, mailbox_holder.mailbox.name);
    source_textures[i].fTarget = mailbox_holder.texture_target;

    // TODO(dcastagna): avoid this copy once Skia supports native textures
    // with a GL_TEXTURE_RECTANGLE_ARB texture target.
    // crbug.com/505026
    if (mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB) {
      unsigned texture_copy = 0;
      gl->GenTextures(1, &texture_copy);
      DCHECK(texture_copy);
      gl->BindTexture(GL_TEXTURE_2D, texture_copy);
      gl->CopyTextureCHROMIUM(source_textures[i].fID, texture_copy, GL_RGB,
                              GL_UNSIGNED_BYTE, false, true, false);

      gl->DeleteTextures(1, &source_textures[i].fID);
      source_textures[i].fID = texture_copy;
      source_textures[i].fTarget = GL_TEXTURE_2D;
    }
  }
  GrBackendObject handles[3] = {
      skia::GrGLTextureInfoToGrBackendObject(source_textures[0]),
      skia::GrGLTextureInfoToGrBackendObject(source_textures[1]),
      skia::GrGLTextureInfoToGrBackendObject(source_textures[2])};

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

  sk_sp<SkImage> img;
  if (video_frame->format() == PIXEL_FORMAT_NV12) {
    img = SkImage::MakeFromNV12TexturesCopy(context_3d.gr_context, color_space,
                                            handles, yuvSizes,
                                            kTopLeft_GrSurfaceOrigin);
  } else {
    img = SkImage::MakeFromYUVTexturesCopy(context_3d.gr_context, color_space,
                                           handles, yuvSizes,
                                           kTopLeft_GrSurfaceOrigin);
  }
  for (size_t i = 0; i < media::VideoFrame::NumPlanes(video_frame->format());
       ++i) {
    gl->DeleteTextures(1, &source_textures[i].fID);
  }
  return img;
}

// Creates a SkImage from a |video_frame| backed by native resources.
// The SkImage will take ownership of the underlying resource.
sk_sp<SkImage> NewSkImageFromVideoFrameNative(VideoFrame* video_frame,
                                              const Context3D& context_3d) {
  DCHECK(PIXEL_FORMAT_ARGB == video_frame->format() ||
         PIXEL_FORMAT_XRGB == video_frame->format() ||
         PIXEL_FORMAT_NV12 == video_frame->format() ||
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
    gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
    source_texture = gl->CreateAndConsumeTextureCHROMIUM(
        mailbox_holder.texture_target, mailbox_holder.mailbox.name);
  }
  GrBackendTextureDesc desc;
  desc.fFlags = kRenderTarget_GrBackendTextureFlag;
  desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  desc.fWidth = video_frame->coded_size().width();
  desc.fHeight = video_frame->coded_size().height();
  desc.fConfig = kRGBA_8888_GrPixelConfig;
  GrGLTextureInfo source_texture_info;
  source_texture_info.fID = source_texture;
  source_texture_info.fTarget = GL_TEXTURE_2D;
  desc.fTextureHandle =
      skia::GrGLTextureInfoToGrBackendObject(source_texture_info);
  return SkImage::MakeFromAdoptedTexture(context_3d.gr_context, desc);
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

  bool onQueryYUV8(SkYUVSizeInfo* sizeInfo,
                   SkYUVColorSpace* color_space) const override {
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
      const gfx::Size size = VideoFrame::PlaneSize(
          frame_->format(), plane, gfx::Size(frame_->visible_rect().width(),
                                             frame_->visible_rect().height()));
      sizeInfo->fSizes[plane].set(size.width(), size.height());
      sizeInfo->fWidthBytes[plane] = size.width();
    }

    return true;
  }

  bool onGetYUV8Planes(const SkYUVSizeInfo& sizeInfo,
                       void* planes[3]) override {
    media::VideoPixelFormat format = frame_->format();
    DCHECK(media::IsYuvPlanar(format) && format != PIXEL_FORMAT_YV12A);

    for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane;
         ++plane) {
      const gfx::Size size = VideoFrame::PlaneSize(
          frame_->format(), plane, gfx::Size(frame_->visible_rect().width(),
                                             frame_->visible_rect().height()));
      if (size.width() != sizeInfo.fSizes[plane].width() ||
          size.height() != sizeInfo.fSizes[plane].height()) {
        return false;
      }

      size_t offset;
      const int y_shift =
          (frame_->format() == media::PIXEL_FORMAT_YV16) ? 0 : 1;
      if (plane == VideoFrame::kYPlane) {
        offset =
            (frame_->stride(VideoFrame::kYPlane) * frame_->visible_rect().y()) +
            frame_->visible_rect().x();
      } else {
        offset = (frame_->stride(VideoFrame::kUPlane) *
                  (frame_->visible_rect().y() >> y_shift)) +
                 (frame_->visible_rect().x() >> 1);
      }

      // Copy the frame to the supplied memory.
      // TODO: Find a way (API change?) to avoid this copy.
      char* out_line = static_cast<char*>(planes[plane]);
      int out_line_stride = sizeInfo.fWidthBytes[plane];
      uint8_t* in_line = frame_->data(plane) + offset;
      int in_line_stride = frame_->stride(plane);
      int plane_height = sizeInfo.fSizes[plane].height();
      if (in_line_stride == out_line_stride) {
        memcpy(out_line, in_line, plane_height * in_line_stride);
      } else {
        // Different line padding so need to copy one line at a time.
        int bytes_to_copy_per_line =
            out_line_stride < in_line_stride ? out_line_stride : in_line_stride;
        for (int line_no = 0; line_no < plane_height; line_no++) {
          memcpy(out_line, in_line, bytes_to_copy_per_line);
          in_line += in_line_stride;
          out_line += out_line_stride;
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
                                  uint8_t alpha,
                                  SkXfermode::Mode mode,
                                  VideoRotation video_rotation,
                                  const Context3D& context_3d) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  if (!UpdateLastImage(video_frame, context_3d))
    return;

  paint.setXfermodeMode(mode);
  paint.setFilterQuality(kLow_SkFilterQuality);

  const bool need_rotation = video_rotation != VIDEO_ROTATION_0;
  const bool need_scaling =
      dest_rect.size() !=
      gfx::SizeF(gfx::SkISizeToSize(last_image_->dimensions()));
  const bool need_translation = !dest_rect.origin().IsOrigin();
  bool need_transform = need_rotation || need_scaling || need_translation;
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

  // This is a workaround for crbug.com/524717. A texture backed image is not
  // safe to access on another thread or GL context. So if we're drawing into a
  // recording canvas we read the texture back into CPU memory and record that
  // sw image into the SkPicture. The long term solution is for Skia to provide
  // a SkPicture filter that makes a picture safe for multiple CPU raster
  // threads. (skbug.com/4321).
  if (canvas->imageInfo().colorType() == kUnknown_SkColorType) {
    sk_sp<SkImage> swImage = last_image_->makeNonTextureImage();
    canvas->drawImage(swImage, 0, 0, &paint);
  } else {
    canvas->drawImage(last_image_.get(), 0, 0, &paint);
  }

  if (need_transform)
    canvas->restore();
  // Make sure to flush so we can remove the videoframe from the generator.
  canvas->flush();

  if (video_frame->HasTextures()) {
    DCHECK(gl);
    SyncTokenClientImpl client(gl);
    video_frame->UpdateReleaseSyncToken(&client);
  }
}

void SkCanvasVideoRenderer::Copy(const scoped_refptr<VideoFrame>& video_frame,
                                 SkCanvas* canvas,
                                 const Context3D& context_3d) {
  Paint(video_frame, canvas, gfx::RectF(video_frame->visible_rect()), 0xff,
        SkXfermode::kSrc_Mode, media::VIDEO_ROTATION_0, context_3d);
}

namespace {

// libyuv doesn't support 9- and 10-bit video frames yet. This function
// creates a regular 8-bit video frame which we can give to libyuv.
scoped_refptr<VideoFrame> DownShiftHighbitVideoFrame(
    const VideoFrame* video_frame) {
  VideoPixelFormat format;
  int shift = 1;
  switch (video_frame->format()) {
    case PIXEL_FORMAT_YUV420P10:
      shift = 2;
    case PIXEL_FORMAT_YUV420P9:
      format = PIXEL_FORMAT_I420;
      break;
    case PIXEL_FORMAT_YUV422P10:
      shift = 2;
    case PIXEL_FORMAT_YUV422P9:
      format = PIXEL_FORMAT_YV16;
      break;
    case PIXEL_FORMAT_YUV444P10:
      shift = 2;
    case PIXEL_FORMAT_YUV444P9:
      format = PIXEL_FORMAT_YV24;
      break;

    default:
      NOTREACHED();
      return nullptr;
  }
  scoped_refptr<VideoFrame> ret = VideoFrame::CreateFrame(
      format, video_frame->coded_size(), video_frame->visible_rect(),
      video_frame->natural_size(), video_frame->timestamp());

  // Copy all metadata.
  // (May be enough to copy color space)
  base::DictionaryValue tmp;
  video_frame->metadata()->MergeInternalValuesInto(&tmp);
  ret->metadata()->MergeInternalValuesFrom(tmp);

  for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane; ++plane) {
    int width = ret->row_bytes(plane);
    const uint16_t* src =
        reinterpret_cast<const uint16_t*>(video_frame->data(plane));
    uint8_t* dst = ret->data(plane);
    for (int row = 0; row < video_frame->rows(plane); row++) {
      for (int x = 0; x < width; x++) {
        dst[x] = src[x] >> shift;
      }
      src += video_frame->stride(plane) / 2;
      dst += ret->stride(plane);
    }
  }
  return ret;
}
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

  switch (video_frame->format()) {
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
      if (CheckColorSpace(video_frame, COLOR_SPACE_JPEG)) {
        LIBYUV_J420_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                            video_frame->stride(VideoFrame::kYPlane),
                            video_frame->visible_data(VideoFrame::kUPlane),
                            video_frame->stride(VideoFrame::kUPlane),
                            video_frame->visible_data(VideoFrame::kVPlane),
                            video_frame->stride(VideoFrame::kVPlane),
                            static_cast<uint8_t*>(rgb_pixels), row_bytes,
                            video_frame->visible_rect().width(),
                            video_frame->visible_rect().height());
      } else if (CheckColorSpace(video_frame, COLOR_SPACE_HD_REC709)) {
        LIBYUV_H420_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                            video_frame->stride(VideoFrame::kYPlane),
                            video_frame->visible_data(VideoFrame::kUPlane),
                            video_frame->stride(VideoFrame::kUPlane),
                            video_frame->visible_data(VideoFrame::kVPlane),
                            video_frame->stride(VideoFrame::kVPlane),
                            static_cast<uint8_t*>(rgb_pixels), row_bytes,
                            video_frame->visible_rect().width(),
                            video_frame->visible_rect().height());
      } else {
        LIBYUV_I420_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                            video_frame->stride(VideoFrame::kYPlane),
                            video_frame->visible_data(VideoFrame::kUPlane),
                            video_frame->stride(VideoFrame::kUPlane),
                            video_frame->visible_data(VideoFrame::kVPlane),
                            video_frame->stride(VideoFrame::kVPlane),
                            static_cast<uint8_t*>(rgb_pixels), row_bytes,
                            video_frame->visible_rect().width(),
                            video_frame->visible_rect().height());
      }
      break;
    case PIXEL_FORMAT_YV16:
      LIBYUV_I422_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                          video_frame->stride(VideoFrame::kYPlane),
                          video_frame->visible_data(VideoFrame::kUPlane),
                          video_frame->stride(VideoFrame::kUPlane),
                          video_frame->visible_data(VideoFrame::kVPlane),
                          video_frame->stride(VideoFrame::kVPlane),
                          static_cast<uint8_t*>(rgb_pixels), row_bytes,
                          video_frame->visible_rect().width(),
                          video_frame->visible_rect().height());
      break;

    case PIXEL_FORMAT_YV12A:
      LIBYUV_I420ALPHA_TO_ARGB(
          video_frame->visible_data(VideoFrame::kYPlane),
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->visible_data(VideoFrame::kUPlane),
          video_frame->stride(VideoFrame::kUPlane),
          video_frame->visible_data(VideoFrame::kVPlane),
          video_frame->stride(VideoFrame::kVPlane),
          video_frame->visible_data(VideoFrame::kAPlane),
          video_frame->stride(VideoFrame::kAPlane),
          static_cast<uint8_t*>(rgb_pixels), row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          1);  // 1 = enable RGB premultiplication by Alpha.
      break;

    case PIXEL_FORMAT_YV24:
      LIBYUV_I444_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                          video_frame->stride(VideoFrame::kYPlane),
                          video_frame->visible_data(VideoFrame::kUPlane),
                          video_frame->stride(VideoFrame::kUPlane),
                          video_frame->visible_data(VideoFrame::kVPlane),
                          video_frame->stride(VideoFrame::kVPlane),
                          static_cast<uint8_t*>(rgb_pixels), row_bytes,
                          video_frame->visible_rect().width(),
                          video_frame->visible_rect().height());
      break;

    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10: {
      scoped_refptr<VideoFrame> temporary_frame =
          DownShiftHighbitVideoFrame(video_frame);
      ConvertVideoFrameToRGBPixels(temporary_frame.get(), rgb_pixels,
                                   row_bytes);
      break;
    }

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB32:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_MT21:
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

  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
         mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB ||
         mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES)
      << mailbox_holder.texture_target;

  gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
  uint32_t source_texture = gl->CreateAndConsumeTextureCHROMIUM(
      mailbox_holder.texture_target, mailbox_holder.mailbox.name);

  // The video is stored in a unmultiplied format, so premultiply
  // if necessary.
  // Application itself needs to take care of setting the right |flip_y|
  // value down to get the expected result.
  // "flip_y == true" means to reverse the video orientation while
  // "flip_y == false" means to keep the intrinsic orientation.
  gl->CopyTextureCHROMIUM(source_texture, texture, internal_format, type,
                          flip_y, premultiply_alpha, false);
  gl->DeleteTextures(1, &source_texture);
  gl->Flush();

  SyncTokenClientImpl client(gl);
  video_frame->UpdateReleaseSyncToken(&client);
}

bool SkCanvasVideoRenderer::CopyVideoFrameTexturesToGLTexture(
    const Context3D& context_3d,
    gpu::gles2::GLES2Interface* destination_gl,
    const scoped_refptr<VideoFrame>& video_frame,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(video_frame);
  DCHECK(video_frame->HasTextures());
  if (media::VideoFrame::NumPlanes(video_frame->format()) > 1) {
    if (!context_3d.gr_context)
      return false;
    if (!UpdateLastImage(video_frame, context_3d))
      return false;

    const GrGLTextureInfo* texture_info =
        skia::GrBackendObjectToGrGLTextureInfo(
            last_image_->getTextureHandle(true));

    gpu::gles2::GLES2Interface* canvas_gl = context_3d.gl;
    gpu::MailboxHolder mailbox_holder;
    mailbox_holder.texture_target = texture_info->fTarget;
    canvas_gl->GenMailboxCHROMIUM(mailbox_holder.mailbox.name);
    canvas_gl->ProduceTextureDirectCHROMIUM(texture_info->fID,
                                            mailbox_holder.texture_target,
                                            mailbox_holder.mailbox.name);

    // Wait for mailbox creation on canvas context before consuming it and
    // copying from it on the consumer context.
    const GLuint64 fence_sync = canvas_gl->InsertFenceSyncCHROMIUM();
    canvas_gl->ShallowFlushCHROMIUM();
    canvas_gl->GenSyncTokenCHROMIUM(fence_sync,
                                    mailbox_holder.sync_token.GetData());

    destination_gl->WaitSyncTokenCHROMIUM(
        mailbox_holder.sync_token.GetConstData());
    uint32_t intermediate_texture =
        destination_gl->CreateAndConsumeTextureCHROMIUM(
            mailbox_holder.texture_target, mailbox_holder.mailbox.name);

    destination_gl->CopyTextureCHROMIUM(intermediate_texture, texture,
                                        internal_format, type, flip_y,
                                        premultiply_alpha, false);
    destination_gl->DeleteTextures(1, &intermediate_texture);

    // Wait for destination context to consume mailbox before deleting it in
    // canvas context.
    const GLuint64 dest_fence_sync = destination_gl->InsertFenceSyncCHROMIUM();
    destination_gl->ShallowFlushCHROMIUM();
    gpu::SyncToken dest_sync_token;
    destination_gl->GenSyncTokenCHROMIUM(dest_fence_sync,
                                         dest_sync_token.GetData());
    canvas_gl->WaitSyncTokenCHROMIUM(dest_sync_token.GetConstData());

    SyncTokenClientImpl client(canvas_gl);
    video_frame->UpdateReleaseSyncToken(&client);
  } else {
    CopyVideoFrameSingleTextureToGLTexture(destination_gl, video_frame.get(),
                                           texture, internal_format, type,
                                           premultiply_alpha, flip_y);
  }

  return true;
}

void SkCanvasVideoRenderer::ResetCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Clear cached values.
  last_image_ = nullptr;
  last_timestamp_ = kNoTimestamp;
}

bool SkCanvasVideoRenderer::UpdateLastImage(
    const scoped_refptr<VideoFrame>& video_frame,
    const Context3D& context_3d) {
  if (!last_image_ || video_frame->timestamp() != last_timestamp_) {
    ResetCache();
    // Generate a new image.
    // Note: Skia will hold onto |video_frame| via |video_generator| only when
    // |video_frame| is software.
    // Holding |video_frame| longer than this call when using GPUVideoDecoder
    // could cause problems since the pool of VideoFrames has a fixed size.
    if (video_frame->HasTextures()) {
      DCHECK(context_3d.gr_context);
      DCHECK(context_3d.gl);
      if (media::VideoFrame::NumPlanes(video_frame->format()) > 1) {
        last_image_ =
            NewSkImageFromVideoFrameYUVTextures(video_frame.get(), context_3d);
      } else {
        last_image_ =
            NewSkImageFromVideoFrameNative(video_frame.get(), context_3d);
      }
    } else {
      auto* video_generator = new VideoImageGenerator(video_frame);
      last_image_ = SkImage::MakeFromGenerator(video_generator);
    }
    if (!last_image_)  // Couldn't create the SkImage.
      return false;
    last_timestamp_ = video_frame->timestamp();
  }
  last_image_deleting_timer_.Reset();
  DCHECK(!!last_image_);
  return true;
}

}  // namespace media
