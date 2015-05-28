/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkOpSegment_DEFINE
#define SkOpSegment_DEFINE

#include "SkOpAngle.h"
#include "SkOpSpan.h"
#include "SkOpTAllocator.h"
#include "SkPathOpsBounds.h"
#include "SkPathOpsCurve.h"

class SkOpCoincidence;
class SkOpContour;
class SkPathWriter;

class SkOpSegment {
public:
    enum AllowAlias {
        kAllowAlias,
        kNoAlias
    };

    bool operator<(const SkOpSegment& rh) const {
        return fBounds.fTop < rh.fBounds.fTop;
    }

    SkOpAngle* activeAngle(SkOpSpanBase* start, SkOpSpanBase** startPtr, SkOpSpanBase** endPtr,
                            bool* done, bool* sortable);
    SkOpAngle* activeAngleInner(SkOpSpanBase* start, SkOpSpanBase** startPtr,
                                       SkOpSpanBase** endPtr, bool* done, bool* sortable);
    SkOpAngle* activeAngleOther(SkOpSpanBase* start, SkOpSpanBase** startPtr,
                                       SkOpSpanBase** endPtr, bool* done, bool* sortable);
    bool activeOp(SkOpSpanBase* start, SkOpSpanBase* end, int xorMiMask, int xorSuMask,
                  SkPathOp op);
    bool activeOp(int xorMiMask, int xorSuMask, SkOpSpanBase* start, SkOpSpanBase* end, SkPathOp op,
                  int* sumMiWinding, int* sumSuWinding);

    SkPoint activeLeftTop(SkOpSpanBase** firstT);

    bool activeWinding(SkOpSpanBase* start, SkOpSpanBase* end);
    bool activeWinding(SkOpSpanBase* start, SkOpSpanBase* end, int* sumWinding);

    void addCubic(SkPoint pts[4], SkOpContour* parent) {
        init(pts, parent, SkPath::kCubic_Verb);
        fBounds.setCubicBounds(pts);
    }

    void addCurveTo(const SkOpSpanBase* start, const SkOpSpanBase* end, SkPathWriter* path,
                    bool active) const;

    SkOpAngle* addEndSpan(SkChunkAlloc* allocator) {
        SkOpAngle* angle = SkOpTAllocator<SkOpAngle>::Allocate(allocator);
        angle->set(&fTail, fTail.prev());
        fTail.setFromAngle(angle);
        return angle;
    }

    void addLine(SkPoint pts[2], SkOpContour* parent) {
        init(pts, parent, SkPath::kLine_Verb);
        fBounds.set(pts, 2);
    }

    SkOpPtT* addMissing(double t, SkOpSegment* opp, SkChunkAlloc* );
    SkOpAngle* addSingletonAngleDown(SkOpSegment** otherPtr, SkOpAngle** , SkChunkAlloc* );
    SkOpAngle* addSingletonAngles(int step, SkChunkAlloc* );
    SkOpAngle* addSingletonAngleUp(SkOpSegment** otherPtr, SkOpAngle** , SkChunkAlloc* );

    SkOpAngle* addStartSpan(SkChunkAlloc* allocator) {
        SkOpAngle* angle = SkOpTAllocator<SkOpAngle>::Allocate(allocator);
        angle->set(&fHead, fHead.next());
        fHead.setToAngle(angle);
        return angle;
    }

    void addQuad(SkPoint pts[3], SkOpContour* parent) {
        init(pts, parent, SkPath::kQuad_Verb);
        fBounds.setQuadBounds(pts);
    }

    SkOpPtT* addT(double t, AllowAlias , SkChunkAlloc* );

    void align();
    static bool BetweenTs(const SkOpSpanBase* lesser, double testT, const SkOpSpanBase* greater);

    const SkPathOpsBounds& bounds() const {
        return fBounds;
    }

    void bumpCount() {
        ++fCount;
    }

