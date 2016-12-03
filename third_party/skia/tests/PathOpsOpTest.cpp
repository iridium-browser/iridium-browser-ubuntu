/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "PathOpsExtendedTest.h"
#include "PathOpsTestCommon.h"

class PathTest_Private {
public:
    PathTest_Private(SkPath* path)
        : fPath(path) {}

    void setPt(int index, SkScalar x, SkScalar y) {
        fPath->setPt(index, x, y);
    }

    SkPath* fPath;
};

static void path_edit(const SkPoint& from, const SkPoint& to, SkPath* path) {
    PathTest_Private testPath(path);
    for (int index = 0; index < path->countPoints(); ++index) {
        if (SkDPoint::ApproximatelyEqual(path->getPoint(index), from)) {
            testPath.setPt(index, to.fX, to.fY);
            return;
        }
    }
}

static void cubicOp1d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,2, 1,0, 1,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(0,1, 1,0, 2,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp2d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(0,1, 1,0, 1,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(0,1, 2,0, 1,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp3d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,3, 1,0, 1,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(0,1, 1,0, 3,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp5d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,2, 1,0, 2,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(0,2, 1,0, 2,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp6d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,6, 1,0, 3,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(0,3, 1,0, 6,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp7d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,4, 1,0, 3,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(0,3, 1,0, 4,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp8d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,5, 1,0, 4,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(0,4, 1,0, 5,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp9d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,6, 1,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(1,2, 1,0, 6,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void quadOp9d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.quadTo(1,6, 1.5f,1);
    path.quadTo(1.5f,0.5f, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.quadTo(1,2, 1.4f,1);
    pathB.quadTo(3,0.4f, 6,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void lineOp9d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.lineTo(1,6);
    path.lineTo(1.5f,1);
    path.lineTo(1.8f,0.8f);
    path.lineTo(2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.lineTo(1,2);
    pathB.lineTo(1.4f,1);
    pathB.lineTo(3,0.4f);
    pathB.lineTo(6,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp1i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,2, 1,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(1,2, 1,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp10d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,3, 1,0, 4,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(1,4, 1,0, 3,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp11d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,4, 1,0, 5,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(1,5, 1,0, 4,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp12d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,6, 1,0, 1,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(0,1, 1,0, 6,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp13d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(4,5, 1,0, 5,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(3,5, 1,0, 5,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp14d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,2, 2,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(1,2, 1,0, 2,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp15d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,6, 2,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(1,2, 1,0, 6,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp16d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(0,1, 3,0, 1,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,3);
    pathB.cubicTo(0,1, 2,0, 1,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp17d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(0,2, 4,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(1,2, 2,0, 2,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp18d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,5, 2,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(1,2, 1,0, 5,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp19i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(0,1, 2,1, 6,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(2,6, 2,0, 1,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp20d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,1, 6,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(1,2, 1,0, 1,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp21d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,1, 2,1, 6,5);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(5,6, 1,0, 1,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp22d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,3, 3,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,3);
    pathB.cubicTo(1,2, 1,0, 3,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp23d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,2, 4,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(1,2, 1,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp24d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,2, 2,0, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(2,3, 1,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void testIntersect1(skiatest::Reporter* reporter, const char* filename) {
    SkPath one, two;
    one.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    two.addRect(3, 3, 9, 9, SkPath::kCW_Direction);
    testPathOp(reporter, one, two, kIntersect_SkPathOp, filename);
}

static void testUnion1(skiatest::Reporter* reporter, const char* filename) {
    SkPath one, two;
    one.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    two.addRect(3, 3, 9, 9, SkPath::kCW_Direction);
    testPathOp(reporter, one, two, kUnion_SkPathOp, filename);
}

static void testDiff1(skiatest::Reporter* reporter, const char* filename) {
    SkPath one, two;
    one.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    two.addRect(3, 3, 9, 9, SkPath::kCW_Direction);
    testPathOp(reporter, one, two, kDifference_SkPathOp, filename);
}

static void testXor1(skiatest::Reporter* reporter, const char* filename) {
    SkPath one, two;
    one.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    two.addRect(3, 3, 9, 9, SkPath::kCW_Direction);
    testPathOp(reporter, one, two, kXOR_SkPathOp, filename);
}

static void testIntersect2(skiatest::Reporter* reporter, const char* filename) {
    SkPath one, two;
    one.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    two.addRect(0, 3, 9, 9, SkPath::kCW_Direction);
    testPathOp(reporter, one, two, kIntersect_SkPathOp, filename);
}

static void testUnion2(skiatest::Reporter* reporter, const char* filename) {
    SkPath one, two;
    one.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    two.addRect(0, 3, 9, 9, SkPath::kCW_Direction);
    testPathOp(reporter, one, two, kUnion_SkPathOp, filename);
}

static void testDiff2(skiatest::Reporter* reporter, const char* filename) {
    SkPath one, two;
    one.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    two.addRect(0, 3, 9, 9, SkPath::kCW_Direction);
    testPathOp(reporter, one, two, kDifference_SkPathOp, filename);
}

static void testXor2(skiatest::Reporter* reporter, const char* filename) {
    SkPath one, two;
    one.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    two.addRect(0, 3, 9, 9, SkPath::kCW_Direction);
    testPathOp(reporter, one, two, kXOR_SkPathOp, filename);
}

static void testOp1d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void testOp2d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void testOp3d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(1, 1, 2, 2, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void testOp1u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(0, 0, 3, 3, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void testOp4d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(2, 2, 4, 4, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void testOp5d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    path.addRect(0, 0, 3, 3, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void testOp6d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(0, 0, 3, 3, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void testOp7d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void testOp2u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    path.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 3, 3, SkPath::kCW_Direction);
    pathB.addRect(1, 1, 2, 2, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void testOp8d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.addRect(0, 0, 640, 480);
    pathB.moveTo(577330, 1971.72f);
    pathB.cubicTo(10.7082f, -116.596f, 262.057f, 45.6468f, 294.694f, 1.96237f);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}
static void cubicOp25i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,4, 5,0, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(2,3, 1,0, 4,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp26d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,4, 4,0, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(2,3, 1,0, 4,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp27d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,6, 1,0, 5,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(2,5, 1,0, 6,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp28u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,4, 6,0, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(2,3, 1,0, 4,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp29d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,5, 6,0, 4,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(2,4, 1,0, 5,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp30d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,5, 6,0, 5,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(3,5, 1,0, 5,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp31d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(0,3, 2,1, 4,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(0,4, 2,0, 3,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp31u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(0,3, 2,1, 4,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(0,4, 2,0, 3,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp31x(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(0,3, 2,1, 4,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(0,4, 2,0, 3,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kXOR_SkPathOp, filename);
}

static void cubicOp32d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,2, 6,0, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(1,3, 1,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp33i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,2, 6,0, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(1,3, 1,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp34d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,5, 2,1, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(1,3, 1,0, 5,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp35d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,5, 2,1, 4,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(0,4, 1,0, 5,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp36u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,6, 2,0, 5,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(1,5, 1,0, 6,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp37d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,6, 6,1, 4,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,6);
    pathB.cubicTo(3,4, 1,0, 6,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp38d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,6, 3,2, 4,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,3);
    pathB.cubicTo(1,4, 1,0, 6,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp39d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,3, 5,1, 4,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,5);
    pathB.cubicTo(3,4, 1,0, 3,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp40d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,5, 3,2, 4,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,3);
    pathB.cubicTo(2,4, 1,0, 5,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp41i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,6, 4,3, 6,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3,4);
    pathB.cubicTo(4,6, 1,0, 6,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp42d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,2, 6,5, 5,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(5,6);
    pathB.cubicTo(4,5, 1,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp43d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(1,2, 4,0, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(1,3, 2,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp44d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(3,6, 4,0, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(2,3, 2,0, 6,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp45d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(2,4, 4,0, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(2,3, 2,0, 4,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp46d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(3,5, 5,0, 4,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(2,4, 2,0, 5,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp47d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,6, 6,2, 5,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,6);
    pathB.cubicTo(4,5, 1,0, 6,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp48d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(2,3, 5,1, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,5);
    pathB.cubicTo(2,3, 2,0, 3,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp49d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(1,5, 3,2, 4,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,3);
    pathB.cubicTo(1,4, 2,0, 5,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp50d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,3);
    path.cubicTo(1,6, 5,0, 5,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(1,5, 3,0, 6,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp51d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,3);
    path.cubicTo(1,2, 4,1, 6,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,4);
    pathB.cubicTo(0,6, 3,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp52d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(1,2, 5,4, 4,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(4,5);
    pathB.cubicTo(3,4, 2,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp53d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,3);
    path.cubicTo(1,2, 5,3, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3,5);
    pathB.cubicTo(1,2, 3,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp54d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,4);
    path.cubicTo(1,3, 5,4, 4,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(4,5);
    pathB.cubicTo(2,4, 4,0, 3,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp55d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,5);
    path.cubicTo(1,3, 3,2, 5,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,3);
    pathB.cubicTo(0,5, 5,0, 3,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp56d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,6, 5,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(1,2, 1,0, 6,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp57d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,5);
    path.cubicTo(0,5, 5,4, 6,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(4,5);
    pathB.cubicTo(4,6, 5,0, 5,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp58d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,5);
    path.cubicTo(3,4, 6,5, 5,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(5,6);
    pathB.cubicTo(3,5, 5,0, 4,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp59d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(5,6, 4,0, 4,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(1,4, 1,0, 6,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp60d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(4,6, 6,0, 5,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(2,5, 2,0, 6,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp61d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1,2);
    path.cubicTo(0,5, 3,2, 6,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,3);
    pathB.cubicTo(1,6, 2,1, 5,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp62d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1,3);
    path.cubicTo(5,6, 5,3, 5,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3,5);
    pathB.cubicTo(4,5, 3,1, 6,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp63d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2,3);
    path.cubicTo(0,4, 3,2, 5,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,3);
    pathB.cubicTo(3,5, 3,2, 4,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp64d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(0,1, 1,0, 3,0);
    path.lineTo(0,1);
    path.close();
    pathB.moveTo(0,1);
    pathB.cubicTo(0,3, 1,0, 1,0);
    pathB.lineTo(0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp65d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(1,5, 1,0, 1,0);
    path.lineTo(0,1);
    path.close();
    pathB.moveTo(0,1);
    pathB.cubicTo(0,1, 1,0, 5,1);
    pathB.lineTo(0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void rectOp1d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(0,1, 1,0, 3,0);
    path.lineTo(0,1);
    path.close();
    pathB.moveTo(0,1);
    pathB.cubicTo(0,3, 1,0, 1,0);
    pathB.lineTo(0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp66u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,6, 4,2, 5,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,4);
    pathB.cubicTo(3,5, 1,0, 6,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp67u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(3,5);
    path.cubicTo(1,6, 5,0, 3,1);
    path.lineTo(3,5);
    path.close();
    pathB.moveTo(0,5);
    pathB.cubicTo(1,3, 5,3, 6,1);
    pathB.lineTo(0,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp68u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,5);
    path.cubicTo(4,5, 4,1, 5,0);
    path.close();
    pathB.moveTo(1,4);
    pathB.cubicTo(0,5, 5,0, 5,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp69d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(1,3);
    path.cubicTo(0,1, 3,1, 2,0);
    path.close();
    pathB.moveTo(1,3);
    pathB.cubicTo(0,2, 3,1, 1,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

SkPathOp ops[] = {
    kUnion_SkPathOp,
    kXOR_SkPathOp,
    kReverseDifference_SkPathOp,
    kXOR_SkPathOp,
    kReverseDifference_SkPathOp,
};

static void rRect1(skiatest::Reporter* reporter, const char* filename) {
    SkScalar xA = 0.65f;
    SkScalar xB = 10.65f;
    SkScalar xC = 20.65f;
    SkScalar xD = 30.65f;
    SkScalar xE = 40.65f;
    SkScalar xF = 50.65f;

    SkScalar yA = 0.65f;
    SkScalar yB = 10.65f;
    SkScalar yC = 20.65f;
    SkScalar yD = 30.65f;
    SkScalar yE = 40.65f;
    SkScalar yF = 50.65f;
    SkPath paths[5];
    SkRect rects[5];
    rects[0].set(xB, yB, xE, yE);
    paths[0].addRoundRect(rects[0], SkIntToScalar(5), SkIntToScalar(5));  // red
    rects[1].set(xA, yA, xD, yD);
    paths[1].addRoundRect(rects[1], SkIntToScalar(5), SkIntToScalar(5));  // green
    rects[2].set(xC, yA, xF, yD);
    paths[2].addRoundRect(rects[2], SkIntToScalar(5), SkIntToScalar(5));  // blue
    rects[3].set(xA, yC, xD, yF);
    paths[3].addRoundRect(rects[3], SkIntToScalar(5), SkIntToScalar(5));  // yellow
    rects[4].set(xC, yC, xF, yF);
    paths[4].addRoundRect(rects[4], SkIntToScalar(5), SkIntToScalar(5));  // cyan
    SkPath path;
    path.setFillType(SkPath::kInverseEvenOdd_FillType);
    for (int index = 0; index < 5; ++index) {
        testPathOp(reporter, path, paths[index], ops[index], filename);
        REPORTER_ASSERT(reporter, Op(path, paths[index], ops[index], &path));
    }
}

static void skp1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(189,7);
    path.cubicTo(189,5.34314585f, 190.34314f,4, 192,4);
    path.lineTo(243,4);
    path.cubicTo(244.65686f,4, 246,5.34314585f, 246,7);
    path.lineTo(246,21);
    path.cubicTo(246,22.6568546f, 244.65686f,24, 243,24);
    path.lineTo(192,24);
    path.cubicTo(190.34314f,24, 189,22.6568546f, 189,21);
    path.lineTo(189,7);
    path.close();
    path.moveTo(191,8);
    path.cubicTo(191,6.89543009f, 191.895432f,6, 193,6);
    path.lineTo(242,6);
    path.cubicTo(243.104568f,6, 244,6.89543009f, 244,8);
    path.lineTo(244,20);
    path.cubicTo(244,21.1045704f, 243.104568f,22, 242,22);
    path.lineTo(193,22);
    path.cubicTo(191.895432f,22, 191,21.1045704f, 191,20);
    path.lineTo(191,8);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(189,4);
    pathB.lineTo(199,14);
    pathB.lineTo(236,14);
    pathB.lineTo(246,4);
    pathB.lineTo(189,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skp2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(253.000000f, 11757.0000f);
    path.lineTo(253.000000f, 222.000000f);
    path.lineTo(823.000000f, 222.000000f);
    path.lineTo(823.000000f, 11757.0000f);
    path.lineTo(253.000000f, 11757.0000f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(258.000000f, 1028.00000f);
    pathB.lineTo(258.000000f, 1027.00000f);
    pathB.lineTo(823.000000f, 1027.00000f);
    pathB.lineTo(823.000000f, 1028.00000f);
    pathB.lineTo(258.000000f, 1028.00000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skp3(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(717.000000f, 507.000000f);
    path.lineTo(717.000000f, 425.000000f);
    path.lineTo(973.000000f, 425.000000f);
    path.lineTo(973.000000f, 507.000000f);
    path.quadTo(973.000000f, 508.242645f, 972.121582f, 509.121613f);
    path.quadTo(971.242615f, 510.000000f, 970.000000f, 510.000000f);
    path.lineTo(720.000000f, 510.000000f);
    path.quadTo(718.757385f, 510.000000f, 717.878418f, 509.121613f);
    path.quadTo(717.000000f, 508.242645f, 717.000000f, 507.000000f);
    path.close();
    path.moveTo(719.000000f, 426.000000f);
    path.lineTo(971.000000f, 426.000000f);
    path.lineTo(971.000000f, 506.000000f);
    path.cubicTo(971.000000f, 507.104584f, 970.104553f, 508.000000f, 969.000000f, 508.000000f);
    path.lineTo(721.000000f, 508.000000f);
    path.cubicTo(719.895447f, 508.000000f, 719.000000f, 507.104584f, 719.000000f, 506.000000f);
    path.lineTo(719.000000f, 426.000000f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(717.000000f, 510.000000f);
    pathB.lineTo(760.000000f, 467.000000f);
    pathB.lineTo(930.000000f, 467.000000f);
    pathB.lineTo(973.000000f, 510.000000f);
    pathB.lineTo(717.000000f, 510.000000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skp4(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(230.756805f, 591.756775f);
    path.quadTo(232.514725f, 590.000000f, 235.000000f, 590.000000f);
    path.lineTo(300.000000f, 590.000000f);
    path.quadTo(302.485291f, 590.000000f, 304.243195f, 591.756775f);
    path.quadTo(306.000000f, 593.514709f, 306.000000f, 596.000000f);
    path.lineTo(306.000000f, 617.000000f);
    path.lineTo(229.000000f, 617.000000f);
    path.lineTo(229.000000f, 596.000000f);
    path.quadTo(229.000000f, 593.514709f, 230.756805f, 591.756775f);
    path.close();
    path.moveTo(231.000000f, 597.000000f);
    path.cubicTo(231.000000f, 594.238586f, 233.238571f, 592.000000f, 236.000000f, 592.000000f);
    path.lineTo(299.000000f, 592.000000f);
    path.cubicTo(301.761414f, 592.000000f, 304.000000f, 594.238586f, 304.000000f, 597.000000f);
    path.lineTo(304.000000f, 616.000000f);
    path.lineTo(231.000000f, 616.000000f);
    path.lineTo(231.000000f, 597.000000f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(306.000000f, 590.000000f);
    pathB.lineTo(292.000000f, 604.000000f);
    pathB.lineTo(305.000000f, 617.000000f);
    pathB.lineTo(306.000000f, 617.000000f);
    pathB.lineTo(306.000000f, 590.000000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skp5(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(18.0000000f, 226.000000f);
    path.quadTo(14.6862917f, 226.000000f, 12.3423996f, 228.342407f);
    path.quadTo(10.0000000f, 230.686295f, 10.0000000f, 234.000000f);
    path.lineTo(10.0000000f, 253.000000f);
    path.lineTo(1247.00000f, 253.000000f);
    path.lineTo(1247.00000f, 234.000000f);
    path.quadTo(1247.00000f, 230.686295f, 1244.65759f, 228.342407f);
    path.quadTo(1242.31372f, 226.000000f, 1239.00000f, 226.000000f);
    path.lineTo(18.0000000f, 226.000000f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kInverseWinding_FillType);
    pathB.moveTo(18.0000000f, 226.000000f);
    pathB.lineTo(1239.00000f, 226.000000f);
    pathB.cubicTo(1243.41833f, 226.000000f, 1247.00000f, 229.581726f, 1247.00000f, 234.000000f);
    pathB.lineTo(1247.00000f, 252.000000f);
    pathB.lineTo(10.0000000f, 252.000000f);
    pathB.lineTo(10.0000000f, 234.000000f);
    pathB.cubicTo(10.0000000f, 229.581726f, 13.5817204f, 226.000000f, 18.0000000f, 226.000000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp70d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,5, 4,0, 5,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(0,5, 1,0, 5,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp71d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,5, 4,1, 6,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,4);
    pathB.cubicTo(4,6, 1,0, 5,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp72i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,5, 5,2, 5,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,5);
    pathB.cubicTo(4,5, 1,0, 5,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp73d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,4, 4,0, 6,4);
    path.lineTo(0,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(4,6, 1,0, 4,3);
    pathB.lineTo(0,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp74d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,5, 5,1, 5,1);
    path.lineTo(0,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,5);
    pathB.cubicTo(1,5, 1,0, 5,1);
    pathB.lineTo(1,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp75d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,4, 5,1, 6,4);
    path.lineTo(0,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,5);
    pathB.cubicTo(4,6, 1,0, 4,0);
    pathB.lineTo(1,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp76u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,2, 2,0, 5,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(3,5, 1,0, 2,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp77i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,3, 2,0, 3,2);
    path.lineTo(0,1);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(2,3, 1,0, 3,1);
    pathB.lineTo(0,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp78u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1,6);
    path.cubicTo(1,6, 5,0, 6,1);
    path.lineTo(1,6);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(1,6, 6,1, 6,1);
    pathB.lineTo(0,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp79u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,3, 1,0, 6,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(4,6, 1,0, 3,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp80i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,3, 2,1, 4,3);
    path.lineTo(0,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(3,4, 1,0, 3,2);
    pathB.lineTo(1,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp81d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(4,6, 4,3, 5,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3,4);
    pathB.cubicTo(4,5, 1,0, 6,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp82i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,3, 5,2, 3,0);
    path.lineTo(0,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,5);
    pathB.cubicTo(0,3, 1,0, 3,2);
    pathB.lineTo(2,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp83i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,3, 2,1, 4,1);
    path.lineTo(0,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(1,4, 1,0, 3,0);
    pathB.lineTo(1,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp84d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,4);
    path.cubicTo(2,3, 6,3, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3,6);
    pathB.cubicTo(2,3, 4,0, 3,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void skpClip1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1126.17114f, 877.171204f);
    path.quadTo(1127.34314f, 876.000000f, 1129.00000f, 876.000000f);
    path.lineTo(1243.00000f, 876.000000f);
    path.quadTo(1244.65686f, 876.000000f, 1245.82886f, 877.171204f);
    path.quadTo(1247.00000f, 878.343140f, 1247.00000f, 880.000000f);
    path.lineTo(1247.00000f, 907.000000f);
    path.lineTo(1246.00000f, 907.000000f);
    path.lineTo(1246.00000f, 880.000000f);
    path.cubicTo(1246.00000f, 878.343140f, 1244.65686f, 877.000000f, 1243.00000f, 877.000000f);
    path.lineTo(1129.00000f, 877.000000f);
    path.cubicTo(1127.34314f, 877.000000f, 1126.00000f, 878.343140f, 1126.00000f, 880.000000f);
    path.lineTo(1126.00000f, 907.000000f);
    path.lineTo(1125.00000f, 907.000000f);
    path.lineTo(1125.00000f, 880.000000f);
    path.quadTo(1125.00000f, 878.343140f, 1126.17114f, 877.171204f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1247.00000f, 876.000000f);
    pathB.lineTo(1231.00000f, 892.000000f);
    pathB.lineTo(1246.00000f, 907.000000f);
    pathB.lineTo(1247.00000f, 907.000000f);
    pathB.lineTo(1247.00000f, 876.000000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpClip2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(134.000000f, 11414.0000f);
    path.cubicTo(131.990234f, 11414.0000f, 130.326660f, 11415.4824f, 130.042755f, 11417.4131f);
    path.cubicTo(130.233124f, 11418.3193f, 131.037079f, 11419.0000f, 132.000000f, 11419.0000f);
    path.lineTo(806.000000f, 11419.0000f);
    path.cubicTo(806.962891f, 11419.0000f, 807.766907f, 11418.3193f, 807.957275f, 11417.4131f);
    path.cubicTo(807.673401f, 11415.4824f, 806.009766f, 11414.0000f, 804.000000f, 11414.0000f);
    path.lineTo(134.000000f, 11414.0000f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kInverseWinding_FillType);
    pathB.moveTo(132.000000f, 11415.0000f);
    pathB.lineTo(806.000000f, 11415.0000f);
    pathB.cubicTo(807.104553f, 11415.0000f, 808.000000f, 11415.4473f, 808.000000f, 11416.0000f);
    pathB.lineTo(808.000000f, 11417.0000f);
    pathB.cubicTo(808.000000f, 11418.1045f, 807.104553f, 11419.0000f, 806.000000f, 11419.0000f);
    pathB.lineTo(132.000000f, 11419.0000f);
    pathB.cubicTo(130.895432f, 11419.0000f, 130.000000f, 11418.1045f, 130.000000f, 11417.0000f);
    pathB.lineTo(130.000000f, 11416.0000f);
    pathB.cubicTo(130.000000f, 11415.4473f, 130.895432f, 11415.0000f, 132.000000f, 11415.0000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skp96prezzi1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(157.464005f, 670.463989f);
    path.quadTo(158.928925f, 669.000000f, 161.000000f, 669.000000f);
    path.lineTo(248.000000f, 669.000000f);
    path.quadTo(250.071075f, 669.000000f, 251.535995f, 670.463989f);
    path.quadTo(253.000000f, 671.928955f, 253.000000f, 674.000000f);
    path.lineTo(253.000000f, 706.000000f);
    path.lineTo(251.000000f, 706.000000f);
    path.lineTo(251.000000f, 675.000000f);
    path.cubicTo(251.000000f, 672.790833f, 249.209137f, 671.000000f, 247.000000f, 671.000000f);
    path.lineTo(162.000000f, 671.000000f);
    path.cubicTo(159.790863f, 671.000000f, 158.000000f, 672.790833f, 158.000000f, 675.000000f);
    path.lineTo(158.000000f, 706.000000f);
    path.lineTo(156.000000f, 706.000000f);
    path.lineTo(156.000000f, 674.000000f);
    path.quadTo(156.000000f, 671.928955f, 157.464005f, 670.463989f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(156.000000f, 669.000000f);
    pathB.lineTo(178.500000f, 691.500000f);
    pathB.lineTo(230.500000f, 691.500000f);
    pathB.lineTo(253.000000f, 669.000000f);
    pathB.lineTo(156.000000f, 669.000000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpancestry_com1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(161.000000f, 925.000000f);
    path.cubicTo(159.874390f, 925.000000f, 158.835663f, 925.371948f, 158.000000f, 925.999634f);
    path.lineTo(158.000000f, 926.000000f);
    path.lineTo(1108.00000f, 926.000000f);
    path.lineTo(1108.00000f, 925.999634f);
    path.cubicTo(1107.16443f, 925.371948f, 1106.12561f, 925.000000f, 1105.00000f, 925.000000f);
    path.lineTo(161.000000f, 925.000000f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(161.000000f, 926.000000f);
    pathB.lineTo(1105.00000f, 926.000000f);
    pathB.cubicTo(1107.20911f, 926.000000f, 1109.00000f, 927.790833f, 1109.00000f, 930.000000f);
    pathB.lineTo(1109.00000f, 956.000000f);
    pathB.cubicTo(1109.00000f, 958.209167f, 1107.20911f, 960.000000f, 1105.00000f, 960.000000f);
    pathB.lineTo(161.000000f, 960.000000f);
    pathB.cubicTo(158.790863f, 960.000000f, 157.000000f, 958.209167f, 157.000000f, 956.000000f);
    pathB.lineTo(157.000000f, 930.000000f);
    pathB.cubicTo(157.000000f, 927.790833f, 158.790863f, 926.000000f, 161.000000f, 926.000000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpeldorado_com_ua1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(286.695129f, 291.000000f);
    path.lineTo(229.304855f, 561.000000f);
    path.lineTo(979.304871f, 561.000000f);
    path.lineTo(1036.69507f, 291.000000f);
    path.lineTo(286.695129f, 291.000000f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1006.69513f, 291.000000f);
    pathB.cubicTo(1023.26367f, 291.000000f, 1033.84021f, 304.431458f, 1030.31836f, 321.000000f);
    pathB.lineTo(985.681519f, 531.000000f);
    pathB.cubicTo(982.159790f, 547.568542f, 965.873413f, 561.000000f, 949.304871f, 561.000000f);
    pathB.lineTo(259.304871f, 561.000000f);
    pathB.cubicTo(242.736313f, 561.000000f, 232.159805f, 547.568542f, 235.681549f, 531.000000f);
    pathB.lineTo(280.318420f, 321.000000f);
    pathB.cubicTo(283.840179f, 304.431458f, 300.126587f, 291.000000f, 316.695129f, 291.000000f);
    pathB.lineTo(1006.69513f, 291.000000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpbyte_com1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(968.000000f, 14.0000000f);
    path.cubicTo(965.238586f, 14.0000000f, 963.000000f, 16.2385769f, 963.000000f, 19.0000000f);
    path.lineTo(963.000000f, 32.0000000f);
    path.cubicTo(963.000000f, 34.7614250f, 965.238586f, 37.0000000f, 968.000000f, 37.0000000f);
    path.lineTo(1034.00000f, 37.0000000f);
    path.cubicTo(1036.76147f, 37.0000000f, 1039.00000f, 34.7614250f, 1039.00000f, 32.0000000f);
    path.lineTo(1039.00000f, 19.0000000f);
    path.cubicTo(1039.00000f, 16.2385769f, 1036.76147f, 14.0000000f, 1034.00000f, 14.0000000f);
    path.lineTo(968.000000f, 14.0000000f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kInverseWinding_FillType);
    pathB.moveTo(968.000000f, 14.0000000f);
    pathB.lineTo(1034.00000f, 14.0000000f);
    pathB.cubicTo(1036.76147f, 14.0000000f, 1039.00000f, 16.2385750f, 1039.00000f, 19.0000000f);
    pathB.lineTo(1039.00000f, 32.0000000f);
    pathB.cubicTo(1039.00000f, 34.2091408f, 1036.76147f, 36.0000000f, 1034.00000f, 36.0000000f);
    pathB.lineTo(968.000000f, 36.0000000f);
    pathB.cubicTo(965.238586f, 36.0000000f, 963.000000f, 34.2091408f, 963.000000f, 32.0000000f);
    pathB.lineTo(963.000000f, 19.0000000f);
    pathB.cubicTo(963.000000f, 16.2385750f, 965.238586f, 14.0000000f, 968.000000f, 14.0000000f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skphealth_com76(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(708.099182f, 7.09919119f);
    path.lineTo(708.099182f, 7.09920025f);
    path.quadTo(704.000000f, 11.2010098f, 704.000000f, 17.0000000f);
    path.lineTo(704.000000f, 33.0000000f);
    path.lineTo(705.000000f, 33.0000000f);
    path.lineTo(705.000000f, 17.0000000f);
    path.cubicTo(705.000000f, 13.4101496f, 706.455078f, 10.1601505f, 708.807617f, 7.80761385f);
    path.lineTo(708.099182f, 7.09919119f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(704.000000f, 3.00000000f);
    pathB.lineTo(704.000000f, 33.0000000f);
    pathB.lineTo(705.000000f, 33.0000000f);
    pathB.lineTo(719.500000f, 3.00000000f);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpahrefs_com88(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1099.82886f, 7.17117119f);
    path.lineTo(1099.12134f, 7.87867832f);
    path.cubicTo(1099.66418f, 8.42157173f, 1100.00000f, 9.17157173f, 1100.00000f, 10.0000000f);
    path.lineTo(1100.00000f, 28.0000000f);
    path.cubicTo(1100.00000f, 29.6568546f, 1098.65686f, 31.0000000f, 1097.00000f, 31.0000000f);
    path.lineTo(1088.00000f, 31.0000000f);
    path.lineTo(1088.00000f, 32.0000000f);
    path.lineTo(1097.00000f, 32.0000000f);
    path.quadTo(1098.65686f, 32.0000000f, 1099.82886f, 30.8288002f);
    path.quadTo(1101.00000f, 29.6568546f, 1101.00000f, 28.0000000f);
    path.lineTo(1101.00000f, 10.0000000f);
    path.quadTo(1101.00000f, 8.34314537f, 1099.82886f, 7.17119980f);
    path.lineTo(1099.82886f, 7.17117119f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1101.00000f, 6.00000000f);
    pathB.lineTo(1088.00000f, 6.00000000f);
    pathB.lineTo(1088.00000f, 19.0000000f);
    pathB.lineTo(1101.00000f, 32.0000000f);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpahrefs_com29(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1037.17114f, 7.17119980f);
    path.quadTo(1038.34314f, 6.00000000f, 1040.00000f, 6.00000000f);
    path.lineTo(1074.00000f, 6.00000000f);
    path.lineTo(1074.00000f, 32.0000000f);
    path.lineTo(1040.00000f, 32.0000000f);
    path.quadTo(1038.34314f, 32.0000000f, 1037.17114f, 30.8288002f);
    path.quadTo(1036.00000f, 29.6568546f, 1036.00000f, 28.0000000f);
    path.lineTo(1036.00000f, 10.0000000f);
    path.quadTo(1036.00000f, 8.34314537f, 1037.17114f, 7.17119980f);
    path.close();
    path.moveTo(1037.00000f, 10.0000000f);
    path.cubicTo(1037.00000f, 8.34314537f, 1038.34314f, 7.00000000f, 1040.00000f, 7.00000000f);
    path.lineTo(1073.00000f, 7.00000000f);
    path.lineTo(1073.00000f, 31.0000000f);
    path.lineTo(1040.00000f, 31.0000000f);
    path.cubicTo(1038.34314f, 31.0000000f, 1037.00000f, 29.6568546f, 1037.00000f, 28.0000000f);
    path.lineTo(1037.00000f, 10.0000000f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1036.00000f, 32.0000000f);
    pathB.lineTo(1049.00000f, 19.0000000f);
    pathB.lineTo(1073.00000f, 31.0000000f);
    pathB.lineTo(1074.00000f, 32.0000000f);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp85d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,6, 1,0, 6,2);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(2,6, 1,0, 6,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void skpkkiste_to98(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(96, 122);
    path.cubicTo(94.6192932f, 122, 93.3692932f, 122.559647f, 92.4644699f, 123.46447f);
    path.lineTo(94.1715698f, 125.17157f);
    path.cubicTo(94.8954315f, 124.447708f, 95.8954315f, 124, 97, 124);
    path.lineTo(257, 124);
    path.cubicTo(258.104553f, 124, 259.104584f, 124.447708f, 259.82843f, 125.17157f);
    path.lineTo(261.535522f, 123.46447f);
    path.cubicTo(260.630707f, 122.559647f, 259.380707f, 122, 258, 122);
    path.lineTo(96, 122);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(258, 122);
    pathB.cubicTo(260.761414f, 122, 263, 124.238579f, 263, 127);
    pathB.lineTo(263, 284);
    pathB.cubicTo(263, 286.761414f, 260.761414f, 289, 258, 289);
    pathB.lineTo(96, 289);
    pathB.cubicTo(93.2385788f, 289, 91, 286.761414f, 91, 284);
    pathB.lineTo(91, 127);
    pathB.cubicTo(91, 124.238579f, 93.2385788f, 122, 96, 122);
    pathB.lineTo(258, 122);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void issue1417(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1;
    path1.moveTo(122.58908843994140625f, 82.2836456298828125f);
    path1.quadTo(129.8215789794921875f, 80, 138, 80);
    path1.quadTo(147.15692138671875f, 80, 155.1280364990234375f, 82.86279296875f);
    path1.lineTo(161.1764678955078125f, 100);
    path1.lineTo(161.1764678955078125f, 100);
    path1.lineTo(115.29412078857421875f, 100);
    path1.lineTo(115.29412078857421875f, 100);
    path1.lineTo(122.58908843994140625f, 82.2836456298828125f);
    path1.lineTo(122.58908843994140625f, 82.2836456298828125f);
    path1.close();
    path1.moveTo(98.68194580078125f, 140.343841552734375f);
    path1.lineTo(115.29412078857421875f, 100);
    path1.lineTo(115.29412078857421875f, 100);
    path1.lineTo(97.9337615966796875f, 100);
    path1.lineTo(97.9337615966796875f, 100);
    path1.quadTo(88, 112.94264984130859375f, 88, 130);
    path1.quadTo(88, 131.544830322265625f, 88.08148956298828125f, 133.0560302734375f);
    path1.lineTo(98.68194580078125f, 140.343841552734375f);
    path1.lineTo(98.68194580078125f, 140.343841552734375f);
    path1.close();
    path1.moveTo(136.969696044921875f, 166.6666717529296875f);
    path1.lineTo(98.68194580078125f, 140.343841552734375f);
    path1.lineTo(98.68194580078125f, 140.343841552734375f);
    path1.lineTo(93.45894622802734375f, 153.02825927734375f);
    path1.lineTo(93.45894622802734375f, 153.02825927734375f);
    path1.quadTo(96.94116973876953125f, 159.65185546875f, 102.64466094970703125f, 165.3553466796875f);
    path1.quadTo(110.7924652099609375f, 173.503143310546875f, 120.8179779052734375f, 177.1177825927734375f);
    path1.lineTo(136.969696044921875f, 166.6666717529296875f);
    path1.lineTo(136.969696044921875f, 166.6666717529296875f);
    path1.close();
    path1.moveTo(175.8309783935546875f, 141.5211334228515625f);
    path1.lineTo(136.969696044921875f, 166.6666717529296875f);
    path1.lineTo(136.969696044921875f, 166.6666717529296875f);
    path1.lineTo(153.15728759765625f, 177.7956390380859375f);
    path1.lineTo(153.15728759765625f, 177.7956390380859375f);
    path1.quadTo(164.392425537109375f, 174.318267822265625f, 173.3553466796875f, 165.3553466796875f);
    path1.quadTo(177.805816650390625f, 160.9048614501953125f, 180.90380859375f, 155.8941650390625f);
    path1.lineTo(175.8309783935546875f, 141.5211334228515625f);
    path1.lineTo(175.8309783935546875f, 141.5211334228515625f);
    path1.close();
    path1.moveTo(175.8309783935546875f, 141.5211334228515625f);
    path1.lineTo(187.8782806396484375f, 133.7258148193359375f);
    path1.lineTo(187.8782806396484375f, 133.7258148193359375f);
    path1.quadTo(188, 131.8880615234375f, 188, 130);
    path1.quadTo(188, 112.942657470703125f, 178.0662384033203125f, 100);
    path1.lineTo(161.1764678955078125f, 100);
    path1.lineTo(161.1764678955078125f, 100);
    path1.lineTo(175.8309783935546875f, 141.5211334228515625f);
    path1.lineTo(175.8309783935546875f, 141.5211334228515625f);
    path1.close();

    SkPath path2;
    path2.moveTo(174.117645263671875f, 100);
    path2.lineTo(161.1764678955078125f, 100);
    path2.lineTo(161.1764678955078125f, 100);
    path2.lineTo(155.1280364990234375f, 82.86279296875f);
    path2.lineTo(155.1280364990234375f, 82.86279296875f);
    path2.quadTo(153.14971923828125f, 82.15229034423828125f, 151.098419189453125f, 81.618133544921875f);
    path2.lineTo(143.5294189453125f, 100);
    path2.lineTo(143.5294189453125f, 100);
    path2.lineTo(161.1764678955078125f, 100);
    path2.lineTo(161.1764678955078125f, 100);
    path2.lineTo(168.23529052734375f, 120);
    path2.lineTo(168.23529052734375f, 120);
    path2.lineTo(181.1764678955078125f, 120);
    path2.lineTo(181.1764678955078125f, 120);
    path2.lineTo(186.3661956787109375f, 134.7042236328125f);
    path2.lineTo(186.3661956787109375f, 134.7042236328125f);
    path2.lineTo(187.8782806396484375f, 133.7258148193359375f);
    path2.lineTo(187.8782806396484375f, 133.7258148193359375f);
    path2.quadTo(188, 131.8880615234375f, 188, 130);
    path2.quadTo(188, 124.80947113037109375f, 187.080169677734375f, 120);
    path2.lineTo(181.1764678955078125f, 120);
    path2.lineTo(181.1764678955078125f, 120);
    path2.lineTo(174.117645263671875f, 100);
    path2.lineTo(174.117645263671875f, 100);
    path2.close();
    path2.moveTo(88.91983795166015625f, 120);
    path2.lineTo(107.0588226318359375f, 120);
    path2.lineTo(107.0588226318359375f, 120);
    path2.lineTo(98.68194580078125f, 140.343841552734375f);
    path2.lineTo(98.68194580078125f, 140.343841552734375f);
    path2.lineTo(88.08148956298828125f, 133.0560302734375f);
    path2.lineTo(88.08148956298828125f, 133.0560302734375f);
    path2.quadTo(88, 131.544830322265625f, 88, 130);
    path2.quadTo(88, 124.80951690673828125f, 88.91983795166015625f, 120);
    path2.close();
    path2.moveTo(96.67621612548828125f, 145.21490478515625f);
    path2.lineTo(98.68194580078125f, 140.343841552734375f);
    path2.lineTo(98.68194580078125f, 140.343841552734375f);
    path2.lineTo(120.68767547607421875f, 155.4727783203125f);
    path2.lineTo(120.68767547607421875f, 155.4727783203125f);
    path2.lineTo(118.68194580078125f, 160.343841552734375f);
    path2.lineTo(118.68194580078125f, 160.343841552734375f);
    path2.lineTo(96.67621612548828125f, 145.21490478515625f);
    path2.lineTo(96.67621612548828125f, 145.21490478515625f);
    path2.close();
    path2.moveTo(113.232177734375f, 173.5789947509765625f);
    path2.quadTo(116.8802642822265625f, 175.69805908203125f, 120.8179779052734375f, 177.1177825927734375f);
    path2.lineTo(132.2864990234375f, 169.6969757080078125f);
    path2.lineTo(132.2864990234375f, 169.6969757080078125f);
    path2.lineTo(118.68194580078125f, 160.343841552734375f);
    path2.lineTo(118.68194580078125f, 160.343841552734375f);
    path2.lineTo(113.232177734375f, 173.5789947509765625f);
    path2.lineTo(113.232177734375f, 173.5789947509765625f);
    path2.close();
    // FIXME : difficult data, circle back later
    testPathOp(reporter, path1, path2, kUnion_SkPathOp, filename);
}

static void issue1418(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1;
    path1.moveTo(0, 0);
    path1.lineTo(1, 0);
    path1.lineTo(1, 0);
    path1.lineTo(1, 1);
    path1.lineTo(1, 1);
    path1.lineTo(0, 1);
    path1.lineTo(0, 1);
    path1.lineTo(0, 0);
    path1.lineTo(0, 0);
    path1.close();

    SkPath path2;
    path2.moveTo(0.64644664525985717773f, -0.35355341434478759766f);
    path2.quadTo(0.79289329051971435547f, -0.50000005960464477539f, 1.0000001192092895508f, -0.50000005960464477539f);
    path2.quadTo(1.2071068286895751953f, -0.50000005960464477539f, 1.3535535335540771484f, -0.35355341434478759766f);
    path2.quadTo(1.5000001192092895508f, -0.20710679888725280762f, 1.5000001192092895508f, 0);
    path2.quadTo(1.5000001192092895508f, 0.20710679888725280762f, 1.3535535335540771484f, 0.35355341434478759766f);
    path2.quadTo(1.2071068286895751953f, 0.50000005960464477539f, 1.0000001192092895508f, 0.50000005960464477539f);
    path2.quadTo(0.79289329051971435547f, 0.50000005960464477539f, 0.64644664525985717773f, 0.35355341434478759766f);
    path2.quadTo(0.50000005960464477539f, 0.20710679888725280762f, 0.50000005960464477539f, 0);
    path2.quadTo(0.50000005960464477539f, -0.20710679888725280762f, 0.64644664525985717773f, -0.35355341434478759766f);
    testPathOp(reporter, path1, path2, kIntersect_SkPathOp, filename);
}

static void cubicOp85i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 4);
    path.cubicTo(1, 5, 4, 3, 6, 4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3, 4);
    pathB.cubicTo(4, 6, 4, 3, 5, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void issue1418b(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1;
    path1.moveTo(0, 0);
    path1.lineTo(1, 0);
    path1.lineTo(1, 1);
    path1.lineTo(0, 1);
    path1.lineTo(0, 0);
    path1.close();
    path1.setFillType(SkPath::kWinding_FillType);
    SkPath path2;
    path2.moveTo(0.646446645f, -0.353553414f);
    path2.quadTo(0.792893291f, -0.50000006f, 1.00000012f, -0.50000006f);
    path2.quadTo(1.20710683f, -0.50000006f, 1.35355353f, -0.353553414f);
    path2.quadTo(1.50000012f, -0.207106799f, 1.50000012f, 0);
    path2.quadTo(1.50000012f, 0.207106799f, 1.35355353f, 0.353553414f);
    path2.quadTo(1.20710683f, 0.50000006f, 1.00000012f, 0.50000006f);
    path2.quadTo(0.792893291f, 0.50000006f, 0.646446645f, 0.353553414f);
    path2.quadTo(0.50000006f, 0.207106799f, 0.50000006f, 0);
    path2.quadTo(0.50000006f, -0.207106799f, 0.646446645f, -0.353553414f);
    path2.close();
    path2.moveTo(1.00000012f, 0.50000006f);
    path2.lineTo(1.00000012f, 1.00000012f);
    path2.lineTo(0.50000006f, 1.00000012f);
    path2.quadTo(0.50000006f, 0.792893291f, 0.646446645f, 0.646446645f);
    path2.quadTo(0.792893291f, 0.50000006f, 1.00000012f, 0.50000006f);
    path2.close();
    path2.setFillType(SkPath::kEvenOdd_FillType);
    testPathOp(reporter, path1, path2, kIntersect_SkPathOp, filename);
}

static void rectOp1i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(2, 2, 4, 4, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void rectOp2i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(0, 0, 3, 3, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void rectOp3x(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 0);
    path.lineTo(3, 0);
    path.lineTo(3, 3);
    path.lineTo(0, 3);
    path.close();
    path.moveTo(2, 2);
    path.lineTo(3, 2);
    path.lineTo(3, 3);
    path.lineTo(2, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 1);
    pathB.lineTo(3, 1);
    pathB.lineTo(3, 3);
    pathB.lineTo(1, 3);
    pathB.close();
    pathB.moveTo(2, 2);
    pathB.lineTo(3, 2);
    pathB.lineTo(3, 3);
    pathB.lineTo(2, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kXOR_SkPathOp, filename);
}

static void issue1435(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1;
    path1.moveTo(160, 60);
    path1.lineTo(220, 230);
    path1.lineTo(60, 120);
    path1.lineTo(260, 120);
    path1.lineTo(90, 230);
    path1.lineTo(160, 60);
    path1.close();
    path1.setFillType(SkPath::kEvenOdd_FillType);

    SkPath path2;
    path2.moveTo(142.589081f, 102.283646f);
    path2.quadTo(149.821579f, 100, 158, 100);
    path2.quadTo(167.156921f, 100, 175.128036f, 102.862793f);
    path2.lineTo(181.176468f, 120);
    path2.lineTo(135.294128f, 120);
    path2.lineTo(142.589081f, 102.283646f);
    path2.close();
    path2.moveTo(118.681946f, 160.343842f);
    path2.lineTo(135.294128f, 120);
    path2.lineTo(117.933762f, 120);
    path2.quadTo(108, 132.942657f, 108, 150);
    path2.quadTo(108, 151.54483f, 108.08149f, 153.05603f);
    path2.lineTo(118.681946f, 160.343842f);
    path2.close();
    path2.moveTo(156.969696f, 186.666672f);
    path2.lineTo(118.681946f, 160.343842f);
    path2.lineTo(113.458946f, 173.028259f);
    path2.quadTo(116.94117f, 179.651855f, 122.644661f, 185.355347f);
    path2.quadTo(130.792465f, 193.503143f, 140.817978f, 197.117783f);
    path2.lineTo(156.969696f, 186.666672f);
    path2.close();
    path2.moveTo(195.830978f, 161.521133f);
    path2.lineTo(156.969696f, 186.666672f);
    path2.lineTo(173.157288f, 197.795639f);
    path2.quadTo(184.392426f, 194.318268f, 193.355347f, 185.355347f);
    path2.quadTo(197.805817f, 180.904861f, 200.903809f, 175.894165f);
    path2.lineTo(195.830978f, 161.521133f);
    path2.close();
    path2.moveTo(195.830978f, 161.521133f);
    path2.lineTo(207.878281f, 153.725815f);
    path2.quadTo(208, 151.888062f, 208, 150);
    path2.quadTo(208, 132.942657f, 198.066238f, 120);
    path2.lineTo(181.176468f, 120);
    path2.lineTo(195.830978f, 161.521133f);
    path2.close();
    path2.setFillType(SkPath::kEvenOdd_FillType);
    testPathOp(reporter, path1, path2, kIntersect_SkPathOp, filename);
}

static void skpkkiste_to716(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1173, 284);
    path.cubicTo(1173, 285.125824f, 1173.37207f, 286.164734f, 1174, 287.000488f);
    path.lineTo(1174, 123.999496f);
    path.cubicTo(1173.37207f, 124.835243f, 1173, 125.874168f, 1173, 127);
    path.lineTo(1173, 284);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1340, 122);
    pathB.cubicTo(1342.76147f, 122, 1345, 124.238579f, 1345, 127);
    pathB.lineTo(1345, 284);
    pathB.cubicTo(1345, 286.761414f, 1342.76147f, 289, 1340, 289);
    pathB.lineTo(1178, 289);
    pathB.cubicTo(1175.23853f, 289, 1173, 286.761414f, 1173, 284);
    pathB.lineTo(1173, 127);
    pathB.cubicTo(1173, 124.238579f, 1175.23853f, 122, 1178, 122);
    pathB.lineTo(1340, 122);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loopEdge1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0,0);
    path.lineTo(3,0);
    path.lineTo(3,2);
    path.lineTo(1,2);
    path.lineTo(1,1);
    path.lineTo(2,1);
    path.lineTo(2,3);
    path.lineTo(0,3);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(1,2);
    pathB.lineTo(2,2);
    pathB.lineTo(2,4);
    pathB.lineTo(1,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loopEdge2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0,0);
    path.lineTo(3,0);
    path.lineTo(3,2);
    path.lineTo(1,2);
    path.lineTo(1,1);
    path.lineTo(2,1);
    path.lineTo(2,3);
    path.lineTo(0,3);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(1 - 1e-6f,2);
    pathB.lineTo(2 - 1e-6f,2);
    pathB.lineTo(2 - 1e-6f,4);
    pathB.lineTo(1 - 1e-6f,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp86i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 4);
    path.cubicTo(3, 4, 6, 2, 5, 2);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(2, 6);
    pathB.cubicTo(2, 5, 4, 0, 4, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp87u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,2, 2,0, 6,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(4,6, 1,0, 2,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp88u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,5, 5,0, 6,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(4,6, 1,0, 5,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp89u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 3);
    path.cubicTo(1, 6, 5, 0, 6, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 5);
    pathB.cubicTo(3, 6, 3, 0, 6, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp90u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 5);
    path.cubicTo(1, 2, 5, 2, 4, 1);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(2, 5);
    pathB.cubicTo(1, 4, 5, 0, 2, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp91u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 6);
    path.cubicTo(0, 3, 6, 3, 5, 0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3, 6);
    pathB.cubicTo(0, 5, 6, 1, 3, 0);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void skpaaalgarve_org53(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
   path.moveTo(-1.24344979e-014f, 348);
    path.lineTo(258, 348);
    path.lineTo(258, 322);
    path.quadTo(258, 317.857849f, 255.072006f, 314.928009f);
    path.quadTo(252.142136f, 312, 248, 312);
    path.lineTo(1.77635684e-015f, 312);
    path.lineTo(-1.24344979e-014f, 348);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
   pathB.moveTo(0, 312);
    pathB.lineTo(258, 312);
    pathB.lineTo(258, 348);
    pathB.lineTo(0, 348);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpabcspark_ca103(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1.99840144e-015f, 494);
    path.lineTo(97, 494);
    path.quadTo(100.313705f, 494, 102.6576f, 491.657593f);
    path.quadTo(105, 489.313721f, 105, 486);
    path.lineTo(105, 425);
    path.quadTo(105, 421.686279f, 102.6576f, 419.342407f);
    path.quadTo(100.313705f, 417, 97, 417);
    path.lineTo(2.22044605e-016f, 417);
    path.lineTo(1.99840144e-015f, 494);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 417);
    pathB.lineTo(105, 417);
    pathB.lineTo(105, 494);
    pathB.lineTo(0, 494);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpacesoftech_com47(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(670.537415f, 285);
    path.lineTo(670.387451f, 285);
    path.lineTo(596.315186f, 314.850708f);
    path.lineTo(626.19696f, 389);
    path.lineTo(626.346863f, 389);
    path.lineTo(700.419189f, 359.149261f);
    path.lineTo(670.537415f, 285);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(663.318542f, 374.100616f);
    pathB.quadTo(647.950989f, 380.293671f, 632.705322f, 373.806305f);
    pathB.quadTo(617.459595f, 367.318909f, 611.266541f, 351.951355f);
    pathB.quadTo(605.073486f, 336.58374f, 611.560913f, 321.338074f);
    pathB.quadTo(618.048279f, 306.092407f, 633.415833f, 299.899353f);
    pathB.quadTo(648.783447f, 293.706299f, 664.029114f, 300.193665f);
    pathB.quadTo(679.27478f, 306.68103f, 685.467834f, 322.048645f);
    pathB.quadTo(691.660889f, 337.416199f, 685.173523f, 352.661896f);
    pathB.quadTo(678.686157f, 367.907562f, 663.318542f, 374.100616f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpact_com43(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1.45716772e-016f, 924.336121f);
    path.lineTo(-1.11022302e-016f, 920);
    path.lineTo(6, 920);
    path.lineTo(6, 926);
    path.lineTo(1.66389287f, 926);
    path.quadTo(1.18842196f, 925.674561f, 0.756800175f, 925.243225f);
    path.quadTo(0.325406998f, 924.811523f, 1.45716772e-016f, 924.336121f);
    path.close();
    path.moveTo(1, 921);
    path.lineTo(5, 921);
    path.lineTo(5, 925);
    path.cubicTo(2.79086018f, 925, 1, 923.209167f, 1, 921);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(-1, 920);
    pathB.lineTo(0, 920);
    pathB.lineTo(3, 927);
    pathB.lineTo(-1, 927);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpadbox_lt8(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(320.097229f, 628.573669f);
    path.lineTo(610.227173f, 85.7786865f);
    path.lineTo(946.652588f, 265.601807f);
    path.lineTo(656.522644f, 808.39679f);
    path.lineTo(320.097229f, 628.573669f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kInverseWinding_FillType);
    pathB.moveTo(333.866608f, 623.496155f);
    pathB.lineTo(613.368042f, 100.585754f);
    pathB.cubicTo(613.685303f, 99.9921265f, 614.423767f, 99.7681885f, 615.017395f, 100.085449f);
    pathB.lineTo(932.633057f, 269.854553f);
    pathB.cubicTo(933.226685f, 270.171875f, 933.450623f, 270.910278f, 933.133301f, 271.503906f);
    pathB.lineTo(653.631897f, 794.414307f);
    pathB.cubicTo(653.314636f, 795.007935f, 652.576172f, 795.231934f, 651.982544f, 794.914612f);
    pathB.lineTo(334.366943f, 625.145508f);
    pathB.cubicTo(333.773315f, 624.828247f, 333.549286f, 624.089783f, 333.866608f, 623.496155f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpadindex_de4(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 926);
    path.lineTo(0, 0);
    path.lineTo(1280, 0);
    path.lineTo(1280, 926);
    path.lineTo(0, 926);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 312);
    pathB.lineTo(8.20486257e-015f, 178);
    pathB.lineTo(49, 178);
    pathB.lineTo(49, 312);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpadithya_putr4_blogspot_com551(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(205.605804f, 142.334625f);
    path.lineTo(254.665359f, 85.6058044f);
    path.lineTo(311.394196f, 134.665359f);
    path.lineTo(262.334625f, 191.39418f);
    path.lineTo(205.605804f, 142.334625f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(283.407959f, 110.462646f);
    pathB.cubicTo(298.864319f, 123.829437f, 300.558258f, 147.195221f, 287.191467f, 162.651581f);
    pathB.lineTo(286.537354f, 163.407959f);
    pathB.cubicTo(273.170563f, 178.864334f, 249.804779f, 180.558258f, 234.348419f, 167.191467f);
    pathB.lineTo(233.592026f, 166.537338f);
    pathB.cubicTo(218.135666f, 153.170547f, 216.441727f, 129.804779f, 229.808517f, 114.348412f);
    pathB.lineTo(230.462646f, 113.592026f);
    pathB.cubicTo(243.829437f, 98.1356659f, 267.195221f, 96.4417267f, 282.651581f, 109.808517f);
    pathB.lineTo(283.407959f, 110.462646f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpadspert_de11(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(-4.4408921e-016f, 682.5f);
    path.lineTo(30.5f, 682.5f);
    path.cubicTo(32.709137f, 682.5f, 34.5f, 680.709167f, 34.5f, 678.5f);
    path.lineTo(34.5f, 486.5f);
    path.cubicTo(34.5f, 484.290863f, 32.709137f, 482.5f, 30.5f, 482.5f);
    path.lineTo(0, 482.5f);
    path.lineTo(-4.4408921e-016f, 682.5f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 482);
    pathB.lineTo(35, 482);
    pathB.lineTo(35, 683);
    pathB.lineTo(0, 683);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpaiaigames_com870(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(324.071075f, 845.071045f);
    path.cubicTo(324.405151f, 844.737f, 324.715668f, 844.379395f, 325, 844.000977f);
    path.lineTo(325, 842.127197f);
    path.cubicTo(324.571411f, 842.956238f, 324.017761f, 843.710144f, 323.363953f, 844.363953f);
    path.lineTo(324.071075f, 845.071045f);
    path.close();
    path.moveTo(323.363953f, 714.636047f);
    path.lineTo(324.071075f, 713.928955f);
    path.cubicTo(324.405151f, 714.263f, 324.715668f, 714.620605f, 325, 714.999023f);
    path.lineTo(325, 716.872803f);
    path.cubicTo(324.571411f, 716.043762f, 324.017761f, 715.289856f, 323.363953f, 714.636047f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(317, 711);
    pathB.cubicTo(322.522858f, 711, 327, 715.477173f, 327, 721);
    pathB.lineTo(327, 838);
    pathB.cubicTo(327, 843.522827f, 322.522858f, 848, 317, 848);
    pathB.lineTo(155, 848);
    pathB.cubicTo(149.477158f, 848, 145, 843.522827f, 145, 838);
    pathB.lineTo(145, 721);
    pathB.cubicTo(145, 715.477173f, 149.477158f, 711, 155, 711);
    pathB.lineTo(317, 711);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp92i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(2, 6, 4, 1, 5, 4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 4);
    pathB.cubicTo(4, 5, 1, 0, 6, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp93d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(1, 6, 4, 1, 4, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 4);
    pathB.cubicTo(3, 4, 1, 0, 6, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp94u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 3);
    path.cubicTo(2, 3, 5, 0, 5, 3);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(0, 5);
    pathB.cubicTo(3, 5, 3, 0, 3, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void skpadbox_lt15(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(333.292084f, 624.570984f);
    path.lineTo(614.229797f, 98.9735107f);
    path.lineTo(933.457764f, 269.604431f);
    path.lineTo(652.52002f, 795.201904f);
    path.lineTo(333.292084f, 624.570984f);
    path.close();
    SkPath pathB;
     pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(613.368042f, 100.585754f);
    pathB.cubicTo(613.685303f, 99.9921265f, 614.423767f, 99.7681885f, 615.017395f, 100.085449f);
    pathB.lineTo(932.633057f, 269.854553f);
    pathB.cubicTo(933.226685f, 270.171875f, 933.450623f, 270.910278f, 933.133301f, 271.503906f);
    pathB.lineTo(653.631897f, 794.414307f);
    pathB.cubicTo(653.314636f, 795.007935f, 652.576172f, 795.231934f, 651.982544f, 794.914612f);
    pathB.lineTo(334.366943f, 625.145508f);
    pathB.cubicTo(333.773315f, 624.828247f, 333.549286f, 624.089783f, 333.866608f, 623.496155f);
    pathB.lineTo(613.368042f, 100.585754f);
     pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpadoption_org196(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(802, 367);
    path.lineTo(802, 324);
    path.lineTo(956, 324);
    path.lineTo(956, 371);
    path.quadTo(956, 373.071075f, 954.536011f, 374.536011f);
    path.quadTo(953.071045f, 376, 951, 376);
    path.lineTo(811, 376);
    path.cubicTo(806.029419f, 376, 802, 371.970551f, 802, 367);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kInverseWinding_FillType);
    pathB.moveTo(803, 326);
    pathB.lineTo(955, 326);
    pathB.lineTo(955, 370);
    pathB.cubicTo(955, 372.761414f, 952.761414f, 375, 950, 375);
    pathB.lineTo(808, 375);
    pathB.cubicTo(805.238586f, 375, 803, 372.761414f, 803, 370);
    pathB.lineTo(803, 326);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpadspert_net23(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(-2.220446e-018f, 483.5f);
    path.lineTo(0, 482.5f);
    path.lineTo(30.5f, 482.5f);
    path.cubicTo(32.709137f, 482.5f, 34.5f, 484.290863f, 34.5f, 486.5f);
    path.lineTo(34.5f, 678.5f);
    path.cubicTo(34.5f, 680.709167f, 32.709137f, 682.5f, 30.5f, 682.5f);
    path.lineTo(-4.4408921e-016f, 682.5f);
    path.lineTo(-4.41868766e-016f, 681.5f);
    path.lineTo(30.5f, 681.5f);
    path.cubicTo(32.1568565f, 681.5f, 33.5f, 680.15686f, 33.5f, 678.5f);
    path.lineTo(33.5f, 486.5f);
    path.cubicTo(33.5f, 484.84314f, 32.1568565f, 483.5f, 30.5f, 483.5f);
    path.lineTo(-2.220446e-018f, 483.5f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 482);
    pathB.lineTo(35, 482);
    pathB.lineTo(35, 683);
    pathB.lineTo(0, 683);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpadventistmission_org572(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1182.00037f, 926);
    path.cubicTo(1181.08813f, 924.785583f, 1179.63586f, 924, 1178, 924);
    path.lineTo(938, 924);
    path.cubicTo(936.364197f, 924, 934.911865f, 924.785583f, 933.999634f, 926);
    path.lineTo(1182.00037f, 926);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(934, 924);
    pathB.lineTo(1182, 924);
    pathB.lineTo(1182, 926);
    pathB.lineTo(934, 926);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpagentxsites_com55(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(925, 27);
    path.cubicTo(924.447693f, 27, 924, 27.4477158f, 924, 28);
    path.lineTo(924, 55);
    path.cubicTo(924, 55.5522842f, 924.447693f, 56, 925, 56);
    path.lineTo(1103, 56);
    path.cubicTo(1103.55225f, 56, 1104, 55.5522842f, 1104, 55);
    path.lineTo(1104, 28);
    path.cubicTo(1104, 27.4477158f, 1103.55225f, 27, 1103, 27);
    path.lineTo(925, 27);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1103, 27);
    pathB.cubicTo(1104.10461f, 27, 1105, 27.8954315f, 1105, 29);
    pathB.lineTo(1105, 54);
    pathB.cubicTo(1105, 55.1045685f, 1104.10461f, 56, 1103, 56);
    pathB.lineTo(926, 56);
    pathB.cubicTo(924.895447f, 56, 924, 55.1045685f, 924, 54);
    pathB.lineTo(924, 29);
    pathB.cubicTo(924, 27.8954315f, 924.895447f, 27, 926, 27);
    pathB.lineTo(1103, 27);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpbakosoft_com10(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(190, 170);
    path.cubicTo(178.9543f, 170, 170, 178.9543f, 170, 190);
    path.cubicTo(170, 201.0457f, 178.9543f, 210, 190, 210);
    path.lineTo(370, 210);
    path.cubicTo(381.045685f, 210, 390, 201.0457f, 390, 190);
    path.cubicTo(390, 178.9543f, 381.045685f, 170, 370, 170);
    path.lineTo(190, 170);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(210, 190);
    pathB.quadTo(210, 198.284271f, 204.142136f, 204.142136f);
    pathB.quadTo(198.284271f, 210, 190, 210);
    pathB.quadTo(181.715729f, 210, 175.857864f, 204.142136f);
    pathB.quadTo(170, 198.284271f, 170, 190);
    pathB.quadTo(170, 181.715729f, 175.857864f, 175.857864f);
    pathB.quadTo(181.715729f, 170, 190, 170);
    pathB.quadTo(198.284271f, 170, 204.142136f, 175.857864f);
    pathB.quadTo(210, 181.715729f, 210, 190);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpbambootheme_com12(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(47.8780937f, 58);
    path.lineTo(0, 58);
    path.lineTo(-8.65973959e-015f, 96.9914017f);
    path.quadTo(20.0654926f, 96.6451874f, 34.3553391f, 82.3553391f);
    path.quadTo(44.9466133f, 71.764061f, 47.8780937f, 58);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(-1, -3);
    pathB.lineTo(-1, -3);
    pathB.cubicTo(26.6142502f, -3, 49, 19.3857498f, 49, 47);
    pathB.lineTo(49, 47);
    pathB.cubicTo(49, 74.6142502f, 26.6142502f, 97, -1, 97);
    pathB.lineTo(-1, 97);
    pathB.cubicTo(-28.6142502f, 97, -51, 74.6142502f, -51, 47);
    pathB.lineTo(-51, 47);
    pathB.cubicTo(-51, 19.3857498f, -28.6142502f, -3, -1, -3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpakmmos_ru100(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(693.000488f, 926);
    path.cubicTo(692.164734f, 925.37207f, 691.125793f, 925, 690, 925);
    path.lineTo(578, 925);
    path.cubicTo(576.874207f, 925, 575.835266f, 925.37207f, 574.999512f, 926);
    path.lineTo(693.000488f, 926);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(575, 925);
    pathB.lineTo(693, 925);
    pathB.lineTo(693, 926);
    pathB.lineTo(575, 926);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpcarpetplanet_ru22(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(195, 785);
    path.cubicTo(124.307556f, 785, 67, 841.859863f, 67, 912);
    path.lineTo(67, 913);
    path.cubicTo(67, 917.388916f, 67.2243805f, 921.725769f, 67.662384f, 926);
    path.lineTo(322, 926);
    path.lineTo(322, 896.048035f);
    path.cubicTo(314.09201f, 833.437622f, 260.247131f, 785, 195, 785);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(195, 785);
    pathB.cubicTo(265.140167f, 785, 322, 842.307556f, 322, 913);
    pathB.cubicTo(322, 983.692444f, 265.140167f, 1041, 195, 1041);
    pathB.lineTo(194, 1041);
    pathB.cubicTo(123.85984f, 1041, 67, 983.692444f, 67, 913);
    pathB.cubicTo(67, 842.307556f, 123.85984f, 785, 194, 785);
    pathB.lineTo(195, 785);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpcarrot_is24(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(945, 597);
    path.quadTo(913.93396f, 597, 891.96698f, 618.96698f);
    path.quadTo(870, 640.93396f, 870, 672);
    path.quadTo(870, 703.06604f, 891.96698f, 725.03302f);
    path.quadTo(913.93396f, 747, 945, 747);
    path.quadTo(976.06604f, 747, 998.03302f, 725.03302f);
    path.quadTo(1020, 703.06604f, 1020, 672);
    path.quadTo(1020, 640.93396f, 998.03302f, 618.96698f);
    path.quadTo(976.06604f, 597, 945, 597);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(945.080994f, 597.161987f);
    pathB.cubicTo(903.659973f, 597.161987f, 870.080994f, 630.73999f, 870.080994f, 672.161987f);
    pathB.cubicTo(870.080994f, 676.096008f, 870.387024f, 679.957031f, 870.971008f, 683.726013f);
    pathB.cubicTo(876.53302f, 719.656006f, 907.593994f, 747.161987f, 945.080994f, 747.161987f);
    pathB.cubicTo(982.567993f, 747.161987f, 1013.62903f, 719.656006f, 1019.19104f, 683.726013f);
    pathB.cubicTo(1019.77502f, 679.955017f, 1020.08099f, 676.094971f, 1020.08099f, 672.161987f);
    pathB.cubicTo(1020.08002f, 630.73999f, 986.502014f, 597.161987f, 945.080994f, 597.161987f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpbangalorenest_com4(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 926);
    path.lineTo(0, 0);
    path.lineTo(1265, 0);
    path.lineTo(1265, 926);
    path.lineTo(0, 926);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 290);
    pathB.lineTo(-2.64514972e-014f, 146);
    pathB.lineTo(30, 146);
    pathB.lineTo(30, 290);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpbenzoteh_ru152(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(883, 23);
    path.lineTo(883, 0);
    path.lineTo(1122.5f, 0);
    path.lineTo(1122.5f, 25.2136822f);
    path.quadTo(1122.14441f, 25.9271851f, 1121.53601f, 26.5359993f);
    path.quadTo(1120.07104f, 28, 1118, 28);
    path.lineTo(888, 28);
    path.quadTo(885.928955f, 28, 884.463989f, 26.5359993f);
    path.quadTo(883, 25.0710678f, 883, 23);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(883, 0);
    pathB.lineTo(1123, 0);
    pathB.lineTo(1123, 23);
    pathB.quadTo(1123, 25.0710678f, 1121.53601f, 26.5359993f);
    pathB.quadTo(1120.07104f, 28, 1118, 28);
    pathB.lineTo(888, 28);
    pathB.quadTo(885.928955f, 28, 884.463989f, 26.5359993f);
    pathB.quadTo(883, 25.0710678f, 883, 23);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpbestred_ru37(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(883, 23);
    path.lineTo(883, 0);
    path.lineTo(1122.5f, 0);
    path.lineTo(1122.5f, 25.2136822f);
    path.quadTo(1122.14441f, 25.9271851f, 1121.53601f, 26.5359993f);
    path.quadTo(1120.07104f, 28, 1118, 28);
    path.lineTo(888, 28);
    path.quadTo(885.928955f, 28, 884.463989f, 26.5359993f);
    path.quadTo(883, 25.0710678f, 883, 23);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(883, 0);
    pathB.lineTo(1123, 0);
    pathB.lineTo(1123, 23);
    pathB.quadTo(1123, 25.0710678f, 1121.53601f, 26.5359993f);
    pathB.quadTo(1120.07104f, 28, 1118, 28);
    pathB.lineTo(888, 28);
    pathB.quadTo(885.928955f, 28, 884.463989f, 26.5359993f);
    pathB.quadTo(883, 25.0710678f, 883, 23);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpbingoentertainment_net189(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(896, 745.38678f);
    path.lineTo(896, 873.38678f);
    path.lineTo(922.567993f, 876.683716f);
    path.lineTo(922.567993f, 748.683716f);
    path.lineTo(896, 745.38678f);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(899.200928f, 745.783997f);
    pathB.cubicTo(897.119385f, 745.525696f, 895.432007f, 752.031982f, 895.432007f, 760.316284f);
    pathB.lineTo(895.432007f, 858.316284f);
    pathB.cubicTo(895.432007f, 866.600586f, 897.119385f, 873.525696f, 899.200928f, 873.783997f);
    pathB.lineTo(918.799133f, 876.216003f);
    pathB.cubicTo(920.880615f, 876.474304f, 922.567993f, 869.968018f, 922.567993f, 861.683716f);
    pathB.lineTo(922.567993f, 763.683716f);
    pathB.cubicTo(922.567993f, 755.399414f, 920.880615f, 748.474304f, 918.799133f, 748.216003f);
    pathB.lineTo(899.200928f, 745.783997f);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpcarrefour_ro62(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1104, 453);
    path.lineTo(399, 453);
    path.lineTo(399, 657);
    path.cubicTo(399, 661.970581f, 403.029449f, 666, 408, 666);
    path.lineTo(1095, 666);
    path.cubicTo(1099.97058f, 666, 1104, 661.970581f, 1104, 657);
    path.lineTo(1104, 453);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kInverseWinding_FillType);
    pathB.moveTo(400, 453);
    pathB.lineTo(1103, 453);
    pathB.lineTo(1103, 666);
    pathB.lineTo(406, 666);
    pathB.cubicTo(402.686279f, 666, 400, 663.313721f, 400, 660);
    pathB.lineTo(400, 453);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpcaffelavazzait_com_ua21(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(883, 23);
    path.lineTo(883, 0);
    path.lineTo(1122.5f, 0);
    path.lineTo(1122.5f, 25.2136822f);
    path.quadTo(1122.14441f, 25.9271851f, 1121.53601f, 26.5359993f);
    path.quadTo(1120.07104f, 28, 1118, 28);
    path.lineTo(888, 28);
    path.quadTo(885.928955f, 28, 884.463989f, 26.5359993f);
    path.quadTo(883, 25.0710678f, 883, 23);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(883, 0);
    pathB.lineTo(1123, 0);
    pathB.lineTo(1123, 23);
    pathB.quadTo(1123, 25.0710678f, 1121.53601f, 26.5359993f);
    pathB.quadTo(1120.07104f, 28, 1118, 28);
    pathB.lineTo(888, 28);
    pathB.quadTo(885.928955f, 28, 884.463989f, 26.5359993f);
    pathB.quadTo(883, 25.0710678f, 883, 23);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpcamcorder_kz21(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(883, 23);
    path.lineTo(883, 0);
    path.lineTo(1122.5f, 0);
    path.lineTo(1122.5f, 25.2136822f);
    path.quadTo(1122.14441f, 25.9271851f, 1121.53601f, 26.5359993f);
    path.quadTo(1120.07104f, 28, 1118, 28);
    path.lineTo(888, 28);
    path.quadTo(885.928955f, 28, 884.463989f, 26.5359993f);
    path.quadTo(883, 25.0710678f, 883, 23);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(883, 0);
    pathB.lineTo(1123, 0);
    pathB.lineTo(1123, 23);
    pathB.quadTo(1123, 25.0710678f, 1121.53601f, 26.5359993f);
    pathB.quadTo(1120.07104f, 28, 1118, 28);
    pathB.lineTo(888, 28);
    pathB.quadTo(885.928955f, 28, 884.463989f, 26.5359993f);
    pathB.quadTo(883, 25.0710678f, 883, 23);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpcavablar_net563(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(160.000488f, 918);
    path.cubicTo(159.164749f, 917.37207f, 158.125824f, 917, 157, 917);
    path.lineTo(94, 917);
    path.cubicTo(92.874176f, 917, 91.8352661f, 917.37207f, 90.9995193f, 918);
    path.lineTo(160.000488f, 918);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(91, 917);
    pathB.lineTo(160, 917);
    pathB.lineTo(160, 918);
    pathB.lineTo(91, 918);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void skpinsomnia_gr72(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1138, 231);
    path.lineTo(1137, 243.625748f);
    path.lineTo(1137, 926);
    path.lineTo(1139, 926);
    path.lineTo(1139, 231);
    path.lineTo(1138, 231);
    path.close();
    SkPath pathB;
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1139, 231);
    pathB.lineTo(1138, 231);
    pathB.lineTo(633, 6101);
    pathB.lineTo(1139, 6607);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp95u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 2);
    path.cubicTo(2, 3, 5, 1, 3, 2);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(1, 5);
    pathB.cubicTo(2, 3, 2, 0, 3, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp96d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1, 6);
    path.cubicTo(0, 3, 6, 3, 5, 0);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(3, 6);
    pathB.cubicTo(0, 5, 6, 1, 3, 0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp97x(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 2);
    path.cubicTo(0, 6, 2, 1, 2, 1);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(1, 2, 2, 0, 6, 0);
    pathB.close();
    testPathOp(reporter, path, pathB, kXOR_SkPathOp, filename);
}

static void cubicOp98x(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 3);
    path.cubicTo(3, 6, 4, 1, 6, 3);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(1, 4);
    pathB.cubicTo(3, 6, 3, 0, 6, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kXOR_SkPathOp, filename);
}

static void cubicOp99(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3,6);
    path.cubicTo(0,3, 6,5, 5,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(5,6);
    pathB.cubicTo(4,5, 6,3, 3,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp100(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,2, 2,1, 4,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(2,4, 1,0, 2,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp101(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(2, 3, 2, 1, 5, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(3, 5, 1, 0, 3, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp102(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,2, 1,0, 3,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,1);
    pathB.cubicTo(0,3, 1,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp103(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,5, 2,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(1,2, 1,0, 5,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp104(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,6, 4,0, 6,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,4);
    pathB.cubicTo(1,6, 1,0, 6,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp105(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,4, 6,5, 2,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(5,6);
    pathB.cubicTo(0,2, 1,0, 4,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp106(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(4, 6, 2, 1, 2, 0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(0, 2, 1, 0, 6, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp107(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(4, 6, 2, 1, 2, 0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(0, 2, 1, 0, 6, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp108(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(4, 6, 2, 1, 2, 0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(0, 2, 1, 0, 6, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp109(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(4,5, 6,3, 5,4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3,6);
    pathB.cubicTo(4,5, 1,0, 5,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp110(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(0, 0, 4, 4, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp111(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1,4);
    path.cubicTo(0,5, 4,1, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,4);
    pathB.cubicTo(1,3, 4,1, 5,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void xOp1u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1, 4);
    path.cubicTo(4, 5, 3, 2, 6, 3);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(2, 3);
    pathB.cubicTo(3, 6, 4, 1, 5, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void xOp1i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1, 4);
    path.cubicTo(1, 5, 6, 0, 5, 1);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(0, 6);
    pathB.cubicTo(1, 5, 4, 1, 5, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void xOp2i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(1, 5);
    path.cubicTo(0, 4, 3, 2, 6, 1);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(2, 3);
    pathB.cubicTo(1, 6, 5, 1, 4, 0);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void xOp3i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1,4);
    path.cubicTo(0,5, 4,1, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,4);
    pathB.cubicTo(1,3, 4,1, 5,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void findFirst1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,6, 5,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(1,2, 1,0, 6,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp112(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2,4);
    path.cubicTo(2,3, 6,4, 1,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(4,6);
    pathB.cubicTo(0,1, 4,2, 3,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp113(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(2,4);
    path.cubicTo(3,5, 2.33333325f,4.33333349f, 3.83333325f,3.83333349f);
    path.close();
    pathB.moveTo(3,5);
    pathB.cubicTo(2.33333325f,4.33333349f, 3.83333325f,3.83333349f, 2,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp114(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(1, 3, -1, 2, 3.5f, 1.33333337f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 3);
    pathB.cubicTo(-1, 2, 3.5f, 1.33333337f, 0, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp114asQuad(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(1, 3, -1, 2, 3.5f, 1.33333337f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 3);
    pathB.cubicTo(-1, 2, 3.5f, 1.33333337f, 0, 1);
    pathB.close();
    SkPath qPath, qPathB;
    CubicPathToQuads(path, &qPath);
    CubicPathToQuads(pathB, &qPathB);
    testPathOp(reporter, qPath, qPathB, kIntersect_SkPathOp, filename);
}

static void quadOp10i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0, 0);
    path.quadTo(1, 8, 3, 5);
    path.lineTo(8, 1);
    path.close();
    pathB.moveTo(0, 0);
    pathB.quadTo(8, 1, 4, 8);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void kari1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1;
    path1.moveTo(39.9375, -5.8359375);
    path1.lineTo(40.625, -5.7890625);
    path1.lineTo(37.7109375, 1.3515625);
    path1.lineTo(37.203125, 0.9609375);
    path1.close();

    SkPath path2;
    path2.moveTo(37.52734375f, -1.44140625f);
    path2.cubicTo(37.8736991882324f, -1.69921875f, 38.1640625f, -2.140625f, 38.3984375f, -2.765625f);
    path2.lineTo(38.640625f, -2.609375f);
    path2.cubicTo(38.53125f, -1.89583337306976f, 38.0664443969727f, -0.154893040657043f, 38.0664443969727f, -0.154893040657043f);
    path2.cubicTo(38.0664443969727f, -0.154893040657043f, 37.1809883117676f, -1.18359375f, 37.52734375, -1.44140625f);
    path2.close();

    testPathOp(reporter, path1, path2, kDifference_SkPathOp, filename);
}

static void issue2504(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1;
    path1.moveTo(34.2421875, -5.976562976837158203125);
    path1.lineTo(35.453121185302734375, 0);
    path1.lineTo(31.9375, 0);
    path1.close();

    SkPath path2;
    path2.moveTo(36.71843719482421875, 0.8886508941650390625);
    path2.cubicTo(36.71843719482421875, 0.8886508941650390625,
                  35.123386383056640625, 0.554015457630157470703125,
                  34.511409759521484375, -0.1152553558349609375);
    path2.cubicTo(33.899425506591796875, -0.7845261096954345703125,
                  34.53484344482421875, -5.6777553558349609375,
                  34.53484344482421875, -5.6777553558349609375);
    path2.close();
    testPathOp(reporter, path1, path2, kUnion_SkPathOp, filename);
}

static void issue2540(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1;
    path1.moveTo(26.5054988861083984375, 85.73960113525390625);
    path1.cubicTo(84.19739532470703125, 17.77140045166015625, 16.93920135498046875, 101.86199951171875, 12.631000518798828125, 105.24700164794921875);
    path1.cubicTo(11.0819997787475585937500000, 106.46399688720703125, 11.5260000228881835937500000, 104.464996337890625, 11.5260000228881835937500000, 104.464996337890625);
    path1.lineTo(23.1654987335205078125, 89.72879791259765625);
    path1.cubicTo(23.1654987335205078125, 89.72879791259765625, -10.1713008880615234375, 119.9160003662109375, -17.1620006561279296875, 120.8249969482421875);
    path1.cubicTo(-19.1149997711181640625, 121.07900238037109375, -18.0380001068115234375, 119.79299163818359375, -18.0380001068115234375, 119.79299163818359375);
    path1.cubicTo(-18.0380001068115234375, 119.79299163818359375, 14.22100067138671875, 90.60700225830078125, 26.5054988861083984375, 85.73960113525390625);
    path1.close();

    SkPath path2;
    path2.moveTo(-25.077999114990234375, 124.9120025634765625);
    path2.cubicTo(-25.077999114990234375, 124.9120025634765625, -25.9509983062744140625, 125.95400238037109375, -24.368999481201171875, 125.7480010986328125);
    path2.cubicTo(-16.06999969482421875, 124.66899871826171875, 1.2680000066757202148437500, 91.23999786376953125, 37.264003753662109375, 95.35400390625);
    path2.cubicTo(37.264003753662109375, 95.35400390625, 11.3710002899169921875, 83.7339935302734375, -25.077999114990234375, 124.9120025634765625);
    path2.close();
    testPathOp(reporter, path1, path2, kUnion_SkPathOp, filename);
}

static void rects1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 0);
    path.lineTo(1, 0);
    path.lineTo(1, 1);
    path.lineTo(0, 1);
    path.close();
    path.moveTo(0, 0);
    path.lineTo(6, 0);
    path.lineTo(6, 6);
    path.lineTo(0, 6);
    path.close();
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.moveTo(0, 0);
    pathB.lineTo(1, 0);
    pathB.lineTo(1, 1);
    pathB.lineTo(0, 1);
    pathB.close();
    pathB.moveTo(0, 0);
    pathB.lineTo(2, 0);
    pathB.lineTo(2, 2);
    pathB.lineTo(0, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void rects2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(0, 0);
    path.lineTo(4, 0);
    path.lineTo(4, 4);
    path.lineTo(0, 4);
    path.close();
    path.moveTo(3, 3);
    path.lineTo(4, 3);
    path.lineTo(4, 4);
    path.lineTo(3, 4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3, 3);
    pathB.lineTo(6, 3);
    pathB.lineTo(6, 6);
    pathB.lineTo(3, 6);
    pathB.close();
    pathB.moveTo(3, 3);
    pathB.lineTo(4, 3);
    pathB.lineTo(4, 4);
    pathB.lineTo(3, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void rects3(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(0, 0, 4, 4, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void rects4(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 3, 3, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void issue2753(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1;
    path1.moveTo(142.701f, 110.568f);
    path1.lineTo(142.957f, 100);
    path1.lineTo(153.835f, 100);
    path1.lineTo(154.592f, 108.188f);
    path1.cubicTo(154.592f, 108.188f, 153.173f, 108.483f, 152.83f, 109.412f);
    path1.cubicTo(152.83f, 109.412f, 142.701f, 110.568f, 142.701f, 110.568f);
    path1.close();

    SkPath path2;
    path2.moveTo(39, 124.001f);
    path2.cubicTo(39, 124.001f, 50.6f, 117.001f, 50.6f, 117.001f);
    path2.cubicTo(50.6f, 117.001f, 164.601f, 85.2f, 188.201f, 117.601f);
    path2.cubicTo(188.201f, 117.601f, 174.801f, 93, 39, 124.001f);
    path2.close();

    testPathOp(reporter, path1, path2, kUnion_SkPathOp, filename);
}

static void issue2808(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1, path2;

    path1.moveTo(509.20300293f, 385.601989746f);
    path1.quadTo(509.20300293f, 415.68838501f, 487.928710938f, 436.96270752f);
    path1.quadTo(466.654388428f, 458.236999512f, 436.567993164f, 458.236999512f);
    path1.quadTo(406.4815979f, 458.236999512f, 385.207275391f, 436.96270752f);
    path1.quadTo(363.932983398f, 415.68838501f, 363.932983398f, 385.601989746f);
    path1.quadTo(363.932983398f, 355.515594482f, 385.207275391f, 334.241271973f);
    path1.quadTo(406.4815979f, 312.96697998f, 436.567993164f, 312.96697998f);
    path1.quadTo(466.654388428f, 312.96697998f, 487.928710938f, 334.241271973f);
    path1.quadTo(509.20300293f, 355.515594482f, 509.20300293f, 385.601989746f);
    path1.close();

    path2.moveTo(449.033996582f, 290.87298584f);
    path2.quadTo(449.033996582f, 301.028259277f, 441.853149414f, 308.209106445f);
    path2.quadTo(434.672271729f, 315.389984131f, 424.516998291f, 315.389984131f);
    path2.quadTo(414.361724854f, 315.389984131f, 407.180847168f, 308.209106445f);
    path2.quadTo(400, 301.028259277f, 400, 290.87298584f);
    path2.quadTo(400, 280.717712402f, 407.180847168f, 273.536865234f);
    path2.quadTo(414.361724854f, 266.355987549f, 424.516998291f, 266.355987549f);
    path2.quadTo(434.672271729f, 266.355987549f, 441.853149414f, 273.536865234f);
    path2.quadTo(449.033996582f, 280.717712402f, 449.033996582f, 290.87298584f);
    path2.close();

    testPathOp(reporter, path1, path2, kUnion_SkPathOp, filename);
}

static void cubicOp115(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,4, 2,1, 5,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(3,5, 1,0, 4,3);
    pathB.close();
    SkPath path2(path);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void testRect1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, path2;
    path.addRect(0, 0, 60, 60, SkPath::kCCW_Direction);
    path.addRect(30, 20, 50, 50, SkPath::kCCW_Direction);
    path.addRect(24, 20, 36, 30, SkPath::kCCW_Direction);
//    path.addRect(32, 24, 36, 41, SkPath::kCCW_Direction);
    testPathOp(reporter, path, path2, kUnion_SkPathOp, filename);
}

static void testRect2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addRect(0, 0, 1, 1, SkPath::kCW_Direction);
    path.addRect(4, 4, 5, 5, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.addRect(0, 0, 2, 2, SkPath::kCW_Direction);
    pathB.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp116(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(4,6, 2,0, 2,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(0,2, 1,0, 6,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp117(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(4,5, 6,0, 1,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(0,1, 1,0, 5,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp118(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(4,6, 5,1, 6,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,5);
    pathB.cubicTo(2,6, 1,0, 6,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void loop1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(1,5, -5.66666651f,3.33333349f, 8.83333302f,2.33333349f);
    path.close();
    pathB.moveTo(1,5);
    pathB.cubicTo(-5.66666651f,3.33333349f, 8.83333302f,2.33333349f, 0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

#include "SkPathOpsCubic.h"

static void loop1asQuad(skiatest::Reporter* reporter, const char* filename) {
    SkDCubic c1 = {{{0,1}, {1,5}, {-5.66666651f,3.33333349f}, {8.83333302f,2.33333349f}}};
    SkDCubic c2 = {{{1,5}, {-5.66666651f,3.33333349f}, {8.83333302f,2.33333349f}, {0,1}}};
    double c1InflectionTs[2], c2InflectionTs[2];
    SkDEBUGCODE(int c1InfTCount =) c1.findInflections(c1InflectionTs);
    SkASSERT(c1InfTCount == 2);
    SkDEBUGCODE(int c2InfTCount =) c2.findInflections(c2InflectionTs);
    SkASSERT(c2InfTCount == 1);
    SkASSERT(c1InflectionTs[0] > c1InflectionTs[1]);
    SkDCubicPair c1pair = c1.chopAt(c1InflectionTs[0]);
    SkDCubicPair c1apair = c1pair.first().chopAt(c1InflectionTs[1]);
    SkDCubicPair c2pair = c2.chopAt(c2InflectionTs[0]);
    SkDQuad q1[2] = { c1pair.first().toQuad(), c1pair.second().toQuad() };
    SkDQuad q1a[2] = { c1apair.first().toQuad(), c1apair.second().toQuad() };
    SkDQuad q2[2] = { c2pair.first().toQuad(), c2pair.second().toQuad() };
    SkPath path, pathB;
    path.moveTo(q1a[0].fPts[0].asSkPoint());
    path.quadTo(q1a[0].fPts[1].asSkPoint(), q1a[0].fPts[2].asSkPoint());
    path.quadTo(q1a[1].fPts[1].asSkPoint(), q1a[1].fPts[2].asSkPoint());
    path.quadTo(q1[1].fPts[1].asSkPoint(), q1[1].fPts[2].asSkPoint());
    path.close();
    pathB.moveTo(q2[0].fPts[0].asSkPoint());
    pathB.quadTo(q2[0].fPts[1].asSkPoint(), q2[0].fPts[2].asSkPoint());
    pathB.quadTo(q2[1].fPts[1].asSkPoint(), q2[1].fPts[2].asSkPoint());
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loop2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(3,4, 3.f,4.f, 4.5f,1.5f);
    path.close();
    pathB.moveTo(3,4);
    pathB.cubicTo(3.f,4.f, 4.5f,1.5f, 0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loop3(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(3,5, -3.66666651f,0, 10.5f,-1.66666651f);
    path.close();
    pathB.moveTo(3,5);
    pathB.cubicTo(-3.66666651f,0, 10.5f,-1.66666651f, 0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loop4(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,5);
    path.cubicTo(1,5, 1,4, 0.833333313f,3);
    path.close();
    pathB.moveTo(1,5);
    pathB.cubicTo(1,4, 0.833333313f,3, 0,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

#include "SkParsePath.h"

static void issue3517(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;

    const char str[] = "M31.35 57.75L31.35 57.75C31.9 57.7486 32.45 57.7948 33 57.7413C33.55 57.6878 34.1 57.5014 34.65 57.4291C35.2 57.3569 35.75 57.3223 36.3 57.3079C36.85 57.2935 37.4 57.3143 37.95 57.3428C38.5 57.3712 39.05 57.4112 39.6 57.4786C40.15 57.546 40.7 57.7029 41.25 57.7472C41.8 57.7916 42.35 57.7962 42.9 57.7445C43.45 57.6928 44 57.5345 44.55 57.4373C45.1 57.34 45.65 57.2115 46.2 57.1611C46.75 57.1107 47.3 57.1371 47.85 57.1349C48.4 57.1327 48.95 57.144 49.5 57.1478C50.05 57.1516 50.6 57.1553 51.15 57.1579C51.7 57.1605 52.25 57.1601 52.8 57.1634C53.35 57.1667 53.9 57.1731 54.45 57.1776C55 57.182 55.55 57.1916 56.1 57.19C56.65 57.1884 57.2 57.178 57.75 57.168C58.3 57.158 58.85 57.1355 59.4 57.1299C59.95 57.1243 60.5 57.1338 61.05 57.1345C61.6 57.1352 62.15 57.124 62.7 57.134C63.25 57.1441 63.8 57.1731 64.35 57.195C64.9 57.2169 65.45 57.2532 66 57.2655C66.55 57.2778 67.1 57.2647 67.65 57.2687C68.2 57.2728 68.75 57.267 69.3 57.2896C69.85 57.3122 70.4 57.371 70.95 57.4044C71.5 57.4377 72.05 57.4668 72.6 57.4896C73.15 57.5123 73.7 57.545 74.25 57.5408C74.8 57.5365 75.35 57.5068 75.9 57.4641C76.45 57.4213 77 57.3244 77.55 57.2842C78.1 57.244 78.65 57.2163 79.2 57.2228C79.75 57.2293 80.3 57.29 80.85 57.3232C81.4 57.3563 81.95 57.396 82.5 57.4219C83.05 57.4478 83.6 57.4637 84.15 57.4787C84.7 57.4937 85.25 57.5011 85.8 57.5121C86.35 57.523 86.9 57.5411 87.45 57.5444C88 57.5477 88.55 57.5663 89.1 57.5318C89.65 57.4972 90.2 57.3126 90.75 57.337C91.3 57.3613 91.85 57.6088 92.4 57.6776C92.95 57.7465 93.5 57.7379 94.05 57.75C94.6 57.7621 95.15 57.75 95.7 57.75L95.7 57.75L31.35 57.75Z";
    SkParsePath::FromSVGString(str, &path);

    const char strB[] = "M31.35 57.75L31.35 57.75C31.9 57.7514 32.45 57.7052 33 57.7587C33.55 57.8122 34.1 57.9986 34.65 58.0709C35.2 58.1431 35.75 58.1777 36.3 58.1921C36.85 58.2065 37.4 58.1857 37.95 58.1572C38.5 58.1288 39.05 58.0888 39.6 58.0214C40.15 57.954 40.7 57.7971 41.25 57.7528C41.8 57.7084 42.35 57.7038 42.9 57.7555C43.45 57.8072 44 57.9655 44.55 58.0627C45.1 58.16 45.65 58.2885 46.2 58.3389C46.75 58.3893 47.3 58.3629 47.85 58.3651C48.4 58.3673 48.95 58.356 49.5 58.3522C50.05 58.3484 50.6 58.3447 51.15 58.3421C51.7 58.3395 52.25 58.3399 52.8 58.3366C53.35 58.3333 53.9 58.3269 54.45 58.3224C55 58.318 55.55 58.3084 56.1 58.31C56.65 58.3116 57.2 58.322 57.75 58.332C58.3 58.342 58.85 58.3645 59.4 58.3701C59.95 58.3757 60.5 58.3662 61.05 58.3655C61.6 58.3648 62.15 58.376 62.7 58.366C63.25 58.3559 63.8 58.3269 64.35 58.305C64.9 58.2831 65.45 58.2468 66 58.2345C66.55 58.2222 67.1 58.2353 67.65 58.2313C68.2 58.2272 68.75 58.233 69.3 58.2104C69.85 58.1878 70.4 58.129 70.95 58.0956C71.5 58.0623 72.05 58.0332 72.6 58.0104C73.15 57.9877 73.7 57.955 74.25 57.9592C74.8 57.9635 75.35 57.9932 75.9 58.0359C76.45 58.0787 77 58.1756 77.55 58.2158C78.1 58.256 78.65 58.2837 79.2 58.2772C79.75 58.2707 80.3 58.21 80.85 58.1768C81.4 58.1437 81.95 58.104 82.5 58.0781C83.05 58.0522 83.6 58.0363 84.15 58.0213C84.7 58.0063 85.25 57.9989 85.8 57.9879C86.35 57.977 86.9 57.9589 87.45 57.9556C88 57.9523 88.55 57.9337 89.1 57.9682C89.65 58.0028 90.2 58.1874 90.75 58.163C91.3 58.1387 91.85 57.8912 92.4 57.8224C92.95 57.7535 93.5 57.7621 94.05 57.75C94.6 57.7379 95.15 57.75 95.7 57.75L95.7 57.75L31.35 57.75Z";
    SkParsePath::FromSVGString(strB, &pathB);
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubicOp119(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,5, 2,1, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(1,3, 1,0, 5,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp120(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(2,4, 2,1, 4,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(0,4, 1,0, 4,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp121(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,4, 3,2, 4,3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,3);
    pathB.cubicTo(3,4, 1,0, 4,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

// FIXME : haven't debugged this failure yet
static void cubicOp122(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,5, 4,1, 4,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,4);
    pathB.cubicTo(0,4, 1,0, 5,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp123(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,5, 2,0, 6,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(0,6, 1,0, 5,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void loop5(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,2);
    path.cubicTo(1,2, 1,1.66666663f, 0.833333313f,1.33333325f);
    path.close();
    pathB.moveTo(1,2);
    pathB.cubicTo(1,1.66666663f, 0.833333313f,1.33333325f, 0,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loop6(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(1,3, -1.66666675f,1.66666663f, 4.16666651f,1.00000012f);
    path.close();
    pathB.moveTo(1,3);
    pathB.cubicTo(-1.66666675f,1.66666663f, 4.16666651f,1.00000012f, 0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp124(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,5, 6,0, 3,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(0,3, 1,0, 5,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp125(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,6, 3,1, 6,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,3);
    pathB.cubicTo(2,6, 1,0, 6,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp126(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,3, 6,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(1,2, 1,0, 3,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp127(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(1,5, 6,0, 3,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(0,3, 1,0, 5,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp128(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(0,3, 3,2, 5,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,3);
    pathB.cubicTo(2,5, 1,0, 3,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp129(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    path.cubicTo(3,4, 2,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,2);
    pathB.cubicTo(1,2, 6,5, 4,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp130(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    path.cubicTo(4,6, 3,0, 2,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,3);
    pathB.cubicTo(1,2, 6,5, 6,4);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

#include "SkGeometry.h"

static void complex_to_quads(const SkPoint pts[], SkPath* path) {
    SkScalar loopT;
    if (SkDCubic::ComplexBreak(pts, &loopT)) {
        SkPoint cubicPair[7];
        SkChopCubicAt(pts, cubicPair, loopT);
        SkDCubic c1, c2;
        c1.set(cubicPair);
        c2.set(&cubicPair[3]);
        SkDQuad q1 = c1.toQuad();
        SkDQuad q2 = c2.toQuad();
        path->quadTo(q1[1].asSkPoint(), q1[2].asSkPoint());
        path->quadTo(q2[1].asSkPoint(), q2[2].asSkPoint());
    } else {
        path->cubicTo(pts[1], pts[2], pts[3]);
    }
}

static void cubicOp130a(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    SkPoint pts[] = { {5,6}, {4,6}, {3,0}, {2,1} };
    complex_to_quads(pts, &path);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,3);
    SkPoint pts2[] = { {0,3}, {1,2}, {6,5}, {6,4} };
    complex_to_quads(pts2, &path);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp131(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,1);
    path.cubicTo(3,4, 3,0, 6,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,3);
    pathB.cubicTo(2,6, 1,0, 4,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void circlesOp1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addCircle(0, 1, 2, SkPath::kCCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addCircle(0, 1, 1, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void circlesOp2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addCircle(0, 1, 4, SkPath::kCCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addCircle(0, 4, 3, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void rRect1x(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kEvenOdd_FillType);
    path.moveTo(20.65f, 5.65f);
    path.conicTo(20.65f, 1.13612f, 25.1404f, 0.65f, 0.888488f);
    path.lineTo(25.65f, 0.65f);
    path.lineTo(26.1596f, 0.67604f);
    path.conicTo(30.65f, 1.13612f, 30.65f, 5.65f, 0.888488f);
    path.lineTo(30.65f, 25.65f);
    path.conicTo(30.65f, 20.65f, 25.65f, 20.65f, 0.707107f);
    path.lineTo(20.65f, 20.65f);
    path.lineTo(20.65f, 5.65f);
    path.close();
    path.moveTo(20.65f, 20.65f);
    path.lineTo(5.65f, 20.65f);
    path.conicTo(0.65f, 20.65f, 0.65f, 25.65f, 0.707107f);
    path.lineTo(0.65f, 45.65f);
    path.conicTo(0.65f, 50.65f, 5.65f, 50.65f, 0.707107f);
    path.lineTo(25.65f, 50.65f);
    path.conicTo(30.65f, 50.65f, 30.65f, 45.65f, 0.707107f);
    path.lineTo(30.65f, 25.65f);
    path.conicTo(30.65f, 30.65f, 25.65f, 30.65f, 0.707107f);
    path.conicTo(20.65f, 30.65f, 20.65f, 25.65f, 0.707107f);
    path.lineTo(20.65f, 20.65f);
    path.close();
    SkPath path1(path);

    path.reset();
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(20.65f, 45.65f);
    path.lineTo(20.65f, 25.65f);
    path.conicTo(20.65f, 20.65f, 25.65f, 20.65f, 0.707107f);
    path.lineTo(45.65f, 20.65f);
    path.conicTo(50.65f, 20.65f, 50.65f, 25.65f, 0.707107f);
    path.lineTo(50.65f, 45.65f);
    path.conicTo(50.65f, 50.65f, 45.65f, 50.65f, 0.707107f);
    path.lineTo(25.65f, 50.65f);
    path.conicTo(20.65f, 50.65f, 20.65f, 45.65f, 0.707107f);
    path.close();
    SkPath path2(path);

    testPathOp(reporter, path1, path2, kDifference_SkPathOp, filename);
}

static void loop7(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(3,4, -1,0, 8.5f,-2.5f);
    path.close();
    pathB.moveTo(3,4);
    pathB.cubicTo(-1,0, 8.5f,-2.5f, 0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void rects5(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addRect(5, 5, 6, 6, SkPath::kCW_Direction);
    path.addRect(5, 5, 6, 6, SkPath::kCW_Direction);
    pathB.setFillType(SkPath::kEvenOdd_FillType);
    pathB.addRect(0, 0, 6, 6, SkPath::kCW_Direction);
    pathB.addRect(5, 5, 6, 6, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void loop8(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(1,4, -3.83333325f,0.166666627f, 6,-1);
    path.close();
    pathB.moveTo(1,4);
    pathB.cubicTo(-3.83333325f,0.166666627f, 6,-1, 0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loop9(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(1,3, -2.5f,0, 3.33333325f,-0.666666627f);
    path.close();
    pathB.moveTo(1,3);
    pathB.cubicTo(-2.5f,0, 3.33333325f,-0.666666627f, 0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void circlesOp3(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.addCircle(0, 1, 2, SkPath::kCCW_Direction);
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.addCircle(3, 5, 3, SkPath::kCW_Direction);
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void loop10(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(5,6);
    path.cubicTo(1,2, 1,2, -3.66666651f,13.333334f);
    path.close();
    pathB.moveTo(1,2);
    pathB.cubicTo(1,2, -3.66666651f,13.333334f, 5,6);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loop11(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(0,1);
    path.cubicTo(1,3, -1.83333349f,1.33333337f, 4,-1);
    path.close();
    pathB.moveTo(1,3);
    pathB.cubicTo(-1.83333349f,1.33333337f, 4,-1, 0,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp132(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    path.cubicTo(3,4, 3,0, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,3);
    pathB.cubicTo(2,3, 6,5, 4,3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void loop12(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(1,2);
    path.cubicTo(0,6, -3.16666675f,3.66666675f, 6.33333349f,3.33333349f);
    path.close();
    pathB.moveTo(0,6);
    pathB.cubicTo(-3.16666675f,3.66666675f, 6.33333349f,3.33333349f, 1,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp133(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    path.cubicTo(5,6, 5,0, 4,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(1,4, 6,5, 6,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp134(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    path.cubicTo(5,6, 6,0, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(1,3, 6,5, 6,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp135(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    path.cubicTo(5,6, 6,0, 4,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,6);
    pathB.cubicTo(1,4, 6,5, 6,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp136(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    path.cubicTo(5,6, 5,0, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(1,3, 6,5, 6,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp136a(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    path.quadTo(5,0, 3,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0,5);
    pathB.cubicTo(1,3, 6,5, 6,5);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics137(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 5);
    path.cubicTo(3, 6, 1, 0, 3, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 1);
    pathB.cubicTo(2, 3, 5, 0, 6, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics138(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 5);
    path.cubicTo(3, 6, 1, 0, 4, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 1);
    pathB.cubicTo(2, 4, 5, 0, 6, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

// three curves intersect successfully nearby -- the angle only gets 2 of the 3 pts
static void cubicOp139(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(0,4, 3,1, 5,1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,3);
    pathB.cubicTo(1,5, 2,0, 4,0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp140(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(1,2, 5,4, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(4,5);
    pathB.cubicTo(2,3, 2,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp141(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0,2);
    path.cubicTo(1,2, 6,4, 3,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(4,6);
    pathB.cubicTo(2,3, 2,0, 2,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void quadRect1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(6,15);
    path.quadTo(16,0, 8,4);
    path.quadTo(2,7, 12,12);
    path.close();
    pathB.addRect(4,11, 13,16);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void quadRect2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(5,12);
    path.quadTo(15,7, 9,4);
    path.quadTo(1,0, 11,15);
    path.close();
    pathB.addRect(4,11, 13,16);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void quadRect3(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(12,12);
    path.quadTo(2,7, 8,4);
    path.quadTo(16,0, 6,15);
    path.close();
    pathB.addRect(4,11, 13,16);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void quadRect4(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(11,15);
    path.quadTo(1,0, 9,4);
    path.quadTo(15,7, 5,12);
    path.close();
    pathB.addRect(4,11, 13,16);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void quadRect5(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(11,13);
    path.quadTo(4,4, 8,4);
    path.quadTo(12,4, 5,13);
    path.close();
    pathB.addRect(4,11, 13,16);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void quadRect6(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(5,13);
    path.quadTo(12,4, 8,4);
    path.quadTo(4,4, 11,13);
    path.close();
    pathB.addRect(4,11, 13,16);
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops4i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 3);
    path.cubicTo(0, 2, 0, 2, -1.66666663f, 2.16666675f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 2);
    pathB.cubicTo(0, 2, -1.66666663f, 2.16666675f, 0, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops5i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 2);
    path.cubicTo(0, 2, 0, 2, 0.166666672f, 2.66666675f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 2);
    pathB.cubicTo(0, 2, 0.166666672f, 2.66666675f, 1, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubicOp142(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(5,6);
    path.cubicTo(2,5, 2,1, 1,0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1,2);
    pathB.cubicTo(0,1, 6,5, 5,2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics6d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 5);
    path.cubicTo(1, 5, 4, 2, 4, 0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 4);
    pathB.cubicTo(0, 4, 5, 3, 5, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics7d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 6);
    path.cubicTo(2, 4, 5, 1, 3, 1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 5);
    pathB.cubicTo(1, 3, 6, 2, 4, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics8d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 5);
    path.cubicTo(2, 4, 5, 1, 3, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 5);
    pathB.cubicTo(2, 3, 5, 2, 4, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics9d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 4);
    path.cubicTo(2, 6, 3, 1, 5, 1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 3);
    pathB.cubicTo(1, 5, 4, 2, 6, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics10u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 4);
    path.cubicTo(1, 6, 4, 1, 5, 1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 4);
    pathB.cubicTo(1, 5, 4, 2, 6, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void cubics11i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 4);
    path.cubicTo(2, 5, 3, 2, 5, 1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 3);
    pathB.cubicTo(1, 5, 4, 2, 5, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubics12d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 4);
    path.cubicTo(0, 4, 5, 3, 5, 1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3, 5);
    pathB.cubicTo(1, 5, 4, 2, 4, 0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics13d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 3);
    path.cubicTo(1, 5, 4, 2, 5, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 4);
    pathB.cubicTo(2, 5, 3, 2, 5, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics14d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 3);
    path.cubicTo(0, 4, 3, 1, 3, 0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 3);
    pathB.cubicTo(0, 3, 3, 2, 4, 0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics15d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 5);
    path.cubicTo(3, 5, 4, 0, 4, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 4);
    pathB.cubicTo(2, 4, 5, 1, 5, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics16i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 5);
    path.cubicTo(2, 5, 5, 0, 4, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 5);
    pathB.cubicTo(2, 4, 5, 1, 5, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubics17d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 5);
    path.cubicTo(3, 4, 4, 1, 4, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 4);
    pathB.cubicTo(2, 4, 5, 1, 4, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics18d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 5);
    path.cubicTo(1, 3, 4, 0, 2, 0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 4);
    pathB.cubicTo(0, 2, 5, 1, 3, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics19d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 5);
    path.cubicTo(2, 3, 5, 2, 4, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 5);
    pathB.cubicTo(2, 4, 5, 1, 3, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubicOp157(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1,5);
    path.cubicTo(1,3, 6,2, 4,2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2,6);
    pathB.cubicTo(2,4, 5,1, 3,1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics20d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 2);
    path.cubicTo(0, 3, 6, 0, 3, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 6);
    pathB.cubicTo(2, 3, 2, 1, 3, 0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void loops20i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 2);
    path.cubicTo(0, 2, 0.833333313f, 2, 1, 3.66666651f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 2);
    pathB.cubicTo(0.833333313f, 2, 1, 3.66666651f, 1, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops21i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 2);
    path.cubicTo(0, 2, 0.833333313f, 2, 1, 4);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 2);
    pathB.cubicTo(0.833333313f, 2, 1, 4, 1, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops22i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 3);
    path.cubicTo(0, 3, 0.833333313f, 3, 1, 4.66666651f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 3);
    pathB.cubicTo(0.833333313f, 3, 1, 4.66666651f, 1, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops23i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 5);
    path.cubicTo(0, 1, 6.16666698f, 5.66666698f, -5.66666651f, 6.66666651f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 1);
    pathB.cubicTo(6.16666698f, 5.66666698f, -5.66666651f, 6.66666651f, 1, 5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops24i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 2);
    path.cubicTo(0, 2, 0.833333313f, 2, 1, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 2);
    pathB.cubicTo(0.833333313f, 2, 1, 3, 1, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops25i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 5);
    path.cubicTo(0, 5, 0.833333313f, 5, 1, 7);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 5);
    pathB.cubicTo(0.833333313f, 5, 1, 7, 1, 5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops26i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 6);
    path.cubicTo(0, 2, 6.16666698f, 6.66666698f, -5.66666651f, 7.66666651f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 2);
    pathB.cubicTo(6.16666698f, 6.66666698f, -5.66666651f, 7.66666651f, 1, 6);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops27i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 3);
    path.cubicTo(0, 3, 0.833333313f, 3, 1, 4.33333349f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 3);
    pathB.cubicTo(0.833333313f, 3, 1, 4.33333349f, 1, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops28i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 3);
    path.cubicTo(1, 3, 1.83333337f, 3, 2, 4.66666651f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 3);
    pathB.cubicTo(1.83333337f, 3, 2, 4.66666651f, 2, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops29i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 4);
    path.cubicTo(0, 4, 1.66666663f, 4, 2, 7.33333302f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 4);
    pathB.cubicTo(1.66666663f, 4, 2, 7.33333302f, 2, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops30i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 4);
    path.cubicTo(0, 4, 1.66666663f, 4, 2, 8);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 4);
    pathB.cubicTo(1.66666663f, 4, 2, 8, 2, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops31i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 5);
    path.cubicTo(1, 5, 1.83333337f, 5, 2, 6.66666651f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 5);
    pathB.cubicTo(1.83333337f, 5, 2, 6.66666651f, 2, 5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops32i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 6);
    path.cubicTo(1, 6, 1.83333337f, 6, 2, 8);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 6);
    pathB.cubicTo(1.83333337f, 6, 2, 8, 2, 6);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops33i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 6);
    path.cubicTo(1, 2, 7.16666698f, 6.66666698f, -4.66666651f, 7.66666651f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(7.16666698f, 6.66666698f, -4.66666651f, 7.66666651f, 2, 6);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops33iMod(skiatest::Reporter* reporter, const char* filename) {
    SkPoint pts[] = {{2, 6}, {1, 2}, {7.16666698f, 6.66666698f}, {-4.66666651f, 7.66666651f},
                     {1, 2}, {7.16666698f, 6.66666698f}, {-4.66666651f, 7.66666651f}, {2, 6}};
    bool up = false;
    float offset = 0.0380172729f;
    float step = 7.62939453e-006f;
    bool lastResult = true;
 //   for (int i = 0; i < 30; ++i) {
        SkString name(filename);
 //       name.appendS32(i);
 //       if (i > 0) {
 //           SkDebugf("\n\n<div id=\"%s\">\n", name.c_str());
 //       }
        pts[5].fY = 6.66666698f + offset;
        SkPath path, pathB;
        path.setFillType(SkPath::kWinding_FillType);
        path.moveTo(pts[0]);
        path.cubicTo(pts[1], pts[2], pts[3]);
        path.close();
        pathB.setFillType(SkPath::kWinding_FillType);
        pathB.moveTo(pts[4]);
        pathB.cubicTo(pts[5], pts[6], pts[7]);
        pathB.close();
        bool result = testPathOp(reporter, path, pathB, kIntersect_SkPathOp, name.c_str());
        if (lastResult != result) {
            up = !up;
        }
        step /= 2;
        offset += up ? step : -step;
        lastResult = result;
 //   }
}


static void loops33iAsQuads(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 6);
    path.cubicTo(1, 2, 7.16666698f, 6.66666698f, -4.66666651f, 7.66666651f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(7.16666698f, 6.66666698f, -4.66666651f, 7.66666651f, 2, 6);
    pathB.close();
    SkPath qPath, qPathB;
    CubicPathToQuads(path, &qPath);
    CubicPathToQuads(pathB, &qPathB);
    testPathOp(reporter, qPath, qPathB, kIntersect_SkPathOp, filename);
}

static void loops34i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 4);
    path.cubicTo(0, 4, 2.5f, 4, 3, 9);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 4);
    pathB.cubicTo(2.5f, 4, 3, 9, 3, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops35i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 4);
    path.cubicTo(0, 4, 2.5f, 4, 3, 10);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 4);
    pathB.cubicTo(2.5f, 4, 3, 10, 3, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops36i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 4);
    path.cubicTo(1, 4, 2.66666675f, 4, 3, 8);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 4);
    pathB.cubicTo(2.66666675f, 4, 3, 8, 3, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops37i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 4);
    path.cubicTo(1, 4, 1.83333337f, 4, 2, 5.33333349f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 4);
    pathB.cubicTo(1.83333337f, 4, 2, 5.33333349f, 2, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops38i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 4);
    path.cubicTo(2, 4, 2.83333325f, 4, 3, 6);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 4);
    pathB.cubicTo(2.83333325f, 4, 3, 6, 3, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops39i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 5);
    path.cubicTo(0, 5, 2.5f, 5, 3, 10);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 5);
    pathB.cubicTo(2.5f, 5, 3, 10, 3, 5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops40i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 5);
    path.cubicTo(0, 5, 2.5f, 5, 3, 11);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 5);
    pathB.cubicTo(2.5f, 5, 3, 11, 3, 5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops40iAsQuads(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 5);
    path.cubicTo(0, 5, 2.5f, 5, 3, 11);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 5);
    pathB.cubicTo(2.5f, 5, 3, 11, 3, 5);
    pathB.close();
    SkPath qPath, qPathB;
    CubicPathToQuads(path, &qPath);
    CubicPathToQuads(pathB, &qPathB);
    testPathOp(reporter, qPath, qPathB, kIntersect_SkPathOp, filename);
}

static void loops44i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 5);
    path.cubicTo(0, 1, 7.33333302f, 5.33333349f, -7, 7);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 1);
    pathB.cubicTo(7.33333302f, 5.33333349f, -7, 7, 1, 5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops45i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 6);
    path.cubicTo(0, 2, 7.33333302f, 6.33333302f, -7, 8);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 2);
    pathB.cubicTo(7.33333302f, 6.33333302f, -7, 8, 1, 6);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops46i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 6);
    path.cubicTo(1, 2, 8.33333302f, 6.33333302f, -6, 8);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(8.33333302f, 6.33333302f, -6, 8, 2, 6);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

/*
FAILED: d:\cygwin\puregit\tests\pathopsextendedtest.cpp:346    0 */
static void loops47i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 4);
    path.cubicTo(0, 1, 6, 5.83333302f, -4, 8);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 1);
    pathB.cubicTo(6, 5.83333302f, -4, 8, 2, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops48i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 6);
    path.cubicTo(0, 1, 9.33333302f, 6.83333302f, -8.33333302f, 9.16666603f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 1);
    pathB.cubicTo(9.33333302f, 6.83333302f, -8.33333302f, 9.16666603f, 2, 6);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops49i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 2);
    path.cubicTo(1, 4, -0.166666687f, 2.66666675f, 1.66666675f, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 4);
    pathB.cubicTo(-0.166666687f, 2.66666675f, 1.66666675f, 2, 0, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops50i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 3);
    path.cubicTo(1, 5, -0.166666687f, 3.66666675f, 1.66666675f, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 5);
    pathB.cubicTo(-0.166666687f, 3.66666675f, 1.66666675f, 3, 0, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops51i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 2);
    path.cubicTo(2, 4, 0.833333313f, 2.66666675f, 2.66666675f, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 4);
    pathB.cubicTo(0.833333313f, 2.66666675f, 2.66666675f, 2, 1, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops52i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 3);
    path.cubicTo(2, 5, 0.833333313f, 3.66666675f, 2.66666675f, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 5);
    pathB.cubicTo(0.833333313f, 3.66666675f, 2.66666675f, 3, 1, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops53i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 3);
    path.cubicTo(3, 5, 1.83333325f, 3.66666675f, 3.66666651f, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3, 5);
    pathB.cubicTo(1.83333325f, 3.66666675f, 3.66666651f, 3, 2, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops54i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 2);
    path.cubicTo(1, 4, 0, 3, 1.66666675f, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 4);
    pathB.cubicTo(0, 3, 1.66666675f, 2, 0, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops55i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 3);
    path.cubicTo(1, 5, 0, 4, 1.66666675f, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 5);
    pathB.cubicTo(0, 4, 1.66666675f, 3, 0, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops56i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 2);
    path.cubicTo(2, 4, 0.99999994f, 3, 2.66666675f, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 4);
    pathB.cubicTo(0.99999994f, 3, 2.66666675f, 2, 1, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops57i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 3);
    path.cubicTo(2, 5, 0.99999994f, 4, 2.66666675f, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 5);
    pathB.cubicTo(0.99999994f, 4, 2.66666675f, 3, 1, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops58i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 3);
    path.cubicTo(3, 5, 2, 4, 3.66666651f, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3, 5);
    pathB.cubicTo(2, 4, 3.66666651f, 3, 2, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops58iAsQuads(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 3);
    path.cubicTo(3, 5, 2, 4, 3.66666651f, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3, 5);
    pathB.cubicTo(2, 4, 3.66666651f, 3, 2, 3);
    pathB.close();
    SkPath qPath, qPathB;
    CubicPathToQuads(path, &qPath);
    CubicPathToQuads(pathB, &qPathB);
//    SkPoint from = {2.61714339f,1.90228665f};
//    SkPoint to = {2.617045833359139f,1.9013528935803314f};
//    path_edit(from, to, &qPathB);
    testPathOp(reporter, qPath, qPathB, kIntersect_SkPathOp, filename);
}

static void loops59i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 6);
    path.cubicTo(1, 2, 7.33333302f, 1.66666663f, -7.5f, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(7.33333302f, 1.66666663f, -7.5f, 2, 0, 6);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops59iasQuads(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 6);
    path.cubicTo(1, 2, 7.33333302f, 1.66666663f, -7.5f, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 2);
    pathB.cubicTo(7.33333302f, 1.66666663f, -7.5f, 2, 0, 6);
    pathB.close();
    SkPath qPath, qPathB;
    CubicPathToQuads(path, &qPath);
    CubicPathToQuads(pathB, &qPathB);
    SkPoint from = {2.61714339f,1.90228665f};
    SkPoint to = {2.617045833359139f,1.9013528935803314f};
    path_edit(from, to, &qPathB);
    testPathOp(reporter, qPath, qPathB, kIntersect_SkPathOp, filename);
}

static void cubics41d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(1, 4, 3, 0, 3, 1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 3);
    pathB.cubicTo(1, 3, 1, 0, 4, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

void loops61i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(1, 5, -6.33333302f, 0.666666627f, 8, -1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 5);
    pathB.cubicTo(-6.33333302f, 0.666666627f, 8, -1, 0, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops62i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 2);
    path.cubicTo(1, 6, -6.33333302f, 1.66666663f, 8, 0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 6);
    pathB.cubicTo(-6.33333302f, 1.66666663f, 8, 0, 0, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops63i(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(2, 4, -4, -0.833333254f, 6, -3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 4);
    pathB.cubicTo(-4, -0.833333254f, 6, -3, 0, 1);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubics44d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 4);
    path.cubicTo(2, 5, 3, 1, 6, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 3);
    pathB.cubicTo(2, 6, 4, 3, 5, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics45u(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 3);
    path.cubicTo(2, 6, 4, 3, 5, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(3, 4);
    pathB.cubicTo(2, 5, 3, 1, 6, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kUnion_SkPathOp, filename);
}

static void fuzz38(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.moveTo(100.34f, 303.312f);
    path.lineTo(-1e+08, 303.312f);
    path.lineTo(102, 310.156f);
    path.lineTo(100.34f, 310.156f);
    path.lineTo(100.34f, 303.312f);
    path.close();
    testPathOpCheck(reporter, path, pathB, kUnion_SkPathOp, filename, true);
}

static void crbug_526025(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x43b40000), SkBits2Float(0xcf000000));  // 360, -2.14748e+09f
path.cubicTo(SkBits2Float(0x4e0d628f), SkBits2Float(0xceffffff), SkBits2Float(0x4e800003), SkBits2Float(0xcec6b143), SkBits2Float(0x4e800002), SkBits2Float(0xce7ffffc));  // 5.93012e+08f, -2.14748e+09f, 1.07374e+09f, -1.66675e+09f, 1.07374e+09f, -1.07374e+09f
path.cubicTo(SkBits2Float(0x4e800002), SkBits2Float(0xcde53aee), SkBits2Float(0x4e0d6292), SkBits2Float(0xc307820e), SkBits2Float(0x44627d00), SkBits2Float(0x437ffff2));  // 1.07374e+09f, -4.80731e+08f, 5.93012e+08f, -135.508f, 905.953f, 256
path.lineTo(SkBits2Float(0x444bf3bc), SkBits2Float(0x4460537e));  // 815.808f, 897.305f
path.lineTo(SkBits2Float(0x43553abd), SkBits2Float(0x440f3cbd));  // 213.229f, 572.949f
path.lineTo(SkBits2Float(0x42000000), SkBits2Float(0x41800000));  // 32, 16
path.lineTo(SkBits2Float(0x42c80000), SkBits2Float(0x44000000));  // 100, 512
path.lineTo(SkBits2Float(0x43553abd), SkBits2Float(0x440f3cbd));  // 213.229f, 572.949f
path.lineTo(SkBits2Float(0x43b40000), SkBits2Float(0x44800000));  // 360, 1024
path.lineTo(SkBits2Float(0x43b40000), SkBits2Float(0x45816000));  // 360, 4140

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x42fe0000), SkBits2Float(0x43a08000));  // 127, 321
path.lineTo(SkBits2Float(0x45d5c000), SkBits2Float(0x43870000));  // 6840, 270
path.lineTo(SkBits2Float(0xd0a00000), SkBits2Float(0x4cbebc20));  // -2.14748e+10f, 1e+08
path.lineTo(SkBits2Float(0x451f7000), SkBits2Float(0x42800000));  // 2551, 64
path.lineTo(SkBits2Float(0x42fe0000), SkBits2Float(0x43a08000));  // 127, 321
path.close();

    SkPath path2(path);
    testPathOp(reporter, path1, path2, (SkPathOp) 2, filename);
}

static void fuzzX_392(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType(SkPath::kEvenOdd_FillType);
path.moveTo(SkBits2Float(0x41e80000), SkBits2Float(0x43bde212));  // 29, 379.766f
path.lineTo(SkBits2Float(0x41e80000), SkBits2Float(0x43bdc7ef));  // 29, 379.562f
path.conicTo(SkBits2Float(0x42a5861e), SkBits2Float(0x43c61f86), SkBits2Float(0x430b0610), SkBits2Float(0x43c61f86), SkBits2Float(0x3f7d23f3));  // 82.7619f, 396.246f, 139.024f, 396.246f, 0.98883f
path.conicTo(SkBits2Float(0x42a58e20), SkBits2Float(0x43c61f86), SkBits2Float(0x41e80000), SkBits2Float(0x43bde212), SkBits2Float(0x3f7d2cf5));  // 82.7776f, 396.246f, 29, 379.766f, 0.988967f
path.close();

    SkPath path1(path);
    path.setFillType(SkPath::kWinding_FillType);
path.moveTo(SkBits2Float(0xc36c7bd8), SkBits2Float(0xc3a31d72));  // -236.484f, -326.23f
path.lineTo(SkBits2Float(0xc367a4ae), SkBits2Float(0xc3a31d72));  // -231.643f, -326.23f
path.lineTo(SkBits2Float(0x430b0610), SkBits2Float(0x43c61f86));  // 139.024f, 396.246f
path.lineTo(SkBits2Float(0xc36c7bd8), SkBits2Float(0x43c61f86));  // -236.484f, 396.246f

    SkPath path2(path);
    testPathOp(reporter, path1, path2, kIntersect_SkPathOp, filename);
}

static void dean2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x3f2b74b3), SkBits2Float(0x4154a02b)); // 0.669749f, 13.2891f
path.cubicTo(SkBits2Float(0x3f2b74b3), SkBits2Float(0x4154a02b), SkBits2Float(0x41531912), SkBits2Float(0x3f130322), SkBits2Float(0x4154a02b), SkBits2Float(0x3f2b74b3)); // 0.669749f, 13.2891f, 13.1936f, 0.574267f, 13.2891f, 0.669749f
path.cubicTo(SkBits2Float(0x414a835a), SkBits2Float(0x3ec07ba6), SkBits2Float(0x413fcc0d), SkBits2Float(0x3e193319), SkBits2Float(0x4134a02b), SkBits2Float(0x00000000)); // 12.6571f, 0.375943f, 11.9873f, 0.149609f, 11.2891f, 0
path.lineTo(SkBits2Float(0x3f2b74b3), SkBits2Float(0x4154a02b)); // 0.669749f, 13.2891f
path.close();
    SkPath path1(path);

    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x3f2b74b3), SkBits2Float(0x4154a02b)); // 0.669749f, 13.2891f
path.cubicTo(SkBits2Float(0x3f2b74b3), SkBits2Float(0x4154a02b), SkBits2Float(0x41531912), SkBits2Float(0x3f130322), SkBits2Float(0x4154a02b), SkBits2Float(0x3f2b74b3)); // 0.669749f, 13.2891f, 13.1936f, 0.574267f, 13.2891f, 0.669749f
path.lineTo(SkBits2Float(0x417ab74b), SkBits2Float(0x4154a02b)); // 15.6697f, 13.2891f
path.lineTo(SkBits2Float(0x3f2b74b3), SkBits2Float(0x4154a02b)); // 0.669749f, 13.2891f
path.close();
    SkPath path2(path);
    testPathOp(reporter, path1, path2, kIntersect_SkPathOp, filename);
}

static void cubics_d(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(3, 5, 1, 0, 3, 0);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 1);
    pathB.cubicTo(0, 3, 1, 0, 5, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics_d2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(0, 1);
    path.cubicTo(2, 5, 2, 0, 2, 1);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 2);
    pathB.cubicTo(1, 2, 1, 0, 5, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void loops_i1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 3);
    path.cubicTo(0, 4, -0.333333343f, 4.66666651f, 3, 5.83333349f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 4);
    pathB.cubicTo(-0.333333343f, 4.66666651f, 3, 5.83333349f, 2, 3);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops_i2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 4);
    path.cubicTo(0, 5, -0.333333343f, 5.66666651f, 3, 6.83333302f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 5);
    pathB.cubicTo(-0.333333343f, 5.66666651f, 3, 6.83333302f, 2, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops_i3(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(2, 5);
    path.cubicTo(0, 6, -0.333333343f, 6.66666651f, 3, 7.83333302f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 6);
    pathB.cubicTo(-0.333333343f, 6.66666651f, 3, 7.83333302f, 2, 5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops_i4(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 4);
    path.cubicTo(1, 5, 0.666666627f, 5.66666651f, 4, 6.83333302f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 5);
    pathB.cubicTo(0.666666627f, 5.66666651f, 4, 6.83333302f, 3, 4);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops_i5(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 5);
    path.cubicTo(1, 6, 0.666666627f, 6.66666651f, 4, 7.83333302f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 6);
    pathB.cubicTo(0.666666627f, 6.66666651f, 4, 7.83333302f, 3, 5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void loops_i6(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(4, 5);
    path.cubicTo(2, 6, 1.66666663f, 6.66666651f, 5, 7.83333302f);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(2, 6);
    pathB.cubicTo(1.66666663f, 6.66666651f, 5, 7.83333302f, 4, 5);
    pathB.close();
    testPathOp(reporter, path, pathB, kIntersect_SkPathOp, filename);
}

static void cubics_d3(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(3, 4);
    path.cubicTo(0, 6, 6, 1, 4, 2);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(1, 6);
    pathB.cubicTo(2, 4, 4, 3, 6, 0);
    pathB.close();
    testPathOp(reporter, path, pathB, kDifference_SkPathOp, filename);
}

static void cubics_o(skiatest::Reporter* reporter, const char* filename) {
    SkPath path, pathB;
    path.setFillType(SkPath::kWinding_FillType);
    path.moveTo(1, 4);
    path.cubicTo(2, 6, 5, 0, 5, 3);
    path.close();
    pathB.setFillType(SkPath::kWinding_FillType);
    pathB.moveTo(0, 5);
    pathB.cubicTo(3, 5, 4, 1, 6, 2);
    pathB.close();
    testPathOp(reporter, path, pathB, kXOR_SkPathOp, filename);
}


static void (*skipTest)(skiatest::Reporter* , const char* filename) = 0;
static void (*firstTest)(skiatest::Reporter* , const char* filename) = cubics_d3;
static void (*stopTest)(skiatest::Reporter* , const char* filename) = 0;

#define TEST(name) { name, #name }

static struct TestDesc tests[] = {
    TEST(loops_i1),
    TEST(loops_i2),
    TEST(loops_i3),
    TEST(loops_i4),
    TEST(loops_i5),
    TEST(loops_i6),
    TEST(cubics_d3),
    TEST(cubics_o),
    TEST(cubics_d2),
    TEST(cubics_d),
    TEST(dean2),
    TEST(fuzzX_392),
    TEST(crbug_526025),
    TEST(fuzz38),
    TEST(cubics44d),
    TEST(cubics45u),
    TEST(loops61i),
    TEST(loops62i),
    TEST(loops63i),
    TEST(loops58iAsQuads),
    TEST(cubics41d),
    TEST(loops59iasQuads),
    TEST(loops59i),
    TEST(loops44i),
    TEST(loops45i),
    TEST(loops46i),
    TEST(loops47i),
    TEST(loops48i),
    TEST(loops49i),
    TEST(loops50i),
    TEST(loops51i),
    TEST(loops52i),
    TEST(loops53i),
    TEST(loops54i),
    TEST(loops55i),
    TEST(loops56i),
    TEST(loops57i),
    TEST(loops58i),
    TEST(loops33iMod),
    TEST(loops33iAsQuads),
    TEST(loops33i),
    TEST(loops40i),
    TEST(loops40iAsQuads),
    TEST(loops39i),
    TEST(loops38i),
    TEST(loops37i),
    TEST(loops36i),
    TEST(loops35i),
    TEST(loops34i),
    TEST(loops32i),
    TEST(loops31i),
    TEST(loops30i),
    TEST(loops29i),
    TEST(loops28i),
    TEST(loops27i),
    TEST(loops26i),
    TEST(loops25i),
    TEST(loops24i),
    TEST(loops23i),
    TEST(loops22i),
    TEST(loops21i),
    TEST(loops20i),
    TEST(cubics20d),
    TEST(cubics6d),
    TEST(cubics7d),
    TEST(cubics8d),
    TEST(cubics9d),
    TEST(cubics10u),
    TEST(cubics11i),
    TEST(cubics12d),
    TEST(cubics13d),
    TEST(cubics14d),
    TEST(cubics15d),
    TEST(cubics16i),
    TEST(cubics17d),
    TEST(cubics18d),
    TEST(cubics19d),
    TEST(cubicOp157),
    TEST(cubicOp142),
    TEST(loops4i),
    TEST(quadRect1),
    TEST(quadRect2),
    TEST(quadRect3),
    TEST(quadRect4),
    TEST(quadRect5),
    TEST(quadRect6),
    TEST(cubicOp141),
    TEST(cubicOp58d),
    TEST(loops5i),
    TEST(cubicOp140),
    TEST(cubicOp139),
    TEST(cubics138),
    TEST(cubics137),
    TEST(cubicOp136a),
    TEST(cubicOp136),
    TEST(cubicOp135),
    TEST(cubicOp134),
    TEST(cubicOp133),
    TEST(loop12),
    TEST(cubicOp132),
    TEST(loop11),
    TEST(loop10),
    TEST(circlesOp3),
    TEST(loop9),
    TEST(loop8),
    TEST(rects5),
    TEST(loop7),
    TEST(cubicOp130a),
    TEST(rRect1x),
    TEST(circlesOp2),
    TEST(circlesOp1),
    TEST(cubicOp131),
    TEST(cubicOp130),
    TEST(cubicOp129),
    TEST(cubicOp128),
    TEST(cubicOp127),
    TEST(cubicOp126),
    TEST(cubicOp125),
    TEST(cubicOp124),
    TEST(loop6),
    TEST(loop5),
    TEST(cubicOp123),
    TEST(cubicOp122),
    TEST(cubicOp121),
    TEST(cubicOp120),
    TEST(cubicOp119),
    TEST(loop4),
    TEST(loop3),
    TEST(loop2),
    TEST(loop1asQuad),
    TEST(loop1),
    TEST(issue3517),
    TEST(cubicOp118),
    TEST(cubicOp117),
    TEST(cubicOp116),
    TEST(testRect2),
    TEST(testRect1),
    TEST(cubicOp115),
    TEST(issue2753),
    TEST(cubicOp114),
    TEST(issue2808),
    TEST(cubicOp114asQuad),
    TEST(rects4),
    TEST(rects3),
    TEST(rects2),
    TEST(rects1),
    TEST(issue2540),
    TEST(issue2504),
    TEST(kari1),
    TEST(quadOp10i),
    TEST(cubicOp113),
    TEST(skpcarrot_is24),
    TEST(issue1417),
    TEST(cubicOp112),
    TEST(skpadspert_net23),
    TEST(skpadspert_de11),
    TEST(findFirst1),
    TEST(xOp2i),
    TEST(xOp3i),
    TEST(xOp1u),
    TEST(xOp1i),
    TEST(cubicOp111),
    TEST(cubicOp110),
    TEST(cubicOp109),
    TEST(cubicOp108),
    TEST(cubicOp107),
    TEST(cubicOp106),
    TEST(cubicOp105),
    TEST(cubicOp104),
    TEST(cubicOp103),
    TEST(cubicOp102),
    TEST(cubicOp101),
    TEST(cubicOp100),
    TEST(cubicOp99),
    TEST(issue1435),
    TEST(cubicOp98x),
    TEST(cubicOp97x),
    TEST(skpcarpetplanet_ru22),
    TEST(cubicOp96d),
    TEST(cubicOp95u),
    TEST(skpadbox_lt15),
    TEST(skpagentxsites_com55),
    TEST(skpadventistmission_org572),
    TEST(skpadoption_org196),
    TEST(skpbambootheme_com12),
    TEST(skpbakosoft_com10),
    TEST(skpakmmos_ru100),
    TEST(skpbangalorenest_com4),
    TEST(skpbingoentertainment_net189),
    TEST(skpbestred_ru37),
    TEST(skpbenzoteh_ru152),
    TEST(skpcamcorder_kz21),
    TEST(skpcaffelavazzait_com_ua21),
    TEST(skpcarrefour_ro62),
    TEST(skpcavablar_net563),
    TEST(skpinsomnia_gr72),
    TEST(skpadbox_lt8),
    TEST(skpact_com43),
    TEST(skpacesoftech_com47),
    TEST(skpabcspark_ca103),
    TEST(cubicOp94u),
    TEST(cubicOp93d),
    TEST(cubicOp92i),
    TEST(skpadithya_putr4_blogspot_com551),
    TEST(skpadindex_de4),
    TEST(skpaiaigames_com870),
    TEST(skpaaalgarve_org53),
    TEST(skpkkiste_to716),
    TEST(cubicOp91u),
    TEST(cubicOp90u),
    TEST(cubicOp89u),
    TEST(cubicOp88u),
    TEST(cubicOp87u),
    TEST(cubicOp86i),
    TEST(loopEdge2),
    TEST(loopEdge1),
    TEST(rectOp3x),
    TEST(rectOp2i),
    TEST(rectOp1i),
    TEST(issue1418b),
    TEST(cubicOp85i),
    TEST(issue1418),
    TEST(skpkkiste_to98),
    TEST(skpahrefs_com29),
    TEST(cubicOp85d),
    TEST(skpahrefs_com88),
    TEST(skphealth_com76),
    TEST(skpancestry_com1),
    TEST(skpbyte_com1),
    TEST(skpeldorado_com_ua1),
    TEST(skp96prezzi1),
    TEST(skpClip2),
    TEST(skpClip1),
    TEST(cubicOp84d),
    TEST(cubicOp83i),
    TEST(cubicOp82i),
    TEST(cubicOp81d),
    TEST(cubicOp80i),
    TEST(cubicOp79u),
    TEST(cubicOp78u),
    TEST(cubicOp77i),
    TEST(cubicOp76u),
    TEST(cubicOp75d),
    TEST(cubicOp74d),
    TEST(cubicOp73d),
    TEST(cubicOp72i),
    TEST(cubicOp71d),
    TEST(skp5),
    TEST(skp4),
    TEST(skp3),
    TEST(skp2),
    TEST(skp1),
    TEST(rRect1),
    TEST(cubicOp70d),
    TEST(cubicOp69d),
    TEST(cubicOp68u),
    TEST(cubicOp67u),
    TEST(cubicOp66u),
    TEST(rectOp1d),
    TEST(cubicOp65d),
    TEST(cubicOp64d),
    TEST(cubicOp63d),
    TEST(cubicOp62d),
    TEST(cubicOp61d),
    TEST(cubicOp60d),
    TEST(cubicOp59d),
    TEST(cubicOp57d),
    TEST(cubicOp56d),
    TEST(cubicOp55d),
    TEST(cubicOp54d),
    TEST(cubicOp53d),
    TEST(cubicOp52d),
    TEST(cubicOp51d),
    TEST(cubicOp50d),
    TEST(cubicOp49d),
    TEST(cubicOp48d),
    TEST(cubicOp47d),
    TEST(cubicOp46d),
    TEST(cubicOp45d),
    TEST(cubicOp44d),
    TEST(cubicOp43d),
    TEST(cubicOp42d),
    TEST(cubicOp41i),
    TEST(cubicOp40d),
    TEST(cubicOp39d),
    TEST(cubicOp38d),
    TEST(cubicOp37d),
    TEST(cubicOp36u),
    TEST(cubicOp35d),
    TEST(cubicOp34d),
    TEST(cubicOp33i),
    TEST(cubicOp32d),
    TEST(cubicOp31d),
    TEST(cubicOp31x),
    TEST(cubicOp31u),
    TEST(cubicOp30d),
    TEST(cubicOp29d),
    TEST(cubicOp28u),
    TEST(cubicOp27d),
    TEST(cubicOp26d),
    TEST(cubicOp25i),
    TEST(testOp8d),
    TEST(testDiff1),
    TEST(testIntersect1),
    TEST(testUnion1),
    TEST(testXor1),
    TEST(testDiff2),
    TEST(testIntersect2),
    TEST(testUnion2),
    TEST(testXor2),
    TEST(testOp1d),
    TEST(testOp2d),
    TEST(testOp3d),
    TEST(testOp1u),
    TEST(testOp4d),
    TEST(testOp5d),
    TEST(testOp6d),
    TEST(testOp7d),
    TEST(testOp2u),

    TEST(cubicOp24d),
    TEST(cubicOp23d),
    TEST(cubicOp22d),
    TEST(cubicOp21d),
    TEST(cubicOp20d),
    TEST(cubicOp19i),
    TEST(cubicOp18d),
    TEST(cubicOp17d),
    TEST(cubicOp16d),
    TEST(cubicOp15d),
    TEST(cubicOp14d),
    TEST(cubicOp13d),
    TEST(cubicOp12d),
    TEST(cubicOp11d),
    TEST(cubicOp10d),
    TEST(cubicOp1i),
    TEST(cubicOp9d),
    TEST(quadOp9d),
    TEST(lineOp9d),
    TEST(cubicOp8d),
    TEST(cubicOp7d),
    TEST(cubicOp6d),
    TEST(cubicOp5d),
    TEST(cubicOp3d),
    TEST(cubicOp2d),
    TEST(cubicOp1d),
};

static const size_t testCount = SK_ARRAY_COUNT(tests);

static struct TestDesc subTests[] = {
    TEST(loops47i),
    TEST(loops61i),
    TEST(loops62i),
    TEST(issue3517),
};

static const size_t subTestCount = SK_ARRAY_COUNT(subTests);

static void (*firstSubTest)(skiatest::Reporter* , const char* filename) = 0;

static bool runSubTests = false;
static bool runSubTestsFirst = true;
static bool runReverse = false;

DEF_TEST(PathOpsOp, reporter) {
#if DEBUG_SHOW_TEST_NAME
    strncpy(DEBUG_FILENAME_STRING, "", DEBUG_FILENAME_STRING_LENGTH);
#endif
    if (runSubTests && runSubTestsFirst) {
        RunTestSet(reporter, subTests, subTestCount, firstSubTest, nullptr, stopTest, runReverse);
    }
    RunTestSet(reporter, tests, testCount, firstTest, skipTest, stopTest, runReverse);
    if (runSubTests && !runSubTestsFirst) {
        RunTestSet(reporter, subTests, subTestCount, firstSubTest, nullptr, stopTest, runReverse);
    }
}

static void fuzz535151(skiatest::Reporter* reporter, const char* filename) {
    SkPath one;
    one.setFillType(SkPath::kWinding_FillType);
    SkPath two;
    two.setFillType(SkPath::kWinding_FillType);
    two.moveTo(0, 0);
    two.lineTo(0, 50);
    two.lineTo(4.29497e+09f, 50);
    SkPath dummy;
    testPathOp(reporter, one, two, kIntersect_SkPathOp, filename);
}

static void bufferOverflow(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.addRect(0,0, 300,170141183460469231731687303715884105728.f);
    SkPath pathB;
    pathB.addRect(0,0, 300,16);
    testPathOpCheck(reporter, path, pathB, kUnion_SkPathOp, filename, true);
}

// m 100,0 60,170 -160,-110 200,0 -170,11000000000 z
static void fuzz433(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1, path2;
    path1.moveTo(100,0);
    path1.lineTo(60,170);
    path1.lineTo(-160,-110);
    path1.lineTo(200,0);
    path1.lineTo(-170,11000000000.0f);
    path1.close();

    path2.moveTo(100 + 20,0 + 20);
    path2.lineTo(60 + 20,170 + 20);
    path2.lineTo(-160 + 20,-110 + 20);
    path2.lineTo(200 + 20,0 + 20);
    path2.lineTo(-170 + 20,11000000000.0f + 20);
    path2.close();

    testPathOpCheck(reporter, path1, path2, kIntersect_SkPathOp, filename, true);
}

static void fuzz433b(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1, path2;
    path1.setFillType(SkPath::kEvenOdd_FillType);
    path1.moveTo(140, 40);
    path1.lineTo(200, 210);
    path1.lineTo(40, 100);
    path1.lineTo(240, 100);
    path1.lineTo(70, 1.1e+10f);
    path1.lineTo(140, 40);
    path1.close();

    path1.setFillType(SkPath::kWinding_FillType);
    path2.moveTo(190, 60);
    path2.lineTo(250, 230);
    path2.lineTo(90, 120);
    path2.lineTo(290, 120);
    path2.lineTo(120, 1.1e+10f);
    path2.lineTo(190, 60);
    path2.close();

    testPathOpCheck(reporter, path1, path2, kUnion_SkPathOp, filename, true);
}

static void fuzz487a(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x432c8000), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4309999a), SkBits2Float(0x42c00000));
path.cubicTo(SkBits2Float(0x4309999a), SkBits2Float(0x429a6666), SkBits2Float(0x42f9999a), SkBits2Float(0x4275999a), SkBits2Float(0x42d70001), SkBits2Float(0x42633333));
path.lineTo(SkBits2Float(0x42e90001), SkBits2Float(0x41b8cccc));
path.cubicTo(SkBits2Float(0x42dc6667), SkBits2Float(0x41ab3332), SkBits2Float(0x42cf3334), SkBits2Float(0x41a3ffff), SkBits2Float(0x42c20001), SkBits2Float(0x41a3ffff));
path.lineTo(SkBits2Float(0x42c20001), SkBits2Float(0x425d999a));
path.lineTo(SkBits2Float(0x42c20001), SkBits2Float(0x425d999a));
path.cubicTo(SkBits2Float(0x429c6668), SkBits2Float(0x425d999a), SkBits2Float(0x4279999c), SkBits2Float(0x42886667), SkBits2Float(0x42673335), SkBits2Float(0x42ab0000));
path.lineTo(SkBits2Float(0x41c0ccd0), SkBits2Float(0x42990000));
path.cubicTo(SkBits2Float(0x41b33336), SkBits2Float(0x42a5999a), SkBits2Float(0x41ac0003), SkBits2Float(0x42b2cccd), SkBits2Float(0x41ac0003), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4261999c), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4261999c), SkBits2Float(0x42c00000));
path.cubicTo(SkBits2Float(0x4261999c), SkBits2Float(0x434d3333), SkBits2Float(0x4364e667), SkBits2Float(0x4346b333), SkBits2Float(0x4364e667), SkBits2Float(0x43400000));
path.lineTo(SkBits2Float(0x432c8000), SkBits2Float(0x42c00000));
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x432c8000), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4309999a), SkBits2Float(0x42c00000));
path.cubicTo(SkBits2Float(0x4309999a), SkBits2Float(0x42a20000), SkBits2Float(0x43016667), SkBits2Float(0x4287cccd), SkBits2Float(0x42ea999a), SkBits2Float(0x4273999a));
path.lineTo(SkBits2Float(0x4306cccd), SkBits2Float(0x41f5999a));
path.cubicTo(SkBits2Float(0x42f76667), SkBits2Float(0x41c26667), SkBits2Float(0x42dd999a), SkBits2Float(0x41a4cccd), SkBits2Float(0x42c23334), SkBits2Float(0x41a4cccd));
path.lineTo(SkBits2Float(0x42c23334), SkBits2Float(0x425e0000));
path.cubicTo(SkBits2Float(0x42a43334), SkBits2Float(0x425e0000), SkBits2Float(0x428a0001), SkBits2Float(0x427ecccd), SkBits2Float(0x42780002), SkBits2Float(0x4297999a));
path.lineTo(SkBits2Float(0x41fccccd), SkBits2Float(0x42693333));
path.cubicTo(SkBits2Float(0x41c9999a), SkBits2Float(0x428acccd), SkBits2Float(0x41ac0000), SkBits2Float(0x42a4999a), SkBits2Float(0x41ac0000), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4261999a), SkBits2Float(0x42c00000));
path.cubicTo(SkBits2Float(0x4261999a), SkBits2Float(0x42de0000), SkBits2Float(0x42813333), SkBits2Float(0x42f83333), SkBits2Float(0x42996666), SkBits2Float(0x4303199a));
path.cubicTo(SkBits2Float(0x4272cccc), SkBits2Float(0x4303199a), SkBits2Float(0x423d3332), SkBits2Float(0x430de667), SkBits2Float(0x422d9999), SkBits2Float(0x431cb334));
path.lineTo(SkBits2Float(0x7086a1dc), SkBits2Float(0x42eecccd));
path.lineTo(SkBits2Float(0x41eb3333), SkBits2Float(0xc12ccccd));
path.lineTo(SkBits2Float(0x42053333), SkBits2Float(0xc1cccccd));
path.lineTo(SkBits2Float(0x42780000), SkBits2Float(0xc18f3334));
path.cubicTo(SkBits2Float(0x43206666), SkBits2Float(0x43134ccd), SkBits2Float(0x43213333), SkBits2Float(0x430db333), SkBits2Float(0x43213333), SkBits2Float(0x43080000));
path.lineTo(SkBits2Float(0x432c8000), SkBits2Float(0x42c00000));
path.close();

    SkPath path2(path);
    testPathOpCheck(reporter, path1, path2, (SkPathOp) 2, filename, true);
}

static void fuzz487b(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x432c8000), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4309999a), SkBits2Float(0x42c00000));
path.cubicTo(SkBits2Float(0x4309999a), SkBits2Float(0x429a6666), SkBits2Float(0x42f9999a), SkBits2Float(0x4275999a), SkBits2Float(0x42d70001), SkBits2Float(0x42633333));
path.lineTo(SkBits2Float(0x42e90001), SkBits2Float(0x41b8cccc));
path.cubicTo(SkBits2Float(0x42dc6667), SkBits2Float(0x41ab3332), SkBits2Float(0x42cf3334), SkBits2Float(0x41a3ffff), SkBits2Float(0x42c20001), SkBits2Float(0x41a3ffff));
path.lineTo(SkBits2Float(0x42c20001), SkBits2Float(0x425d999a));
path.lineTo(SkBits2Float(0x42c20001), SkBits2Float(0x425d999a));
path.cubicTo(SkBits2Float(0x429c6668), SkBits2Float(0x425d999a), SkBits2Float(0x4279999c), SkBits2Float(0x42886667), SkBits2Float(0x42673335), SkBits2Float(0x42ab0000));
path.lineTo(SkBits2Float(0x41c0ccd0), SkBits2Float(0x42990000));
path.cubicTo(SkBits2Float(0x41b33336), SkBits2Float(0x42a5999a), SkBits2Float(0x41ac0003), SkBits2Float(0x42b2cccd), SkBits2Float(0x41ac0003), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4261999c), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4261999c), SkBits2Float(0x42c00000));
path.cubicTo(SkBits2Float(0x4261999c), SkBits2Float(0x434d3333), SkBits2Float(0x4364e667), SkBits2Float(0x4346b333), SkBits2Float(0x4364e667), SkBits2Float(0x43400000));
path.lineTo(SkBits2Float(0x432c8000), SkBits2Float(0x42c00000));
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x432c8000), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4309999a), SkBits2Float(0x42c00000));
path.cubicTo(SkBits2Float(0x4309999a), SkBits2Float(0x42a20000), SkBits2Float(0x43016667), SkBits2Float(0x4287cccd), SkBits2Float(0x42ea999a), SkBits2Float(0x4273999a));
path.lineTo(SkBits2Float(0x4306cccd), SkBits2Float(0x41f5999a));
path.cubicTo(SkBits2Float(0x42f76667), SkBits2Float(0x41c26667), SkBits2Float(0x42dd999a), SkBits2Float(0x41a4cccd), SkBits2Float(0x42c23334), SkBits2Float(0x41a4cccd));
path.lineTo(SkBits2Float(0x42c23334), SkBits2Float(0x425e0000));
path.cubicTo(SkBits2Float(0x42a43334), SkBits2Float(0x425e0000), SkBits2Float(0x428a0001), SkBits2Float(0x427ecccd), SkBits2Float(0x42780002), SkBits2Float(0x4297999a));
path.lineTo(SkBits2Float(0x41fccccd), SkBits2Float(0x42693333));
path.cubicTo(SkBits2Float(0x41c9999a), SkBits2Float(0x428acccd), SkBits2Float(0x41ac0000), SkBits2Float(0x42a4999a), SkBits2Float(0x41ac0000), SkBits2Float(0x42c00000));
path.lineTo(SkBits2Float(0x4261999a), SkBits2Float(0x42c00000));
path.cubicTo(SkBits2Float(0x4261999a), SkBits2Float(0x42de0000), SkBits2Float(0x42813333), SkBits2Float(0x42f83333), SkBits2Float(0x42996666), SkBits2Float(0x4303199a));
path.cubicTo(SkBits2Float(0x4272cccc), SkBits2Float(0x4303199a), SkBits2Float(0x423d3332), SkBits2Float(0x430de667), SkBits2Float(0x422d9999), SkBits2Float(0x431cb334));
path.lineTo(SkBits2Float(0x7086a1dc), SkBits2Float(0x42eecccd));
path.lineTo(SkBits2Float(0x41eb3333), SkBits2Float(0xc12ccccd));
path.lineTo(SkBits2Float(0x42053333), SkBits2Float(0xc1cccccd));
path.lineTo(SkBits2Float(0x42780000), SkBits2Float(0xc18f3334));
path.cubicTo(SkBits2Float(0x43206666), SkBits2Float(0x43134ccd), SkBits2Float(0x43213333), SkBits2Float(0x430db333), SkBits2Float(0x43213333), SkBits2Float(0x43080000));
path.lineTo(SkBits2Float(0x432c8000), SkBits2Float(0x42c00000));
path.close();

    SkPath path2(path);
    testPathOpCheck(reporter, path1, path2, (SkPathOp) 2, filename, true);
}

static void fuzz714(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x430c0000), SkBits2Float(0x42200000));
path.lineTo(SkBits2Float(0x43480000), SkBits2Float(0x43520000));
path.lineTo(SkBits2Float(0x42200000), SkBits2Float(0x42c80000));
path.lineTo(SkBits2Float(0x64969569), SkBits2Float(0x42c80000));  // 2.22222e+022f
path.lineTo(SkBits2Float(0x64969569), SkBits2Float(0x43520000));  // 2.22222e+022f
path.lineTo(SkBits2Float(0x430c0000), SkBits2Float(0x42200000));
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x43200000), SkBits2Float(0x42700000));
path.lineTo(SkBits2Float(0x435c0000), SkBits2Float(0x43660000));
path.lineTo(SkBits2Float(0x42700000), SkBits2Float(0x42f00000));
path.lineTo(SkBits2Float(0x64969569), SkBits2Float(0x42f00000));  // 2.22222e+022f
path.lineTo(SkBits2Float(0x64969569), SkBits2Float(0x43660000));  // 2.22222e+022f
path.lineTo(SkBits2Float(0x43200000), SkBits2Float(0x42700000));
path.close();

    SkPath path2(path);
    testPathOpCheck(reporter, path1, path2, (SkPathOp) 2, filename, true);
}

static void fuzz1(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x7f800000), SkBits2Float(0x7f800000));
path.quadTo(SkBits2Float(0x7f800000), SkBits2Float(0x7f800000), SkBits2Float(0x7f800000), SkBits2Float(0x7f800000));
path.quadTo(SkBits2Float(0x7f800000), SkBits2Float(0x7f800000), SkBits2Float(0x7f800000), SkBits2Float(0x7f800000));
path.quadTo(SkBits2Float(0xffc00000), SkBits2Float(0x7f800000), SkBits2Float(0xffc00000), SkBits2Float(0x7f800000));
path.quadTo(SkBits2Float(0xff000001), SkBits2Float(0x7f800000), SkBits2Float(0xff000001), SkBits2Float(0x7f800000));
path.quadTo(SkBits2Float(0xff000001), SkBits2Float(0xffc00000), SkBits2Float(0xffc00000), SkBits2Float(0xffc00000));
path.quadTo(SkBits2Float(0xffc00000), SkBits2Float(0xff000001), SkBits2Float(0x7f800000), SkBits2Float(0xff000001));
path.quadTo(SkBits2Float(0x7f800000), SkBits2Float(0xff000001), SkBits2Float(0x7f800000), SkBits2Float(0xffc00000));
path.quadTo(SkBits2Float(0x7f800000), SkBits2Float(0xffc00000), SkBits2Float(0x7f800000), SkBits2Float(0x7f800000));
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);

    SkPath path2(path);
    testPathOpFail(reporter, path1, path2, (SkPathOp) 2, filename);
}


static void fuzz753_91(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x42910000), SkBits2Float(0x00000000));  // 72.5f, 0
path.lineTo(SkBits2Float(0x42166668), SkBits2Float(0x00000000));  // 37.6f, 0
path.cubicTo(SkBits2Float(0x42166668), SkBits2Float(0xc1966668), SkBits2Float(0x41c66668), SkBits2Float(0xc20a6666), SkBits2Float(0x40f00010), SkBits2Float(0xc21ccccd));  // 37.6f, -18.8f, 24.8f, -34.6f, 7.50001f, -39.2f
path.lineTo(SkBits2Float(0x41840004), SkBits2Float(0xc291cccd));  // 16.5f, -72.9f
path.lineTo(SkBits2Float(0x42fb6668), SkBits2Float(0x42c73334));  // 125.7f, 99.6f
path.lineTo(SkBits2Float(0x43646668), SkBits2Float(0x43880ccd));  // 228.4f, 272.1f

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x428bf702), SkBits2Float(0xcf223cbf));  // 69.9824f, -2.72189e+09f
path.lineTo(SkBits2Float(0x42112d68), SkBits2Float(0xcf223cbf));  // 36.2943f, -2.72189e+09f
path.cubicTo(SkBits2Float(0x4220d9fc), SkBits2Float(0xcf223cc0), SkBits2Float(0x420ee118), SkBits2Float(0xcf223cc0), SkBits2Float(0x41cef2f8), SkBits2Float(0xcf223cc0));  // 40.2129f, -2.72189e+09f, 35.7198f, -2.72189e+09f, 25.8686f, -2.72189e+09f
path.lineTo(SkBits2Float(0x424a99e0), SkBits2Float(0xcf223cc0));  // 50.6503f, -2.72189e+09f
path.cubicTo(SkBits2Float(0x42266e32), SkBits2Float(0xcf223cc0), SkBits2Float(0x41f0fa20), SkBits2Float(0xcf223cc0), SkBits2Float(0x41872ed4), SkBits2Float(0xcf223cc0));  // 41.6076f, -2.72189e+09f, 30.1221f, -2.72189e+09f, 16.8979f, -2.72189e+09f
path.lineTo(SkBits2Float(0x40f8fbe0), SkBits2Float(0xcf223cc0));  // 7.78075f, -2.72189e+09f

    SkPath path2(path);
    testPathOp(reporter, path1, path2, (SkPathOp) 2, filename);
}

static void bug597926_0(skiatest::Reporter* reporter, const char* filename) {
SkPath path;
path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x43b38000), SkBits2Float(0x433e0000));  // 359, 190
path.lineTo(SkBits2Float(0x40c00000), SkBits2Float(0x449ce000));  // 6, 1255
path.cubicTo(SkBits2Float(0x438c0000), SkBits2Float(0x4497a000), SkBits2Float(0x43e40000), SkBits2Float(0x44750000), SkBits2Float(0x41000000), SkBits2Float(0x44aa2000));  // 280, 1213, 456, 980, 8, 1361
path.moveTo(SkBits2Float(0x43290000), SkBits2Float(0x4431c000));  // 169, 711
path.lineTo(SkBits2Float(0xd987d6ba), SkBits2Float(0xd93d0ad4));  // -4.7794e+15f, -3.32567e+15f
path.conicTo(SkBits2Float(0x43cc8000), SkBits2Float(0x445b8000), SkBits2Float(0xd888b096), SkBits2Float(0xd9a1ebfa), SkBits2Float(0x3ebcb199));  // 409, 878, -1.20234e+15f, -5.69712e+15f, 0.368542f
path.cubicTo(SkBits2Float(0x43c00000), SkBits2Float(0x443a8000), SkBits2Float(0x42380000), SkBits2Float(0x4421c000), SkBits2Float(0x42500000), SkBits2Float(0x448ca000));  // 384, 746, 46, 647, 52, 1125
path.quadTo(SkBits2Float(0x43948000), SkBits2Float(0x42ac0000), SkBits2Float(0x43880000), SkBits2Float(0x4487e000));  // 297, 86, 272, 1087
SkPath path1(path);
path.reset();
path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0xc51d735c), SkBits2Float(0xc49db029));  // -2519.21f, -1261.51f
path.cubicTo(SkBits2Float(0xc51d1dbd), SkBits2Float(0xc49d7a3f), SkBits2Float(0xc51c524a), SkBits2Float(0xc49d1610), SkBits2Float(0xc51d1a96), SkBits2Float(0xc49d86a6));  // -2513.86f, -1259.82f, -2501.14f, -1256.69f, -2513.66f, -1260.21f
path.cubicTo(SkBits2Float(0xc51cd471), SkBits2Float(0xc49d54d0), SkBits2Float(0xc51c2e51), SkBits2Float(0xc49d0081), SkBits2Float(0xc51d197b), SkBits2Float(0xc49d7927));  // -2509.28f, -1258.65f, -2498.89f, -1256.02f, -2513.59f, -1259.79f
path.quadTo(SkBits2Float(0xc51bf7eb), SkBits2Float(0xc49cf010), SkBits2Float(0xc51ba866), SkBits2Float(0xc49cb9e6));  // -2495.49f, -1255.5f, -2490.52f, -1253.81f
path.cubicTo(SkBits2Float(0xc51bac0d), SkBits2Float(0xc49cc50e), SkBits2Float(0xc51c29eb), SkBits2Float(0xc49cfb01), SkBits2Float(0xc51c5bca), SkBits2Float(0xc49d1fa6));  // -2490.75f, -1254.16f, -2498.62f, -1255.84f, -2501.74f, -1256.99f
SkPath path2(path);
testPathOp(reporter, path1, path2, (SkPathOp) 1, filename);
}

static void fuzz1450_0(skiatest::Reporter* reporter, const char* filename) {
SkPath path;
path.moveTo(SkBits2Float(0x43b40000), SkBits2Float(0xcf000000));  // 360, -2.14748e+09f
path.conicTo(SkBits2Float(0x4e800002), SkBits2Float(0xcf000000), SkBits2Float(0x4e800002), SkBits2Float(0xce7ffffe), SkBits2Float(0x3f3504f4));  // 1.07374e+09f, -2.14748e+09f, 1.07374e+09f, -1.07374e+09f, 0.707107f
path.conicTo(SkBits2Float(0x4e800002), SkBits2Float(0x43800001), SkBits2Float(0x43348000), SkBits2Float(0x43800001), SkBits2Float(0x3f3504f4));  // 1.07374e+09f, 256, 180.5f, 256, 0.707107f
SkPath path1(path);
path.reset();
path.moveTo(SkBits2Float(0x43b40000), SkBits2Float(0x45816000));  // 360, 4140
path.conicTo(SkBits2Float(0x43b40005), SkBits2Float(0x458a945d), SkBits2Float(0x45610000), SkBits2Float(0x458a945d), SkBits2Float(0x3f3504f3));  // 360, 4434.55f, 3600, 4434.55f, 0.707107f
path.conicTo(SkBits2Float(0x45d5bfff), SkBits2Float(0x458a945d), SkBits2Float(0x45d5bfff), SkBits2Float(0x45816000), SkBits2Float(0x3f3504f3));  // 6840, 4434.55f, 6840, 4140, 0.707107f
path.lineTo(SkBits2Float(0x42c80000), SkBits2Float(0x44000000));  // 100, 512
path.lineTo(SkBits2Float(0x42000000), SkBits2Float(0x41800000));  // 32, 16
path.lineTo(SkBits2Float(0x43b40000), SkBits2Float(0x44800000));  // 360, 1024
path.lineTo(SkBits2Float(0x43b40000), SkBits2Float(0x45816000));  // 360, 4140
path.close();
SkPath path2(path);
testPathOpSkipAssert(reporter, path1, path2, kUnion_SkPathOp, filename);
}

static void fuzz1450_1(skiatest::Reporter* reporter, const char* filename) {
SkPath path;
path.setFillType(SkPath::kEvenOdd_FillType);
path.moveTo(SkBits2Float(0x4e800002), SkBits2Float(0xce7ffffe));  // 1.07374e+09f, -1.07374e+09f
path.conicTo(SkBits2Float(0x4e800002), SkBits2Float(0xcf000000), SkBits2Float(0x43b40000), SkBits2Float(0xcf000000), SkBits2Float(0x3f3504f4));  // 1.07374e+09f, -2.14748e+09f, 360, -2.14748e+09f, 0.707107f
path.lineTo(SkBits2Float(0x43348000), SkBits2Float(0x43800001));  // 180.5f, 256
path.lineTo(SkBits2Float(0x42000000), SkBits2Float(0x41800000));  // 32, 16
path.lineTo(SkBits2Float(0x42c80000), SkBits2Float(0x44000000));  // 100, 512
path.lineTo(SkBits2Float(0x43553abd), SkBits2Float(0x440f3cbd));  // 213.229f, 572.949f
path.lineTo(SkBits2Float(0x43b40000), SkBits2Float(0x44800000));  // 360, 1024
path.lineTo(SkBits2Float(0x43b40000), SkBits2Float(0x45816000));  // 360, 4140
path.conicTo(SkBits2Float(0x43b40005), SkBits2Float(0x458a945d), SkBits2Float(0x45610000), SkBits2Float(0x458a945d), SkBits2Float(0x3f3504f3));  // 360, 4434.55f, 3600, 4434.55f, 0.707107f
path.conicTo(SkBits2Float(0x45d5bfff), SkBits2Float(0x458a945d), SkBits2Float(0x45d5bfff), SkBits2Float(0x45816000), SkBits2Float(0x3f3504f3));  // 6840, 4434.55f, 6840, 4140, 0.707107f
path.lineTo(SkBits2Float(0x43553abd), SkBits2Float(0x440f3cbd));  // 213.229f, 572.949f
path.lineTo(SkBits2Float(0x43348000), SkBits2Float(0x43800001));  // 180.5f, 256
path.conicTo(SkBits2Float(0x4e800002), SkBits2Float(0x43800001), SkBits2Float(0x4e800002), SkBits2Float(0xce7ffffe), SkBits2Float(0x3f3504f4));  // 1.07374e+09f, 256, 1.07374e+09f, -1.07374e+09f, 0.707107f
path.close();
SkPath path1(path);
path.reset();
path.moveTo(SkBits2Float(0x42fe0000), SkBits2Float(0x43a08000));  // 127, 321
path.lineTo(SkBits2Float(0x45d5c000), SkBits2Float(0x43870000));  // 6840, 270
path.lineTo(SkBits2Float(0xd0a00000), SkBits2Float(0x4cbebc20));  // -2.14748e+10f, 1e+08
path.lineTo(SkBits2Float(0x451f7000), SkBits2Float(0x42800000));  // 2551, 64
path.lineTo(SkBits2Float(0x42fe0000), SkBits2Float(0x43a08000));  // 127, 321
path.close();
SkPath path2(path);
testPathOpSkipAssert(reporter, path1, path2, kUnion_SkPathOp, filename);
}

static void fuzz763_9(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.conicTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x081f2a21), SkBits2Float(0x7bc00321), SkBits2Float(0xed7a6a4b), SkBits2Float(0x1f212a8c));  // 2.49282e-13f, 4.78968e-34f, 1.99397e+36f, -4.84373e+27f, 3.41283e-20f
path.lineTo(SkBits2Float(0x7bc00321), SkBits2Float(0xed7a6a4b));  // 1.99397e+36f, -4.84373e+27f
path.lineTo(SkBits2Float(0x282a3a21), SkBits2Float(0x3a21df28));  // 9.4495e-15f, 0.000617492f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.close();
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.quadTo(SkBits2Float(0x8a284f9a), SkBits2Float(0x3ac23ab3), SkBits2Float(0x1d2a2928), SkBits2Float(0x63962be6));  // -8.10388e-33f, 0.00148185f, 2.25206e-21f, 5.54035e+21f
path.moveTo(SkBits2Float(0x29272a81), SkBits2Float(0x2ab03a55));  // 3.71183e-14f, 3.13044e-13f
path.quadTo(SkBits2Float(0x2720213b), SkBits2Float(0x3a214729), SkBits2Float(0xdf28282a), SkBits2Float(0x8a2f2121));  // 2.22225e-15f, 0.000615227f, -1.2117e+19f, -8.43217e-33f
path.quadTo(SkBits2Float(0x373b3a27), SkBits2Float(0x201fc4c1), SkBits2Float(0x27576c2a), SkBits2Float(0x5921c25d));  // 1.11596e-05f, 1.35329e-19f, 2.98959e-15f, 2.8457e+15f
path.quadTo(SkBits2Float(0x2720213b), SkBits2Float(0x3a214729), SkBits2Float(0xdf28282a), SkBits2Float(0x3a8a3a21));  // 2.22225e-15f, 0.000615227f, -1.2117e+19f, 0.00105459f
path.cubicTo(SkBits2Float(0x373b3ac5), SkBits2Float(0x201fc422), SkBits2Float(0x523a702a), SkBits2Float(0x27576c51), SkBits2Float(0x5921c25d), SkBits2Float(0x51523a70));  // 1.11598e-05f, 1.35327e-19f, 2.00186e+11f, 2.9896e-15f, 2.8457e+15f, 5.64327e+10f
path.quadTo(SkBits2Float(0xd912102a), SkBits2Float(0x284f9a28), SkBits2Float(0xb38a1f30), SkBits2Float(0x3a3ac23a));  // -2.56957e+15f, 1.15242e-14f, -6.4318e-08f, 0.000712428f
path.lineTo(SkBits2Float(0xc809272a), SkBits2Float(0x29b02829));  // -140445, 7.82294e-14f

    SkPath path2(path);
    testPathOp(reporter, path1, path2, (SkPathOp) 1, filename);
}


static void fuzz763_4(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0x555b3a2d), SkBits2Float(0x2a212a8c));  // 1.50652e+13f, 1.43144e-13f
path.conicTo(SkBits2Float(0xc0032108), SkBits2Float(0x7a6a4b7b), SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a3a7bc0));  // -2.04889f, 3.04132e+35f, 5.77848e-19f, 4.7323e-37f, 5.63611e+25f
path.conicTo(SkBits2Float(0x3a2147ed), SkBits2Float(0xdf28282a), SkBits2Float(0x3a8a3a21), SkBits2Float(0x8a284f9a), SkBits2Float(0x3ac2b33a));  // 0.000615238f, -1.2117e+19f, 0.00105459f, -8.10388e-33f, 0.00148544f
path.cubicTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x63962be6), SkBits2Float(0x295b2d2a), SkBits2Float(0x68295b2d), SkBits2Float(0x2d296855), SkBits2Float(0x2a8c275b));  // 2.25206e-21f, 5.54035e+21f, 4.86669e-14f, 3.19905e+24f, 9.6297e-12f, 2.48963e-13f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.close();
path.moveTo(SkBits2Float(0x55685b1f), SkBits2Float(0x5b2d2968));  // 1.59674e+13f, 4.87407e+16f
path.lineTo(SkBits2Float(0x2a212a8c), SkBits2Float(0x2a21081f));  // 1.43144e-13f, 1.43025e-13f
path.conicTo(SkBits2Float(0xde6a4b7b), SkBits2Float(0x2a8ced7a), SkBits2Float(0x21081f21), SkBits2Float(0x3a7bc003), SkBits2Float(0x47ed7a6a));  // -4.22068e+18f, 2.50338e-13f, 4.61198e-19f, 0.00096035f, 121589
path.lineTo(SkBits2Float(0x55685b1f), SkBits2Float(0x5b2d2968));  // 1.59674e+13f, 4.87407e+16f
path.close();
path.moveTo(SkBits2Float(0x55685b1f), SkBits2Float(0x5b2d2968));  // 1.59674e+13f, 4.87407e+16f
path.quadTo(SkBits2Float(0xdf28282a), SkBits2Float(0x3a8a3a21), SkBits2Float(0x8a284f9a), SkBits2Float(0x3ac23ab3));  // -1.2117e+19f, 0.00105459f, -8.10388e-33f, 0.00148185f
path.lineTo(SkBits2Float(0x2928088c), SkBits2Float(0x2be61d2a));  // 3.73109e-14f, 1.63506e-12f
path.conicTo(SkBits2Float(0x2a812a63), SkBits2Float(0x2d292a27), SkBits2Float(0x5568295b), SkBits2Float(0x5b2d2968), SkBits2Float(0x552d6829));  // 2.29444e-13f, 9.6159e-12f, 1.5954e+13f, 4.87407e+16f, 1.19164e+13f
path.conicTo(SkBits2Float(0x395b2d5b), SkBits2Float(0x68552768), SkBits2Float(0x555b2df0), SkBits2Float(0x1f722a8c), SkBits2Float(0x082a212a));  // 0.000209024f, 4.02636e+24f, 1.50619e+13f, 5.12807e-20f, 5.11965e-34f
path.lineTo(SkBits2Float(0x55685b1f), SkBits2Float(0x5b2d2968));  // 1.59674e+13f, 4.87407e+16f
path.close();
path.moveTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2147ed7a), SkBits2Float(0x28282a3a), SkBits2Float(0x21df212a), SkBits2Float(0x033a8a3a));  // 6.14991e+25f, 6.77381e-19f, 9.33503e-15f, 1.51198e-18f, 5.48192e-37f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 1, filename);
}

static void fuzz763_3(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0x555b292d), SkBits2Float(0x2a212a8c));  // 1.50606e+13f, 1.43144e-13f
path.conicTo(SkBits2Float(0xc0032108), SkBits2Float(0x7a6a4b7b), SkBits2Float(0x212a8ced), SkBits2Float(0x295b2d1f), SkBits2Float(0x29685568));  // -2.04889f, 3.04132e+35f, 5.77848e-19f, 4.86669e-14f, 5.15884e-14f
path.conicTo(SkBits2Float(0x8c28295b), SkBits2Float(0x1f21212a), SkBits2Float(0xc0032a08), SkBits2Float(0x7a6a4b7b), SkBits2Float(0x212a8ced));  // -1.29547e-31f, 3.41205e-20f, -2.04944f, 3.04132e+35f, 5.77848e-19f
path.moveTo(SkBits2Float(0x25682929), SkBits2Float(0x212a8c5b));  // 2.01367e-16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4a7bc0));  // 4.7323e-37f, 6.11969e+25f
path.conicTo(SkBits2Float(0x032108ed), SkBits2Float(0x283a7bc0), SkBits2Float(0x47ed7a6a), SkBits2Float(0x282a3a21), SkBits2Float(0x3a21ff28));  // 4.73239e-37f, 1.03519e-14f, 121589, 9.4495e-15f, 0.000617968f
path.quadTo(SkBits2Float(0x8a284f9a), SkBits2Float(0x3ac23ab3), SkBits2Float(0x2a292827), SkBits2Float(0x962be61d));  // -8.10388e-33f, 0.00148185f, 1.50241e-13f, -1.38859e-25f
path.lineTo(SkBits2Float(0x295b2d2a), SkBits2Float(0x2d296868));  // 4.86669e-14f, 9.62972e-12f
path.moveTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x898ced7a), SkBits2Float(0x21081f21), SkBits2Float(0x3a7bc003), SkBits2Float(0x47ed7a6a));  // 6.14991e+25f, -3.39271e-33f, 4.61198e-19f, 0.00096035f, 121589
path.lineTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.close();
path.moveTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.quadTo(SkBits2Float(0xdf28282a), SkBits2Float(0x3a8a3a21), SkBits2Float(0xb38a281a), SkBits2Float(0x29283ac2));  // -1.2117e+19f, 0.00105459f, -6.43342e-08f, 3.73545e-14f
path.moveTo(SkBits2Float(0x962be61d), SkBits2Float(0x432a2927));  // -1.38859e-25f, 170.161f
path.conicTo(SkBits2Float(0x3a2a552a), SkBits2Float(0x3b1e2ab0), SkBits2Float(0x29272021), SkBits2Float(0x3b3ac527), SkBits2Float(0x1fc42236));  // 0.000649768f, 0.00241343f, 3.71093e-14f, 0.00284989f, 8.30658e-20f
path.cubicTo(SkBits2Float(0x27576c2a), SkBits2Float(0x5921c25d), SkBits2Float(0x51503a70), SkBits2Float(0x12102a10), SkBits2Float(0x633a28d9), SkBits2Float(0x29c80927));  // 2.98959e-15f, 2.8457e+15f, 5.58959e+10f, 4.54902e-28f, 3.43404e+21f, 8.88337e-14f
path.lineTo(SkBits2Float(0x272927b0), SkBits2Float(0x5b392929));  // 2.3475e-15f, 5.21181e+16f
path.moveTo(SkBits2Float(0x3a1127b4), SkBits2Float(0x2921ee3b));  // 0.000553723f, 3.59558e-14f
path.cubicTo(SkBits2Float(0x5e215d3b), SkBits2Float(0x7828ee3a), SkBits2Float(0x8e28b03b), SkBits2Float(0x50783be8), SkBits2Float(0x9e0b8a3a), SkBits2Float(0x555b2d68));  // 2.90688e+18f, 1.37053e+34f, -2.07925e-30f, 1.66587e+10f, -7.38718e-21f, 1.50618e+13f
path.moveTo(SkBits2Float(0x21081f3f), SkBits2Float(0x9fd4e62a));  // 4.61199e-19f, -9.01663e-20f
path.cubicTo(SkBits2Float(0x3a293a2a), SkBits2Float(0x0e3bf0c5), SkBits2Float(0x3b29d42a), SkBits2Float(0x0f217265), SkBits2Float(0x2d5d2921), SkBits2Float(0x5568295b));  // 0.000645551f, 2.31655e-30f, 0.00259138f, 7.95994e-30f, 1.25715e-11f, 1.5954e+13f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 1, filename);
}

static void fuzz763_5(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x5b292d55), SkBits2Float(0x2a2a8c55));  // 4.76191e+16f, 1.51477e-13f
path.conicTo(SkBits2Float(0xc0032108), SkBits2Float(0x7a6a4b79), SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a3a7bc0));  // -2.04889f, 3.04132e+35f, 5.77848e-19f, 4.7323e-37f, 5.63611e+25f
path.conicTo(SkBits2Float(0x3a2147ed), SkBits2Float(0xdf28282a), SkBits2Float(0x3a8a3a21), SkBits2Float(0x8a284f9a), SkBits2Float(0x3ac23ab3));  // 0.000615238f, -1.2117e+19f, 0.00105459f, -8.10388e-33f, 0.00148185f
path.cubicTo(SkBits2Float(0xe62a2928), SkBits2Float(0x2a63962b), SkBits2Float(0x68295b2d), SkBits2Float(0x2d296855), SkBits2Float(0x2a8c555b), SkBits2Float(0x001f2a21));  // -2.0089e+23f, 2.02138e-13f, 3.19905e+24f, 9.6297e-12f, 2.49282e-13f, 2.86201e-39f
path.lineTo(SkBits2Float(0x5b292d55), SkBits2Float(0x2a2a8c55));  // 4.76191e+16f, 1.51477e-13f
path.close();
path.moveTo(SkBits2Float(0x5b292d55), SkBits2Float(0x2a2a8c55));  // 4.76191e+16f, 1.51477e-13f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2a8ced7a), SkBits2Float(0x21081f21), SkBits2Float(0x3a7bc003), SkBits2Float(0x47ed7a6a));  // 6.14991e+25f, 2.50338e-13f, 4.61198e-19f, 0.00096035f, 121589
path.lineTo(SkBits2Float(0x5b292d55), SkBits2Float(0x2a2a8c55));  // 4.76191e+16f, 1.51477e-13f
path.close();
path.moveTo(SkBits2Float(0x5b292d55), SkBits2Float(0x2a2a8c55));  // 4.76191e+16f, 1.51477e-13f
path.quadTo(SkBits2Float(0xdf28282a), SkBits2Float(0x3a8a3b21), SkBits2Float(0x28ee4f9a), SkBits2Float(0x68293b78));  // -1.2117e+19f, 0.00105462f, 2.64578e-14f, 3.19671e+24f
path.lineTo(SkBits2Float(0x5b2d2968), SkBits2Float(0x5b2d8c55));  // 4.87407e+16f, 4.88495e+16f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 4, filename);
}

static void fuzz763_2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0x555b292d), SkBits2Float(0x2a212a8c));  // 1.50606e+13f, 1.43144e-13f
path.conicTo(SkBits2Float(0xc0032108), SkBits2Float(0x7a6a4b7b), SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a3a7bc0));  // -2.04889f, 3.04132e+35f, 5.77848e-19f, 4.7323e-37f, 5.63611e+25f
path.lineTo(SkBits2Float(0x081f2ad7), SkBits2Float(0x7bc00321));  // 4.78977e-34f, 1.99397e+36f
path.moveTo(SkBits2Float(0x2a3a2147), SkBits2Float(0xdf212828));  // 1.65317e-13f, -1.16126e+19f
path.quadTo(SkBits2Float(0x4f1a3a8a), SkBits2Float(0x3ab38a28), SkBits2Float(0x29283ac2), SkBits2Float(0x962be62a));  // 2.58753e+09f, 0.00136978f, 3.73545e-14f, -1.38859e-25f
path.cubicTo(SkBits2Float(0x272a812a), SkBits2Float(0x3a2a5529), SkBits2Float(0x3b1e2ab0), SkBits2Float(0x29272021), SkBits2Float(0x3b3ac527), SkBits2Float(0x1fc42237));  // 2.36623e-15f, 0.000649768f, 0.00241343f, 3.71093e-14f, 0.00284989f, 8.30658e-20f
path.cubicTo(SkBits2Float(0x27576c2a), SkBits2Float(0x5921c25d), SkBits2Float(0x51523a70), SkBits2Float(0x12102a10), SkBits2Float(0x633a28d9), SkBits2Float(0x29c80927));  // 2.98959e-15f, 2.8457e+15f, 5.64327e+10f, 4.54902e-28f, 3.43404e+21f, 8.88337e-14f
path.lineTo(SkBits2Float(0x29292727), SkBits2Float(0x21475b3b));  // 3.75595e-14f, 6.75446e-19f
path.quadTo(SkBits2Float(0xdf28282a), SkBits2Float(0x3a8a3a21), SkBits2Float(0x8a284f9a), SkBits2Float(0x3ac23ab3));  // -1.2117e+19f, 0.00105459f, -8.10388e-33f, 0.00148185f
path.cubicTo(SkBits2Float(0x682d2928), SkBits2Float(0x555b6829), SkBits2Float(0x555b292d), SkBits2Float(0x2a212a8c), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 3.27091e+24f, 1.50775e+13f, 1.50606e+13f, 1.43144e-13f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x295b2ded), SkBits2Float(0x29685568), SkBits2Float(0x8c555b2d), SkBits2Float(0xe61d2a2a), SkBits2Float(0x2a63962b));  // 4.86676e-14f, 5.15884e-14f, -1.64364e-31f, -1.85547e+23f, 2.02138e-13f
path.conicTo(SkBits2Float(0x5568295b), SkBits2Float(0x5b2d2968), SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a), SkBits2Float(0x4b7bc003));  // 1.5954e+13f, 4.87407e+16f, 5.7784e-19f, 4.61198e-19f, 1.64987e+07f
path.lineTo(SkBits2Float(0x2a8ced7a), SkBits2Float(0x21081f21));  // 2.50338e-13f, 4.61198e-19f
path.conicTo(SkBits2Float(0x6a3a7bc0), SkBits2Float(0x2147ed7a), SkBits2Float(0x28282a3a), SkBits2Float(0x8a3a21df), SkBits2Float(0x27b42a3a));  // 5.63611e+25f, 6.77381e-19f, 9.33503e-15f, -8.96194e-33f, 5.00058e-15f
path.conicTo(SkBits2Float(0x2921217d), SkBits2Float(0x5e3a3b35), SkBits2Float(0x7828ee3a), SkBits2Float(0x8e28b03b), SkBits2Float(0x783be82a));  // 3.57782e-14f, 3.35484e+18f, 1.37053e+34f, -2.07925e-30f, 1.52448e+34f
path.conicTo(SkBits2Float(0x8e0b8a3a), SkBits2Float(0x279fd4e6), SkBits2Float(0x7a293a2a), SkBits2Float(0x2a0ef0c5), SkBits2Float(0x653b29d4));  // -1.71996e-30f, 4.43622e-15f, 2.19669e+35f, 1.26957e-13f, 5.52409e+22f
path.quadTo(SkBits2Float(0x29210f21), SkBits2Float(0x282a085d), SkBits2Float(0xc2ab2127), SkBits2Float(0xa6800028));  // 3.57623e-14f, 9.43871e-15f, -85.5648f, -8.88183e-16f
path.lineTo(SkBits2Float(0x2a3a2147), SkBits2Float(0xdf212828));  // 1.65317e-13f, -1.16126e+19f
path.close();
path.moveTo(SkBits2Float(0x2a3a2147), SkBits2Float(0xdf212828));  // 1.65317e-13f, -1.16126e+19f
path.quadTo(SkBits2Float(0x216a2770), SkBits2Float(0x2ab73b28), SkBits2Float(0x4b28f427), SkBits2Float(0x283b5b28));  // 7.93345e-19f, 3.25484e-13f, 1.10726e+07f, 1.04004e-14f
path.lineTo(SkBits2Float(0x2a3a2147), SkBits2Float(0xdf212828));  // 1.65317e-13f, -1.16126e+19f
path.close();
path.moveTo(SkBits2Float(0x2a3a2147), SkBits2Float(0xdf212828));  // 1.65317e-13f, -1.16126e+19f
path.conicTo(SkBits2Float(0xf86d273b), SkBits2Float(0x27e523e3), SkBits2Float(0x2927e0f5), SkBits2Float(0x2ac0e729), SkBits2Float(0x6b492128));  // -1.92402e+34f, 6.35992e-15f, 3.72766e-14f, 3.42665e-13f, 2.43151e+26f
path.cubicTo(SkBits2Float(0x2f273927), SkBits2Float(0xa83a2c21), SkBits2Float(0xd7122121), SkBits2Float(0x21212921), SkBits2Float(0x3be3db3a), SkBits2Float(0xa9deb63b));  // 1.52089e-10f, -1.03346e-14f, -1.60671e+14f, 5.46034e-19f, 0.00695362f, -9.89039e-14f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 1, filename);
}

// crbug.com/626164
static void fuzz763_1c(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
    path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
    path.cubicTo(SkBits2Float(0x1931204a), SkBits2Float(0x2ba1a14a), SkBits2Float(0x4a4a08ff), SkBits2Float(0x4a4a08ff), SkBits2Float(0x4a4a4a34), SkBits2Float(0x4a4a4a4a));  // 9.15721e-24f, 1.14845e-12f, 3.31014e+06f, 3.31014e+06f, 3.31432e+06f, 3.31432e+06f
    path.moveTo(SkBits2Float(0x000010a1), SkBits2Float(0x19312000));  // 5.96533e-42f, 9.15715e-24f
    path.cubicTo(SkBits2Float(0x4a4a4a4a), SkBits2Float(0x4a4a4a4a), SkBits2Float(0xa14a4a4a), SkBits2Float(0x08ff2ba1), SkBits2Float(0x08ff4a4a), SkBits2Float(0x4a344a4a));  // 3.31432e+06f, 3.31432e+06f, -6.85386e-19f, 1.53575e-33f, 1.53647e-33f, 2.95387e+06f
    path.cubicTo(SkBits2Float(0x4a4a4a4a), SkBits2Float(0x4a4a4a4a), SkBits2Float(0x2ba1a14a), SkBits2Float(0x4e4a08ff), SkBits2Float(0x4a4a4a4a), SkBits2Float(0xa1a181ff));  // 3.31432e+06f, 3.31432e+06f, 1.14845e-12f, 8.47397e+08f, 3.31432e+06f, -1.09442e-18f

    SkPath path2(path);
    SkPath dummy;
    testPathOp(reporter, path1, path2, (SkPathOp)4, filename);
}

// crbug.com/626186
static void fuzz763_1b(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);
    path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
    path.cubicTo(SkBits2Float(0x0000ff07), SkBits2Float(0xf9f9ff00), SkBits2Float(0xfe0ef9f4), SkBits2Float(0xd9b105fb), SkBits2Float(0x000000f9), SkBits2Float(0xfe11f901));  // 9.14866e-41f, -1.62257e+35f, -4.75121e+37f, -6.22846e+15f, 3.48923e-43f, -4.85077e+37f
    path.lineTo(SkBits2Float(0xda1905ed), SkBits2Float(0x3c05fbfb));  // -1.0768e+16f, 0.00817775f
    path.cubicTo(SkBits2Float(0x3c3c3c3c), SkBits2Float(0x3c3c3c3c), SkBits2Float(0x253c7f00), SkBits2Float(0xfa00d3fa), SkBits2Float(0x250025fe), SkBits2Float(0x00000006));  // 0.011489f, 0.011489f, 1.63494e-16f, -1.67228e+35f, 1.11151e-16f, 8.40779e-45f

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
    path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
    path.quadTo(SkBits2Float(0x3c3c3c3c), SkBits2Float(0xfa253c3c), SkBits2Float(0xfefa00d3), SkBits2Float(0x25fad9df));  // 0.011489f, -2.14488e+35f, -1.66156e+38f, 4.35157e-16f
    path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
    path.close();
    path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
    path.lineTo(SkBits2Float(0x8dfefa00), SkBits2Float(0xf0f9fad9));  // -1.57141e-30f, -6.1892e+29f
    path.cubicTo(SkBits2Float(0x20fe58f9), SkBits2Float(0x0525fbed), SkBits2Float(0x1905ffff), SkBits2Float(0x01f9f9f9), SkBits2Float(0xfbfe0ef9), SkBits2Float(0xfb212fff));  // 4.30882e-19f, 7.80453e-36f, 6.92764e-24f, 9.18268e-38f, -2.63829e+36f, -8.36933e+35f

    SkPath path2(path);
    testPathOp(reporter, path1, path2, (SkPathOp)2, filename);
}

static void fuzz763_1a(skiatest::Reporter* reporter, const char* filename) {
// FIXME: fails on Nexus5 by generating a path with points that are not inside the path bounds
#if DEBUG_UNDER_DEVELOPMENT
    return;
#endif
    SkPath path;
    path.setFillType((SkPath::FillType) 0);
    path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
    path.cubicTo(SkBits2Float(0x154be880), SkBits2Float(0x80000640), SkBits2Float(0x5559a419), SkBits2Float(0x59d55928), SkBits2Float(0x80045959), SkBits2Float(0x40154be8));  // 4.11789e-26f, -2.24208e-42f, 1.49562e+13f, 7.50652e+15f, -3.99394e-40f, 2.33276f

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
    path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
    path.quadTo(SkBits2Float(0x5559a419), SkBits2Float(0x59d55928), SkBits2Float(0xbd595959), SkBits2Float(0x3f3f3f09));  // 1.49562e+13f, 7.50652e+15f, -0.0530637f, 0.747056f
    path.moveTo(SkBits2Float(0x3f3f3f3f), SkBits2Float(0x3f3f3f3f));  // 0.747059f, 0.747059f
    path.moveTo(SkBits2Float(0x3f3f3f3f), SkBits2Float(0xff3f3f3f));  // 0.747059f, -2.54211e+38f
    path.lineTo(SkBits2Float(0x09090909), SkBits2Float(0x3038d509));  // 1.6495e-33f, 6.72416e-10f
    path.conicTo(SkBits2Float(0x5947ffff), SkBits2Float(0x40e88004), SkBits2Float(0x00002059), SkBits2Float(0x28555900), SkBits2Float(0x5959d559));  // 3.51844e+15f, 7.26563f, 1.16042e-41f, 1.18432e-14f, 3.83217e+15f
    path.lineTo(SkBits2Float(0x3f3f3f3f), SkBits2Float(0xff3f3f3f));  // 0.747059f, -2.54211e+38f
    path.close();
    path.moveTo(SkBits2Float(0x3f3f3f3f), SkBits2Float(0xff3f3f3f));  // 0.747059f, -2.54211e+38f
    path.lineTo(SkBits2Float(0x38d57f4b), SkBits2Float(0x59597f4b));  // 0.000101803f, 3.82625e+15f
    path.lineTo(SkBits2Float(0x3f3f3f3f), SkBits2Float(0xff3f3f3f));  // 0.747059f, -2.54211e+38f
    path.close();
    path.moveTo(SkBits2Float(0x384700ff), SkBits2Float(0x0108804b));  // 4.74462e-05f, 2.50713e-38f

    SkPath path2(path);
    testPathOp(reporter, path1, path2, (SkPathOp)0, filename);
}

// crbug.com/627780
static void fuzz763_3a(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0x555b292d), SkBits2Float(0x2a212a8c));  // 1.50606e+13f, 1.43144e-13f
path.conicTo(SkBits2Float(0xc0032108), SkBits2Float(0x7a6a4b7b), SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a3a7bc0));  // -2.04889f, 3.04132e+35f, 5.77848e-19f, 4.7323e-37f, 5.63611e+25f
path.conicTo(SkBits2Float(0x3a2147ed), SkBits2Float(0xdf28282a), SkBits2Float(0x3a8a3a21), SkBits2Float(0x8a284f9a), SkBits2Float(0x3ac23ab3));  // 0.000615238f, -1.2117e+19f, 0.00105459f, -8.10388e-33f, 0.00148185f
path.cubicTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x63962be6), SkBits2Float(0x272a812a), SkBits2Float(0x295b2d29), SkBits2Float(0x2a685568), SkBits2Float(0x68295b2d));  // 2.25206e-21f, 5.54035e+21f, 2.36623e-15f, 4.86669e-14f, 2.06354e-13f, 3.19905e+24f
path.conicTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x081f2a21), SkBits2Float(0x7bc00321), SkBits2Float(0x7a6a4b77), SkBits2Float(0x3a214726));  // 2.49282e-13f, 4.78968e-34f, 1.99397e+36f, 3.04132e+35f, 0.000615226f
path.moveTo(SkBits2Float(0x8adf2028), SkBits2Float(0x3a219a3a));  // -2.14862e-32f, 0.000616464f
path.quadTo(SkBits2Float(0x3ab38e28), SkBits2Float(0x29283ac2), SkBits2Float(0x2be61d2a), SkBits2Float(0x812a4396));  // 0.0013699f, 3.73545e-14f, 1.63506e-12f, -3.12726e-38f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 1, filename);
}

// crbug.com/627689
static void fuzz763_5a(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x38bd8610), SkBits2Float(0x00000000));  // 9.03719e-05f, 0
path.conicTo(SkBits2Float(0x4183d871), SkBits2Float(0x41fea321), SkBits2Float(0xb700ff00), SkBits2Float(0x4240b8b8), SkBits2Float(0x3b058283));  // 16.4807f, 31.8297f, -7.68877e-06f, 48.1804f, 0.0020372f
path.lineTo(SkBits2Float(0x3a3a3ab8), SkBits2Float(0xb8b8b8b8));  // 0.000710409f, -8.80821e-05f
path.conicTo(SkBits2Float(0x3a455ec8), SkBits2Float(0xb8b8b8b3), SkBits2Float(0x38b2418d), SkBits2Float(0xb730d014), SkBits2Float(0x3f7ffff3));  // 0.000752908f, -8.80821e-05f, 8.49991e-05f, -1.05389e-05f, 0.999999f
path.quadTo(SkBits2Float(0x3a51246a), SkBits2Float(0xb6da45a3), SkBits2Float(0x38bc5c3c), SkBits2Float(0x00000000));  // 0.000797814f, -6.50501e-06f, 8.98172e-05f, 0
path.lineTo(SkBits2Float(0x3a3a3ab8), SkBits2Float(0xb8b8b8b8));  // 0.000710409f, -8.80821e-05f
path.quadTo(SkBits2Float(0x39a32d2d), SkBits2Float(0x00000000), SkBits2Float(0xb8a13a00), SkBits2Float(0x00000000));  // 0.000311234f, 0, -7.68788e-05f, 0
path.lineTo(SkBits2Float(0x3a3a3ab8), SkBits2Float(0xb8b8b8b8));  // 0.000710409f, -8.80821e-05f
path.quadTo(SkBits2Float(0x39ba814c), SkBits2Float(0xb838fed2), SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0.00035573f, -4.41063e-05f, 0, 0
path.lineTo(SkBits2Float(0x38bd8610), SkBits2Float(0x00000000));  // 9.03719e-05f, 0
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);

    SkPath path2(path);
    testPathOp(reporter, path1, path2, (SkPathOp) 4, filename);
}

// crbug.com/627401
static void fuzz763_2a(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.quadTo(SkBits2Float(0x3e484500), SkBits2Float(0x164f3a30), SkBits2Float(0x49484801), SkBits2Float(0x7d0100c8));  // 0.195576f, 1.67397e-25f, 820352, 1.07172e+37f
path.conicTo(SkBits2Float(0xff7f36fd), SkBits2Float(0x3e647d01), SkBits2Float(0x0c00f430), SkBits2Float(0x486b6448), SkBits2Float(0x00484848));  // -3.39239e+38f, 0.223133f, 9.93424e-32f, 241041, 6.63809e-39f
path.lineTo(SkBits2Float(0x4f4f557d), SkBits2Float(0x48480112));  // 3.47849e+09f, 204804
path.lineTo(SkBits2Float(0xf40c01ff), SkBits2Float(0x45008000));  // -4.43702e+31f, 2056
path.moveTo(SkBits2Float(0x4bfffa00), SkBits2Float(0x7d4ac859));  // 3.35514e+07f, 1.68465e+37f
path.conicTo(SkBits2Float(0x7d014f3e), SkBits2Float(0x00f4ff01), SkBits2Float(0x6b64480c), SkBits2Float(0x48484848), SkBits2Float(0x557d0100));  // 1.07426e+37f, 2.24993e-38f, 2.75975e+26f, 205089, 1.73863e+13f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

// crbug.com/627761
static void fuzz763_2b(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x5b292d55), SkBits2Float(0x212a8c55));  // 4.76191e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x3b21081f), SkBits2Float(0x4b7bc003));  // 0.00245715f, 1.64987e+07f
path.lineTo(SkBits2Float(0x2a8ced7a), SkBits2Float(0x21081f21));  // 2.50338e-13f, 4.61198e-19f
path.conicTo(SkBits2Float(0x6a3a7bc0), SkBits2Float(0x4721ed7a), SkBits2Float(0x282a3a21), SkBits2Float(0x3a21df28), SkBits2Float(0x4f9a3a8a));  // 5.63611e+25f, 41453.5f, 9.4495e-15f, 0.000617492f, 5.17506e+09f
path.lineTo(SkBits2Float(0x3b21081f), SkBits2Float(0x4b7bc003));  // 0.00245715f, 1.64987e+07f
path.close();
path.moveTo(SkBits2Float(0x3b21081f), SkBits2Float(0x4b7bc003));  // 0.00245715f, 1.64987e+07f
path.cubicTo(SkBits2Float(0x273ac23a), SkBits2Float(0x1d2a2928), SkBits2Float(0x63962be6), SkBits2Float(0x272a812a), SkBits2Float(0x295b2d29), SkBits2Float(0x29685568));  // 2.5918e-15f, 2.25206e-21f, 5.54035e+21f, 2.36623e-15f, 4.86669e-14f, 5.15884e-14f
path.lineTo(SkBits2Float(0x081f2a21), SkBits2Float(0x7bc00321));  // 4.78968e-34f, 1.99397e+36f
path.lineTo(SkBits2Float(0x282a3a21), SkBits2Float(0x3a21df28));  // 9.4495e-15f, 0.000617492f
path.lineTo(SkBits2Float(0x3b21081f), SkBits2Float(0x4b7bc003));  // 0.00245715f, 1.64987e+07f
path.close();
path.moveTo(SkBits2Float(0x3b21081f), SkBits2Float(0x4b7bc003));  // 0.00245715f, 1.64987e+07f
path.quadTo(SkBits2Float(0x8a4fc29a), SkBits2Float(0x3ab3283a), SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6));  // -1.00033e-32f, 0.00136686f, 2.25206e-21f, 300.343f
path.moveTo(SkBits2Float(0x5b2d2a81), SkBits2Float(0x29276829));  // 4.87419e+16f, 3.71718e-14f
path.conicTo(SkBits2Float(0x1e2ab03a), SkBits2Float(0x2920213b), SkBits2Float(0x3b3ac527), SkBits2Float(0xc422333b), SkBits2Float(0x6c2a9f1f));  // 9.03617e-21f, 3.5556e-14f, 0.00284989f, -648.8f, 8.25075e+26f
path.quadTo(SkBits2Float(0xc25d2757), SkBits2Float(0x3a705921), SkBits2Float(0x2a105152), SkBits2Float(0x28d91210));  // -55.2884f, 0.000916855f, 1.2818e-13f, 2.40997e-14f
path.quadTo(SkBits2Float(0x68295b2d), SkBits2Float(0x2d296855), SkBits2Float(0x2a8c555b), SkBits2Float(0x081f2a21));  // 3.19905e+24f, 9.6297e-12f, 2.49282e-13f, 4.78968e-34f
path.lineTo(SkBits2Float(0x5b2d2a81), SkBits2Float(0x29276829));  // 4.87419e+16f, 3.71718e-14f
path.close();
path.moveTo(SkBits2Float(0x5b2d2a81), SkBits2Float(0x29276829));  // 4.87419e+16f, 3.71718e-14f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2a8ced7a), SkBits2Float(0x21081f21), SkBits2Float(0xcb7bc003), SkBits2Float(0x47ed7a6a));  // 6.14991e+25f, 2.50338e-13f, 4.61198e-19f, -1.64987e+07f, 121589
path.lineTo(SkBits2Float(0x5b2d2a81), SkBits2Float(0x29276829));  // 4.87419e+16f, 3.71718e-14f
path.close();
path.moveTo(SkBits2Float(0x5b2d2a81), SkBits2Float(0x29276829));  // 4.87419e+16f, 3.71718e-14f
path.quadTo(SkBits2Float(0xdf28282a), SkBits2Float(0x2d8a3a21), SkBits2Float(0x5b682b68), SkBits2Float(0x5b292d55));  // -1.2117e+19f, 1.57146e-11f, 6.53499e+16f, 4.76191e+16f
path.lineTo(SkBits2Float(0x2a212a8c), SkBits2Float(0x0321081f));  // 1.43144e-13f, 4.7323e-37f
path.conicTo(SkBits2Float(0x7a6a4b7b), SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a3a7bc0), SkBits2Float(0x3a21477a));  // 3.04132e+35f, 5.77848e-19f, 4.7323e-37f, 5.63611e+25f, 0.000615231f
path.moveTo(SkBits2Float(0x21df2828), SkBits2Float(0x9a3a8a3a));  // 1.51217e-18f, -3.85756e-23f
path.quadTo(SkBits2Float(0x3ab38a28), SkBits2Float(0x28273ac2), SkBits2Float(0xe61d2a29), SkBits2Float(0x2a63962b));  // 0.00136978f, 9.2831e-15f, -1.85547e+23f, 2.02138e-13f
path.conicTo(SkBits2Float(0x2d29272a), SkBits2Float(0x5568295b), SkBits2Float(0x5b2d2968), SkBits2Float(0x5b2d6829), SkBits2Float(0x212a8c55));  // 9.61523e-12f, 1.5954e+13f, 4.87407e+16f, 4.88097e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x3a2147ed), SkBits2Float(0xdf28282a), SkBits2Float(0x3a8a3a21), SkBits2Float(0x8a284f9a), SkBits2Float(0x3ac23ab3));  // 0.000615238f, -1.2117e+19f, 0.00105459f, -8.10388e-33f, 0.00148185f
path.lineTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.close();

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 4, filename);
}

static void fuzz763_2c(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x36344a4a));  // 0, 2.68653e-06f
path.cubicTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x364a4a4a), SkBits2Float(0x364a4a4a), SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0, 3.01436e-06f, 3.01436e-06f, 0, 0
path.lineTo(SkBits2Float(0x364a4a4a), SkBits2Float(0x00000000));  // 3.01436e-06f, 0
path.cubicTo(SkBits2Float(0x364a30f0), SkBits2Float(0x344ac7fb), SkBits2Float(0x3656d432), SkBits2Float(0x34cabb48), SkBits2Float(0x367031a9), SkBits2Float(0x351802f1));  // 3.01288e-06f, 1.88855e-07f, 3.2012e-06f, 3.77617e-07f, 3.57917e-06f, 5.66287e-07f
path.cubicTo(SkBits2Float(0x36a7b150), SkBits2Float(0x35ab09db), SkBits2Float(0x371874ed), SkBits2Float(0x3604f2c7), SkBits2Float(0x3784e0c7), SkBits2Float(0x36344a51));  // 4.99763e-06f, 1.27434e-06f, 9.08713e-06f, 1.98108e-06f, 1.58403e-05f, 2.68653e-06f
path.cubicTo(SkBits2Float(0x3743dc9a), SkBits2Float(0x36344a4f), SkBits2Float(0x36fbef33), SkBits2Float(0x36344a4e), SkBits2Float(0x36604a35), SkBits2Float(0x36344a4c));  // 1.16743e-05f, 2.68653e-06f, 7.50823e-06f, 2.68653e-06f, 3.34218e-06f, 2.68653e-06f
path.cubicTo(SkBits2Float(0x36531715), SkBits2Float(0x36344a4c), SkBits2Float(0x3645e3f5), SkBits2Float(0x36344a4b), SkBits2Float(0x3638b0d4), SkBits2Float(0x36344a4b));  // 3.14549e-06f, 2.68653e-06f, 2.9488e-06f, 2.68653e-06f, 2.75211e-06f, 2.68653e-06f
path.cubicTo(SkBits2Float(0x35f64120), SkBits2Float(0x36344a4b), SkBits2Float(0x35764124), SkBits2Float(0x36344a4a), SkBits2Float(0x00000000), SkBits2Float(0x36344a4a));  // 1.83474e-06f, 2.68653e-06f, 9.17369e-07f, 2.68653e-06f, 0, 2.68653e-06f
path.close();
    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.cubicTo(SkBits2Float(0x1931204a), SkBits2Float(0x2ba1a14a), SkBits2Float(0x4a4a08ff), SkBits2Float(0x4a4a08ff), SkBits2Float(0x4a4a4a34), SkBits2Float(0x4a4a4a4a));  // 9.15721e-24f, 1.14845e-12f, 3.31014e+06f, 3.31014e+06f, 3.31432e+06f, 3.31432e+06f
path.moveTo(SkBits2Float(0x000010a1), SkBits2Float(0x19312000));  // 5.96533e-42f, 9.15715e-24f
path.cubicTo(SkBits2Float(0x4a4a4a4a), SkBits2Float(0x4a4a4a4a), SkBits2Float(0xa14a4a4a), SkBits2Float(0x08ff2ba1), SkBits2Float(0x08ff4a4a), SkBits2Float(0x4a344a4a));  // 3.31432e+06f, 3.31432e+06f, -6.85386e-19f, 1.53575e-33f, 1.53647e-33f, 2.95387e+06f
path.cubicTo(SkBits2Float(0x544a4a4a), SkBits2Float(0x4a4a4a4a), SkBits2Float(0x2ba1a14a), SkBits2Float(0x4e4a08ff), SkBits2Float(0x4a4a4a4a), SkBits2Float(0xa1a181ff));  // 3.47532e+12f, 3.31432e+06f, 1.14845e-12f, 8.47397e+08f, 3.31432e+06f, -1.09442e-18f
    SkPath path2(path);
    testPathOpCheck(reporter, path1, path2, kReverseDifference_SkPathOp, filename, true);
}

static void fuzz763_6(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x6a2a291f));  // 0, 5.14279e+25f
path.cubicTo(SkBits2Float(0x68295b2d), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x68556829));  // 3.19905e+24f, 0, 0, 0, 0, 4.03114e+24f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x68555b2a));  // 0, 4.03018e+24f
path.cubicTo(SkBits2Float(0x00000000), SkBits2Float(0x67d55b2a), SkBits2Float(0x67296a4b), SkBits2Float(0x67555b2a), SkBits2Float(0x677e1f70), SkBits2Float(0x66d55b2a));  // 0, 2.01509e+24f, 8.00041e+23f, 1.00755e+24f, 1.20006e+24f, 5.03773e+23f
path.cubicTo(SkBits2Float(0x678f0684), SkBits2Float(0x6684f008), SkBits2Float(0x6798f8ea), SkBits2Float(0x6625a942), SkBits2Float(0x67961914), SkBits2Float(0x65ce709a));  // 1.35084e+24f, 3.1389e+23f, 1.44478e+24f, 1.95578e+23f, 1.41764e+24f, 1.21861e+23f
path.cubicTo(SkBits2Float(0x679174f7), SkBits2Float(0x63199132), SkBits2Float(0x6756c79f), SkBits2Float(0x606478de), SkBits2Float(0x65682bcf), SkBits2Float(0x00000000));  // 1.3738e+24f, 2.83281e+21f, 1.01427e+24f, 6.58526e+19f, 6.85248e+22f, 0
path.conicTo(SkBits2Float(0x68295b02), SkBits2Float(0x60f7f28b), SkBits2Float(0x00000000), SkBits2Float(0x6a2a291f), SkBits2Float(0x42784f5a));  // 3.19903e+24f, 1.42932e+20f, 0, 5.14279e+25f, 62.0775f
path.close();
path.moveTo(SkBits2Float(0x654d6d10), SkBits2Float(0x00000000));  // 6.06311e+22f, 0
path.lineTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x00000000));  // 6.14991e+25f, 0
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x6a4b7bc0));  // 0, 6.14991e+25f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x3ac23a55), SkBits2Float(0x2a292827));  // 0.00148184f, 1.50241e-13f
path.lineTo(SkBits2Float(0x63962be6), SkBits2Float(0x272a812a));  // 5.54035e+21f, 2.36623e-15f

    SkPath path2(path);
    testPathOpFail(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_7(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x0f2a312a), SkBits2Float(0xc0032108));  // 8.39112e-30f, -2.04889f
path.cubicTo(SkBits2Float(0x68392d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a1f2a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 3.4979e+24f, -2.71613e+29f, -1.64207e-31f, 5.76395e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x2d555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68345b2d), SkBits2Float(0xf0682955));  // 3.40683e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0xef2a8c55), SkBits2Float(0x295b2d2a), SkBits2Float(0x08685568), SkBits2Float(0x7bc00321));  // 5.76397e-19f, -5.27821e+28f, 4.86669e-14f, 6.99154e-34f, 1.99397e+36f
path.lineTo(SkBits2Float(0x68345b2d), SkBits2Float(0xf0682955));  // 3.40683e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68345b2d), SkBits2Float(0xf0682955));  // 3.40683e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.lineTo(SkBits2Float(0x3a8a3adf), SkBits2Float(0x8a281a4f));  // 0.00105461f, -8.09385e-33f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x272a812a), SkBits2Float(0x3a2a5529));  // 2.25206e-21f, 300.343f, 2.36623e-15f, 0.000649768f
path.lineTo(SkBits2Float(0x213b1e2a), SkBits2Float(0x27292720));  // 6.3398e-19f, 2.34747e-15f
path.conicTo(SkBits2Float(0xba1f203a), SkBits2Float(0xc422c538), SkBits2Float(0x215d5927), SkBits2Float(0x70ec2ac2), SkBits2Float(0x2a51523a));  // -0.000607017f, -651.082f, 7.49957e-19f, 5.84721e+29f, 1.85915e-13f
path.quadTo(SkBits2Float(0x633ad912), SkBits2Float(0x29c80927), SkBits2Float(0x272927b0), SkBits2Float(0x683a5b2d));  // 3.44674e+21f, 8.88337e-14f, 2.3475e-15f, 3.52017e+24f
path.lineTo(SkBits2Float(0x295b2d68), SkBits2Float(0x29685568));  // 4.86672e-14f, 5.15884e-14f
path.conicTo(SkBits2Float(0xaa8c555b), SkBits2Float(0x081f2a21), SkBits2Float(0x5b2d0321), SkBits2Float(0x68556829), SkBits2Float(0x2a552d29));  // -2.49282e-13f, 4.78968e-34f, 4.86986e+16f, 4.03114e+24f, 1.89339e-13f
path.cubicTo(SkBits2Float(0x21295b2d), SkBits2Float(0x2a688c5b), SkBits2Float(0x68295b2d), SkBits2Float(0x2d296855), SkBits2Float(0x8c08555b), SkBits2Float(0x2a2a29ca));  // 5.73801e-19f, 2.06544e-13f, 3.19905e+24f, 9.6297e-12f, -1.05027e-31f, 1.51135e-13f
path.quadTo(SkBits2Float(0x68295b21), SkBits2Float(0x2d296855), SkBits2Float(0x2a8c555b), SkBits2Float(0x081f2a21));  // 3.19904e+24f, 9.6297e-12f, 2.49282e-13f, 4.78968e-34f
path.lineTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.close();
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x5b2d6829), SkBits2Float(0x212a8c55), SkBits2Float(0xed7aba1f), SkBits2Float(0x2a212a8c));  // 6.14991e+25f, 4.88097e+16f, 5.7784e-19f, -4.84977e+27f, 1.43144e-13f
path.moveTo(SkBits2Float(0x2d212d08), SkBits2Float(0x5568295b));  // 9.16179e-12f, 1.5954e+13f
path.moveTo(SkBits2Float(0x5529685b), SkBits2Float(0x11295b68));  // 1.16416e+13f, 1.33599e-28f
path.conicTo(SkBits2Float(0x5b782968), SkBits2Float(0x3a292d55), SkBits2Float(0x2a8c555b), SkBits2Float(0x68295a2d), SkBits2Float(0x2d296855));  // 6.98513e+16f, 0.000645359f, 2.49282e-13f, 3.19897e+24f, 9.6297e-12f
path.moveTo(SkBits2Float(0x555b8c55), SkBits2Float(0x21682929));  // 1.50872e+13f, 7.86591e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0xac2d8ced), SkBits2Float(0x5b682968), SkBits2Float(0x5b292d55), SkBits2Float(0x212a8c55), SkBits2Float(0x081f282a));  // -2.4663e-12f, 6.53477e+16f, 4.76191e+16f, 5.7784e-19f, 4.78945e-34f
path.lineTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.close();
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2a8ced7a), SkBits2Float(0x03081f21), SkBits2Float(0x6a3a7bc0), SkBits2Float(0x2147ed7a));  // 6.14991e+25f, 2.50338e-13f, 4.00025e-37f, 5.63611e+25f, 6.77381e-19f
path.lineTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.close();
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.quadTo(SkBits2Float(0x2d28282a), SkBits2Float(0x5568295b), SkBits2Float(0x3a21df68), SkBits2Float(0x4f9a3a8a));  // 9.55861e-12f, 1.5954e+13f, 0.000617495f, 5.17506e+09f
path.lineTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.close();
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.cubicTo(SkBits2Float(0x5568c23a), SkBits2Float(0x5b2d2968), SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a), SkBits2Float(0x3a7bc003), SkBits2Float(0x294b2827));  // 1.59951e+13f, 4.87407e+16f, 5.7784e-19f, 4.61198e-19f, 0.00096035f, 4.51099e-14f

    SkPath path2(path);
    testPathOpFail(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void kfuzz2(skiatest::Reporter* reporter, const char* filename) {
    SkPath path1;
    SkPath path;
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0xfafadbfa));  // 0, -6.51268e+35f
path.close();
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0xfafadbfa));  // 0, -6.51268e+35f
path.cubicTo(SkBits2Float(0xe3000000), SkBits2Float(0xf19e92c7), SkBits2Float(0xf17febcb), SkBits2Float(0xff7febcb), SkBits2Float(0x60600100), SkBits2Float(0x0100ff60));  // -2.36118e+21f, -1.57043e+30f, -1.26726e+30f, -3.40177e+38f, 6.45647e+19f, 2.36931e-38f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0xfafadbfa));  // 0, -6.51268e+35f
path.close();
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0xfafadbfa));  // 0, -6.51268e+35f
path.lineTo(SkBits2Float(0x60601a1d), SkBits2Float(0x60606060));  // 6.4593e+19f, 6.46721e+19f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0xfafadbfa));  // 0, -6.51268e+35f
path.close();
path.moveTo(SkBits2Float(0xe5e2f300), SkBits2Float(0xee244a40));  // -1.33967e+23f, -1.27113e+28f
path.moveTo(SkBits2Float(0xba98ffee), SkBits2Float(0xfafafa1a));  // -0.0011673f, -6.51573e+35f
path.close();
path.moveTo(SkBits2Float(0xba98ffee), SkBits2Float(0xfafafa1a));  // -0.0011673f, -6.51573e+35f
path.lineTo(SkBits2Float(0xfafafafa), SkBits2Float(0xe30000fa));  // -6.51582e+35f, -2.36125e+21f
path.conicTo(SkBits2Float(0x92e592e5), SkBits2Float(0xfafafafb), SkBits2Float(0xc4fa0000), SkBits2Float(0x6060fafa), SkBits2Float(0x60606060));  // -1.44881e-27f, -6.51582e+35f, -2000, 6.48462e+19f, 6.46721e+19f
path.lineTo(SkBits2Float(0xba98ffee), SkBits2Float(0xfafafa1a));  // -0.0011673f, -6.51573e+35f
path.close();
path.moveTo(SkBits2Float(0xba98ffee), SkBits2Float(0xfafafa1a));  // -0.0011673f, -6.51573e+35f
path.cubicTo(SkBits2Float(0xe3000000), SkBits2Float(0xf19e92c7), SkBits2Float(0xf17febcb), SkBits2Float(0xff7febcb), SkBits2Float(0xfafafa00), SkBits2Float(0xfafafafa));  // -2.36118e+21f, -1.57043e+30f, -1.26726e+30f, -3.40177e+38f, -6.51572e+35f, -6.51582e+35f
path.lineTo(SkBits2Float(0xba98ffee), SkBits2Float(0xfafafa1a));  // -0.0011673f, -6.51573e+35f
path.close();
path.moveTo(SkBits2Float(0xba98ffee), SkBits2Float(0xfafafa1a));  // -0.0011673f, -6.51573e+35f
path.cubicTo(SkBits2Float(0xe3000000), SkBits2Float(0xe39e92c7), SkBits2Float(0xf17febcb), SkBits2Float(0xff7febcb), SkBits2Float(0xeed0ee9a), SkBits2Float(0x9a98ffca));  // -2.36118e+21f, -5.85032e+21f, -1.26726e+30f, -3.40177e+38f, -3.23307e+28f, -6.3279e-23f
path.lineTo(SkBits2Float(0xba98ffee), SkBits2Float(0xfafafa1a));  // -0.0011673f, -6.51573e+35f
path.close();
SkPath path2(path);
    testPathOpFail(reporter, path1, path2, kXOR_SkPathOp, filename);
}

static void fuzz763_10(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x68556829));  // 0, 4.03114e+24f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.quadTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x6a4b7bc4));  // 6.14991e+25f, 0, 0, 6.14991e+25f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x68556829));  // 0, 4.03114e+24f
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0x5b2d2968), SkBits2Float(0x2a8c8f55));  // 4.87407e+16f, 2.49685e-13f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.close();
path.moveTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.conicTo(SkBits2Float(0x682d2fed), SkBits2Float(0x755b6829), SkBits2Float(0x5b292d2b), SkBits2Float(0xc92a8c55), SkBits2Float(0x081f2a21));  // 3.27141e+24f, 2.78131e+32f, 4.76189e+16f, -698565, 4.78968e-34f
path.lineTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.close();
path.moveTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2a8ced7a), SkBits2Float(0x21081f21), SkBits2Float(0x3a7bc003), SkBits2Float(0x47ed7a29));  // 6.14991e+25f, 2.50338e-13f, 4.61198e-19f, 0.00096035f, 121588
path.lineTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.close();
path.moveTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.quadTo(SkBits2Float(0x6829682d), SkBits2Float(0x292d555b), SkBits2Float(0x2a8c555b), SkBits2Float(0x081f2a29));  // 3.20001e+24f, 3.84878e-14f, 2.49282e-13f, 4.78969e-34f
path.conicTo(SkBits2Float(0x6a497b19), SkBits2Float(0x218ced7a), SkBits2Float(0x0321081f), SkBits2Float(0x6a3a7bc0), SkBits2Float(0x47ed3a7a));  // 6.08939e+25f, 9.54963e-19f, 4.7323e-37f, 5.63611e+25f, 121461
path.lineTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.close();
path.moveTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.quadTo(SkBits2Float(0x282a282a), SkBits2Float(0x8a3a21df), SkBits2Float(0x2728282a), SkBits2Float(0x8a3a2129));  // 9.4456e-15f, -8.96194e-33f, 2.33365e-15f, -8.96181e-33f
path.quadTo(SkBits2Float(0x8a284f9a), SkBits2Float(0x3a3ac2b3), SkBits2Float(0x2a292827), SkBits2Float(0x962be61d));  // -8.10388e-33f, 0.000712435f, 1.50241e-13f, -1.38859e-25f
path.lineTo(SkBits2Float(0x272a802a), SkBits2Float(0x2a8c2d29));  // 2.36617e-15f, 2.49003e-13f
path.lineTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.close();
path.moveTo(SkBits2Float(0x4f9a3a29), SkBits2Float(0x3ab38a28));  // 5.17501e+09f, 0.00136978f
path.quadTo(SkBits2Float(0xc368305b), SkBits2Float(0x5b296855), SkBits2Float(0x2d8c5568), SkBits2Float(0x1f2a2172));  // -232.189f, 4.7684e+16f, 1.59541e-11f, 3.60266e-20f
path.lineTo(SkBits2Float(0x29c00321), SkBits2Float(0x5b4b7b13));  // 8.52706e-14f, 5.72747e+16f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 4, filename);
}

static void fuzz763_11(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x2a0f312a), SkBits2Float(0xc0032108));  // 1.2718e-13f, -2.04889f
path.cubicTo(SkBits2Float(0x68392d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a1f2a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 3.4979e+24f, -2.71613e+29f, -1.64207e-31f, 5.76395e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7b21), SkBits2Float(0x2829ed84), SkBits2Float(0x2d555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14984e+25f, 9.43289e-15f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68385b2d), SkBits2Float(0x70682955));  // 3.48239e+24f, 2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0xef2a8c55), SkBits2Float(0x295b2d2a), SkBits2Float(0x08685568), SkBits2Float(0x7bc00321));  // 5.76397e-19f, -5.27821e+28f, 4.86669e-14f, 6.99154e-34f, 1.99397e+36f
path.lineTo(SkBits2Float(0x68385b2d), SkBits2Float(0x70682955));  // 3.48239e+24f, 2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68385b2d), SkBits2Float(0x70682955));  // 3.48239e+24f, 2.87402e+29f
path.lineTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.lineTo(SkBits2Float(0x3a8a3adf), SkBits2Float(0x8a281a4f));  // 0.00105461f, -8.09385e-33f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x2a812a3b), SkBits2Float(0x2a552927));  // 2.25206e-21f, 300.343f, 2.29443e-13f, 1.89325e-13f
path.quadTo(SkBits2Float(0x3b1e2ab0), SkBits2Float(0x29272021), SkBits2Float(0x203a3b27), SkBits2Float(0x22c5381f));  // 0.00241343f, 3.71093e-14f, 1.57744e-19f, 5.34564e-18f
path.moveTo(SkBits2Float(0x5d27ec2a), SkBits2Float(0x705921c2));  // 7.56256e+17f, 2.68796e+29f
path.quadTo(SkBits2Float(0x102a5152), SkBits2Float(0x5b2dd912), SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 3.35892e-29f, 4.89338e+16f, 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032127));  // 3.60396e-20f, -2.0489f
path.cubicTo(SkBits2Float(0x68392d55), SkBits2Float(0x2a8c684b), SkBits2Float(0xf05b272d), SkBits2Float(0x2a1f1555), SkBits2Float(0x21082a21), SkBits2Float(0x6a4b7b03));  // 3.4979e+24f, 2.49414e-13f, -2.71298e+29f, 1.41294e-13f, 4.61343e-19f, 6.14982e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x2d555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.21279e-11f
path.moveTo(SkBits2Float(0x2a395b2d), SkBits2Float(0xf0682955));  // 1.64629e-13f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0xef2a8c55), SkBits2Float(0x295b2d2a), SkBits2Float(0x68210368), SkBits2Float(0x7bc05508));  // 5.76397e-19f, -5.27821e+28f, 4.86669e-14f, 3.04146e+24f, 1.99729e+36f
path.lineTo(SkBits2Float(0x2a395b2d), SkBits2Float(0xf0682955));  // 1.64629e-13f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x2a395b2d), SkBits2Float(0xf0682955));  // 1.64629e-13f, -2.87402e+29f
path.lineTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x2a21211f));  // 4.85282e+16f, 1.43112e-13f
path.lineTo(SkBits2Float(0x03552a8c), SkBits2Float(0x6a4f7b28));  // 6.26439e-37f, 6.27073e+25f
path.conicTo(SkBits2Float(0x2347ed93), SkBits2Float(0x282a3a21), SkBits2Float(0x3adf2128), SkBits2Float(0x4f1a3a8a), SkBits2Float(0x3ab38a28));  // 1.08381e-17f, 9.4495e-15f, 0.00170234f, 2.58753e+09f, 0.00136978f
path.lineTo(SkBits2Float(0x2a395b2d), SkBits2Float(0xf0682955));  // 1.64629e-13f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x2a395b2d), SkBits2Float(0xf0682955));  // 1.64629e-13f, -2.87402e+29f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x262a812a), SkBits2Float(0x3a2a5529));  // 2.25206e-21f, 300.343f, 5.91556e-16f, 0.000649768f
path.lineTo(SkBits2Float(0x213b1e2a), SkBits2Float(0x27292720));  // 6.3398e-19f, 2.34747e-15f
path.conicTo(SkBits2Float(0x371f203a), SkBits2Float(0xc52a22c4), SkBits2Float(0xc25d27ec), SkBits2Float(0x3a705921), SkBits2Float(0x5210513a));  // 9.48464e-06f, -2722.17f, -55.289f, 0.000916855f, 1.5496e+11f
path.cubicTo(SkBits2Float(0x63102ad9), SkBits2Float(0x29c80927), SkBits2Float(0x633a27b0), SkBits2Float(0x2909c827), SkBits2Float(0x272927b1), SkBits2Float(0x3a685b2d));  // 2.65942e+21f, 8.88337e-14f, 3.43395e+21f, 3.05937e-14f, 2.3475e-15f, 0.000886368f
path.moveTo(SkBits2Float(0x682d6829), SkBits2Float(0x29685555));  // 3.27556e+24f, 5.15884e-14f
path.conicTo(SkBits2Float(0xaa8c555b), SkBits2Float(0x081f2a21), SkBits2Float(0x5b2d0321), SkBits2Float(0x68556829), SkBits2Float(0x5b2d2729));  // -2.49282e-13f, 4.78968e-34f, 4.86986e+16f, 4.03114e+24f, 4.87382e+16f
path.quadTo(SkBits2Float(0x2d685568), SkBits2Float(0x5568295b), SkBits2Float(0x2a552d29), SkBits2Float(0x295b2d27));  // 1.32066e-11f, 1.5954e+13f, 1.89339e-13f, 4.86669e-14f
path.lineTo(SkBits2Float(0x682d6829), SkBits2Float(0x29685555));  // 3.27556e+24f, 5.15884e-14f
path.close();

    SkPath path2(path);
    testPathOpFailSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_12(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x6a29082a));  // 0, 5.10868e+25f
path.conicTo(SkBits2Float(0x6a295ac3), SkBits2Float(0x61bb988e), SkBits2Float(0x6829682d), SkBits2Float(0x5f3ba76a), SkBits2Float(0x42730a87));  // 5.11843e+25f, 4.32567e+20f, 3.20001e+24f, 1.35219e+19f, 60.7603f
path.conicTo(SkBits2Float(0x67aedf99), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x3f801112));  // 1.65163e+24f, 0, 0, 0, 1.00052f
path.close();
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 6.14991e+25f, 0, 0, 4.03114e+24f, 1.50617e+13f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x68555b2a));  // 0, 4.03018e+24f
path.cubicTo(SkBits2Float(0x00000000), SkBits2Float(0x67d55b2a), SkBits2Float(0x67296a4b), SkBits2Float(0x67555b2a), SkBits2Float(0x677e1f70), SkBits2Float(0x66d55b2a));  // 0, 2.01509e+24f, 8.00041e+23f, 1.00755e+24f, 1.20006e+24f, 5.03773e+23f
path.cubicTo(SkBits2Float(0x678f0684), SkBits2Float(0x6684f008), SkBits2Float(0x6798f8ea), SkBits2Float(0x6625a942), SkBits2Float(0x67961914), SkBits2Float(0x65ce709a));  // 1.35084e+24f, 3.1389e+23f, 1.44478e+24f, 1.95578e+23f, 1.41764e+24f, 1.21861e+23f
path.cubicTo(SkBits2Float(0x679158b0), SkBits2Float(0x00000000), SkBits2Float(0x67531e34), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 1.37276e+24f, 0, 9.96976e+23f, 0, 0, 0
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.conicTo(SkBits2Float(0x21081f21), SkBits2Float(0x4b7bc003), SkBits2Float(0xed237a6a), SkBits2Float(0x2d682967), SkBits2Float(0x2a8c555b));  // 4.61198e-19f, 1.64987e+07f, -3.16213e+27f, 1.31969e-11f, 2.49282e-13f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.close();
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0x3a6821df), SkBits2Float(0x2a8c3a8a));  // 0.000885514f, 2.49096e-13f
path.moveTo(SkBits2Float(0x29272a1d), SkBits2Float(0xb03a2a55));  // 3.7118e-14f, -6.77266e-10f
path.moveTo(SkBits2Float(0x20213b1e), SkBits2Float(0xc5272927));  // 1.36568e-19f, -2674.57f
path.quadTo(SkBits2Float(0xc422373b), SkBits2Float(0xec2a201f), SkBits2Float(0x21c25d27), SkBits2Float(0x523a7059));  // -648.863f, -8.22676e+26f, 1.31706e-18f, 2.00187e+11f
path.cubicTo(SkBits2Float(0x12102a10), SkBits2Float(0xe73a28d9), SkBits2Float(0xc8092763), SkBits2Float(0x2927b029), SkBits2Float(0x295b2d27), SkBits2Float(0x2d685568));  // 4.54902e-28f, -8.79114e+23f, -140446, 3.72342e-14f, 4.86669e-14f, 1.32066e-11f
path.moveTo(SkBits2Float(0x68556809), SkBits2Float(0x555b2d29));  // 4.03113e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a212a), SkBits2Float(0x2d032108));  // 3.60263e-20f, 7.45382e-12f
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x2a552d29));  // 4.03114e+24f, 1.89339e-13f
path.cubicTo(SkBits2Float(0x21295b2d), SkBits2Float(0x2a528c5b), SkBits2Float(0x284f5b2d), SkBits2Float(0x218aa621), SkBits2Float(0x3f2d2db3), SkBits2Float(0x68293a2a));  // 5.73801e-19f, 1.87004e-13f, 1.15106e-14f, 9.39522e-19f, 0.676479f, 3.19661e+24f

    SkPath path2(path);
    testPathOpFailSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_13(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x4793ed7a), SkBits2Float(0x282a3a21), SkBits2Float(0x3adf2128), SkBits2Float(0x4f1a3a8a));  // 6.14991e+25f, 75739, 9.4495e-15f, 0.00170234f, 2.58753e+09f
path.lineTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.close();
path.moveTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.cubicTo(SkBits2Float(0x3ac2213a), SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x272a8128), SkBits2Float(0x3a2a5529), SkBits2Float(0x3b1e2ab0));  // 0.00148109f, 2.25206e-21f, 300.343f, 2.36623e-15f, 0.000649768f, 0.00241343f
path.lineTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.close();
path.moveTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.cubicTo(SkBits2Float(0x3b272927), SkBits2Float(0x381f203a), SkBits2Float(0x2ac422c5), SkBits2Float(0xc25d27ec), SkBits2Float(0x3a705921), SkBits2Float(0x2a105152));  // 0.00255067f, 3.79386e-05f, 3.48407e-13f, -55.289f, 0.000916855f, 1.2818e-13f
path.quadTo(SkBits2Float(0x633ad912), SkBits2Float(0x29c80927), SkBits2Float(0x272927b0), SkBits2Float(0x68295b2d));  // 3.44674e+21f, 8.88337e-14f, 2.3475e-15f, 3.19905e+24f
path.lineTo(SkBits2Float(0x295b2d68), SkBits2Float(0x29685568));  // 4.86672e-14f, 5.15884e-14f
path.conicTo(SkBits2Float(0xaa8c555b), SkBits2Float(0x081f2a21), SkBits2Float(0x5b2d0321), SkBits2Float(0x68556829), SkBits2Float(0x2a552d29));  // -2.49282e-13f, 4.78968e-34f, 4.86986e+16f, 4.03114e+24f, 1.89339e-13f
path.cubicTo(SkBits2Float(0x21295b2d), SkBits2Float(0x2a688c5b), SkBits2Float(0x6829292d), SkBits2Float(0x2d296855), SkBits2Float(0x8c08555b), SkBits2Float(0x2a2a291f));  // 5.73801e-19f, 2.06544e-13f, 3.19536e+24f, 9.6297e-12f, -1.05027e-31f, 1.51133e-13f
path.conicTo(SkBits2Float(0x68295b21), SkBits2Float(0x2d296855), SkBits2Float(0x2a8c555b), SkBits2Float(0x081f2a21), SkBits2Float(0x7bc00321));  // 3.19904e+24f, 9.6297e-12f, 2.49282e-13f, 4.78968e-34f, 1.99397e+36f
path.lineTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.close();
path.moveTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.lineTo(SkBits2Float(0x5b2d6829), SkBits2Float(0x212a8c55));  // 4.88097e+16f, 5.7784e-19f
path.conicTo(SkBits2Float(0x8ced7aba), SkBits2Float(0x3f2a212a), SkBits2Float(0x2d212d08), SkBits2Float(0x5568295b), SkBits2Float(0x29685b2d));  // -3.65895e-31f, 0.664569f, 9.16179e-12f, 1.5954e+13f, 5.15934e-14f
path.lineTo(SkBits2Float(0x68295b68), SkBits2Float(0x2d296855));  // 3.19906e+24f, 9.6297e-12f
path.moveTo(SkBits2Float(0x212a8c55), SkBits2Float(0x21081f2a));  // 5.7784e-19f, 4.61198e-19f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2a8ced7a), SkBits2Float(0x21081f21), SkBits2Float(0x6aba7b03), SkBits2Float(0x2147ed7a));  // 6.14991e+25f, 2.50338e-13f, 4.61198e-19f, 1.12721e+26f, 6.77381e-19f
path.quadTo(SkBits2Float(0x6028282a), SkBits2Float(0x68292ddf), SkBits2Float(0x5b2d555b), SkBits2Float(0x68556829));  // 4.84679e+19f, 3.1957e+24f, 4.8789e+16f, 4.03114e+24f

    SkPath path2(path);
    testPathOp(reporter, path1, path2, (SkPathOp) 4, filename);
}

static void fuzz763_14(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0xf45b2d29));  // 4.03114e+24f, -6.94598e+31f
path.moveTo(SkBits2Float(0x1f2a302a), SkBits2Float(0xc8032108));  // 3.60387e-20f, -134276
path.cubicTo(SkBits2Float(0x68392d55), SkBits2Float(0xf0db684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x302a5b25), SkBits2Float(0xf0685568));  // 3.4979e+24f, -5.43226e+29f, -1.64207e-31f, 5.76527e-19f, 6.19752e-10f, -2.87615e+29f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_15(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x5b292d55), SkBits2Float(0x212a8c55));  // 4.76191e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a6b7bc4));  // 4.7323e-37f, 7.11705e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x2c6829c0), SkBits2Float(0x2a8c555b), SkBits2Float(0x081f2a29));  // 5.77848e-19f, 4.7323e-37f, 3.29924e-12f, 2.49282e-13f, 4.78969e-34f
path.lineTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a6b7bc4));  // 4.7323e-37f, 7.11705e+25f
path.close();
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a6b7bc4));  // 4.7323e-37f, 7.11705e+25f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x4793ed7a), SkBits2Float(0x282a3a21), SkBits2Float(0xdf218a28), SkBits2Float(0x4f1a3a3a));  // 6.14991e+25f, 75739, 9.4495e-15f, -1.16402e+19f, 2.58751e+09f
path.quadTo(SkBits2Float(0x3ab38a28), SkBits2Float(0x283ac221), SkBits2Float(0xe6432a29), SkBits2Float(0x2a96812b));  // 0.00136978f, 1.03672e-14f, -2.3041e+23f, 2.6735e-13f
path.lineTo(SkBits2Float(0x5529272a), SkBits2Float(0x1eb03a2a));  // 1.16241e+13f, 1.86588e-20f
path.conicTo(SkBits2Float(0x2a272021), SkBits2Float(0x3ac52729), SkBits2Float(0xc422313b), SkBits2Float(0xec2a201f), SkBits2Float(0x21c25d27));  // 1.48437e-13f, 0.00150416f, -648.769f, -8.22676e+26f, 1.31706e-18f
path.lineTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a6b7bc4));  // 4.7323e-37f, 7.11705e+25f
path.close();
path.moveTo(SkBits2Float(0x1051523a), SkBits2Float(0xd912102a));  // 4.12813e-29f, -2.56957e+15f
path.close();
path.moveTo(SkBits2Float(0x1051523a), SkBits2Float(0xd912102a));  // 4.12813e-29f, -2.56957e+15f
path.quadTo(SkBits2Float(0xc82763e7), SkBits2Float(0x2927b029), SkBits2Float(0x295b2d27), SkBits2Float(0x2d685568));  // -171408, 3.72342e-14f, 4.86669e-14f, 1.32066e-11f
path.moveTo(SkBits2Float(0x68556809), SkBits2Float(0x8c555b2d));  // 4.03113e+24f, -1.64364e-31f
path.moveTo(SkBits2Float(0x081f2a21), SkBits2Float(0x252d0321));  // 4.78968e-34f, 1.50064e-16f
path.moveTo(SkBits2Float(0x5568392a), SkBits2Float(0x5b2df068));  // 1.59583e+13f, 4.89595e+16f
path.quadTo(SkBits2Float(0x2a1f2a8c), SkBits2Float(0x21482a21), SkBits2Float(0x4b7bc003), SkBits2Float(0x8ced3a6a));  // 1.41368e-13f, 6.78184e-19f, 1.64987e+07f, -3.65508e-31f
path.moveTo(SkBits2Float(0x21481f21), SkBits2Float(0x4b7bc003));  // 6.78038e-19f, 1.64987e+07f
path.conicTo(SkBits2Float(0x6829ed27), SkBits2Float(0x2d155b2d), SkBits2Float(0x5568295b), SkBits2Float(0x5b2d2968), SkBits2Float(0x2a8c8f55));  // 3.20982e+24f, 8.48991e-12f, 1.5954e+13f, 4.87407e+16f, 2.49685e-13f
path.lineTo(SkBits2Float(0x21481f21), SkBits2Float(0x4b7bc003));  // 6.78038e-19f, 1.64987e+07f
path.close();
path.moveTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.conicTo(SkBits2Float(0x682d2fed), SkBits2Float(0x755b6829), SkBits2Float(0x5b292d2b), SkBits2Float(0xc92a8c55), SkBits2Float(0x081f2a21));  // 3.27141e+24f, 2.78131e+32f, 4.76189e+16f, -698565, 4.78968e-34f
path.lineTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.close();
path.moveTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x212aed7a), SkBits2Float(0x0321081f), SkBits2Float(0x293a7bc0), SkBits2Float(0x2147ed7a));  // 6.14991e+25f, 5.79125e-19f, 4.7323e-37f, 4.14076e-14f, 6.77381e-19f
path.quadTo(SkBits2Float(0x6829682d), SkBits2Float(0x292d555b), SkBits2Float(0x292a8c55), SkBits2Float(0x21081f2a));  // 3.20001e+24f, 3.84878e-14f, 3.78693e-14f, 4.61198e-19f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x218ced7a), SkBits2Float(0x0321081f), SkBits2Float(0x6a3a7bc0), SkBits2Float(0x47ed3a7a));  // 6.14991e+25f, 9.54963e-19f, 4.7323e-37f, 5.63611e+25f, 121461
path.lineTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.close();
path.moveTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.quadTo(SkBits2Float(0x282a282a), SkBits2Float(0x8a3a21df), SkBits2Float(0x2728282a), SkBits2Float(0x8a3a21df));  // 9.4456e-15f, -8.96194e-33f, 2.33365e-15f, -8.96194e-33f
path.quadTo(SkBits2Float(0x8a284f9a), SkBits2Float(0x3a3ac2b3), SkBits2Float(0x2a292827), SkBits2Float(0x962be61d));  // -8.10388e-33f, 0.000712435f, 1.50241e-13f, -1.38859e-25f
path.lineTo(SkBits2Float(0x272a802a), SkBits2Float(0x2a8c2d29));  // 2.36617e-15f, 2.49003e-13f
path.lineTo(SkBits2Float(0xc021211f), SkBits2Float(0x6a4b7b03));  // -2.51765f, 6.14982e+25f
path.close();
path.moveTo(SkBits2Float(0x4f9a3a29), SkBits2Float(0x3ab38a28));  // 5.17501e+09f, 0.00136978f
path.quadTo(SkBits2Float(0xc368305b), SkBits2Float(0x5b296855), SkBits2Float(0x2d8c5568), SkBits2Float(0x1f2a2172));  // -232.189f, 4.7684e+16f, 1.59541e-11f, 3.60266e-20f
path.lineTo(SkBits2Float(0x29c00321), SkBits2Float(0x5b4b7b13));  // 8.52706e-14f, 5.72747e+16f

    SkPath path2(path);
    testPathOpFailSkipAssert(reporter, path1, path2, (SkPathOp) 4, filename);
}

static void fuzz763_16(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68372d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c552775), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 3.46012e+24f, -2.71613e+29f, -1.64208e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x69555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.61207e+25f
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0682955));  // 3.35016e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x8cef552a), SkBits2Float(0x295b2d2a), SkBits2Float(0x68210368), SkBits2Float(0x7bc05508));  // 5.76397e-19f, -3.6875e-31f, 4.86669e-14f, 3.04146e+24f, 1.99729e+36f
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0682955));  // 3.35016e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0682955));  // 3.35016e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x68385b2d), SkBits2Float(0x555bf055), SkBits2Float(0x2a1f2a8c), SkBits2Float(0x03212a21), SkBits2Float(0x5a4b7bc0));  // 3.48239e+24f, 1.51141e+13f, 1.41368e-13f, 4.7362e-37f, 1.43189e+16f
path.conicTo(SkBits2Float(0xc08c2aed), SkBits2Float(0x211f2108), SkBits2Float(0x6a4b7b03), SkBits2Float(0x6829ed27), SkBits2Float(0x2d555b2d));  // -4.38024f, 5.3915e-19f, 6.14982e+25f, 3.20982e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x6e2a1f72), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x4793ed7a));  // 2.49282e-13f, 1.31626e+28f, 4.7323e-37f, 6.14991e+25f, 75739
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.quadTo(SkBits2Float(0x2128282a), SkBits2Float(0x3a8a3adf), SkBits2Float(0x8a284f1a), SkBits2Float(0x2c213ab3));  // 5.69738e-19f, 0.00105461f, -8.10378e-33f, 2.29121e-12f
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x3a2a812a), SkBits2Float(0x2a8ced29));  // 2.25206e-21f, 300.343f, 0.000650423f, 2.50336e-13f
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x03210831), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x681aed27), SkBits2Float(0x55555b2d), SkBits2Float(0x1e2a3a2a));  // 4.73231e-37f, 6.14991e+25f, 2.92648e+24f, 1.46617e+13f, 9.01175e-21f
path.conicTo(SkBits2Float(0x27202140), SkBits2Float(0x3a3b2769), SkBits2Float(0xc4371f20), SkBits2Float(0xecc52a22), SkBits2Float(0x21512727));  // 2.22225e-15f, 0.000713936f, -732.486f, -1.90686e+27f, 7.08638e-19f
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x6829523a), SkBits2Float(0x2d555b2d));  // 3.19839e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a322a), SkBits2Float(0xc0032108));  // 3.60404e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68572d55), SkBits2Float(0xf05bd24b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0xed4b7bc0));  // 4.06458e+24f, -2.72126e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, -3.93594e+27f
path.conicTo(SkBits2Float(0x212a8c6a), SkBits2Float(0x0329081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x2d555b2d));  // 5.77841e-19f, 4.9674e-37f, 6.14991e+25f, 9.43289e-15f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0682955));  // 3.33127e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x8cef552a), SkBits2Float(0x295b2d2a), SkBits2Float(0x68210368), SkBits2Float(0x7bc05508));  // 5.76397e-19f, -3.6875e-31f, 4.86669e-14f, 3.04146e+24f, 1.99729e+36f
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0682955));  // 3.33127e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0682955));  // 3.33127e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x555b6829), SkBits2Float(0x6c212a8c));  // 1.50775e+13f, 7.79352e+26f
path.conicTo(SkBits2Float(0x084b0321), SkBits2Float(0x6ac07b2a), SkBits2Float(0x395b2d7a), SkBits2Float(0x5bf05568), SkBits2Float(0x212a3a8c));  // 6.10918e-34f, 1.16348e+26f, 0.000209024f, 1.35296e+17f, 5.76757e-19f
path.lineTo(SkBits2Float(0x8c558c55), SkBits2Float(0x212a1f2a));  // -1.64512e-31f, 5.76395e-19f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_17(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68392d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 3.4979e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x69555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.61207e+25f
path.moveTo(SkBits2Float(0x6835282d), SkBits2Float(0xf0682955));  // 3.42196e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x2aef552a), SkBits2Float(0x68295b2d), SkBits2Float(0x08682103), SkBits2Float(0x4b7bc055));  // 5.76397e-19f, 4.2514e-13f, 3.19905e+24f, 6.98538e-34f, 1.64988e+07f
path.lineTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x68395b2d), SkBits2Float(0x555bf055), SkBits2Float(0x2a1f2a8c), SkBits2Float(0x03212a21), SkBits2Float(0x5a4b7bc0));  // 3.50128e+24f, 1.51141e+13f, 1.41368e-13f, 4.7362e-37f, 1.43189e+16f
path.conicTo(SkBits2Float(0xc08c2aed), SkBits2Float(0x211f2108), SkBits2Float(0x6a4b7b03), SkBits2Float(0x6829ed27), SkBits2Float(0x2d555b2d));  // -4.38024f, 5.3915e-19f, 6.14982e+25f, 3.20982e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x212a1f72), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x254793ed));  // 2.49282e-13f, 5.76399e-19f, 4.7323e-37f, 6.14991e+25f, 1.73106e-16f
path.quadTo(SkBits2Float(0x2128282a), SkBits2Float(0x3a8a3adf), SkBits2Float(0x8a284f1a), SkBits2Float(0xc2213ab3));  // 5.69738e-19f, 0.00105461f, -8.10378e-33f, -40.3073f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x3a2a812a), SkBits2Float(0x2a8ced29));  // 2.25206e-21f, 300.343f, 0.000650423f, 2.50336e-13f
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x03210831), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x6829ed27), SkBits2Float(0x55555b2d), SkBits2Float(0x1e2a3a2a));  // 4.73231e-37f, 6.14991e+25f, 3.20982e+24f, 1.46617e+13f, 9.01175e-21f
path.conicTo(SkBits2Float(0x27202140), SkBits2Float(0x3a3b2729), SkBits2Float(0xc4371f20), SkBits2Float(0x16c52a22), SkBits2Float(0x515d27ec));  // 2.22225e-15f, 0.000713932f, -732.486f, 3.18537e-25f, 5.93661e+10f
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x6829523a), SkBits2Float(0x2d555b2d));  // 3.19839e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68572d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 4.06458e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x2d555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x2a212a1f));  // 2.49282e-13f, 1.43143e-13f
path.lineTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x8c2aed7a), SkBits2Float(0x2a1f08c0));  // -1.31678e-31f, 1.41251e-13f
path.lineTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x2a8cef55), SkBits2Float(0x68295b2d));  // 2.50351e-13f, 3.19905e+24f
path.conicTo(SkBits2Float(0x55086821), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x5b2c6829), SkBits2Float(0x21218c55), SkBits2Float(0x2a6c1f03));  // 9.3738e+12f, 6.14991e+25f, 4.85282e+16f, 5.47346e-19f, 2.09718e-13f
path.lineTo(SkBits2Float(0x2a8cef55), SkBits2Float(0x68295b2d));  // 2.50351e-13f, 3.19905e+24f
path.close();
path.moveTo(SkBits2Float(0x2a8cef55), SkBits2Float(0x68295b2d));  // 2.50351e-13f, 3.19905e+24f
path.lineTo(SkBits2Float(0x6ac07b2a), SkBits2Float(0x395b2d7a));  // 1.16348e+26f, 0.000209024f

    SkPath path2(path);
    testPathOpFailSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_18(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68392d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 3.4979e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x69555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.61207e+25f
path.moveTo(SkBits2Float(0x6835282d), SkBits2Float(0xf0682955));  // 3.42196e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x2aef552a), SkBits2Float(0x68295b2d), SkBits2Float(0x08682103), SkBits2Float(0x4b7bc055));  // 5.76397e-19f, 4.2514e-13f, 3.19905e+24f, 6.98538e-34f, 1.64988e+07f
path.lineTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x68395b2d), SkBits2Float(0x555bf055), SkBits2Float(0x2a1f2a8c), SkBits2Float(0x03212a21), SkBits2Float(0x5a4b7bc0));  // 3.50128e+24f, 1.51141e+13f, 1.41368e-13f, 4.7362e-37f, 1.43189e+16f
path.conicTo(SkBits2Float(0xc08c2aed), SkBits2Float(0x211f2108), SkBits2Float(0x6a4b7b03), SkBits2Float(0x6829ed27), SkBits2Float(0x2d555b2d));  // -4.38024f, 5.3915e-19f, 6.14982e+25f, 3.20982e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x212a1f72), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x254793ed));  // 2.49282e-13f, 5.76399e-19f, 4.7323e-37f, 6.14991e+25f, 1.73106e-16f
path.quadTo(SkBits2Float(0x2128282a), SkBits2Float(0x3a8a3adf), SkBits2Float(0x8a284f1a), SkBits2Float(0xc2213ab3));  // 5.69738e-19f, 0.00105461f, -8.10378e-33f, -40.3073f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x3a2a812a), SkBits2Float(0x2a8ced29));  // 2.25206e-21f, 300.343f, 0.000650423f, 2.50336e-13f
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x03210831), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x6829ed27), SkBits2Float(0x55555b2d), SkBits2Float(0x1e2a3a2a));  // 4.73231e-37f, 6.14991e+25f, 3.20982e+24f, 1.46617e+13f, 9.01175e-21f
path.conicTo(SkBits2Float(0x27202140), SkBits2Float(0x3a3b2729), SkBits2Float(0xc4371f20), SkBits2Float(0x16c52a22), SkBits2Float(0x515d27ec));  // 2.22225e-15f, 0.000713932f, -732.486f, 3.18537e-25f, 5.93661e+10f
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x6829523a), SkBits2Float(0x2d555b2d));  // 3.19839e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68572d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 4.06458e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x2d555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x2a212a1f));  // 2.49282e-13f, 1.43143e-13f
path.lineTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x8c2aed7a), SkBits2Float(0x2a1f08c0));  // -1.31678e-31f, 1.41251e-13f

path.moveTo(SkBits2Float(0x6829523a), SkBits2Float(0x2d555b2d));  // 3.19839e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68572d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 4.06458e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x2a8c54ed), SkBits2Float(0x21081f21), SkBits2Float(0x4b7bc003), SkBits2Float(0x29ed846a), SkBits2Float(0x555b2d28));  // 2.49279e-13f, 4.61198e-19f, 1.64987e+07f, 1.05479e-13f, 1.50617e+13f
path.conicTo(SkBits2Float(0x68392d5b), SkBits2Float(0xf0682955), SkBits2Float(0x2a1f5b2d), SkBits2Float(0xef552a21), SkBits2Float(0x5b2d2a8c));  // 3.4979e+24f, -2.87402e+29f, 1.41537e-13f, -6.59712e+28f, 4.8742e+16f

    SkPath path2(path);
    // FIXME: fails on	Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Release-Fast, passes everywhere else
    testPathOpFlakySkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_19(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x21081f21), SkBits2Float(0x4b7bc003));  // 4.61198e-19f, 1.64987e+07f
path.lineTo(SkBits2Float(0x2829ed84), SkBits2Float(0x69555b2d));  // 9.43289e-15f, 1.61207e+25f
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0682955));  // 3.33127e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x2aef552a), SkBits2Float(0x68295b2d), SkBits2Float(0x08682103), SkBits2Float(0x4b7bc055));  // 5.76397e-19f, 4.2514e-13f, 3.19905e+24f, 6.98538e-34f, 1.64988e+07f
path.moveTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x68395b2d), SkBits2Float(0x8c5bf055), SkBits2Float(0x2a1f2a55), SkBits2Float(0x03212a21), SkBits2Float(0x5a4b7bc0));  // 3.50128e+24f, -1.69435e-31f, 1.41367e-13f, 4.7362e-37f, 1.43189e+16f
path.conicTo(SkBits2Float(0xc08c2aed), SkBits2Float(0x211f2108), SkBits2Float(0x6a4b7b03), SkBits2Float(0x6829ed27), SkBits2Float(0x2d555b2d));  // -4.38024f, 5.3915e-19f, 6.14982e+25f, 3.20982e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x212a1f72), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2547937a));  // 2.49282e-13f, 5.76399e-19f, 4.7323e-37f, 6.14991e+25f, 1.73105e-16f
path.quadTo(SkBits2Float(0x2128282a), SkBits2Float(0x3a8a3adf), SkBits2Float(0x8a284f1a), SkBits2Float(0xc2213ab3));  // 5.69738e-19f, 0.00105461f, -8.10378e-33f, -40.3073f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x3a2a812a), SkBits2Float(0x2a8ced29));  // 2.25206e-21f, 300.343f, 0.000650423f, 2.50336e-13f
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x03210831), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x6829ed27), SkBits2Float(0x55555b2d), SkBits2Float(0x1e2a3a2a));  // 4.73231e-37f, 6.14991e+25f, 3.20982e+24f, 1.46617e+13f, 9.01175e-21f
path.conicTo(SkBits2Float(0x27202140), SkBits2Float(0x3a3b2729), SkBits2Float(0xc4371f20), SkBits2Float(0xecc52a22), SkBits2Float(0x21515d27));  // 2.22225e-15f, 0.000713932f, -732.486f, -1.90686e+27f, 7.09352e-19f
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x6829523a), SkBits2Float(0x2d555b2d));  // 3.19839e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68572d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 4.06458e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x2d555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x8cef552a), SkBits2Float(0x295b2d2a), SkBits2Float(0x68210368), SkBits2Float(0x7bc05508));  // 5.76397e-19f, -3.6875e-31f, 4.86669e-14f, 3.04146e+24f, 1.99729e+36f
path.lineTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x555b2c29), SkBits2Float(0x6c212a8c));  // 1.50614e+13f, 7.79352e+26f
path.conicTo(SkBits2Float(0x084b0321), SkBits2Float(0x6ac07b2a), SkBits2Float(0x395b2d7a), SkBits2Float(0xf05b5568), SkBits2Float(0x212a3a8c));  // 6.10918e-34f, 1.16348e+26f, 0.000209024f, -2.71522e+29f, 5.76757e-19f
path.conicTo(SkBits2Float(0x290321d9), SkBits2Float(0x555b2d68), SkBits2Float(0x2a8c558c), SkBits2Float(0x2abe2a1f), SkBits2Float(0x7bc00321));  // 2.91172e-14f, 1.50618e+13f, 2.49284e-13f, 3.378e-13f, 1.99397e+36f
path.lineTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x8c2aed7a), SkBits2Float(0x1f2128c0));  // -1.31678e-31f, 3.41268e-20f
path.lineTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.close();

    SkPath path2(path);
    testPathOpFlakySkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_20(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68392d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 3.4979e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x69555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.61207e+25f
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0682955));  // 3.33127e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x2a8c552a), SkBits2Float(0x68295b2d), SkBits2Float(0x08682103), SkBits2Float(0x4b7bc055));  // 5.76397e-19f, 2.49281e-13f, 3.19905e+24f, 6.98538e-34f, 1.64988e+07f
path.lineTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x68395b2d), SkBits2Float(0x555bf055), SkBits2Float(0x2a1f2a8c), SkBits2Float(0x03212a21), SkBits2Float(0x5a4b7bc0));  // 3.50128e+24f, 1.51141e+13f, 1.41368e-13f, 4.7362e-37f, 1.43189e+16f
path.conicTo(SkBits2Float(0xc08c2aed), SkBits2Float(0x211f2108), SkBits2Float(0x6a4b7b03), SkBits2Float(0x6829ed27), SkBits2Float(0x2d555b2d));  // -4.38024f, 5.3915e-19f, 6.14982e+25f, 3.20982e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x6e2a1f72), SkBits2Float(0x0321182a), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x4793ed7a));  // 2.49282e-13f, 1.31626e+28f, 4.73414e-37f, 6.14991e+25f, 75739
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.quadTo(SkBits2Float(0x2128282a), SkBits2Float(0x3a8a3adf), SkBits2Float(0x8a284f1a), SkBits2Float(0x2c213ab3));  // 5.69738e-19f, 0.00105461f, -8.10378e-33f, 2.29121e-12f
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x3a2a812a), SkBits2Float(0x2a8ced29));  // 2.25206e-21f, 300.343f, 0.000650423f, 2.50336e-13f
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x03210831), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x6829ed27), SkBits2Float(0x55555b2d), SkBits2Float(0x1e2a3a2a));  // 4.73231e-37f, 6.14991e+25f, 3.20982e+24f, 1.46617e+13f, 9.01175e-21f
path.conicTo(SkBits2Float(0x27202140), SkBits2Float(0x3a3b2769), SkBits2Float(0xc4371f20), SkBits2Float(0xecc52a22), SkBits2Float(0x51282727));  // 2.22225e-15f, 0.000713936f, -732.486f, -1.90686e+27f, 4.51382e+10f
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0685527));  // 3.33127e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x6829523a), SkBits2Float(0x2d555b2d));  // 3.19839e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x8c555b2d));  // 4.03114e+24f, -1.64364e-31f
path.moveTo(SkBits2Float(0x081f2a31), SkBits2Float(0xc0032921));  // 4.78969e-34f, -2.04939f
path.cubicTo(SkBits2Float(0x68572d55), SkBits2Float(0xf05bd24b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0xed4b7bc0));  // 4.06458e+24f, -2.72126e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, -3.93594e+27f
path.conicTo(SkBits2Float(0x212a8c6a), SkBits2Float(0x4329081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x5b2d2d55));  // 5.77841e-19f, 169.032f, 6.14991e+25f, 9.43289e-15f, 4.8745e+16f
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x8cef552a), SkBits2Float(0x295b2d2a), SkBits2Float(0x3a210368), SkBits2Float(0x7bc05508));  // 5.76397e-19f, -3.6875e-31f, 4.86669e-14f, 0.000614217f, 1.99729e+36f
path.lineTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x555b6829), SkBits2Float(0x6c212a8c));  // 1.50775e+13f, 7.79352e+26f
path.lineTo(SkBits2Float(0x5b2d7a6a), SkBits2Float(0xf0556830));  // 4.88298e+16f, -2.64185e+29f
path.lineTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x0321d90a), SkBits2Float(0x555b2d68), SkBits2Float(0x2a8c558c), SkBits2Float(0x212a2a1f), SkBits2Float(0x4b7bc003));  // 4.75628e-37f, 1.50618e+13f, 2.49284e-13f, 5.7654e-19f, 1.64987e+07f
path.lineTo(SkBits2Float(0x8c2aed7a), SkBits2Float(0x212128c0));  // -1.31678e-31f, 5.46029e-19f
path.lineTo(SkBits2Float(0x68395b2d), SkBits2Float(0xf0682955));  // 3.50128e+24f, -2.87402e+29f
path.close();

    SkPath path2(path);
    testPathOpFailSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_21(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x6828c6f9), SkBits2Float(0x6614dc9e));  // 3.18811e+24f, 1.75745e+23f
path.cubicTo(SkBits2Float(0x68303469), SkBits2Float(0x661f92fc), SkBits2Float(0x6837d3c3), SkBits2Float(0x662b0eb2), SkBits2Float(0x683fa268), SkBits2Float(0x663759e1));  // 3.32841e+24f, 1.88392e+23f, 3.4724e+24f, 2.01949e+23f, 3.61987e+24f, 2.16463e+23f
path.cubicTo(SkBits2Float(0x68c4391f), SkBits2Float(0x672c5c9f), SkBits2Float(0x688b20ab), SkBits2Float(0x6804b825), SkBits2Float(0x681ddb5e), SkBits2Float(0x6838dc00));  // 7.4131e+24f, 8.13956e+23f, 5.25609e+24f, 2.507e+24f, 2.98183e+24f, 3.49189e+24f
path.lineTo(SkBits2Float(0x6828c6f9), SkBits2Float(0x6614dc9e));  // 3.18811e+24f, 1.75745e+23f
path.close();
path.moveTo(SkBits2Float(0x68226c73), SkBits2Float(0x660bd15e));  // 3.0681e+24f, 1.65068e+23f
path.cubicTo(SkBits2Float(0x6823b0e1), SkBits2Float(0x660d990f), SkBits2Float(0x6824f6d5), SkBits2Float(0x660f668c), SkBits2Float(0x68263e4e), SkBits2Float(0x66113632));  // 3.09203e+24f, 1.67169e+23f, 3.11609e+24f, 1.69298e+23f, 3.14025e+24f, 1.71436e+23f
path.cubicTo(SkBits2Float(0x682715e4), SkBits2Float(0x6612676d), SkBits2Float(0x6827ee22), SkBits2Float(0x66139997), SkBits2Float(0x6828c709), SkBits2Float(0x6614cba5));  // 3.15616e+24f, 1.72843e+23f, 3.17211e+24f, 1.74255e+23f, 3.18812e+24f, 1.75667e+23f
path.lineTo(SkBits2Float(0x6828d720), SkBits2Float(0x6604a1a2));  // 3.1893e+24f, 1.56583e+23f
path.cubicTo(SkBits2Float(0x68270421), SkBits2Float(0x6601102c), SkBits2Float(0x68252b97), SkBits2Float(0x65fb1edd), SkBits2Float(0x68234ce5), SkBits2Float(0x65f4367f));  // 3.15485e+24f, 1.52371e+23f, 3.11998e+24f, 1.48235e+23f, 3.08466e+24f, 1.44158e+23f
path.conicTo(SkBits2Float(0x6822e012), SkBits2Float(0x6602acc5), SkBits2Float(0x68226c73), SkBits2Float(0x660bd15e), SkBits2Float(0x3f7ffa04));  // 3.07663e+24f, 1.54274e+23f, 3.0681e+24f, 1.65068e+23f, 0.999909f
path.close();
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x6a2a291f));  // 0, 5.14279e+25f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x68555b2a));  // 0, 4.03018e+24f
path.cubicTo(SkBits2Float(0x00000000), SkBits2Float(0x68617414), SkBits2Float(0x66af1c42), SkBits2Float(0x68624f96), SkBits2Float(0x6757755b), SkBits2Float(0x685b93f2));  // 0, 4.25869e+24f, 4.13468e+23f, 4.27489e+24f, 1.01747e+24f, 4.14771e+24f
path.cubicTo(SkBits2Float(0x67a63a84), SkBits2Float(0x68fe1c37), SkBits2Float(0x67c05eed), SkBits2Float(0x69930962), SkBits2Float(0x00000000), SkBits2Float(0x6a2a291f));  // 1.56998e+24f, 9.60001e+24f, 1.81689e+24f, 2.22196e+25f, 0, 5.14279e+25f
path.close();
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x6a2a291f));  // 0, 5.14279e+25f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x6a4b7bc4));  // 0, 6.14991e+25f
path.cubicTo(SkBits2Float(0x6a2c8798), SkBits2Float(0x68f7a144), SkBits2Float(0x6951f5ea), SkBits2Float(0x6796ad55), SkBits2Float(0x683fa268), SkBits2Float(0x663759e1));  // 5.21439e+25f, 9.35519e+24f, 1.58642e+25f, 1.4231e+24f, 3.61987e+24f, 2.16463e+23f
path.cubicTo(SkBits2Float(0x683871e3), SkBits2Float(0x66253b4f), SkBits2Float(0x6830da01), SkBits2Float(0x66144d3e), SkBits2Float(0x6828d720), SkBits2Float(0x6604a1a2));  // 3.48407e+24f, 1.95071e+23f, 3.34063e+24f, 1.75084e+23f, 3.1893e+24f, 1.56583e+23f
path.conicTo(SkBits2Float(0x68295b21), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x492bb324));  // 3.19904e+24f, 0, 0, 0, 703282
path.cubicTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x677b84f0), SkBits2Float(0x00000000), SkBits2Float(0x68226c73), SkBits2Float(0x660bd15e));  // 0, 0, 1.18777e+24f, 0, 3.0681e+24f, 1.65068e+23f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x68156829));  // 0, 2.82222e+24f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x68555b2a));  // 0, 4.03018e+24f
path.lineTo(SkBits2Float(0x673918f3), SkBits2Float(0x681b0f5f));  // 8.74098e+23f, 2.929e+24f
path.lineTo(SkBits2Float(0x67391759), SkBits2Float(0x681b0fae));  // 8.74068e+23f, 2.92902e+24f
path.cubicTo(SkBits2Float(0x674384e7), SkBits2Float(0x682e2068), SkBits2Float(0x674db698), SkBits2Float(0x6843893b), SkBits2Float(0x6757755b), SkBits2Float(0x685b93f2));  // 9.23313e+23f, 3.28916e+24f, 9.71453e+23f, 3.69357e+24f, 1.01747e+24f, 4.14771e+24f
path.cubicTo(SkBits2Float(0x67a63484), SkBits2Float(0x68556bdd), SkBits2Float(0x67f18c5f), SkBits2Float(0x6848eb25), SkBits2Float(0x681ddb5e), SkBits2Float(0x6838dc00));  // 1.56976e+24f, 4.03142e+24f, 2.28136e+24f, 3.79524e+24f, 2.98183e+24f, 3.49189e+24f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x6a2a291f));  // 0, 5.14279e+25f
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);

    SkPath path2(path);
    testPathOp(reporter, path1, path2, (SkPathOp) 1, filename);
}

static void fuzz763_22(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x68295b2d));  // 0, 3.19905e+24f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0x6a3a7bc0), SkBits2Float(0x00000000));  // 5.63611e+25f, 0
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x6a034b21));  // 0, 3.9681e+25f
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x68295b2d));  // 0, 3.19905e+24f
path.close();
path.moveTo(SkBits2Float(0x6617da56), SkBits2Float(0x00000000));  // 1.79276e+23f, 0
path.conicTo(SkBits2Float(0x5e704d09), SkBits2Float(0x5e3a4dfd), SkBits2Float(0x00000000), SkBits2Float(0x65eb62ef), SkBits2Float(0x430fa5e6));  // 4.32888e+18f, 3.35617e+18f, 0, 1.38948e+23f, 143.648f
path.conicTo(SkBits2Float(0x5e798b32), SkBits2Float(0x627a95c0), SkBits2Float(0x61f5014c), SkBits2Float(0x61fba0fd), SkBits2Float(0x40f8a1a1));  // 4.49538e+18f, 1.15562e+21f, 5.64943e+20f, 5.80217e+20f, 7.76973f
path.conicTo(SkBits2Float(0x62743d2d), SkBits2Float(0x5e49b862), SkBits2Float(0x6617da56), SkBits2Float(0x00000000), SkBits2Float(0x410ef54c));  // 1.12635e+21f, 3.63387e+18f, 1.79276e+23f, 0, 8.93489f
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.quadTo(SkBits2Float(0x4f9a3a8a), SkBits2Float(0xc28a0d28), SkBits2Float(0x273a3ab3), SkBits2Float(0x8b2a2928));  // 5.17506e+09f, -69.0257f, 2.58445e-15f, -3.27718e-32f
path.lineTo(SkBits2Float(0x63283ae6), SkBits2Float(0x27282a81));  // 3.1033e+21f, 2.33377e-15f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 3, filename);
}

static void fuzz763_23(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68392d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x03210c2a), SkBits2Float(0x6a4b7bc0));  // 3.4979e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.73276e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x69555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.61207e+25f
path.moveTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0682955));  // 3.33127e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x2aef552a), SkBits2Float(0x29295b2d), SkBits2Float(0x68210368), SkBits2Float(0x7bc05508));  // 5.76397e-19f, 4.2514e-13f, 3.76046e-14f, 3.04146e+24f, 1.99729e+36f
path.lineTo(SkBits2Float(0x68305b2d), SkBits2Float(0xf0682955));  // 3.33127e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x68395b2d), SkBits2Float(0x8c5bf055), SkBits2Float(0x2a1f2a55), SkBits2Float(0x03212a21), SkBits2Float(0x5a4b7bc0));  // 3.50128e+24f, -1.69435e-31f, 1.41367e-13f, 4.7362e-37f, 1.43189e+16f
path.conicTo(SkBits2Float(0xc08c2aed), SkBits2Float(0x211f2108), SkBits2Float(0x6a4b7b03), SkBits2Float(0x6829ed27), SkBits2Float(0x2d555b2d));  // -4.38024f, 5.3915e-19f, 6.14982e+25f, 3.20982e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x08211f72), SkBits2Float(0x032a2a21), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2547937a));  // 2.49282e-13f, 4.84861e-34f, 5.00069e-37f, 6.14991e+25f, 1.73105e-16f
path.quadTo(SkBits2Float(0x2128282a), SkBits2Float(0x3a8a3adf), SkBits2Float(0x8a284f1a), SkBits2Float(0xc2213ab3));  // 5.69738e-19f, 0.00105461f, -8.10378e-33f, -40.3073f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x3a2a812a), SkBits2Float(0x2a8ced29));  // 2.25206e-21f, 300.343f, 0.000650423f, 2.50336e-13f
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x03210831), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x6829ed27), SkBits2Float(0x55555b2d), SkBits2Float(0x1e2a3a2a));  // 4.73231e-37f, 6.14991e+25f, 3.20982e+24f, 1.46617e+13f, 9.01175e-21f
path.conicTo(SkBits2Float(0x27202140), SkBits2Float(0x3a3b2729), SkBits2Float(0xc4371f20), SkBits2Float(0xecc52a22), SkBits2Float(0x21515d27));  // 2.22225e-15f, 0.000713932f, -732.486f, -1.90686e+27f, 7.09352e-19f
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0685527));  // 3.35016e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x6829523a), SkBits2Float(0x2d555b2d));  // 3.19839e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68572d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 4.06458e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x2a8c54ed), SkBits2Float(0x21081f21), SkBits2Float(0x4b7bc003), SkBits2Float(0x29ed846a), SkBits2Float(0x555b2d28));  // 2.49279e-13f, 4.61198e-19f, 1.64987e+07f, 1.05479e-13f, 1.50617e+13f
path.conicTo(SkBits2Float(0x68392d5b), SkBits2Float(0xf0682955), SkBits2Float(0x2a1f5b2d), SkBits2Float(0xef552a21), SkBits2Float(0x5b2d2a8c));  // 3.4979e+24f, -2.87402e+29f, 1.41537e-13f, -6.59712e+28f, 4.8742e+16f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_24(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0xededed02), SkBits2Float(0xedededed));  // -9.20431e+27f, -9.20445e+27f
path.close();
path.moveTo(SkBits2Float(0xededed02), SkBits2Float(0xedededed));  // -9.20431e+27f, -9.20445e+27f
path.quadTo(SkBits2Float(0x9fb9c16e), SkBits2Float(0x27737375), SkBits2Float(0xb7c5ff00), SkBits2Float(0x00ff9908));  // -7.86706e-20f, 3.37856e-15f, -2.3603e-05f, 2.34729e-38f
path.moveTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.cubicTo(SkBits2Float(0x1616ecec), SkBits2Float(0x2c321616), SkBits2Float(0x3516c616), SkBits2Float(0x6e161616), SkBits2Float(0x4c416033), SkBits2Float(0xf6000000));  // 1.21917e-25f, 2.53076e-12f, 5.61676e-07f, 1.16124e+28f, 5.06923e+07f, -6.49037e+32f
path.quadTo(SkBits2Float(0x04007f41), SkBits2Float(0xecececec), SkBits2Float(0xecececec), SkBits2Float(0xecec41ec));  // 1.51048e-36f, -2.2914e+27f, -2.2914e+27f, -2.28494e+27f
path.lineTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.close();
path.moveTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.quadTo(SkBits2Float(0x000000ec), SkBits2Float(0xececcc00), SkBits2Float(0x48ececec), SkBits2Float(0x0278806e));  // 3.30706e-43f, -2.29016e+27f, 485223, 1.8257e-37f
path.lineTo(SkBits2Float(0x72ececec), SkBits2Float(0xecec02ec));  // 9.38559e+30f, -2.28256e+27f
path.quadTo(SkBits2Float(0xec04007f), SkBits2Float(0xecececec), SkBits2Float(0xecececec), SkBits2Float(0xecec0400));  // -6.38322e+26f, -2.2914e+27f, -2.2914e+27f, -2.2826e+27f
path.lineTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.close();
path.moveTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.quadTo(SkBits2Float(0x000040ec), SkBits2Float(0x3a333300), SkBits2Float(0xecec3333), SkBits2Float(0xececdbec));  // 2.32896e-41f, 0.000683591f, -2.28439e+27f, -2.29076e+27f
path.lineTo(SkBits2Float(0x3300007f), SkBits2Float(0x33d83333));  // 2.98028e-08f, 1.00676e-07f
path.lineTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.close();
path.moveTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.quadTo(SkBits2Float(0x9e9ea900), SkBits2Float(0x33ececec), SkBits2Float(0xececec33), SkBits2Float(0xec336e6e));  // -1.67988e-20f, 1.10327e-07f, -2.29138e+27f, -8.67677e+26f
path.lineTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.close();
path.moveTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.lineTo(SkBits2Float(0xedededed), SkBits2Float(0xedededed));  // -9.20445e+27f, -9.20445e+27f
path.lineTo(SkBits2Float(0xecececec), SkBits2Float(0xecececec));  // -2.2914e+27f, -2.2914e+27f
path.lineTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.close();
path.moveTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.lineTo(SkBits2Float(0x01003300), SkBits2Float(0x33d83333));  // 2.35465e-38f, 1.00676e-07f
path.quadTo(SkBits2Float(0xecec3333), SkBits2Float(0x04eeedec), SkBits2Float(0xe0e0e0e0), SkBits2Float(0x9ee0e0e0));  // -2.28439e+27f, 5.6172e-36f, -1.29634e+20f, -2.38099e-20f
path.lineTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.close();
path.moveTo(SkBits2Float(0x73737300), SkBits2Float(0x73735273));  // 1.9288e+31f, 1.9278e+31f
path.cubicTo(SkBits2Float(0x299e9e9e), SkBits2Float(0xecececec), SkBits2Float(0xececb6ec), SkBits2Float(0xf0ececec), SkBits2Float(0x0000ecec), SkBits2Float(0x9ebe6e6e));  // 7.04413e-14f, -2.2914e+27f, -2.28936e+27f, -5.86599e+29f, 8.49916e-41f, -2.01627e-20f
path.cubicTo(SkBits2Float(0x9e9e9e9e), SkBits2Float(0xe8009e9e), SkBits2Float(0x9e9e9e9e), SkBits2Float(0xecec9e9e), SkBits2Float(0xec3333ec), SkBits2Float(0xececf0ec));  // -1.67945e-20f, -2.42956e+24f, -1.67945e-20f, -2.28844e+27f, -8.66572e+26f, -2.29155e+27f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 2, filename);
}

static void fuzz763_25(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x6a4b7bc4));  // 0, 6.14991e+25f
path.conicTo(SkBits2Float(0x653140d9), SkBits2Float(0x6a4b4f74), SkBits2Float(0x65906630), SkBits2Float(0x6a25a070), SkBits2Float(0x3f6728a2));  // 5.23159e+22f, 6.14468e+25f, 8.52382e+22f, 5.00576e+25f, 0.902964f
path.cubicTo(SkBits2Float(0x68295bc5), SkBits2Float(0x00000000), SkBits2Float(0x682958ff), SkBits2Float(0x00000000), SkBits2Float(0x68286829), SkBits2Float(0x00000000));  // 3.19909e+24f, 0, 3.19889e+24f, 0, 3.18112e+24f, 0
path.lineTo(SkBits2Float(0x68555b29), SkBits2Float(0x00000000));  // 4.03018e+24f, 0
path.conicTo(SkBits2Float(0x00000000), SkBits2Float(0x682d2927), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x6829686f));  // 0, 3.27091e+24f, 0, 0, 3.20003e+24f
path.lineTo(SkBits2Float(0xdf218a28), SkBits2Float(0x00000000));  // -1.16402e+19f, 0
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x6a4b7bc4));  // 0, 6.14991e+25f
path.close();
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.conicTo(SkBits2Float(0x6642c40c), SkBits2Float(0x00000000), SkBits2Float(0x65906630), SkBits2Float(0x6a25a070), SkBits2Float(0x3edcd74d));  // 2.29939e+23f, 0, 8.52382e+22f, 5.00576e+25f, 0.43133f
path.conicTo(SkBits2Float(0x68295afa), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x00000000), SkBits2Float(0x4277a57b));  // 3.19903e+24f, 0, 0, 0, 61.9116f
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 4, filename);
}


static void fuzz763_26(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc003210a));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68372d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0));  // 3.46012e+24f, -2.71613e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x69555b2d));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.43289e-15f, 1.61207e+25f
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0682955));  // 3.35016e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x8cef552a), SkBits2Float(0x295b2d2a), SkBits2Float(0x68210368), SkBits2Float(0x7bc05508));  // 5.76397e-19f, -3.6875e-31f, 4.86669e-14f, 3.04146e+24f, 1.99729e+36f
path.lineTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0682955));  // 3.35016e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68315b2d), SkBits2Float(0xf0682955));  // 3.35016e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x68385b2d), SkBits2Float(0x555bf055), SkBits2Float(0x2a1f2a8c), SkBits2Float(0x03212121), SkBits2Float(0x5a4b7bc0));  // 3.48239e+24f, 1.51141e+13f, 1.41368e-13f, 4.73517e-37f, 1.43189e+16f
path.conicTo(SkBits2Float(0xc08c2aed), SkBits2Float(0x211f2108), SkBits2Float(0x6a4b7b03), SkBits2Float(0x6829ed27), SkBits2Float(0x2d555b2d));  // -4.38024f, 5.3915e-19f, 6.14982e+25f, 3.20982e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68355b2d), SkBits2Float(0xf0685527));  // 3.42572e+24f, -2.87614e+29f
path.conicTo(SkBits2Float(0x2a8c555b), SkBits2Float(0x6e2a1f72), SkBits2Float(0x0321082a), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x4793ed7a));  // 2.49282e-13f, 1.31626e+28f, 4.7323e-37f, 6.14991e+25f, 75739
path.lineTo(SkBits2Float(0x68355b2d), SkBits2Float(0xf0685527));  // 3.42572e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68355b2d), SkBits2Float(0xf0685527));  // 3.42572e+24f, -2.87614e+29f
path.quadTo(SkBits2Float(0x2128282a), SkBits2Float(0x3a8a3adf), SkBits2Float(0x8a284f1a), SkBits2Float(0x2c213ab3));  // 5.69738e-19f, 0.00105461f, -8.10378e-33f, 2.29121e-12f
path.lineTo(SkBits2Float(0x68355b2d), SkBits2Float(0xf0685527));  // 3.42572e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x68355b2d), SkBits2Float(0xf0685527));  // 3.42572e+24f, -2.87614e+29f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x3a2a812a), SkBits2Float(0x2127ed29));  // 2.25206e-21f, 300.343f, 0.000650423f, 5.68957e-19f
path.conicTo(SkBits2Float(0x03210831), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x6829ed27), SkBits2Float(0x55555b2d), SkBits2Float(0x1e2a3a2a));  // 4.73231e-37f, 6.14991e+25f, 3.20982e+24f, 1.46617e+13f, 9.01175e-21f
path.conicTo(SkBits2Float(0x27202140), SkBits2Float(0x3a3b2769), SkBits2Float(0xc4371f20), SkBits2Float(0xecc52a22), SkBits2Float(0x21512727));  // 2.22225e-15f, 0.000713936f, -732.486f, -1.90686e+27f, 7.08638e-19f
path.lineTo(SkBits2Float(0x68355b2d), SkBits2Float(0xf0685527));  // 3.42572e+24f, -2.87614e+29f
path.close();
path.moveTo(SkBits2Float(0x6829523a), SkBits2Float(0x2d555b2d));  // 3.19839e+24f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x5b2d5529));  // 4.03114e+24f, 4.87888e+16f
path.moveTo(SkBits2Float(0x1f2a322a), SkBits2Float(0xc0032108));  // 3.60404e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68572d55), SkBits2Float(0xf05bd24b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a292a), SkBits2Float(0x0321082a), SkBits2Float(0xed4b7bc0));  // 4.06458e+24f, -2.72126e+29f, -1.64207e-31f, 5.76527e-19f, 4.7323e-37f, -3.93594e+27f
path.conicTo(SkBits2Float(0x212a8c6a), SkBits2Float(0x0329081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2829ed84), SkBits2Float(0x2d555b2d));  // 5.77841e-19f, 4.9674e-37f, 6.14991e+25f, 9.43289e-15f, 1.21279e-11f
path.moveTo(SkBits2Float(0x68385b2d), SkBits2Float(0xf0682955));  // 3.48239e+24f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x8cef552a), SkBits2Float(0x295b2d2a), SkBits2Float(0x68210368), SkBits2Float(0x7bc05508));  // 5.76397e-19f, -3.6875e-31f, 4.86669e-14f, 3.04146e+24f, 1.99729e+36f
path.lineTo(SkBits2Float(0x68385b2d), SkBits2Float(0xf0682955));  // 3.48239e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68385b2d), SkBits2Float(0xf0682955));  // 3.48239e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x555b1b29), SkBits2Float(0x6c212a8c));  // 1.50569e+13f, 7.79352e+26f
path.conicTo(SkBits2Float(0x084b0321), SkBits2Float(0x6ac07b2a), SkBits2Float(0x395b2d7a), SkBits2Float(0x8c5bf055), SkBits2Float(0x1f212a3a));  // 6.10918e-34f, 1.16348e+26f, 0.000209024f, -1.69435e-31f, 3.4128e-20f
path.conicTo(SkBits2Float(0x290321d9), SkBits2Float(0x555b2d68), SkBits2Float(0x2a8c558c), SkBits2Float(0x2a212a1f), SkBits2Float(0x7bc00321));  // 2.91172e-14f, 1.50618e+13f, 2.49284e-13f, 1.43143e-13f, 1.99397e+36f
path.lineTo(SkBits2Float(0x68385b2d), SkBits2Float(0xf0682955));  // 3.48239e+24f, -2.87402e+29f
path.close();
path.moveTo(SkBits2Float(0x68385b2d), SkBits2Float(0xf0682955));  // 3.48239e+24f, -2.87402e+29f
path.lineTo(SkBits2Float(0x8c2aed7a), SkBits2Float(0x1f2128c0));  // -1.31678e-31f, 3.41268e-20f
path.lineTo(SkBits2Float(0x68385b2d), SkBits2Float(0xf0682955));  // 3.48239e+24f, -2.87402e+29f
path.close();

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_28(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x68556829), SkBits2Float(0x555b2d29));  // 4.03114e+24f, 1.50617e+13f
path.moveTo(SkBits2Float(0x1f2a312a), SkBits2Float(0xc0032108));  // 3.60396e-20f, -2.04889f
path.cubicTo(SkBits2Float(0x68302d55), SkBits2Float(0xf05b684b), SkBits2Float(0x8c55272d), SkBits2Float(0x212a1f2a), SkBits2Float(0x0321082a), SkBits2Float(0x6aa37bc0));  // 3.32789e+24f, -2.71613e+29f, -1.64207e-31f, 5.76395e-19f, 4.7323e-37f, 9.88197e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0), SkBits2Float(0x2d28ed84), SkBits2Float(0x5b2d2955));  // 5.77848e-19f, 4.7323e-37f, 6.14991e+25f, 9.60243e-12f, 4.87406e+16f
path.moveTo(SkBits2Float(0x6c395b2d), SkBits2Float(0xf0682955));  // 8.96327e+26f, -2.87402e+29f
path.conicTo(SkBits2Float(0x212a1f5b), SkBits2Float(0x2aef8c55), SkBits2Float(0x68295b2d), SkBits2Float(0x21086855), SkBits2Float(0x4b7bc003));  // 5.76397e-19f, 4.25523e-13f, 3.19905e+24f, 4.62167e-19f, 1.64987e+07f
path.lineTo(SkBits2Float(0x5b2c6829), SkBits2Float(0x212a8c55));  // 4.85282e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.lineTo(SkBits2Float(0x8a283a28), SkBits2Float(0x284f1a3a));  // -8.09984e-33f, 1.14965e-14f
path.quadTo(SkBits2Float(0x1d2a2928), SkBits2Float(0x43962be6), SkBits2Float(0x272a812a), SkBits2Float(0x3a2a5529));  // 2.25206e-21f, 300.343f, 2.36623e-15f, 0.000649768f
path.lineTo(SkBits2Float(0x213b1e2a), SkBits2Float(0x27292720));  // 6.3398e-19f, 2.34747e-15f
path.conicTo(SkBits2Float(0x381f203a), SkBits2Float(0x2ac422c5), SkBits2Float(0xc25d27ec), SkBits2Float(0x3a705921), SkBits2Float(0x2a105152));  // 3.79386e-05f, 3.48407e-13f, -55.289f, 0.000916855f, 1.2818e-13f
path.quadTo(SkBits2Float(0x633ad912), SkBits2Float(0x29c80927), SkBits2Float(0x272927b0), SkBits2Float(0x683a5b2d));  // 3.44674e+21f, 8.88337e-14f, 2.3475e-15f, 3.52017e+24f
path.lineTo(SkBits2Float(0x295b2d68), SkBits2Float(0x29685568));  // 4.86672e-14f, 5.15884e-14f
path.conicTo(SkBits2Float(0xaa8c555b), SkBits2Float(0x081f2a21), SkBits2Float(0x5b2d0321), SkBits2Float(0x68556829), SkBits2Float(0x2a552d29));  // -2.49282e-13f, 4.78968e-34f, 4.86986e+16f, 4.03114e+24f, 1.89339e-13f
path.cubicTo(SkBits2Float(0x21295b2d), SkBits2Float(0x2a688c5b), SkBits2Float(0x68295b2d), SkBits2Float(0x2d296855), SkBits2Float(0x8c08555b), SkBits2Float(0x2a2a29ca));  // 5.73801e-19f, 2.06544e-13f, 3.19905e+24f, 9.6297e-12f, -1.05027e-31f, 1.51135e-13f
path.quadTo(SkBits2Float(0x68295b21), SkBits2Float(0x2d296855), SkBits2Float(0x2a8c555b), SkBits2Float(0x081f2a21));  // 3.19904e+24f, 9.6297e-12f, 2.49282e-13f, 4.78968e-34f
path.lineTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.close();
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x6a4b7bc0), SkBits2Float(0x5b2d6829), SkBits2Float(0x1f212a55), SkBits2Float(0x8ced7aba), SkBits2Float(0x3f2a212a));  // 6.14991e+25f, 4.88097e+16f, 3.41281e-20f, -3.65895e-31f, 0.664569f
path.lineTo(SkBits2Float(0x5b2d212d), SkBits2Float(0x2d556829));  // 4.87316e+16f, 1.21308e-11f
path.moveTo(SkBits2Float(0x68552968), SkBits2Float(0x5568295b));  // 4.02651e+24f, 1.5954e+13f
path.moveTo(SkBits2Float(0x5b2d2968), SkBits2Float(0x212a8c55));  // 4.87407e+16f, 5.7784e-19f
path.moveTo(SkBits2Float(0x0321081f), SkBits2Float(0x6a4b7bc0));  // 4.7323e-37f, 6.14991e+25f
path.conicTo(SkBits2Float(0x212a8ced), SkBits2Float(0x0321081f), SkBits2Float(0x6a3a7bc0), SkBits2Float(0x2147ed7a), SkBits2Float(0x28282a3a));  // 5.77848e-19f, 4.7323e-37f, 5.63611e+25f, 6.77381e-19f, 9.33503e-15f

    SkPath path2(path);
    testPathOpSkipAssert(reporter, path1, path2, (SkPathOp) 0, filename);
}

static void fuzz763_27(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 0);

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.quadTo(SkBits2Float(0x30309ab8), SkBits2Float(0x305b3030), SkBits2Float(0x00f53030), SkBits2Float(0x3a3a0000));  // 6.42483e-10f, 7.97402e-10f, 2.2517e-38f, 0.000709534f
path.quadTo(SkBits2Float(0xb8b8d5b8), SkBits2Float(0x0b0b0b03), SkBits2Float(0x0b0b0b0b), SkBits2Float(0x3a3a0b0b));  // -8.81361e-05f, 2.67787e-32f, 2.67787e-32f, 0.000709698f
path.quadTo(SkBits2Float(0xb8b8b8b8), SkBits2Float(0x0b1203b8), SkBits2Float(0x0b0b0b0b), SkBits2Float(0x3a3a2110));  // -8.80821e-05f, 2.81214e-32f, 2.67787e-32f, 0.000710026f

    SkPath path2(path);
    testPathOpFailSkipAssert(reporter, path1, path2, (SkPathOp) 4, filename);
}

static void fuzz763_29(skiatest::Reporter* reporter, const char* filename) {
    SkPath path;
    path.setFillType((SkPath::FillType) 1);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x743e0000));  // 0, 6.02134e+31f
