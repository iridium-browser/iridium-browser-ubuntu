/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "SkOpCoincidence.h"
#include "SkOpContour.h"
#include "SkOpSegment.h"
#include "SkPathWriter.h"

/*
After computing raw intersections, post process all segments to:
- find small collections of points that can be collapsed to a single point
- find missing intersections to resolve differences caused by different algorithms

Consider segments containing tiny or small intervals. Consider coincident segments
because coincidence finds intersections through distance measurement that non-coincident
intersection tests cannot.
 */

#define F (false)      // discard the edge
#define T (true)       // keep the edge

static const bool gUnaryActiveEdge[2][2] = {
//  from=0  from=1
//  to=0,1  to=0,1
    {F, T}, {T, F},
};

static const bool gActiveEdge[kXOR_SkPathOp + 1][2][2][2][2] = {
//                 miFrom=0                              miFrom=1
//         miTo=0             miTo=1             miTo=0             miTo=1
//     suFrom=0    1      suFrom=0    1      suFrom=0    1      suFrom=0    1
//   suTo=0,1 suTo=0,1  suTo=0,1 suTo=0,1  suTo=0,1 suTo=0,1  suTo=0,1 suTo=0,1
    {{{{F, F}, {F, F}}, {{T, F}, {T, F}}}, {{{T, T}, {F, F}}, {{F, T}, {T, F}}}},  // mi - su
    {{{{F, F}, {F, F}}, {{F, T}, {F, T}}}, {{{F, F}, {T, T}}, {{F, T}, {T, F}}}},  // mi & su
    {{{{F, T}, {T, F}}, {{T, T}, {F, F}}}, {{{T, F}, {T, F}}, {{F, F}, {F, F}}}},  // mi | su
    {{{{F, T}, {T, F}}, {{T, F}, {F, T}}}, {{{T, F}, {F, T}}, {{F, T}, {T, F}}}},  // mi ^ su
};

#undef F
#undef T

SkOpAngle* SkOpSegment::activeAngle(SkOpSpanBase* start, SkOpSpanBase** startPtr,
        SkOpSpanBase** endPtr, bool* done, bool* sortable) {
    if (SkOpAngle* result = activeAngleInner(start, startPtr, endPtr, done, sortable)) {
        return result;
    }
    if (SkOpAngle* result = activeAngleOther(start, startPtr, endPtr, done, sortable)) {
        return result;
    }
    return NULL;
}

SkOpAngle* SkOpSegment::activeAngleInner(SkOpSpanBase* start, SkOpSpanBase** startPtr,
        SkOpSpanBase** endPtr, bool* done, bool* sortable) {
    SkOpSpan* upSpan = start->upCastable();
    if (upSpan) {
        if (upSpan->windValue() || upSpan->oppValue()) {
            SkOpSpanBase* next = upSpan->next();
            if (!*endPtr) {
                *startPtr = start;
                *endPtr = next;
            }
            if (!upSpan->done()) {
                if (upSpan->windSum() != SK_MinS32) {
                    return spanToAngle(start, next);
                }
                *done = false;
            }
        } else {
            SkASSERT(upSpan->done());
        }
    }
    SkOpSpan* downSpan = start->prev();
    // edge leading into junction
    if (downSpan) {
        if (downSpan->windValue() || downSpan->oppValue()) {
            if (!*endPtr) {
                *startPtr = start;
                *endPtr = downSpan;
            }
            if (!downSpan->done()) {
                if (downSpan->windSum() != SK_MinS32) {
                    return spanToAngle(start, downSpan);
                }
                *done = false;
            }
        } else {
            SkASSERT(downSpan->done());
        }
    }
    return NULL;
}

SkOpAngle* SkOpSegment::activeAngleOther(SkOpSpanBase* start, SkOpSpanBase** startPtr,
        SkOpSpanBase** endPtr, bool* done, bool* sortable) {
    SkOpPtT* oPtT = start->ptT()->next();
    SkOpSegment* other = oPtT->segment();
    SkOpSpanBase* oSpan = oPtT->span();
    return other->activeAngleInner(oSpan, startPtr, endPtr, done, sortable);
}

SkPoint SkOpSegment::activeLeftTop(SkOpSpanBase** firstSpan) {
    SkASSERT(!done());
    SkPoint topPt = {SK_ScalarMax, SK_ScalarMax};
    // see if either end is not done since we want smaller Y of the pair
    bool lastDone = true;
    double lastT = -1;
    SkOpSpanBase* span = &fHead;
    do {
        if (lastDone && (span->final() || span->upCast()->done())) {
            goto next;
        }
        {
            const SkPoint& xy = span->pt();
            if (topPt.fY > xy.fY || (topPt.fY == xy.fY && topPt.fX > xy.fX)) {
                topPt = xy;
                if (firstSpan) {
                    *firstSpan = span;
                }
            }
            if (fVerb != SkPath::kLine_Verb && !lastDone) {
                SkPoint curveTop = (*CurveTop[SkPathOpsVerbToPoints(fVerb)])(fPts, lastT,
                        span->t());
                if (topPt.fY > curveTop.fY || (topPt.fY == curveTop.fY
                        && topPt.fX > curveTop.fX)) {
                    topPt = curveTop;
                    if (firstSpan) {
                        *firstSpan = span;
                    }
                }
            }
            lastT = span->t();
        }
next:
        if (span->final()) {
            break;
        }
        lastDone = span->upCast()->done();
    } while ((span = span->upCast()->next()));
    return topPt;
}

bool SkOpSegment::activeOp(SkOpSpanBase* start, SkOpSpanBase* end, int xorMiMask, int xorSuMask,
        SkPathOp op) {
    int sumMiWinding = this->updateWinding(end, start);
    int sumSuWinding = this->updateOppWinding(end, start);
#if DEBUG_LIMIT_WIND_SUM
    SkASSERT(abs(sumMiWinding) <= DEBUG_LIMIT_WIND_SUM);
    SkASSERT(abs(sumSuWinding) <= DEBUG_LIMIT_WIND_SUM);
#endif
    if (this->operand()) {
        SkTSwap<int>(sumMiWinding, sumSuWinding);
    }
    return this->activeOp(xorMiMask, xorSuMask, start, end, op, &sumMiWinding, &sumSuWinding);
}

bool SkOpSegment::activeOp(int xorMiMask, int xorSuMask, SkOpSpanBase* start, SkOpSpanBase* end,
        SkPathOp op, int* sumMiWinding, int* sumSuWinding) {
    int maxWinding, sumWinding, oppMaxWinding, oppSumWinding;
    this->setUpWindings(start, end, sumMiWinding, sumSuWinding,
            &maxWinding, &sumWinding, &oppMaxWinding, &oppSumWinding);
    bool miFrom;
    bool miTo;
    bool suFrom;
    bool suTo;
    if (operand()) {
        miFrom = (oppMaxWinding & xorMiMask) != 0;
        miTo = (oppSumWinding & xorMiMask) != 0;
        suFrom = (maxWinding & xorSuMask) != 0;
        suTo = (sumWinding & xorSuMask) != 0;
    } else {
        miFrom = (maxWinding & xorMiMask) != 0;
        miTo = (sumWinding & xorMiMask) != 0;
        suFrom = (oppMaxWinding & xorSuMask) != 0;
        suTo = (oppSumWinding & xorSuMask) != 0;
    }
    bool result = gActiveEdge[op][miFrom][miTo][suFrom][suTo];
#if DEBUG_ACTIVE_OP
    SkDebugf("%s id=%d t=%1.9g tEnd=%1.9g op=%s miFrom=%d miTo=%d suFrom=%d suTo=%d result=%d\n",
            __FUNCTION__, debugID(), start->t(), end->t(),
            SkPathOpsDebug::kPathOpStr[op], miFrom, miTo, suFrom, suTo, result);
#endif
    return result;
}

bool SkOpSegment::activeWinding(SkOpSpanBase* start, SkOpSpanBase* end) {
    int sumWinding = updateWinding(end, start);
    return activeWinding(start, end, &sumWinding);
}

bool SkOpSegment::activeWinding(SkOpSpanBase* start, SkOpSpanBase* end, int* sumWinding) {
    int maxWinding;
    setUpWinding(start, end, &maxWinding, sumWinding);
    bool from = maxWinding != 0;
    bool to = *sumWinding  != 0;
    bool result = gUnaryActiveEdge[from][to];
    return result;
}

void SkOpSegment::addCurveTo(const SkOpSpanBase* start, const SkOpSpanBase* end,
        SkPathWriter* path, bool active) const {
    SkPoint edge[4];
    const SkPoint* ePtr;
    if ((start == &fHead && end == &fTail) || (start == &fTail && end == &fHead)) {
        ePtr = fPts;
    } else {
    // OPTIMIZE? if not active, skip remainder and return xyAtT(end)
        subDivide(start, end, edge);
        ePtr = edge;
    }
    if (active) {
        bool reverse = ePtr == fPts && start != &fHead;
        if (reverse) {
            path->deferredMoveLine(ePtr[SkPathOpsVerbToPoints(fVerb)]);
            switch (fVerb) {
                case SkPath::kLine_Verb:
                    path->deferredLine(ePtr[0]);
                    break;
                case SkPath::kQuad_Verb:
                    path->quadTo(ePtr[1], ePtr[0]);
                    break;
                case SkPath::kCubic_Verb:
                    path->cubicTo(ePtr[2], ePtr[1], ePtr[0]);
                    break;
                default:
                    SkASSERT(0);
            }
       } else {
            path->deferredMoveLine(ePtr[0]);
            switch (fVerb) {
                case SkPath::kLine_Verb:
                    path->deferredLine(ePtr[1]);
                    break;
                case SkPath::kQuad_Verb:
                    path->quadTo(ePtr[1], ePtr[2]);
                    break;
                case SkPath::kCubic_Verb:
                    path->cubicTo(ePtr[1], ePtr[2], ePtr[3]);
                    break;
                default:
                    SkASSERT(0);
            }
        }
    }
}

