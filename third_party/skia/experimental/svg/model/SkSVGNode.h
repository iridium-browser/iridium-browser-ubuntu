/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGNode_DEFINED
#define SkSVGNode_DEFINED

#include "SkRefCnt.h"
#include "SkSVGAttribute.h"

class SkCanvas;
class SkMatrix;
class SkSVGRenderContext;
class SkSVGValue;

enum class SkSVGTag {
    kCircle,
    kEllipse,
    kG,
    kLine,
    kPath,
    kPolygon,
    kPolyline,
    kRect,
    kSvg
};

class SkSVGNode : public SkRefCnt {
public:
    virtual ~SkSVGNode();

    SkSVGTag tag() const { return fTag; }

    virtual void appendChild(sk_sp<SkSVGNode>) = 0;

    void render(const SkSVGRenderContext&) const;

    void setAttribute(SkSVGAttribute, const SkSVGValue&);

    void setFill(const SkSVGPaint&);
    void setFillOpacity(const SkSVGNumberType&);
    void setOpacity(const SkSVGNumberType&);
    void setStroke(const SkSVGPaint&);
    void setStrokeOpacity(const SkSVGNumberType&);
    void setStrokeWidth(const SkSVGLength&);

protected:
    SkSVGNode(SkSVGTag);

    // Called before onRender(), to apply local attributes to the context.  Unlike onRender(),
    // onPrepareToRender() bubbles up the inheritance chain: overriders should always call
    // INHERITED::onPrepareToRender(), unless they intend to short-circuit rendering
    // (return false).
    // Implementations are expected to return true if rendering is to continue, or false if
    // the node/subtree rendering is disabled.
    virtual bool onPrepareToRender(SkSVGRenderContext*) const;

    virtual void onRender(const SkSVGRenderContext&) const = 0;

    virtual void onSetAttribute(SkSVGAttribute, const SkSVGValue&);

private:
    SkSVGTag                    fTag;

    // FIXME: this should be sparse
    SkSVGPresentationAttributes fPresentationAttributes;

    typedef SkRefCnt INHERITED;
};

#endif // SkSVGNode_DEFINED
