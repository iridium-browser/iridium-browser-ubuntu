/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ThreadState_h
#define ThreadState_h

#include "platform/PlatformExport.h"
#include "platform/heap/AddressSanitizer.h"
#include "platform/heap/ThreadingTraits.h"
#include "public/platform/WebThread.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/WTFString.h"

namespace v8 {
class Isolate;
};

namespace blink {

class BasePage;
class CallbackStack;
struct GCInfo;
class GarbageCollectedMixinConstructorMarker;
class HeapObjectHeader;
class PageMemoryRegion;
class PageMemory;
class PersistentNode;
class BaseHeap;
class SafePointAwareMutexLocker;
class SafePointBarrier;
class ThreadState;
class Visitor;

using Address = uint8_t*;

using FinalizationCallback = void (*)(void*);
using VisitorCallback = void (*)(Visitor*, void* self);
using TraceCallback = VisitorCallback;
using WeakPointerCallback = VisitorCallback;
using EphemeronCallback = VisitorCallback;

// Declare that a class has a pre-finalizer function.  The function is called in
// the object's owner thread, and can access Member<>s to other
// garbage-collected objects allocated in the thread.  However we must not
// allocate new garbage-collected objects, nor update Member<> and Persistent<>
// pointers.
//
// This feature is similar to the HeapHashMap<WeakMember<Foo>, OwnPtr<Disposer>>
// idiom.  The difference between this and the idiom is that pre-finalizer
// function is called whenever an object is destructed with this feature.  The
// HeapHashMap<WeakMember<Foo>, OwnPtr<Disposer>> idiom requires an assumption
// that the HeapHashMap outlives objects pointed by WeakMembers.
// FIXME: Replace all of the HeapHashMap<WeakMember<Foo>, OwnPtr<Disposer>>
// idiom usages with the pre-finalizer if the replacement won't cause
// performance regressions.
//
// See ThreadState::registerPreFinalizer.
//
// Usage:
//
// class Foo : GarbageCollected<Foo> {
//     USING_PRE_FINALIZER(Foo, dispose);
// public:
//     Foo()
//     {
//         ThreadState::current()->registerPreFinalizer(*this);
//     }
// private:
//     void dispose();
//     Member<Bar> m_bar;
// };
//
// void Foo::dispose()
// {
//     m_bar->...
// }
#define USING_PRE_FINALIZER(Class, method)   \
    public: \
        static bool invokePreFinalizer(void* object, Visitor& visitor)   \
        { \
            Class* self = reinterpret_cast<Class*>(object); \
            if (visitor.isHeapObjectAlive(self)) \
                return false; \
            self->method(); \
            return true; \
        } \
        using UsingPreFinazlizerMacroNeedsTrailingSemiColon = char

#if ENABLE(OILPAN)
#define WILL_BE_USING_PRE_FINALIZER(Class, method) USING_PRE_FINALIZER(Class, method)
#else
#define WILL_BE_USING_PRE_FINALIZER(Class, method)
#endif

// List of typed heaps. The list is used to generate the implementation
// of typed heap related methods.
//
// To create a new typed heap add a H(<ClassName>) to the
// FOR_EACH_TYPED_HEAP macro below.
#define FOR_EACH_TYPED_HEAP(H)              \
    H(Node)                                 \
    H(CSSValue)

#define TypedHeapEnumName(Type) Type##HeapIndex,

enum HeapIndices {
    NormalPage1HeapIndex = 0,
    NormalPage2HeapIndex,
    NormalPage3HeapIndex,
    NormalPage4HeapIndex,
    Vector1HeapIndex,
    Vector2HeapIndex,
    Vector3HeapIndex,
    Vector4HeapIndex,
    InlineVectorHeapIndex,
    HashTableHeapIndex,
    FOR_EACH_TYPED_HEAP(TypedHeapEnumName)
    LargeObjectHeapIndex,
    // Values used for iteration of heap segments.
    NumberOfHeaps,
};

#if ENABLE(GC_PROFILING)
const size_t numberOfGenerationsToTrack = 8;
const size_t maxHeapObjectAge = numberOfGenerationsToTrack - 1;

struct AgeCounts {
    int ages[numberOfGenerationsToTrack];
    AgeCounts() { std::fill(ages, ages + numberOfGenerationsToTrack, 0); }
};
typedef HashMap<String, AgeCounts> ClassAgeCountsMap;
#endif

class PLATFORM_EXPORT ThreadState {
    WTF_MAKE_NONCOPYABLE(ThreadState);
public:
    // When garbage collecting we need to know whether or not there
    // can be pointers to Blink GC managed objects on the stack for
    // each thread. When threads reach a safe point they record
    // whether or not they have pointers on the stack.
    enum StackState {
        NoHeapPointersOnStack,
        HeapPointersOnStack
    };

