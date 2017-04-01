/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageDecoder_h
#define ImageDecoder_h

#include "SkColorPriv.h"
#include "SkColorSpaceXform.h"
#include "platform/PlatformExport.h"
#include "platform/SharedBuffer.h"
#include "platform/graphics/ColorBehavior.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/image-decoders/ImageAnimation.h"
#include "platform/image-decoders/ImageFrame.h"
#include "platform/image-decoders/SegmentReader.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

#if SK_B32_SHIFT
inline SkColorSpaceXform::ColorFormat xformColorFormat() {
  return SkColorSpaceXform::kRGBA_8888_ColorFormat;
}
#else
inline SkColorSpaceXform::ColorFormat xformColorFormat() {
  return SkColorSpaceXform::kBGRA_8888_ColorFormat;
}
#endif

// ImagePlanes can be used to decode color components into provided buffers
// instead of using an ImageFrame.
class PLATFORM_EXPORT ImagePlanes final {
  USING_FAST_MALLOC(ImagePlanes);
  WTF_MAKE_NONCOPYABLE(ImagePlanes);

 public:
  ImagePlanes();
  ImagePlanes(void* planes[3], const size_t rowBytes[3]);

  void* plane(int);
  size_t rowBytes(int) const;

 private:
  void* m_planes[3];
  size_t m_rowBytes[3];
};

// ImageDecoder is a base for all format-specific decoders
// (e.g. JPEGImageDecoder). This base manages the ImageFrame cache.
//
class PLATFORM_EXPORT ImageDecoder {
  WTF_MAKE_NONCOPYABLE(ImageDecoder);
  USING_FAST_MALLOC(ImageDecoder);

 public:
  static const size_t noDecodedImageByteLimit =
      Platform::noDecodedImageByteLimit;

  enum AlphaOption { AlphaPremultiplied, AlphaNotPremultiplied };

  virtual ~ImageDecoder() {}

  // Returns a caller-owned decoder of the appropriate type.  Returns nullptr if
  // we can't sniff a supported type from the provided data (possibly
  // because there isn't enough data yet).
  // Sets m_maxDecodedBytes to Platform::maxImageDecodedBytes().
  static std::unique_ptr<ImageDecoder> create(PassRefPtr<SegmentReader> data,
                                              bool dataComplete,
                                              AlphaOption,
                                              const ColorBehavior&);
  static std::unique_ptr<ImageDecoder> create(
      PassRefPtr<SharedBuffer> data,
      bool dataComplete,
      AlphaOption alphaoption,
      const ColorBehavior& colorBehavior) {
    return create(SegmentReader::createFromSharedBuffer(std::move(data)),
                  dataComplete, alphaoption, colorBehavior);
  }

  virtual String filenameExtension() const = 0;

  bool isAllDataReceived() const { return m_isAllDataReceived; }

  // Returns true if the buffer holds enough data to instantiate a decoder.
  // This is useful for callers to determine whether a decoder instantiation
  // failure is due to insufficient or bad data.
  static bool hasSufficientDataToSniffImageType(const SharedBuffer&);

  void setData(PassRefPtr<SegmentReader> data, bool allDataReceived) {
    if (m_failed)
      return;
    m_data = data;
    m_isAllDataReceived = allDataReceived;
    onSetData(m_data.get());
  }

  void setData(PassRefPtr<SharedBuffer> data, bool allDataReceived) {
    setData(SegmentReader::createFromSharedBuffer(std::move(data)),
            allDataReceived);
  }

  virtual void onSetData(SegmentReader* data) {}

  bool isSizeAvailable() {
    if (m_failed)
      return false;
    if (!m_sizeAvailable)
      decodeSize();
    return isDecodedSizeAvailable();
  }

  bool isDecodedSizeAvailable() const { return !m_failed && m_sizeAvailable; }

  virtual IntSize size() const { return m_size; }

  // Decoders which downsample images should override this method to
  // return the actual decoded size.
  virtual IntSize decodedSize() const { return size(); }