path.cubicTo(SkBits2Float(0x74083cf1), SkBits2Float(0x74536e73), SkBits2Float(0x742ac4e4), SkBits2Float(0x7415f5be), SkBits2Float(0x7433ee3c), SkBits2Float(0x7405a69a));  // 4.31756e+31f, 6.70053e+31f, 5.41189e+31f, 4.75242e+31f, 5.70223e+31f, 4.23556e+31f
path.quadTo(SkBits2Float(0x74360ca0), SkBits2Float(0x7401e10c), SkBits2Float(0x7436a382), SkBits2Float(0x7401cc18));  // 5.76937e+31f, 4.11603e+31f, 5.78805e+31f, 4.11344e+31f
path.cubicTo(SkBits2Float(0x74374a91), SkBits2Float(0x7401ef19), SkBits2Float(0x74375c84), SkBits2Float(0x7404d9b9), SkBits2Float(0x7437868f), SkBits2Float(0x740bae8a));  // 5.80873e+31f, 4.11777e+31f, 5.81095e+31f, 4.2102e+31f, 5.81616e+31f, 4.42669e+31f
path.cubicTo(SkBits2Float(0x7437d6c1), SkBits2Float(0x7418b629), SkBits2Float(0x74387e9b), SkBits2Float(0x7433fbc5), SkBits2Float(0x743e2ff7), SkBits2Float(0x74655fa2));  // 5.82609e+31f, 4.83962e+31f, 5.84687e+31f, 5.7039e+31f, 6.02728e+31f, 7.26914e+31f
path.cubicTo(SkBits2Float(0x741ada75), SkBits2Float(0x74745717), SkBits2Float(0x73c106b4), SkBits2Float(0x74744e64), SkBits2Float(0x00000000), SkBits2Float(0x74744006));  // 4.9075e+31f, 7.74345e+31f, 3.05862e+31f, 7.74237e+31f, 0, 7.74059e+31f
path.cubicTo(SkBits2Float(0x00000000), SkBits2Float(0x74746c7c), SkBits2Float(0x74244dce), SkBits2Float(0x7474733e), SkBits2Float(0x74400000), SkBits2Float(0x74747445));  // 0, 7.7461e+31f, 5.207e+31f, 7.74693e+31f, 6.08472e+31f, 7.74706e+31f
path.cubicTo(SkBits2Float(0x743f5854), SkBits2Float(0x746f3659), SkBits2Float(0x743ebe05), SkBits2Float(0x746a3017), SkBits2Float(0x743e2ff7), SkBits2Float(0x74655fa2));  // 6.06397e+31f, 7.58094e+31f, 6.04486e+31f, 7.42171e+31f, 6.02728e+31f, 7.26914e+31f
path.cubicTo(SkBits2Float(0x7447a582), SkBits2Float(0x74615dee), SkBits2Float(0x744f74f6), SkBits2Float(0x745c4903), SkBits2Float(0x7455e7e6), SkBits2Float(0x7455d751));  // 6.32705e+31f, 7.14216e+31f, 6.57457e+31f, 6.98112e+31f, 6.77895e+31f, 6.77689e+31f
path.cubicTo(SkBits2Float(0x74747474), SkBits2Float(0x743750a4), SkBits2Float(0x74747474), SkBits2Float(0x73f46f0d), SkBits2Float(0x74747474), SkBits2Float(0x00000000));  // 7.74708e+31f, 5.80948e+31f, 7.74708e+31f, 3.87321e+31f, 7.74708e+31f, 0
path.lineTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.close();

    SkPath path1(path);
    path.reset();
    path.setFillType((SkPath::FillType) 0);