    void calcAngles(SkChunkAlloc*);
    void checkAngleCoin(SkOpCoincidence* coincidences, SkChunkAlloc* allocator);
    void checkNearCoincidence(SkOpAngle* );
    bool clockwise(const SkOpSpanBase* start, const SkOpSpanBase* end, bool* swap) const;
    static void ComputeOneSum(const SkOpAngle* baseAngle, SkOpAngle* nextAngle,
                              SkOpAngle::IncludeType );
    static void ComputeOneSumReverse(const SkOpAngle* baseAngle, SkOpAngle* nextAngle,
                                     SkOpAngle::IncludeType );
    int computeSum(SkOpSpanBase* start, SkOpSpanBase* end, SkOpAngle::IncludeType includeType);

    SkOpContour* contour() const {
        return fContour;
    }

    int count() const {
        return fCount;
    }

    SkOpSpan* crossedSpanY(const SkPoint& basePt, double mid, bool opp, bool current,
                            SkScalar* bestY, double* hitT, bool* hitSomething, bool* vertical);

    void debugAddAngle(double startT, double endT, SkChunkAlloc*);
    const SkOpAngle* debugAngle(int id) const;
    SkOpContour* debugContour(int id);

    int debugID() const {
        return PATH_OPS_DEBUG_RELEASE(fID, -1);
    }

#if DEBUG_SWAP_TOP
    int debugInflections(const SkOpSpanBase* start, const SkOpSpanBase* end) const;
#endif

    SkOpAngle* debugLastAngle();
    const SkOpPtT* debugPtT(int id) const;
    void debugReset();
    const SkOpSegment* debugSegment(int id) const;

#if DEBUG_ACTIVE_SPANS
    void debugShowActiveSpans() const;
#endif
#if DEBUG_MARK_DONE
    void debugShowNewWinding(const char* fun, const SkOpSpan* span, int winding);
    void debugShowNewWinding(const char* fun, const SkOpSpan* span, int winding, int oppWinding);
#endif

    const SkOpSpanBase* debugSpan(int id) const;
    void debugValidate() const;
    void detach(const SkOpSpan* );
    double distSq(double t, SkOpAngle* opp);

    bool done() const {
        SkASSERT(fDoneCount <= fCount);
        return fDoneCount == fCount;
    }

    bool done(const SkOpAngle* angle) const {
        return angle->start()->starter(angle->end())->done();
    }

    SkDPoint dPtAtT(double mid) const {
        return (*CurveDPointAtT[SkPathOpsVerbToPoints(fVerb)])(fPts, mid);
    }

    SkDVector dSlopeAtT(double mid) const {
        return (*CurveDSlopeAtT[SkPathOpsVerbToPoints(fVerb)])(fPts, mid);
    }

    void dump() const;
    void dumpAll() const;
    void dumpAngles() const;
    void dumpCoin() const;
    void dumpPts() const;

    SkOpSegment* findNextOp(SkTDArray<SkOpSpanBase*>* chase, SkOpSpanBase** nextStart,
                             SkOpSpanBase** nextEnd, bool* unsortable, SkPathOp op,
                             int xorMiMask, int xorSuMask);
    SkOpSegment* findNextWinding(SkTDArray<SkOpSpanBase*>* chase, SkOpSpanBase** nextStart,
                                  SkOpSpanBase** nextEnd, bool* unsortable);
    SkOpSegment* findNextXor(SkOpSpanBase** nextStart, SkOpSpanBase** nextEnd, bool* unsortable);
    SkOpSegment* findTop(bool firstPass, SkOpSpanBase** startPtr, SkOpSpanBase** endPtr,
                          bool* unsortable, SkChunkAlloc* );
    SkOpGlobalState* globalState() const;

    const SkOpSpan* head() const {
        return &fHead;
    }

    SkOpSpan* head() {
        return &fHead;
    }

