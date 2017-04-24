/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef GraphicsContext_h
#define GraphicsContext_h

#include "platform/PlatformExport.h"
#include "platform/fonts/Font.h"
#include "platform/graphics/ColorBehavior.h"
#include "platform/graphics/DashArray.h"
#include "platform/graphics/DrawLooperBuilder.h"
#include "platform/graphics/GraphicsContextState.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMetaData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include <memory>

class SkPath;
class SkRRect;
struct SkRect;

namespace blink {

class FloatRect;
class FloatRoundedRect;
class KURL;
class PaintController;
class Path;

class PLATFORM_EXPORT GraphicsContext {
  WTF_MAKE_NONCOPYABLE(GraphicsContext);
  USING_FAST_MALLOC(GraphicsContext);

 public:
  enum DisabledMode {
    NothingDisabled = 0,  // Run as normal.
    FullyDisabled = 1     // Do absolutely minimal work to remove the cost of
                          // the context from performance tests.
  };

  explicit GraphicsContext(PaintController&,
                           DisabledMode = NothingDisabled,
                           SkMetaData* = 0);

  ~GraphicsContext();

  PaintCanvas* canvas() { return m_canvas; }
  const PaintCanvas* canvas() const { return m_canvas; }

  PaintController& getPaintController() { return m_paintController; }

  bool contextDisabled() const { return m_disabledState; }

  // ---------- State management methods -----------------
  void save();
  void restore();

#if DCHECK_IS_ON()
  unsigned saveCount() const;
#endif

  float strokeThickness() const {
    return immutableState()->getStrokeData().thickness();
  }
  void setStrokeThickness(float thickness) {
    mutableState()->setStrokeThickness(thickness);
  }

  StrokeStyle getStrokeStyle() const {
    return immutableState()->getStrokeData().style();
  }
  void setStrokeStyle(StrokeStyle style) {
    mutableState()->setStrokeStyle(style);
  }

  Color strokeColor() const { return immutableState()->strokeColor(); }
  void setStrokeColor(const Color& color) {
    mutableState()->setStrokeColor(color);
  }

  void setLineCap(LineCap cap) { mutableState()->setLineCap(cap); }
  void setLineDash(const DashArray& dashes, float dashOffset) {
    mutableState()->setLineDash(dashes, dashOffset);
  }
  void setLineJoin(LineJoin join) { mutableState()->setLineJoin(join); }
  void setMiterLimit(float limit) { mutableState()->setMiterLimit(limit); }

  Color fillColor() const { return immutableState()->fillColor(); }
  void setFillColor(const Color& color) { mutableState()->setFillColor(color); }

  void setShouldAntialias(bool antialias) {
    mutableState()->setShouldAntialias(antialias);
  }
  bool shouldAntialias() const { return immutableState()->shouldAntialias(); }

  void setTextDrawingMode(TextDrawingModeFlags mode) {
    mutableState()->setTextDrawingMode(mode);
  }
  TextDrawingModeFlags textDrawingMode() const {
    return immutableState()->textDrawingMode();
  }

  void setImageInterpolationQuality(InterpolationQuality quality) {
    mutableState()->setInterpolationQuality(quality);
  }
  InterpolationQuality imageInterpolationQuality() const {
    return immutableState()->getInterpolationQuality();
  }

  // Specify the device scale factor which may change the way document markers
  // and fonts are rendered.
  void setDeviceScaleFactor(float factor) { m_deviceScaleFactor = factor; }
  float deviceScaleFactor() const { return m_deviceScaleFactor; }

  // Returns if the context is a printing context instead of a display
  // context. Bitmap shouldn't be resampled when printing to keep the best
  // possible quality.
  bool printing() const { return m_printing; }
  void setPrinting(bool printing) { m_printing = printing; }

  SkColorFilter* getColorFilter() const;
  void setColorFilter(ColorFilter);
  // ---------- End state management methods -----------------

  // These draw methods will do both stroking and filling.
  // FIXME: ...except drawRect(), which fills properly but always strokes
  // using a 1-pixel stroke inset from the rect borders (of the correct
  // stroke color).
  void drawRect(const IntRect&);
  void drawLine(const IntPoint&, const IntPoint&);

  void fillPath(const Path&);
  void strokePath(const Path&);

  void fillEllipse(const FloatRect&);
  void strokeEllipse(const FloatRect&);

  void fillRect(const FloatRect&);
  void fillRect(const FloatRect&,
                const Color&,
                SkBlendMode = SkBlendMode::kSrcOver);
  void fillRoundedRect(const FloatRoundedRect&, const Color&);
  void fillDRRect(const FloatRoundedRect&,
                  const FloatRoundedRect&,
                  const Color&);

  void strokeRect(const FloatRect&, float lineWidth);

  void drawRecord(const PaintRecord*);
  void compositeRecord(sk_sp<PaintRecord>,
                       const FloatRect& dest,
                       const FloatRect& src,
                       SkBlendMode);

