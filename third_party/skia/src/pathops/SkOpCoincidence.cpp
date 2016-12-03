/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "SkOpCoincidence.h"
#include "SkOpSegment.h"
#include "SkPathOpsTSect.h"

#if DEBUG_COINCIDENCE
#define FAIL_IF(cond) SkASSERT(!(cond))
#else
#define FAIL_IF(cond) do { if (cond) return false; } while (false)
#endif

// returns true if coincident span's start and end are the same
bool SkCoincidentSpans::collapsed(const SkOpPtT* test) const {
    return (fCoinPtTStart == test && fCoinPtTEnd->contains(test))
        || (fCoinPtTEnd == test && fCoinPtTStart->contains(test))
        || (fOppPtTStart == test && fOppPtTEnd->contains(test))
        || (fOppPtTEnd == test && fOppPtTStart->contains(test));
}

// sets the span's end to the ptT referenced by the previous-next
void SkCoincidentSpans::correctOneEnd(
        const SkOpPtT* (SkCoincidentSpans::* getEnd)() const,
        void (SkCoincidentSpans::*setEnd)(const SkOpPtT* ptT) ) {
    const SkOpPtT* origPtT = (this->*getEnd)();
    const SkOpSpanBase* origSpan = origPtT->span();
    const SkOpSpan* prev = origSpan->prev();
    const SkOpPtT* testPtT = prev ? prev->next()->ptT()
            : origSpan->upCast()->next()->prev()->ptT();
    if (origPtT != testPtT) {
        (this->*setEnd)(testPtT);
    }
}

// FIXME: member pointers have fallen out of favor and can be replaced with
// an alternative approach.
// makes all span ends agree with the segment's spans that define them
void SkCoincidentSpans::correctEnds() {
    this->correctOneEnd(&SkCoincidentSpans::coinPtTStart, &SkCoincidentSpans::setCoinPtTStart);
    this->correctOneEnd(&SkCoincidentSpans::coinPtTEnd, &SkCoincidentSpans::setCoinPtTEnd);
    this->correctOneEnd(&SkCoincidentSpans::oppPtTStart, &SkCoincidentSpans::setOppPtTStart);
    this->correctOneEnd(&SkCoincidentSpans::oppPtTEnd, &SkCoincidentSpans::setOppPtTEnd);
}

/* Please keep this in sync with debugExpand */
// expand the range by checking adjacent spans for coincidence
bool SkCoincidentSpans::expand() {
    bool expanded = false;
    const SkOpSegment* segment = coinPtTStart()->segment();
    const SkOpSegment* oppSegment = oppPtTStart()->segment();
    do {
        const SkOpSpan* start = coinPtTStart()->span()->upCast();
        const SkOpSpan* prev = start->prev();
        const SkOpPtT* oppPtT;
        if (!prev || !(oppPtT = prev->contains(oppSegment))) {
            break;
        }
        double midT = (prev->t() + start->t()) / 2;
        if (!segment->isClose(midT, oppSegment)) {
            break;
        }
        setStarts(prev->ptT(), oppPtT);
        expanded = true;
    } while (true);
    do {
        const SkOpSpanBase* end = coinPtTEnd()->span();
        SkOpSpanBase* next = end->final() ? nullptr : end->upCast()->next();
        if (next && next->deleted()) {
            break;
        }
        const SkOpPtT* oppPtT;
        if (!next || !(oppPtT = next->contains(oppSegment))) {
            break;
        }
        double midT = (end->t() + next->t()) / 2;
        if (!segment->isClose(midT, oppSegment)) {
            break;
        }
        setEnds(next->ptT(), oppPtT);
        expanded = true;
    } while (true);
    return expanded;
}

// increase the range of this span
bool SkCoincidentSpans::extend(const SkOpPtT* coinPtTStart, const SkOpPtT* coinPtTEnd,
        const SkOpPtT* oppPtTStart, const SkOpPtT* oppPtTEnd) {
    bool result = false;
    if (fCoinPtTStart->fT > coinPtTStart->fT || (this->flipped()
            ? fOppPtTStart->fT < oppPtTStart->fT : fOppPtTStart->fT > oppPtTStart->fT)) {
        this->setStarts(coinPtTStart, oppPtTStart);
        result = true;
    }
    if (fCoinPtTEnd->fT < coinPtTEnd->fT || (this->flipped()
            ? fOppPtTEnd->fT > oppPtTEnd->fT : fOppPtTEnd->fT < oppPtTEnd->fT)) {
        this->setEnds(coinPtTEnd, oppPtTEnd);
        result = true;
    }
    return result;
}

// set the range of this span
void SkCoincidentSpans::set(SkCoincidentSpans* next, const SkOpPtT* coinPtTStart,
        const SkOpPtT* coinPtTEnd, const SkOpPtT* oppPtTStart, const SkOpPtT* oppPtTEnd
        SkDEBUGPARAMS(int id)) {
    SkASSERT(SkOpCoincidence::Ordered(coinPtTStart, oppPtTStart));
    fNext = next;
    this->setStarts(coinPtTStart, oppPtTStart);
    this->setEnds(coinPtTEnd, oppPtTEnd);
    SkDEBUGCODE(fID = id);
}

// returns true if both points are inside this
bool SkCoincidentSpans::contains(const SkOpPtT* s, const SkOpPtT* e) const {
    if (s->fT > e->fT) {
        SkTSwap(s, e);
    }
    if (s->segment() == fCoinPtTStart->segment()) {
        return fCoinPtTStart->fT <= s->fT && e->fT <= fCoinPtTEnd->fT;
    } else {
        SkASSERT(s->segment() == fOppPtTStart->segment());
        double oppTs = fOppPtTStart->fT;
        double oppTe = fOppPtTEnd->fT;
        if (oppTs > oppTe) {
            SkTSwap(oppTs, oppTe);
        }
        return oppTs <= s->fT && e->fT <= oppTe;
    }
}

// returns the number of segment span's contained by this, or -1 if inconsistent
int SkCoincidentSpans::spanCount() const {
    // most commonly, concidence are one span long; check for that first
    const SkOpSpanBase* start = coinPtTStart()->span();
    const SkOpSpanBase* end = coinPtTEnd()->span();
    int coinIntervals = 0;
    while (start != end) {
        coinIntervals++;
        start = start->upCast()->next();
    }
    const SkOpSpanBase* oppStart = (flipped() ? oppPtTEnd() : oppPtTStart())->span();
    const SkOpSpanBase* oppEnd = (flipped() ? oppPtTStart() : oppPtTEnd())->span();
    int oppIntervals = 0;
    while (oppStart != oppEnd) {
        oppIntervals++;
        oppStart = oppStart->upCast()->next();
    }
    return coinIntervals == oppIntervals ? coinIntervals : -1;
}

// returns true if the point is on a coincident edge, and if it is the start of that edge
bool SkOpCoincidence::edge(const SkOpPtT* test, bool* start) const {
    SkCoincidentSpans* coinRec = fHead;
    if (!coinRec) {
        return false;
    }
    do {
        if (coinRec->coinPtTStart() == test) {
            *start = true;
            return true;
        }
        if (coinRec->coinPtTEnd() == test) {
            *start = false;
            return true;
        }
        if (coinRec->oppPtTStart() == test) {
            *start = !coinRec->flipped();
            return true;
        }
        if (coinRec->coinPtTEnd() == test) {
            *start = coinRec->flipped();
            return true;
        }
    } while ((coinRec = coinRec->next()));
    return false;
}

