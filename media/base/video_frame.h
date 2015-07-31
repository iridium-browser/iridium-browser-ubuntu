// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_H_
#define MEDIA_BASE_VIDEO_FRAME_H_

#include <vector>

#include "base/callback.h"
#include "base/md5.h"
#include "base/memory/shared_memory.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/buffers.h"
#include "media/base/video_frame_metadata.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_MACOSX)
#include <CoreVideo/CVPixelBuffer.h>
#include "base/mac/scoped_cftyperef.h"
#endif

namespace media {

class MEDIA_EXPORT VideoFrame : public base::RefCountedThreadSafe<VideoFrame> {
 public:
  enum {
    kFrameSizeAlignment = 16,
    kFrameSizePadding = 16,
    kFrameAddressAlignment = 32
  };

  enum {
    kMaxPlanes = 4,

    kYPlane = 0,
    kARGBPlane = kYPlane,
    kUPlane = 1,
    kUVPlane = kUPlane,
    kVPlane = 2,
    kAPlane = 3,
  };

  // Surface formats roughly based on FOURCC labels, see:
  // http://www.fourcc.org/rgb.php
  // http://www.fourcc.org/yuv.php
  // Logged to UMA, so never reuse values.
  enum Format {
    UNKNOWN = 0,  // Unknown format value.
    YV12 = 1,     // 12bpp YVU planar 1x1 Y, 2x2 VU samples
    YV16 = 2,     // 16bpp YVU planar 1x1 Y, 2x1 VU samples
    I420 = 3,     // 12bpp YVU planar 1x1 Y, 2x2 UV samples.
    YV12A = 4,    // 20bpp YUVA planar 1x1 Y, 2x2 VU, 1x1 A samples.
#if defined(VIDEO_HOLE)
    HOLE = 5,            // Hole frame.
#endif  // defined(VIDEO_HOLE)
    NATIVE_TEXTURE = 6,  // Native texture.  Pixel-format agnostic.
    YV12J = 7,  // JPEG color range version of YV12
    NV12 = 8,  // 12bpp 1x1 Y plane followed by an interleaved 2x2 UV plane.
    YV24 = 9,  // 24bpp YUV planar, no subsampling.
    ARGB = 10,  // 32bpp ARGB, 1 plane.
    YV12HD = 11,  // Rec709 "HD" color space version of YV12
    // Please update UMA histogram enumeration when adding new formats here.
    FORMAT_MAX = YV12HD,  // Must always be equal to largest entry logged.
  };

  // Defines the internal format and the number of the textures in the mailbox
  // holders.
  enum TextureFormat {
    TEXTURE_RGBA,     // One RGBA texture.
    TEXTURE_RGB,      // One RGB texture.
    TEXTURE_YUV_420,  // 3 RED textures one per channel. UV are 2x2 subsampled.
  };

  // Returns the name of a Format as a string.
  static std::string FormatToString(Format format);

  // Creates a new frame in system memory with given parameters. Buffers for
  // the frame are allocated but not initialized.
  static scoped_refptr<VideoFrame> CreateFrame(
      Format format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp);

  // Returns true if |plane| is a valid plane number for the given format. This
  // can be used to DCHECK() plane parameters.
  static bool IsValidPlane(size_t plane, VideoFrame::Format format);

  // Call prior to CreateFrame to ensure validity of frame configuration. Called
  // automatically by VideoDecoderConfig::IsValidConfig().
  // TODO(scherkus): VideoDecoderConfig shouldn't call this method
  static bool IsValidConfig(Format format,
                            const gfx::Size& coded_size,
                            const gfx::Rect& visible_rect,
                            const gfx::Size& natural_size);

  // CB to be called on the mailbox backing this frame when the frame is
  // destroyed.
  typedef base::Callback<void(uint32)> ReleaseMailboxCB;

  // Wraps a native texture of the given parameters with a VideoFrame.
  // The backing of the VideoFrame is held in the mailbox held by
  // |mailbox_holder|, and |mailbox_holder_release_cb| will be called with
  // a syncpoint as the argument when the VideoFrame is to be destroyed.
  static scoped_refptr<VideoFrame> WrapNativeTexture(
      const gpu::MailboxHolder& mailbox_holder,
      const ReleaseMailboxCB& mailbox_holder_release_cb,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp,
      bool allow_overlay,
      bool has_alpha);

