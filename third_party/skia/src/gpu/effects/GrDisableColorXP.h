/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrDisableColorXP_DEFINED
#define GrDisableColorXP_DEFINED

#include "GrTypes.h"
#include "GrXferProcessor.h"
#include "SkRefCnt.h"

class GrProcOptInfo;

// See the comment above GrXPFactory's definition about this warning suppression.
#if defined(__GNUC__) || defined(__clang)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif
class GrDisableColorXPFactory : public GrXPFactory {
public:
    static const GrXPFactory* Get();

    void getInvariantBlendedColor(const GrProcOptInfo& colorPOI,
                                  GrXPFactory::InvariantBlendedColor* blendedColor) const override {
        blendedColor->fKnownColorFlags = kNone_GrColorComponentFlags;
        blendedColor->fWillBlendWithDst = false;
    }

private:
    constexpr GrDisableColorXPFactory() {}

    GrXferProcessor* onCreateXferProcessor(const GrCaps& caps,
                                           const GrPipelineAnalysis&,
                                           bool hasMixedSamples,
                                           const DstTexture* dstTexture) const override;

    bool onWillReadDstColor(const GrCaps&, const GrPipelineAnalysis&) const override {
        return false;
    }

    GR_DECLARE_XP_FACTORY_TEST;

    typedef GrXPFactory INHERITED;
};
#if defined(__GNUC__) || defined(__clang)
#pragma GCC diagnostic pop
#endif

inline const GrXPFactory* GrDisableColorXPFactory::Get() {
    // If this is constructed as static constexpr by cl.exe (2015 SP2) the vtable is null.
#ifdef SK_BUILD_FOR_WIN
    static const GrDisableColorXPFactory gDisableColorXPFactory;
#else
    static constexpr const GrDisableColorXPFactory gDisableColorXPFactory;
#endif
    return &gDisableColorXPFactory;
}

#endif
