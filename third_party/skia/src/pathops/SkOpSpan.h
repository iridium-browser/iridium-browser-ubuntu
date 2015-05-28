/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef SkOpSpan_DEFINED
#define SkOpSpan_DEFINED

#include "SkPathOpsDebug.h"
#include "SkPoint.h"

class SkChunkAlloc;
struct SkOpAngle;
class SkOpContour;
class SkOpGlobalState;
class SkOpSegment;
class SkOpSpanBase;
class SkOpSpan;

// subset of op span used by terminal span (when t is equal to one)
class SkOpPtT {
public:
    enum {
        kIsAlias = 1,
        kIsDuplicate = 1
    };

    void addOpp(SkOpPtT* opp) {
        // find the fOpp ptr to opp
        SkOpPtT* oppPrev = opp->fNext;
        if (oppPrev == this) {
            return;
        }
        while (oppPrev->fNext != opp) {
            oppPrev = oppPrev->fNext;
             if (oppPrev == this) {
                 return;
             }
        }
        
        SkOpPtT* oldNext = this->fNext;
        SkASSERT(this != opp);
        this->fNext = opp;
        SkASSERT(oppPrev != oldNext);
        oppPrev->fNext = oldNext;
    }

    bool alias() const;
    SkOpContour* contour() const;

    int debugID() const {
        return PATH_OPS_DEBUG_RELEASE(fID, -1);
    }

    const SkOpAngle* debugAngle(int id) const;
    SkOpContour* debugContour(int id);
    int debugLoopLimit(bool report) const;
    bool debugMatchID(int id) const;
    const SkOpPtT* debugPtT(int id) const;
    const SkOpSegment* debugSegment(int id) const;
    const SkOpSpanBase* debugSpan(int id) const;
    SkOpGlobalState* globalState() const;
    void debugValidate() const;

    bool deleted() const {
        return fDeleted;
    }

    bool duplicate() const {
        return fDuplicatePt;
    }

    void dump() const;  // available to testing only
    void dumpAll() const;
    void dumpBase() const;

    void init(SkOpSpanBase* , double t, const SkPoint& , bool dup);

    void insert(SkOpPtT* span) {
        SkASSERT(span != this);
        span->fNext = fNext;
        fNext = span;
    }

    const SkOpPtT* next() const {
        return fNext;
    }

    SkOpPtT* next() {
        return fNext;
    }

    bool onEnd() const;
    SkOpPtT* prev();
    SkOpPtT* remove();
    void removeNext(SkOpPtT* kept);

    const SkOpSegment* segment() const;
    SkOpSegment* segment();

    void setDeleted() {
        SkASSERT(!fDeleted);
        fDeleted = true;
    }

    const SkOpSpanBase* span() const {
        return fSpan;
    }

    SkOpSpanBase* span() {
        return fSpan;
    }

    double fT; 
    SkPoint fPt;   // cache of point value at this t
protected:
    SkOpSpanBase* fSpan;  // contains winding data
    SkOpPtT* fNext;  // intersection on opposite curve or alias on this curve
    bool fDeleted;  // set if removed from span list 
    bool fDuplicatePt;  // set if identical pt is somewhere in the next loop
    PATH_OPS_DEBUG_CODE(int fID);
};

class SkOpSpanBase {
public:
    void addSimpleAngle(bool checkFrom , SkChunkAlloc* );
    void align();

    bool aligned() const {
        return fAligned;
    }

    void alignEnd(double t, const SkPoint& pt);

    bool chased() const {
        return fChased;
    }

    void clearCoinEnd() {
        SkASSERT(fCoinEnd != this);
        fCoinEnd = this;
    }

    const SkOpSpanBase* coinEnd() const {
        return fCoinEnd;
    }

    bool contains(const SkOpSpanBase* ) const;
    SkOpPtT* contains(const SkOpSegment* );

    bool containsCoinEnd(const SkOpSpanBase* coin) const {
        SkASSERT(this != coin);
        const SkOpSpanBase* next = this;
        while ((next = next->fCoinEnd) != this) {
            if (next == coin) {
                return true;
            }
        }
        return false;
    }

    bool containsCoinEnd(const SkOpSegment* ) const;
    SkOpContour* contour() const;