    enum GCType {
        GCWithSweep, // Sweeping is completed in Heap::collectGarbage().
        GCWithoutSweep, // Lazy sweeping is scheduled.
    };

    // See setGCState() for possible state transitions.
    enum GCState {
        NoGCScheduled,
        IdleGCScheduled,
        PreciseGCScheduled,
        FullGCScheduled,
        StoppingOtherThreads,
        GCRunning,
        EagerSweepScheduled,
        LazySweepScheduled,
        Sweeping,
        SweepingAndIdleGCScheduled,
        SweepingAndPreciseGCScheduled,
    };

    // The NoAllocationScope class is used in debug mode to catch unwanted
    // allocations. E.g. allocations during GC.
    class NoAllocationScope final {
    public:
        explicit NoAllocationScope(ThreadState* state) : m_state(state)
        {
            m_state->enterNoAllocationScope();
        }
        ~NoAllocationScope()
        {
            m_state->leaveNoAllocationScope();
        }
    private:
        ThreadState* m_state;
    };

    class SweepForbiddenScope final {
    public:
        explicit SweepForbiddenScope(ThreadState* state) : m_state(state)
        {
            ASSERT(!m_state->m_sweepForbidden);
            m_state->m_sweepForbidden = true;
        }
        ~SweepForbiddenScope()
        {
            ASSERT(m_state->m_sweepForbidden);
            m_state->m_sweepForbidden = false;
        }
    private:
        ThreadState* m_state;
    };

    // The set of ThreadStates for all threads attached to the Blink
    // garbage collector.
    using AttachedThreadStateSet = HashSet<ThreadState*>;
    static AttachedThreadStateSet& attachedThreads();
    void lockThreadAttachMutex();
    void unlockThreadAttachMutex();

    // Initialize threading infrastructure. Should be called from the main
    // thread.
    static void init();
    static void shutdown();
    static void shutdownHeapIfNecessary();
    bool isTerminating() { return m_isTerminating; }

    static void attachMainThread();
    static void detachMainThread();

    // Trace all persistent roots, called when marking the managed heap objects.
    static void visitPersistentRoots(Visitor*);

    // Trace all objects found on the stack, used when doing conservative GCs.
    static void visitStackRoots(Visitor*);

    // Associate ThreadState object with the current thread. After this
    // call thread can start using the garbage collected heap infrastructure.
    // It also has to periodically check for safepoints.
    static void attach();

    // Disassociate attached ThreadState from the current thread. The thread
    // can no longer use the garbage collected heap after this call.
    static void detach();

    static ThreadState* current()
    {
#if defined(__GLIBC__) || OS(ANDROID) || OS(FREEBSD)
        // TLS lookup is fast in these platforms.
        return **s_threadSpecific;
#else
        uintptr_t dummy;
        uintptr_t addressDiff = s_mainThreadStackStart - reinterpret_cast<uintptr_t>(&dummy);
        // This is a fast way to judge if we are in the main thread.
        // If |&dummy| is within |s_mainThreadUnderestimatedStackSize| byte from
        // the stack start of the main thread, we judge that we are in
        // the main thread.
        if (LIKELY(addressDiff < s_mainThreadUnderestimatedStackSize)) {
            ASSERT(**s_threadSpecific == mainThreadState());
            return mainThreadState();
        }
        // TLS lookup is slow.
        return **s_threadSpecific;
#endif
    }

    static ThreadState* mainThreadState()
    {
        return reinterpret_cast<ThreadState*>(s_mainThreadStateStorage);
    }

    bool isMainThread() const { return this == mainThreadState(); }
    bool checkThread() const
    {
        ASSERT(m_thread == currentThread());
        return true;
    }

    void didV8GC();

    void performIdleGC(double deadlineSeconds);
    void performIdleLazySweep(double deadlineSeconds);

