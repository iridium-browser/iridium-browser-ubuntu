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

#ifndef ImageFrame_h
#define ImageFrame_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "public/platform/WebVector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/PassRefPtr.h"

namespace blink {

// ImageFrame represents the decoded image data.  This buffer is what all
// decoders write a single frame into.
class PLATFORM_EXPORT ImageFrame final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  enum Status { FrameEmpty, FramePartial, FrameComplete };
  enum DisposalMethod {
    // If you change the numeric values of these, make sure you audit
    // all users, as some users may cast raw values to/from these
    // constants.
    DisposeNotSpecified,      // Leave frame in framebuffer
    DisposeKeep,              // Leave frame in framebuffer
    DisposeOverwriteBgcolor,  // Clear frame to fully transparent
    DisposeOverwritePrevious  // Clear frame to previous framebuffer contents
  };
  // Indicates how non-opaque pixels in the current frame rectangle
  // are blended with those in the previous frame.
  // Notes:
  // * GIF always uses 'BlendAtopPreviousFrame'.
  // * WebP also uses the 'BlendAtopBgcolor' option. This is useful for
  //   cases where one wants to transform a few opaque pixels of the
  //   previous frame into non-opaque pixels in the current frame.
  enum AlphaBlendSource {
    // Blend non-opaque pixels atop the corresponding pixels in the
    // initial buffer state (i.e. any previous frame buffer after having
    // been properly disposed).
    BlendAtopPreviousFrame,

    // Blend non-opaque pixels against fully transparent (i.e. simply
    // overwrite the corresponding pixels).
    BlendAtopBgcolor,
  };
  typedef uint32_t PixelData;

  typedef WebVector<char> ICCProfile;

  ImageFrame();

  // The assignment operator reads m_hasAlpha (inside setStatus()) before it
  // sets it (in setHasAlpha()).  This doesn't cause any problems, since the
  // setHasAlpha() call ensures all state is set correctly, but it means we
  // need to initialize m_hasAlpha to some value before calling the operator
  // lest any tools complain about using an uninitialized value.
  ImageFrame(const ImageFrame& other) : m_hasAlpha(false) { operator=(other); }

  // For backends which refcount their data, this operator doesn't need to
  // create a new copy of the image data, only increase the ref count.
  ImageFrame& operator=(const ImageFrame& other);

  // These do not touch other metadata, only the raw pixel data.
  void clearPixelData();
  void zeroFillPixelData();
  void zeroFillFrameRect(const IntRect&);

  // Makes this frame have an independent copy of the provided image's
  // pixel data, so that modifications in one frame are not reflected in
  // the other.  Returns whether the copy succeeded.
  bool copyBitmapData(const ImageFrame&);

  // Moves the bitmap data from the provided frame to this one, leaving the
  // provided frame empty.  Operation is successful only if bitmap data is not
  // marked as done (immutable).  Returns whether the move succeeded.
  bool takeBitmapDataIfWritable(ImageFrame*);

  // Copies the pixel data at [(startX, startY), (endX, startY)) to the
  // same X-coordinates on each subsequent row up to but not including
  // endY.
  void copyRowNTimes(int startX, int endX, int startY, int endY) {
    DCHECK_LT(startX, width());
    DCHECK_LE(endX, width());
    DCHECK_LT(startY, height());
    DCHECK_LE(endY, height());
    const int rowBytes = (endX - startX) * sizeof(PixelData);
    const PixelData* const startAddr = getAddr(startX, startY);
    for (int destY = startY + 1; destY < endY; ++destY)
      memcpy(getAddr(startX, destY), startAddr, rowBytes);
  }

  // Allocates space for the pixel data.  Must be called before any pixels
  // are written.  Must only be called once. The specified color space may
  // be nullptr if and only if color correct rendering is enabled. Returns
  // whether allocation succeeded.
  bool setSizeAndColorSpace(int newWidth, int newHeight, sk_sp<SkColorSpace>);

  bool hasAlpha() const;
  const IntRect& originalFrameRect() const { return m_originalFrameRect; }
  Status getStatus() const { return m_status; }
  unsigned duration() const { return m_duration; }
  DisposalMethod getDisposalMethod() const { return m_disposalMethod; }
  AlphaBlendSource getAlphaBlendSource() const { return m_alphaBlendSource; }
  bool premultiplyAlpha() const { return m_premultiplyAlpha; }
  SkBitmap::Allocator* allocator() const { return m_allocator; }

  // Returns the bitmap that is the output of decoding.
  const SkBitmap& bitmap() const { return m_bitmap; }

  // Create SkImage from bitmap() and return it.  This should be called only
  // if frame is complete.  The bitmap is set immutable before creating
  // SkImage to avoid copying bitmap in SkImage::MakeFromBitmap(m_bitmap).
  sk_sp<SkImage> finalizePixelsAndGetImage();

