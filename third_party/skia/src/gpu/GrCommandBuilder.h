/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrCommandBuilder_DEFINED
#define GrCommandBuilder_DEFINED

#include "GrTargetCommands.h"

class GrGpu;
class GrResourceProvider;
class GrBufferedDrawTarget;

class GrCommandBuilder : ::SkNoncopyable {
public:
    typedef GrTargetCommands::Cmd               Cmd;
    typedef GrTargetCommands::StateForPathDraw  State;

    static GrCommandBuilder* Create(GrGpu* gpu, bool reorder);

    virtual ~GrCommandBuilder() {}

    void reset() { fCommands.reset(); }
    void flush(GrGpu* gpu, GrResourceProvider* rp) { fCommands.flush(gpu, rp); }

    virtual Cmd* recordDrawBatch(GrBatch*, const GrCaps&) = 0;
    virtual Cmd* recordDrawPaths(State*,
                                 GrBufferedDrawTarget*,
                                 const GrPathProcessor*,
                                 const GrPathRange*,
                                 const void*,
                                 GrDrawTarget::PathIndexType,
                                 const float transformValues[],
                                 GrDrawTarget::PathTransformType ,
                                 int,
                                 const GrStencilSettings&,
                                 const GrPipelineOptimizations&) = 0;

protected:
    typedef GrTargetCommands::DrawBatch DrawBatch;
    typedef GrTargetCommands::DrawPath DrawPath;
    typedef GrTargetCommands::DrawPaths DrawPaths;

    GrCommandBuilder() {}

    GrTargetCommands::CmdBuffer* cmdBuffer() { return fCommands.cmdBuffer(); }
private:
    GrTargetCommands fCommands;

};

#endif
