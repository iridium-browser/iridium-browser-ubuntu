/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkPathOpsDebug_DEFINED
#define SkPathOpsDebug_DEFINED

#include "SkPathOps.h"
#include "SkTypes.h"
#include <stdio.h>

#ifdef SK_RELEASE
#define FORCE_RELEASE 1
#else
#define FORCE_RELEASE 1  // set force release to 1 for multiple thread -- no debugging
#endif

#define ONE_OFF_DEBUG 0
#define ONE_OFF_DEBUG_MATHEMATICA 0

#if defined(SK_BUILD_FOR_WIN) || defined(SK_BUILD_FOR_ANDROID)
    #define SK_RAND(seed) rand()
#else
    #define SK_RAND(seed) rand_r(&seed)
#endif
#ifdef SK_BUILD_FOR_WIN
    #define SK_SNPRINTF _snprintf
#else
    #define SK_SNPRINTF snprintf
#endif

#define WIND_AS_STRING(x) char x##Str[12]; \
        if (!SkPathOpsDebug::ValidWind(x)) strcpy(x##Str, "?"); \
        else SK_SNPRINTF(x##Str, sizeof(x##Str), "%d", x)

#if FORCE_RELEASE

#define DEBUG_ACTIVE_OP 0
#define DEBUG_ACTIVE_SPANS 0
#define DEBUG_ADD_INTERSECTING_TS 0
#define DEBUG_ADD_T 0
#define DEBUG_ANGLE 0
#define DEBUG_ASSEMBLE 0
#define DEBUG_CUBIC_BINARY_SEARCH 0
#define DEBUG_FLOW 0
#define DEBUG_LIMIT_WIND_SUM 0
#define DEBUG_MARK_DONE 0
#define DEBUG_PATH_CONSTRUCTION 0
#define DEBUG_PERP 0
#define DEBUG_SHOW_TEST_NAME 0
#define DEBUG_SORT 0
#define DEBUG_SWAP_TOP 0
#define DEBUG_T_SECT 0
#define DEBUG_T_SECT_DUMP 0
#define DEBUG_VALIDATE 0
#define DEBUG_WINDING 0
#define DEBUG_WINDING_AT_T 0

#else

#define DEBUG_ACTIVE_OP 1
#define DEBUG_ACTIVE_SPANS 1
#define DEBUG_ADD_INTERSECTING_TS 1
#define DEBUG_ADD_T 1
#define DEBUG_ANGLE 1
#define DEBUG_ASSEMBLE 1
#define DEBUG_CUBIC_BINARY_SEARCH 0
#define DEBUG_FLOW 1
#define DEBUG_LIMIT_WIND_SUM 5
#define DEBUG_MARK_DONE 1
#define DEBUG_PATH_CONSTRUCTION 1
#define DEBUG_PERP 0
#define DEBUG_SHOW_TEST_NAME 1
#define DEBUG_SORT 1
#define DEBUG_SWAP_TOP 1
#define DEBUG_T_SECT 1
#define DEBUG_T_SECT_DUMP 02
#define DEBUG_VALIDATE 1
#define DEBUG_WINDING 1
#define DEBUG_WINDING_AT_T 1

#endif

#ifdef SK_RELEASE
    #define PATH_OPS_DEBUG_RELEASE(a, b) b
    #define PATH_OPS_DEBUG_CODE(...)
    #define PATH_OPS_DEBUG_PARAMS(...)
#else
    #define PATH_OPS_DEBUG_RELEASE(a, b) a
    #define PATH_OPS_DEBUG_CODE(...) __VA_ARGS__
    #define PATH_OPS_DEBUG_PARAMS(...) , __VA_ARGS__
#endif

#if DEBUG_T_SECT == 0
    #define PATH_OPS_DEBUG_T_SECT_RELEASE(a, b) b
    #define PATH_OPS_DEBUG_T_SECT_PARAMS(...)
    #define PATH_OPS_DEBUG_T_SECT_CODE(...)
#else
    #define PATH_OPS_DEBUG_T_SECT_RELEASE(a, b) a
    #define PATH_OPS_DEBUG_T_SECT_PARAMS(...) , __VA_ARGS__
    #define PATH_OPS_DEBUG_T_SECT_CODE(...) __VA_ARGS__
#endif

#if DEBUG_T_SECT_DUMP > 1
    extern int gDumpTSectNum;
#endif

#define CUBIC_DEBUG_STR "{{{%1.9g,%1.9g}, {%1.9g,%1.9g}, {%1.9g,%1.9g}, {%1.9g,%1.9g}}}"
#define QUAD_DEBUG_STR  "{{{%1.9g,%1.9g}, {%1.9g,%1.9g}, {%1.9g,%1.9g}}}"
#define LINE_DEBUG_STR  "{{{%1.9g,%1.9g}, {%1.9g,%1.9g}}}"
#define PT_DEBUG_STR "{{%1.9g,%1.9g}}"

#define T_DEBUG_STR(t, n) #t "[" #n "]=%1.9g"
#define TX_DEBUG_STR(t) #t "[%d]=%1.9g"
#define CUBIC_DEBUG_DATA(c) c[0].fX, c[0].fY, c[1].fX, c[1].fY, c[2].fX, c[2].fY, c[3].fX, c[3].fY
#define QUAD_DEBUG_DATA(q)  q[0].fX, q[0].fY, q[1].fX, q[1].fY, q[2].fX, q[2].fY
#define LINE_DEBUG_DATA(l)  l[0].fX, l[0].fY, l[1].fX, l[1].fY
#define PT_DEBUG_DATA(i, n) i.pt(n).asSkPoint().fX, i.pt(n).asSkPoint().fY

#ifndef DEBUG_TEST
#define DEBUG_TEST 0
#endif

#if DEBUG_SHOW_TEST_NAME
#include "SkTLS.h"
#endif

#include "SkTDArray.h"

class SkPathOpsDebug {
public:
    static const char* kLVerbStr[];

#if defined(SK_DEBUG) || !FORCE_RELEASE
    static int gContourID;
    static int gSegmentID;
#endif

#if DEBUG_SORT || DEBUG_SWAP_TOP
    static int gSortCountDefault;
    static int gSortCount;
#endif

#if DEBUG_ACTIVE_OP
    static const char* kPathOpStr[];
#endif

    static void MathematicaIze(char* str, size_t bufferSize);
    static bool ValidWind(int winding);
    static void WindingPrintf(int winding);

#if DEBUG_SHOW_TEST_NAME
    static void* CreateNameStr();
    static void DeleteNameStr(void* v);
#define DEBUG_FILENAME_STRING_LENGTH 64
#define DEBUG_FILENAME_STRING (reinterpret_cast<char* >(SkTLS::Get(SkPathOpsDebug::CreateNameStr, \
        SkPathOpsDebug::DeleteNameStr)))
    static void BumpTestName(char* );