SkOpPtT* SkOpSegment::addMissing(double t, SkOpSegment* opp, SkChunkAlloc* allocator) {
    SkOpSpanBase* existing = NULL;
    SkOpSpanBase* test = &fHead;
    double testT;
    do {
        if ((testT = test->ptT()->fT) >= t) {
            if (testT == t) {
                existing = test;
            }
            break;
        }
    } while ((test = test->upCast()->next()));
    SkOpPtT* result;
    if (existing && existing->contains(opp)) {
        result = existing->ptT();
    } else {
        result = this->addT(t, SkOpSegment::kNoAlias, allocator);
    }
    SkASSERT(result);
    return result;
}

SkOpAngle* SkOpSegment::addSingletonAngleDown(SkOpSegment** otherPtr, SkOpAngle** anglePtr,
        SkChunkAlloc* allocator) {
    SkOpSpan* startSpan = fTail.prev();
    SkASSERT(startSpan);
    SkOpAngle* angle = SkOpTAllocator<SkOpAngle>::Allocate(allocator);
    *anglePtr = angle;
    angle->set(&fTail, startSpan);
    fTail.setFromAngle(angle);
    SkOpSegment* other = NULL;  // these initializations silence a release build warning
    SkOpSpan* oStartSpan = NULL;
    SkOpSpanBase* oEndSpan = NULL;
    SkOpPtT* ptT = fTail.ptT(), * startPtT = ptT;
    while ((ptT = ptT->next()) != startPtT) {
        other = ptT->segment();
        oStartSpan = ptT->span()->upCastable();
        if (oStartSpan && oStartSpan->windValue()) {
            oEndSpan = oStartSpan->next();
            break;
        }
        oEndSpan = ptT->span();
        oStartSpan = oEndSpan->prev();
        if (oStartSpan && oStartSpan->windValue()) {
            break;
        }
    }
    SkOpAngle* oAngle = SkOpTAllocator<SkOpAngle>::Allocate(allocator);
    oAngle->set(oStartSpan, oEndSpan);
    oStartSpan->setToAngle(oAngle);
    *otherPtr = other;
    return oAngle;
}

SkOpAngle* SkOpSegment::addSingletonAngles(int step, SkChunkAlloc* allocator) {
    SkOpSegment* other;
    SkOpAngle* angle, * otherAngle;
    if (step > 0) {
        otherAngle = addSingletonAngleUp(&other, &angle, allocator);
    } else {
        otherAngle = addSingletonAngleDown(&other, &angle, allocator);
    }
    angle->insert(otherAngle);
    return angle;
}

SkOpAngle* SkOpSegment::addSingletonAngleUp(SkOpSegment** otherPtr, SkOpAngle** anglePtr,
        SkChunkAlloc* allocator) {
    SkOpSpanBase* endSpan = fHead.next();
    SkASSERT(endSpan);
    SkOpAngle* angle = SkOpTAllocator<SkOpAngle>::Allocate(allocator);
    *anglePtr = angle;
    angle->set(&fHead, endSpan);
    fHead.setToAngle(angle);
    SkOpSegment* other = NULL;  // these initializations silence a release build warning
    SkOpSpan* oStartSpan = NULL;
    SkOpSpanBase* oEndSpan = NULL;
    SkOpPtT* ptT = fHead.ptT(), * startPtT = ptT;
    while ((ptT = ptT->next()) != startPtT) {
        other = ptT->segment();
        oEndSpan = ptT->span();
        oStartSpan = oEndSpan->prev();
        if (oStartSpan && oStartSpan->windValue()) {
            break;
        }
        oStartSpan = oEndSpan->upCastable();
        if (oStartSpan && oStartSpan->windValue()) {
            oEndSpan = oStartSpan->next();
            break;
        }
    }
    SkOpAngle* oAngle = SkOpTAllocator<SkOpAngle>::Allocate(allocator);
    oAngle->set(oEndSpan, oStartSpan);
    oEndSpan->setFromAngle(oAngle);
    *otherPtr = other;
    return oAngle;
}

SkOpPtT* SkOpSegment::addT(double t, AllowAlias allowAlias, SkChunkAlloc* allocator) {
    debugValidate();
    SkPoint pt = this->ptAtT(t);
    SkOpSpanBase* span = &fHead;
    do {
        SkOpPtT* result = span->ptT();
        if (t == result->fT) {
            return result;
        }
        if (this->match(result, this, t, pt)) {
            // see if any existing alias matches segment, pt, and t
            SkOpPtT* loop = result->next();
            bool duplicatePt = false;
            while (loop != result) {
                bool ptMatch = loop->fPt == pt;
                if (loop->segment() == this && loop->fT == t && ptMatch) {
                    return result;
                }
                duplicatePt |= ptMatch;
                loop = loop->next();
            }
            if (kNoAlias == allowAlias) {
                return result;
            }
            SkOpPtT* alias = SkOpTAllocator<SkOpPtT>::Allocate(allocator);
            alias->init(result->span(), t, pt, duplicatePt);
            result->insert(alias);
            result->span()->unaligned();
            this->debugValidate();
#if DEBUG_ADD_T
            SkDebugf("%s alias t=%1.9g segID=%d spanID=%d\n",  __FUNCTION__, t,
                    alias->segment()->debugID(), alias->span()->debugID());
#endif
            return alias;
        }
        if (t < result->fT) {
            SkOpSpan* prev = result->span()->prev();
            SkOpSpan* span = insert(prev, allocator);
            span->init(this, prev, t, pt);
            this->debugValidate();
#if DEBUG_ADD_T
            SkDebugf("%s insert t=%1.9g segID=%d spanID=%d\n", __FUNCTION__, t,
                    span->segment()->debugID(), span->debugID());
#endif
            return span->ptT();
        }
        SkASSERT(span != &fTail);
    } while ((span = span->upCast()->next()));
    SkASSERT(0);
    return NULL;
}

// choose a solitary t and pt value; remove aliases; align the opposite ends
void SkOpSegment::align() {
    debugValidate();
    SkOpSpanBase* span = &fHead;
    if (!span->aligned()) {
        span->alignEnd(0, fPts[0]);
    }
    while ((span = span->upCast()->next())) {
        if (span == &fTail) {
            break;
        }
        span->align();
    }
    if (!span->aligned()) {
        span->alignEnd(1, fPts[SkPathOpsVerbToPoints(fVerb)]);
    }
    debugValidate();
}

bool SkOpSegment::BetweenTs(const SkOpSpanBase* lesser, double testT,
        const SkOpSpanBase* greater) {
    if (lesser->t() > greater->t()) {
        SkTSwap<const SkOpSpanBase*>(lesser, greater);
    }
    return approximately_between(lesser->t(), testT, greater->t());
}

void SkOpSegment::calcAngles(SkChunkAlloc* allocator) {
    bool activePrior = !fHead.isCanceled();
    if (activePrior && !fHead.simple()) {
        addStartSpan(allocator);
    }
    SkOpSpan* prior = &fHead;
    SkOpSpanBase* spanBase = fHead.next();
    while (spanBase != &fTail) {
        if (activePrior) {
            SkOpAngle* priorAngle = SkOpTAllocator<SkOpAngle>::Allocate(allocator);
            priorAngle->set(spanBase, prior);
            spanBase->setFromAngle(priorAngle);
        }
        SkOpSpan* span = spanBase->upCast();
        bool active = !span->isCanceled();
        SkOpSpanBase* next = span->next();
        if (active) {
            SkOpAngle* angle = SkOpTAllocator<SkOpAngle>::Allocate(allocator);
            angle->set(span, next);
            span->setToAngle(angle);
        }
        activePrior = active;
        prior = span;
        spanBase = next;
    }
    if (activePrior && !fTail.simple()) {
        addEndSpan(allocator);
    }
}

void SkOpSegment::checkAngleCoin(SkOpCoincidence* coincidences, SkChunkAlloc* allocator) {
    SkOpSpanBase* base = &fHead;
    SkOpSpan* span;
    do {
        SkOpAngle* angle = base->fromAngle();
        if (angle && angle->fCheckCoincidence) {
            angle->checkNearCoincidence();
        }
        if (base->final()) {
             break;
        }
        span = base->upCast();
        angle = span->toAngle();
        if (angle && angle->fCheckCoincidence) {
            angle->checkNearCoincidence();
        }
    } while ((base = span->next()));
}

// from http://stackoverflow.com/questions/1165647/how-to-determine-if-a-list-of-polygon-points-are-in-clockwise-order
bool SkOpSegment::clockwise(const SkOpSpanBase* start, const SkOpSpanBase* end, bool* swap) const {
    SkASSERT(fVerb != SkPath::kLine_Verb);
    SkPoint edge[4];
    if (fVerb == SkPath::kCubic_Verb) {
        double startT = start->t();
        double endT = end->t();
        bool flip = startT > endT;
        SkDCubic cubic;
        cubic.set(fPts);
        double inflectionTs[2];
        int inflections = cubic.findInflections(inflectionTs);
        for (int index = 0; index < inflections; ++index) {
            double inflectionT = inflectionTs[index];
            if (between(startT, inflectionT, endT)) {
                if (flip) {
                    if (inflectionT != endT) {
                        startT = inflectionT;
                    }
                } else {
                    if (inflectionT != startT) {
                        endT = inflectionT;
                    }
                }
            }
        }
        SkDCubic part = cubic.subDivide(startT, endT);
        for (int index = 0; index < 4; ++index) {
            edge[index] = part[index].asSkPoint();
        }
    } else {
    subDivide(start, end, edge);
    }
    bool sumSet = false;
    int points = SkPathOpsVerbToPoints(fVerb);
    double sum = (edge[0].fX - edge[points].fX) * (edge[0].fY + edge[points].fY);
    if (!sumSet) {
        for (int idx = 0; idx < points; ++idx){
            sum += (edge[idx + 1].fX - edge[idx].fX) * (edge[idx + 1].fY + edge[idx].fY);
        }
    }
    if (fVerb == SkPath::kCubic_Verb) {
        SkDCubic cubic;
        cubic.set(edge);
        *swap = sum > 0 && !cubic.monotonicInY();
    } else {
        SkDQuad quad;
        quad.set(edge);
        *swap = sum > 0 && !quad.monotonicInY();
    }
    return sum <= 0;
}

