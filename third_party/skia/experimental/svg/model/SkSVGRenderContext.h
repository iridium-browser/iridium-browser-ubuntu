/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGRenderContext_DEFINED
#define SkSVGRenderContext_DEFINED

#include "SkPaint.h"
#include "SkRect.h"
#include "SkSize.h"
#include "SkSVGAttribute.h"
#include "SkTLazy.h"
#include "SkTypes.h"

class SkCanvas;
class SkSVGLength;

class SkSVGLengthContext {
public:
    SkSVGLengthContext(const SkSize& viewport, SkScalar dpi = 90)
        : fViewport(viewport), fDPI(dpi) {}

    enum class LengthType {
        kHorizontal,
        kVertical,
        kOther,
    };

    const SkSize& viewPort() const { return fViewport; }
    void setViewPort(const SkSize& viewport) { fViewport = viewport; }

    SkScalar resolve(const SkSVGLength&, LengthType) const;
    SkRect   resolveRect(const SkSVGLength& x, const SkSVGLength& y,
                         const SkSVGLength& w, const SkSVGLength& h) const;

private:
    SkSize   fViewport;
    SkScalar fDPI;
};

struct SkSVGPresentationContext {
    SkSVGPresentationContext();
    SkSVGPresentationContext(const SkSVGPresentationContext&)            = default;
    SkSVGPresentationContext& operator=(const SkSVGPresentationContext&) = default;

    // Inherited presentation attributes, computed for the current node.
    SkSVGPresentationAttributes fInherited;

    // Cached paints, reflecting the current presentation attributes.
    SkPaint fFillPaint;
    SkPaint fStrokePaint;
};

class SkSVGRenderContext {
public:
    SkSVGRenderContext(SkCanvas*, const SkSVGLengthContext&, const SkSVGPresentationContext&);
    SkSVGRenderContext(const SkSVGRenderContext&);
    ~SkSVGRenderContext();

    const SkSVGLengthContext& lengthContext() const { return *fLengthContext; }
    SkSVGLengthContext* writableLengthContext() { return fLengthContext.writable(); }

    SkCanvas* canvas() const { return fCanvas; }

    void applyPresentationAttributes(const SkSVGPresentationAttributes&);

    const SkPaint* fillPaint() const;
    const SkPaint* strokePaint() const;

private:
    // Stack-only
    void* operator new(size_t)                               = delete;
    void* operator new(size_t, void*)                        = delete;
    SkSVGRenderContext& operator=(const SkSVGRenderContext&) = delete;

    SkTCopyOnFirstWrite<SkSVGLengthContext>       fLengthContext;
    SkTCopyOnFirstWrite<SkSVGPresentationContext> fPresentationContext;
    SkCanvas*                                     fCanvas;
    // The save count on 'fCanvas' at construction time.
    // A restoreToCount() will be issued on destruction.
    int                                           fCanvasSaveCount;
};

#endif // SkSVGRenderContext_DEFINED