    void scheduleIdleGC();
    void scheduleIdleLazySweep();
    void schedulePreciseGC();
    void scheduleGCIfNeeded();
    void setGCState(GCState);
    GCState gcState() const;
    bool isInGC() const { return gcState() == GCRunning; }
    bool isSweepingInProgress() const
    {
        return gcState() == Sweeping || gcState() == SweepingAndPreciseGCScheduled || gcState() == SweepingAndIdleGCScheduled;
    }

    // A GC runs in the following sequence.
    //
    // 1) All threads park at safe points.
    // 2) The GCing thread calls preGC() for all ThreadStates.
    // 3) The GCing thread calls Heap::collectGarbage().
    //    This does marking but doesn't do sweeping.
    // 4) The GCing thread calls postGC() for all ThreadStates.
    // 5) The GCing thread resume all threads.
    // 6) Each thread calls preSweep().
    // 7) Each thread runs lazy sweeping (concurrently with sweepings
    //    in other threads) and eventually calls completeSweep().
    // 8) Each thread calls postSweep().
    //
    // Notes:
    // - We stop the world between 1) and 5).
    // - isInGC() returns true between 2) and 4).
    // - isSweepingInProgress() returns true between 6) and 7).
    // - It is valid that the next GC is scheduled while some thread
    //   has not yet completed its lazy sweeping of the last GC.
    //   In this case, the next GC just cancels the remaining lazy sweeping.
    //   Specifically, preGC() of the next GC calls makeConsistentForSweeping()
    //   and it marks all not-yet-swept objets as dead.
    void makeConsistentForSweeping();
    void preGC();
    void postGC(GCType);
    void preSweep();
    void completeSweep();
    void postSweep();

    // Support for disallowing allocation. Mainly used for sanity
    // checks asserts.
    bool isAllocationAllowed() const { return !isAtSafePoint() && !m_noAllocationCount; }
    void enterNoAllocationScope() { m_noAllocationCount++; }
    void leaveNoAllocationScope() { m_noAllocationCount--; }
    bool isGCForbidden() const { return m_gcForbiddenCount; }
    void enterGCForbiddenScope() { m_gcForbiddenCount++; }
    void leaveGCForbiddenScope() { m_gcForbiddenCount--; }
    bool sweepForbidden() const { return m_sweepForbidden; }

    void prepareRegionTree();
    void flushHeapDoesNotContainCacheIfNeeded();

    // Safepoint related functionality.
    //
    // When a thread attempts to perform GC it needs to stop all other threads
    // that use the heap or at least guarantee that they will not touch any
    // heap allocated object until GC is complete.
    //
    // We say that a thread is at a safepoint if this thread is guaranteed to
    // not touch any heap allocated object or any heap related functionality until
    // it leaves the safepoint.
    //
    // Notice that a thread does not have to be paused if it is at safepoint it
    // can continue to run and perform tasks that do not require interaction
    // with the heap. It will be paused if it attempts to leave the safepoint and
    // there is a GC in progress.
    //
    // Each thread that has ThreadState attached must:
    //   - periodically check if GC is requested from another thread by calling a safePoint() method;
    //   - use SafePointScope around long running loops that have no safePoint() invocation inside,
    //     such loops must not touch any heap object;
    //   - register an Interruptor that can interrupt long running loops that have no calls to safePoint and
    //     are not wrapped in a SafePointScope (e.g. Interruptor for JavaScript code)
    //

    // Request all other threads to stop. Must only be called if the current thread is at safepoint.
    static bool stopThreads();
    static void resumeThreads();

    // Check if GC is requested by another thread and pause this thread if this is the case.
    // Can only be called when current thread is in a consistent state.
    void safePoint(StackState);

    // Mark current thread as running inside safepoint.
    void enterSafePointWithPointers(void* scopeMarker) { enterSafePoint(HeapPointersOnStack, scopeMarker); }
    void leaveSafePoint(SafePointAwareMutexLocker* = nullptr);
    bool isAtSafePoint() const { return m_atSafePoint; }

    // If attached thread enters long running loop that can call back
    // into Blink and leaving and reentering safepoint at every
    // transition between this loop and Blink is deemed too expensive
    // then instead of marking this loop as a GC safepoint thread
    // can provide an interruptor object which would allow GC
    // to temporarily interrupt and pause this long running loop at
    // an arbitrary moment creating a safepoint for a GC.
    class PLATFORM_EXPORT Interruptor {
    public:
        virtual ~Interruptor() { }

