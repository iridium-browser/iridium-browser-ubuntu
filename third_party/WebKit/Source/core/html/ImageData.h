/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageData_h
#define ImageData_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/DOMTypedArray.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "wtf/CheckedNumeric.h"
#include "wtf/Compiler.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ImageBitmapOptions;

enum ConstructorParams {
  kParamSize = 1,
  kParamWidth = 1 << 1,
  kParamHeight = 1 << 2,
  kParamData = 1 << 3,
  kParamColorSpace = 1 << 4,
};

enum ImageDataType {
  kUint8ClampedImageData,
  kFloat32ImageData,
};

enum ImageDataColorSpace {
  kLegacyImageDataColorSpace,
  kSRGBImageDataColorSpace,
  kLinearRGBImageDataColorSpace,
};

const char* const kLinearRGBImageDataColorSpaceName = "linear-rgb";
const char* const kSRGBImageDataColorSpaceName = "srgb";
const char* const kLegacyImageDataColorSpaceName = "legacy-srgb";

class CORE_EXPORT ImageData final : public GarbageCollectedFinalized<ImageData>,
                                    public ScriptWrappable,
                                    public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageData* create(const IntSize&);
  static ImageData* create(const IntSize&, DOMUint8ClampedArray*);
  static ImageData* create(unsigned width, unsigned height, ExceptionState&);
  static ImageData* create(DOMUint8ClampedArray*,
                           unsigned width,
                           ExceptionState&);
  static ImageData* create(DOMUint8ClampedArray*,
                           unsigned width,
                           unsigned height,
                           ExceptionState&);

  static ImageData* createForTest(const IntSize&);

  ImageData* createImageData(unsigned width,
                             unsigned height,
                             String colorSpace,
                             ExceptionState&);
  ImageData* createImageData(DOMUint8ClampedArray*,
                             unsigned width,
                             String colorSpace,
                             ExceptionState&);
  ImageData* createImageData(DOMUint8ClampedArray*,
                             unsigned width,
                             unsigned height,
                             String colorSpace,
                             ExceptionState&);

  static ImageDataColorSpace getImageDataColorSpace(String);
  static String getImageDataColorSpaceName(ImageDataColorSpace);

  IntSize size() const { return m_size; }
  int width() const { return m_size.width(); }
  int height() const { return m_size.height(); }
  String colorSpace() const { return getImageDataColorSpaceName(m_colorSpace); }
  ImageDataColorSpace imageDataColorSpace() { return m_colorSpace; }
  const DOMUint8ClampedArray* data() const { return m_data.get(); }
  DOMUint8ClampedArray* data() { return m_data.get(); }

  // ImageBitmapSource implementation
  IntSize bitmapSourceSize() const override { return m_size; }
  ScriptPromise createImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect> cropRect,
                                  const ImageBitmapOptions&,
                                  ExceptionState&) override;

  DEFINE_INLINE_TRACE() { visitor->trace(m_data); }

  WARN_UNUSED_RESULT v8::Local<v8::Object> associateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

  static bool validateConstructorArguments(
      const unsigned&,
      const IntSize* = nullptr,
      const unsigned& = 0,
      const unsigned& = 0,
      const DOMArrayBufferView* = nullptr,
      const String* = nullptr,
      ExceptionState* = nullptr,
      ImageDataType = kUint8ClampedImageData);

 private:
  ImageData(const IntSize&,
            DOMUint8ClampedArray*,
            String = kLegacyImageDataColorSpaceName);

  IntSize m_size;
  ImageDataColorSpace m_colorSpace;
  Member<DOMUint8ClampedArray> m_data;

  static DOMUint8ClampedArray* allocateAndValidateUint8ClampedArray(
      const unsigned&,
      ExceptionState* = nullptr);
};

}  // namespace blink

#endif  // ImageData_h
