/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGAttribute_DEFINED
#define SkSVGAttribute_DEFINED

#include "SkSVGTypes.h"
#include "SkTLazy.h"

class SkSVGRenderContext;

enum class SkSVGAttribute {
    kCx, // <circle>,<ellipse>: center x position
    kCy, // <circle>,<ellipse>: center y position
    kD,
    kFill,
    kFillOpacity,
    kHeight,
    kOpacity,
    kPoints,
    kR,  // <circle>: radius
    kRx, // <ellipse>,<rect>: horizontal (corner) radius
    kRy, // <ellipse>,<rect>: vertical (corner) radius
    kStroke,
    kStrokeOpacity,
    kStrokeLineCap,
    kStrokeLineJoin,
    kStrokeWidth,
    kTransform,
    kViewBox,
    kWidth,
    kX,
    kX1, // <line>: first endpoint x
    kX2, // <line>: second endpoint x
    kY,
    kY1, // <line>: first endpoint y
    kY2, // <line>: second endpoint y

    kUnknown,
};

struct SkSVGPresentationAttributes {
    static SkSVGPresentationAttributes MakeInitial();

    // TODO: SkTLazy adds an extra ptr per attribute; refactor to reduce overhead.

    SkTLazy<SkSVGPaint>      fFill;
    SkTLazy<SkSVGNumberType> fFillOpacity;

    SkTLazy<SkSVGPaint>      fStroke;
    SkTLazy<SkSVGLineCap>    fStrokeLineCap;
    SkTLazy<SkSVGLineJoin>   fStrokeLineJoin;
    SkTLazy<SkSVGNumberType> fStrokeOpacity;
    SkTLazy<SkSVGLength>     fStrokeWidth;

    SkTLazy<SkSVGNumberType> fOpacity;
};

#endif // SkSVGAttribute_DEFINED