// if there is an existing pair that overlaps the addition, extend it
bool SkOpCoincidence::extend(const SkOpPtT* coinPtTStart, const SkOpPtT* coinPtTEnd,
        const SkOpPtT* oppPtTStart, const SkOpPtT* oppPtTEnd) {
    SkCoincidentSpans* test = fHead;
    if (!test) {
        return false;
    }
    const SkOpSegment* coinSeg = coinPtTStart->segment();
    const SkOpSegment* oppSeg = oppPtTStart->segment();
    if (!Ordered(coinPtTStart, oppPtTStart)) {
        SkTSwap(coinSeg, oppSeg);
        SkTSwap(coinPtTStart, oppPtTStart);
        SkTSwap(coinPtTEnd, oppPtTEnd);
        if (coinPtTStart->fT > coinPtTEnd->fT) {
            SkTSwap(coinPtTStart, coinPtTEnd);
            SkTSwap(oppPtTStart, oppPtTEnd);
        }
    }
    double oppMinT = SkTMin(oppPtTStart->fT, oppPtTEnd->fT);
    SkDEBUGCODE(double oppMaxT = SkTMax(oppPtTStart->fT, oppPtTEnd->fT));
    do {
        if (coinSeg != test->coinPtTStart()->segment()) {
            continue;
        }
        if (oppSeg != test->oppPtTStart()->segment()) {
            continue;
        }
        double oTestMinT = SkTMin(test->oppPtTStart()->fT, test->oppPtTEnd()->fT);
        double oTestMaxT = SkTMax(test->oppPtTStart()->fT, test->oppPtTEnd()->fT);
        // if debug check triggers, caller failed to check if extended already exists
        SkASSERT(test->coinPtTStart()->fT > coinPtTStart->fT
                || coinPtTEnd->fT > test->coinPtTEnd()->fT
                || oTestMinT > oppMinT || oppMaxT > oTestMaxT);
        if ((test->coinPtTStart()->fT <= coinPtTEnd->fT
                && coinPtTStart->fT <= test->coinPtTEnd()->fT)
                || (oTestMinT <= oTestMaxT && oppMinT <= oTestMaxT)) {
            test->extend(coinPtTStart, coinPtTEnd, oppPtTStart, oppPtTEnd);
            return true;
        }
    } while ((test = test->next()));
    return false;
}

// verifies that the coincidence hasn't already been added
static void DebugCheckAdd(const SkCoincidentSpans* check, const SkOpPtT* coinPtTStart,
        const SkOpPtT* coinPtTEnd, const SkOpPtT* oppPtTStart, const SkOpPtT* oppPtTEnd) {
#if DEBUG_COINCIDENCE
    while (check) {
        SkASSERT(check->coinPtTStart() != coinPtTStart || check->coinPtTEnd() != coinPtTEnd
                || check->oppPtTStart() != oppPtTStart || check->oppPtTEnd() != oppPtTEnd);
        SkASSERT(check->coinPtTStart() != oppPtTStart || check->coinPtTEnd() != oppPtTEnd
                || check->oppPtTStart() != coinPtTStart || check->oppPtTEnd() != coinPtTEnd);
        check = check->next();
    }
#endif
}

// adds a new coincident pair
void SkOpCoincidence::add(SkOpPtT* coinPtTStart, SkOpPtT* coinPtTEnd, SkOpPtT* oppPtTStart,
        SkOpPtT* oppPtTEnd) {
    // OPTIMIZE: caller should have already sorted
    if (!Ordered(coinPtTStart, oppPtTStart)) {
        if (oppPtTStart->fT < oppPtTEnd->fT) {
            this->add(oppPtTStart, oppPtTEnd, coinPtTStart, coinPtTEnd);
        } else {
            this->add(oppPtTEnd, oppPtTStart, coinPtTEnd, coinPtTStart);
        }
        return;
    }
    SkASSERT(Ordered(coinPtTStart, oppPtTStart));
    // choose the ptT at the front of the list to track
    coinPtTStart = coinPtTStart->span()->ptT();
    coinPtTEnd = coinPtTEnd->span()->ptT();
    oppPtTStart = oppPtTStart->span()->ptT();
    oppPtTEnd = oppPtTEnd->span()->ptT();
    SkASSERT(coinPtTStart->fT < coinPtTEnd->fT);
    SkASSERT(oppPtTStart->fT != oppPtTEnd->fT);
    SkASSERT(!coinPtTStart->deleted());
    SkASSERT(!coinPtTEnd->deleted());
    SkASSERT(!oppPtTStart->deleted());
    SkASSERT(!oppPtTEnd->deleted());
    DebugCheckAdd(fHead, coinPtTStart, coinPtTEnd, oppPtTStart, oppPtTEnd);
    DebugCheckAdd(fTop, coinPtTStart, coinPtTEnd, oppPtTStart, oppPtTEnd);
    SkCoincidentSpans* coinRec = SkOpTAllocator<SkCoincidentSpans>::Allocate(
            this->globalState()->allocator());
    coinRec->init(SkDEBUGCODE(fGlobalState));
    coinRec->set(this->fHead, coinPtTStart, coinPtTEnd, oppPtTStart, oppPtTEnd
            SkDEBUGPARAMS(fGlobalState->nextCoinID()));
    fHead = coinRec;
}

// description below
bool SkOpCoincidence::addEndMovedSpans(const SkOpSpan* base, const SkOpSpanBase* testSpan) {
    const SkOpPtT* testPtT = testSpan->ptT();
    const SkOpPtT* stopPtT = testPtT;
    const SkOpSegment* baseSeg = base->segment();
    while ((testPtT = testPtT->next()) != stopPtT) {
        const SkOpSegment* testSeg = testPtT->segment();
        if (testPtT->deleted()) {
            continue;
        }
        if (testSeg == baseSeg) {
            continue;
        }
        if (testPtT->span()->ptT() != testPtT) {
            continue;
        }
        if (this->contains(baseSeg, testSeg, testPtT->fT)) {
            continue;
        }
        // intersect perp with base->ptT() with testPtT->segment()
        SkDVector dxdy = baseSeg->dSlopeAtT(base->t());
        const SkPoint& pt = base->pt();
        SkDLine ray = {{{pt.fX, pt.fY}, {pt.fX + dxdy.fY, pt.fY - dxdy.fX}}};
        SkIntersections i;
        (*CurveIntersectRay[testSeg->verb()])(testSeg->pts(), testSeg->weight(), ray, &i);
        for (int index = 0; index < i.used(); ++index) {
            double t = i[0][index];
            if (!between(0, t, 1)) {
                continue;
            }
            SkDPoint oppPt = i.pt(index);
            if (!oppPt.approximatelyEqual(pt)) {
                continue;
            }
            SkOpSegment* writableSeg = const_cast<SkOpSegment*>(testSeg);
            SkOpPtT* oppStart = writableSeg->addT(t, nullptr);
            SkOpSpan* writableBase = const_cast<SkOpSpan*>(base);
            oppStart->span()->addOppAndMerge(writableBase);
            if (oppStart->deleted()) {
                continue;
            }
            SkOpSegment* coinSeg = base->segment();
            SkOpSegment* oppSeg = oppStart->segment();
            double coinTs, coinTe, oppTs, oppTe;
            if (coinSeg < oppSeg) {
                coinTs = base->t();
                coinTe = testSpan->t();
                oppTs = oppStart->fT;
                oppTe = testPtT->fT;
            } else {
                SkTSwap(coinSeg, oppSeg);
                coinTs = oppStart->fT;
                coinTe = testPtT->fT;
                oppTs = base->t();
                oppTe = testSpan->t();
            }
            if (coinTs > coinTe) {
                SkTSwap(coinTs, coinTe);
                SkTSwap(oppTs, oppTe);
            }
            if (!this->addOrOverlap(coinSeg, oppSeg, coinTs, coinTe, oppTs, oppTe)) {
                return false;
            }
        }
    }
    return true;
}

