/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "SkOpCoincidence.h"
#include "SkOpContour.h"
#include "SkOpSegment.h"
#include "SkPathWriter.h"

bool SkOpPtT::alias() const {
    return this->span()->ptT() != this;
}

SkOpContour* SkOpPtT::contour() const {
    return segment()->contour();
}

SkOpGlobalState* SkOpPtT::globalState() const {
    return contour()->globalState(); 
}

void SkOpPtT::init(SkOpSpanBase* span, double t, const SkPoint& pt, bool duplicate) {
    fT = t;
    fPt = pt;
    fSpan = span;
    fNext = this;
    fDuplicatePt = duplicate;
    fDeleted = false;
    PATH_OPS_DEBUG_CODE(fID = span->globalState()->nextPtTID());
}

bool SkOpPtT::onEnd() const {
    const SkOpSpanBase* span = this->span();
    if (span->ptT() != this) {
        return false;
    }
    const SkOpSegment* segment = this->segment();
    return span == segment->head() || span == segment->tail();
}

SkOpPtT* SkOpPtT::prev() {
    SkOpPtT* result = this;
    SkOpPtT* next = this;
    while ((next = next->fNext) != this) {
        result = next;
    }
    SkASSERT(result->fNext == this);
    return result;
}

SkOpPtT* SkOpPtT::remove() {
    SkOpPtT* prev = this;
    do {
        SkOpPtT* next = prev->fNext;
        if (next == this) {
            prev->removeNext(this);
            SkASSERT(prev->fNext != prev);
            fDeleted = true;
            return prev;
        }
        prev = next;
    } while (prev != this);
    SkASSERT(0);
    return NULL;
}

void SkOpPtT::removeNext(SkOpPtT* kept) {
    SkASSERT(this->fNext);
    SkOpPtT* next = this->fNext;
    SkASSERT(this != next->fNext);
    this->fNext = next->fNext;
    SkOpSpanBase* span = next->span();
    next->setDeleted();
    if (span->ptT() == next) {
        span->upCast()->detach(kept);
    }
}

const SkOpSegment* SkOpPtT::segment() const {
    return span()->segment();
}

SkOpSegment* SkOpPtT::segment() {
    return span()->segment();
}

// find the starting or ending span with an existing loop of angles
// OPTIMIZE? remove the spans pointing to windValue==0 here or earlier?
// FIXME? assert that only one other span has a valid windValue or oppValue
void SkOpSpanBase::addSimpleAngle(bool checkFrom, SkChunkAlloc* allocator) {
    SkOpAngle* angle;
    if (checkFrom) {
        SkASSERT(this->final());
        if (this->fromAngle()) {
            SkASSERT(this->fromAngle()->loopCount() == 2);
            return;
        }
        angle = this->segment()->addEndSpan(allocator);
    } else {
        SkASSERT(this->t() == 0);
        SkOpSpan* span = this->upCast();
        if (span->toAngle()) {
            SkASSERT(span->toAngle()->loopCount() == 2);
            SkASSERT(!span->fromAngle());
            span->setFromAngle(span->toAngle()->next());
            return;
        }
        angle = this->segment()->addStartSpan(allocator);
    }
    SkOpPtT* ptT = this->ptT();
    SkOpSpanBase* oSpanBase;
    SkOpSpan* oSpan;
    SkOpSegment* other;
    do {
        ptT = ptT->next();
        oSpanBase = ptT->span();
        oSpan = oSpanBase->upCastable();
        other = oSpanBase->segment();
        if (oSpan && oSpan->windValue()) {
            break;
        }
        if (oSpanBase->t() == 0) {
            continue;
        }
        SkOpSpan* oFromSpan = oSpanBase->prev();
        SkASSERT(oFromSpan->t() < 1);
        if (oFromSpan->windValue()) {
            break;
        }
    } while (ptT != this->ptT());
    SkOpAngle* oAngle;
    if (checkFrom) {
        oAngle = other->addStartSpan(allocator);
        SkASSERT(oSpan && !oSpan->final());
        SkASSERT(oAngle == oSpan->toAngle());
    } else {
        oAngle = other->addEndSpan(allocator);
        SkASSERT(oAngle == oSpanBase->fromAngle());
    }
    angle->insert(oAngle);
}

void SkOpSpanBase::align() {
    if (this->fAligned) {
        return;
    }
    SkASSERT(!zero_or_one(this->fPtT.fT));
    SkASSERT(this->fPtT.next());
    // if a linked pt/t pair has a t of zero or one, use it as the base for alignment
    SkOpPtT* ptT = &this->fPtT, * stopPtT = ptT;
    while ((ptT = ptT->next()) != stopPtT) {
        if (zero_or_one(ptT->fT)) {
            SkOpSegment* segment = ptT->segment();
            SkASSERT(this->segment() != segment);
            SkASSERT(segment->head()->ptT() == ptT || segment->tail()->ptT() == ptT);
            if (ptT->fT) {
                segment->tail()->alignEnd(1, segment->lastPt());
            } else {
                segment->head()->alignEnd(0, segment->pts()[0]);
            }
            return;
        }
    }
    alignInner();
    this->fAligned = true;
}


// FIXME: delete spans that collapse
// delete segments that collapse
// delete contours that collapse
void SkOpSpanBase::alignEnd(double t, const SkPoint& pt) {
    SkASSERT(zero_or_one(t));
    SkOpSegment* segment = this->segment();
    SkASSERT(t ? segment->lastPt() == pt : segment->pts()[0] == pt);
    alignInner();
    *segment->writablePt(!!t) = pt;
    SkOpPtT* ptT = &this->fPtT;
    SkASSERT(t == ptT->fT);
    SkASSERT(pt == ptT->fPt);
    SkOpPtT* test = ptT, * stopPtT = ptT;
    while ((test = test->next()) != stopPtT) {
        SkOpSegment* other = test->segment();
        if (other == this->segment()) {
            continue;
        }
        if (!zero_or_one(test->fT)) {
            continue;
        }
        *other->writablePt(!!test->fT) = pt;
    }
    this->fAligned = true;
}

