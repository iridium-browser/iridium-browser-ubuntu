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
#include "platform/heap/BlinkGC.h"
#include "platform/heap/ThreadingTraits.h"
#include "public/platform/WebThread.h"
#include "wtf/AddressSanitizer.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/ThreadingPrimitives.h"
#include <memory>

namespace v8 {
class Isolate;
};

namespace blink {

class BasePage;
class GarbageCollectedMixinConstructorMarker;
class PersistentNode;
class PersistentRegion;
class BaseArena;
class ThreadHeap;
class ThreadState;
class Visitor;

// Declare that a class has a pre-finalizer. The pre-finalizer is called
// before any object gets swept, so it is safe to touch on-heap objects
// that may be collected in the same GC cycle. If you cannot avoid touching
// on-heap objects in a destructor (which is not allowed), you can consider
// using the pre-finalizer. The only restriction is that the pre-finalizer
// must not resurrect dead objects (e.g., store unmarked objects into
// Members etc). The pre-finalizer is called on the thread that registered
// the pre-finalizer.
//
// Since a pre-finalizer adds pressure on GC performance, you should use it
// only if necessary.
//
// A pre-finalizer is similar to the
// HeapHashMap<WeakMember<Foo>, std::unique_ptr<Disposer>> idiom.  The
// difference between this and the idiom is that pre-finalizer function is
// called whenever an object is destructed with this feature.  The
// HeapHashMap<WeakMember<Foo>, std::unique_ptr<Disposer>> idiom requires an
// assumption that the HeapHashMap outlives objects pointed by WeakMembers.
// FIXME: Replace all of the
// HeapHashMap<WeakMember<Foo>, std::unique_ptr<Disposer>> idiom usages with the
// pre-finalizer if the replacement won't cause performance regressions.
//
// Usage:
//
// class Foo : GarbageCollected<Foo> {
//     USING_PRE_FINALIZER(Foo, dispose);
// private:
//     void dispose()
//     {
//         m_bar->...; // It is safe to touch other on-heap objects.
//     }
//     Member<Bar> m_bar;
// };
#define USING_PRE_FINALIZER(Class, preFinalizer)                           \
 public:                                                                   \
  static bool invokePreFinalizer(void* object) {                           \
    Class* self = reinterpret_cast<Class*>(object);                        \
    if (ThreadHeap::isHeapObjectAlive(self))                               \
      return false;                                                        \
    self->Class::preFinalizer();                                           \
    return true;                                                           \
  }                                                                        \
                                                                           \
 private:                                                                  \
  ThreadState::PrefinalizerRegistration<Class> m_prefinalizerDummy = this; \
  using UsingPreFinalizerMacroNeedsTrailingSemiColon = char

class PLATFORM_EXPORT ThreadState {
  USING_FAST_MALLOC(ThreadState);
  WTF_MAKE_NONCOPYABLE(ThreadState);

 public:
  // See setGCState() for possible state transitions.
  enum GCState {
    NoGCScheduled,
    IdleGCScheduled,
    PreciseGCScheduled,
    FullGCScheduled,
    PageNavigationGCScheduled,
    GCRunning,
    Sweeping,
    SweepingAndIdleGCScheduled,
    SweepingAndPreciseGCScheduled,
  };

  // The NoAllocationScope class is used in debug mode to catch unwanted
  // allocations. E.g. allocations during GC.
  class NoAllocationScope final {
    STACK_ALLOCATED();

   public:
    explicit NoAllocationScope(ThreadState* state) : m_state(state) {
      m_state->enterNoAllocationScope();
    }
    ~NoAllocationScope() { m_state->leaveNoAllocationScope(); }

   private:
    ThreadState* m_state;
  };

  class SweepForbiddenScope final {
    STACK_ALLOCATED();

   public:
    explicit SweepForbiddenScope(ThreadState* state) : m_state(state) {
      ASSERT(!m_state->m_sweepForbidden);
      m_state->m_sweepForbidden = true;
    }
    ~SweepForbiddenScope() {
      ASSERT(m_state->m_sweepForbidden);
      m_state->m_sweepForbidden = false;
    }

