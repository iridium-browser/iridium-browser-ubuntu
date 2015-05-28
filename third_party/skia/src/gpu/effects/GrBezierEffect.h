/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrBezierEffect_DEFINED
#define GrBezierEffect_DEFINED

#include "GrDrawTargetCaps.h"
#include "GrProcessor.h"
#include "GrGeometryProcessor.h"
#include "GrInvariantOutput.h"
#include "GrTypesPriv.h"

/**
 * Shader is based off of Loop-Blinn Quadratic GPU Rendering
 * The output of this effect is a hairline edge for conics.
 * Conics specified by implicit equation K^2 - LM.
 * K, L, and M, are the first three values of the vertex attribute,
 * the fourth value is not used. Distance is calculated using a
 * first order approximation from the taylor series.
 * Coverage for AA is max(0, 1-distance).
 *
 * Test were also run using a second order distance approximation.
 * There were two versions of the second order approx. The first version
 * is of roughly the form:
 * f(q) = |f(p)| - ||f'(p)||*||q-p|| - ||f''(p)||*||q-p||^2.
 * The second is similar:
 * f(q) = |f(p)| + ||f'(p)||*||q-p|| + ||f''(p)||*||q-p||^2.
 * The exact version of the equations can be found in the paper
 * "Distance Approximations for Rasterizing Implicit Curves" by Gabriel Taubin
 *
 * In both versions we solve the quadratic for ||q-p||.
 * Version 1:
 * gFM is magnitude of first partials and gFM2 is magnitude of 2nd partials (as derived from paper)
 * builder->fsCodeAppend("\t\tedgeAlpha = (sqrt(gFM*gFM+4.0*func*gF2M) - gFM)/(2.0*gF2M);\n");
 * Version 2:
 * builder->fsCodeAppend("\t\tedgeAlpha = (gFM - sqrt(gFM*gFM-4.0*func*gF2M))/(2.0*gF2M);\n");
 *
 * Also note that 2nd partials of k,l,m are zero
 *
 * When comparing the two second order approximations to the first order approximations,
 * the following results were found. Version 1 tends to underestimate the distances, thus it
 * basically increases all the error that we were already seeing in the first order
 * approx. So this version is not the one to use. Version 2 has the opposite effect
 * and tends to overestimate the distances. This is much closer to what we are
 * looking for. It is able to render ellipses (even thin ones) without the need to chop.
 * However, it can not handle thin hyperbolas well and thus would still rely on
 * chopping to tighten the clipping. Another side effect of the overestimating is
 * that the curves become much thinner and "ropey". If all that was ever rendered
 * were "not too thin" curves and ellipses then 2nd order may have an advantage since
 * only one geometry would need to be rendered. However no benches were run comparing
 * chopped first order and non chopped 2nd order.
 */
class GrGLConicEffect;

class GrConicEffect : public GrGeometryProcessor {
public:
    static GrGeometryProcessor* Create(GrColor color,
                                       const SkMatrix& viewMatrix,
                                       const GrPrimitiveEdgeType edgeType,
                                       const GrDrawTargetCaps& caps,
                                       const SkMatrix& localMatrix,
                                       uint8_t coverage = 0xff) {
        switch (edgeType) {
            case kFillAA_GrProcessorEdgeType:
                if (!caps.shaderDerivativeSupport()) {
                    return NULL;
                }
                return SkNEW_ARGS(GrConicEffect, (color, viewMatrix, coverage,
                                                  kFillAA_GrProcessorEdgeType,
                                                  localMatrix));
            case kHairlineAA_GrProcessorEdgeType:
                if (!caps.shaderDerivativeSupport()) {
                    return NULL;
                }
                return SkNEW_ARGS(GrConicEffect, (color, viewMatrix, coverage,
                                                  kHairlineAA_GrProcessorEdgeType,
                                                  localMatrix));
            case kFillBW_GrProcessorEdgeType:
                return SkNEW_ARGS(GrConicEffect, (color, viewMatrix, coverage,
                                                  kFillBW_GrProcessorEdgeType,
                                                  localMatrix));
            default:
                return NULL;
        }
    }

    virtual ~GrConicEffect();

    const char* name() const override { return "Conic"; }

