/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrPipeline_DEFINED
#define GrPipeline_DEFINED

#include "GrColor.h"
#include "GrFragmentProcessor.h"
#include "GrNonAtomicRef.h"
#include "GrPendingProgramElement.h"
#include "GrPrimitiveProcessor.h"
#include "GrProcOptInfo.h"
#include "GrProgramDesc.h"
#include "GrScissorState.h"
#include "GrUserStencilSettings.h"
#include "GrWindowRectsState.h"
#include "SkMatrix.h"
#include "SkRefCnt.h"

#include "effects/GrCoverageSetOpXP.h"
#include "effects/GrDisableColorXP.h"
#include "effects/GrPorterDuffXferProcessor.h"
#include "effects/GrSimpleTextureEffect.h"

class GrAppliedClip;
class GrDeviceCoordTexture;
class GrOp;
class GrPipelineBuilder;
class GrRenderTargetContext;

/**
 * This Describes aspects of the GrPrimitiveProcessor produced by a GrDrawOp that are used in
 * pipeline analysis.
 */
class GrPipelineAnalysisDrawOpInput {
public:
    GrPipelineAnalysisDrawOpInput(GrPipelineInput* color, GrPipelineInput* coverage)
            : fColorInput(color), fCoverageInput(coverage) {}
    GrPipelineInput* pipelineColorInput() { return fColorInput; }
    GrPipelineInput* pipelineCoverageInput() { return fCoverageInput; }

    void setUsesPLSDstRead() { fUsesPLSDstRead = true; }

    bool usesPLSDstRead() const { return fUsesPLSDstRead; }

private:
    GrPipelineInput* fColorInput;
    GrPipelineInput* fCoverageInput;
    bool fUsesPLSDstRead = false;
};

/** This is used to track pipeline analysis through the color and coverage fragment processors. */
struct GrPipelineAnalysis {
    GrProcOptInfo fColorPOI;
    GrProcOptInfo fCoveragePOI;
    bool fUsesPLSDstRead = false;
};

/**
 * Class that holds an optimized version of a GrPipelineBuilder. It is meant to be an immutable
 * class, and contains all data needed to set the state for a gpu draw.
 */
class GrPipeline : public GrNonAtomicRef<GrPipeline> {
public:
    ///////////////////////////////////////////////////////////////////////////
    /// @name Creation

    struct CreateArgs {
        const GrPipelineBuilder* fPipelineBuilder;
        GrAppliedClip* fAppliedClip;
        GrRenderTargetContext* fRenderTargetContext;
        const GrCaps* fCaps;
        GrPipelineAnalysis fAnalysis;
        GrXferProcessor::DstTexture fDstTexture;
    };

    /** Creates a pipeline into a pre-allocated buffer */
    static GrPipeline* CreateAt(void* memory, const CreateArgs&, GrPipelineOptimizations*);

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name Comparisons

    /**
     * Returns true if these pipelines are equivalent.  Coord transforms may be applied either on
     * the GPU or the CPU. When we apply them on the CPU then the matrices need not agree in order
     * to combine draws. Therefore we take a param that indicates whether coord transforms should be
     * compared."
     */
    static bool AreEqual(const GrPipeline& a, const GrPipeline& b);

    /**
     * Allows a GrOp subclass to determine whether two GrOp instances can combine. This is a
     * stricter test than isEqual because it also considers blend barriers when the two ops'
     * bounds overlap
     */
    static bool CanCombine(const GrPipeline& a, const SkRect& aBounds,
                           const GrPipeline& b, const SkRect& bBounds,
                           const GrCaps& caps)  {
        if (!AreEqual(a, b)) {
            return false;
        }
        if (a.xferBarrierType(caps)) {
            return aBounds.fRight <= bBounds.fLeft ||
                   aBounds.fBottom <= bBounds.fTop ||
                   bBounds.fRight <= aBounds.fLeft ||
                   bBounds.fBottom <= aBounds.fTop;
        }
        return true;
    }

    /// @}

    ///////////////////////////////////////////////////////////////////////////
    /// @name GrFragmentProcessors

    // Make the renderTarget's GrOpList (if it exists) be dependent on any
    // GrOpLists in this pipeline
    void addDependenciesTo(GrRenderTarget* rt) const;

    int numColorFragmentProcessors() const { return fNumColorProcessors; }
    int numCoverageFragmentProcessors() const {
        return fFragmentProcessors.count() - fNumColorProcessors;
    }
    int numFragmentProcessors() const { return fFragmentProcessors.count(); }

