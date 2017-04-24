// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/RecordingImageBufferSurface.h"

#include "platform/Histogram.h"
#include "platform/graphics/CanvasMetrics.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

RecordingImageBufferSurface::RecordingImageBufferSurface(
    const IntSize& size,
    std::unique_ptr<RecordingImageBufferFallbackSurfaceFactory> fallbackFactory,
    OpacityMode opacityMode,
    sk_sp<SkColorSpace> colorSpace,
    SkColorType colorType)
    : ImageBufferSurface(size, opacityMode, std::move(colorSpace), colorType),
      m_imageBuffer(0),
      m_currentFramePixelCount(0),
      m_previousFramePixelCount(0),
      m_frameWasCleared(true),
      m_didRecordDrawCommandsInCurrentFrame(false),
      m_currentFrameHasExpensiveOp(false),
      m_previousFrameHasExpensiveOp(false),
      m_fallbackFactory(std::move(fallbackFactory)) {
  initializeCurrentFrame();
}

RecordingImageBufferSurface::~RecordingImageBufferSurface() {}

void RecordingImageBufferSurface::initializeCurrentFrame() {
  static SkRTreeFactory rTreeFactory;
  m_currentFrame = WTF::wrapUnique(new PaintRecorder);
  PaintCanvas* canvas = m_currentFrame->beginRecording(
      size().width(), size().height(), &rTreeFactory);
  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  canvas->save();

  if (m_imageBuffer) {
    m_imageBuffer->resetCanvas(canvas);
  }
  m_didRecordDrawCommandsInCurrentFrame = false;
  m_currentFrameHasExpensiveOp = false;
  m_currentFramePixelCount = 0;
}

void RecordingImageBufferSurface::setImageBuffer(ImageBuffer* imageBuffer) {
  m_imageBuffer = imageBuffer;
  if (m_currentFrame && m_imageBuffer) {
    m_imageBuffer->resetCanvas(m_currentFrame->getRecordingCanvas());
  }
  if (m_fallbackSurface) {
    m_fallbackSurface->setImageBuffer(imageBuffer);
  }
}

bool RecordingImageBufferSurface::writePixels(const SkImageInfo& origInfo,
                                              const void* pixels,
                                              size_t rowBytes,
                                              int x,
                                              int y) {
  if (!m_fallbackSurface) {
    if (x <= 0 && y <= 0 && x + origInfo.width() >= size().width() &&
        y + origInfo.height() >= size().height()) {
      willOverwriteCanvas();
    }
    fallBackToRasterCanvas(FallbackReasonWritePixels);
  }
  return m_fallbackSurface->writePixels(origInfo, pixels, rowBytes, x, y);
}

void RecordingImageBufferSurface::fallBackToRasterCanvas(
    FallbackReason reason) {
  DCHECK(m_fallbackFactory);
  CHECK(reason != FallbackReasonUnknown);

  if (m_fallbackSurface) {
    DCHECK(!m_currentFrame);
    return;
  }

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, canvasFallbackHistogram,
      new EnumerationHistogram("Canvas.DisplayListFallbackReason",
                               FallbackReasonCount));
  canvasFallbackHistogram.count(reason);

  m_fallbackSurface = m_fallbackFactory->createSurface(
      size(), getOpacityMode(), colorSpace(), colorType());
  m_fallbackSurface->setImageBuffer(m_imageBuffer);

  if (m_previousFrame) {
    m_previousFrame->playback(m_fallbackSurface->canvas());
    m_previousFrame.reset();
  }

  if (m_currentFrame) {
    sk_sp<PaintRecord> record = m_currentFrame->finishRecordingAsPicture();
    if (record)
      record->playback(m_fallbackSurface->canvas());
    m_currentFrame.reset();
  }

  if (m_imageBuffer) {
    m_imageBuffer->resetCanvas(m_fallbackSurface->canvas());
  }

  CanvasMetrics::countCanvasContextUsage(
      CanvasMetrics::DisplayList2DCanvasFallbackToRaster);
}