  // Returns true if the pixels changed, but the bitmap has not yet been
  // notified.
  bool pixelsChanged() const { return m_pixelsChanged; }
  size_t requiredPreviousFrameIndex() const {
    return m_requiredPreviousFrameIndex;
  }
  void setHasAlpha(bool alpha);
  void setOriginalFrameRect(const IntRect& r) { m_originalFrameRect = r; }
  void setStatus(Status);
  void setDuration(unsigned duration) { m_duration = duration; }
  void setDisposalMethod(DisposalMethod disposalMethod) {
    m_disposalMethod = disposalMethod;
  }
  void setAlphaBlendSource(AlphaBlendSource alphaBlendSource) {
    m_alphaBlendSource = alphaBlendSource;
  }
  void setPremultiplyAlpha(bool premultiplyAlpha) {
    m_premultiplyAlpha = premultiplyAlpha;
  }
  void setMemoryAllocator(SkBitmap::Allocator* allocator) {
    m_allocator = allocator;
  }
  // The pixelsChanged flag needs to be set when the raw pixel data was directly
  // modified (e.g. through a pointer or setRGBA). The flag is usually set after
  // a batch of changes was made.
  void setPixelsChanged(bool pixelsChanged) { m_pixelsChanged = pixelsChanged; }
  void setRequiredPreviousFrameIndex(size_t previousFrameIndex) {
    m_requiredPreviousFrameIndex = previousFrameIndex;
  }

  inline PixelData* getAddr(int x, int y) { return m_bitmap.getAddr32(x, y); }

  inline void setRGBA(int x,
                      int y,
                      unsigned r,
                      unsigned g,
                      unsigned b,
                      unsigned a) {
    setRGBA(getAddr(x, y), r, g, b, a);
  }

  inline void setRGBA(PixelData* dest,
                      unsigned r,
                      unsigned g,
                      unsigned b,
                      unsigned a) {
    if (m_premultiplyAlpha)
      setRGBAPremultiply(dest, r, g, b, a);
    else
      *dest = SkPackARGB32NoCheck(a, r, g, b);
  }

  static inline void setRGBAPremultiply(PixelData* dest,
                                        unsigned r,
                                        unsigned g,
                                        unsigned b,
                                        unsigned a) {
    enum FractionControl { RoundFractionControl = 257 * 128 };

    if (a < 255) {
      unsigned alpha = a * 257;
      r = (r * alpha + RoundFractionControl) >> 16;
      g = (g * alpha + RoundFractionControl) >> 16;
      b = (b * alpha + RoundFractionControl) >> 16;
    }

    *dest = SkPackARGB32NoCheck(a, r, g, b);
  }

  static inline void setRGBARaw(PixelData* dest,
                                unsigned r,
                                unsigned g,
                                unsigned b,
                                unsigned a) {
    *dest = SkPackARGB32NoCheck(a, r, g, b);
  }

  // Blend the RGBA pixel provided by |red|, |green|, |blue| and |alpha| over
  // the pixel in |dest|, without premultiplication, and overwrite |dest| with
  // the result.
  static void blendRGBARaw(PixelData* dest,
                           unsigned red,
                           unsigned green,
                           unsigned blue,
                           unsigned alpha);

  // Blend the pixel, without premultiplication, in |src| over |dst| and
  // overwrite |src| with the result.
  static void blendSrcOverDstRaw(PixelData* src, PixelData dst);

  // Blend the RGBA pixel provided by |r|, |g|, |b|, |a| over the pixel in
  // |dest| and overwrite |dest| with the result. Premultiply the pixel values
  // before blending.
  static inline void blendRGBAPremultiplied(PixelData* dest,
                                            unsigned r,
                                            unsigned g,
                                            unsigned b,
                                            unsigned a) {
    // If the new pixel is completely transparent, no operation is necessary
    // since |dest| contains the background pixel.
    if (a == 0x0)
      return;

    // If the new pixel is opaque, no need for blending - just write the pixel.
    if (a == 0xFF) {
      setRGBAPremultiply(dest, r, g, b, a);
      return;
    }

    PixelData src;
    setRGBAPremultiply(&src, r, g, b, a);
    *dest = SkPMSrcOver(src, *dest);
  }

  // Blend the pixel in |src| over |dst| and overwrite |src| with the result.
  static inline void blendSrcOverDstPremultiplied(PixelData* src,
                                                  PixelData dst) {
    *src = SkPMSrcOver(*src, dst);
  }

  // Notifies the SkBitmap if any pixels changed and resets the flag.
  inline void notifyBitmapIfPixelsChanged() {
    if (m_pixelsChanged)
      m_bitmap.notifyPixelsChanged();
    m_pixelsChanged = false;
  }

 private:
  int width() const { return m_bitmap.width(); }

  int height() const { return m_bitmap.height(); }

  SkAlphaType computeAlphaType() const;

  SkBitmap m_bitmap;
  SkBitmap::Allocator* m_allocator;
  bool m_hasAlpha;
  // This will always just be the entire buffer except for GIF or WebP
  // frames whose original rect was smaller than the overall image size.
  IntRect m_originalFrameRect;
  Status m_status;
  unsigned m_duration;
  DisposalMethod m_disposalMethod;
  AlphaBlendSource m_alphaBlendSource;
  bool m_premultiplyAlpha;
  // True if the pixels changed, but the bitmap has not yet been notified.
  bool m_pixelsChanged;

  // The frame that must be decoded before this frame can be decoded.
  // WTF::kNotFound if this frame doesn't require any previous frame.
  // This is used by ImageDecoder::clearCacheExceptFrame(), and will never
  // be read for image formats that do not have multiple frames.
  size_t m_requiredPreviousFrameIndex;
};

}  // namespace blink

#endif