  // Image decoders that support YUV decoding must override this to
  // provide the size of each component.
  virtual IntSize decodedYUVSize(int component) const {
    ASSERT(false);
    return IntSize();
  }

  // Image decoders that support YUV decoding must override this to
  // return the width of each row of the memory allocation.
  virtual size_t decodedYUVWidthBytes(int component) const {
    ASSERT(false);
    return 0;
  }

  // This will only differ from size() for ICO (where each frame is a
  // different icon) or other formats where different frames are different
  // sizes. This does NOT differ from size() for GIF or WebP, since
  // decoding GIF or WebP composites any smaller frames against previous
  // frames to create full-size frames.
  virtual IntSize frameSizeAtIndex(size_t) const { return size(); }

  // Returns whether the size is legal (i.e. not going to result in
  // overflow elsewhere).  If not, marks decoding as failed.
  virtual bool setSize(unsigned width, unsigned height) {
    if (sizeCalculationMayOverflow(width, height))
      return setFailed();

    m_size = IntSize(width, height);
    m_sizeAvailable = true;
    return true;
  }

  // Calls decodeFrameCount() to get the frame count (if possible), without
  // decoding the individual frames.  Resizes m_frameBufferCache to the
  // correct size and returns its size.
  size_t frameCount();

  virtual int repetitionCount() const { return cAnimationNone; }

  // Decodes as much of the requested frame as possible, and returns an
  // ImageDecoder-owned pointer.
  ImageFrame* frameBufferAtIndex(size_t);

  // Whether the requested frame has alpha.
  virtual bool frameHasAlphaAtIndex(size_t) const;

  // Whether or not the frame is fully received.
  virtual bool frameIsCompleteAtIndex(size_t) const;

  // Duration for displaying a frame in seconds. This method is only used by
  // animated images.
  virtual float frameDurationAtIndex(size_t) const { return 0; }

  // Number of bytes in the decoded frame. Returns 0 if the decoder doesn't
  // have this frame cached (either because it hasn't been decoded, or because
  // it has been cleared).
  virtual size_t frameBytesAtIndex(size_t) const;

  ImageOrientation orientation() const { return m_orientation; }

  bool ignoresColorSpace() const { return m_colorBehavior.isIgnore(); }
  const ColorBehavior& colorBehavior() const { return m_colorBehavior; }

  // This returns the color space that will be included in the SkImageInfo of
  // SkImages created from this decoder. This will be nullptr unless the
  // decoder was created with the option ColorSpaceTagged.
  sk_sp<SkColorSpace> colorSpaceForSkImages() const;

  // This returns whether or not the image included a not-ignored embedded
  // color space. This is independent of whether or not that space's transform
  // has been baked into the pixel values.
  bool hasEmbeddedColorSpace() const { return m_embeddedColorSpace.get(); }

  // Set the embedded color space directly or via ICC profile.
  void setEmbeddedColorProfile(const char* iccData, unsigned iccLength);
  void setEmbeddedColorSpace(sk_sp<SkColorSpace> srcSpace);

  // Transformation from embedded color space to target color space.
  SkColorSpaceXform* colorTransform();

  // Sets the "decode failure" flag.  For caller convenience (since so
  // many callers want to return false after calling this), returns false
  // to enable easy tailcalling.  Subclasses may override this to also
  // clean up any local data.
  virtual bool setFailed() {
    m_failed = true;
    return false;
  }

  bool failed() const { return m_failed; }

  // Clears decoded pixel data from all frames except the provided frame. If
  // subsequent frames depend on this frame's required previous frame, then that
  // frame is also kept in cache to prevent re-decoding from the beginning.
  // Callers may pass WTF::kNotFound to clear all frames.
  // Note: If |m_frameBufferCache| contains only one frame, it won't be cleared.
  // Returns the number of bytes of frame data actually cleared.
  //
  // This is a virtual method because MockImageDecoder needs to override it in
  // order to run the test ImageFrameGeneratorTest::clearMultiFrameDecode.
  //
  // @TODO  Let MockImageDecoder override ImageFrame::clearFrameBuffer instead,
  //        so this method can be made non-virtual. It is used in the test
  //        ImageFrameGeneratorTest::clearMultiFrameDecode. The test needs to
  //        be modified since two frames may be kept in cache, instead of
  //        always just one, with this clearCacheExceptFrame implementation.
  virtual size_t clearCacheExceptFrame(size_t);