    inline const Attribute* inPosition() const { return fInPosition; }
    inline const Attribute* inConicCoeffs() const { return fInConicCoeffs; }
    inline bool isAntiAliased() const { return GrProcessorEdgeTypeIsAA(fEdgeType); }
    inline bool isFilled() const { return GrProcessorEdgeTypeIsFill(fEdgeType); }
    inline GrPrimitiveEdgeType getEdgeType() const { return fEdgeType; }

    virtual void getGLProcessorKey(const GrBatchTracker& bt,
                                   const GrGLCaps& caps,
                                   GrProcessorKeyBuilder* b) const override;

    virtual GrGLPrimitiveProcessor* createGLInstance(const GrBatchTracker& bt,
                                                     const GrGLCaps&) const override;

    void initBatchTracker(GrBatchTracker*, const GrPipelineInfo&) const override;
    bool onCanMakeEqual(const GrBatchTracker&,
                        const GrGeometryProcessor&,
                        const GrBatchTracker&) const override;

private:
    GrConicEffect(GrColor, const SkMatrix& viewMatrix, uint8_t coverage, GrPrimitiveEdgeType,
                  const SkMatrix& localMatrix);

    bool onIsEqual(const GrGeometryProcessor& other) const override;

    void onGetInvariantOutputCoverage(GrInitInvariantOutput* out) const override {
        out->setUnknownSingleComponent();
    }

    uint8_t               fCoverageScale;
    GrPrimitiveEdgeType   fEdgeType;
    const Attribute*    fInPosition;
    const Attribute*    fInConicCoeffs;

    GR_DECLARE_GEOMETRY_PROCESSOR_TEST;

    typedef GrGeometryProcessor INHERITED;
};

///////////////////////////////////////////////////////////////////////////////
/**
 * The output of this effect is a hairline edge for quadratics.
 * Quadratic specified by 0=u^2-v canonical coords. u and v are the first
 * two components of the vertex attribute. At the three control points that define
 * the Quadratic, u, v have the values {0,0}, {1/2, 0}, and {1, 1} respectively.
 * Coverage for AA is min(0, 1-distance). 3rd & 4th cimponent unused.
 * Requires shader derivative instruction support.
 */
class GrGLQuadEffect;

class GrQuadEffect : public GrGeometryProcessor {
public:
    static GrGeometryProcessor* Create(GrColor color,
                                       const SkMatrix& viewMatrix,
                                       const GrPrimitiveEdgeType edgeType,
                                       const GrDrawTargetCaps& caps,
                                       const SkMatrix& localMatrix,
                                       uint8_t coverage = 0xff) {
        switch (edgeType) {
            case kFillAA_GrProcessorEdgeType:
                if (!caps.shaderDerivativeSupport()) {
                    return NULL;
                }
                return SkNEW_ARGS(GrQuadEffect, (color, viewMatrix, coverage,
                                                 kFillAA_GrProcessorEdgeType,
                                                 localMatrix));
            case kHairlineAA_GrProcessorEdgeType:
                if (!caps.shaderDerivativeSupport()) {
                    return NULL;
                }
                return SkNEW_ARGS(GrQuadEffect, (color, viewMatrix, coverage,
                                                 kHairlineAA_GrProcessorEdgeType,
                                                 localMatrix));
            case kFillBW_GrProcessorEdgeType:
                return SkNEW_ARGS(GrQuadEffect, (color, viewMatrix, coverage,
                                                 kFillBW_GrProcessorEdgeType,
                                                 localMatrix));
            default:
                return NULL;
        }
    }

    virtual ~GrQuadEffect();

    const char* name() const override { return "Quad"; }

    inline const Attribute* inPosition() const { return fInPosition; }
    inline const Attribute* inHairQuadEdge() const { return fInHairQuadEdge; }
    inline bool isAntiAliased() const { return GrProcessorEdgeTypeIsAA(fEdgeType); }
    inline bool isFilled() const { return GrProcessorEdgeTypeIsFill(fEdgeType); }
    inline GrPrimitiveEdgeType getEdgeType() const { return fEdgeType; }

    virtual void getGLProcessorKey(const GrBatchTracker& bt,
                                   const GrGLCaps& caps,
                                   GrProcessorKeyBuilder* b) const override;

    virtual GrGLPrimitiveProcessor* createGLInstance(const GrBatchTracker& bt,
                                                     const GrGLCaps&) const override;