// description below
bool SkOpCoincidence::addEndMovedSpans(const SkOpPtT* ptT) {
    if (!ptT->span()->upCastable()) {
        return false;
    }
    const SkOpSpan* base = ptT->span()->upCast();
    const SkOpSpan* prev = base->prev();
    if (!prev) {
        return false;
    }
    if (!prev->isCanceled()) {
        if (!this->addEndMovedSpans(base, base->prev())) {
            return false;
        }
    }
    if (!base->isCanceled()) {
        if (!this->addEndMovedSpans(base, base->next())) {
            return false;
        }
    }
    return true;
}

/*  If A is coincident with B and B includes an endpoint, and A's matching point
    is not the endpoint (i.e., there's an implied line connecting B-end and A)
    then assume that the same implied line may intersect another curve close to B.
    Since we only care about coincidence that was undetected, look at the
    ptT list on B-segment adjacent to the B-end/A ptT loop (not in the loop, but
    next door) and see if the A matching point is close enough to form another
    coincident pair. If so, check for a new coincident span between B-end/A ptT loop
    and the adjacent ptT loop.
*/
bool SkOpCoincidence::addEndMovedSpans() {
    SkCoincidentSpans* span = fHead;
    if (!span) {
        return true;
    }
    fTop = span;
    fHead = nullptr;
    do {
        if (span->coinPtTStart()->fPt != span->oppPtTStart()->fPt) {
            if (1 == span->coinPtTStart()->fT) {
                return false;
            }
            bool onEnd = span->coinPtTStart()->fT == 0;
            bool oOnEnd = zero_or_one(span->oppPtTStart()->fT);
            if (onEnd) {
                if (!oOnEnd) {  // if both are on end, any nearby intersect was already found
                    if (!this->addEndMovedSpans(span->oppPtTStart())) {
                        return false;
                    }
                }
            } else if (oOnEnd) {
                if (!this->addEndMovedSpans(span->coinPtTStart())) {
                    return false;
                }
            }
        }
        if (span->coinPtTEnd()->fPt != span->oppPtTEnd()->fPt) {
            bool onEnd = span->coinPtTEnd()->fT == 1;
            bool oOnEnd = zero_or_one(span->oppPtTEnd()->fT);
            if (onEnd) {
                if (!oOnEnd) {
                    if (!this->addEndMovedSpans(span->oppPtTEnd())) {
                        return false;
                    }
                }
            } else if (oOnEnd) {
                if (!this->addEndMovedSpans(span->coinPtTEnd())) {
                    return false;
                }
            }
        }
    } while ((span = span->next()));
    this->restoreHead();
    return true;
}

/* Please keep this in sync with debugAddExpanded */
// for each coincident pair, match the spans
// if the spans don't match, add the missing pt to the segment and loop it in the opposite span
bool SkOpCoincidence::addExpanded() {
    SkCoincidentSpans* coin = this->fHead;
    if (!coin) {
        return true;
    }
    do {
        const SkOpPtT* startPtT = coin->coinPtTStart();
        const SkOpPtT* oStartPtT = coin->oppPtTStart();
        SkASSERT(startPtT->contains(oStartPtT));
        SkOPASSERT(coin->coinPtTEnd()->contains(coin->oppPtTEnd()));
        const SkOpSpanBase* start = startPtT->span();
        const SkOpSpanBase* oStart = oStartPtT->span();
        const SkOpSpanBase* end = coin->coinPtTEnd()->span();
        const SkOpSpanBase* oEnd = coin->oppPtTEnd()->span();
        FAIL_IF(oEnd->deleted());
        FAIL_IF(!start->upCastable());
        const SkOpSpanBase* test = start->upCast()->next();
        const SkOpSpanBase* oTest = coin->flipped() ? oStart->prev() : oStart->upCast()->next();
        if (!oTest) {
            return false;
        }
        while (test != end || oTest != oEnd) {
            if (!test->ptT()->contains(oStart->segment())
                    || !oTest->ptT()->contains(start->segment())) {
                // use t ranges to guess which one is missing
                double startRange = coin->coinPtTEnd()->fT - startPtT->fT;
                FAIL_IF(!startRange);
                double startPart = (test->t() - startPtT->fT) / startRange;
                double oStartRange = coin->oppPtTEnd()->fT - oStartPtT->fT;
                FAIL_IF(!oStartRange);
                double oStartPart = (oTest->t() - oStartPtT->fT) / oStartRange;
                FAIL_IF(startPart == oStartPart);
                bool startOver = false;
                bool success = startPart < oStartPart
                        ? oStart->segment()->addExpanded(
                                oStartPtT->fT + oStartRange * startPart, test, &startOver)
                        : start->segment()->addExpanded(
                                startPtT->fT + startRange * oStartPart, oTest, &startOver);
                if (!success) {
                    SkOPASSERT(false);
                    return false;
                }
                if (startOver) {
                    test = start;
                    oTest = oStart;
                }
            }
            if (test != end) {
                if (!test->upCastable()) {
                    return false;
                }
                test = test->upCast()->next();
            }
            if (oTest != oEnd) {
                oTest = coin->flipped() ? oTest->prev() : oTest->upCast()->next();
                if (!oTest) {
                    return false;
                }
            }
        }
    } while ((coin = coin->next()));
    return true;
}

// checks to see if coincidence has already been found
bool SkOpCoincidence::alreadyAdded(const SkCoincidentSpans* check, const SkCoincidentSpans* outer,
        const SkOpPtT* over1s, const SkOpPtT* over1e) const {
    do {
        if (check->oppPtTStart() == outer->coinPtTStart() && check->coinPtTStart() == over1s
                && check->oppPtTEnd() == outer->coinPtTEnd() && check->coinPtTEnd() == over1e) {
            return true;
        }
        if (check->coinPtTStart() == outer->coinPtTStart() && check->oppPtTStart() == over1s
                && check->coinPtTEnd() == outer->coinPtTEnd() && check->oppPtTEnd() == over1e) {
            return true;
        }
        if (check->startEquals(outer->oppPtTStart()->span(), over1s->span())) {
            SkDEBUGCODE(check->debugStartCheck(outer->oppPtTEnd()->span(), over1e->span(),
                    fGlobalState));
            return true;
        }
        if (check->startEquals(over1s->span(), outer->coinPtTStart()->span())) {
            SkDEBUGCODE(check->debugStartCheck(over1e->span(), outer->oppPtTEnd()->span(),
                    fGlobalState));
            return true;
        }
    } while ((check = check->next()));
    return false;
}

    /* Please keep this in sync with debugAddIfMissing() */
bool SkOpCoincidence::addIfMissing(const SkCoincidentSpans* outer, SkOpPtT* over1s,
            SkOpPtT* over1e) {
    SkASSERT(fTop);
    if (this->alreadyAdded(fTop, outer, over1s, over1e)) {
        return false;
    }
    if (fHead && this->alreadyAdded(fHead, outer, over1s, over1e)) {
        return false;
    }
    this->add(outer->coinPtTStart(), outer->coinPtTEnd(), over1s, over1e);
    this->debugValidate();
    return true;
}