  // If the image has a cursor hot-spot, stores it in the argument
  // and returns true. Otherwise returns false.
  virtual bool hotSpot(IntPoint&) const { return false; }

  virtual void setMemoryAllocator(SkBitmap::Allocator* allocator) {
    // FIXME: this doesn't work for images with multiple frames.
    if (m_frameBufferCache.isEmpty()) {
      m_frameBufferCache.resize(1);
      m_frameBufferCache[0].setRequiredPreviousFrameIndex(
          findRequiredPreviousFrame(0, false));
    }
    m_frameBufferCache[0].setMemoryAllocator(allocator);
  }

  virtual bool canDecodeToYUV() { return false; }
  virtual bool decodeToYUV() { return false; }
  virtual void setImagePlanes(std::unique_ptr<ImagePlanes>) {}

 protected:
  ImageDecoder(AlphaOption alphaOption,
               const ColorBehavior& colorBehavior,
               size_t maxDecodedBytes)
      : m_premultiplyAlpha(alphaOption == AlphaPremultiplied),
        m_colorBehavior(colorBehavior),
        m_maxDecodedBytes(maxDecodedBytes),
        m_purgeAggressively(false) {}

  // Calculates the most recent frame whose image data may be needed in
  // order to decode frame |frameIndex|, based on frame disposal methods
  // and |frameRectIsOpaque|, where |frameRectIsOpaque| signifies whether
  // the rectangle of frame at |frameIndex| is known to be opaque.
  // If no previous frame's data is required, returns WTF::kNotFound.
  //
  // This function requires that the previous frame's
  // |m_requiredPreviousFrameIndex| member has been set correctly. The
  // easiest way to ensure this is for subclasses to call this method and
  // store the result on the frame via setRequiredPreviousFrameIndex()
  // as soon as the frame has been created and parsed sufficiently to
  // determine the disposal method; assuming this happens for all frames
  // in order, the required invariant will hold.
  //
  // Image formats which do not use more than one frame do not need to
  // worry about this; see comments on
  // ImageFrame::m_requiredPreviousFrameIndex.
  size_t findRequiredPreviousFrame(size_t frameIndex, bool frameRectIsOpaque);

  // This is called by clearCacheExceptFrame() if that method decides it wants
  // to preserve another frame, to avoid unnecessary redecoding.
  size_t clearCacheExceptTwoFrames(size_t, size_t);
  virtual void clearFrameBuffer(size_t frameIndex);

  // Decodes the image sufficiently to determine the image size.
  virtual void decodeSize() = 0;

  // Decodes the image sufficiently to determine the number of frames and
  // returns that number.
  virtual size_t decodeFrameCount() { return 1; }

  // Called to initialize the frame buffer with the given index, based on the
  // provided and previous frame's characteristics. Returns true on success. On
  // failure, this will mark the image as failed. Before calling this method,
  // the caller must verify that the frame exists.
  bool initFrameBuffer(size_t);

  // Performs any additional setup of the requested frame after it has been
  // initially created, e.g. setting a duration or disposal method.
  virtual void initializeNewFrame(size_t) {}

  // Decodes the requested frame.
  virtual void decode(size_t) = 0;

  // This method is only required for animated images. It returns a vector with
  // all frame indices that need to be decoded in order to succesfully decode
  // the provided frame.  The indices are returned in reverse order, so the
  // last frame needs to be decoded first.  Before calling this method, the
  // caller must verify that the frame exists.
  Vector<size_t> findFramesToDecode(size_t) const;