    void init(SkPoint pts[], SkOpContour* parent, SkPath::Verb verb);
    void initWinding(SkOpSpanBase* start, SkOpSpanBase* end,
                     SkOpAngle::IncludeType angleIncludeType);
    bool initWinding(SkOpSpanBase* start, SkOpSpanBase* end, double tHit, int winding,
            SkScalar hitDx, int oppWind, SkScalar hitOppDx);

    SkOpSpan* insert(SkOpSpan* prev, SkChunkAlloc* allocator) {
        SkOpSpan* result = SkOpTAllocator<SkOpSpan>::Allocate(allocator);
        SkOpSpanBase* next = prev->next();
        result->setPrev(prev);
        prev->setNext(result);
        SkDEBUGCODE(result->ptT()->fT = 0);
        result->setNext(next);
        if (next) {
            next->setPrev(result);
        }
        return result;
    }

    bool isClose(double t, const SkOpSegment* opp) const;

    bool isHorizontal() const {
        return fBounds.fTop == fBounds.fBottom;
    }

    SkOpSegment* isSimple(SkOpSpanBase** end, int* step) {
        return nextChase(end, step, NULL, NULL);
    }

    bool isVertical() const {
        return fBounds.fLeft == fBounds.fRight;
    }

    bool isVertical(SkOpSpanBase* start, SkOpSpanBase* end) const {
        return (*CurveIsVertical[SkPathOpsVerbToPoints(fVerb)])(fPts, start->t(), end->t());
    }

    bool isXor() const;

    const SkPoint& lastPt() const {
        return fPts[SkPathOpsVerbToPoints(fVerb)];
    }

    SkOpSpanBase* markAndChaseDone(SkOpSpanBase* start, SkOpSpanBase* end);
    bool markAndChaseWinding(SkOpSpanBase* start, SkOpSpanBase* end, int winding,
            SkOpSpanBase** lastPtr);
    bool markAndChaseWinding(SkOpSpanBase* start, SkOpSpanBase* end, int winding,
            int oppWinding, SkOpSpanBase** lastPtr);
    SkOpSpanBase* markAngle(int maxWinding, int sumWinding, const SkOpAngle* angle);
    SkOpSpanBase* markAngle(int maxWinding, int sumWinding, int oppMaxWinding, int oppSumWinding,
                         const SkOpAngle* angle);
    void markDone(SkOpSpan* );
    bool markWinding(SkOpSpan* , int winding);
    bool markWinding(SkOpSpan* , int winding, int oppWinding);
    bool match(const SkOpPtT* span, const SkOpSegment* parent, double t, const SkPoint& pt) const;
    void missingCoincidence(SkOpCoincidence* coincidences, SkChunkAlloc* allocator);
    bool monotonicInY(const SkOpSpanBase* start, const SkOpSpanBase* end) const;
    bool moveNearby();

    SkOpSegment* next() const {
        return fNext;
    }

    static bool NextCandidate(SkOpSpanBase* span, SkOpSpanBase** start, SkOpSpanBase** end);
    SkOpSegment* nextChase(SkOpSpanBase** , int* step, SkOpSpan** , SkOpSpanBase** last) const;
    bool operand() const;

    static int OppSign(const SkOpSpanBase* start, const SkOpSpanBase* end) {
        int result = start->t() < end->t() ? -start->upCast()->oppValue()
                : end->upCast()->oppValue();
        return result;
    }

    bool oppXor() const;

    const SkOpSegment* prev() const {
        return fPrev;
    }

    SkPoint ptAtT(double mid) const {
        return (*CurvePointAtT[SkPathOpsVerbToPoints(fVerb)])(fPts, mid);
    }

    const SkPoint* pts() const {
        return fPts;
    }

    bool ptsDisjoint(const SkOpPtT& span, const SkOpPtT& test) const {
        return ptsDisjoint(span.fT, span.fPt, test.fT, test.fPt);
    }

    bool ptsDisjoint(const SkOpPtT& span, double t, const SkPoint& pt) const {
        return ptsDisjoint(span.fT, span.fPt, t, pt);
    }

    bool ptsDisjoint(double t1, const SkPoint& pt1, double t2, const SkPoint& pt2) const;