static RecordingImageBufferSurface::FallbackReason
snapshotReasonToFallbackReason(SnapshotReason reason) {
  switch (reason) {
    case SnapshotReasonUnknown:
      return RecordingImageBufferSurface::FallbackReasonUnknown;
    case SnapshotReasonGetImageData:
      return RecordingImageBufferSurface::FallbackReasonSnapshotForGetImageData;
    case SnapshotReasonPaint:
      return RecordingImageBufferSurface::FallbackReasonSnapshotForPaint;
    case SnapshotReasonToDataURL:
      return RecordingImageBufferSurface::FallbackReasonSnapshotForToDataURL;
    case SnapshotReasonToBlob:
      return RecordingImageBufferSurface::FallbackReasonSnapshotForToBlob;
    case SnapshotReasonCanvasListenerCapture:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotForCanvasListenerCapture;
    case SnapshotReasonDrawImage:
      return RecordingImageBufferSurface::FallbackReasonSnapshotForDrawImage;
    case SnapshotReasonCreatePattern:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotForCreatePattern;
    case SnapshotReasonTransferToImageBitmap:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotForTransferToImageBitmap;
    case SnapshotReasonUnitTests:
      return RecordingImageBufferSurface::FallbackReasonSnapshotForUnitTests;
    case SnapshotReasonGetCopiedImage:
      return RecordingImageBufferSurface::FallbackReasonSnapshotGetCopiedImage;
    case SnapshotReasonWebGLDrawImageIntoBuffer:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotWebGLDrawImageIntoBuffer;
    case SnapshotReasonWebGLTexImage2D:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotForWebGLTexImage2D;
    case SnapshotReasonWebGLTexSubImage2D:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotForWebGLTexSubImage2D;
    case SnapshotReasonWebGLTexImage3D:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotForWebGLTexImage3D;
    case SnapshotReasonWebGLTexSubImage3D:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotForWebGLTexSubImage3D;
    case SnapshotReasonCopyToClipboard:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotForCopyToClipboard;
    case SnapshotReasonCreateImageBitmap:
      return RecordingImageBufferSurface::
          FallbackReasonSnapshotForCreateImageBitmap;
  }
  NOTREACHED();
  return RecordingImageBufferSurface::FallbackReasonUnknown;
}

sk_sp<SkImage> RecordingImageBufferSurface::newImageSnapshot(
    AccelerationHint hint,
    SnapshotReason reason) {
  if (!m_fallbackSurface)
    fallBackToRasterCanvas(snapshotReasonToFallbackReason(reason));
  return m_fallbackSurface->newImageSnapshot(hint, reason);
}

PaintCanvas* RecordingImageBufferSurface::canvas() {
  if (m_fallbackSurface)
    return m_fallbackSurface->canvas();

  DCHECK(m_currentFrame->getRecordingCanvas());
  return m_currentFrame->getRecordingCanvas();
}