// given a t span, map the same range on the coincident span
void SkOpCoincidence::TRange(const SkOpPtT* overS, const SkOpPtT* overE, double tStart,
        double tEnd, const SkOpPtT* coinPtTStart, const SkOpPtT* coinPtTEnd, double* coinTs,
        double* coinTe) {
    double denom = overE->fT - overS->fT;
    double start = 0 < denom ? tStart : tEnd;
    double end = 0 < denom ? tEnd : tStart;
    double sRatio = (start - overS->fT) / denom;
    double eRatio = (end - overS->fT) / denom;
    *coinTs = coinPtTStart->fT + (coinPtTEnd->fT - coinPtTStart->fT) * sRatio;
    *coinTe = coinPtTStart->fT + (coinPtTEnd->fT - coinPtTStart->fT) * eRatio;
}

// return true if span overlaps existing and needs to adjust the coincident list
bool SkOpCoincidence::checkOverlap(SkCoincidentSpans* check,
        const SkOpSegment* coinSeg, const SkOpSegment* oppSeg,
        double coinTs, double coinTe, double oppTs, double oppTe,
        SkTDArray<SkCoincidentSpans*>* overlaps) const {
    if (!Ordered(coinSeg, oppSeg)) {
        if (oppTs < oppTe) {
            return this->checkOverlap(check, oppSeg, coinSeg, oppTs, oppTe, coinTs, coinTe,
                    overlaps);
        }
        return this->checkOverlap(check, oppSeg, coinSeg, oppTe, oppTs, coinTe, coinTs, overlaps);
    }
    bool swapOpp = oppTs > oppTe;
    if (swapOpp) {
        SkTSwap(oppTs, oppTe);
    }
    do {
        if (check->coinPtTStart()->segment() != coinSeg) {
            continue;
        }
        if (check->oppPtTStart()->segment() != oppSeg) {
            continue;
        }
        double checkTs = check->coinPtTStart()->fT;
        double checkTe = check->coinPtTEnd()->fT;
        bool coinOutside = coinTe < checkTs || coinTs > checkTe;
        double oCheckTs = check->oppPtTStart()->fT;
        double oCheckTe = check->oppPtTEnd()->fT;
        if (swapOpp) {
            if (oCheckTs <= oCheckTe) {
              return false;
            }
            SkTSwap(oCheckTs, oCheckTe);
        }
        bool oppOutside = oppTe < oCheckTs || oppTs > oCheckTe;
        if (coinOutside && oppOutside) {
            continue;
        }
        bool coinInside = coinTe <= checkTe && coinTs >= checkTs;
        bool oppInside = oppTe <= oCheckTe && oppTs >= oCheckTs;
        if (coinInside && oppInside) {
            return false;  // complete overlap, already included, do nothing
        }
        *overlaps->append() = check; // partial overlap, extend existing entry
    } while ((check = check->next()));
    return true;
}

/* Please keep this in sync with debugAddIfMissing() */
bool SkOpCoincidence::addIfMissing(const SkOpPtT* over1s, const SkOpPtT* over1e,
        const SkOpPtT* over2s, const SkOpPtT* over2e, double tStart, double tEnd,
        SkOpPtT* coinPtTStart, const SkOpPtT* coinPtTEnd,
        SkOpPtT* oppPtTStart, const SkOpPtT* oppPtTEnd) {
    double coinTs, coinTe, oppTs, oppTe;
    TRange(over1s, over1e, tStart, tEnd, coinPtTStart, coinPtTEnd, &coinTs, &coinTe);
    TRange(over2s, over2e, tStart, tEnd, oppPtTStart, oppPtTEnd, &oppTs, &oppTe);
    bool swap = coinTs > coinTe;
    if (swap) {
        SkTSwap(coinTs, coinTe);
    }
    if ((over1s->fT < over1e->fT) != (over2s->fT < over2e->fT)) {
        SkTSwap(oppTs, oppTe);
    }
    if (swap) {
        SkTSwap(oppTs, oppTe);
    }
    SkOpSegment* coinSeg = coinPtTStart->segment();
    SkOpSegment* oppSeg = oppPtTStart->segment();
    if (coinSeg == oppSeg) {
        return false;
    }
    return this->addOrOverlap(coinSeg, oppSeg, coinTs, coinTe, oppTs, oppTe);
}

/* Please keep this in sync with debugAddOrOverlap() */
bool SkOpCoincidence::addOrOverlap(SkOpSegment* coinSeg, SkOpSegment* oppSeg,
        double coinTs, double coinTe, double oppTs, double oppTe) {
    SkTDArray<SkCoincidentSpans*> overlaps;
    if (!fTop) {
        return false;
    }
    if (!this->checkOverlap(fTop, coinSeg, oppSeg, coinTs, coinTe, oppTs, oppTe, &overlaps)) {
        return false;
    }
    if (fHead && !this->checkOverlap(fHead, coinSeg, oppSeg, coinTs,
            coinTe, oppTs, oppTe, &overlaps)) {
        return false;
    }
    SkCoincidentSpans* overlap = overlaps.count() ? overlaps[0] : nullptr;
    for (int index = 1; index < overlaps.count(); ++index) { // combine overlaps before continuing
        SkCoincidentSpans* test = overlaps[index];
        if (overlap->coinPtTStart()->fT > test->coinPtTStart()->fT) {
            overlap->setCoinPtTStart(test->coinPtTStart());
        }
        if (overlap->coinPtTEnd()->fT < test->coinPtTEnd()->fT) {
            overlap->setCoinPtTEnd(test->coinPtTEnd());
        }
        if (overlap->flipped()
                ? overlap->oppPtTStart()->fT < test->oppPtTStart()->fT
                : overlap->oppPtTStart()->fT > test->oppPtTStart()->fT) {
            overlap->setOppPtTStart(test->oppPtTStart());
        }
        if (overlap->flipped()
                ? overlap->oppPtTEnd()->fT > test->oppPtTEnd()->fT
                : overlap->oppPtTEnd()->fT < test->oppPtTEnd()->fT) {
            overlap->setOppPtTEnd(test->oppPtTEnd());
        }
        if (!fHead || !this->release(fHead, test)) {
            SkAssertResult(this->release(fTop, test));
        }
    }
    const SkOpPtT* cs = coinSeg->existing(coinTs, oppSeg);
    const SkOpPtT* ce = coinSeg->existing(coinTe, oppSeg);
    if (overlap && cs && ce && overlap->contains(cs, ce)) {
        return false;
    }
    if (cs == ce && cs) {
        return false;
    }
    const SkOpPtT* os = oppSeg->existing(oppTs, coinSeg);
    const SkOpPtT* oe = oppSeg->existing(oppTe, coinSeg);
    if (overlap && os && oe && overlap->contains(os, oe)) {
        return false;
    }
    SkASSERT(!cs || !cs->deleted());
    SkASSERT(!os || !os->deleted());
    SkASSERT(!ce || !ce->deleted());
    SkASSERT(!oe || !oe->deleted());
    const SkOpPtT* csExisting = !cs ? coinSeg->existing(coinTs, nullptr) : nullptr;
    const SkOpPtT* ceExisting = !ce ? coinSeg->existing(coinTe, nullptr) : nullptr;
    if (csExisting && csExisting == ceExisting) {
        return false;
    }
    if (csExisting && (csExisting == ce || csExisting->contains(ceExisting ? ceExisting : ce))) {
        return false;
    }
    if (ceExisting && (ceExisting == cs || ceExisting->contains(csExisting ? csExisting : cs))) {
        return false;
    }
    const SkOpPtT* osExisting = !os ? oppSeg->existing(oppTs, nullptr) : nullptr;
    const SkOpPtT* oeExisting = !oe ? oppSeg->existing(oppTe, nullptr) : nullptr;
    if (osExisting && osExisting == oeExisting) {
        return false;
    }
    if (osExisting && (osExisting == oe || osExisting->contains(oeExisting ? oeExisting : oe))) {
        return false;
    }
    if (oeExisting && (oeExisting == os || oeExisting->contains(osExisting ? osExisting : os))) {
        return false;
    }
    // extra line in debug code
    this->debugValidate();
    if (!cs || !os) {
        SkOpPtT* csWritable = cs ? const_cast<SkOpPtT*>(cs)
            : coinSeg->addT(coinTs, nullptr);
        SkOpPtT* osWritable = os ? const_cast<SkOpPtT*>(os)
            : oppSeg->addT(oppTs, nullptr);
        if (!csWritable || !osWritable) {
            return false;
        }
        csWritable->span()->addOppAndMerge(osWritable->span());
        cs = csWritable;
        os = osWritable;
        if ((ce && ce->deleted()) || (oe && oe->deleted())) {
            return false;
        }
    }
    if (!ce || !oe) {
        SkOpPtT* ceWritable = ce ? const_cast<SkOpPtT*>(ce)
            : coinSeg->addT(coinTe, nullptr);
        SkOpPtT* oeWritable = oe ? const_cast<SkOpPtT*>(oe)
            : oppSeg->addT(oppTe, nullptr);
        ceWritable->span()->addOppAndMerge(oeWritable->span());
        ce = ceWritable;
        oe = oeWritable;
    }
    this->debugValidate();
    if (cs->deleted() || os->deleted() || ce->deleted() || oe->deleted()) {
        return false;
    }
    if (cs->contains(ce) || os->contains(oe)) {
        return false;
    }
    bool result = true;
    if (overlap) {
        if (overlap->coinPtTStart()->segment() == coinSeg) {
            result = overlap->extend(cs, ce, os, oe);
        } else {
            if (os->fT > oe->fT) {
                SkTSwap(cs, ce);
                SkTSwap(os, oe);
            }
            result = overlap->extend(os, oe, cs, ce);
        }
#if DEBUG_COINCIDENCE_VERBOSE
        if (result) {
            overlaps[0]->debugShow();
        }
#endif
    } else {
        this->add(cs, ce, os, oe);
#if DEBUG_COINCIDENCE_VERBOSE
        fHead->debugShow();
#endif
    }
    this->debugValidate();
    return result;
}