        // Request the interruptor to interrupt the thread and
        // call onInterrupted on that thread once interruption
        // succeeds.
        virtual void requestInterrupt() = 0;

    protected:
        // This method is called on the interrupted thread to
        // create a safepoint for a GC.
        void onInterrupted();
    };

    void addInterruptor(Interruptor*);
    void removeInterruptor(Interruptor*);

    // Should only be called under protection of threadAttachMutex().
    const Vector<Interruptor*>& interruptors() const { return m_interruptors; }

    void recordStackEnd(intptr_t* endOfStack)
    {
        m_endOfStack = endOfStack;
    }

    // Get one of the heap structures for this thread.
    // The thread heap is split into multiple heap parts based on object types
    // and object sizes.
    BaseHeap* heap(int heapIndex) const
    {
        ASSERT(0 <= heapIndex);
        ASSERT(heapIndex < NumberOfHeaps);
        return m_heaps[heapIndex];
    }

#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
    // Infrastructure to determine if an address is within one of the
    // address ranges for the Blink heap. If the address is in the Blink
    // heap the containing heap page is returned.
    BasePage* findPageFromAddress(Address);
    BasePage* findPageFromAddress(void* pointer) { return findPageFromAddress(reinterpret_cast<Address>(pointer)); }
#endif

    // List of persistent roots allocated on the given thread.
    PersistentNode* roots() const { return m_persistents.get(); }

    // List of global persistent roots not owned by any particular thread.
    // globalRootsMutex must be acquired before any modifications.
    static PersistentNode& globalRoots();
    static Mutex& globalRootsMutex();

    // Visit local thread stack and trace all pointers conservatively.
    void visitStack(Visitor*);

    // Visit the asan fake stack frame corresponding to a slot on the
    // real machine stack if there is one.
    void visitAsanFakeStackForPointer(Visitor*, Address);

    // Visit all persistents allocated on this thread.
    void visitPersistents(Visitor*);

#if ENABLE(GC_PROFILING)
    const GCInfo* findGCInfo(Address);
    static const GCInfo* findGCInfoFromAllThreads(Address);

    struct SnapshotInfo {
        ThreadState* state;

        size_t freeSize;
        size_t pageCount;

        // Map from base-classes to a snapshot class-ids (used as index below).
        using ClassTagMap = HashMap<const GCInfo*, size_t>;
        ClassTagMap classTags;

        // Map from class-id (index) to count/size.
        Vector<int> liveCount;
        Vector<int> deadCount;
        Vector<size_t> liveSize;
        Vector<size_t> deadSize;

        // Map from class-id (index) to a vector of generation counts.
        // For i < 7, the count is the number of objects that died after surviving |i| GCs.
        // For i == 7, the count is the number of objects that survived at least 7 GCs.
        using GenerationCountsVector = Vector<int, numberOfGenerationsToTrack>;
        Vector<GenerationCountsVector> generations;

        explicit SnapshotInfo(ThreadState* state) : state(state), freeSize(0), pageCount(0) { }

        size_t getClassTag(const GCInfo*);
    };

    void snapshot();
    void incrementMarkedObjectsAge();

    void snapshotFreeListIfNecessary();

    void collectAndReportMarkSweepStats() const;
    void reportMarkSweepStats(const char* statsName, const ClassAgeCountsMap&) const;
#endif

    void pushWeakPointerCallback(void*, WeakPointerCallback);
    bool popAndInvokeWeakPointerCallback(Visitor*);

    size_t objectPayloadSizeForTesting();
    void prepareHeapForTermination();

    // Request to call a pref-finalizer of the target object before the object
    // is destructed.  The class T must have USING_PRE_FINALIZER().  The
    // argument should be |*this|.  Registering a lot of objects affects GC
    // performance.  We should register an object only if the object really
    // requires pre-finalizer, and we should unregister the object if
    // pre-finalizer is unnecessary.
    template<typename T>
    void registerPreFinalizer(T& target)
    {
        checkThread();
        ASSERT(!m_preFinalizers.contains(&target));
        ASSERT(!sweepForbidden());
        m_preFinalizers.add(&target, &T::invokePreFinalizer);
    }

    // Cancel above requests.  The argument should be |*this|.  This function is
    // ignored if it is called in pre-finalizer functions.
    template<typename T>
    void unregisterPreFinalizer(T& target)
    {
        static_assert(sizeof(&T::invokePreFinalizer) > 0, "Declaration of USING_PRE_FINALIZER()'s prefinalizer trampoline not in scope.");
        checkThread();
        unregisterPreFinalizerInternal(&target);
    }