#endif
    static void ShowOnePath(const SkPath& path, const char* name, bool includeDeclaration);
    static void ShowPath(const SkPath& one, const SkPath& two, SkPathOp op, const char* name);

    static bool ChaseContains(const SkTDArray<class SkOpSpanBase*>& , const class SkOpSpanBase* );

    static const struct SkOpAngle* DebugAngleAngle(const struct SkOpAngle*, int id);
    static class SkOpContour* DebugAngleContour(struct SkOpAngle*, int id);
    static const class SkOpPtT* DebugAnglePtT(const struct SkOpAngle*, int id);
    static const class SkOpSegment* DebugAngleSegment(const struct SkOpAngle*, int id);
    static const class SkOpSpanBase* DebugAngleSpan(const struct SkOpAngle*, int id);

    static const struct SkOpAngle* DebugContourAngle(class SkOpContour*, int id);
    static class SkOpContour* DebugContourContour(class SkOpContour*, int id);
    static const class SkOpPtT* DebugContourPtT(class SkOpContour*, int id);
    static const class SkOpSegment* DebugContourSegment(class SkOpContour*, int id);
    static const class SkOpSpanBase* DebugContourSpan(class SkOpContour*, int id);

    static const struct SkOpAngle* DebugPtTAngle(const class SkOpPtT*, int id);
    static class SkOpContour* DebugPtTContour(class SkOpPtT*, int id);
    static const class SkOpPtT* DebugPtTPtT(const class SkOpPtT*, int id);
    static const class SkOpSegment* DebugPtTSegment(const class SkOpPtT*, int id);
    static const class SkOpSpanBase* DebugPtTSpan(const class SkOpPtT*, int id);

    static const struct SkOpAngle* DebugSegmentAngle(const class SkOpSegment*, int id);
    static class SkOpContour* DebugSegmentContour(class SkOpSegment*, int id);
    static const class SkOpPtT* DebugSegmentPtT(const class SkOpSegment*, int id);
    static const class SkOpSegment* DebugSegmentSegment(const class SkOpSegment*, int id);
    static const class SkOpSpanBase* DebugSegmentSpan(const class SkOpSegment*, int id);

    static const struct SkOpAngle* DebugSpanAngle(const class SkOpSpanBase*, int id);
    static class SkOpContour* DebugSpanContour(class SkOpSpanBase*, int id);
    static const class SkOpPtT* DebugSpanPtT(const class SkOpSpanBase*, int id);
    static const class SkOpSegment* DebugSpanSegment(const class SkOpSpanBase*, int id);
    static const class SkOpSpanBase* DebugSpanSpan(const class SkOpSpanBase*, int id);

    static void DumpContours(SkTDArray<class SkOpContour* >* contours);
    static void DumpContoursAll(SkTDArray<class SkOpContour* >* contours);
    static void DumpContoursAngles(const SkTDArray<class SkOpContour* >* contours);
    static void DumpContoursPt(const SkTDArray<class SkOpContour* >* contours, int id);
    static void DumpContoursPts(const SkTDArray<class SkOpContour* >* contours);
    static void DumpContoursSegment(const SkTDArray<class SkOpContour* >* contours, int id);
    static void DumpContoursSpan(const SkTDArray<class SkOpContour* >* contours, int id);
    static void DumpContoursSpans(const SkTDArray<class SkOpContour* >* contours);
};