   private:
    ThreadState* m_state;
  };

  static void attachMainThread();

  // Associate ThreadState object with the current thread. After this
  // call thread can start using the garbage collected heap infrastructure.
  // It also has to periodically check for safepoints.
  static void attachCurrentThread();

  // Disassociate attached ThreadState from the current thread. The thread
  // can no longer use the garbage collected heap after this call.
  static void detachCurrentThread();

  static ThreadState* current() {
    return **s_threadSpecific;
  }

  static ThreadState* mainThreadState() {
    return reinterpret_cast<ThreadState*>(s_mainThreadStateStorage);
  }

  static ThreadState* fromObject(const void*);

  bool isMainThread() const { return this == mainThreadState(); }
  bool checkThread() const { return m_thread == currentThread(); }

  ThreadHeap& heap() const { return *m_heap; }

  // When ThreadState is detaching from non-main thread its
  // heap is expected to be empty (because it is going away).
  // Perform registered cleanup tasks and garbage collection
  // to sweep away any objects that are left on this heap.
  // We assert that nothing must remain after this cleanup.
  // If assertion does not hold we crash as we are potentially
  // in the dangling pointer situation.
  void runTerminationGC();

  void performIdleGC(double deadlineSeconds);
  void performIdleLazySweep(double deadlineSeconds);

  void scheduleIdleGC();
  void scheduleIdleLazySweep();
  void schedulePreciseGC();
  void scheduleV8FollowupGCIfNeeded(BlinkGC::V8GCType);
  void schedulePageNavigationGCIfNeeded(float estimatedRemovalRatio);
  void schedulePageNavigationGC();
  void scheduleGCIfNeeded();
  void willStartV8GC(BlinkGC::V8GCType);
  void setGCState(GCState);
  GCState gcState() const { return m_gcState; }
  bool isInGC() const { return gcState() == GCRunning; }
  bool isSweepingInProgress() const {
    return gcState() == Sweeping ||
           gcState() == SweepingAndPreciseGCScheduled ||
           gcState() == SweepingAndIdleGCScheduled;
  }

  // A GC runs in the following sequence.
  //
  // 1) preGC() is called.
  // 2) ThreadHeap::collectGarbage() is called. This marks live objects.
  // 3) postGC() is called. This does thread-local weak processing.
  // 4) preSweep() is called. This does pre-finalization, eager sweeping and
  //    heap compaction.
  // 4) Lazy sweeping sweeps heaps incrementally. completeSweep() may be called
  //    to complete the sweeping.
  // 5) postSweep() is called.
  //
  // Notes:
  // - The world is stopped between 1) and 3).
  // - isInGC() returns true between 1) and 3).
  // - isSweepingInProgress() returns true while any sweeping operation is
  //   running.
  void makeConsistentForGC();
  void preGC();
  void postGC(BlinkGC::GCType);
  void completeSweep();
  void preSweep(BlinkGC::GCType);
  void postSweep();
  // makeConsistentForMutator() drops marks from marked objects and rebuild
  // free lists. This is called after taking a snapshot and before resuming
  // the executions of mutators.
  void makeConsistentForMutator();

  void compact();

  // Support for disallowing allocation. Mainly used for sanity
  // checks asserts.
  bool isAllocationAllowed() const { return !m_noAllocationCount; }
  void enterNoAllocationScope() { m_noAllocationCount++; }
  void leaveNoAllocationScope() { m_noAllocationCount--; }
  bool isWrapperTracingForbidden() { return isMixinInConstruction(); }
  bool isGCForbidden() const {
    return m_gcForbiddenCount || isMixinInConstruction();
  }
  void enterGCForbiddenScope() { m_gcForbiddenCount++; }
  void leaveGCForbiddenScope() {
    DCHECK_GE(m_gcForbiddenCount, 0u);
    m_gcForbiddenCount--;
  }
  bool isMixinInConstruction() const { return m_mixinsBeingConstructedCount; }
  void enterMixinConstructionScope() { m_mixinsBeingConstructedCount++; }
  void leaveMixinConstructionScope() {
    DCHECK_GT(m_mixinsBeingConstructedCount, 0u);
    m_mixinsBeingConstructedCount--;
  }
  bool sweepForbidden() const { return m_sweepForbidden; }