    Vector<PageMemoryRegion*>& allocatedRegionsSinceLastGC() { return m_allocatedRegionsSinceLastGC; }

    void shouldFlushHeapDoesNotContainCache() { m_shouldFlushHeapDoesNotContainCache = true; }

    void registerTraceDOMWrappers(v8::Isolate* isolate, void (*traceDOMWrappers)(v8::Isolate*, Visitor*))
    {
        m_isolate = isolate;
        m_traceDOMWrappers = traceDOMWrappers;
    }

    // By entering a gc-forbidden scope, conservative GCs will not
    // be allowed while handling an out-of-line allocation request.
    // Intended used when constructing subclasses of GC mixins, where
    // the object being constructed cannot be safely traced & marked
    // fully should a GC be allowed while its subclasses are being
    // constructed.
    void enterGCForbiddenScopeIfNeeded(GarbageCollectedMixinConstructorMarker* gcMixinMarker)
    {
        if (!m_gcMixinMarker) {
            enterGCForbiddenScope();
            m_gcMixinMarker = gcMixinMarker;
        }
    }
    void leaveGCForbiddenScopeIfNeeded(GarbageCollectedMixinConstructorMarker* gcMixinMarker)
    {
        ASSERT(m_gcForbiddenCount > 0);
        if (m_gcMixinMarker == gcMixinMarker) {
            leaveGCForbiddenScope();
            m_gcMixinMarker = nullptr;
        }
    }

    // vectorBackingHeap() returns a heap that the vector allocation should use.
    // We have four vector heaps and want to choose the best heap here.
    //
    // The goal is to improve the succession rate where expand and
    // promptlyFree happen at an allocation point. This is a key for reusing
    // the same memory as much as possible and thus improves performance.
    // To achieve the goal, we use the following heuristics:
    //
    // - A vector that has been expanded recently is likely to be expanded
    //   again soon.
    // - A vector is likely to be promptly freed if the same type of vector
    //   has been frequently promptly freed in the past.
    // - Given the above, when allocating a new vector, look at the four vectors
    //   that are placed immediately prior to the allocation point of each heap.
    //   Choose the heap where the vector is least likely to be expanded
    //   nor promptly freed.
    //
    // To implement the heuristics, we add a heapAge to each heap. The heapAge
    // is updated if:
    //
    // - a vector on the heap is expanded; or
    // - a vector that meets the condition (*) is allocated on the heap
    //
    //   (*) More than 33% of the same type of vectors have been promptly
    //       freed since the last GC.
    //
    BaseHeap* vectorBackingHeap(size_t gcInfoIndex)
    {
        size_t entryIndex = gcInfoIndex & likelyToBePromptlyFreedArrayMask;
        --m_likelyToBePromptlyFreed[entryIndex];
        int heapIndex = m_vectorBackingHeapIndex;
        // If m_likelyToBePromptlyFreed[entryIndex] > 0, that means that
        // more than 33% of vectors of the type have been promptly freed
        // since the last GC.
        if (m_likelyToBePromptlyFreed[entryIndex] > 0) {
            m_heapAges[heapIndex] = ++m_currentHeapAges;
            m_vectorBackingHeapIndex = heapIndexOfVectorHeapLeastRecentlyExpanded(Vector1HeapIndex, Vector4HeapIndex);
        }
        ASSERT(isVectorHeapIndex(heapIndex));
        return m_heaps[heapIndex];
    }
    BaseHeap* expandedVectorBackingHeap(size_t gcInfoIndex);
    static bool isVectorHeapIndex(int heapIndex)
    {
        return Vector1HeapIndex <= heapIndex && heapIndex <= Vector4HeapIndex;
    }
    void allocationPointAdjusted(int heapIndex);
    void promptlyFreed(size_t gcInfoIndex);

private:
    ThreadState();
    ~ThreadState();

    void enterSafePoint(StackState, void*);
    NO_SANITIZE_ADDRESS void copyStackUntilSafePointScope();
    void clearSafePointScopeMarker()
    {
        m_safePointStackCopy.clear();
        m_safePointScopeMarker = nullptr;
    }