static RecordingImageBufferSurface::FallbackReason
disableDeferralReasonToFallbackReason(DisableDeferralReason reason) {
  switch (reason) {
    case DisableDeferralReasonUnknown:
      return RecordingImageBufferSurface::FallbackReasonUnknown;
    case DisableDeferralReasonExpensiveOverdrawHeuristic:
      return RecordingImageBufferSurface::
          FallbackReasonExpensiveOverdrawHeuristic;
    case DisableDeferralReasonUsingTextureBackedPattern:
      return RecordingImageBufferSurface::FallbackReasonTextureBackedPattern;
    case DisableDeferralReasonDrawImageOfVideo:
      return RecordingImageBufferSurface::FallbackReasonDrawImageOfVideo;
    case DisableDeferralReasonDrawImageOfAnimated2dCanvas:
      return RecordingImageBufferSurface::
          FallbackReasonDrawImageOfAnimated2dCanvas;
    case DisableDeferralReasonSubPixelTextAntiAliasingSupport:
      return RecordingImageBufferSurface::
          FallbackReasonSubPixelTextAntiAliasingSupport;
    case DisableDeferralDrawImageWithTextureBackedSourceImage:
      return RecordingImageBufferSurface::
          FallbackReasonDrawImageWithTextureBackedSourceImage;
    case DisableDeferralReasonCount:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return RecordingImageBufferSurface::FallbackReasonUnknown;
}

void RecordingImageBufferSurface::disableDeferral(
    DisableDeferralReason reason) {
  if (!m_fallbackSurface)
    fallBackToRasterCanvas(disableDeferralReasonToFallbackReason(reason));
}

sk_sp<PaintRecord> RecordingImageBufferSurface::getRecord() {
  if (m_fallbackSurface)
    return nullptr;

  FallbackReason fallbackReason = FallbackReasonUnknown;
  bool canUseRecord = finalizeFrameInternal(&fallbackReason);

  DCHECK(canUseRecord || m_fallbackFactory);

  if (canUseRecord) {
    return m_previousFrame;
  }

  if (!m_fallbackSurface)
    fallBackToRasterCanvas(fallbackReason);
  return nullptr;
}

void RecordingImageBufferSurface::finalizeFrame() {
  if (m_fallbackSurface) {
    m_fallbackSurface->finalizeFrame();
    return;
  }

  FallbackReason fallbackReason = FallbackReasonUnknown;
  if (!finalizeFrameInternal(&fallbackReason))
    fallBackToRasterCanvas(fallbackReason);
}

void RecordingImageBufferSurface::doPaintInvalidation(
    const FloatRect& dirtyRect) {
  if (m_fallbackSurface) {
    m_fallbackSurface->doPaintInvalidation(dirtyRect);
  }
}

static RecordingImageBufferSurface::FallbackReason flushReasonToFallbackReason(
    FlushReason reason) {
  switch (reason) {
    case FlushReasonUnknown:
      return RecordingImageBufferSurface::FallbackReasonUnknown;
    case FlushReasonInitialClear:
      return RecordingImageBufferSurface::FallbackReasonFlushInitialClear;
    case FlushReasonDrawImageOfWebGL:
      return RecordingImageBufferSurface::
          FallbackReasonFlushForDrawImageOfWebGL;
  }
  NOTREACHED();
  return RecordingImageBufferSurface::FallbackReasonUnknown;
}

void RecordingImageBufferSurface::flush(FlushReason reason) {
  if (!m_fallbackSurface)
    fallBackToRasterCanvas(flushReasonToFallbackReason(reason));
  m_fallbackSurface->flush(reason);
}

void RecordingImageBufferSurface::willOverwriteCanvas() {
  m_frameWasCleared = true;
  m_previousFrame.reset();
  m_previousFrameHasExpensiveOp = false;
  m_previousFramePixelCount = 0;
  if (m_didRecordDrawCommandsInCurrentFrame) {
    // Discard previous draw commands
    m_currentFrame->finishRecordingAsPicture();
    initializeCurrentFrame();
  }
}

void RecordingImageBufferSurface::didDraw(const FloatRect& rect) {
  m_didRecordDrawCommandsInCurrentFrame = true;
  IntRect pixelBounds = enclosingIntRect(rect);
  m_currentFramePixelCount += pixelBounds.width() * pixelBounds.height();
}

bool RecordingImageBufferSurface::finalizeFrameInternal(
    FallbackReason* fallbackReason) {
  CHECK(!m_fallbackSurface);
  CHECK(m_currentFrame);
  DCHECK(m_currentFrame->getRecordingCanvas());
  DCHECK(fallbackReason);
  DCHECK(*fallbackReason == FallbackReasonUnknown);
  if (!m_didRecordDrawCommandsInCurrentFrame) {
    if (!m_previousFrame) {
      // Create an initial blank frame
      m_previousFrame = m_currentFrame->finishRecordingAsPicture();
      initializeCurrentFrame();
    }
    CHECK(m_currentFrame);
    return true;
  }

  if (!m_frameWasCleared) {
    *fallbackReason = FallbackReasonCanvasNotClearedBetweenFrames;
    return false;
  }

  if (m_fallbackFactory &&
      m_currentFrame->getRecordingCanvas()->getSaveCount() - 1 >
          ExpensiveCanvasHeuristicParameters::ExpensiveRecordingStackDepth) {
    // (getSaveCount() decremented to account  for the intial recording canvas
    // save frame.)
    *fallbackReason = FallbackReasonRunawayStateStack;
    return false;
  }

  m_previousFrame = m_currentFrame->finishRecordingAsPicture();
  m_previousFrameHasExpensiveOp = m_currentFrameHasExpensiveOp;
  m_previousFramePixelCount = m_currentFramePixelCount;
  initializeCurrentFrame();

  m_frameWasCleared = false;
  return true;
}

void RecordingImageBufferSurface::draw(GraphicsContext& context,
                                       const FloatRect& destRect,
                                       const FloatRect& srcRect,
                                       SkBlendMode op) {
  if (m_fallbackSurface) {
    m_fallbackSurface->draw(context, destRect, srcRect, op);
    return;
  }

  sk_sp<PaintRecord> record = getRecord();
  if (record) {
    context.compositeRecord(std::move(record), destRect, srcRect, op);
  } else {
    ImageBufferSurface::draw(context, destRect, srcRect, op);
  }
}

bool RecordingImageBufferSurface::isExpensiveToPaint() {
  if (m_fallbackSurface)
    return m_fallbackSurface->isExpensiveToPaint();

  if (m_didRecordDrawCommandsInCurrentFrame) {
    if (m_currentFrameHasExpensiveOp)
      return true;

    if (m_currentFramePixelCount >=
        (size().width() * size().height() *
         ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold))
      return true;

    if (m_frameWasCleared)
      return false;  // early exit because previous frame is overdrawn
  }

  if (m_previousFrame) {
    if (m_previousFrameHasExpensiveOp)
      return true;

    if (m_previousFramePixelCount >=
        (size().width() * size().height() *
         ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold))
      return true;
  }

  return false;
}

// Fallback passthroughs

bool RecordingImageBufferSurface::restore() {
  if (m_fallbackSurface)
    return m_fallbackSurface->restore();
  return ImageBufferSurface::restore();
}

WebLayer* RecordingImageBufferSurface::layer() const {
  if (m_fallbackSurface)
    return m_fallbackSurface->layer();
  return ImageBufferSurface::layer();
}

bool RecordingImageBufferSurface::isAccelerated() const {
  if (m_fallbackSurface)
    return m_fallbackSurface->isAccelerated();
  return ImageBufferSurface::isAccelerated();
}

void RecordingImageBufferSurface::setIsHidden(bool hidden) {
  if (m_fallbackSurface)
    m_fallbackSurface->setIsHidden(hidden);
  else
    ImageBufferSurface::setIsHidden(hidden);
}

}  // namespace blink