// shorthand for calling from debugger
template<typename TCurve> class SkTSect;
template<typename TCurve> class SkTSpan;

struct SkDQuad;
struct SkDCubic;

const SkTSpan<SkDCubic>* DebugSpan(const SkTSect<SkDCubic>* , int id);
const SkTSpan<SkDQuad>* DebugSpan(const SkTSect<SkDQuad>* , int id);
const SkTSpan<SkDCubic>* DebugT(const SkTSect<SkDCubic>* , double t);
const SkTSpan<SkDQuad>* DebugT(const SkTSect<SkDQuad>* , double t);

const SkTSpan<SkDCubic>* DebugSpan(const SkTSpan<SkDCubic>* , int id);
const SkTSpan<SkDQuad>* DebugSpan(const SkTSpan<SkDQuad>* , int id);
const SkTSpan<SkDCubic>* DebugT(const SkTSpan<SkDCubic>* , double t);
const SkTSpan<SkDQuad>* DebugT(const SkTSpan<SkDQuad>* , double t);

void Dump(const SkTSect<SkDCubic>* );
void Dump(const SkTSect<SkDQuad>* );
void Dump(const SkTSpan<SkDCubic>* , const SkTSect<SkDCubic>* = NULL);
void Dump(const SkTSpan<SkDQuad>* , const SkTSect<SkDQuad>* = NULL);
void DumpBoth(SkTSect<SkDCubic>* sect1, SkTSect<SkDCubic>* sect2);
void DumpBoth(SkTSect<SkDQuad>* sect1, SkTSect<SkDQuad>* sect2);
void DumpCoin(SkTSect<SkDCubic>* sect1);
void DumpCoin(SkTSect<SkDQuad>* sect1);
void DumpCoinCurves(SkTSect<SkDCubic>* sect1);
void DumpCoinCurves(SkTSect<SkDQuad>* sect1);
void DumpCurves(const SkTSpan<SkDCubic>* );
void DumpCurves(const SkTSpan<SkDQuad>* );

// generates tools/path_sorter.htm and path_visualizer.htm compatible data
void DumpQ(const SkDQuad& quad1, const SkDQuad& quad2, int testNo);
void DumpT(const SkDQuad& quad, double t);

const struct SkOpAngle* DebugAngle(const SkTDArray<class SkOpContour* >* contours, int id);
class SkOpContour* DebugContour(const SkTDArray<class SkOpContour* >* contours, int id);
const class SkOpPtT* DebugPtT(const SkTDArray<class SkOpContour* >* contours, int id);
const class SkOpSegment* DebugSegment(const SkTDArray<class SkOpContour* >* contours, int id);
const class SkOpSpanBase* DebugSpan(const SkTDArray<class SkOpContour* >* contours, int id);

void Dump(const SkTDArray<class SkOpContour* >* contours);
void DumpAll(SkTDArray<class SkOpContour* >* contours);
void DumpAngles(const SkTDArray<class SkOpContour* >* contours);
void DumpCoin(const SkTDArray<class SkOpContour* >* contours);
void DumpPt(const SkTDArray<class SkOpContour* >* contours, int segmentID);
void DumpPts(const SkTDArray<class SkOpContour* >* contours);
void DumpSegment(const SkTDArray<class SkOpContour* >* contours, int segmentID);
void DumpSpan(const SkTDArray<class SkOpContour* >* contours, int spanID);
void DumpSpans(const SkTDArray<class SkOpContour* >* contours);
#endif
