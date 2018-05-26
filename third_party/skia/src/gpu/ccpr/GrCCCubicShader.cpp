/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrCCCubicShader.h"

#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "glsl/GrGLSLProgramBuilder.h"
#include "glsl/GrGLSLVertexGeoBuilder.h"

using Shader = GrCCCoverageProcessor::Shader;

void GrCCCubicShader::emitSetupCode(GrGLSLVertexGeoBuilder* s, const char* pts,
                                    const char* wind, const char** /*outHull4*/) const {
    // Define a function that normalizes the homogeneous coordinates T=t/s in order to avoid
    // exponent overflow.
    SkString normalizeHomogCoordFn;
    GrShaderVar coord("coord", kFloat2_GrSLType);
    s->emitFunction(kFloat2_GrSLType, "normalize_homogeneous_coord", 1, &coord,
                    s->getProgramBuilder()->shaderCaps()->fpManipulationSupport()
                            // Exponent manipulation version: Scale the exponents so the larger
                            // component has a magnitude in 1..2.
                            // (Neither component should be infinity because ccpr crops big paths.)
                            ? "int exp;"
                              "frexp(max(abs(coord.t), abs(coord.s)), exp);"
                              "return coord * ldexp(1, 1 - exp);"

                            // Division version: Divide by the component with the larger magnitude.
                            // (Both should not be 0 because ccpr catches degenerate cubics.)
                            : "bool swap = abs(coord.t) > abs(coord.s);"
                              "coord = swap ? coord.ts : coord;"
                              "coord = float2(1, coord.t/coord.s);"
                              "return swap ? coord.ts : coord;",

                    &normalizeHomogCoordFn);

    // Find the cubic's power basis coefficients.
    s->codeAppendf("float2x4 C = float4x4(-1,  3, -3,  1, "
                                         " 3, -6,  3,  0, "
                                         "-3,  3,  0,  0, "
                                         " 1,  0,  0,  0) * transpose(%s);", pts);

    // Find the cubic's inflection function.
    s->codeAppend ("float D3 = +determinant(float2x2(C[0].yz, C[1].yz));");
    s->codeAppend ("float D2 = -determinant(float2x2(C[0].xz, C[1].xz));");
    s->codeAppend ("float D1 = +determinant(float2x2(C));");

    // Calculate the KLM matrix.
    s->declareGlobal(fKLMMatrix);
    s->codeAppend ("float discr = 3*D2*D2 - 4*D1*D3;");
    s->codeAppend ("float x = discr >= 0 ? 3 : 1;");
    s->codeAppend ("float q = sqrt(x * abs(discr));");
    s->codeAppend ("q = x*D2 + (D2 >= 0 ? q : -q);");

    s->codeAppend ("float2 l, m;");
    s->codeAppendf("l.ts = %s(float2(q, 2*x * D1));", normalizeHomogCoordFn.c_str());
    s->codeAppendf("m.ts = %s(float2(2, q) * (discr >= 0 ? float2(D3, 1) "
                                                        ": float2(D2*D2 - D3*D1, D1)));",
                                                        normalizeHomogCoordFn.c_str());

    s->codeAppend ("float4 K;");
    s->codeAppend ("float4 lm = l.sstt * m.stst;");
    s->codeAppend ("K = float4(0, lm.x, -lm.y - lm.z, lm.w);");

    s->codeAppend ("float4 L, M;");
    s->codeAppend ("lm.yz += 2*lm.zy;");
    s->codeAppend ("L = float4(-1,x,-x,1) * l.sstt * (discr >= 0 ? l.ssst * l.sttt : lm);");
    s->codeAppend ("M = float4(-1,x,-x,1) * m.sstt * (discr >= 0 ? m.ssst * m.sttt : lm.xzyw);");

    s->codeAppend ("int middlerow = abs(D2) > abs(D1) ? 2 : 1;");
    s->codeAppend ("float3x3 CI = inverse(float3x3(C[0][0], C[0][middlerow], C[0][3], "
                                                  "C[1][0], C[1][middlerow], C[1][3], "
                                                  "      0,               0,       1));");
    s->codeAppendf("%s = CI * float3x3(K[0], K[middlerow], K[3], "
                                      "L[0], L[middlerow], L[3], "
                                      "M[0], M[middlerow], M[3]);", fKLMMatrix.c_str());

    // Evaluate the cubic at T=.5 for a mid-ish point.
    s->codeAppendf("float2 midpoint = %s * float4(.125, .375, .375, .125);", pts);

    // Orient the KLM matrix so L & M are both positive on the side of the curve we wish to fill.
    s->codeAppendf("float2 orientation = sign(float3(midpoint, 1) * float2x3(%s[1], %s[2]));",
                   fKLMMatrix.c_str(), fKLMMatrix.c_str());
    s->codeAppendf("%s *= float3x3(orientation[0] * orientation[1], 0, 0, "
                                  "0, orientation[0], 0, "
                                  "0, 0, orientation[1]);", fKLMMatrix.c_str());

    // Determine the amount of additional coverage to subtract out for the flat edge (P3 -> P0).
    s->declareGlobal(fEdgeDistanceEquation);
    s->codeAppendf("int edgeidx0 = %s > 0 ? 3 : 0;", wind);
    s->codeAppendf("float2 edgept0 = %s[edgeidx0];", pts);
    s->codeAppendf("float2 edgept1 = %s[3 - edgeidx0];", pts);
    Shader::EmitEdgeDistanceEquation(s, "edgept0", "edgept1", fEdgeDistanceEquation.c_str());
}

