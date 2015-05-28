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

class GrProcOptInfo;

class GrDisableColorXPFactory : public GrXPFactory {
public:
    static GrXPFactory* Create() {
        return SkNEW(GrDisableColorXPFactory);
    }

    bool supportsRGBCoverage(GrColor knownColor, uint32_t knownColorFlags) const override {
        return true;
    }

    bool canTweakAlphaForCoverage() const override { return true; }

    void getInvariantOutput(const GrProcOptInfo& colorPOI, const GrProcOptInfo& coveragePOI,
                            GrXPFactory::InvariantOutput* output) const override {
        output->fBlendedColorFlags = 0;
        output->fWillBlendWithDst = 0;
    }

private:
    GrDisableColorXPFactory();

    GrXferProcessor* onCreateXferProcessor(const GrDrawTargetCaps& caps,
                                           const GrProcOptInfo& colorPOI,
                                           const GrProcOptInfo& coveragePOI,
                                           const GrDeviceCoordTexture* dstCopy) const override;

    bool willReadDstColor(const GrDrawTargetCaps& caps,
                          const GrProcOptInfo& colorPOI,
                          const GrProcOptInfo& coveragePOI) const override {
        return false;
    }

    bool onIsEqual(const GrXPFactory& xpfBase) const override {
        return true;
    }

    GR_DECLARE_XP_FACTORY_TEST;

    typedef GrXPFactory INHERITED;
};

#endif