// Please keep this in sync with debugAddMissing()
/* detects overlaps of different coincident runs on same segment */
/* does not detect overlaps for pairs without any segments in common */
// returns true if caller should loop again
bool SkOpCoincidence::addMissing() {
    SkCoincidentSpans* outer = fHead;
    if (!outer) {
        return false;
    }
    bool added = false;
    fTop = outer;
    fHead = nullptr;
    do {
    // addifmissing can modify the list that this is walking
    // save head so that walker can iterate over old data unperturbed
    // addifmissing adds to head freely then add saved head in the end
        const SkOpSegment* outerCoin = outer->coinPtTStart()->segment();
        const SkOpSegment* outerOpp = outer->oppPtTStart()->segment();
        if (outerCoin->done() || outerOpp->done()) {
            continue;
        }
        SkCoincidentSpans* inner = outer;
        while ((inner = inner->next())) {
            this->debugValidate();
            double overS, overE;
            const SkOpSegment* innerCoin = inner->coinPtTStart()->segment();
            const SkOpSegment* innerOpp = inner->oppPtTStart()->segment();
            if (innerCoin->done() || innerOpp->done()) {
                continue;
            }
            if (outerCoin == innerCoin) {
                if (outerOpp != innerOpp
                        && this->overlap(outer->coinPtTStart(), outer->coinPtTEnd(),
                        inner->coinPtTStart(), inner->coinPtTEnd(), &overS, &overE)) {
                    added |= this->addIfMissing(outer->coinPtTStart(), outer->coinPtTEnd(),
                            inner->coinPtTStart(), inner->coinPtTEnd(), overS, overE,
                            outer->oppPtTStart(), outer->oppPtTEnd(),
                            inner->oppPtTStart(), inner->oppPtTEnd());
                }
            } else if (outerCoin == innerOpp) {
                if (outerOpp != innerCoin
                        && this->overlap(outer->coinPtTStart(), outer->coinPtTEnd(),
                        inner->oppPtTStart(), inner->oppPtTEnd(), &overS, &overE)) {
                    added |= this->addIfMissing(outer->coinPtTStart(), outer->coinPtTEnd(),
                            inner->oppPtTStart(), inner->oppPtTEnd(), overS, overE,
                            outer->oppPtTStart(), outer->oppPtTEnd(),
                            inner->coinPtTStart(), inner->coinPtTEnd());
                }
            } else if (outerOpp == innerCoin) {
                SkASSERT(outerCoin != innerOpp);
                if (this->overlap(outer->oppPtTStart(), outer->oppPtTEnd(),
                        inner->coinPtTStart(), inner->coinPtTEnd(), &overS, &overE)) {
                    added |= this->addIfMissing(outer->oppPtTStart(), outer->oppPtTEnd(),
                            inner->coinPtTStart(), inner->coinPtTEnd(), overS, overE,
                            outer->coinPtTStart(), outer->coinPtTEnd(),
                            inner->oppPtTStart(), inner->oppPtTEnd());
                }
            } else if (outerOpp == innerOpp) {
                SkASSERT(outerCoin != innerCoin);
                if (this->overlap(outer->oppPtTStart(), outer->oppPtTEnd(),
                        inner->oppPtTStart(), inner->oppPtTEnd(), &overS, &overE)) {
                    added |= this->addIfMissing(outer->oppPtTStart(), outer->oppPtTEnd(),
                            inner->oppPtTStart(), inner->oppPtTEnd(), overS, overE,
                            outer->coinPtTStart(), outer->coinPtTEnd(),
                            inner->coinPtTStart(), inner->coinPtTEnd());
                }
            }
            this->debugValidate();
        }
    } while ((outer = outer->next()));
    this->restoreHead();
    return added;
}

bool SkOpCoincidence::addOverlap(const SkOpSegment* seg1, const SkOpSegment* seg1o,
        const SkOpSegment* seg2, const SkOpSegment* seg2o,
        const SkOpPtT* overS, const SkOpPtT* overE) {
    const SkOpPtT* s1, * e1, * s2, * e2;
    if (!(s1 = overS->find(seg1))) {
        return true;
    }
    if (!(e1 = overE->find(seg1))) {
        return true;
    }
    if (s1 == e1) {
        return true;
    }
    if (approximately_equal_half(s1->fT, e1->fT)) {
        return false;
    }
    if (!s1->starter(e1)->span()->upCast()->windValue()) {
        if (!(s1 = overS->find(seg1o))) {
            return true;
        }
        if (!(e1 = overE->find(seg1o))) {
            return true;
        }
        if (s1 == e1) {
            return true;
        }
        if (!s1->starter(e1)->span()->upCast()->windValue()) {
            return true;
        }
    }
    if (!(s2 = overS->find(seg2))) {
        return true;
    }
    if (!(e2 = overE->find(seg2))) {
        return true;
    }
    if (s2 == e2) {
        return true;
    }
    if (approximately_equal_half(s2->fT, e2->fT)) {
        return false;
    }
    if (!s2->starter(e2)->span()->upCast()->windValue()) {
        if (!(s2 = overS->find(seg2o))) {
            return true;
        }
        if (!(e2 = overE->find(seg2o))) {
            return true;
        }
        if (s2 == e2) {
            return true;
        }
        if (!s2->starter(e2)->span()->upCast()->windValue()) {
            return true;
        }
    }
    if (s1->segment() == s2->segment()) {
        return true;
    }
    if (s1->fT > e1->fT) {
        SkTSwap(s1, e1);
        SkTSwap(s2, e2);
    }
    this->add(s1, e1, s2, e2);
    return true;
}