  // This is called by decode() after decoding a frame in an animated image.
  // Before calling this method, the caller must verify that the frame exists.
  // @return true  if the frame was fully decoded,
  //         false otherwise.
  bool postDecodeProcessing(size_t);

  // The GIF and PNG decoders set the default alpha setting of the ImageFrame to
  // true. When the frame rect does not contain any (semi-) transparent pixels,
  // this may need to be changed to false. This depends on whether the required
  // previous frame adds transparency to the image, outside of the frame rect.
  // This methods corrects the alpha setting of the frame buffer to false when
  // the whole frame is opaque.
  //
  // This method should be called by the GIF and PNG decoder when the pixels in
  // the frame rect do *not* contain any transparent pixels. Before calling
  // this method, the caller must verify that the frame exists.
  void correctAlphaWhenFrameBufferSawNoAlpha(size_t);

  RefPtr<SegmentReader> m_data;  // The encoded data.
  Vector<ImageFrame, 1> m_frameBufferCache;
  const bool m_premultiplyAlpha;
  const ColorBehavior m_colorBehavior;
  ImageOrientation m_orientation;

  // The maximum amount of memory a decoded image should require. Ideally,
  // image decoders should downsample large images to fit under this limit
  // (and then return the downsampled size from decodedSize()). Ignoring
  // this limit can cause excessive memory use or even crashes on low-
  // memory devices.
  const size_t m_maxDecodedBytes;

  // While decoding, we may learn that there are so many animation frames that
  // we would go beyond our cache budget.
  // If that happens, m_purgeAggressively is set to true. This signals
  // future decodes to purge old frames as it goes.
  void updateAggressivePurging(size_t index);

  // The method is only relevant for multi-frame images.
  //
  // This method indicates whether the provided frame has enough data to decode
  // successive frames that depend on it. It is used by clearCacheExceptFrame
  // to determine which frame to keep in cache when the indicated frame is not
  // yet sufficiently decoded.
  //
  // The default condition is that the frame status needs to be FramePartial or
  // FrameComplete, since the data of previous frames is copied in
  // initFrameBuffer() before setting the status to FramePartial. For WebP,
  // however, the status needs to be FrameComplete since the complete buffer is
  // used to do alpha blending in WEBPImageDecoder::applyPostProcessing().
  //
  // Before calling this, verify that frame |index| exists by checking that
  // |index| is smaller than |m_frameBufferCache|.size().
  virtual bool frameStatusSufficientForSuccessors(size_t index) {
    DCHECK(index < m_frameBufferCache.size());
    return m_frameBufferCache[index].getStatus() != ImageFrame::FrameEmpty;
  }

 private:
  enum class SniffResult { JPEG, PNG, GIF, WEBP, ICO, BMP, Invalid };

  static SniffResult determineImageType(const char* data, size_t length);

  // Some code paths compute the size of the image as "width * height * 4"
  // and return it as a (signed) int.  Avoid overflow.
  static bool sizeCalculationMayOverflow(unsigned width, unsigned height) {
    unsigned long long total_size = static_cast<unsigned long long>(width) *
                                    static_cast<unsigned long long>(height);
    return total_size > ((1 << 29) - 1);
  }

  bool m_purgeAggressively;

  // This methods gets called at the end of initFrameBuffer. Subclasses can do
  // format specific initialization, for e.g. alpha settings, here.
  virtual void onInitFrameBuffer(size_t){};

  // Called by initFrameBuffer to determine if it can take the bitmap of the
  // previous frame. This condition is different for GIF and WEBP.
  virtual bool canReusePreviousFrameBuffer(size_t) const { return false; }

  IntSize m_size;
  bool m_sizeAvailable = false;
  bool m_isAllDataReceived = false;
  bool m_failed = false;
  bool m_hasHistogrammedColorSpace = false;

  sk_sp<SkColorSpace> m_embeddedColorSpace = nullptr;
  bool m_sourceToTargetColorTransformNeedsUpdate = false;
  std::unique_ptr<SkColorSpaceXform> m_sourceToTargetColorTransform;
};

}  // namespace blink

#endif