  class MainThreadGCForbiddenScope final {
    STACK_ALLOCATED();

   public:
    MainThreadGCForbiddenScope()
        : m_threadState(ThreadState::mainThreadState()) {
      m_threadState->enterGCForbiddenScope();
    }
    ~MainThreadGCForbiddenScope() { m_threadState->leaveGCForbiddenScope(); }

   private:
    ThreadState* const m_threadState;
  };

  class GCForbiddenScope final {
    STACK_ALLOCATED();

   public:
    explicit GCForbiddenScope(ThreadState* threadState)
        : m_threadState(threadState) {
      m_threadState->enterGCForbiddenScope();
    }
    ~GCForbiddenScope() { m_threadState->leaveGCForbiddenScope(); }

   private:
    ThreadState* const m_threadState;
  };

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
  //   - periodically check if GC is requested from another thread by calling a
  //     safePoint() method;
  //   - use SafePointScope around long running loops that have no safePoint()
  //     invocation inside, such loops must not touch any heap object;
  //
  // Check if GC is requested by another thread and pause this thread if this is
  // the case.  Can only be called when current thread is in a consistent state.
  void safePoint(BlinkGC::StackState);

  // Mark current thread as running inside safepoint.
  void enterSafePoint(BlinkGC::StackState, void*);
  void leaveSafePoint();

  void recordStackEnd(intptr_t* endOfStack) { m_endOfStack = endOfStack; }
  NO_SANITIZE_ADDRESS void copyStackUntilSafePointScope();

  // Get one of the heap structures for this thread.
  // The thread heap is split into multiple heap parts based on object types
  // and object sizes.
  BaseArena* arena(int arenaIndex) const {
    ASSERT(0 <= arenaIndex);
    ASSERT(arenaIndex < BlinkGC::NumberOfArenas);
    return m_arenas[arenaIndex];
  }

#if DCHECK_IS_ON()
  // Infrastructure to determine if an address is within one of the
  // address ranges for the Blink heap. If the address is in the Blink
  // heap the containing heap page is returned.
  BasePage* findPageFromAddress(Address);
  BasePage* findPageFromAddress(const void* pointer) {
    return findPageFromAddress(
        reinterpret_cast<Address>(const_cast<void*>(pointer)));
  }
#endif

  // A region of PersistentNodes allocated on the given thread.
  PersistentRegion* getPersistentRegion() const {
    return m_persistentRegion.get();
  }
  // A region of PersistentNodes not owned by any particular thread.

  // Visit local thread stack and trace all pointers conservatively.
  void visitStack(Visitor*);

  // Visit the asan fake stack frame corresponding to a slot on the
  // real machine stack if there is one.
  void visitAsanFakeStackForPointer(Visitor*, Address);

  // Visit all persistents allocated on this thread.
  void visitPersistents(Visitor*);

  struct GCSnapshotInfo {
    STACK_ALLOCATED();
    GCSnapshotInfo(size_t numObjectTypes);

    // Map from gcInfoIndex (vector-index) to count/size.
    Vector<int> liveCount;
    Vector<int> deadCount;
    Vector<size_t> liveSize;
    Vector<size_t> deadSize;
  };

  size_t objectPayloadSizeForTesting();

  void shouldFlushHeapDoesNotContainCache() {
    m_shouldFlushHeapDoesNotContainCache = true;
  }

  bool isAddressInHeapDoesNotContainCache(Address);