/* look for pairs of coincidence with no common segments
   if there's no existing coincidence found that matches up the segments, and
   if the pt-t list for one contains the other, create coincident pairs for what's left */
bool SkOpCoincidence::addUncommon() {
    SkCoincidentSpans* outer = fHead;
    if (!outer) {
        return false;
    }
    bool added = false;
    fTop = outer;
    fHead = nullptr;
    do {
        // addifmissing can modify the list that this is walking
        // save head so that walker can iterate over old data unperturbed
        // addifmissing adds to head freely then add saved head in the end
        const SkOpSegment* outerCoin = outer->coinPtTStart()->segment();
        const SkOpSegment* outerOpp = outer->oppPtTStart()->segment();
        if (outerCoin->done() || outerOpp->done()) {
            continue;
        }
        SkCoincidentSpans* inner = outer;
        while ((inner = inner->next())) {
            this->debugValidate();
            const SkOpSegment* innerCoin = inner->coinPtTStart()->segment();
            const SkOpSegment* innerOpp = inner->oppPtTStart()->segment();
            if (innerCoin->done() || innerOpp->done()) {
                continue;
            }
            // check to see if outer span overlaps the inner span
            // look for inner segment in pt-t list
            // if present, and if t values are in coincident range
            // add two pairs of new coincidence
            const SkOpPtT* testS = outer->coinPtTStart()->contains(innerCoin);
            const SkOpPtT* testE = outer->coinPtTEnd()->contains(innerCoin);
            if (testS && testS->fT >= inner->coinPtTStart()->fT
                && testE && testE->fT <= inner->coinPtTEnd()->fT
                && this->testForCoincidence(outer, testS, testE)) {
                added |= this->addIfMissing(outer, testS, testE);
            } else {
                testS = inner->coinPtTStart()->contains(outerCoin);
                testE = inner->coinPtTEnd()->contains(outerCoin);
                if (testS && testS->fT >= outer->coinPtTStart()->fT
                    && testE && testE->fT <= outer->coinPtTEnd()->fT
                    && this->testForCoincidence(inner, testS, testE)) {
                    added |= this->addIfMissing(inner, testS, testE);
                }
            }
        }
    } while ((outer = outer->next()));
    this->restoreHead();
    return added;
}

bool SkOpCoincidence::contains(const SkOpSegment* seg, const SkOpSegment* opp, double oppT) const {
    if (this->contains(fHead, seg, opp, oppT)) {
        return true;
    }
    if (this->contains(fTop, seg, opp, oppT)) {
        return true;
    }
    return false;
}

bool SkOpCoincidence::contains(const SkCoincidentSpans* coin, const SkOpSegment* seg,
        const SkOpSegment* opp, double oppT) const {
    if (!coin) {
        return false;
   }
    do {
        if (coin->coinPtTStart()->segment() == seg && coin->oppPtTStart()->segment() == opp
                && between(coin->oppPtTStart()->fT, oppT, coin->oppPtTEnd()->fT)) {
            return true;
        }
        if (coin->oppPtTStart()->segment() == seg && coin->coinPtTStart()->segment() == opp
                && between(coin->coinPtTStart()->fT, oppT, coin->coinPtTEnd()->fT)) {
            return true;
        }
    } while ((coin = coin->next()));
    return false;
}

bool SkOpCoincidence::contains(const SkOpPtT* coinPtTStart, const SkOpPtT* coinPtTEnd,
        const SkOpPtT* oppPtTStart, const SkOpPtT* oppPtTEnd) const {
    const SkCoincidentSpans* test = fHead;
    if (!test) {
        return false;
    }
    const SkOpSegment* coinSeg = coinPtTStart->segment();
    const SkOpSegment* oppSeg = oppPtTStart->segment();
    if (!Ordered(coinPtTStart, oppPtTStart)) {
        SkTSwap(coinSeg, oppSeg);
        SkTSwap(coinPtTStart, oppPtTStart);
        SkTSwap(coinPtTEnd, oppPtTEnd);
        if (coinPtTStart->fT > coinPtTEnd->fT) {
            SkTSwap(coinPtTStart, coinPtTEnd);
            SkTSwap(oppPtTStart, oppPtTEnd);
        }
    }
    double oppMinT = SkTMin(oppPtTStart->fT, oppPtTEnd->fT);
    double oppMaxT = SkTMax(oppPtTStart->fT, oppPtTEnd->fT);
    do {
        if (coinSeg != test->coinPtTStart()->segment()) {
            continue;
        }
        if (coinPtTStart->fT < test->coinPtTStart()->fT) {
            continue;
        }
        if (coinPtTEnd->fT > test->coinPtTEnd()->fT) {
            continue;
        }
        if (oppSeg != test->oppPtTStart()->segment()) {
            continue;
        }
        if (oppMinT < SkTMin(test->oppPtTStart()->fT, test->oppPtTEnd()->fT)) {
            continue;
        }
        if (oppMaxT > SkTMax(test->oppPtTStart()->fT, test->oppPtTEnd()->fT)) {
            continue;
        }
        return true;
    } while ((test = test->next()));
    return false;
}

void SkOpCoincidence::correctEnds() {
    SkCoincidentSpans* coin = fHead;
    if (!coin) {
        return;
    }
    do {
        coin->correctEnds();
    } while ((coin = coin->next()));
}