void SkOpSegment::ComputeOneSum(const SkOpAngle* baseAngle, SkOpAngle* nextAngle,
        SkOpAngle::IncludeType includeType) {
    const SkOpSegment* baseSegment = baseAngle->segment();
    int sumMiWinding = baseSegment->updateWindingReverse(baseAngle);
    int sumSuWinding;
    bool binary = includeType >= SkOpAngle::kBinarySingle;
    if (binary) {
        sumSuWinding = baseSegment->updateOppWindingReverse(baseAngle);
        if (baseSegment->operand()) {
            SkTSwap<int>(sumMiWinding, sumSuWinding);
        }
    }
    SkOpSegment* nextSegment = nextAngle->segment();
    int maxWinding, sumWinding;
    SkOpSpanBase* last;
    if (binary) {
        int oppMaxWinding, oppSumWinding;
        nextSegment->setUpWindings(nextAngle->start(), nextAngle->end(), &sumMiWinding,
                &sumSuWinding, &maxWinding, &sumWinding, &oppMaxWinding, &oppSumWinding);
        last = nextSegment->markAngle(maxWinding, sumWinding, oppMaxWinding, oppSumWinding,
                nextAngle);
    } else {
        nextSegment->setUpWindings(nextAngle->start(), nextAngle->end(), &sumMiWinding,
                &maxWinding, &sumWinding);
        last = nextSegment->markAngle(maxWinding, sumWinding, nextAngle);
    }
    nextAngle->setLastMarked(last);
}

void SkOpSegment::ComputeOneSumReverse(const SkOpAngle* baseAngle, SkOpAngle* nextAngle,
        SkOpAngle::IncludeType includeType) {
    const SkOpSegment* baseSegment = baseAngle->segment();
    int sumMiWinding = baseSegment->updateWinding(baseAngle);
    int sumSuWinding;
    bool binary = includeType >= SkOpAngle::kBinarySingle;
    if (binary) {
        sumSuWinding = baseSegment->updateOppWinding(baseAngle);
        if (baseSegment->operand()) {
            SkTSwap<int>(sumMiWinding, sumSuWinding);
        }
    }
    SkOpSegment* nextSegment = nextAngle->segment();
    int maxWinding, sumWinding;
    SkOpSpanBase* last;
    if (binary) {
        int oppMaxWinding, oppSumWinding;
        nextSegment->setUpWindings(nextAngle->end(), nextAngle->start(), &sumMiWinding,
                &sumSuWinding, &maxWinding, &sumWinding, &oppMaxWinding, &oppSumWinding);
        last = nextSegment->markAngle(maxWinding, sumWinding, oppMaxWinding, oppSumWinding,
                nextAngle);
    } else {
        nextSegment->setUpWindings(nextAngle->end(), nextAngle->start(), &sumMiWinding,
                &maxWinding, &sumWinding);
        last = nextSegment->markAngle(maxWinding, sumWinding, nextAngle);
    }
    nextAngle->setLastMarked(last);
}

// at this point, the span is already ordered, or unorderable
int SkOpSegment::computeSum(SkOpSpanBase* start, SkOpSpanBase* end,
        SkOpAngle::IncludeType includeType) {
    SkASSERT(includeType != SkOpAngle::kUnaryXor);
    SkOpAngle* firstAngle = this->spanToAngle(end, start);
    if (NULL == firstAngle || NULL == firstAngle->next()) {
        return SK_NaN32;
    }
    // if all angles have a computed winding,
    //  or if no adjacent angles are orderable,
    //  or if adjacent orderable angles have no computed winding,
    //  there's nothing to do
    // if two orderable angles are adjacent, and both are next to orderable angles,
    //  and one has winding computed, transfer to the other
    SkOpAngle* baseAngle = NULL;
    bool tryReverse = false;
    // look for counterclockwise transfers
    SkOpAngle* angle = firstAngle->previous();
    SkOpAngle* next = angle->next();
    firstAngle = next;
    do {
        SkOpAngle* prior = angle;
        angle = next;
        next = angle->next();
        SkASSERT(prior->next() == angle);
        SkASSERT(angle->next() == next);
        if (prior->unorderable() || angle->unorderable() || next->unorderable()) {
            baseAngle = NULL;
            continue;
        }
        int testWinding = angle->starter()->windSum();
        if (SK_MinS32 != testWinding) {
            baseAngle = angle;
            tryReverse = true;
            continue;
        }
        if (baseAngle) {
            ComputeOneSum(baseAngle, angle, includeType);
            baseAngle = SK_MinS32 != angle->starter()->windSum() ? angle : NULL;
        }
    } while (next != firstAngle);
    if (baseAngle && SK_MinS32 == firstAngle->starter()->windSum()) {
        firstAngle = baseAngle;
        tryReverse = true;
    }
    if (tryReverse) {
        baseAngle = NULL;
        SkOpAngle* prior = firstAngle;
        do {
            angle = prior;
            prior = angle->previous();
            SkASSERT(prior->next() == angle);
            next = angle->next();
            if (prior->unorderable() || angle->unorderable() || next->unorderable()) {
                baseAngle = NULL;
                continue;
            }
            int testWinding = angle->starter()->windSum();
            if (SK_MinS32 != testWinding) {
                baseAngle = angle;
                continue;
            }
            if (baseAngle) {
                ComputeOneSumReverse(baseAngle, angle, includeType);
                baseAngle = SK_MinS32 != angle->starter()->windSum() ? angle : NULL;
            }
        } while (prior != firstAngle);
    }
    return start->starter(end)->windSum();
}

SkOpSpan* SkOpSegment::crossedSpanY(const SkPoint& basePt, double mid, bool opp, bool current,
        SkScalar* bestY, double* hitT, bool* hitSomething, bool* vertical) {
    SkScalar bottom = fBounds.fBottom;
    *vertical = false;
    if (bottom <= *bestY) {
        return NULL;
    }
    SkScalar top = fBounds.fTop;
    if (top >= basePt.fY) {
        return NULL;
    }
    if (fBounds.fLeft > basePt.fX) {
        return NULL;
    }
    if (fBounds.fRight < basePt.fX) {
        return NULL;
    }
    if (fBounds.fLeft == fBounds.fRight) {
        // if vertical, and directly above test point, wait for another one
        *vertical = AlmostEqualUlps(basePt.fX, fBounds.fLeft);
        return NULL;
    }
    // intersect ray starting at basePt with edge
    SkIntersections intersections;
    // OPTIMIZE: use specialty function that intersects ray with curve,
    // returning t values only for curve (we don't care about t on ray)
    intersections.allowNear(false);
    int pts = (intersections.*CurveVertical[SkPathOpsVerbToPoints(fVerb)])
            (fPts, top, bottom, basePt.fX, false);
    if (pts == 0 || (current && pts == 1)) {
        return NULL;
    }
    if (current) {
        SkASSERT(pts > 1);
        int closestIdx = 0;
        double closest = fabs(intersections[0][0] - mid);
        for (int idx = 1; idx < pts; ++idx) {
            double test = fabs(intersections[0][idx] - mid);
            if (closest > test) {
                closestIdx = idx;
                closest = test;
            }
        }
        intersections.quickRemoveOne(closestIdx, --pts);
    }
    double bestT = -1;
    for (int index = 0; index < pts; ++index) {
        double foundT = intersections[0][index];
        if (approximately_less_than_zero(foundT)
                    || approximately_greater_than_one(foundT)) {
            continue;
        }
        SkScalar testY = (*CurvePointAtT[SkPathOpsVerbToPoints(fVerb)])(fPts, foundT).fY;
        if (approximately_negative(testY - *bestY)
                || approximately_negative(basePt.fY - testY)) {
            continue;
        }
        if (pts > 1 && fVerb == SkPath::kLine_Verb) {
            *vertical = true;
            return NULL;  // if the intersection is edge on, wait for another one
        }
        if (fVerb > SkPath::kLine_Verb) {
            SkScalar dx = (*CurveSlopeAtT[SkPathOpsVerbToPoints(fVerb)])(fPts, foundT).fX;
            if (approximately_zero(dx)) {
                *vertical = true;
                return NULL;  // hit vertical, wait for another one
            }
        }
        *bestY = testY;
        bestT = foundT;
    }
    if (bestT < 0) {
        return NULL;
    }
    SkASSERT(bestT >= 0);
    SkASSERT(bestT < 1);
    SkOpSpanBase* testTSpanBase = &this->fHead;
    SkOpSpanBase* nextTSpan;
    double endT = 0;
    do {
        nextTSpan = testTSpanBase->upCast()->next();
        endT = nextTSpan->t();
        if (endT >= bestT) {
            break;
        }
        testTSpanBase = nextTSpan;
    } while (testTSpanBase);
    SkOpSpan* bestTSpan = NULL;
    SkOpSpan* testTSpan = testTSpanBase->upCast();
    if (!testTSpan->isCanceled()) {
        *hitT = bestT;
        bestTSpan = testTSpan;
        *hitSomething = true;
    }
    return bestTSpan;
}

void SkOpSegment::detach(const SkOpSpan* span) {
    if (span->done()) {
        --this->fDoneCount;
    }
    --this->fCount;
}

double SkOpSegment::distSq(double t, SkOpAngle* oppAngle) {
    SkDPoint testPt = this->dPtAtT(t);
    SkDLine testPerp = {{ testPt, testPt }};
    SkDVector slope = this->dSlopeAtT(t);
    testPerp[1].fX += slope.fY;
    testPerp[1].fY -= slope.fX;
    SkIntersections i;
    SkOpSegment* oppSegment = oppAngle->segment();
    int oppPtCount = SkPathOpsVerbToPoints(oppSegment->verb());
    (*CurveIntersectRay[oppPtCount])(oppSegment->pts(), testPerp, &i);
    double closestDistSq = SK_ScalarInfinity;
    for (int index = 0; index < i.used(); ++index) {
        if (!between(oppAngle->start()->t(), i[0][index], oppAngle->end()->t())) {
            continue;
        }
        double testDistSq = testPt.distanceSquared(i.pt(index));
        if (closestDistSq > testDistSq) {
            closestDistSq = testDistSq;
        }
    }
    return closestDistSq;
}