  void registerTraceDOMWrappers(
      v8::Isolate* isolate,
      void (*traceDOMWrappers)(v8::Isolate*, Visitor*),
      void (*invalidateDeadObjectsInWrappersMarkingDeque)(v8::Isolate*),
      void (*performCleanup)(v8::Isolate*)) {
    m_isolate = isolate;
    DCHECK(!m_isolate || traceDOMWrappers);
    DCHECK(!m_isolate || invalidateDeadObjectsInWrappersMarkingDeque);
    DCHECK(!m_isolate || performCleanup);
    m_traceDOMWrappers = traceDOMWrappers;
    m_invalidateDeadObjectsInWrappersMarkingDeque =
        invalidateDeadObjectsInWrappersMarkingDeque;
    m_performCleanup = performCleanup;
  }

  // By entering a gc-forbidden scope, conservative GCs will not
  // be allowed while handling an out-of-line allocation request.
  // Intended used when constructing subclasses of GC mixins, where
  // the object being constructed cannot be safely traced & marked
  // fully should a GC be allowed while its subclasses are being
  // constructed.
  void enterGCForbiddenScopeIfNeeded(
      GarbageCollectedMixinConstructorMarker* gcMixinMarker) {
    ASSERT(checkThread());
    if (!m_gcMixinMarker) {
      enterMixinConstructionScope();
      m_gcMixinMarker = gcMixinMarker;
    }
  }
  void leaveGCForbiddenScopeIfNeeded(
      GarbageCollectedMixinConstructorMarker* gcMixinMarker) {
    ASSERT(checkThread());
    if (m_gcMixinMarker == gcMixinMarker) {
      leaveMixinConstructionScope();
      m_gcMixinMarker = nullptr;
    }
  }

  // vectorBackingArena() returns an arena that the vector allocation should
  // use.  We have four vector arenas and want to choose the best arena here.
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
  //   that are placed immediately prior to the allocation point of each arena.
  //   Choose the arena where the vector is least likely to be expanded
  //   nor promptly freed.
  //
  // To implement the heuristics, we add an arenaAge to each arena. The arenaAge
  // is updated if:
  //
  // - a vector on the arena is expanded; or
  // - a vector that meets the condition (*) is allocated on the arena
  //
  //   (*) More than 33% of the same type of vectors have been promptly
  //       freed since the last GC.
  //
  BaseArena* vectorBackingArena(size_t gcInfoIndex) {
    ASSERT(checkThread());
    size_t entryIndex = gcInfoIndex & likelyToBePromptlyFreedArrayMask;
    --m_likelyToBePromptlyFreed[entryIndex];
    int arenaIndex = m_vectorBackingArenaIndex;
    // If m_likelyToBePromptlyFreed[entryIndex] > 0, that means that
    // more than 33% of vectors of the type have been promptly freed
    // since the last GC.
    if (m_likelyToBePromptlyFreed[entryIndex] > 0) {
      m_arenaAges[arenaIndex] = ++m_currentArenaAges;
      m_vectorBackingArenaIndex = arenaIndexOfVectorArenaLeastRecentlyExpanded(
          BlinkGC::Vector1ArenaIndex, BlinkGC::Vector4ArenaIndex);
    }
    ASSERT(isVectorArenaIndex(arenaIndex));
    return m_arenas[arenaIndex];
  }
  BaseArena* expandedVectorBackingArena(size_t gcInfoIndex);
  static bool isVectorArenaIndex(int arenaIndex) {
    return BlinkGC::Vector1ArenaIndex <= arenaIndex &&
           arenaIndex <= BlinkGC::Vector4ArenaIndex;
  }
  void allocationPointAdjusted(int arenaIndex);
  void promptlyFreed(size_t gcInfoIndex);

  void accumulateSweepingTime(double time) {
    m_accumulatedSweepingTime += time;
  }

  void freePersistentNode(PersistentNode*);

  using PersistentClearCallback = void (*)(void*);

  void registerStaticPersistentNode(PersistentNode*, PersistentClearCallback);
  void releaseStaticPersistentNodes();

#if defined(LEAK_SANITIZER)
  void enterStaticReferenceRegistrationDisabledScope();
  void leaveStaticReferenceRegistrationDisabledScope();
#endif

  void resetHeapCounters();
  void increaseAllocatedObjectSize(size_t);
  void decreaseAllocatedObjectSize(size_t);
  void increaseMarkedObjectSize(size_t);