    // shouldSchedule{Precise,Idle}GC and shouldForceConservativeGC
    // implement the heuristics that are used to determine when to collect garbage.
    // If shouldForceConservativeGC returns true, we force the garbage
    // collection immediately. Otherwise, if shouldGC returns true, we
    // record that we should garbage collect the next time we return
    // to the event loop. If both return false, we don't need to
    // collect garbage at this point.
    bool shouldScheduleIdleGC();
    bool shouldSchedulePreciseGC();
    bool shouldForceConservativeGC();
    void runScheduledGC(StackState);

    // When ThreadState is detaching from non-main thread its
    // heap is expected to be empty (because it is going away).
    // Perform registered cleanup tasks and garbage collection
    // to sweep away any objects that are left on this heap.
    // We assert that nothing must remain after this cleanup.
    // If assertion does not hold we crash as we are potentially
    // in the dangling pointer situation.
    void cleanup();
    void cleanupPages();

    void unregisterPreFinalizerInternal(void*);
    void invokePreFinalizers(Visitor&);

#if ENABLE(GC_PROFILING)
    void snapshotFreeList();
#endif
    void clearHeapAges();
    int heapIndexOfVectorHeapLeastRecentlyExpanded(int beginHeapIndex, int endHeapIndex);

    friend class SafePointAwareMutexLocker;
    friend class SafePointBarrier;
    friend class SafePointScope;

    static WTF::ThreadSpecific<ThreadState*>* s_threadSpecific;
    static uintptr_t s_mainThreadStackStart;
    static uintptr_t s_mainThreadUnderestimatedStackSize;
    static SafePointBarrier* s_safePointBarrier;

    // We can't create a static member of type ThreadState here
    // because it will introduce global constructor and destructor.
    // We would like to manage lifetime of the ThreadState attached
    // to the main thread explicitly instead and still use normal
    // constructor and destructor for the ThreadState class.
    // For this we reserve static storage for the main ThreadState
    // and lazily construct ThreadState in it using placement new.
    static uint8_t s_mainThreadStateStorage[];

    ThreadIdentifier m_thread;
    OwnPtr<PersistentNode> m_persistents;
    StackState m_stackState;
    intptr_t* m_startOfStack;
    intptr_t* m_endOfStack;
    void* m_safePointScopeMarker;
    Vector<Address> m_safePointStackCopy;
    bool m_atSafePoint;
    Vector<Interruptor*> m_interruptors;
    bool m_sweepForbidden;
    size_t m_noAllocationCount;
    size_t m_gcForbiddenCount;
    BaseHeap* m_heaps[NumberOfHeaps];

    int m_vectorBackingHeapIndex;
    size_t m_heapAges[NumberOfHeaps];
    size_t m_currentHeapAges;

    bool m_isTerminating;
    GarbageCollectedMixinConstructorMarker* m_gcMixinMarker;

    bool m_shouldFlushHeapDoesNotContainCache;
    GCState m_gcState;

    CallbackStack* m_weakCallbackStack;
    HashMap<void*, bool (*)(void*, Visitor&)> m_preFinalizers;

    v8::Isolate* m_isolate;
    void (*m_traceDOMWrappers)(v8::Isolate*, Visitor*);

#if defined(ADDRESS_SANITIZER)
    void* m_asanFakeStack;
#endif

    Vector<PageMemoryRegion*> m_allocatedRegionsSinceLastGC;

#if ENABLE(GC_PROFILING)
    double m_nextFreeListSnapshotTime;
#endif
    // Ideally we want to allocate an array of size |gcInfoTableMax| but it will
    // waste memory. Thus we limit the array size to 2^8 and share one entry
    // with multiple types of vectors. This won't be an issue in practice,
    // since there will be less than 2^8 types of objects in common cases.
    static const int likelyToBePromptlyFreedArraySize = (1 << 8);
    static const int likelyToBePromptlyFreedArrayMask = likelyToBePromptlyFreedArraySize - 1;
    OwnPtr<int[]> m_likelyToBePromptlyFreed;
};

template<ThreadAffinity affinity> class ThreadStateFor;

template<> class ThreadStateFor<MainThreadOnly> {
public:
    static ThreadState* state()
    {
        // This specialization must only be used from the main thread.
        ASSERT(ThreadState::current()->isMainThread());
        return ThreadState::mainThreadState();
    }
};

template<> class ThreadStateFor<AnyThread> {
public:
    static ThreadState* state() { return ThreadState::current(); }
};

} // namespace blink

#endif // ThreadState_h
