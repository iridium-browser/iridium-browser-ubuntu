/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrAAFillRectBatch_DEFINED
#define GrAAFillRectBatch_DEFINED

#include "GrColor.h"

class GrDrawBatch;
class SkMatrix;
struct SkRect;

namespace GrAAFillRectBatch {
GrDrawBatch* Create(GrColor color,
                    const SkMatrix& viewMatrix,
                    const SkRect& rect,
                    const SkRect& devRect);

GrDrawBatch* Create(GrColor color,
                    const SkMatrix& viewMatrix,
                    const SkMatrix& localMatrix,
                    const SkRect& rect,
                    const SkRect& devRect);
};

#endif