/*
 The M and S variable name parts stand for the operators.
   Mi stands for Minuend (see wiki subtraction, analogous to difference)
   Su stands for Subtrahend
 The Opp variable name part designates that the value is for the Opposite operator.
 Opposite values result from combining coincident spans.
 */
SkOpSegment* SkOpSegment::findNextOp(SkTDArray<SkOpSpanBase*>* chase, SkOpSpanBase** nextStart,
        SkOpSpanBase** nextEnd, bool* unsortable, SkPathOp op, int xorMiMask, int xorSuMask) {
    SkOpSpanBase* start = *nextStart;
    SkOpSpanBase* end = *nextEnd;
    SkASSERT(start != end);
    int step = start->step(end);
    SkOpSegment* other = this->isSimple(nextStart, &step);  // advances nextStart
    if (other) {
    // mark the smaller of startIndex, endIndex done, and all adjacent
    // spans with the same T value (but not 'other' spans)
#if DEBUG_WINDING
        SkDebugf("%s simple\n", __FUNCTION__);
#endif
        SkOpSpan* startSpan = start->starter(end);
        if (startSpan->done()) {
            return NULL;
        }
        markDone(startSpan);
        *nextEnd = step > 0 ? (*nextStart)->upCast()->next() : (*nextStart)->prev();
        return other;
    }
    SkOpSpanBase* endNear = step > 0 ? (*nextStart)->upCast()->next() : (*nextStart)->prev();
    SkASSERT(endNear == end);  // is this ever not end?
    SkASSERT(endNear);
    SkASSERT(start != endNear);
    SkASSERT((start->t() < endNear->t()) ^ (step < 0));
    // more than one viable candidate -- measure angles to find best
    int calcWinding = computeSum(start, endNear, SkOpAngle::kBinaryOpp);
    bool sortable = calcWinding != SK_NaN32;
    if (!sortable) {
        *unsortable = true;
        markDone(start->starter(end));
        return NULL;
    }
    SkOpAngle* angle = this->spanToAngle(end, start);
    if (angle->unorderable()) {
        *unsortable = true;
        markDone(start->starter(end));
        return NULL;
    }
#if DEBUG_SORT
    SkDebugf("%s\n", __FUNCTION__);
    angle->debugLoop();
#endif
    int sumMiWinding = updateWinding(end, start);
    if (sumMiWinding == SK_MinS32) {
        *unsortable = true;
        markDone(start->starter(end));
        return NULL;
    }
    int sumSuWinding = updateOppWinding(end, start);
    if (operand()) {
        SkTSwap<int>(sumMiWinding, sumSuWinding);
    }
    SkOpAngle* nextAngle = angle->next();
    const SkOpAngle* foundAngle = NULL;
    bool foundDone = false;
    // iterate through the angle, and compute everyone's winding
    SkOpSegment* nextSegment;
    int activeCount = 0;
    do {
        nextSegment = nextAngle->segment();
        bool activeAngle = nextSegment->activeOp(xorMiMask, xorSuMask, nextAngle->start(),
                nextAngle->end(), op, &sumMiWinding, &sumSuWinding);
        if (activeAngle) {
            ++activeCount;
            if (!foundAngle || (foundDone && activeCount & 1)) {
                foundAngle = nextAngle;
                foundDone = nextSegment->done(nextAngle);
            }
        }
        if (nextSegment->done()) {
            continue;
        }
        if (!activeAngle) {
            (void) nextSegment->markAndChaseDone(nextAngle->start(), nextAngle->end());
        }
        SkOpSpanBase* last = nextAngle->lastMarked();
        if (last) {
            SkASSERT(!SkPathOpsDebug::ChaseContains(*chase, last));
            *chase->append() = last;
#if DEBUG_WINDING
            SkDebugf("%s chase.append segment=%d span=%d", __FUNCTION__,
                    last->segment()->debugID(), last->debugID());
            if (!last->final()) {
                SkDebugf(" windSum=%d", last->upCast()->windSum());
            }
            SkDebugf("\n");
#endif
        }
    } while ((nextAngle = nextAngle->next()) != angle);
    start->segment()->markDone(start->starter(end));
    if (!foundAngle) {
        return NULL;
    }
    *nextStart = foundAngle->start();
    *nextEnd = foundAngle->end();
    nextSegment = foundAngle->segment();
#if DEBUG_WINDING
    SkDebugf("%s from:[%d] to:[%d] start=%d end=%d\n",
            __FUNCTION__, debugID(), nextSegment->debugID(), *nextStart, *nextEnd);
 #endif
    return nextSegment;
}

SkOpSegment* SkOpSegment::findNextWinding(SkTDArray<SkOpSpanBase*>* chase,
        SkOpSpanBase** nextStart, SkOpSpanBase** nextEnd, bool* unsortable) {
    SkOpSpanBase* start = *nextStart;
    SkOpSpanBase* end = *nextEnd;
    SkASSERT(start != end);
    int step = start->step(end);
    SkOpSegment* other = this->isSimple(nextStart, &step);  // advances nextStart
    if (other) {
    // mark the smaller of startIndex, endIndex done, and all adjacent
    // spans with the same T value (but not 'other' spans)
#if DEBUG_WINDING
        SkDebugf("%s simple\n", __FUNCTION__);
#endif
        SkOpSpan* startSpan = start->starter(end);
        if (startSpan->done()) {
            return NULL;
        }
        markDone(startSpan);
        *nextEnd = step > 0 ? (*nextStart)->upCast()->next() : (*nextStart)->prev();
        return other;
    }
    SkOpSpanBase* endNear = step > 0 ? (*nextStart)->upCast()->next() : (*nextStart)->prev();
    SkASSERT(endNear == end);  // is this ever not end?
    SkASSERT(endNear);
    SkASSERT(start != endNear);
    SkASSERT((start->t() < endNear->t()) ^ (step < 0));
    // more than one viable candidate -- measure angles to find best
    int calcWinding = computeSum(start, endNear, SkOpAngle::kUnaryWinding);
    bool sortable = calcWinding != SK_NaN32;
    if (!sortable) {
        *unsortable = true;
        markDone(start->starter(end));
        return NULL;
    }
    SkOpAngle* angle = this->spanToAngle(end, start);
    if (angle->unorderable()) {
        *unsortable = true;
        markDone(start->starter(end));
        return NULL;
    }
#if DEBUG_SORT
    SkDebugf("%s\n", __FUNCTION__);
    angle->debugLoop();
#endif
    int sumWinding = updateWinding(end, start);
    SkOpAngle* nextAngle = angle->next();
    const SkOpAngle* foundAngle = NULL;
    bool foundDone = false;
    // iterate through the angle, and compute everyone's winding
    SkOpSegment* nextSegment;
    int activeCount = 0;
    do {
        nextSegment = nextAngle->segment();
        bool activeAngle = nextSegment->activeWinding(nextAngle->start(), nextAngle->end(),
                &sumWinding);
        if (activeAngle) {
            ++activeCount;
            if (!foundAngle || (foundDone && activeCount & 1)) {
                foundAngle = nextAngle;
                foundDone = nextSegment->done(nextAngle);
            }
        }
        if (nextSegment->done()) {
            continue;
        }
        if (!activeAngle) {
            (void) nextSegment->markAndChaseDone(nextAngle->start(), nextAngle->end());
        }
        SkOpSpanBase* last = nextAngle->lastMarked();
        if (last) {
            SkASSERT(!SkPathOpsDebug::ChaseContains(*chase, last));
            *chase->append() = last;
#if DEBUG_WINDING
            SkDebugf("%s chase.append segment=%d span=%d", __FUNCTION__,
                    last->segment()->debugID(), last->debugID());
            if (!last->final()) {
                SkDebugf(" windSum=%d", last->upCast()->windSum());
            }
            SkDebugf("\n");
#endif
        }
    } while ((nextAngle = nextAngle->next()) != angle);
    start->segment()->markDone(start->starter(end));
    if (!foundAngle) {
        return NULL;
    }
    *nextStart = foundAngle->start();
    *nextEnd = foundAngle->end();
    nextSegment = foundAngle->segment();
#if DEBUG_WINDING
    SkDebugf("%s from:[%d] to:[%d] start=%d end=%d\n",
            __FUNCTION__, debugID(), nextSegment->debugID(), *nextStart, *nextEnd);
 #endif
    return nextSegment;
}