void SkOpSpanBase::alignInner() {
    // force the spans to share points and t values
    SkOpPtT* ptT = &this->fPtT, * stopPtT = ptT;
    const SkPoint& pt = ptT->fPt;
    do {
        ptT->fPt = pt;
        const SkOpSpanBase* span = ptT->span();
        SkOpPtT* test = ptT;
        do {
            SkOpPtT* prev = test;
            if ((test = test->next()) == stopPtT) {
                break;
            }
            if (span == test->span() && !span->segment()->ptsDisjoint(*ptT, *test)) {
                // omit aliases that alignment makes redundant
                if ((!ptT->alias() || test->alias()) && (ptT->onEnd() || !test->onEnd())) {
                    SkASSERT(test->alias());
                    prev->removeNext(ptT);
                    test = prev;
                } else {
                    SkASSERT(ptT->alias());
                    stopPtT = ptT = ptT->remove();
                    break;
                }
            }
        } while (true);
    } while ((ptT = ptT->next()) != stopPtT);
}

bool SkOpSpanBase::contains(const SkOpSpanBase* span) const {
    const SkOpPtT* start = &fPtT;
    const SkOpPtT* check = &span->fPtT;
    SkASSERT(start != check);
    const SkOpPtT* walk = start;
    while ((walk = walk->next()) != start) {
        if (walk == check) {
            return true;
        }
    }
    return false;
}

SkOpPtT* SkOpSpanBase::contains(const SkOpSegment* segment) {
    SkOpPtT* start = &fPtT;
    SkOpPtT* walk = start;
    while ((walk = walk->next()) != start) {
        if (walk->segment() == segment) {
            return walk;
        }
    }
    return NULL;
}

bool SkOpSpanBase::containsCoinEnd(const SkOpSegment* segment) const {
    SkASSERT(this->segment() != segment);
    const SkOpSpanBase* next = this;
    while ((next = next->fCoinEnd) != this) {
        if (next->segment() == segment) {
            return true;
        }
    }
    return false;
}

SkOpContour* SkOpSpanBase::contour() const {
    return segment()->contour();
}

SkOpGlobalState* SkOpSpanBase::globalState() const {
    return contour()->globalState(); 
}

void SkOpSpanBase::initBase(SkOpSegment* segment, SkOpSpan* prev, double t, const SkPoint& pt) {
    fSegment = segment;
    fPtT.init(this, t, pt, false);
    fCoinEnd = this;
    fFromAngle = NULL;
    fPrev = prev;
    fAligned = true;
    fChased = false;
    PATH_OPS_DEBUG_CODE(fCount = 1);
    PATH_OPS_DEBUG_CODE(fID = globalState()->nextSpanID());
}

// this pair of spans share a common t value or point; merge them and eliminate duplicates
// this does not compute the best t or pt value; this merely moves all data into a single list
void SkOpSpanBase::merge(SkOpSpan* span) {
    SkOpPtT* spanPtT = span->ptT();
    SkASSERT(this->t() != spanPtT->fT);
    SkASSERT(!zero_or_one(spanPtT->fT));
    span->detach(this->ptT());
    SkOpPtT* remainder = spanPtT->next();
    ptT()->insert(spanPtT);
    while (remainder != spanPtT) {
        SkOpPtT* next = remainder->next();
        SkOpPtT* compare = spanPtT->next();
        while (compare != spanPtT) {
            SkOpPtT* nextC = compare->next();
            if (nextC->span() == remainder->span() && nextC->fT == remainder->fT) {
                goto tryNextRemainder;
            }
            compare = nextC;
        }
        spanPtT->insert(remainder);
tryNextRemainder:
        remainder = next;
    }
}

void SkOpSpan::applyCoincidence(SkOpSpan* opp) {
    SkASSERT(!final());
    SkASSERT(0);  // incomplete
}

bool SkOpSpan::containsCoincidence(const SkOpSegment* segment) const {
    SkASSERT(this->segment() != segment);
    const SkOpSpan* next = fCoincident;
    do {
        if (next->segment() == segment) {
            return true;
        }
    } while ((next = next->fCoincident) != this);
    return false;
}

void SkOpSpan::detach(SkOpPtT* kept) {
    SkASSERT(!final());
    SkOpSpan* prev = this->prev();
    SkASSERT(prev);
    SkOpSpanBase* next = this->next();
    SkASSERT(next);
    prev->setNext(next);
    next->setPrev(prev);
    this->segment()->detach(this);
    this->globalState()->coincidence()->fixUp(this->ptT(), kept);
    this->ptT()->setDeleted();
}

void SkOpSpan::init(SkOpSegment* segment, SkOpSpan* prev, double t, const SkPoint& pt) {
    SkASSERT(t != 1);
    initBase(segment, prev, t, pt);
    fCoincident = this;
    fToAngle = NULL;
    fWindSum = fOppSum = SK_MinS32;
    fWindValue = 1;
    fOppValue = 0;
    fChased = fDone = false;
    segment->bumpCount();
}

void SkOpSpan::setOppSum(int oppSum) {
    SkASSERT(!final());
    if (fOppSum != SK_MinS32 && fOppSum != oppSum) {
        this->globalState()->setWindingFailed();
        return;
    }
    SkASSERT(!DEBUG_LIMIT_WIND_SUM || abs(oppSum) <= DEBUG_LIMIT_WIND_SUM);
    fOppSum = oppSum;
}