  void drawImage(Image*,
                 const FloatRect& destRect,
                 const FloatRect* srcRect = nullptr,
                 SkBlendMode = SkBlendMode::kSrcOver,
                 RespectImageOrientationEnum = DoNotRespectImageOrientation);
  void drawImageRRect(
      Image*,
      const FloatRoundedRect& dest,
      const FloatRect& srcRect,
      SkBlendMode = SkBlendMode::kSrcOver,
      RespectImageOrientationEnum = DoNotRespectImageOrientation);
  void drawTiledImage(Image*,
                      const FloatRect& destRect,
                      const FloatPoint& srcPoint,
                      const FloatSize& tileSize,
                      SkBlendMode = SkBlendMode::kSrcOver,
                      const FloatSize& repeatSpacing = FloatSize());
  void drawTiledImage(Image*,
                      const FloatRect& destRect,
                      const FloatRect& srcRect,
                      const FloatSize& tileScaleFactor,
                      Image::TileRule hRule = Image::StretchTile,
                      Image::TileRule vRule = Image::StretchTile,
                      SkBlendMode = SkBlendMode::kSrcOver);

  // These methods write to the canvas.
  // Also drawLine(const IntPoint& point1, const IntPoint& point2) and
  // fillRoundedRect().
  void drawOval(const SkRect&, const PaintFlags&);
  void drawPath(const SkPath&, const PaintFlags&);
  void drawRect(const SkRect&, const PaintFlags&);
  void drawRRect(const SkRRect&, const PaintFlags&);

  void clip(const IntRect& rect) { clipRect(rect); }
  void clip(const FloatRect& rect) { clipRect(rect); }
  void clipRoundedRect(const FloatRoundedRect&,
                       SkClipOp = SkClipOp::kIntersect,
                       AntiAliasingMode = AntiAliased);
  void clipOut(const IntRect& rect) {
    clipRect(rect, NotAntiAliased, SkClipOp::kDifference);
  }
  void clipOut(const FloatRect& rect) {
    clipRect(rect, NotAntiAliased, SkClipOp::kDifference);
  }
  void clipOut(const Path&);
  void clipOutRoundedRect(const FloatRoundedRect&);
  void clipPath(const SkPath&,
                AntiAliasingMode = NotAntiAliased,
                SkClipOp = SkClipOp::kIntersect);
  void clipRect(const SkRect&,
                AntiAliasingMode = NotAntiAliased,
                SkClipOp = SkClipOp::kIntersect);

  void drawText(const Font&, const TextRunPaintInfo&, const FloatPoint&);
  void drawText(const Font&,
                const TextRunPaintInfo&,
                const FloatPoint&,
                const PaintFlags&);
  void drawEmphasisMarks(const Font&,
                         const TextRunPaintInfo&,
                         const AtomicString& mark,
                         const FloatPoint&);
  void drawBidiText(
      const Font&,
      const TextRunPaintInfo&,
      const FloatPoint&,
      Font::CustomFontNotReadyAction = Font::DoNotPaintIfFontNotReady);
  void drawHighlightForText(const Font&,
                            const TextRun&,
                            const FloatPoint&,
                            int h,
                            const Color& backgroundColor,
                            int from = 0,
                            int to = -1);

  void drawLineForText(const FloatPoint&, float width);
  enum DocumentMarkerLineStyle {
    DocumentMarkerSpellingLineStyle,
    DocumentMarkerGrammarLineStyle
  };
  void drawLineForDocumentMarker(const FloatPoint&,
                                 float width,
                                 DocumentMarkerLineStyle,
                                 float zoom);

  // beginLayer()/endLayer() behave like save()/restore() for CTM and clip
  // states. Apply SkBlendMode when the layer is composited on the backdrop
  // (i.e. endLayer()).
  void beginLayer(float opacity = 1.0f,
                  SkBlendMode = SkBlendMode::kSrcOver,
                  const FloatRect* = 0,
                  ColorFilter = ColorFilterNone,
                  sk_sp<SkImageFilter> = nullptr);
  void endLayer();

  // Instead of being dispatched to the active canvas, draw commands following
  // beginRecording() are stored in a display list that can be replayed at a
  // later time. Pass in the bounding rectangle for the content in the list.
  void beginRecording(const FloatRect&);

  // Returns a record with any recorded draw commands since the prerequisite
  // call to beginRecording().  The record is guaranteed to be non-null (but
  // not necessarily non-empty), even when the context is disabled.
  sk_sp<PaintRecord> endRecording();

  void setShadow(const FloatSize& offset,
                 float blur,
                 const Color&,
                 DrawLooperBuilder::ShadowTransformMode =
                     DrawLooperBuilder::ShadowRespectsTransforms,
                 DrawLooperBuilder::ShadowAlphaMode =
                     DrawLooperBuilder::ShadowRespectsAlpha,
                 ShadowMode = DrawShadowAndForeground);