SkOpSegment* SkOpSegment::findNextXor(SkOpSpanBase** nextStart, SkOpSpanBase** nextEnd,
        bool* unsortable) {
    SkOpSpanBase* start = *nextStart;
    SkOpSpanBase* end = *nextEnd;
    SkASSERT(start != end);
    int step = start->step(end);
    SkOpSegment* other = this->isSimple(nextStart, &step);  // advances nextStart
    if (other) {
    // mark the smaller of startIndex, endIndex done, and all adjacent
    // spans with the same T value (but not 'other' spans)
#if DEBUG_WINDING
        SkDebugf("%s simple\n", __FUNCTION__);
#endif
        SkOpSpan* startSpan = start->starter(end);
        if (startSpan->done()) {
            return NULL;
        }
        markDone(startSpan);
        *nextEnd = step > 0 ? (*nextStart)->upCast()->next() : (*nextStart)->prev();
        return other;
    }
    SkDEBUGCODE(SkOpSpanBase* endNear = step > 0 ? (*nextStart)->upCast()->next() \
            : (*nextStart)->prev());
    SkASSERT(endNear == end);  // is this ever not end?
    SkASSERT(endNear);
    SkASSERT(start != endNear);
    SkASSERT((start->t() < endNear->t()) ^ (step < 0));
    SkOpAngle* angle = this->spanToAngle(end, start);
    if (angle->unorderable()) {
        *unsortable = true;
        markDone(start->starter(end));
        return NULL;
    }
#if DEBUG_SORT
    SkDebugf("%s\n", __FUNCTION__);
    angle->debugLoop();
#endif
    SkOpAngle* nextAngle = angle->next();
    const SkOpAngle* foundAngle = NULL;
    bool foundDone = false;
    // iterate through the angle, and compute everyone's winding
    SkOpSegment* nextSegment;
    int activeCount = 0;
    do {
        nextSegment = nextAngle->segment();
        ++activeCount;
        if (!foundAngle || (foundDone && activeCount & 1)) {
            foundAngle = nextAngle;
            if (!(foundDone = nextSegment->done(nextAngle))) {
                break;
            }
        }
        nextAngle = nextAngle->next();
    } while (nextAngle != angle);
    start->segment()->markDone(start->starter(end));
    if (!foundAngle) {
        return NULL;
    }
    *nextStart = foundAngle->start();
    *nextEnd = foundAngle->end();
    nextSegment = foundAngle->segment();
#if DEBUG_WINDING
    SkDebugf("%s from:[%d] to:[%d] start=%d end=%d\n",
            __FUNCTION__, debugID(), nextSegment->debugID(), *nextStart, *nextEnd);
 #endif
    return nextSegment;
}

SkOpSegment* SkOpSegment::findTop(bool firstPass, SkOpSpanBase** startPtr, SkOpSpanBase** endPtr,
        bool* unsortable, SkChunkAlloc* allocator) {
    // iterate through T intersections and return topmost
    // topmost tangent from y-min to first pt is closer to horizontal
    SkASSERT(!done());
    SkOpSpanBase* firstT = NULL;
    (void) this->activeLeftTop(&firstT);
    if (!firstT) {
        *unsortable = !firstPass;
        firstT = &fHead;
        while (firstT->upCast()->done()) {
            firstT = firstT->upCast()->next();
        }
        *startPtr = firstT;
        *endPtr = firstT->upCast()->next();
        return this;
    }
    // sort the edges to find the leftmost
    int step = 1;
    SkOpSpanBase* end;
    if (firstT->final() || firstT->upCast()->done()) {
        step = -1;
        end = firstT->prev();
        SkASSERT(end);
    } else {
        end = firstT->upCast()->next();
    }
    // if the topmost T is not on end, or is three-way or more, find left
    // look for left-ness from tLeft to firstT (matching y of other)
    SkASSERT(firstT != end);
    SkOpAngle* markAngle = spanToAngle(firstT, end);
    if (!markAngle) {
        markAngle = addSingletonAngles(step, allocator);
    }
    markAngle->markStops();
    const SkOpAngle* baseAngle = markAngle->next() == markAngle && !isVertical() ? markAngle
            : markAngle->findFirst();
    if (!baseAngle) {
        return NULL;  // nothing to do
    }
    SkScalar top = SK_ScalarMax;
    const SkOpAngle* firstAngle = NULL;
    const SkOpAngle* angle = baseAngle;
    do {
        if (!angle->unorderable()) {
            const SkOpSegment* next = angle->segment();
            SkPathOpsBounds bounds;
            next->subDivideBounds(angle->end(), angle->start(), &bounds);
            bool nearSame = AlmostEqualUlps(top, bounds.top());
            bool lowerSector = !firstAngle || angle->sectorEnd() < firstAngle->sectorStart();
            bool lesserSector = top > bounds.fTop;
            if (lesserSector && (!nearSame || lowerSector)) {
                top = bounds.fTop;
                firstAngle = angle;
            }
        }
        angle = angle->next();
    } while (angle != baseAngle);
    if (!firstAngle) {
        return NULL;  // if all are unorderable, give up
    }
#if DEBUG_SORT
    SkDebugf("%s\n", __FUNCTION__);
    firstAngle->debugLoop();
#endif
    // skip edges that have already been processed
    angle = firstAngle;
    SkOpSegment* leftSegment = NULL;
    bool looped = false;
    do {
        *unsortable = angle->unorderable();
        if (firstPass || !*unsortable) {
            leftSegment = angle->segment();
            *startPtr = angle->end();
            *endPtr = angle->start();
            const SkOpSpan* firstSpan = (*startPtr)->starter(*endPtr);
            if (!firstSpan->done()) {
                break;
            }
        }
        angle = angle->next();
        looped = true;
    } while (angle != firstAngle);
    if (angle == firstAngle && looped) {
        return NULL;
    }
    if (leftSegment->verb() >= SkPath::kQuad_Verb) {
        SkOpSpanBase* start = *startPtr;
        SkOpSpanBase* end = *endPtr;
        bool swap;
        if (!leftSegment->clockwise(start, end, &swap)) {
    #if DEBUG_SWAP_TOP
            SkDebugf("%s swap=%d inflections=%d monotonic=%d\n",
                    __FUNCTION__,
                    swap, leftSegment->debugInflections(start, end),
                    leftSegment->monotonicInY(start, end));
    #endif
            if (swap) {
    // FIXME: I doubt it makes sense to (necessarily) swap if the edge was not the first
    // sorted but merely the first not already processed (i.e., not done)
                SkTSwap(*startPtr, *endPtr);
            }
        }
    }
    return leftSegment;
}

SkOpGlobalState* SkOpSegment::globalState() const {
    return contour()->globalState(); 
}

void SkOpSegment::init(SkPoint pts[], SkOpContour* contour, SkPath::Verb verb) {
    fContour = contour;
    fNext = NULL;
    fPts = pts;
    fVerb = verb;
    fCount = 0;
    fDoneCount = 0;
    fVisited = false;
    SkOpSpan* zeroSpan = &fHead;
    zeroSpan->init(this, NULL, 0, fPts[0]);
    SkOpSpanBase* oneSpan = &fTail;
    zeroSpan->setNext(oneSpan);
    oneSpan->initBase(this, zeroSpan, 1, fPts[SkPathOpsVerbToPoints(fVerb)]);
    PATH_OPS_DEBUG_CODE(fID = globalState()->nextSegmentID());
}

void SkOpSegment::initWinding(SkOpSpanBase* start, SkOpSpanBase* end,
        SkOpAngle::IncludeType angleIncludeType) {
    int local = SkOpSegment::SpanSign(start, end);
    SkDEBUGCODE(bool success);
    if (angleIncludeType == SkOpAngle::kBinarySingle) {
        int oppLocal = SkOpSegment::OppSign(start, end);
        SkDEBUGCODE(success =) markAndChaseWinding(start, end, local, oppLocal, NULL);
    // OPTIMIZATION: the reverse mark and chase could skip the first marking
        SkDEBUGCODE(success |=) markAndChaseWinding(end, start, local, oppLocal, NULL);
    } else {
        SkDEBUGCODE(success =) markAndChaseWinding(start, end, local, NULL);
    // OPTIMIZATION: the reverse mark and chase could skip the first marking
        SkDEBUGCODE(success |=) markAndChaseWinding(end, start, local, NULL);
    }
    SkASSERT(success);
}

/*
when we start with a vertical intersect, we try to use the dx to determine if the edge is to
the left or the right of vertical. This determines if we need to add the span's
sign or not. However, this isn't enough.
If the supplied sign (winding) is zero, then we didn't hit another vertical span, so dx is needed.
If there was a winding, then it may or may not need adjusting. If the span the winding was borrowed
from has the same x direction as this span, the winding should change. If the dx is opposite, then
the same winding is shared by both.
*/
bool SkOpSegment::initWinding(SkOpSpanBase* start, SkOpSpanBase* end, double tHit,
        int winding, SkScalar hitDx, int oppWind, SkScalar hitOppDx) {
    SkASSERT(this == start->segment());
    SkASSERT(hitDx || !winding);
    SkScalar dx = (*CurveSlopeAtT[SkPathOpsVerbToPoints(fVerb)])(fPts, tHit).fX;
//    SkASSERT(dx);
    int windVal = start->starter(end)->windValue();
#if DEBUG_WINDING_AT_T
    SkDebugf("%s id=%d oldWinding=%d hitDx=%c dx=%c windVal=%d", __FUNCTION__, debugID(), winding,
            hitDx ? hitDx > 0 ? '+' : '-' : '0', dx > 0 ? '+' : '-', windVal);
#endif
    int sideWind = winding + (dx < 0 ? windVal : -windVal);
    if (abs(winding) < abs(sideWind)) {
        winding = sideWind;
    }
    SkDEBUGCODE(int oppLocal = SkOpSegment::OppSign(start, end));
    SkASSERT(hitOppDx || !oppWind || !oppLocal);
    int oppWindVal = start->starter(end)->oppValue();
    if (!oppWind) {
        oppWind = dx < 0 ? oppWindVal : -oppWindVal;
    } else if (hitOppDx * dx >= 0) {
        int oppSideWind = oppWind + (dx < 0 ? oppWindVal : -oppWindVal);
        if (abs(oppWind) < abs(oppSideWind)) {
            oppWind = oppSideWind;
        }
    }
#if DEBUG_WINDING_AT_T
    SkDebugf(" winding=%d oppWind=%d\n", winding, oppWind);
#endif
    // if this fails to mark (because the edges are too small) inform caller to try again
    bool success = markAndChaseWinding(start, end, winding, oppWind, NULL);
    // OPTIMIZATION: the reverse mark and chase could skip the first marking
    success |= markAndChaseWinding(end, start, winding, oppWind, NULL);
    return success;
}

bool SkOpSegment::isClose(double t, const SkOpSegment* opp) const {
    SkDPoint cPt = this->dPtAtT(t);
    int pts = SkPathOpsVerbToPoints(this->verb());
    SkDVector dxdy = (*CurveDSlopeAtT[pts])(this->pts(), t);
    SkDLine perp = {{ cPt, {cPt.fX + dxdy.fY, cPt.fY - dxdy.fX} }};
    SkIntersections i;
    int oppPts = SkPathOpsVerbToPoints(opp->verb());
    (*CurveIntersectRay[oppPts])(opp->pts(), perp, &i);
    int used = i.used();
    for (int index = 0; index < used; ++index) {
        if (cPt.roughlyEqual(i.pt(index))) {
            return true;
        }
    }
    return false;
}