// walk span sets in parallel, moving winding from one to the other
bool SkOpCoincidence::apply() {
    SkCoincidentSpans* coin = fHead;
    if (!coin) {
        return true;
    }
    do {
        SkOpSpan* start = coin->coinPtTStartWritable()->span()->upCast();
        if (start->deleted()) {
            continue;
        }
        const SkOpSpanBase* end = coin->coinPtTEnd()->span();
        SkASSERT(start == start->starter(end));
        bool flipped = coin->flipped();
        SkOpSpan* oStart = (flipped ? coin->oppPtTEndWritable()
                : coin->oppPtTStartWritable())->span()->upCast();
        if (oStart->deleted()) {
            continue;
        }
        const SkOpSpanBase* oEnd = (flipped ? coin->oppPtTStart() : coin->oppPtTEnd())->span();
        SkASSERT(oStart == oStart->starter(oEnd));
        SkOpSegment* segment = start->segment();
        SkOpSegment* oSegment = oStart->segment();
        bool operandSwap = segment->operand() != oSegment->operand();
        if (flipped) {
            if (oEnd->deleted()) {
                continue;
            }
            do {
                SkOpSpanBase* oNext = oStart->next();
                if (oNext == oEnd) {
                    break;
                }
                oStart = oNext->upCast();
            } while (true);
        }
        do {
            int windValue = start->windValue();
            int oppValue = start->oppValue();
            int oWindValue = oStart->windValue();
            int oOppValue = oStart->oppValue();
            // winding values are added or subtracted depending on direction and wind type
            // same or opposite values are summed depending on the operand value
            int windDiff = operandSwap ? oOppValue : oWindValue;
            int oWindDiff = operandSwap ? oppValue : windValue;
            if (!flipped) {
                windDiff = -windDiff;
                oWindDiff = -oWindDiff;
            }
            bool addToStart = windValue && (windValue > windDiff || (windValue == windDiff
                    && oWindValue <= oWindDiff));
            if (addToStart ? start->done() : oStart->done()) {
                addToStart ^= true;
            }
            if (addToStart) {
                if (operandSwap) {
                    SkTSwap(oWindValue, oOppValue);
                }
                if (flipped) {
                    windValue -= oWindValue;
                    oppValue -= oOppValue;
                } else {
                    windValue += oWindValue;
                    oppValue += oOppValue;
                }
                if (segment->isXor()) {
                    windValue &= 1;
                }
                if (segment->oppXor()) {
                    oppValue &= 1;
                }
                oWindValue = oOppValue = 0;
            } else {
                if (operandSwap) {
                    SkTSwap(windValue, oppValue);
                }
                if (flipped) {
                    oWindValue -= windValue;
                    oOppValue -= oppValue;
                } else {
                    oWindValue += windValue;
                    oOppValue += oppValue;
                }
                if (oSegment->isXor()) {
                    oWindValue &= 1;
                }
                if (oSegment->oppXor()) {
                    oOppValue &= 1;
                }
                windValue = oppValue = 0;
            }
#if DEBUG_COINCIDENCE
            SkDebugf("seg=%d span=%d windValue=%d oppValue=%d\n", segment->debugID(),
                    start->debugID(), windValue, oppValue);
            SkDebugf("seg=%d span=%d windValue=%d oppValue=%d\n", oSegment->debugID(),
                    oStart->debugID(), oWindValue, oOppValue);
#endif
            start->setWindValue(windValue);
            start->setOppValue(oppValue);
            oStart->setWindValue(oWindValue);
            oStart->setOppValue(oOppValue);
            if (!windValue && !oppValue) {
                segment->markDone(start);
            }
            if (!oWindValue && !oOppValue) {
                oSegment->markDone(oStart);
            }
            SkOpSpanBase* next = start->next();
            SkOpSpanBase* oNext = flipped ? oStart->prev() : oStart->next();
            if (next == end) {
                break;
            }
            if (!next->upCastable()) {
                return false;
            }
            start = next->upCast();
            // if the opposite ran out too soon, just reuse the last span
            if (!oNext || !oNext->upCastable()) {
               oNext = oStart;
            }
            oStart = oNext->upCast();
        } while (true);
    } while ((coin = coin->next()));
    return true;
}

// Please keep this in sync with debugRelease()
bool SkOpCoincidence::release(SkCoincidentSpans* coin, SkCoincidentSpans* remove)  {
    SkCoincidentSpans* head = coin;
    SkCoincidentSpans* prev = nullptr;
    SkCoincidentSpans* next;
    do {
        next = coin->next();
        if (coin == remove) {
            if (prev) {
                prev->setNext(next);
            } else if (head == fHead) {
                fHead = next;
            } else {
                fTop = next;
            }
            break;
        }
        prev = coin;
    } while ((coin = next));
    return coin != nullptr;
}

// Please keep this in sync with debugReorder()
// iterate through all coincident pairs, looking for ranges greater than 1
// if found, see if the opposite pair can match it -- which may require
// reordering the ptT pairs
bool SkOpCoincidence::reorder() {
    SkCoincidentSpans* coin = fHead;
    if (!coin) {
        return true;
    }
    do {
        // most commonly, concidence are one span long; check for that first
        int intervals = coin->spanCount();
        if (intervals <= 0) {
            return false;
        }
        if (1 == intervals) {
#if DEBUG_COINCIDENCE_VERBOSE
            SkASSERT(!coin->debugExpand(nullptr, nullptr));
#endif
            continue;
        }
        coin->expand();  // be all that you can be
        if (coin->spanCount() <= 0) {
            return false;
        }
        // check to see if every span in coin has a mate in opp
        const SkOpSpan* start = coin->coinPtTStart()->span()->upCast();
        bool flipped = coin->flipped();
        const SkOpSpanBase* oppStartBase = coin->oppPtTStart()->span();
        const SkOpSpan* oppStart = flipped ? oppStartBase->prev() : oppStartBase->upCast();
        SkDebugf("", start, oppStart);
    } while ((coin = coin->next()));
    return true;
}

void SkOpCoincidence::restoreHead() {
    SkCoincidentSpans** headPtr = &fHead;
    while (*headPtr) {
        headPtr = (*headPtr)->nextPtr();
    }
    *headPtr = fTop;
    fTop = nullptr;
    // segments may have collapsed in the meantime; remove empty referenced segments
    headPtr = &fHead;
    while (*headPtr) {
        SkCoincidentSpans* test = *headPtr;
        if (test->coinPtTStart()->segment()->done() || test->oppPtTStart()->segment()->done()) {
            *headPtr = test->next();
            continue;
        }
        headPtr = (*headPtr)->nextPtr();
    }
}

// Please keep this in sync with debugExpand()
bool SkOpCoincidence::expand() {
    SkCoincidentSpans* coin = fHead;
    if (!coin) {
        return false;
    }
    bool expanded = false;
    do {
        if (coin->expand()) {
            // check to see if multiple spans expanded so they are now identical
            SkCoincidentSpans* test = fHead;
            do {
                if (coin == test) {
                    continue;
                }
                if (coin->coinPtTStart() == test->coinPtTStart()
                        && coin->oppPtTStart() == test->oppPtTStart()) {
                    this->release(fHead, test);
                    break;
                }
            } while ((test = test->next()));
            expanded = true;
        }
    } while ((coin = coin->next()));
    return expanded;
}

bool SkOpCoincidence::findOverlaps(SkOpCoincidence* overlaps) const {
    overlaps->fHead = overlaps->fTop = nullptr;
    SkCoincidentSpans* outer = fHead;
    while (outer) {
        const SkOpSegment* outerCoin = outer->coinPtTStart()->segment();
        const SkOpSegment* outerOpp = outer->oppPtTStart()->segment();
        SkCoincidentSpans* inner = outer;
        while ((inner = inner->next())) {
            const SkOpSegment* innerCoin = inner->coinPtTStart()->segment();
            if (outerCoin == innerCoin) {
                continue;  // both winners are the same segment, so there's no additional overlap
            }
            const SkOpSegment* innerOpp = inner->oppPtTStart()->segment();
            const SkOpPtT* overlapS;
            const SkOpPtT* overlapE;
            if ((outerOpp == innerCoin && SkOpPtT::Overlaps(outer->oppPtTStart(),
                    outer->oppPtTEnd(),inner->coinPtTStart(), inner->coinPtTEnd(), &overlapS,
                    &overlapE))
                    || (outerCoin == innerOpp && SkOpPtT::Overlaps(outer->coinPtTStart(),
                    outer->coinPtTEnd(), inner->oppPtTStart(), inner->oppPtTEnd(),
                    &overlapS, &overlapE))
                    || (outerOpp == innerOpp && SkOpPtT::Overlaps(outer->oppPtTStart(),
                    outer->oppPtTEnd(), inner->oppPtTStart(), inner->oppPtTEnd(),
                    &overlapS, &overlapE))) {
                if (!overlaps->addOverlap(outerCoin, outerOpp, innerCoin, innerOpp,
                        overlapS, overlapE)) {
                    return false;
                }
            }
        }
        outer = outer->next();
    }
    return true;
}