  // Wraps a set of native textures representing YUV data with a VideoFrame.
  // |mailbox_holders_release_cb| will be called with a syncpoint as the
  // argument when the VideoFrame is to be destroyed.
  static scoped_refptr<VideoFrame> WrapYUV420NativeTextures(
      const gpu::MailboxHolder& y_mailbox_holder,
      const gpu::MailboxHolder& u_mailbox_holder,
      const gpu::MailboxHolder& v_mailbox_holder,
      const ReleaseMailboxCB& mailbox_holders_release_cb,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      base::TimeDelta timestamp,
      bool allow_overlay);

  // Wraps packed image data residing in a memory buffer with a VideoFrame.
  // The image data resides in |data| and is assumed to be packed tightly in a
  // buffer of logical dimensions |coded_size| with the appropriate bit depth
  // and plane count as given by |format|.  The shared memory handle of the
  // backing allocation, if present, can be passed in with |handle|.  When the
  // frame is destroyed, |no_longer_needed_cb.Run()| will be called.
  // Returns NULL on failure.
  static scoped_refptr<VideoFrame> WrapExternalPackedMemory(
      Format format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      uint8* data,
      size_t data_size,
      base::SharedMemoryHandle handle,
      size_t shared_memory_offset,
      base::TimeDelta timestamp,
      const base::Closure& no_longer_needed_cb);

  // Wraps external YUV data of the given parameters with a VideoFrame.
  // The returned VideoFrame does not own the data passed in. When the frame
  // is destroyed |no_longer_needed_cb.Run()| will be called.
  static scoped_refptr<VideoFrame> WrapExternalYuvData(
      Format format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      int32 y_stride,
      int32 u_stride,
      int32 v_stride,
      uint8* y_data,
      uint8* u_data,
      uint8* v_data,
      base::TimeDelta timestamp,
      const base::Closure& no_longer_needed_cb);

#if defined(OS_POSIX)
  // Wraps provided dmabufs
  // (https://www.kernel.org/doc/Documentation/dma-buf-sharing.txt) with a
  // VideoFrame. The dmabuf fds are dup()ed on creation, so that the VideoFrame
  // retains a reference to them, and are automatically close()d on destruction,
  // dropping the reference. The caller may safely close() its reference after
  // calling WrapExternalDmabufs().
  // The image data is only accessible via dmabuf fds, which are usually passed
  // directly to a hardware device and/or to another process, or can also be
  // mapped via mmap() for CPU access.
  // When the frame is destroyed, |no_longer_needed_cb.Run()| will be called.
  // Returns NULL on failure.
  static scoped_refptr<VideoFrame> WrapExternalDmabufs(
      Format format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      const std::vector<int> dmabuf_fds,
      base::TimeDelta timestamp,
      const base::Closure& no_longer_needed_cb);
#endif

#if defined(OS_MACOSX)
  // Wraps a provided CVPixelBuffer with a VideoFrame. The pixel buffer is
  // retained for the lifetime of the VideoFrame and released upon destruction.
  // The image data is only accessible via the pixel buffer, which could be
  // backed by an IOSurface from another process. All the attributes of the
  // VideoFrame are derived from the pixel buffer, with the exception of the
  // timestamp. If information is missing or is incompatible (for example, a
  // pixel format that has no VideoFrame match), NULL is returned.
  // http://crbug.com/401308
  static scoped_refptr<VideoFrame> WrapCVPixelBuffer(
      CVPixelBufferRef cv_pixel_buffer,
      base::TimeDelta timestamp);
#endif

  // Wraps |frame| and calls |no_longer_needed_cb| when the wrapper VideoFrame
  // gets destroyed. |visible_rect| must be a sub rect within
  // frame->visible_rect().
  static scoped_refptr<VideoFrame> WrapVideoFrame(
      const scoped_refptr<VideoFrame>& frame,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      const base::Closure& no_longer_needed_cb);

  // Creates a frame which indicates end-of-stream.
  static scoped_refptr<VideoFrame> CreateEOSFrame();