    void initBatchTracker(GrBatchTracker*, const GrPipelineInfo&) const override;
    bool onCanMakeEqual(const GrBatchTracker&,
                        const GrGeometryProcessor&,
                        const GrBatchTracker&) const override;

private:
    GrQuadEffect(GrColor, const SkMatrix& viewMatrix, uint8_t coverage, GrPrimitiveEdgeType,
                 const SkMatrix& localMatrix);

    bool onIsEqual(const GrGeometryProcessor& other) const override;

    void onGetInvariantOutputCoverage(GrInitInvariantOutput* out) const override {
        out->setUnknownSingleComponent();
    }

    uint8_t               fCoverageScale;
    GrPrimitiveEdgeType   fEdgeType;
    const Attribute*    fInPosition;
    const Attribute*    fInHairQuadEdge;

    GR_DECLARE_GEOMETRY_PROCESSOR_TEST;

    typedef GrGeometryProcessor INHERITED;
};

//////////////////////////////////////////////////////////////////////////////
/**
 * Shader is based off of "Resolution Independent Curve Rendering using
 * Programmable Graphics Hardware" by Loop and Blinn.
 * The output of this effect is a hairline edge for non rational cubics.
 * Cubics are specified by implicit equation K^3 - LM.
 * K, L, and M, are the first three values of the vertex attribute,
 * the fourth value is not used. Distance is calculated using a
 * first order approximation from the taylor series.
 * Coverage for AA is max(0, 1-distance).
 */
class GrGLCubicEffect;

class GrCubicEffect : public GrGeometryProcessor {
public:
    static GrGeometryProcessor* Create(GrColor color,
                                       const SkMatrix& viewMatrix,
                                       const GrPrimitiveEdgeType edgeType,
                                       const GrDrawTargetCaps& caps) {
        switch (edgeType) {
            case kFillAA_GrProcessorEdgeType:
                if (!caps.shaderDerivativeSupport()) {
                    return NULL;
                }
                return SkNEW_ARGS(GrCubicEffect, (color, viewMatrix, kFillAA_GrProcessorEdgeType));
            case kHairlineAA_GrProcessorEdgeType:
                if (!caps.shaderDerivativeSupport()) {
                    return NULL;
                }
                return SkNEW_ARGS(GrCubicEffect, (color, viewMatrix,
                                                  kHairlineAA_GrProcessorEdgeType));
            case kFillBW_GrProcessorEdgeType:
                return SkNEW_ARGS(GrCubicEffect, (color, viewMatrix,
                                                  kFillBW_GrProcessorEdgeType));
            default:
                return NULL;
        }
    }

    virtual ~GrCubicEffect();

    const char* name() const override { return "Cubic"; }

    inline const Attribute* inPosition() const { return fInPosition; }
    inline const Attribute* inCubicCoeffs() const { return fInCubicCoeffs; }
    inline bool isAntiAliased() const { return GrProcessorEdgeTypeIsAA(fEdgeType); }
    inline bool isFilled() const { return GrProcessorEdgeTypeIsFill(fEdgeType); }
    inline GrPrimitiveEdgeType getEdgeType() const { return fEdgeType; }

    virtual void getGLProcessorKey(const GrBatchTracker& bt,
                                   const GrGLCaps& caps,
                                   GrProcessorKeyBuilder* b) const override;

    virtual GrGLPrimitiveProcessor* createGLInstance(const GrBatchTracker& bt,
                                                     const GrGLCaps&) const override;

    void initBatchTracker(GrBatchTracker*, const GrPipelineInfo&) const override;
    bool onCanMakeEqual(const GrBatchTracker&,
                        const GrGeometryProcessor&,
                        const GrBatchTracker&) const override;

private:
    GrCubicEffect(GrColor, const SkMatrix& viewMatrix, GrPrimitiveEdgeType);

    bool onIsEqual(const GrGeometryProcessor& other) const override;

    void onGetInvariantOutputCoverage(GrInitInvariantOutput* out) const override {
        out->setUnknownSingleComponent();
    }

    GrPrimitiveEdgeType   fEdgeType;
    const Attribute*    fInPosition;
    const Attribute*    fInCubicCoeffs;

    GR_DECLARE_GEOMETRY_PROCESSOR_TEST;

    typedef GrGeometryProcessor INHERITED;
};

#endif