    const GrXferProcessor& getXferProcessor() const {
        if (fXferProcessor.get()) {
            return *fXferProcessor.get();
        } else {
            // A null xp member means the common src-over case. GrXferProcessor's ref'ing
            // mechanism is not thread safe so we do not hold a ref on this global.
            return GrPorterDuffXPFactory::SimpleSrcOverXP();
        }
    }

    const GrFragmentProcessor& getColorFragmentProcessor(int idx) const {
        SkASSERT(idx < this->numColorFragmentProcessors());
        return *fFragmentProcessors[idx].get();
    }

    const GrFragmentProcessor& getCoverageFragmentProcessor(int idx) const {
        SkASSERT(idx < this->numCoverageFragmentProcessors());
        return *fFragmentProcessors[fNumColorProcessors + idx].get();
    }

    const GrFragmentProcessor& getFragmentProcessor(int idx) const {
        return *fFragmentProcessors[idx].get();
    }

    /// @}

    /**
     * Retrieves the currently set render-target.
     *
     * @return    The currently set render target.
     */
    GrRenderTarget* getRenderTarget() const { return fRenderTarget.get(); }

    const GrUserStencilSettings* getUserStencil() const { return fUserStencilSettings; }

    const GrScissorState& getScissorState() const { return fScissorState; }

    const GrWindowRectsState& getWindowRectsState() const { return fWindowRectsState; }

    bool isHWAntialiasState() const { return SkToBool(fFlags & kHWAA_Flag); }
    bool snapVerticesToPixelCenters() const { return SkToBool(fFlags & kSnapVertices_Flag); }
    bool getDisableOutputConversionToSRGB() const {
        return SkToBool(fFlags & kDisableOutputConversionToSRGB_Flag);
    }
    bool getAllowSRGBInputs() const {
        return SkToBool(fFlags & kAllowSRGBInputs_Flag);
    }
    bool usesDistanceVectorField() const {
        return SkToBool(fFlags & kUsesDistanceVectorField_Flag);
    }
    bool hasStencilClip() const {
        return SkToBool(fFlags & kHasStencilClip_Flag);
    }
    bool isStencilEnabled() const {
        return SkToBool(fFlags & kStencilEnabled_Flag);
    }

    GrXferBarrierType xferBarrierType(const GrCaps& caps) const {
        return this->getXferProcessor().xferBarrierType(fRenderTarget.get(), caps);
    }

    /**
     * Gets whether the target is drawing clockwise, counterclockwise,
     * or both faces.
     * @return the current draw face(s).
     */
    GrDrawFace getDrawFace() const { return fDrawFace; }


private:
    GrPipeline() { /** Initialized in factory function*/ }

    /**
     * Calculates the primary and secondary output types of the shader. For certain output types
     * the function may adjust the blend coefficients. After this function is called the src and dst
     * blend coeffs will represent those used by backend API.
     */
    void setOutputStateInfo(const GrPipelineBuilder& ds, GrXferProcessor::OptFlags,
                            const GrCaps&);

    enum Flags {
        kHWAA_Flag                          = 0x1,
        kSnapVertices_Flag                  = 0x2,
        kDisableOutputConversionToSRGB_Flag = 0x4,
        kAllowSRGBInputs_Flag               = 0x8,
        kUsesDistanceVectorField_Flag       = 0x10,
        kHasStencilClip_Flag                = 0x20,
        kStencilEnabled_Flag                = 0x40,
    };

    typedef GrPendingIOResource<GrRenderTarget, kWrite_GrIOType> RenderTarget;
    typedef GrPendingProgramElement<const GrFragmentProcessor> PendingFragmentProcessor;
    typedef SkAutoSTArray<8, PendingFragmentProcessor> FragmentProcessorArray;
    typedef GrPendingProgramElement<const GrXferProcessor> ProgramXferProcessor;
    RenderTarget                        fRenderTarget;
    GrScissorState                      fScissorState;
    GrWindowRectsState                  fWindowRectsState;
    const GrUserStencilSettings*        fUserStencilSettings;
    GrDrawFace                          fDrawFace;
    uint32_t                            fFlags;
    ProgramXferProcessor                fXferProcessor;
    FragmentProcessorArray              fFragmentProcessors;

    // This value is also the index in fFragmentProcessors where coverage processors begin.
    int                                 fNumColorProcessors;

    typedef SkRefCnt INHERITED;
};

#endif