  // Allocates YV12 frame based on |size|, and sets its data to the YUV(y,u,v).
  static scoped_refptr<VideoFrame> CreateColorFrame(
      const gfx::Size& size,
      uint8 y, uint8 u, uint8 v,
      base::TimeDelta timestamp);

  // Allocates YV12 frame based on |size|, and sets its data to the YUV
  // equivalent of RGB(0,0,0).
  static scoped_refptr<VideoFrame> CreateBlackFrame(const gfx::Size& size);

  // Allocates YV12A frame based on |size|, and sets its data to the YUVA
  // equivalent of RGBA(0,0,0,0).
  static scoped_refptr<VideoFrame> CreateTransparentFrame(
      const gfx::Size& size);

#if defined(VIDEO_HOLE)
  // Allocates a hole frame.
  static scoped_refptr<VideoFrame> CreateHoleFrame(const gfx::Size& size);
#endif  // defined(VIDEO_HOLE)

  static size_t NumPlanes(Format format);

  static size_t NumTextures(TextureFormat texture_format);

  // Returns the required allocation size for a (tightly packed) frame of the
  // given coded size and format.
  static size_t AllocationSize(Format format, const gfx::Size& coded_size);

  // Returns the plane size (in bytes) for a plane of the given coded size and
  // format.
  static gfx::Size PlaneSize(Format format,
                             size_t plane,
                             const gfx::Size& coded_size);

  // Returns the required allocation size for a (tightly packed) plane of the
  // given coded size and format.
  static size_t PlaneAllocationSize(Format format,
                                    size_t plane,
                                    const gfx::Size& coded_size);

  // Returns horizontal bits per pixel for given |plane| and |format|.
  static int PlaneHorizontalBitsPerPixel(Format format, size_t plane);

  // Returns bits per pixel for given |plane| and |format|.
  static int PlaneBitsPerPixel(Format format, size_t plane);

  // Returns the number of bytes per row for the given plane, format, and width.
  // The width may be aligned to format requirements.
  static size_t RowBytes(size_t plane, Format format, int width);

  // Returns the number of rows for the given plane, format, and height.
  // The height may be aligned to format requirements.
  static size_t Rows(size_t plane, Format format, int height);

  // Returns the number of columns for the given plane, format, and width.
  // The width may be aligned to format requirements.
  static size_t Columns(size_t plane, Format format, int width);

  Format format() const { return format_; }

  TextureFormat texture_format() const { return texture_format_; }

  const gfx::Size& coded_size() const { return coded_size_; }
  const gfx::Rect& visible_rect() const { return visible_rect_; }
  const gfx::Size& natural_size() const { return natural_size_; }

  int stride(size_t plane) const;

  // Returns the number of bytes per row and number of rows for a given plane.
  //
  // As opposed to stride(), row_bytes() refers to the bytes representing
  // frame data scanlines (coded_size.width() pixels, without stride padding).
  int row_bytes(size_t plane) const;
  int rows(size_t plane) const;

  // Returns pointer to the buffer for a given plane. The memory is owned by
  // VideoFrame object and must not be freed by the caller.
  const uint8* data(size_t plane) const;
  uint8* data(size_t plane);

  // Returns pointer to the data in the visible region of the frame. I.e. the
  // returned pointer is offsetted into the plane buffer specified by
  // visible_rect().origin(). Memory is owned by VideoFrame object and must not
  // be freed by the caller.
  const uint8* visible_data(size_t plane) const;
  uint8* visible_data(size_t plane);

  // Returns a mailbox holder for a given texture.
  // Only valid to call if this is a NATIVE_TEXTURE frame. Before using the
  // mailbox, the caller must wait for the included sync point.
  const gpu::MailboxHolder& mailbox_holder(size_t texture) const;

  // Returns the shared-memory handle, if present
  base::SharedMemoryHandle shared_memory_handle() const;

  // Returns the offset into the shared memory where the frame data begins.
  size_t shared_memory_offset() const;

  // Returns a dictionary of optional metadata.  This contains information
  // associated with the frame that downstream clients might use for frame-level
  // logging, quality/performance optimizations, signaling, etc.
  //
  // TODO(miu): Move some of the "extra" members of VideoFrame (below) into
  // here as a later clean-up step.
  const VideoFrameMetadata* metadata() const { return &metadata_; }
  VideoFrameMetadata* metadata() { return &metadata_; }

