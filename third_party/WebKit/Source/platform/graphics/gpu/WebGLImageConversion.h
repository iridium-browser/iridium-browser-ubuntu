// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLImageConversion_h
#define WebGLImageConversion_h

#include "platform/PlatformExport.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/skia/ImagePixelLocker.h"
#include "platform/heap/Heap.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/khronos/GLES3/gl3.h"
#include "wtf/Allocator.h"
#include "wtf/Optional.h"
#include "wtf/RefPtr.h"

namespace blink {
class Image;
class IntSize;

// Helper functions for texture uploading and pixel readback.
class PLATFORM_EXPORT WebGLImageConversion final {
  STATIC_ONLY(WebGLImageConversion);

 public:
  // Attempt to enumerate all possible native image formats to
  // reduce the amount of temporary allocations during texture
  // uploading. This enum must be public because it is accessed
  // by non-member functions.
  // "_S" postfix indicates signed type.
  enum DataFormat {
    DataFormatRGBA8 = 0,
    DataFormatRGBA8_S,
    DataFormatRGBA16,
    DataFormatRGBA16_S,
    DataFormatRGBA32,
    DataFormatRGBA32_S,
    DataFormatRGBA16F,
    DataFormatRGBA32F,
    DataFormatRGBA2_10_10_10,
    DataFormatRGB8,
    DataFormatRGB8_S,
    DataFormatRGB16,
    DataFormatRGB16_S,
    DataFormatRGB32,
    DataFormatRGB32_S,
    DataFormatRGB16F,
    DataFormatRGB32F,
    DataFormatBGR8,
    DataFormatBGRA8,
    DataFormatARGB8,
    DataFormatABGR8,
    DataFormatRGBA5551,
    DataFormatRGBA4444,
    DataFormatRGB565,
    DataFormatRGB10F11F11F,
    DataFormatRGB5999,
    DataFormatRG8,
    DataFormatRG8_S,
    DataFormatRG16,
    DataFormatRG16_S,
    DataFormatRG32,
    DataFormatRG32_S,
    DataFormatRG16F,
    DataFormatRG32F,
    DataFormatR8,
    DataFormatR8_S,
    DataFormatR16,
    DataFormatR16_S,
    DataFormatR32,
    DataFormatR32_S,
    DataFormatR16F,
    DataFormatR32F,
    DataFormatRA8,
    DataFormatRA16F,
    DataFormatRA32F,
    DataFormatAR8,
    DataFormatA8,
    DataFormatA16F,
    DataFormatA32F,
    DataFormatD16,
    DataFormatD32,
    DataFormatD32F,
    DataFormatDS24_8,
    DataFormatNumFormats
  };

  enum ChannelBits {
    ChannelRed = 1,
    ChannelGreen = 2,
    ChannelBlue = 4,
    ChannelAlpha = 8,
    ChannelDepth = 16,
    ChannelStencil = 32,
    ChannelRG = ChannelRed | ChannelGreen,
    ChannelRGB = ChannelRed | ChannelGreen | ChannelBlue,
    ChannelRGBA = ChannelRGB | ChannelAlpha,
    ChannelDepthStencil = ChannelDepth | ChannelStencil,
  };

  // Possible alpha operations that may need to occur during
  // pixel packing. FIXME: kAlphaDoUnmultiply is lossy and must
  // be removed.
  enum AlphaOp {
    AlphaDoNothing = 0,
    AlphaDoPremultiply = 1,
    AlphaDoUnmultiply = 2
  };

  enum ImageHtmlDomSource {
    HtmlDomImage = 0,
    HtmlDomCanvas = 1,
    HtmlDomVideo = 2,
    HtmlDomNone = 3
  };

  struct PLATFORM_EXPORT PixelStoreParams final {
    PixelStoreParams();

    GLint alignment;
    GLint rowLength;
    GLint imageHeight;
    GLint skipPixels;
    GLint skipRows;
    GLint skipImages;
  };

  class PLATFORM_EXPORT ImageExtractor final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ImageExtractor);

   public:
    ImageExtractor(Image*,
                   ImageHtmlDomSource,
                   bool premultiplyAlpha,
                   bool ignoreColorSpace);

    const void* imagePixelData() {
      return m_imagePixelLocker ? m_imagePixelLocker->pixels() : nullptr;
    }
    unsigned imageWidth() { return m_imageWidth; }
    unsigned imageHeight() { return m_imageHeight; }
    DataFormat imageSourceFormat() { return m_imageSourceFormat; }
    AlphaOp imageAlphaOp() { return m_alphaOp; }
    unsigned imageSourceUnpackAlignment() {
      return m_imageSourceUnpackAlignment;
    }

   private:
    // Extracts the image and keeps track of its status, such as width, height,
    // Source Alignment, format, AlphaOp, etc. This needs to lock the resources
    // or relevant data if needed.
    void extractImage(bool premultiplyAlpha, bool ignoreColorSpace);

