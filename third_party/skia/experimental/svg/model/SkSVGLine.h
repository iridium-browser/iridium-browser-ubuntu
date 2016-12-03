/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGLine_DEFINED
#define SkSVGLine_DEFINED

#include "SkSVGShape.h"
#include "SkSVGTypes.h"

class SkSVGLine final : public SkSVGShape {
public:
    virtual ~SkSVGLine() = default;
    static sk_sp<SkSVGLine> Make() { return sk_sp<SkSVGLine>(new SkSVGLine()); }

    void setX1(const SkSVGLength&);
    void setY1(const SkSVGLength&);
    void setX2(const SkSVGLength&);
    void setY2(const SkSVGLength&);

protected:
    void onSetAttribute(SkSVGAttribute, const SkSVGValue&) override;

    void onDraw(SkCanvas*, const SkSVGLengthContext&, const SkPaint&) const override;

private:
    SkSVGLine();

    SkSVGLength fX1 = SkSVGLength(0);
    SkSVGLength fY1 = SkSVGLength(0);
    SkSVGLength fX2 = SkSVGLength(0);
    SkSVGLength fY2 = SkSVGLength(0);

    typedef SkSVGShape INHERITED;
};

#endif // SkSVGLine_DEFINED
