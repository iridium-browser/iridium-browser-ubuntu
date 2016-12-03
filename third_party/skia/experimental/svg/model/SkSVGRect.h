/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGRect_DEFINED
#define SkSVGRect_DEFINED

#include "SkSVGShape.h"
#include "SkSVGTypes.h"

class SkSVGRect final : public SkSVGShape {
public:
    virtual ~SkSVGRect() = default;
    static sk_sp<SkSVGRect> Make() { return sk_sp<SkSVGRect>(new SkSVGRect()); }

    void setX(const SkSVGLength&);
    void setY(const SkSVGLength&);
    void setWidth(const SkSVGLength&);
    void setHeight(const SkSVGLength&);
    void setRx(const SkSVGLength&);
    void setRy(const SkSVGLength&);

protected:
    void onSetAttribute(SkSVGAttribute, const SkSVGValue&) override;

    void onDraw(SkCanvas*, const SkSVGLengthContext&, const SkPaint&) const override;

private:
    SkSVGRect();

    SkSVGLength fX      = SkSVGLength(0);
    SkSVGLength fY      = SkSVGLength(0);
    SkSVGLength fWidth  = SkSVGLength(0);
    SkSVGLength fHeight = SkSVGLength(0);

    // The x radius for rounded rects.
    SkSVGLength fRx     = SkSVGLength(0);
    // The y radius for rounded rects.
    SkSVGLength fRy     = SkSVGLength(0);

    typedef SkSVGShape INHERITED;
};

#endif // SkSVGRect_DEFINED