bool SkOpSegment::isXor() const {
    return fContour->isXor();
}

SkOpSpanBase* SkOpSegment::markAndChaseDone(SkOpSpanBase* start, SkOpSpanBase* end) {
    int step = start->step(end);
    SkOpSpan* minSpan = start->starter(end);
    markDone(minSpan);
    SkOpSpanBase* last = NULL;
    SkOpSegment* other = this;
    while ((other = other->nextChase(&start, &step, &minSpan, &last))) {
        if (other->done()) {
            SkASSERT(!last);
            break;
        }
        other->markDone(minSpan);
    }
    return last;
}

bool SkOpSegment::markAndChaseWinding(SkOpSpanBase* start, SkOpSpanBase* end, int winding,
        SkOpSpanBase** lastPtr) {
    SkOpSpan* spanStart = start->starter(end);
    int step = start->step(end);
    bool success = markWinding(spanStart, winding);
    SkOpSpanBase* last = NULL;
    SkOpSegment* other = this;
    while ((other = other->nextChase(&start, &step, &spanStart, &last))) {
        if (spanStart->windSum() != SK_MinS32) {
            SkASSERT(spanStart->windSum() == winding);
            SkASSERT(!last);
            break;
        }
        (void) other->markWinding(spanStart, winding);
    }
    if (lastPtr) {
        *lastPtr = last;
    }
    return success;
}

bool SkOpSegment::markAndChaseWinding(SkOpSpanBase* start, SkOpSpanBase* end,
        int winding, int oppWinding, SkOpSpanBase** lastPtr) {
    SkOpSpan* spanStart = start->starter(end);
    int step = start->step(end);
    bool success = markWinding(spanStart, winding, oppWinding);
    SkOpSpanBase* last = NULL;
    SkOpSegment* other = this;
    while ((other = other->nextChase(&start, &step, &spanStart, &last))) {
        if (spanStart->windSum() != SK_MinS32) {
            if (this->operand() == other->operand()) {
                SkASSERT(spanStart->windSum() == winding);
                if (spanStart->oppSum() != oppWinding) {
                    this->globalState()->setWindingFailed();
                    return false;
                }
            } else {
                SkASSERT(spanStart->windSum() == oppWinding);
                SkASSERT(spanStart->oppSum() == winding);
            }
            SkASSERT(!last);
            break;
        }
        if (this->operand() == other->operand()) {
            (void) other->markWinding(spanStart, winding, oppWinding);
        } else {
            (void) other->markWinding(spanStart, oppWinding, winding);
        }
    }
    if (lastPtr) {
        *lastPtr = last;
    }
    return success;
}

SkOpSpanBase* SkOpSegment::markAngle(int maxWinding, int sumWinding, const SkOpAngle* angle) {
    SkASSERT(angle->segment() == this);
    if (UseInnerWinding(maxWinding, sumWinding)) {
        maxWinding = sumWinding;
    }
    SkOpSpanBase* last;
    (void) markAndChaseWinding(angle->start(), angle->end(), maxWinding, &last);
#if DEBUG_WINDING
    if (last) {
        SkDebugf("%s last seg=%d span=%d", __FUNCTION__,
                last->segment()->debugID(), last->debugID());
        if (!last->final()) {
            SkDebugf(" windSum=");
            SkPathOpsDebug::WindingPrintf(last->upCast()->windSum());
        }
        SkDebugf("\n");
    }
#endif
    return last;
}

SkOpSpanBase* SkOpSegment::markAngle(int maxWinding, int sumWinding, int oppMaxWinding,
                                   int oppSumWinding, const SkOpAngle* angle) {
    SkASSERT(angle->segment() == this);
    if (UseInnerWinding(maxWinding, sumWinding)) {
        maxWinding = sumWinding;
    }
    if (oppMaxWinding != oppSumWinding && UseInnerWinding(oppMaxWinding, oppSumWinding)) {
        oppMaxWinding = oppSumWinding;
    }
    SkOpSpanBase* last = NULL;
    // caller doesn't require that this marks anything
    (void) markAndChaseWinding(angle->start(), angle->end(), maxWinding, oppMaxWinding, &last);
#if DEBUG_WINDING
    if (last) {
        SkDebugf("%s last segment=%d span=%d", __FUNCTION__,
                last->segment()->debugID(), last->debugID());
        if (!last->final()) {
            SkDebugf(" windSum=");
            SkPathOpsDebug::WindingPrintf(last->upCast()->windSum());
        }
        SkDebugf(" \n");
    }
#endif
    return last;
}

void SkOpSegment::markDone(SkOpSpan* span) {
    SkASSERT(this == span->segment());
    if (span->done()) {
        return;
    }
#if DEBUG_MARK_DONE
    debugShowNewWinding(__FUNCTION__, span, span->windSum(), span->oppSum());
#endif
    span->setDone(true);
    ++fDoneCount;
    debugValidate();
}

bool SkOpSegment::markWinding(SkOpSpan* span, int winding) {
    SkASSERT(this == span->segment());
    SkASSERT(winding);
    if (span->done()) {
        return false;
    }
#if DEBUG_MARK_DONE
    debugShowNewWinding(__FUNCTION__, span, winding);
#endif
    span->setWindSum(winding);
    debugValidate();
    return true;
}

bool SkOpSegment::markWinding(SkOpSpan* span, int winding, int oppWinding) {
    SkASSERT(this == span->segment());
    SkASSERT(winding || oppWinding);
    if (span->done()) {
        return false;
    }
#if DEBUG_MARK_DONE
    debugShowNewWinding(__FUNCTION__, span, winding, oppWinding);
#endif
    span->setWindSum(winding);
    span->setOppSum(oppWinding);
    debugValidate();
    return true;
}

bool SkOpSegment::match(const SkOpPtT* base, const SkOpSegment* testParent, double testT,
        const SkPoint& testPt) const {
    const SkOpSegment* baseParent = base->segment();
    if (this == baseParent && this == testParent && precisely_equal(base->fT, testT)) {
        return true;
    }
    if (!SkDPoint::ApproximatelyEqual(testPt, base->fPt)) {
        return false;
    }
    return !ptsDisjoint(base->fT, base->fPt, testT, testPt);
}

static SkOpSegment* set_last(SkOpSpanBase** last, SkOpSpanBase* endSpan) {
    if (last) {
        *last = endSpan;
    }
    return NULL;
}

bool SkOpSegment::monotonicInY(const SkOpSpanBase* start, const SkOpSpanBase* end) const {
    SkASSERT(fVerb != SkPath::kLine_Verb);
    if (fVerb == SkPath::kQuad_Verb) {
        SkDQuad dst = SkDQuad::SubDivide(fPts, start->t(), end->t());
        return dst.monotonicInY();
    }
    SkASSERT(fVerb == SkPath::kCubic_Verb);
    SkDCubic dst = SkDCubic::SubDivide(fPts, start->t(), end->t());
    return dst.monotonicInY();
}

bool SkOpSegment::NextCandidate(SkOpSpanBase* span, SkOpSpanBase** start,
        SkOpSpanBase** end) {
    while (span->final() || span->upCast()->done()) {
        if (span->final()) {
            return false;
        }
        span = span->upCast()->next();
    }
    *start = span;
    *end = span->upCast()->next();
    return true;
}

SkOpSegment* SkOpSegment::nextChase(SkOpSpanBase** startPtr, int* stepPtr, SkOpSpan** minPtr,
        SkOpSpanBase** last) const {
    SkOpSpanBase* origStart = *startPtr;
    int step = *stepPtr;
    SkOpSpanBase* endSpan = step > 0 ? origStart->upCast()->next() : origStart->prev();
    SkASSERT(endSpan);
    SkOpAngle* angle = step > 0 ? endSpan->fromAngle() : endSpan->upCast()->toAngle();
    SkOpSpanBase* foundSpan;
    SkOpSpanBase* otherEnd;
    SkOpSegment* other;
    if (angle == NULL) {
        if (endSpan->t() != 0 && endSpan->t() != 1) {
            return NULL;
        }
        SkOpPtT* otherPtT = endSpan->ptT()->next();
        other = otherPtT->segment();
        foundSpan = otherPtT->span();
        otherEnd = step > 0 ? foundSpan->upCast()->next() : foundSpan->prev();
    } else {
        int loopCount = angle->loopCount();
        if (loopCount > 2) {
            return set_last(last, endSpan);
        }
        const SkOpAngle* next = angle->next();
        if (NULL == next) {
            return NULL;
        }
#if DEBUG_WINDING
        if (angle->sign() != next->sign() && !angle->segment()->contour()->isXor()
                && !next->segment()->contour()->isXor()) {
            SkDebugf("%s mismatched signs\n", __FUNCTION__);
        }
#endif
        other = next->segment();
        foundSpan = endSpan = next->start();
        otherEnd = next->end();
    }
    int foundStep = foundSpan->step(otherEnd);
    if (*stepPtr != foundStep) {
        return set_last(last, endSpan);
    }
    SkASSERT(*startPtr);
    if (!otherEnd) {
        return NULL;
    }
//    SkASSERT(otherEnd >= 0);
    SkOpSpan* origMin = step < 0 ? origStart->prev() : origStart->upCast();
    SkOpSpan* foundMin = foundSpan->starter(otherEnd);
    if (foundMin->windValue() != origMin->windValue()
            || foundMin->oppValue() != origMin->oppValue()) {
          return set_last(last, endSpan);
    }
    *startPtr = foundSpan;
    *stepPtr = foundStep;
    if (minPtr) {
        *minPtr = foundMin;
    }
    return other;
}