  bool allow_overlay() const { return allow_overlay_; }

#if defined(OS_POSIX)
  // Returns backing dmabuf file descriptor for given |plane|, if present.
  int dmabuf_fd(size_t plane) const;
#endif

#if defined(OS_MACOSX)
  // Returns the backing CVPixelBuffer, if present.
  CVPixelBufferRef cv_pixel_buffer() const;
#endif

  // Returns true if this VideoFrame represents the end of the stream.
  bool end_of_stream() const { return end_of_stream_; }

  base::TimeDelta timestamp() const {
    return timestamp_;
  }
  void set_timestamp(const base::TimeDelta& timestamp) {
    timestamp_ = timestamp;
  }

  class SyncPointClient {
   public:
    SyncPointClient() {}
    virtual uint32 InsertSyncPoint() = 0;
    virtual void WaitSyncPoint(uint32 sync_point) = 0;

   protected:
    virtual ~SyncPointClient() {}

    DISALLOW_COPY_AND_ASSIGN(SyncPointClient);
  };
  // It uses |client| to insert a new sync point and potentially waits on a
  // older sync point. The final sync point will be used to release this
  // VideoFrame.
  // This method is thread safe. Both blink and compositor threads can call it.
  void UpdateReleaseSyncPoint(SyncPointClient* client);

  // Used to keep a running hash of seen frames.  Expects an initialized MD5
  // context.  Calls MD5Update with the context and the contents of the frame.
  void HashFrameForTesting(base::MD5Context* context);

 private:
  friend class base::RefCountedThreadSafe<VideoFrame>;

  // Clients must use the static CreateFrame() method to create a new frame.
  VideoFrame(Format format,
             const gfx::Size& coded_size,
             const gfx::Rect& visible_rect,
             const gfx::Size& natural_size,
             const gpu::MailboxHolder(&mailbox_holders)[kMaxPlanes],
             TextureFormat texture_format,
             base::TimeDelta timestamp,
             bool end_of_stream);
  virtual ~VideoFrame();

  void AllocateYUV();

  // Frame format.
  const Format format_;

  // Format of the native textures associated with this frame.
  const TextureFormat texture_format_;

  // Width and height of the video frame, in pixels. This must include pixel
  // data for the whole image; i.e. for YUV formats with subsampled chroma
  // planes, in the case that the visible portion of the image does not line up
  // on a sample boundary, |coded_size_| must be rounded up appropriately and
  // the pixel data provided for the odd pixels.
  const gfx::Size coded_size_;

  // Width, height, and offsets of the visible portion of the video frame. Must
  // be a subrect of |coded_size_|. Can be odd with respect to the sample
  // boundaries, e.g. for formats with subsampled chroma.
  const gfx::Rect visible_rect_;

  // Width and height of the visible portion of the video frame
  // (|visible_rect_.size()|) with aspect ratio taken into account.
  const gfx::Size natural_size_;

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.  Note that
  // strides can be negative.
  int32 strides_[kMaxPlanes];

  // Array of data pointers to each plane.
  uint8* data_[kMaxPlanes];

  // Native texture mailboxes, if this is a NATIVE_TEXTURE frame.
  gpu::MailboxHolder mailbox_holders_[kMaxPlanes];
  ReleaseMailboxCB mailbox_holders_release_cb_;

  // Shared memory handle, if this frame was allocated from shared memory.
  base::SharedMemoryHandle shared_memory_handle_;

  // Offset in shared memory buffer.
  size_t shared_memory_offset_;

#if defined(OS_POSIX)
  // Dmabufs for each plane, if this frame is wrapping memory
  // acquired via dmabuf.
  base::ScopedFD dmabuf_fds_[kMaxPlanes];
#endif

#if defined(OS_MACOSX)
  // CVPixelBuffer, if this frame is wrapping one.
  base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer_;
#endif

  base::Closure no_longer_needed_cb_;

  base::TimeDelta timestamp_;

  base::Lock release_sync_point_lock_;
  uint32 release_sync_point_;

  const bool end_of_stream_;

  VideoFrameMetadata metadata_;

  bool allow_overlay_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoFrame);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_H_
