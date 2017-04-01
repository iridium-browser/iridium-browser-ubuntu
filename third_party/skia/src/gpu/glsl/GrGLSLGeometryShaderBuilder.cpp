/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrGLSLGeometryShaderBuilder.h"
#include "GrGLSLProgramBuilder.h"
#include "GrGLSLVarying.h"

static const char* input_type_name(GrGLSLGeometryBuilder::InputType in) {
    using InputType = GrGLSLGeometryBuilder::InputType;
    switch (in) {
        case InputType::kPoints: return "points";
        case InputType::kLines: return "lines";
        case InputType::kLinesAdjacency: return "lines_adjacency";
        case InputType::kTriangles: return "triangles";
        case InputType::kTrianglesAdjacency: return "triangles_adjacency";
    }
    SkFAIL("invalid input type");
    return "unknown_input";
}

static const char* output_type_name(GrGLSLGeometryBuilder::OutputType out) {
    using OutputType = GrGLSLGeometryBuilder::OutputType;
    switch (out) {
        case OutputType::kPoints: return "points";
        case OutputType::kLineStrip: return "line_strip";
        case OutputType::kTriangleStrip: return "triangle_strip";
    }
    SkFAIL("invalid output type");
    return "unknown_output";
}

GrGLSLGeometryBuilder::GrGLSLGeometryBuilder(GrGLSLProgramBuilder* program)
    : INHERITED(program)
    , fIsConfigured(false) {
}

void GrGLSLGeometryBuilder::configure(InputType inputType, OutputType outputType, int maxVertices,
                                      int numInvocations) {
    SkASSERT(!fIsConfigured);
    this->addLayoutQualifier(input_type_name(inputType), kIn_InterfaceQualifier);
    this->addLayoutQualifier(SkStringPrintf("invocations = %i", numInvocations).c_str(),
                             kIn_InterfaceQualifier);
    this->addLayoutQualifier(output_type_name(outputType), kOut_InterfaceQualifier);
    this->addLayoutQualifier(SkStringPrintf("max_vertices = %i", maxVertices).c_str(),
                             kOut_InterfaceQualifier);
    fIsConfigured = true;
}

void GrGLSLGeometryBuilder::onFinalize() {
    SkASSERT(fIsConfigured);
    fProgramBuilder->varyingHandler()->getGeomDecls(&this->inputs(), &this->outputs());
}
