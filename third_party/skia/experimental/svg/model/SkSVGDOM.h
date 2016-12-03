/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGDOM_DEFINED
#define SkSVGDOM_DEFINED

#include "SkRefCnt.h"
#include "SkSize.h"
#include "SkTemplates.h"

class SkCanvas;
class SkDOM;
class SkStream;
class SkSVGNode;

class SkSVGDOM : public SkRefCnt {
public:
    SkSVGDOM(const SkSize& containerSize);
    ~SkSVGDOM() = default;

    static sk_sp<SkSVGDOM> MakeFromDOM(const SkDOM&, const SkSize& containerSize);
    static sk_sp<SkSVGDOM> MakeFromStream(SkStream&, const SkSize& containerSize);

    void setContainerSize(const SkSize&);
    void setRoot(sk_sp<SkSVGNode>);

    void render(SkCanvas*) const;

private:
    SkSize           fContainerSize;
    sk_sp<SkSVGNode> fRoot;

    typedef SkRefCnt INHERITED;
};

#endif // SkSVGDOM_DEFINED