  v8::Isolate* isolate() const { return m_isolate; }

  BlinkGC::StackState stackState() const { return m_stackState; }

  void collectGarbage(BlinkGC::StackState, BlinkGC::GCType, BlinkGC::GCReason);
  void collectAllGarbage();

  // Register the pre-finalizer for the |self| object. The class T must have
  // USING_PRE_FINALIZER().
  template <typename T>
  class PrefinalizerRegistration final {
   public:
    PrefinalizerRegistration(T* self) {
      static_assert(sizeof(&T::invokePreFinalizer) > 0,
                    "USING_PRE_FINALIZER(T) must be defined.");
      ThreadState* state = ThreadState::current();
#if DCHECK_IS_ON()
      DCHECK(state->checkThread());
#endif
      DCHECK(!state->sweepForbidden());
      DCHECK(!state->m_orderedPreFinalizers.contains(
          PreFinalizer(self, T::invokePreFinalizer)));
      state->m_orderedPreFinalizers.insert(
          PreFinalizer(self, T::invokePreFinalizer));
    }
  };

  static const char* gcReasonString(BlinkGC::GCReason);

  // Returns |true| if |object| resides on this thread's heap.
  // It is well-defined to call this method on any heap allocated
  // reference, provided its associated heap hasn't been detached
  // and shut down. Its behavior is undefined for any other pointer
  // value.
  bool isOnThreadHeap(const void* object) const {
    return &fromObject(object)->heap() == &heap();
  }

 private:
  template <typename T>
  friend class PrefinalizerRegistration;
  enum SnapshotType { HeapSnapshot, FreelistSnapshot };

  ThreadState();
  ~ThreadState();

  void clearSafePointScopeMarker() {
    m_safePointStackCopy.clear();
    m_safePointScopeMarker = nullptr;
  }

  // shouldSchedule{Precise,Idle}GC and shouldForceConservativeGC
  // implement the heuristics that are used to determine when to collect
  // garbage.
  // If shouldForceConservativeGC returns true, we force the garbage
  // collection immediately. Otherwise, if should*GC returns true, we
  // record that we should garbage collect the next time we return
  // to the event loop. If both return false, we don't need to
  // collect garbage at this point.
  bool shouldScheduleIdleGC();
  bool shouldSchedulePreciseGC();
  bool shouldForceConservativeGC();
  // V8 minor or major GC is likely to drop a lot of references to objects
  // on Oilpan's heap. We give a chance to schedule a GC.
  bool shouldScheduleV8FollowupGC();
  // Page navigation is likely to drop a lot of references to objects
  // on Oilpan's heap. We give a chance to schedule a GC.
  // estimatedRemovalRatio is the estimated ratio of objects that will be no
  // longer necessary due to the navigation.
  bool shouldSchedulePageNavigationGC(float estimatedRemovalRatio);

  // Internal helpers to handle memory pressure conditions.

  // Returns true if memory use is in a near-OOM state
  // (aka being under "memory pressure".)
  bool shouldForceMemoryPressureGC();

  // Returns true if shouldForceMemoryPressureGC() held and a
  // conservative GC was performed to handle the emergency.
  bool forceMemoryPressureGCIfNeeded();

  size_t estimatedLiveSize(size_t currentSize, size_t sizeAtLastGC);
  size_t totalMemorySize();
  double heapGrowingRate();
  double partitionAllocGrowingRate();
  bool judgeGCThreshold(size_t allocatedObjectSizeThreshold,
                        size_t totalMemorySizeThreshold,
                        double heapGrowingRateThreshold);

  void runScheduledGC(BlinkGC::StackState);

  void eagerSweep();

#if defined(ADDRESS_SANITIZER)
  void poisonEagerArena();
  void poisonAllHeaps();
#endif

  void removeAllPages();

  void invokePreFinalizers();

  void takeSnapshot(SnapshotType);
  void clearArenaAges();
  int arenaIndexOfVectorArenaLeastRecentlyExpanded(int beginArenaIndex,
                                                   int endArenaIndex);

