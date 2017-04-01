/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/PictureSnapshot.h"

#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/LoggingCanvas.h"
#include "platform/graphics/ProfilingCanvas.h"
#include "platform/graphics/ReplayingCanvas.h"
#include "platform/graphics/skia/ImagePixelLocker.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/ImageFrame.h"
#include "platform/image-decoders/SegmentReader.h"
#include "platform/image-encoders/PNGImageEncoder.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageDeserializer.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "wtf/CurrentTime.h"
#include "wtf/HexNumber.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/Base64.h"
#include "wtf/text/TextEncoding.h"
#include <memory>

namespace blink {

PictureSnapshot::PictureSnapshot(sk_sp<const SkPicture> picture)
    : m_picture(std::move(picture)) {}

class SkiaImageDecoder final : public SkImageDeserializer {
 public:
  sk_sp<SkImage> makeFromMemory(const void* data,
                                size_t length,
                                const SkIRect* subset) override {
    // No need to copy the data; this decodes immediately.
    RefPtr<SegmentReader> segmentReader =
        SegmentReader::createFromSkData(SkData::MakeWithoutCopy(data, length));
    std::unique_ptr<ImageDecoder> imageDecoder = ImageDecoder::create(
        std::move(segmentReader), true, ImageDecoder::AlphaPremultiplied,
        ColorBehavior::ignore());
    if (!imageDecoder)
      return nullptr;

    ImageFrame* frame = imageDecoder->frameBufferAtIndex(0);
    return (frame && !imageDecoder->failed())
               ? frame->finalizePixelsAndGetImage()
               : nullptr;
  }
  sk_sp<SkImage> makeFromData(SkData* data, const SkIRect* subset) override {
    return this->makeFromMemory(data->data(), data->size(), subset);
  }
};

PassRefPtr<PictureSnapshot> PictureSnapshot::load(
    const Vector<RefPtr<TilePictureStream>>& tiles) {
  ASSERT(!tiles.isEmpty());
  Vector<sk_sp<SkPicture>> pictures;
  pictures.reserveCapacity(tiles.size());
  FloatRect unionRect;
  for (const auto& tileStream : tiles) {
    SkMemoryStream stream(tileStream->data.begin(), tileStream->data.size());
    SkiaImageDecoder factory;
    sk_sp<SkPicture> picture = SkPicture::MakeFromStream(&stream, &factory);
    if (!picture)
      return nullptr;
    FloatRect cullRect(picture->cullRect());
    cullRect.moveBy(tileStream->layerOffset);
    unionRect.unite(cullRect);
    pictures.push_back(std::move(picture));
  }
  if (tiles.size() == 1)
    return adoptRef(new PictureSnapshot(std::move(pictures[0])));
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(unionRect.width(), unionRect.height(), 0, 0);
  for (size_t i = 0; i < pictures.size(); ++i) {
    canvas->save();
    canvas->translate(tiles[i]->layerOffset.x() - unionRect.x(),
                      tiles[i]->layerOffset.y() - unionRect.y());
    pictures[i]->playback(canvas, 0);
    canvas->restore();
  }
  return adoptRef(new PictureSnapshot(recorder.finishRecordingAsPicture()));
}

bool PictureSnapshot::isEmpty() const {
  return m_picture->cullRect().isEmpty();
}

std::unique_ptr<Vector<char>> PictureSnapshot::replay(unsigned fromStep,
                                                      unsigned toStep,
                                                      double scale) const {
  const SkIRect bounds = m_picture->cullRect().roundOut();
  int width = ceil(scale * bounds.width());
  int height = ceil(scale * bounds.height());

  // TODO(fmalita): convert this to SkSurface/SkImage, drop the intermediate
  // SkBitmap.
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  bitmap.eraseARGB(0, 0, 0, 0);
  {
    ReplayingCanvas canvas(bitmap, fromStep, toStep);
    // Disable LCD text preemptively, because the picture opacity is unknown.
    // The canonical API involves SkSurface props, but since we're not
    // SkSurface-based at this point (see TODO above) we (ab)use saveLayer for
    // this purpose.
    SkAutoCanvasRestore autoRestore(&canvas, false);
    canvas.saveLayer(nullptr, nullptr);

    canvas.scale(scale, scale);
    canvas.resetStepCount();
    m_picture->playback(&canvas, &canvas);
  }
  std::unique_ptr<Vector<char>> base64Data = WTF::makeUnique<Vector<char>>();
  Vector<char> encodedImage;

  sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
  if (!image)
    return nullptr;

  ImagePixelLocker pixelLocker(image, kUnpremul_SkAlphaType,
                               kRGBA_8888_SkColorType);
  ImageDataBuffer imageData(
      IntSize(image->width(), image->height()),
      static_cast<const unsigned char*>(pixelLocker.pixels()));
  if (!PNGImageEncoder::encode(
          imageData, reinterpret_cast<Vector<unsigned char>*>(&encodedImage)))
    return nullptr;

  base64Encode(encodedImage, *base64Data);
  return base64Data;
}

std::unique_ptr<PictureSnapshot::Timings> PictureSnapshot::profile(
    unsigned minRepeatCount,
    double minDuration,
    const FloatRect* clipRect) const {
  std::unique_ptr<PictureSnapshot::Timings> timings =
      WTF::makeUnique<PictureSnapshot::Timings>();
  timings->reserveCapacity(minRepeatCount);
  const SkIRect bounds = m_picture->cullRect().roundOut();
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(bounds.width(), bounds.height()));
  bitmap.eraseARGB(0, 0, 0, 0);

  double now = WTF::monotonicallyIncreasingTime();
  double stopTime = now + minDuration;
  for (unsigned step = 0; step < minRepeatCount || now < stopTime; ++step) {
    timings->push_back(Vector<double>());
    Vector<double>* currentTimings = &timings->back();
    if (timings->size() > 1)
      currentTimings->reserveCapacity(timings->begin()->size());
    ProfilingCanvas canvas(bitmap);
    if (clipRect) {
      canvas.clipRect(SkRect::MakeXYWH(clipRect->x(), clipRect->y(),
                                       clipRect->width(), clipRect->height()));
      canvas.resetStepCount();
    }
    canvas.setTimings(currentTimings);
    m_picture->playback(&canvas);
    now = WTF::monotonicallyIncreasingTime();
  }
  return timings;
}

std::unique_ptr<JSONArray> PictureSnapshot::snapshotCommandLog() const {
  const SkIRect bounds = m_picture->cullRect().roundOut();
  LoggingCanvas canvas(bounds.width(), bounds.height());
  m_picture->playback(&canvas);
  return canvas.log();
}

}  // namespace blink