path.moveTo(SkBits2Float(0x00000000), SkBits2Float(0x00000000));  // 0, 0
path.lineTo(SkBits2Float(0xf0682955), SkBits2Float(0x211f5b2d));  // -2.87402e+29f, 5.3992e-19f
path.moveTo(SkBits2Float(0x2d2aff2d), SkBits2Float(0x74747474));  // 9.72004e-12f, 7.74708e+31f
path.cubicTo(SkBits2Float(0x7474748e), SkBits2Float(0x74747490), SkBits2Float(0x8c722174), SkBits2Float(0x181f0080), SkBits2Float(0x74c0e520), SkBits2Float(0x747d7463));  // 7.7471e+31f, 7.7471e+31f, -1.86531e-31f, 2.05505e-24f, 1.22262e+32f, 8.0323e+31f
path.cubicTo(SkBits2Float(0x7b005e4b), SkBits2Float(0xdf3a6a3a), SkBits2Float(0x2a3a2848), SkBits2Float(0x2d2d7821), SkBits2Float(0x8c55212d), SkBits2Float(0x2d2d2d24));  // 6.66526e+35f, -1.34326e+19f, 1.65341e-13f, 9.86059e-12f, -1.64189e-31f, 9.84393e-12f
path.conicTo(SkBits2Float(0xde28804c), SkBits2Float(0x28e03721), SkBits2Float(0x3329df28), SkBits2Float(0x2d291515), SkBits2Float(0x0568295b));  // -3.03545e+18f, 2.48929e-14f, 3.95513e-08f, 9.61122e-12f, 1.09162e-35f
path.conicTo(SkBits2Float(0x556a2d21), SkBits2Float(0x21088c2a), SkBits2Float(0x3a333303), SkBits2Float(0x5b293a8a), SkBits2Float(0x6855683b));  // 1.60925e+13f, 4.62641e-19f, 0.000683591f, 4.76336e+16f, 4.03115e+24f

    SkPath path2(path);
    testPathOpFail(reporter, path1, path2, (SkPathOp) 0, filename);
}