  void reportMemoryToV8();

  friend class SafePointScope;

  static WTF::ThreadSpecific<ThreadState*>* s_threadSpecific;

  // We can't create a static member of type ThreadState here
  // because it will introduce global constructor and destructor.
  // We would like to manage lifetime of the ThreadState attached
  // to the main thread explicitly instead and still use normal
  // constructor and destructor for the ThreadState class.
  // For this we reserve static storage for the main ThreadState
  // and lazily construct ThreadState in it using placement new.
  static uint8_t s_mainThreadStateStorage[];

  std::unique_ptr<ThreadHeap> m_heap;
  ThreadIdentifier m_thread;
  std::unique_ptr<PersistentRegion> m_persistentRegion;
  BlinkGC::StackState m_stackState;
  intptr_t* m_startOfStack;
  intptr_t* m_endOfStack;

  void* m_safePointScopeMarker;
  Vector<Address> m_safePointStackCopy;
  bool m_sweepForbidden;
  size_t m_noAllocationCount;
  size_t m_gcForbiddenCount;
  size_t m_mixinsBeingConstructedCount;
  double m_accumulatedSweepingTime;

  BaseArena* m_arenas[BlinkGC::NumberOfArenas];
  int m_vectorBackingArenaIndex;
  size_t m_arenaAges[BlinkGC::NumberOfArenas];
  size_t m_currentArenaAges;

  GarbageCollectedMixinConstructorMarker* m_gcMixinMarker;

  bool m_shouldFlushHeapDoesNotContainCache;
  GCState m_gcState;

  using PreFinalizerCallback = bool (*)(void*);
  using PreFinalizer = std::pair<void*, PreFinalizerCallback>;

  // Pre-finalizers are called in the reverse order in which they are
  // registered by the constructors (including constructors of Mixin objects)
  // for an object, by processing the m_orderedPreFinalizers back-to-front.
  ListHashSet<PreFinalizer> m_orderedPreFinalizers;

  v8::Isolate* m_isolate;
  void (*m_traceDOMWrappers)(v8::Isolate*, Visitor*);
  void (*m_invalidateDeadObjectsInWrappersMarkingDeque)(v8::Isolate*);
  void (*m_performCleanup)(v8::Isolate*);

#if defined(ADDRESS_SANITIZER)
  void* m_asanFakeStack;
#endif

  // PersistentNodes that are stored in static references;
  // references that either have to be cleared upon the thread
  // detaching from Oilpan and shutting down or references we
  // have to clear before initiating LSan's leak detection.
  HashMap<PersistentNode*, PersistentClearCallback> m_staticPersistents;

#if defined(LEAK_SANITIZER)
  // Count that controls scoped disabling of persistent registration.
  size_t m_disabledStaticPersistentsRegistration;
#endif

  // Ideally we want to allocate an array of size |gcInfoTableMax| but it will
  // waste memory. Thus we limit the array size to 2^8 and share one entry
  // with multiple types of vectors. This won't be an issue in practice,
  // since there will be less than 2^8 types of objects in common cases.
  static const int likelyToBePromptlyFreedArraySize = (1 << 8);
  static const int likelyToBePromptlyFreedArrayMask =
      likelyToBePromptlyFreedArraySize - 1;
  std::unique_ptr<int[]> m_likelyToBePromptlyFreed;

  // Stats for heap memory of this thread.
  size_t m_allocatedObjectSize;
  size_t m_markedObjectSize;
  size_t m_reportedMemoryToV8;
};

template <ThreadAffinity affinity>
class ThreadStateFor;

template <>
class ThreadStateFor<MainThreadOnly> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ThreadState* state() {
    // This specialization must only be used from the main thread.
    ASSERT(ThreadState::current()->isMainThread());
    return ThreadState::mainThreadState();
  }
};

template <>
class ThreadStateFor<AnyThread> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ThreadState* state() { return ThreadState::current(); }
};

}  // namespace blink

#endif  // ThreadState_h