  void setDrawLooper(sk_sp<SkDrawLooper>);

  void drawFocusRing(const Vector<IntRect>&,
                     float width,
                     int offset,
                     const Color&);
  void drawFocusRing(const Path&, float width, int offset, const Color&);

  enum Edge {
    NoEdge = 0,
    TopEdge = 1 << 1,
    RightEdge = 1 << 2,
    BottomEdge = 1 << 3,
    LeftEdge = 1 << 4
  };
  typedef unsigned Edges;
  void drawInnerShadow(const FloatRoundedRect&,
                       const Color& shadowColor,
                       const FloatSize& shadowOffset,
                       float shadowBlur,
                       float shadowSpread,
                       Edges clippedEdges = NoEdge);

  const PaintFlags& fillFlags() const { return immutableState()->fillFlags(); }
  const PaintFlags& strokeFlags() const {
    return immutableState()->strokeFlags();
  }

  // ---------- Transformation methods -----------------
  void concatCTM(const AffineTransform&);

  void scale(float x, float y);
  void rotate(float angleInRadians);
  void translate(float x, float y);
  // ---------- End transformation methods -----------------

  SkFilterQuality computeFilterQuality(Image*,
                                       const FloatRect& dest,
                                       const FloatRect& src) const;

  // Sets target URL of a clickable area.
  void setURLForRect(const KURL&, const IntRect&);

  // Sets the destination of a clickable area of a URL fragment (in a URL
  // pointing to the same web page). When the area is clicked, the page should
  // be scrolled to the location set by setURLDestinationLocation() for the
  // destination whose name is |name|.
  void setURLFragmentForRect(const String& name, const IntRect&);

  // Sets location of a URL destination (a.k.a. anchor) in the page.
  void setURLDestinationLocation(const String& name, const IntPoint&);

  static void adjustLineToPixelBoundaries(FloatPoint& p1,
                                          FloatPoint& p2,
                                          float strokeWidth,
                                          StrokeStyle);

  static int focusRingOutsetExtent(int offset, int width);

#if DCHECK_IS_ON()
  void setInDrawingRecorder(bool);
#endif

  static sk_sp<SkColorFilter> WebCoreColorFilterToSkiaColorFilter(ColorFilter);

 private:
  const GraphicsContextState* immutableState() const { return m_paintState; }

  GraphicsContextState* mutableState() {
    realizePaintSave();
    return m_paintState;
  }

  template <typename DrawTextFunc>
  void drawTextPasses(const DrawTextFunc&);

  void saveLayer(const SkRect* bounds, const PaintFlags*);
  void restoreLayer();

  // Helpers for drawing a focus ring (drawFocusRing)
  void drawFocusRingPath(const SkPath&, const Color&, float width);
  void drawFocusRingRect(const SkRect&, const Color&, float width);

  // SkCanvas wrappers.
  void clipRRect(const SkRRect&,
                 AntiAliasingMode = NotAntiAliased,
                 SkClipOp = SkClipOp::kIntersect);
  void concat(const SkMatrix&);

  // Apply deferred paint state saves
  void realizePaintSave() {
    if (contextDisabled())
      return;

    if (m_paintState->saveCount()) {
      m_paintState->decrementSaveCount();
      ++m_paintStateIndex;
      if (m_paintStateStack.size() == m_paintStateIndex) {
        m_paintStateStack.push_back(
            GraphicsContextState::createAndCopy(*m_paintState));
        m_paintState = m_paintStateStack[m_paintStateIndex].get();
      } else {
        GraphicsContextState* priorPaintState = m_paintState;
        m_paintState = m_paintStateStack[m_paintStateIndex].get();
        m_paintState->copy(*priorPaintState);
      }
    }
  }

  void fillRectWithRoundedHole(const FloatRect&,
                               const FloatRoundedRect& roundedHoleRect,
                               const Color&);

  const SkMetaData& metaData() const { return m_metaData; }

  // null indicates painting is contextDisabled. Never delete this object.
  PaintCanvas* m_canvas;

  PaintController& m_paintController;

  // Paint states stack. The state controls the appearance of drawn content, so
  // this stack enables local drawing state changes with save()/restore() calls.
  // We do not delete from this stack to avoid memory churn.
  Vector<std::unique_ptr<GraphicsContextState>> m_paintStateStack;

  // Current index on the stack. May not be the last thing on the stack.
  unsigned m_paintStateIndex;

  // Raw pointer to the current state.
  GraphicsContextState* m_paintState;

  PaintRecorder m_paintRecorder;

  SkMetaData m_metaData;

#if DCHECK_IS_ON()
  int m_layerCount;
  bool m_disableDestructionChecks;
  bool m_inDrawingRecorder;
#endif

  const DisabledMode m_disabledState;

  float m_deviceScaleFactor;

  unsigned m_printing : 1;
  unsigned m_hasMetaData : 1;
};

}  // namespace blink

#endif  // GraphicsContext_h