void GrCCCubicShader::onEmitVaryings(GrGLSLVaryingHandler* varyingHandler,
                                     GrGLSLVarying::Scope scope, SkString* code,
                                     const char* position, const char* coverage,
                                     const char* cornerCoverage) {
    fKLM_fEdge.reset(kFloat4_GrSLType, scope);
    varyingHandler->addVarying("klm_and_edge", &fKLM_fEdge);
    code->appendf("float3 klm = float3(%s, 1) * %s;", position, fKLMMatrix.c_str());
    // We give L & M both the same sign as wind, in order to pass this value to the fragment shader.
    // (Cubics are pre-chopped such that L & M do not change sign within any individual segment.)
    code->appendf("%s.xyz = klm * float3(1, %s, %s);",
                  OutName(fKLM_fEdge), coverage, coverage); // coverage == wind on curves.
    code->appendf("%s.w = dot(float3(%s, 1), %s);", // Flat edge opposite the curve.
                  OutName(fKLM_fEdge), position, fEdgeDistanceEquation.c_str());

    fGradMatrix.reset(kFloat2x2_GrSLType, scope);
    varyingHandler->addVarying("grad_matrix", &fGradMatrix);
    code->appendf("%s[0] = 2*bloat * 3 * klm[0] * %s[0].xy;",
                  OutName(fGradMatrix), fKLMMatrix.c_str());
    code->appendf("%s[1] = -2*bloat * (klm[1] * %s[2].xy + klm[2] * %s[1].xy);",
                    OutName(fGradMatrix), fKLMMatrix.c_str(), fKLMMatrix.c_str());

    if (cornerCoverage) {
        code->appendf("half hull_coverage; {");
        this->calcHullCoverage(code, OutName(fKLM_fEdge), OutName(fGradMatrix), "hull_coverage");
        code->appendf("}");
        fCornerCoverage.reset(kHalf2_GrSLType, scope);
        varyingHandler->addVarying("corner_coverage", &fCornerCoverage);
        code->appendf("%s = half2(hull_coverage, 1) * %s;",
                      OutName(fCornerCoverage), cornerCoverage);
    }
}

void GrCCCubicShader::onEmitFragmentCode(GrGLSLFPFragmentBuilder* f,
                                         const char* outputCoverage) const {
    this->calcHullCoverage(&AccessCodeString(f), fKLM_fEdge.fsIn(), fGradMatrix.fsIn(),
                           outputCoverage);

    // Wind is the sign of both L and/or M. Take the sign of whichever has the larger magnitude.
    // (In reality, either would be fine because we chop cubics with more than a half pixel of
    // padding around the L & M lines, so neither should approach zero.)
    f->codeAppend ("half wind = sign(l + m);");
    f->codeAppendf("%s *= wind;", outputCoverage);

    if (fCornerCoverage.fsIn()) {
        f->codeAppendf("%s = %s.x * %s.y + %s;", // Attenuated corner coverage.
                       outputCoverage, fCornerCoverage.fsIn(), fCornerCoverage.fsIn(),
                       outputCoverage);
    }
}

void GrCCCubicShader::calcHullCoverage(SkString* code, const char* klmAndEdge,
                                       const char* gradMatrix, const char* outputCoverage) const {
    code->appendf("float k = %s.x, l = %s.y, m = %s.z;", klmAndEdge, klmAndEdge, klmAndEdge);
    code->append ("float f = k*k*k - l*m;");
    code->appendf("float2 grad = %s * float2(k, 1);", gradMatrix);
    code->append ("float fwidth = abs(grad.x) + abs(grad.y);");
    code->appendf("%s = min(0.5 - f/fwidth, 1);", outputCoverage); // Curve coverage.
    code->appendf("half d = min(%s.w, 0);", klmAndEdge); // Flat edge opposite the curve.
    code->appendf("%s = max(%s + d, 0);", outputCoverage, outputCoverage); // Total hull coverage.
}
