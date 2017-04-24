/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef Image_h
#define Image_h

#include "platform/PlatformExport.h"
#include "platform/SharedBuffer.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/ColorBehavior.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/ImageAnimationPolicy.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/text/WTFString.h"

class SkImage;
class SkMatrix;

namespace blink {

class FloatPoint;
class FloatRect;
class FloatSize;
class GraphicsContext;
class Image;

class PLATFORM_EXPORT Image : public ThreadSafeRefCounted<Image> {
  friend class GeneratedImage;
  friend class CrossfadeGeneratedImage;
  friend class GradientGeneratedImage;
  friend class GraphicsContext;
  WTF_MAKE_NONCOPYABLE(Image);

 public:
  virtual ~Image();

  static PassRefPtr<Image> loadPlatformResource(const char* name);
  static bool supportsType(const String&);

  virtual bool isSVGImage() const { return false; }
  virtual bool isBitmapImage() const { return false; }

  // To increase accuracy of currentFrameKnownToBeOpaque() it may,
  // for applicable image types, be told to pre-cache metadata for
  // the current frame. Since this may initiate a deferred image
  // decoding, PreCacheMetadata requires a InspectorPaintImageEvent
  // during call.
  enum MetadataMode { UseCurrentMetadata, PreCacheMetadata };
  virtual bool currentFrameKnownToBeOpaque(
      MetadataMode = UseCurrentMetadata) = 0;

  virtual bool currentFrameIsComplete() { return false; }
  virtual bool currentFrameIsLazyDecoded() { return false; }
  virtual bool isTextureBacked() const { return false; }

  // Derived classes should override this if they can assure that the current
  // image frame contains only resources from its own security origin.
  virtual bool currentFrameHasSingleSecurityOrigin() const { return false; }

  static Image* nullImage();
  bool isNull() const { return size().isEmpty(); }

  virtual bool usesContainerSize() const { return false; }
  virtual bool hasRelativeSize() const { return false; }

  virtual IntSize size() const = 0;
  IntRect rect() const { return IntRect(IntPoint(), size()); }
  int width() const { return size().width(); }
  int height() const { return size().height(); }
  virtual bool getHotSpot(IntPoint&) const { return false; }

  enum SizeAvailability { SizeAvailable, SizeUnavailable };
  virtual SizeAvailability setData(PassRefPtr<SharedBuffer> data,
                                   bool allDataReceived);
  virtual SizeAvailability dataChanged(bool /*allDataReceived*/) {
    return SizeUnavailable;
  }

  virtual String filenameExtension() const {
    return String();
  }  // null string if unknown

  virtual void destroyDecodedData() = 0;

  virtual PassRefPtr<SharedBuffer> data() { return m_encodedImageData; }

  // Animation begins whenever someone draws the image, so startAnimation() is
  // not normally called. It will automatically pause once all observers no
  // longer want to render the image anywhere.
  enum CatchUpAnimation { DoNotCatchUp, CatchUp };
  virtual void startAnimation(CatchUpAnimation = CatchUp) {}
  virtual void resetAnimation() {}

  // True if this image can potentially animate.
  virtual bool maybeAnimated() { return false; }

  // Set animationPolicy
  virtual void setAnimationPolicy(ImageAnimationPolicy) {}
  virtual ImageAnimationPolicy animationPolicy() {
    return ImageAnimationPolicyAllowed;
  }
  virtual void advanceTime(double deltaTimeInSeconds) {}

  // Advances an animated image. For BitmapImage (e.g., animated gifs) this
  // will advance to the next frame. For SVGImage, this will trigger an
  // animation update for CSS and advance the SMIL timeline by one frame.
  virtual void advanceAnimationForTesting() {}

  // Typically the ImageResourceContent that owns us.
  ImageObserver* getImageObserver() const {
    return m_imageObserverDisabled ? nullptr : m_imageObserver;
  }
  void clearImageObserver() { m_imageObserver = nullptr; }
  // To avoid interleaved accesses to |m_imageObserverDisabled|, do not call
  // setImageObserverDisabled() other than from ImageObserverDisabler.
  void setImageObserverDisabled(bool disabled) {
    m_imageObserverDisabled = disabled;
  }

  enum TileRule { StretchTile, RoundTile, SpaceTile, RepeatTile };

  virtual sk_sp<SkImage> imageForCurrentFrame(const ColorBehavior&) = 0;
  virtual PassRefPtr<Image> imageForDefaultFrame();

  enum ImageClampingMode {
    ClampImageToSourceRect,
    DoNotClampImageToSourceRect
  };

  virtual void draw(PaintCanvas*,
                    const PaintFlags&,
                    const FloatRect& dstRect,
                    const FloatRect& srcRect,
                    RespectImageOrientationEnum,
                    ImageClampingMode) = 0;

  virtual bool applyShader(PaintFlags&, const SkMatrix& localMatrix);

  // Compute the tile which contains a given point (assuming a repeating tile
  // grid). The point and returned value are in destination grid space.
  static FloatRect computeTileContaining(const FloatPoint&,
                                         const FloatSize& tileSize,
                                         const FloatPoint& tilePhase,
                                         const FloatSize& tileSpacing);

  // Compute the image subset which gets mapped onto |dest|, when the whole
  // image is drawn into |tile|.  Assumes |tile| contains |dest|.  The tile rect
  // is in destination grid space while the return value is in image coordinate
  // space.
  static FloatRect computeSubsetForTile(const FloatRect& tile,
                                        const FloatRect& dest,
                                        const FloatSize& imageSize);

 protected:
  Image(ImageObserver* = 0);

  void drawTiledBackground(GraphicsContext&,
                           const FloatRect& dstRect,
                           const FloatPoint& srcPoint,
                           const FloatSize& tileSize,
                           SkBlendMode,
                           const FloatSize& repeatSpacing);
  void drawTiledBorder(GraphicsContext&,
                       const FloatRect& dstRect,
                       const FloatRect& srcRect,
                       const FloatSize& tileScaleFactor,
                       TileRule hRule,
                       TileRule vRule,
                       SkBlendMode);

  virtual void drawPattern(GraphicsContext&,
                           const FloatRect&,
                           const FloatSize&,
                           const FloatPoint& phase,
                           SkBlendMode,
                           const FloatRect&,
                           const FloatSize& repeatSpacing = FloatSize());

 private:
  RefPtr<SharedBuffer> m_encodedImageData;
  // TODO(Oilpan): consider having Image on the Oilpan heap and
  // turn this into a Member<>.
  //
  // The observer (an ImageResourceContent) is an untraced member, with the
  // ImageResourceContent being responsible for clearing itself out.
  UntracedMember<ImageObserver> m_imageObserver;
  bool m_imageObserverDisabled;
};

#define DEFINE_IMAGE_TYPE_CASTS(typeName)                          \
  DEFINE_TYPE_CASTS(typeName, Image, image, image->is##typeName(), \
                    image.is##typeName())

}  // namespace blink

#endif