    int debugBumpCount() {
        return PATH_OPS_DEBUG_RELEASE(++fCount, -1);
    }

    int debugID() const {
        return PATH_OPS_DEBUG_RELEASE(fID, -1);
    }

    const SkOpAngle* debugAngle(int id) const;
    bool debugCoinEndLoopCheck() const;
    SkOpContour* debugContour(int id);
    const SkOpPtT* debugPtT(int id) const;
    const SkOpSegment* debugSegment(int id) const;
    const SkOpSpanBase* debugSpan(int id) const;
    SkOpGlobalState* globalState() const;
    void debugValidate() const;

    bool deleted() const {
        return fPtT.deleted();
    }

    void dump() const;  // available to testing only
    void dumpCoin() const;
    void dumpAll() const;
    void dumpBase() const;

    bool final() const {
        return fPtT.fT == 1;
    }

    SkOpAngle* fromAngle() const {
        return fFromAngle;
    }

    void initBase(SkOpSegment* parent, SkOpSpan* prev, double t, const SkPoint& pt);

    void insertCoinEnd(SkOpSpanBase* coin) {
        if (containsCoinEnd(coin)) {
            SkASSERT(coin->containsCoinEnd(this));
            return;
        }
        debugValidate();
        SkASSERT(this != coin);
        SkOpSpanBase* coinNext = coin->fCoinEnd;
        coin->fCoinEnd = this->fCoinEnd;
        this->fCoinEnd = coinNext;
        debugValidate();
    }

    void merge(SkOpSpan* span);

    SkOpSpan* prev() const {
        return fPrev;
    }

    const SkPoint& pt() const {
        return fPtT.fPt;
    }

    const SkOpPtT* ptT() const {
        return &fPtT;
    }

    SkOpPtT* ptT() {
        return &fPtT;
    }

    SkOpSegment* segment() const {
        return fSegment;
    }

    void setChased(bool chased) {
        fChased = chased;
    }

    SkOpPtT* setCoinEnd(SkOpSpanBase* oldCoinEnd, SkOpSegment* oppSegment);

    void setFromAngle(SkOpAngle* angle) {
        fFromAngle = angle;
    }

    void setPrev(SkOpSpan* prev) {
        fPrev = prev;
    }

    bool simple() const {
        fPtT.debugValidate();
        return fPtT.next()->next() == &fPtT; 
    }

    const SkOpSpan* starter(const SkOpSpanBase* end) const {
        const SkOpSpanBase* result = t() < end->t() ? this : end;
        return result->upCast();
    }

    SkOpSpan* starter(SkOpSpanBase* end) {
        SkASSERT(this->segment() == end->segment());
        SkOpSpanBase* result = t() < end->t() ? this : end;
        return result->upCast();
    }

    SkOpSpan* starter(SkOpSpanBase** endPtr) {
        SkOpSpanBase* end = *endPtr;
        SkASSERT(this->segment() == end->segment());
        SkOpSpanBase* result;
        if (t() < end->t()) {
            result = this;
        } else {
            result = end;
            *endPtr = this;
        }
        return result->upCast();
    }

    int step(const SkOpSpanBase* end) const {
        return t() < end->t() ? 1 : -1;
    }

    double t() const {
        return fPtT.fT;
    }

    void unaligned() {
        fAligned = false;
    }

    SkOpSpan* upCast() {
        SkASSERT(!final());
        return (SkOpSpan*) this;
    }

    const SkOpSpan* upCast() const {
        SkASSERT(!final());
        return (const SkOpSpan*) this;
    }

    SkOpSpan* upCastable() {
        return final() ? NULL : upCast();
    }

    const SkOpSpan* upCastable() const {
        return final() ? NULL : upCast();
    }

private:
    void alignInner();

protected:  // no direct access to internals to avoid treating a span base as a span
    SkOpPtT fPtT;  // list of points and t values associated with the start of this span
    SkOpSegment* fSegment;  // segment that contains this span
    SkOpSpanBase* fCoinEnd;  // linked list of coincident spans that end here (may point to itself)
    SkOpAngle* fFromAngle;  // points to next angle from span start to end
    SkOpSpan* fPrev;  // previous intersection point
    bool fAligned;
    bool fChased;  // set after span has been added to chase array
    PATH_OPS_DEBUG_CODE(int fCount);  // number of pt/t pairs added
    PATH_OPS_DEBUG_CODE(int fID);
};