    Image* m_image;
    Optional<ImagePixelLocker> m_imagePixelLocker;
    ImageHtmlDomSource m_imageHtmlDomSource;
    unsigned m_imageWidth;
    unsigned m_imageHeight;
    DataFormat m_imageSourceFormat;
    AlphaOp m_alphaOp;
    unsigned m_imageSourceUnpackAlignment;
  };

  // Computes the components per pixel and bytes per component
  // for the given format and type combination. Returns false if
  // either was an invalid enum.
  static bool computeFormatAndTypeParameters(GLenum format,
                                             GLenum type,
                                             unsigned* componentsPerPixel,
                                             unsigned* bytesPerComponent);

  // Computes the image size in bytes. If paddingInBytes is not null, padding
  // is also calculated in return. Returns NO_ERROR if succeed, otherwise
  // return the suggested GL error indicating the cause of the failure:
  //   INVALID_VALUE if width/height/depth is negative or overflow happens.
  //   INVALID_ENUM if format/type is illegal.
  // Note that imageSizeBytes does not include skipSizeInBytes, but it is
  // guaranteed if NO_ERROR is returned, adding the two sizes won't cause
  // overflow.
  // |paddingInBytes| and |skipSizeInBytes| are optional and can be null, but
  // the overflow validation is still performed.
  static GLenum computeImageSizeInBytes(GLenum format,
                                        GLenum type,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        const PixelStoreParams&,
                                        unsigned* imageSizeInBytes,
                                        unsigned* paddingInBytes,
                                        unsigned* skipSizeInBytes);

  // Check if the format is one of the formats from the ImageData or DOM
  // elements. The format from ImageData is always RGBA8. The formats from DOM
  // elements vary with Graphics ports, but can only be RGBA8 or BGRA8.
  static ALWAYS_INLINE bool srcFormatComeFromDOMElementOrImageData(
      DataFormat SrcFormat) {
    return SrcFormat == DataFormatBGRA8 || SrcFormat == DataFormatRGBA8;
  }

  // The input can be either format or internalformat.
  static unsigned getChannelBitsByFormat(GLenum);

  // The Following functions are implemented in
  // GraphicsContext3DImagePacking.cpp.

  // Packs the contents of the given Image, which is passed in |pixels|, into
  // the passed Vector according to the given format and type, and obeying the
  // flipY and AlphaOp flags. Returns true upon success.
  static bool packImageData(Image*,
                            const void* pixels,
                            GLenum format,
                            GLenum type,
                            bool flipY,
                            AlphaOp,
                            DataFormat sourceFormat,
                            unsigned sourceImageWidth,
                            unsigned sourceImageHeight,
                            const IntRect& sourceImageSubRectangle,
                            int depth,
                            unsigned sourceUnpackAlignment,
                            int unpackImageHeight,
                            Vector<uint8_t>& data);

  // Extracts the contents of the given ImageData into the passed Vector,
  // packing the pixel data according to the given format and type,
  // and obeying the flipY and premultiplyAlpha flags. Returns true
  // upon success.
  static bool extractImageData(const uint8_t* imageData,
                               DataFormat sourceDataFormat,
                               const IntSize& imageDataSize,
                               const IntRect& sourceImageSubRectangle,
                               int depth,
                               int unpackImageHeight,
                               GLenum format,
                               GLenum type,
                               bool flipY,
                               bool premultiplyAlpha,
                               Vector<uint8_t>& data);

  // Helper function which extracts the user-supplied texture
  // data, applying the flipY and premultiplyAlpha parameters.
  // If the data is not tightly packed according to the passed
  // unpackAlignment, the output data will be tightly packed.
  // Returns true if successful, false if any error occurred.
  static bool extractTextureData(unsigned width,
                                 unsigned height,
                                 GLenum format,
                                 GLenum type,
                                 unsigned unpackAlignment,
                                 bool flipY,
                                 bool premultiplyAlpha,
                                 const void* pixels,
                                 Vector<uint8_t>& data);

  // End GraphicsContext3DImagePacking.cpp functions

 private:
  friend class WebGLImageConversionTest;
  // Helper for packImageData/extractImageData/extractTextureData, which
  // implement packing of pixel data into the specified OpenGL destination
  // format and type. A sourceUnpackAlignment of zero indicates that the source
  // data is tightly packed. Non-zero values may take a slow path. Destination
  // data will have no gaps between rows. Implemented in
  // GraphicsContext3DImagePacking.cpp.
  static bool packPixels(const uint8_t* sourceData,
                         DataFormat sourceDataFormat,
                         unsigned sourceDataWidth,
                         unsigned sourceDataHeight,
                         const IntRect& sourceDataSubRectangle,
                         int depth,
                         unsigned sourceUnpackAlignment,
                         int unpackImageHeight,
                         unsigned destinationFormat,
                         unsigned destinationType,
                         AlphaOp,
                         void* destinationData,
                         bool flipY);
  static void unpackPixels(const uint16_t* sourceData,
                           DataFormat sourceDataFormat,
                           unsigned pixelsPerRow,
                           uint8_t* destinationData);
  static void packPixels(const uint8_t* sourceData,
                         DataFormat sourceDataFormat,
                         unsigned pixelsPerRow,
                         uint8_t* destinationData);
};

}  // namespace blink

#endif  // WebGLImageConversion_h