// Please keep this in sync with debugRemoveCollapsed()
bool SkOpCoincidence::removeCollapsed() {
    SkCoincidentSpans* coin = fHead;
    if (!coin) {
        return true;
    }
    SkCoincidentSpans** priorPtr = &fHead;
    do {
        if (coin->coinPtTStart() == coin->coinPtTEnd()) {
            return false;
        }
        if (coin->oppPtTStart() == coin->oppPtTEnd()) {
            return false;
        }
        if (coin->coinPtTStart()->collapsed(coin->coinPtTEnd())) {
            *priorPtr = coin->next();
            continue;
        }
        if (coin->oppPtTStart()->collapsed(coin->oppPtTEnd())) {
            *priorPtr = coin->next();
            continue;
        }
        priorPtr = coin->nextPtr();
    } while ((coin = coin->next()));
    return true;
}

void SkOpCoincidence::fixUp(SkOpPtT* deleted, const SkOpPtT* kept) {
    SkASSERT(deleted != kept);
    if (fHead) {
        this->fixUp(fHead, deleted, kept);
    }
    if (fTop) {
        this->fixUp(fTop, deleted, kept);
    }
}

void SkOpCoincidence::fixUp(SkCoincidentSpans* coin, SkOpPtT* deleted, const SkOpPtT* kept) {
    SkCoincidentSpans* head = coin;
    do {
        if (coin->coinPtTStart() == deleted) {
            if (coin->coinPtTEnd()->span() == kept->span()) {
                this->release(head, coin);
                continue;
            }
            coin->setCoinPtTStart(kept);
        }
        if (coin->coinPtTEnd() == deleted) {
            if (coin->coinPtTStart()->span() == kept->span()) {
                this->release(head, coin);
                continue;
            }
            coin->setCoinPtTEnd(kept);
       }
        if (coin->oppPtTStart() == deleted) {
            if (coin->oppPtTEnd()->span() == kept->span()) {
                this->release(head, coin);
                continue;
            }
            coin->setOppPtTStart(kept);
        }
        if (coin->oppPtTEnd() == deleted) {
            if (coin->oppPtTStart()->span() == kept->span()) {
                this->release(head, coin);
                continue;
            }
            coin->setOppPtTEnd(kept);
        }
    } while ((coin = coin->next()));
}

// Please keep this in sync with debugMark()
/* this sets up the coincidence links in the segments when the coincidence crosses multiple spans */
bool SkOpCoincidence::mark() {
    SkCoincidentSpans* coin = fHead;
    if (!coin) {
        return true;
    }
    do {
        if (!coin->coinPtTStartWritable()->span()->upCastable()) {
            return false;
        }
        SkOpSpan* start = coin->coinPtTStartWritable()->span()->upCast();
        SkASSERT(!start->deleted());
        SkOpSpanBase* end = coin->coinPtTEndWritable()->span();
        SkASSERT(!end->deleted());
        SkOpSpanBase* oStart = coin->oppPtTStartWritable()->span();
        SkASSERT(!oStart->deleted());
        SkOpSpanBase* oEnd = coin->oppPtTEndWritable()->span();
        SkASSERT(!oEnd->deleted());
        bool flipped = coin->flipped();
        if (flipped) {
            SkTSwap(oStart, oEnd);
        }
        /* coin and opp spans may not match up. Mark the ends, and then let the interior
           get marked as many times as the spans allow */
        start->insertCoincidence(oStart->upCast());
        end->insertCoinEnd(oEnd);
        const SkOpSegment* segment = start->segment();
        const SkOpSegment* oSegment = oStart->segment();
        SkOpSpanBase* next = start;
        SkOpSpanBase* oNext = oStart;
        while ((next = next->upCast()->next()) != end) {
            if (!next->upCastable()) {
                return false;
            }
            if (!next->upCast()->insertCoincidence(oSegment, flipped)) {
                return false;
            }
        }
        while ((oNext = oNext->upCast()->next()) != oEnd) {
            if (!oNext->upCastable()) {
                return false;
            }
            if (!oNext->upCast()->insertCoincidence(segment, flipped)) {
                return false;
            }
        }
    } while ((coin = coin->next()));
    return true;
}

// Please keep in sync with debugMarkCollapsed()
void SkOpCoincidence::markCollapsed(SkCoincidentSpans* coin, SkOpPtT* test) {
    SkCoincidentSpans* head = coin;
    while (coin) {
        if (coin->collapsed(test)) {
            if (zero_or_one(coin->coinPtTStart()->fT) && zero_or_one(coin->coinPtTEnd()->fT)) {
                coin->coinPtTStartWritable()->segment()->markAllDone();
            }
            if (zero_or_one(coin->oppPtTStart()->fT) && zero_or_one(coin->oppPtTEnd()->fT)) {
                coin->oppPtTStartWritable()->segment()->markAllDone();
            }
            this->release(head, coin);
        }
        coin = coin->next();
    }
}

// Please keep in sync with debugMarkCollapsed()
void SkOpCoincidence::markCollapsed(SkOpPtT* test) {
    markCollapsed(fHead, test);
    markCollapsed(fTop, test);
}

bool SkOpCoincidence::Ordered(const SkOpSegment* coinSeg, const SkOpSegment* oppSeg) {
    if (coinSeg->verb() < oppSeg->verb()) {
        return true;
    }
    if (coinSeg->verb() > oppSeg->verb()) {
        return false;
    }
    int count = (SkPathOpsVerbToPoints(coinSeg->verb()) + 1) * 2;
    const SkScalar* cPt = &coinSeg->pts()[0].fX;
    const SkScalar* oPt = &oppSeg->pts()[0].fX;
    for (int index = 0; index < count; ++index) {
        if (*cPt < *oPt) {
            return true;
        }
        if (*cPt > *oPt) {
            return false;
        }
        ++cPt;
        ++oPt;
    }
    return true;
}

bool SkOpCoincidence::overlap(const SkOpPtT* coin1s, const SkOpPtT* coin1e,
        const SkOpPtT* coin2s, const SkOpPtT* coin2e, double* overS, double* overE) const {
    SkASSERT(coin1s->segment() == coin2s->segment());
    *overS = SkTMax(SkTMin(coin1s->fT, coin1e->fT), SkTMin(coin2s->fT, coin2e->fT));
    *overE = SkTMin(SkTMax(coin1s->fT, coin1e->fT), SkTMax(coin2s->fT, coin2e->fT));
    return *overS < *overE;
}

// Commented-out lines keep this in sync with debugRelease()
void SkOpCoincidence::release(const SkOpSegment* deleted) {
    SkCoincidentSpans* coin = fHead;
    if (!coin) {
        return;
    }
    do {
        if (coin->coinPtTStart()->segment() == deleted
                || coin->coinPtTEnd()->segment() == deleted
                || coin->oppPtTStart()->segment() == deleted
                || coin->oppPtTEnd()->segment() == deleted) {
            this->release(fHead, coin);
        }
    } while ((coin = coin->next()));
}

bool SkOpCoincidence::testForCoincidence(const SkCoincidentSpans* outer, const SkOpPtT* testS,
        const SkOpPtT* testE) const {
    return testS->segment()->testForCoincidence(testS, testE, testS->span(),
            testE->span(), outer->coinPtTStart()->segment());
}