class SkOpSpan : public SkOpSpanBase {
public:
    void applyCoincidence(SkOpSpan* opp);

    bool clearCoincident() {
        SkASSERT(!final());
        if (fCoincident == this) {
            return false;
        }
        fCoincident = this;
        return true;
    }

    bool containsCoincidence(const SkOpSegment* ) const;

    bool containsCoincidence(const SkOpSpan* coin) const {
        SkASSERT(this != coin);
        const SkOpSpan* next = this;
        while ((next = next->fCoincident) != this) {
            if (next == coin) {
                return true;
            }
        }
        return false;
    }

    bool debugCoinLoopCheck() const;
    void detach(SkOpPtT* );

    bool done() const {
        SkASSERT(!final());
        return fDone;
    }

    void dumpCoin() const;
    bool dumpSpan() const;
    void init(SkOpSegment* parent, SkOpSpan* prev, double t, const SkPoint& pt);

    void insertCoincidence(SkOpSpan* coin) {
        if (containsCoincidence(coin)) {
            SkASSERT(coin->containsCoincidence(this));
            return;
        }
        debugValidate();
        SkASSERT(this != coin);
        SkOpSpan* coinNext = coin->fCoincident;
        coin->fCoincident = this->fCoincident;
        this->fCoincident = coinNext;
        debugValidate();
    }

    bool isCanceled() const {
        SkASSERT(!final());
        return fWindValue == 0 && fOppValue == 0;
    }

    bool isCoincident() const {
        SkASSERT(!final());
        return fCoincident != this;
    }

    SkOpSpanBase* next() const {
        SkASSERT(!final());
        return fNext;
    }

    int oppSum() const {
        SkASSERT(!final());
        return fOppSum;
    }

    int oppValue() const {
        SkASSERT(!final());
        return fOppValue;
    }

    SkOpPtT* setCoinStart(SkOpSpan* oldCoinStart, SkOpSegment* oppSegment);

    void setDone(bool done) {
        SkASSERT(!final());
        fDone = done;
    }

    void setNext(SkOpSpanBase* nextT) {
        SkASSERT(!final());
        fNext = nextT;
    }

    void setOppSum(int oppSum);

    void setOppValue(int oppValue) {
        SkASSERT(!final());
        SkASSERT(fOppSum == SK_MinS32);
        fOppValue = oppValue;
    }

    void setToAngle(SkOpAngle* angle) {
        SkASSERT(!final());
        fToAngle = angle;
    }

    void setWindSum(int windSum) {
        SkASSERT(!final());
        SkASSERT(fWindSum == SK_MinS32 || fWindSum == windSum);
        SkASSERT(!DEBUG_LIMIT_WIND_SUM || abs(windSum) <= DEBUG_LIMIT_WIND_SUM);
        fWindSum = windSum;
    }

    void setWindValue(int windValue) {
        SkASSERT(!final());
        SkASSERT(windValue >= 0);
        SkASSERT(fWindSum == SK_MinS32);
        fWindValue = windValue;
    }

    SkOpAngle* toAngle() const {
        SkASSERT(!final());
        return fToAngle;
    }

    int windSum() const {
        SkASSERT(!final());
        return fWindSum;
    }

    int windValue() const {
        SkASSERT(!final());
        return fWindValue;
    }

private:  // no direct access to internals to avoid treating a span base as a span
    SkOpSpan* fCoincident;  // linked list of spans coincident with this one (may point to itself)
    SkOpAngle* fToAngle;  // points to next angle from span start to end
    SkOpSpanBase* fNext;  // next intersection point
    int fWindSum;  // accumulated from contours surrounding this one.
    int fOppSum;  // for binary operators: the opposite winding sum
    int fWindValue;  // 0 == canceled; 1 == normal; >1 == coincident
    int fOppValue;  // normally 0 -- when binary coincident edges combine, opp value goes here
    bool fDone;  // if set, this span to next higher T has been processed
};

#endif