static void clear_visited(SkOpSpan* span) {
    // reset visited flag back to false
    do {
        SkOpPtT* ptT = span->ptT(), * stopPtT = ptT;
        while ((ptT = ptT->next()) != stopPtT) {
            SkOpSegment* opp = ptT->segment();
            opp->resetVisited();
        }
    } while ((span = span->next()->upCastable()));
}

// look for pairs of undetected coincident curves
// assumes that segments going in have visited flag clear
// curve/curve intersection should now do a pretty good job of finding coincident runs so 
// this may be only be necessary for line/curve pairs -- so skip unless this is a line and the
// the opp is not a line
void SkOpSegment::missingCoincidence(SkOpCoincidence* coincidences, SkChunkAlloc* allocator) {
    if (this->verb() != SkPath::kLine_Verb) {
        return;
    }
    SkOpSpan* prior = NULL;
    SkOpSpan* span = &fHead;
    do {
        SkOpPtT* ptT = span->ptT(), * spanStopPtT = ptT;
        SkASSERT(ptT->span() == span);
        while ((ptT = ptT->next()) != spanStopPtT) {
            SkOpSegment* opp = ptT->span()->segment();
            if (opp->setVisited()) {
                continue;
            }
            if (opp->verb() == SkPath::kLine_Verb) {
                continue;
            }
            if (span->containsCoincidence(opp)) { // FIXME: this assumes that if the opposite
                                                  // segment is coincident then no more coincidence
                                                  // needs to be detected. This may not be true. 
                continue;
            }
            if (span->containsCoinEnd(opp)) {
                continue;
            } 
            // if already visited and visited again, check for coin
            if (span == &fHead) {
                continue;
            }
            SkOpPtT* priorPtT = NULL, * priorStopPtT;
            // find prior span containing opp segment
            SkOpSegment* priorOpp = NULL;
            prior = span;
            while (!priorOpp && (prior = prior->prev())) {
                priorStopPtT = priorPtT = prior->ptT();
                while ((priorPtT = priorPtT->next()) != priorStopPtT) {
                    SkOpSegment* segment = priorPtT->span()->segment();
                    if (segment == opp) {
                        priorOpp = opp;
                        break;
                    }
                }
            }
            if (!priorOpp) {
                continue;
            }
            SkOpPtT* oppStart = prior->ptT();
            SkOpPtT* oppEnd = span->ptT();
            bool swapped = priorPtT->fT > ptT->fT;
            if (swapped) {
                SkTSwap(priorPtT, ptT);
                SkTSwap(oppStart, oppEnd);
            }
            bool flipped = oppStart->fT > oppEnd->fT;
            bool coincident;
            if (coincidences->contains(priorPtT, ptT, oppStart, oppEnd, flipped)) {
                goto swapBack;
            }
            {
                // average t, find mid pt
                double midT = (prior->t() + span->t()) / 2;
                SkPoint midPt = this->ptAtT(midT);
                coincident = true;
                // if the mid pt is not near either end pt, project perpendicular through opp seg
                if (!SkDPoint::ApproximatelyEqual(priorPtT->fPt, midPt)
                        && !SkDPoint::ApproximatelyEqual(ptT->fPt, midPt)) {
                    coincident = false;
                    SkIntersections i;
                    int ptCount = SkPathOpsVerbToPoints(this->verb());
                    SkVector dxdy = (*CurveSlopeAtT[ptCount])(pts(), midT);
                    SkDLine ray = {{{midPt.fX, midPt.fY},
                            {midPt.fX + dxdy.fY, midPt.fY - dxdy.fX}}};
                    int oppPtCount = SkPathOpsVerbToPoints(opp->verb());
                    (*CurveIntersectRay[oppPtCount])(opp->pts(), ray, &i);
                    // measure distance and see if it's small enough to denote coincidence
                    for (int index = 0; index < i.used(); ++index) {
                        SkDPoint oppPt = i.pt(index);
                        if (oppPt.approximatelyEqual(midPt)) {
                            SkVector oppDxdy = (*CurveSlopeAtT[oppPtCount])(opp->pts(),
                                    i[index][0]);
                            oppDxdy.normalize();
                            dxdy.normalize();
                            SkScalar flatness = SkScalarAbs(dxdy.cross(oppDxdy) / FLT_EPSILON);
                            coincident |= flatness < 5000;  // FIXME: replace with tuned value
                        }
                    }
                }
            }
            if (coincident) {
            // mark coincidence
                coincidences->add(priorPtT, ptT, oppStart, oppEnd, allocator);
                clear_visited(&fHead);
                missingCoincidence(coincidences, allocator);
                return;
            }
    swapBack:
            if (swapped) {
                SkTSwap(priorPtT, ptT);
            }
        }
    } while ((span = span->next()->upCastable()));
    clear_visited(&fHead);
}

// Move nearby t values and pts so they all hang off the same span. Alignment happens later.
bool SkOpSegment::moveNearby() {
    debugValidate();
    SkOpSpanBase* spanS = &fHead;
    do {
        SkOpSpanBase* test = spanS->upCast()->next();
        SkOpSpanBase* next;
        if (spanS->contains(test)) {
            if (!test->final()) {
                test->upCast()->detach(spanS->ptT());
                continue;
            } else if (spanS != &fHead) {
                spanS->upCast()->detach(test->ptT());
                spanS = test;
                continue;
            }
        }
        do {  // iterate through all spans associated with start
            SkOpPtT* startBase = spanS->ptT();
            next = test->final() ? NULL : test->upCast()->next();
            do {
                SkOpPtT* testBase = test->ptT();
                do {
                    if (startBase == testBase) {
                        goto checkNextSpan;
                    }
                    if (testBase->duplicate()) {
                        continue;
                    }
                    if (this->match(startBase, testBase->segment(), testBase->fT, testBase->fPt)) {
                        if (test == &this->fTail) {
                            if (spanS == &fHead) {
                                debugValidate();
                                return true;  // if this span has collapsed, remove it from parent
                            }
                            this->fTail.merge(spanS->upCast());
                            debugValidate();
                            return true;
                        }
                        spanS->merge(test->upCast());
                        spanS->upCast()->setNext(next);
                        goto checkNextSpan;
                    }
                } while ((testBase = testBase->next()) != test->ptT());
            } while ((startBase = startBase->next()) != spanS->ptT());
    checkNextSpan:
            ;
        } while ((test = next));
        spanS = spanS->upCast()->next();
    } while (!spanS->final());
    debugValidate();
    return true;
}

bool SkOpSegment::operand() const {
    return fContour->operand();
}

bool SkOpSegment::oppXor() const {
    return fContour->oppXor();
}

bool SkOpSegment::ptsDisjoint(double t1, const SkPoint& pt1, double t2, const SkPoint& pt2) const {
    if (fVerb == SkPath::kLine_Verb) {
        return false;
    }
    // quads (and cubics) can loop back to nearly a line so that an opposite curve
    // hits in two places with very different t values.
    // OPTIMIZATION: curves could be preflighted so that, for example, something like
    // 'controls contained by ends' could avoid this check for common curves
    // 'ends are extremes in x or y' is cheaper to compute and real-world common
    // on the other hand, the below check is relatively inexpensive
    double midT = (t1 + t2) / 2;
    SkPoint midPt = this->ptAtT(midT);
    double seDistSq = SkTMax(pt1.distanceToSqd(pt2) * 2, FLT_EPSILON * 2);
    return midPt.distanceToSqd(pt1) > seDistSq || midPt.distanceToSqd(pt2) > seDistSq;
}

void SkOpSegment::setUpWindings(SkOpSpanBase* start, SkOpSpanBase* end, int* sumMiWinding,
        int* maxWinding, int* sumWinding) {
    int deltaSum = SpanSign(start, end);
    *maxWinding = *sumMiWinding;
    *sumWinding = *sumMiWinding -= deltaSum;
    SkASSERT(!DEBUG_LIMIT_WIND_SUM || abs(*sumWinding) <= DEBUG_LIMIT_WIND_SUM);
}

void SkOpSegment::setUpWindings(SkOpSpanBase* start, SkOpSpanBase* end, int* sumMiWinding,
        int* sumSuWinding, int* maxWinding, int* sumWinding, int* oppMaxWinding,
        int* oppSumWinding) {
    int deltaSum = SpanSign(start, end);
    int oppDeltaSum = OppSign(start, end);
    if (operand()) {
        *maxWinding = *sumSuWinding;
        *sumWinding = *sumSuWinding -= deltaSum;
        *oppMaxWinding = *sumMiWinding;
        *oppSumWinding = *sumMiWinding -= oppDeltaSum;
    } else {
        *maxWinding = *sumMiWinding;
        *sumWinding = *sumMiWinding -= deltaSum;
        *oppMaxWinding = *sumSuWinding;
        *oppSumWinding = *sumSuWinding -= oppDeltaSum;
    }
    SkASSERT(!DEBUG_LIMIT_WIND_SUM || abs(*sumWinding) <= DEBUG_LIMIT_WIND_SUM);
    SkASSERT(!DEBUG_LIMIT_WIND_SUM || abs(*oppSumWinding) <= DEBUG_LIMIT_WIND_SUM);
}

