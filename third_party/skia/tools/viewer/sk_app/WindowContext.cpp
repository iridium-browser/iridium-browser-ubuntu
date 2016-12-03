
/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrContext.h"
#include "SkSurface.h"
#include "WindowContext.h"

#include "gl/GrGLDefines.h"

#include "gl/GrGLUtil.h"
#include "GrRenderTarget.h"
#include "GrContext.h"

#include "SkCanvas.h"
#include "SkImage_Base.h"

namespace sk_app {

sk_sp<SkSurface> WindowContext::createOffscreenSurface(bool forceSRGB) {
    return createSurface(nullptr, 0, true, forceSRGB);
}

sk_sp<SkSurface> WindowContext::createRenderSurface(GrBackendRenderTargetDesc desc, int colorBits) {
    return createSurface(&desc, colorBits, false, false);
}

sk_sp<SkSurface> WindowContext::createSurface(
        GrBackendRenderTargetDesc* rtDesc, int colorBits, bool offscreen, bool forceSRGB) {
    if (!this->isGpuContext() || colorBits > 24 || offscreen ||
        kRGBA_F16_SkColorType == fDisplayParams.fColorType) {
        // If we're rendering to F16, we need an off-screen surface - the current render
        // target is most likely the wrong format.
        //
        // If we're rendering raster data or using a deep (10-bit or higher) surface, we probably
        // need an off-screen surface. 10-bit, in particular, has strange gamma behavior.
        SkImageInfo info = SkImageInfo::Make(
            fWidth, fHeight,
            fDisplayParams.fColorType,
            kPremul_SkAlphaType,
            forceSRGB ? SkColorSpace::NewNamed(SkColorSpace::kSRGB_Named)
                      : fDisplayParams.fColorSpace
        );
        if (this->isGpuContext()) {
            return SkSurface::MakeRenderTarget(fContext, SkBudgeted::kNo, info,
                                               fDisplayParams.fMSAASampleCount, &fSurfaceProps);
        } else {
            return SkSurface::MakeRaster(info, &fSurfaceProps);
        }
    } else {
        return SkSurface::MakeFromBackendRenderTarget(fContext, *rtDesc, &fSurfaceProps);
    }
}

}   //namespace sk_app