    void resetVisited() {
        fVisited = false;
    }

    void setContour(SkOpContour* contour) {
        fContour = contour;
    }

    void setNext(SkOpSegment* next) {
        fNext = next;
    }

    void setPrev(SkOpSegment* prev) {
        fPrev = prev;
    }

    bool setVisited() {
        if (fVisited) {
            return false;
        }
        return (fVisited = true);
    }

    void setUpWinding(SkOpSpanBase* start, SkOpSpanBase* end, int* maxWinding, int* sumWinding) {
        int deltaSum = SpanSign(start, end);
        *maxWinding = *sumWinding;
        *sumWinding -= deltaSum;
    }

    void setUpWindings(SkOpSpanBase* start, SkOpSpanBase* end, int* sumMiWinding,
                       int* maxWinding, int* sumWinding);
    void setUpWindings(SkOpSpanBase* start, SkOpSpanBase* end, int* sumMiWinding, int* sumSuWinding,
                       int* maxWinding, int* sumWinding, int* oppMaxWinding, int* oppSumWinding);
    void sortAngles();

    static int SpanSign(const SkOpSpanBase* start, const SkOpSpanBase* end) {
        int result = start->t() < end->t() ? -start->upCast()->windValue()
                : end->upCast()->windValue();
        return result;
    }

    SkOpAngle* spanToAngle(SkOpSpanBase* start, SkOpSpanBase* end) {
        SkASSERT(start != end);
        return start->t() < end->t() ? start->upCast()->toAngle() : start->fromAngle();
    }

    bool subDivide(const SkOpSpanBase* start, const SkOpSpanBase* end, SkPoint edge[4]) const;
    bool subDivide(const SkOpSpanBase* start, const SkOpSpanBase* end, SkDCubic* result) const;
    void subDivideBounds(const SkOpSpanBase* start, const SkOpSpanBase* end,
                         SkPathOpsBounds* bounds) const;

    const SkOpSpanBase* tail() const {
        return &fTail;
    }

    SkOpSpanBase* tail() {
        return &fTail;
    }

    static double TAtMid(const SkOpSpanBase* start, const SkOpSpanBase* end, double mid) {
        return start->t() * (1 - mid) + end->t() * mid;
    }

    void undoneSpan(SkOpSpanBase** start, SkOpSpanBase** end);
    int updateOppWinding(const SkOpSpanBase* start, const SkOpSpanBase* end) const;
    int updateOppWinding(const SkOpAngle* angle) const;
    int updateOppWindingReverse(const SkOpAngle* angle) const;
    int updateWinding(const SkOpSpanBase* start, const SkOpSpanBase* end) const;
    int updateWinding(const SkOpAngle* angle) const;
    int updateWindingReverse(const SkOpAngle* angle) const;

    static bool UseInnerWinding(int outerWinding, int innerWinding);

    SkPath::Verb verb() const {
        return fVerb;
    }

    int windingAtT(double tHit, const SkOpSpan* span, bool crossOpp, SkScalar* dx) const;
    int windSum(const SkOpAngle* angle) const;

    SkPoint* writablePt(bool end) {
        return &fPts[end ? SkPathOpsVerbToPoints(fVerb) : 0];
    }

private:
    SkOpSpan fHead;  // the head span always has its t set to zero
    SkOpSpanBase fTail;  // the tail span always has its t set to one
    SkOpContour* fContour;
    SkOpSegment* fNext;  // forward-only linked list used by contour to walk the segments
    const SkOpSegment* fPrev;
    SkPoint* fPts;  // pointer into array of points owned by edge builder that may be tweaked
    SkPathOpsBounds fBounds;  // tight bounds
    int fCount;  // number of spans (one for a non-intersecting segment)
    int fDoneCount;  // number of processed spans (zero initially)
    SkPath::Verb fVerb;
    bool fVisited;  // used by missing coincidence check
    PATH_OPS_DEBUG_CODE(int fID);
};

#endif