void SkOpSegment::sortAngles() {
    SkOpSpanBase* span = &this->fHead;
    do {
        SkOpAngle* fromAngle = span->fromAngle();
        SkOpAngle* toAngle = span->final() ? NULL : span->upCast()->toAngle();
        if (!fromAngle && !toAngle) {
            continue;
        }
#if DEBUG_ANGLE
        bool wroteAfterHeader = false;
#endif
        SkOpAngle* baseAngle = fromAngle;
        if (fromAngle && toAngle) {
#if DEBUG_ANGLE
            SkDebugf("%s [%d] tStart=%1.9g [%d]\n", __FUNCTION__, debugID(), span->t(),
                    span->debugID());
            wroteAfterHeader = true;
#endif
            fromAngle->insert(toAngle);
        } else if (!fromAngle) {
            baseAngle = toAngle;
        }
        SkOpPtT* ptT = span->ptT(), * stopPtT = ptT;
        do {
            SkOpSpanBase* oSpan = ptT->span();
            if (oSpan == span) {
                continue;
            }
            SkOpAngle* oAngle = oSpan->fromAngle();
            if (oAngle) {
#if DEBUG_ANGLE
                if (!wroteAfterHeader) {
                    SkDebugf("%s [%d] tStart=%1.9g [%d]\n", __FUNCTION__, debugID(),
                            span->t(), span->debugID());
                    wroteAfterHeader = true;
                }
#endif
                if (!oAngle->loopContains(baseAngle)) {
                    baseAngle->insert(oAngle);
                }
            }
            if (!oSpan->final()) {
                oAngle = oSpan->upCast()->toAngle();
                if (oAngle) {
#if DEBUG_ANGLE
                    if (!wroteAfterHeader) {
                        SkDebugf("%s [%d] tStart=%1.9g [%d]\n", __FUNCTION__, debugID(),
                                span->t(), span->debugID());
                        wroteAfterHeader = true;
                    }
#endif
                    if (!oAngle->loopContains(baseAngle)) {
                        baseAngle->insert(oAngle);
                    }
                }
            }
        } while ((ptT = ptT->next()) != stopPtT);
        if (baseAngle->loopCount() == 1) {
            span->setFromAngle(NULL);
            if (toAngle) {
                span->upCast()->setToAngle(NULL);
            }
            baseAngle = NULL;
        }
#if DEBUG_SORT
        SkASSERT(!baseAngle || baseAngle->loopCount() > 1);
#endif
    } while (!span->final() && (span = span->upCast()->next()));
}

// return true if midpoints were computed
bool SkOpSegment::subDivide(const SkOpSpanBase* start, const SkOpSpanBase* end,
        SkPoint edge[4]) const {
    SkASSERT(start != end);
    const SkOpPtT& startPtT = *start->ptT();
    const SkOpPtT& endPtT = *end->ptT();
    edge[0] = startPtT.fPt;
    int points = SkPathOpsVerbToPoints(fVerb);
    edge[points] = endPtT.fPt;
    if (fVerb == SkPath::kLine_Verb) {
        return false;
    }
    double startT = startPtT.fT;
    double endT = endPtT.fT;
    if ((startT == 0 || endT == 0) && (startT == 1 || endT == 1)) {
        // don't compute midpoints if we already have them
        if (fVerb == SkPath::kQuad_Verb) {
            edge[1] = fPts[1];
            return false;
        }
        SkASSERT(fVerb == SkPath::kCubic_Verb);
        if (start < end) {
            edge[1] = fPts[1];
            edge[2] = fPts[2];
            return false;
        }
        edge[1] = fPts[2];
        edge[2] = fPts[1];
        return false;
    }
    const SkDPoint sub[2] = {{ edge[0].fX, edge[0].fY}, {edge[points].fX, edge[points].fY }};
    if (fVerb == SkPath::kQuad_Verb) {
        edge[1] = SkDQuad::SubDivide(fPts, sub[0], sub[1], startT, endT).asSkPoint();
    } else {
        SkASSERT(fVerb == SkPath::kCubic_Verb);
        SkDPoint ctrl[2];
        SkDCubic::SubDivide(fPts, sub[0], sub[1], startT, endT, ctrl);
        edge[1] = ctrl[0].asSkPoint();
        edge[2] = ctrl[1].asSkPoint();
    }
    return true;
}

bool SkOpSegment::subDivide(const SkOpSpanBase* start, const SkOpSpanBase* end,
        SkDCubic* result) const {
    SkASSERT(start != end);
    const SkOpPtT& startPtT = *start->ptT();
    const SkOpPtT& endPtT = *end->ptT();
    (*result)[0].set(startPtT.fPt);
    int points = SkPathOpsVerbToPoints(fVerb);
    (*result)[points].set(endPtT.fPt);
    if (fVerb == SkPath::kLine_Verb) {
        return false;
    }
    double startT = startPtT.fT;
    double endT = endPtT.fT;
    if ((startT == 0 || endT == 0) && (startT == 1 || endT == 1)) {
        // don't compute midpoints if we already have them
        if (fVerb == SkPath::kQuad_Verb) {
            (*result)[1].set(fPts[1]);
            return false;
        }
        SkASSERT(fVerb == SkPath::kCubic_Verb);
        if (startT == 0) {
            (*result)[1].set(fPts[1]);
            (*result)[2].set(fPts[2]);
            return false;
        }
        (*result)[1].set(fPts[2]);
        (*result)[2].set(fPts[1]);
        return false;
    }
    if (fVerb == SkPath::kQuad_Verb) {
        (*result)[1] = SkDQuad::SubDivide(fPts, (*result)[0], (*result)[2], startT, endT);
    } else {
        SkASSERT(fVerb == SkPath::kCubic_Verb);
        SkDCubic::SubDivide(fPts, (*result)[0], (*result)[3], startT, endT, &(*result)[1]);
    }
    return true;
}

void SkOpSegment::subDivideBounds(const SkOpSpanBase* start, const SkOpSpanBase* end,
        SkPathOpsBounds* bounds) const {
    SkPoint edge[4];
    subDivide(start, end, edge);
    (bounds->*SetCurveBounds[SkPathOpsVerbToPoints(fVerb)])(edge);
}

void SkOpSegment::undoneSpan(SkOpSpanBase** start, SkOpSpanBase** end) {
    SkOpSpan* span = this->head();
    do {
        if (!span->done()) {
            break;
        }
    } while ((span = span->next()->upCastable()));
    SkASSERT(span);
    *start = span;
    *end = span->next();
}

int SkOpSegment::updateOppWinding(const SkOpSpanBase* start, const SkOpSpanBase* end) const {
    const SkOpSpan* lesser = start->starter(end);
    int oppWinding = lesser->oppSum();
    int oppSpanWinding = SkOpSegment::OppSign(start, end);
    if (oppSpanWinding && UseInnerWinding(oppWinding - oppSpanWinding, oppWinding)
            && oppWinding != SK_MaxS32) {
        oppWinding -= oppSpanWinding;
    }
    return oppWinding;
}

int SkOpSegment::updateOppWinding(const SkOpAngle* angle) const {
    const SkOpSpanBase* startSpan = angle->start();
    const SkOpSpanBase* endSpan = angle->end();
    return updateOppWinding(endSpan, startSpan);
}

int SkOpSegment::updateOppWindingReverse(const SkOpAngle* angle) const {
    const SkOpSpanBase* startSpan = angle->start();
    const SkOpSpanBase* endSpan = angle->end();
    return updateOppWinding(startSpan, endSpan);
}

int SkOpSegment::updateWinding(const SkOpSpanBase* start, const SkOpSpanBase* end) const {
    const SkOpSpan* lesser = start->starter(end);
    int winding = lesser->windSum();
    if (winding == SK_MinS32) {
        return winding;
    }
    int spanWinding = SkOpSegment::SpanSign(start, end);
    if (winding && UseInnerWinding(winding - spanWinding, winding)
            && winding != SK_MaxS32) {
        winding -= spanWinding;
    }
    return winding;
}

int SkOpSegment::updateWinding(const SkOpAngle* angle) const {
    const SkOpSpanBase* startSpan = angle->start();
    const SkOpSpanBase* endSpan = angle->end();
    return updateWinding(endSpan, startSpan);
}

int SkOpSegment::updateWindingReverse(const SkOpAngle* angle) const {
    const SkOpSpanBase* startSpan = angle->start();
    const SkOpSpanBase* endSpan = angle->end();
    return updateWinding(startSpan, endSpan);
}

// OPTIMIZATION: does the following also work, and is it any faster?
// return outerWinding * innerWinding > 0
//      || ((outerWinding + innerWinding < 0) ^ ((outerWinding - innerWinding) < 0)))
bool SkOpSegment::UseInnerWinding(int outerWinding, int innerWinding) {
    SkASSERT(outerWinding != SK_MaxS32);
    SkASSERT(innerWinding != SK_MaxS32);
    int absOut = abs(outerWinding);
    int absIn = abs(innerWinding);
    bool result = absOut == absIn ? outerWinding < 0 : absOut < absIn;
    return result;
}

int SkOpSegment::windingAtT(double tHit, const SkOpSpan* span, bool crossOpp,
        SkScalar* dx) const {
    if (approximately_zero(tHit - span->t())) {  // if we hit the end of a span, disregard
        return SK_MinS32;
    }
    int winding = crossOpp ? span->oppSum() : span->windSum();
    SkASSERT(winding != SK_MinS32);
    int windVal = crossOpp ? span->oppValue() : span->windValue();
#if DEBUG_WINDING_AT_T
    SkDebugf("%s id=%d opp=%d tHit=%1.9g t=%1.9g oldWinding=%d windValue=%d", __FUNCTION__,
            debugID(), crossOpp, tHit, span->t(), winding, windVal);
#endif
    // see if a + change in T results in a +/- change in X (compute x'(T))
    *dx = (*CurveSlopeAtT[SkPathOpsVerbToPoints(fVerb)])(fPts, tHit).fX;
    if (fVerb > SkPath::kLine_Verb && approximately_zero(*dx)) {
        *dx = fPts[2].fX - fPts[1].fX - *dx;
    }
    if (*dx == 0) {
#if DEBUG_WINDING_AT_T
        SkDebugf(" dx=0 winding=SK_MinS32\n");
#endif
        return SK_MinS32;
    }
    if (windVal < 0) {  // reverse sign if opp contour traveled in reverse
            *dx = -*dx;
    }
    if (winding * *dx > 0) {  // if same signs, result is negative
        winding += *dx > 0 ? -windVal : windVal;
    }
#if DEBUG_WINDING_AT_T
    SkDebugf(" dx=%c winding=%d\n", *dx > 0 ? '+' : '-', winding);
#endif
    return winding;
}

int SkOpSegment::windSum(const SkOpAngle* angle) const {
    const SkOpSpan* minSpan = angle->start()->starter(angle->end());
    return minSpan->windSum();
}
