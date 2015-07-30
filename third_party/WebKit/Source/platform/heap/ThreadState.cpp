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

#include "config.h"
#include "platform/heap/ThreadState.h"

#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/heap/AddressSanitizer.h"
#include "platform/heap/CallbackStack.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/heap/SafePoint.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Partitions.h"
#include "wtf/ThreadingPrimitives.h"
#if ENABLE(GC_PROFILING)
#include "platform/TracedValue.h"
#include "wtf/text/StringHash.h"
#endif

#if OS(WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#elif defined(__GLIBC__)
extern "C" void* __libc_stack_end;  // NOLINT
#endif

#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#endif

#if ENABLE(GC_PROFILING)
#include <limits>
#endif

#if OS(FREEBSD)
#include <pthread_np.h>
#endif

namespace blink {

WTF::ThreadSpecific<ThreadState*>* ThreadState::s_threadSpecific = nullptr;
uintptr_t ThreadState::s_mainThreadStackStart = 0;
uintptr_t ThreadState::s_mainThreadUnderestimatedStackSize = 0;
uint8_t ThreadState::s_mainThreadStateStorage[sizeof(ThreadState)];
SafePointBarrier* ThreadState::s_safePointBarrier = nullptr;

static Mutex& threadAttachMutex()
{
    AtomicallyInitializedStaticReference(Mutex, mutex, (new Mutex));
    return mutex;
}

ThreadState::ThreadState()
    : m_thread(currentThread())
    , m_persistents(adoptPtr(new PersistentAnchor()))
    , m_startOfStack(reinterpret_cast<intptr_t*>(StackFrameDepth::getStackStart()))
    , m_endOfStack(reinterpret_cast<intptr_t*>(StackFrameDepth::getStackStart()))
    , m_safePointScopeMarker(nullptr)
    , m_atSafePoint(false)
    , m_interruptors()
    , m_sweepForbidden(false)
    , m_noAllocationCount(0)
    , m_gcForbiddenCount(0)
    , m_vectorBackingHeapIndex(Vector1HeapIndex)
    , m_currentHeapAges(0)
    , m_isTerminating(false)
    , m_gcMixinMarker(nullptr)
    , m_shouldFlushHeapDoesNotContainCache(false)
    , m_gcState(NoGCScheduled)
    , m_traceDOMWrappers(nullptr)
#if defined(ADDRESS_SANITIZER)
    , m_asanFakeStack(__asan_get_current_fake_stack())
#endif
#if ENABLE(GC_PROFILING)
    , m_nextFreeListSnapshotTime(-std::numeric_limits<double>::infinity())
#endif
{
    checkThread();
    ASSERT(!**s_threadSpecific);
    **s_threadSpecific = this;

    if (isMainThread()) {
        s_mainThreadStackStart = reinterpret_cast<uintptr_t>(m_startOfStack) - sizeof(void*);
        size_t underestimatedStackSize = StackFrameDepth::getUnderestimatedStackSize();
        if (underestimatedStackSize > sizeof(void*))
            s_mainThreadUnderestimatedStackSize = underestimatedStackSize - sizeof(void*);
    }

    for (int heapIndex = 0; heapIndex < LargeObjectHeapIndex; heapIndex++)
        m_heaps[heapIndex] = new NormalPageHeap(this, heapIndex);
    m_heaps[LargeObjectHeapIndex] = new LargeObjectHeap(this, LargeObjectHeapIndex);

    m_likelyToBePromptlyFreed = adoptArrayPtr(new int[likelyToBePromptlyFreedArraySize]);
    clearHeapAges();

    m_weakCallbackStack = new CallbackStack();
}

ThreadState::~ThreadState()
{
    checkThread();
    delete m_weakCallbackStack;
    m_weakCallbackStack = nullptr;
    for (int i = 0; i < NumberOfHeaps; ++i)
        delete m_heaps[i];
    deleteAllValues(m_interruptors);
    **s_threadSpecific = nullptr;
    if (isMainThread()) {
        s_mainThreadStackStart = 0;
        s_mainThreadUnderestimatedStackSize = 0;
    }
}

void ThreadState::init()
{
    s_threadSpecific = new WTF::ThreadSpecific<ThreadState*>();
    s_safePointBarrier = new SafePointBarrier;
}

void ThreadState::shutdown()
{
    delete s_safePointBarrier;
    s_safePointBarrier = nullptr;

    // Thread-local storage shouldn't be disposed, so we don't call ~ThreadSpecific().
}

void ThreadState::attachMainThread()
{
    RELEASE_ASSERT(!Heap::s_shutdownCalled);
    MutexLocker locker(threadAttachMutex());
    ThreadState* state = new(s_mainThreadStateStorage) ThreadState();
    attachedThreads().add(state);
}

void ThreadState::detachMainThread()
{
    // Enter a safe point before trying to acquire threadAttachMutex
    // to avoid dead lock if another thread is preparing for GC, has acquired
    // threadAttachMutex and waiting for other threads to pause or reach a
    // safepoint.
    ThreadState* state = mainThreadState();

    // 1. Finish sweeping.
    state->completeSweep();
    {
        SafePointAwareMutexLocker locker(threadAttachMutex(), NoHeapPointersOnStack);

        // 2. Add the main thread's heap pages to the orphaned pool.
        state->cleanupPages();

        // 3. Detach the main thread.
        ASSERT(attachedThreads().contains(state));
        attachedThreads().remove(state);
        state->~ThreadState();
    }
    shutdownHeapIfNecessary();
}

void ThreadState::shutdownHeapIfNecessary()
{
    // We don't need to enter a safe point before acquiring threadAttachMutex
    // because this thread is already detached.

    MutexLocker locker(threadAttachMutex());
    // We start shutting down the heap if there is no running thread
    // and Heap::shutdown() is already called.
    if (!attachedThreads().size() && Heap::s_shutdownCalled)
        Heap::doShutdown();
}

void ThreadState::attach()
{
    RELEASE_ASSERT(!Heap::s_shutdownCalled);
    MutexLocker locker(threadAttachMutex());
    ThreadState* state = new ThreadState();
    attachedThreads().add(state);
}

void ThreadState::cleanupPages()
{
    checkThread();
    for (int i = 0; i < NumberOfHeaps; ++i)
        m_heaps[i]->cleanupPages();
}

void ThreadState::cleanup()
{
    checkThread();
    {
        // Grab the threadAttachMutex to ensure only one thread can shutdown at
        // a time and that no other thread can do a global GC. It also allows
        // safe iteration of the attachedThreads set which happens as part of
        // thread local GC asserts. We enter a safepoint while waiting for the
        // lock to avoid a dead-lock where another thread has already requested
        // GC.
        SafePointAwareMutexLocker locker(threadAttachMutex(), NoHeapPointersOnStack);

        // Finish sweeping.
        completeSweep();

        // From here on ignore all conservatively discovered
        // pointers into the heap owned by this thread.
        m_isTerminating = true;

        // Set the terminate flag on all heap pages of this thread. This is used to
        // ensure we don't trace pages on other threads that are not part of the
        // thread local GC.
        prepareHeapForTermination();

        // Do thread local GC's as long as the count of thread local Persistents
        // changes and is above zero.
        PersistentAnchor* anchor = static_cast<PersistentAnchor*>(m_persistents.get());
        int oldCount = -1;
        int currentCount = anchor->numberOfPersistents();
        ASSERT(currentCount >= 0);
        while (currentCount != oldCount) {
            Heap::collectGarbageForTerminatingThread(this);
            oldCount = currentCount;
            currentCount = anchor->numberOfPersistents();
        }
        // We should not have any persistents left when getting to this point,
        // if we have it is probably a bug so adding a debug ASSERT to catch this.
        ASSERT(!currentCount);
        // All of pre-finalizers should be consumed.
        ASSERT(m_preFinalizers.isEmpty());
        RELEASE_ASSERT(gcState() == NoGCScheduled);

        // Add pages to the orphaned page pool to ensure any global GCs from this point
        // on will not trace objects on this thread's heaps.
        cleanupPages();

        ASSERT(attachedThreads().contains(this));
        attachedThreads().remove(this);
    }
}

void ThreadState::detach()
{
    ThreadState* state = current();
    state->cleanup();
    RELEASE_ASSERT(state->gcState() == ThreadState::NoGCScheduled);
    delete state;
    shutdownHeapIfNecessary();
}

void ThreadState::visitPersistentRoots(Visitor* visitor)
{
    TRACE_EVENT0("blink_gc", "ThreadState::visitPersistentRoots");
    {
        // All threads are at safepoints so this is not strictly necessary.
        // However we acquire the mutex to make mutation and traversal of this
        // list symmetrical.
        MutexLocker locker(globalRootsMutex());
        globalRoots().trace(visitor);
    }

    for (ThreadState* state : attachedThreads())
        state->visitPersistents(visitor);
}

void ThreadState::visitStackRoots(Visitor* visitor)
{
    TRACE_EVENT0("blink_gc", "ThreadState::visitStackRoots");
    for (ThreadState* state : attachedThreads())
        state->visitStack(visitor);
}

NO_SANITIZE_ADDRESS
void ThreadState::visitAsanFakeStackForPointer(Visitor* visitor, Address ptr)
{
#if defined(ADDRESS_SANITIZER)
    Address* start = reinterpret_cast<Address*>(m_startOfStack);
    Address* end = reinterpret_cast<Address*>(m_endOfStack);
    Address* fakeFrameStart = nullptr;
    Address* fakeFrameEnd = nullptr;
    Address* maybeFakeFrame = reinterpret_cast<Address*>(ptr);
    Address* realFrameForFakeFrame =
        reinterpret_cast<Address*>(
            __asan_addr_is_in_fake_stack(
                m_asanFakeStack, maybeFakeFrame,
                reinterpret_cast<void**>(&fakeFrameStart),
                reinterpret_cast<void**>(&fakeFrameEnd)));
    if (realFrameForFakeFrame) {
        // This is a fake frame from the asan fake stack.
        if (realFrameForFakeFrame > end && start > realFrameForFakeFrame) {
            // The real stack address for the asan fake frame is
            // within the stack range that we need to scan so we need
            // to visit the values in the fake frame.
            for (Address* p = fakeFrameStart; p < fakeFrameEnd; ++p)
                Heap::checkAndMarkPointer(visitor, *p);
        }
    }
#endif
}

NO_SANITIZE_ADDRESS
void ThreadState::visitStack(Visitor* visitor)
{
    if (m_stackState == NoHeapPointersOnStack)
        return;

    Address* start = reinterpret_cast<Address*>(m_startOfStack);
    // If there is a safepoint scope marker we should stop the stack
    // scanning there to not touch active parts of the stack. Anything
    // interesting beyond that point is in the safepoint stack copy.
    // If there is no scope marker the thread is blocked and we should
    // scan all the way to the recorded end stack pointer.
    Address* end = reinterpret_cast<Address*>(m_endOfStack);
    Address* safePointScopeMarker = reinterpret_cast<Address*>(m_safePointScopeMarker);
    Address* current = safePointScopeMarker ? safePointScopeMarker : end;

    // Ensure that current is aligned by address size otherwise the loop below
    // will read past start address.
    current = reinterpret_cast<Address*>(reinterpret_cast<intptr_t>(current) & ~(sizeof(Address) - 1));

    for (; current < start; ++current) {
        Address ptr = *current;
#if defined(MEMORY_SANITIZER)
        // |ptr| may be uninitialized by design. Mark it as initialized to keep
        // MSan from complaining.
        // Note: it may be tempting to get rid of |ptr| and simply use |current|
        // here, but that would be incorrect. We intentionally use a local
        // variable because we don't want to unpoison the original stack.
        __msan_unpoison(&ptr, sizeof(ptr));
#endif
        Heap::checkAndMarkPointer(visitor, ptr);
        visitAsanFakeStackForPointer(visitor, ptr);
    }

    for (Address ptr : m_safePointStackCopy) {
#if defined(MEMORY_SANITIZER)
        // See the comment above.
        __msan_unpoison(&ptr, sizeof(ptr));
#endif
        Heap::checkAndMarkPointer(visitor, ptr);
        visitAsanFakeStackForPointer(visitor, ptr);
    }
}

void ThreadState::visitPersistents(Visitor* visitor)
{
    m_persistents->trace(visitor);
    if (m_traceDOMWrappers) {
        TRACE_EVENT0("blink_gc", "V8GCController::traceDOMWrappers");
        m_traceDOMWrappers(m_isolate, visitor);
    }
}

#if ENABLE(GC_PROFILING)
const GCInfo* ThreadState::findGCInfo(Address address)
{
    if (BasePage* page = findPageFromAddress(address))
        return page->findGCInfo(address);
    return nullptr;
}

size_t ThreadState::SnapshotInfo::getClassTag(const GCInfo* gcInfo)
{
    ClassTagMap::AddResult result = classTags.add(gcInfo, classTags.size());
    if (result.isNewEntry) {
        liveCount.append(0);
        deadCount.append(0);
        liveSize.append(0);
        deadSize.append(0);
        generations.append(GenerationCountsVector());
        generations.last().fill(0, 8);
    }
    return result.storedValue->value;
}

void ThreadState::snapshot()
{
    SnapshotInfo info(this);
    RefPtr<TracedValue> json = TracedValue::create();

#define SNAPSHOT_HEAP(HeapType)                                    \
    {                                                              \
        json->beginDictionary();                                   \
        json->setString("name", #HeapType);                        \
        m_heaps[HeapType##HeapIndex]->snapshot(json.get(), &info); \
        json->endDictionary();                                     \
    }
    json->beginArray("heaps");
    SNAPSHOT_HEAP(NormalPage);
    SNAPSHOT_HEAP(Vector1);
    SNAPSHOT_HEAP(Vector2);
    SNAPSHOT_HEAP(Vector3);
    SNAPSHOT_HEAP(Vector4);
    SNAPSHOT_HEAP(InlineVector);
    SNAPSHOT_HEAP(HashTable);
    SNAPSHOT_HEAP(LargeObject);
    FOR_EACH_TYPED_HEAP(SNAPSHOT_HEAP);
    json->endArray();
#undef SNAPSHOT_HEAP

    json->setInteger("allocatedSpace", Heap::allocatedSpace());
    json->setInteger("objectSpace", Heap::allocatedObjectSize());
    json->setInteger("pageCount", info.pageCount);
    json->setInteger("freeSize", info.freeSize);

    Vector<String> classNameVector(info.classTags.size());
    for (SnapshotInfo::ClassTagMap::iterator it = info.classTags.begin(); it != info.classTags.end(); ++it)
        classNameVector[it->value] = it->key->m_className;

    size_t liveSize = 0;
    size_t deadSize = 0;
    json->beginArray("classes");
    for (size_t i = 0; i < classNameVector.size(); ++i) {
        json->beginDictionary();
        json->setString("name", classNameVector[i]);
        json->setInteger("liveCount", info.liveCount[i]);
        json->setInteger("deadCount", info.deadCount[i]);
        json->setInteger("liveSize", info.liveSize[i]);
        json->setInteger("deadSize", info.deadSize[i]);
        liveSize += info.liveSize[i];
        deadSize += info.deadSize[i];

        json->beginArray("generations");
        for (size_t j = 0; j < numberOfGenerationsToTrack; ++j)
            json->pushInteger(info.generations[i][j]);
        json->endArray();
        json->endDictionary();
    }
    json->endArray();
    json->setInteger("liveSize", liveSize);
    json->setInteger("deadSize", deadSize);

    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID("blink_gc", "ThreadState", this, json.release());
}

void ThreadState::incrementMarkedObjectsAge()
{
    for (int i = 0; i < NumberOfHeaps; ++i)
        m_heaps[i]->incrementMarkedObjectsAge();
}
#endif

void ThreadState::pushWeakPointerCallback(void* object, WeakPointerCallback callback)
{
    CallbackStack::Item* slot = m_weakCallbackStack->allocateEntry();
    *slot = CallbackStack::Item(object, callback);
}

bool ThreadState::popAndInvokeWeakPointerCallback(Visitor* visitor)
{
    // For weak processing we should never reach orphaned pages since orphaned
    // pages are not traced and thus objects on those pages are never be
    // registered as objects on orphaned pages. We cannot assert this here since
    // we might have an off-heap collection. We assert it in
    // Heap::pushWeakPointerCallback.
    if (CallbackStack::Item* item = m_weakCallbackStack->pop()) {
        item->call(visitor);
        return true;
    }
    return false;
}

PersistentNode& ThreadState::globalRoots()
{
    AtomicallyInitializedStaticReference(PersistentNode, anchor, new PersistentAnchor);
    return anchor;
}

Mutex& ThreadState::globalRootsMutex()
{
    AtomicallyInitializedStaticReference(Mutex, mutex, new Mutex);
    return mutex;
}

// TODO(haraken): We should improve the GC heuristics.
// These heuristics affect performance significantly.
bool ThreadState::shouldScheduleIdleGC()
{
    if (gcState() != NoGCScheduled)
        return false;
#if ENABLE(OILPAN)
    // The estimated size is updated when the main thread finishes lazy
    // sweeping. If this thread reaches here before the main thread finishes
    // lazy sweeping, the thread will use the estimated size of the last GC.
    size_t estimatedLiveObjectSize = Heap::estimatedLiveObjectSize();
    size_t allocatedObjectSize = Heap::allocatedObjectSize();
    // Heap::markedObjectSize() may be underestimated if any thread has not
    // finished completeSweep().
    size_t currentObjectSize = allocatedObjectSize + Heap::markedObjectSize() + WTF::Partitions::totalSizeOfCommittedPages();
    // Schedule an idle GC if Oilpan has allocated more than 1 MB since
    // the last GC and the current memory usage is >50% larger than
    // the estimated live memory usage.
    return allocatedObjectSize >= 1024 * 1024 && currentObjectSize > estimatedLiveObjectSize * 3 / 2;
#else
    return false;
#endif
}

// TODO(haraken): We should improve the GC heuristics.
// These heuristics affect performance significantly.
bool ThreadState::shouldSchedulePreciseGC()
{
    if (gcState() != NoGCScheduled)
        return false;
#if ENABLE(OILPAN)
    return false;
#else
    // The estimated size is updated when the main thread finishes lazy
    // sweeping. If this thread reaches here before the main thread finishes
    // lazy sweeping, the thread will use the estimated size of the last GC.
    size_t estimatedLiveObjectSize = Heap::estimatedLiveObjectSize();
    size_t allocatedObjectSize = Heap::allocatedObjectSize();
    // Heap::markedObjectSize() may be underestimated if any thread has not
    // finished completeSweep().
    size_t currentObjectSize = allocatedObjectSize + Heap::markedObjectSize() + WTF::Partitions::totalSizeOfCommittedPages();
    // Schedule a precise GC if Oilpan has allocated more than 1 MB since
    // the last GC and the current memory usage is >50% larger than
    // the estimated live memory usage.
    return allocatedObjectSize >= 1024 * 1024 && currentObjectSize > estimatedLiveObjectSize * 3 / 2;
#endif
}

// TODO(haraken): We should improve the GC heuristics.
// These heuristics affect performance significantly.
bool ThreadState::shouldForceConservativeGC()
{
    if (UNLIKELY(isGCForbidden()))
        return false;

    // The estimated size is updated when the main thread finishes lazy
    // sweeping. If this thread reaches here before the main thread finishes
    // lazy sweeping, the thread will use the estimated size of the last GC.
    size_t estimatedLiveObjectSize = Heap::estimatedLiveObjectSize();
    size_t allocatedObjectSize = Heap::allocatedObjectSize();
    // Heap::markedObjectSize() may be underestimated if any thread has not
    // finished completeSweep().
    size_t currentObjectSize = allocatedObjectSize + Heap::markedObjectSize() + WTF::Partitions::totalSizeOfCommittedPages();
    if (currentObjectSize >= 300 * 1024 * 1024) {
        // If we're consuming too much memory, trigger a conservative GC
        // aggressively. This is a safe guard to avoid OOM.
        return currentObjectSize > estimatedLiveObjectSize * 3 / 2;
    }
    // Schedule a conservative GC if Oilpan has allocated more than 32 MB since
    // the last GC and the current memory usage is >400% larger than
    // the estimated live memory usage.
    // TODO(haraken): 400% is too large. Lower the heap growing factor.
    return allocatedObjectSize >= 32 * 1024 * 1024 && currentObjectSize > 5 * estimatedLiveObjectSize;
}

void ThreadState::scheduleGCIfNeeded()
{
    checkThread();
    // Allocation is allowed during sweeping, but those allocations should not
    // trigger nested GCs.
    if (isSweepingInProgress())
        return;
    ASSERT(!sweepForbidden());

    if (shouldForceConservativeGC()) {
        Heap::collectGarbage(HeapPointersOnStack, GCWithoutSweep, Heap::ConservativeGC);
        return;
    }
    if (shouldSchedulePreciseGC())
        schedulePreciseGC();
    else if (shouldScheduleIdleGC())
        scheduleIdleGC();
}

void ThreadState::performIdleGC(double deadlineSeconds)
{
    ASSERT(isMainThread());

    if (gcState() != IdleGCScheduled)
        return;

    double idleDeltaInSeconds = deadlineSeconds - Platform::current()->monotonicallyIncreasingTime();
    if (idleDeltaInSeconds <= Heap::estimatedMarkingTime() && !Platform::current()->currentThread()->scheduler()->canExceedIdleDeadlineIfRequired()) {
        // If marking is estimated to take longer than the deadline and we can't
        // exceed the deadline, then reschedule for the next idle period.
        scheduleIdleGC();
        return;
    }

    Heap::collectGarbage(NoHeapPointersOnStack, GCWithoutSweep, Heap::IdleGC);
}

void ThreadState::performIdleLazySweep(double deadlineSeconds)
{
    ASSERT(isMainThread());

    // If we are not in a sweeping phase, there is nothing to do here.
    if (!isSweepingInProgress())
        return;

    // This check is here to prevent performIdleLazySweep() from being called
    // recursively. I'm not sure if it can happen but it would be safer to have
    // the check just in case.
    if (sweepForbidden())
        return;

    bool sweepCompleted = true;
    ThreadState::SweepForbiddenScope scope(this);
    {
        if (isMainThread())
            ScriptForbiddenScope::enter();

        for (int i = 0; i < NumberOfHeaps; i++) {
            // lazySweepWithDeadline() won't check the deadline until it sweeps
            // 10 pages. So we give a small slack for safety.
            double slack = 0.001;
            double remainingBudget = deadlineSeconds - slack - Platform::current()->monotonicallyIncreasingTime();
            if (remainingBudget <= 0 || !m_heaps[i]->lazySweepWithDeadline(deadlineSeconds)) {
                // We couldn't finish the sweeping within the deadline.
                // We request another idle task for the remaining sweeping.
                scheduleIdleLazySweep();
                sweepCompleted = false;
                break;
            }
        }

        if (isMainThread())
            ScriptForbiddenScope::exit();
    }

    if (sweepCompleted)
        postSweep();
}

void ThreadState::scheduleIdleGC()
{
    // Idle GC is supported only in the main thread.
    if (!isMainThread())
        return;

    if (isSweepingInProgress()) {
        setGCState(SweepingAndIdleGCScheduled);
        return;
    }

    Platform::current()->currentThread()->scheduler()->postNonNestableIdleTask(FROM_HERE, WTF::bind<double>(&ThreadState::performIdleGC, this));
    setGCState(IdleGCScheduled);
}

void ThreadState::scheduleIdleLazySweep()
{
    // Idle complete sweep is supported only in the main thread.
    if (!isMainThread())
        return;

    // TODO(haraken): Remove this. Lazy sweeping is not yet enabled in non-oilpan builds.
#if ENABLE(OILPAN)
    Platform::current()->currentThread()->scheduler()->postIdleTask(FROM_HERE, WTF::bind<double>(&ThreadState::performIdleLazySweep, this));
#endif
}

void ThreadState::schedulePreciseGC()
{
    if (isSweepingInProgress()) {
        setGCState(SweepingAndPreciseGCScheduled);
        return;
    }

    setGCState(PreciseGCScheduled);
}

namespace {

#define UNEXPECTED_GCSTATE(s) case ThreadState::s: RELEASE_ASSERT_WITH_MESSAGE(false, "Unexpected transition while in GCState " #s); return

void unexpectedGCState(ThreadState::GCState gcState)
{
    switch (gcState) {
        UNEXPECTED_GCSTATE(NoGCScheduled);
        UNEXPECTED_GCSTATE(IdleGCScheduled);
        UNEXPECTED_GCSTATE(PreciseGCScheduled);
        UNEXPECTED_GCSTATE(FullGCScheduled);
        UNEXPECTED_GCSTATE(StoppingOtherThreads);
        UNEXPECTED_GCSTATE(GCRunning);
        UNEXPECTED_GCSTATE(EagerSweepScheduled);
        UNEXPECTED_GCSTATE(LazySweepScheduled);
        UNEXPECTED_GCSTATE(Sweeping);
        UNEXPECTED_GCSTATE(SweepingAndIdleGCScheduled);
        UNEXPECTED_GCSTATE(SweepingAndPreciseGCScheduled);
    default:
        ASSERT_NOT_REACHED();
        return;
    }
}

#undef UNEXPECTED_GCSTATE

} // namespace

#define VERIFY_STATE_TRANSITION(condition) if (UNLIKELY(!(condition))) unexpectedGCState(m_gcState)

void ThreadState::setGCState(GCState gcState)
{
    switch (gcState) {
    case NoGCScheduled:
        checkThread();
        VERIFY_STATE_TRANSITION(m_gcState == StoppingOtherThreads || m_gcState == Sweeping || m_gcState == SweepingAndIdleGCScheduled);
        break;
    case IdleGCScheduled:
    case PreciseGCScheduled:
    case FullGCScheduled:
        checkThread();
        VERIFY_STATE_TRANSITION(m_gcState == NoGCScheduled || m_gcState == IdleGCScheduled || m_gcState == PreciseGCScheduled || m_gcState == FullGCScheduled || m_gcState == StoppingOtherThreads || m_gcState == SweepingAndIdleGCScheduled || m_gcState == SweepingAndPreciseGCScheduled);
        completeSweep();
        break;
    case StoppingOtherThreads:
        checkThread();
        VERIFY_STATE_TRANSITION(m_gcState == NoGCScheduled || m_gcState == IdleGCScheduled || m_gcState == PreciseGCScheduled || m_gcState == FullGCScheduled || m_gcState == Sweeping || m_gcState == SweepingAndIdleGCScheduled || m_gcState == SweepingAndPreciseGCScheduled);
        break;
    case GCRunning:
        ASSERT(!isInGC());
        VERIFY_STATE_TRANSITION(m_gcState != GCRunning);
        break;
    case EagerSweepScheduled:
    case LazySweepScheduled:
        ASSERT(isInGC());
        VERIFY_STATE_TRANSITION(m_gcState == GCRunning);
        break;
    case Sweeping:
        checkThread();
        VERIFY_STATE_TRANSITION(m_gcState == StoppingOtherThreads || m_gcState == EagerSweepScheduled || m_gcState == LazySweepScheduled);
        break;
    case SweepingAndIdleGCScheduled:
    case SweepingAndPreciseGCScheduled:
        checkThread();
        VERIFY_STATE_TRANSITION(m_gcState == StoppingOtherThreads || m_gcState == Sweeping || m_gcState == SweepingAndIdleGCScheduled || m_gcState == SweepingAndPreciseGCScheduled);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    m_gcState = gcState;
}

#undef VERIFY_STATE_TRANSITION

ThreadState::GCState ThreadState::gcState() const
{
    return m_gcState;
}

void ThreadState::didV8GC()
{
    checkThread();
    if (isMainThread()) {
        // Lower the estimated live object size because the V8 major GC is
        // expected to have collected a lot of DOM wrappers and dropped
        // references to their DOM objects.
        Heap::setEstimatedLiveObjectSize(Heap::estimatedLiveObjectSize() / 2);
    }
}

void ThreadState::runScheduledGC(StackState stackState)
{
    checkThread();
    if (stackState != NoHeapPointersOnStack)
        return;

    switch (gcState()) {
    case FullGCScheduled:
        Heap::collectAllGarbage();
        break;
    case PreciseGCScheduled:
        Heap::collectGarbage(NoHeapPointersOnStack, GCWithoutSweep, Heap::PreciseGC);
        break;
    case IdleGCScheduled:
        // Idle time GC will be scheduled by Blink Scheduler.
        break;
    default:
        break;
    }
}

void ThreadState::prepareRegionTree()
{
    // Add the regions allocated by this thread to the region search tree.
    for (PageMemoryRegion* region : m_allocatedRegionsSinceLastGC)
        Heap::addPageMemoryRegion(region);
    m_allocatedRegionsSinceLastGC.clear();
}

void ThreadState::flushHeapDoesNotContainCacheIfNeeded()
{
    if (m_shouldFlushHeapDoesNotContainCache) {
        Heap::flushHeapDoesNotContainCache();
        m_shouldFlushHeapDoesNotContainCache = false;
    }
}

void ThreadState::makeConsistentForSweeping()
{
    ASSERT(isInGC());
    TRACE_EVENT0("blink_gc", "ThreadState::makeConsistentForSweeping");
    for (int i = 0; i < NumberOfHeaps; ++i)
        m_heaps[i]->makeConsistentForSweeping();
}

void ThreadState::preGC()
{
    ASSERT(!isInGC());
    setGCState(GCRunning);
    makeConsistentForSweeping();
    prepareRegionTree();
    flushHeapDoesNotContainCacheIfNeeded();
    clearHeapAges();
}

void ThreadState::postGC(GCType gcType)
{
    ASSERT(isInGC());

#if ENABLE(GC_PROFILING)
    // We snapshot the heap prior to sweeping to get numbers for both resources
    // that have been allocated since the last GC and for resources that are
    // going to be freed.
    bool gcTracingEnabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink_gc", &gcTracingEnabled);

    if (gcTracingEnabled) {
        bool disabledByDefaultGCTracingEnabled;
        TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("blink_gc"), &disabledByDefaultGCTracingEnabled);

        snapshot();
        if (disabledByDefaultGCTracingEnabled)
            collectAndReportMarkSweepStats();
        incrementMarkedObjectsAge();
    }
#endif

    setGCState(gcType == GCWithSweep ? EagerSweepScheduled : LazySweepScheduled);
    for (int i = 0; i < NumberOfHeaps; i++)
        m_heaps[i]->prepareForSweep();
}

void ThreadState::preSweep()
{
    checkThread();
    if (gcState() != EagerSweepScheduled && gcState() != LazySweepScheduled)
        return;

    {
        if (isMainThread())
            ScriptForbiddenScope::enter();

        SweepForbiddenScope forbiddenScope(this);
        {
            // Disallow allocation during weak processing.
            NoAllocationScope noAllocationScope(this);
            {
                TRACE_EVENT0("blink_gc", "ThreadState::threadLocalWeakProcessing");
                // Perform thread-specific weak processing.
                while (popAndInvokeWeakPointerCallback(Heap::s_markingVisitor)) { }
            }
            {
                TRACE_EVENT0("blink_gc", "ThreadState::invokePreFinalizers");
                invokePreFinalizers(*Heap::s_markingVisitor);
            }
        }

        if (isMainThread())
            ScriptForbiddenScope::exit();
    }

#if defined(ADDRESS_SANITIZER)
// TODO(haraken): Currently we cannot enable the poisoning because we have
// real bugs where destructors touch other on-heap objects. Remove all the bugs
// and enable the poisoning.
#if 0
    for (int i = 0; i < NumberOfHeaps; i++)
        m_heaps[i]->poisonUnmarkedObjects();
#endif
#endif

#if ENABLE(OILPAN)
    if (gcState() == EagerSweepScheduled) {
        // Eager sweeping should happen only in testing.
        setGCState(Sweeping);
        completeSweep();
    } else {
        // The default behavior is lazy sweeping.
        setGCState(Sweeping);
        scheduleIdleLazySweep();
    }
#else
    // FIXME: For now, we disable lazy sweeping in non-oilpan builds
    // to avoid unacceptable behavior regressions on trunk.
    setGCState(Sweeping);
    completeSweep();
#endif

#if ENABLE(GC_PROFILING)
    snapshotFreeListIfNecessary();
#endif
}

void ThreadState::completeSweep()
{
    // If we are not in a sweeping phase, there is nothing to do here.
    if (!isSweepingInProgress())
        return;

    // completeSweep() can be called recursively if finalizers can allocate
    // memory and the allocation triggers completeSweep(). This check prevents
    // the sweeping from being executed recursively.
    if (sweepForbidden())
        return;

    ThreadState::SweepForbiddenScope scope(this);
    {
        if (isMainThread())
            ScriptForbiddenScope::enter();

        TRACE_EVENT0("blink_gc", "ThreadState::completeSweep");
        double timeStamp = WTF::currentTimeMS();

        for (int i = 0; i < NumberOfHeaps; i++)
            m_heaps[i]->completeSweep();

        Platform::current()->histogramCustomCounts("BlinkGC.CompleteSweep", WTF::currentTimeMS() - timeStamp, 0, 10 * 1000, 50);

        if (isMainThread())
            ScriptForbiddenScope::exit();
    }

    postSweep();
}

void ThreadState::postSweep()
{
    if (isMainThread()) {
        // At the point where the main thread finishes lazy sweeping,
        // we estimate the live object size. Heap::markedObjectSize()
        // may be underestimated if any other thread has not finished
        // lazy sweeping.
        Heap::setEstimatedLiveObjectSize(Heap::markedObjectSize() + Heap::externalObjectSizeAtLastGC());
    }

    switch (gcState()) {
    case Sweeping:
        setGCState(NoGCScheduled);
        break;
    case SweepingAndPreciseGCScheduled:
        setGCState(PreciseGCScheduled);
        break;
    case SweepingAndIdleGCScheduled:
        setGCState(NoGCScheduled);
        scheduleIdleGC();
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void ThreadState::prepareHeapForTermination()
{
    checkThread();
    for (int i = 0; i < NumberOfHeaps; ++i)
        m_heaps[i]->prepareHeapForTermination();
}

#if ENABLE(ASSERT) || ENABLE(GC_PROFILING)
BasePage* ThreadState::findPageFromAddress(Address address)
{
    for (int i = 0; i < NumberOfHeaps; ++i) {
        if (BasePage* page = m_heaps[i]->findPageFromAddress(address))
            return page;
    }
    return nullptr;
}
#endif

size_t ThreadState::objectPayloadSizeForTesting()
{
    size_t objectPayloadSize = 0;
    for (int i = 0; i < NumberOfHeaps; ++i)
        objectPayloadSize += m_heaps[i]->objectPayloadSizeForTesting();
    return objectPayloadSize;
}

bool ThreadState::stopThreads()
{
    return s_safePointBarrier->parkOthers();
}

void ThreadState::resumeThreads()
{
    s_safePointBarrier->resumeOthers();
}

void ThreadState::safePoint(StackState stackState)
{
    checkThread();
    runScheduledGC(stackState);
    ASSERT(!m_atSafePoint);
    m_stackState = stackState;
    m_atSafePoint = true;
    s_safePointBarrier->checkAndPark(this);
    m_atSafePoint = false;
    m_stackState = HeapPointersOnStack;
    preSweep();
}

#ifdef ADDRESS_SANITIZER
// When we are running under AddressSanitizer with detect_stack_use_after_return=1
// then stack marker obtained from SafePointScope will point into a fake stack.
// Detect this case by checking if it falls in between current stack frame
// and stack start and use an arbitrary high enough value for it.
// Don't adjust stack marker in any other case to match behavior of code running
// without AddressSanitizer.
NO_SANITIZE_ADDRESS static void* adjustScopeMarkerForAdressSanitizer(void* scopeMarker)
{
    Address start = reinterpret_cast<Address>(StackFrameDepth::getStackStart());
    Address end = reinterpret_cast<Address>(&start);
    RELEASE_ASSERT(end < start);

    if (end <= scopeMarker && scopeMarker < start)
        return scopeMarker;

    // 256 is as good an approximation as any else.
    const size_t bytesToCopy = sizeof(Address) * 256;
    if (static_cast<size_t>(start - end) < bytesToCopy)
        return start;

    return end + bytesToCopy;
}
#endif

void ThreadState::enterSafePoint(StackState stackState, void* scopeMarker)
{
    checkThread();
#ifdef ADDRESS_SANITIZER
    if (stackState == HeapPointersOnStack)
        scopeMarker = adjustScopeMarkerForAdressSanitizer(scopeMarker);
#endif
    ASSERT(stackState == NoHeapPointersOnStack || scopeMarker);
    ASSERT(!isGCForbidden());
    runScheduledGC(stackState);
    ASSERT(!m_atSafePoint);
    m_atSafePoint = true;
    m_stackState = stackState;
    m_safePointScopeMarker = scopeMarker;
    s_safePointBarrier->enterSafePoint(this);
}

void ThreadState::leaveSafePoint(SafePointAwareMutexLocker* locker)
{
    checkThread();
    ASSERT(m_atSafePoint);
    s_safePointBarrier->leaveSafePoint(this, locker);
    m_atSafePoint = false;
    m_stackState = HeapPointersOnStack;
    clearSafePointScopeMarker();
    preSweep();
}

void ThreadState::copyStackUntilSafePointScope()
{
    if (!m_safePointScopeMarker || m_stackState == NoHeapPointersOnStack)
        return;

    Address* to = reinterpret_cast<Address*>(m_safePointScopeMarker);
    Address* from = reinterpret_cast<Address*>(m_endOfStack);
    RELEASE_ASSERT(from < to);
    RELEASE_ASSERT(to <= reinterpret_cast<Address*>(m_startOfStack));
    size_t slotCount = static_cast<size_t>(to - from);
    // Catch potential performance issues.
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
    // ASan/LSan use more space on the stack and we therefore
    // increase the allowed stack copying for those builds.
    ASSERT(slotCount < 2048);
#else
    ASSERT(slotCount < 1024);
#endif

    ASSERT(!m_safePointStackCopy.size());
    m_safePointStackCopy.resize(slotCount);
    for (size_t i = 0; i < slotCount; ++i) {
        m_safePointStackCopy[i] = from[i];
    }
}

void ThreadState::addInterruptor(Interruptor* interruptor)
{
    checkThread();
    SafePointScope scope(HeapPointersOnStack);
    {
        MutexLocker locker(threadAttachMutex());
        m_interruptors.append(interruptor);
    }
}

void ThreadState::removeInterruptor(Interruptor* interruptor)
{
    checkThread();
    SafePointScope scope(HeapPointersOnStack);
    {
        MutexLocker locker(threadAttachMutex());
        size_t index = m_interruptors.find(interruptor);
        RELEASE_ASSERT(index != kNotFound);
        m_interruptors.remove(index);
    }
}

void ThreadState::Interruptor::onInterrupted()
{
    ThreadState* state = ThreadState::current();
    ASSERT(state);
    ASSERT(!state->isAtSafePoint());
    state->safePoint(HeapPointersOnStack);
}

ThreadState::AttachedThreadStateSet& ThreadState::attachedThreads()
{
    DEFINE_STATIC_LOCAL(AttachedThreadStateSet, threads, ());
    return threads;
}

void ThreadState::lockThreadAttachMutex()
{
    threadAttachMutex().lock();
}

void ThreadState::unlockThreadAttachMutex()
{
    threadAttachMutex().unlock();
}

void ThreadState::unregisterPreFinalizerInternal(void* target)
{
    checkThread();
    if (sweepForbidden())
        return;
    auto it = m_preFinalizers.find(target);
    ASSERT(it != m_preFinalizers.end());
    m_preFinalizers.remove(it);
}

void ThreadState::invokePreFinalizers(Visitor& visitor)
{
    checkThread();
    Vector<void*> deadObjects;
    for (auto& entry : m_preFinalizers) {
        if (entry.value(entry.key, visitor))
            deadObjects.append(entry.key);
    }
    // FIXME: removeAll is inefficient.  It can shrink repeatedly.
    m_preFinalizers.removeAll(deadObjects);
}

void ThreadState::clearHeapAges()
{
    memset(m_heapAges, 0, sizeof(size_t) * NumberOfHeaps);
    memset(m_likelyToBePromptlyFreed.get(), 0, sizeof(int) * likelyToBePromptlyFreedArraySize);
    m_currentHeapAges = 0;
}

int ThreadState::heapIndexOfVectorHeapLeastRecentlyExpanded(int beginHeapIndex, int endHeapIndex)
{
    size_t minHeapAge = m_heapAges[beginHeapIndex];
    int heapIndexWithMinHeapAge = beginHeapIndex;
    for (int heapIndex = beginHeapIndex + 1; heapIndex <= endHeapIndex; heapIndex++) {
        if (m_heapAges[heapIndex] < minHeapAge) {
            minHeapAge = m_heapAges[heapIndex];
            heapIndexWithMinHeapAge = heapIndex;
        }
    }
    ASSERT(isVectorHeapIndex(heapIndexWithMinHeapAge));
    return heapIndexWithMinHeapAge;
}

BaseHeap* ThreadState::expandedVectorBackingHeap(size_t gcInfoIndex)
{
    size_t entryIndex = gcInfoIndex & likelyToBePromptlyFreedArrayMask;
    --m_likelyToBePromptlyFreed[entryIndex];
    int heapIndex = m_vectorBackingHeapIndex;
    m_heapAges[heapIndex] = ++m_currentHeapAges;
    m_vectorBackingHeapIndex = heapIndexOfVectorHeapLeastRecentlyExpanded(Vector1HeapIndex, Vector4HeapIndex);
    return m_heaps[heapIndex];
}

void ThreadState::allocationPointAdjusted(int heapIndex)
{
    m_heapAges[heapIndex] = ++m_currentHeapAges;
    if (m_vectorBackingHeapIndex == heapIndex)
        m_vectorBackingHeapIndex = heapIndexOfVectorHeapLeastRecentlyExpanded(Vector1HeapIndex, Vector4HeapIndex);
}

void ThreadState::promptlyFreed(size_t gcInfoIndex)
{
    size_t entryIndex = gcInfoIndex & likelyToBePromptlyFreedArrayMask;
    // See the comment in vectorBackingHeap() for why this is +3.
    m_likelyToBePromptlyFreed[entryIndex] += 3;
}

#if ENABLE(GC_PROFILING)
const GCInfo* ThreadState::findGCInfoFromAllThreads(Address address)
{
    bool needLockForIteration = !ThreadState::current()->isInGC();
    if (needLockForIteration)
        threadAttachMutex().lock();

    for (ThreadState* state : attachedThreads()) {
        if (const GCInfo* gcInfo = state->findGCInfo(address)) {
            if (needLockForIteration)
                threadAttachMutex().unlock();
            return gcInfo;
        }
    }
    if (needLockForIteration)
        threadAttachMutex().unlock();
    return nullptr;
}

void ThreadState::snapshotFreeListIfNecessary()
{
    bool enabled;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("blink_gc"), &enabled);
    if (!enabled)
        return;

    static const double recordIntervalSeconds = 0.010;
    double now = monotonicallyIncreasingTime();
    if (now > m_nextFreeListSnapshotTime) {
        snapshotFreeList();
        m_nextFreeListSnapshotTime = now + recordIntervalSeconds;
    }
}

void ThreadState::snapshotFreeList()
{
    RefPtr<TracedValue> json = TracedValue::create();

#define SNAPSHOT_FREE_LIST(HeapType)                           \
    {                                                          \
        json->beginDictionary();                               \
        json->setString("name", #HeapType);                    \
        m_heaps[HeapType##HeapIndex]->snapshotFreeList(*json); \
        json->endDictionary();                                 \
    }

    json->beginArray("heaps");
    SNAPSHOT_FREE_LIST(NormalPage);
    SNAPSHOT_FREE_LIST(Vector);
    SNAPSHOT_FREE_LIST(InlineVector);
    SNAPSHOT_FREE_LIST(HashTable);
    SNAPSHOT_FREE_LIST(LargeObject);
    FOR_EACH_TYPED_HEAP(SNAPSHOT_FREE_LIST);
    json->endArray();

#undef SNAPSHOT_FREE_LIST

    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "FreeList", this, json.release());
}

void ThreadState::collectAndReportMarkSweepStats() const
{
    if (!isMainThread())
        return;

    ClassAgeCountsMap markingClassAgeCounts;
    for (int i = 0; i < NumberOfHeaps; ++i)
        m_heaps[i]->countMarkedObjects(markingClassAgeCounts);
    reportMarkSweepStats("MarkingStats", markingClassAgeCounts);

    ClassAgeCountsMap sweepingClassAgeCounts;
    for (int i = 0; i < NumberOfHeaps; ++i)
        m_heaps[i]->countObjectsToSweep(sweepingClassAgeCounts);
    reportMarkSweepStats("SweepingStats", sweepingClassAgeCounts);
}

void ThreadState::reportMarkSweepStats(const char* statsName, const ClassAgeCountsMap& classAgeCounts) const
{
    RefPtr<TracedValue> json = TracedValue::create();
    for (ClassAgeCountsMap::const_iterator it = classAgeCounts.begin(), end = classAgeCounts.end(); it != end; ++it) {
        json->beginArray(it->key.ascii().data());
        for (size_t age = 0; age <= maxHeapObjectAge; ++age)
            json->pushInteger(it->value.ages[age]);
        json->endArray();
    }
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(TRACE_DISABLED_BY_DEFAULT("blink_gc"), statsName, this, json.release());
}
#endif

} // namespace blink