static struct TestDesc failTests[] = {
    TEST(fuzz763_29),
    TEST(fuzz763_28),
    TEST(fuzz763_27),
    TEST(fuzz763_26),
    TEST(fuzz763_25),
    TEST(fuzz763_24),
    TEST(fuzz763_23),
    TEST(fuzz763_22),
    TEST(fuzz763_21),
    TEST(fuzz763_20),
    TEST(fuzz763_19),
    TEST(fuzz763_18),
    TEST(fuzz763_17),
    TEST(fuzz763_16),
    TEST(fuzz763_15),
    TEST(fuzz763_14),
    TEST(fuzz763_13),
    TEST(fuzz763_12),
    TEST(fuzz763_11),
    TEST(fuzz763_10),
    TEST(kfuzz2),
    TEST(fuzz763_7),
    TEST(fuzz763_6),
    TEST(fuzz763_2c),
    TEST(fuzz763_2b),
    TEST(fuzz763_2a),
    TEST(fuzz763_5a),
    TEST(fuzz763_3a),
    TEST(fuzz763_1a),
    TEST(fuzz763_1b),
    TEST(fuzz763_1c),
    TEST(fuzz763_2),
    TEST(fuzz763_5),
    TEST(fuzz763_3),
    TEST(fuzz763_4),
    TEST(fuzz763_9),
    TEST(fuzz1450_1),
    TEST(fuzz1450_0),
    TEST(bug597926_0),
    TEST(fuzz535151),
    TEST(fuzz753_91),
    TEST(fuzz714),
    TEST(fuzz487a),
    TEST(fuzz433),
    TEST(fuzz1),
    TEST(fuzz487b),
    TEST(fuzz433b),
    TEST(bufferOverflow),
};

static const size_t failTestCount = SK_ARRAY_COUNT(failTests);

DEF_TEST(PathOpsFailOp, reporter) {
#if DEBUG_SHOW_TEST_NAME
    strncpy(DEBUG_FILENAME_STRING, "", DEBUG_FILENAME_STRING_LENGTH);
#endif
    RunTestSet(reporter, failTests, failTestCount, nullptr, nullptr, nullptr, false);
}

static struct TestDesc repTests[] = {
    TEST(loops44i),
    TEST(loops45i),
    TEST(loops46i),
};

DEF_TEST(PathOpsRepOp, reporter) {
  for (int index = 0; index < 2; ++index)
    RunTestSet(reporter, repTests, SK_ARRAY_COUNT(repTests), nullptr, nullptr, nullptr, false);
}
