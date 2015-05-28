/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioContext_h
#define AudioContext_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMTypedArray.h"
#include "core/events/EventListener.h"
#include "modules/EventTargetModules.h"
#include "modules/webaudio/AsyncAudioDecoder.h"
#include "modules/webaudio/AudioDestinationNode.h"
#include "platform/audio/AudioBus.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/MainThread.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class AnalyserNode;
class AudioBuffer;
class AudioBufferCallback;
class AudioBufferSourceNode;
class AudioListener;
class AudioSummingJunction;
class BiquadFilterNode;
class ChannelMergerNode;
class ChannelSplitterNode;
class ConvolverNode;
class DelayNode;
class Document;
class DynamicsCompressorNode;
class ExceptionState;
class GainNode;
class HTMLMediaElement;
class MediaElementAudioSourceNode;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;
class OscillatorNode;
class PannerNode;
class PeriodicWave;
class ScriptProcessorNode;
class ScriptPromiseResolver;
class ScriptState;
class SecurityOrigin;
class StereoPannerNode;
class WaveShaperNode;

// DeferredTaskHandler manages the major part of pre- and post- rendering tasks,
// and provides a lock mechanism against the audio rendering graph. A
// DeferredTaskHandler object is created when an AudioContext object is created.
// TODO(tkent): Move this to its own files.
class DeferredTaskHandler final : public ThreadSafeRefCounted<DeferredTaskHandler> {
public:
    static PassRefPtr<DeferredTaskHandler> create();
    ~DeferredTaskHandler();

    void handleDeferredTasks();

    // AudioContext can pull node(s) at the end of each render quantum even when
    // they are not connected to any downstream nodes.  These two methods are
    // called by the nodes who want to add/remove themselves into/from the
    // automatic pull lists.
    void addAutomaticPullNode(AudioHandler*);
    void removeAutomaticPullNode(AudioHandler*);
    // Called right before handlePostRenderTasks() to handle nodes which need to
    // be pulled even when they are not connected to anything.
    void processAutomaticPullNodes(size_t framesToProcess);

    // Keep track of AudioNode's that have their channel count mode changed. We
    // process the changes in the post rendering phase.
    void addChangedChannelCountMode(AudioHandler*);
    void removeChangedChannelCountMode(AudioHandler*);

    // Only accessed when the graph lock is held.
    void markSummingJunctionDirty(AudioSummingJunction*);
    // Only accessed when the graph lock is held. Must be called on the main thread.
    void removeMarkedSummingJunction(AudioSummingJunction*);

    void markAudioNodeOutputDirty(AudioNodeOutput*);
    void removeMarkedAudioNodeOutput(AudioNodeOutput*);
    void disposeOutputs(AudioHandler&);

    // In AudioNode::breakConnection() and deref(), a tryLock() is used for
    // calling actual processing, but if it fails keep track here.
    void addDeferredBreakConnection(AudioHandler&);
    void breakConnections();

    //
    // Thread Safety and Graph Locking:
    //
    void setAudioThread(ThreadIdentifier thread) { m_audioThread = thread; } // FIXME: check either not initialized or the same
    ThreadIdentifier audioThread() const { return m_audioThread; }
    bool isAudioThread() const;

    void lock();
    bool tryLock();
    void unlock();
#if ENABLE(ASSERT)
    // Returns true if this thread owns the context's lock.
    bool isGraphOwner();
#endif

    class AutoLocker {
        STACK_ALLOCATED();
    public:
        explicit AutoLocker(DeferredTaskHandler& handler)
            : m_handler(handler)
        {
            m_handler.lock();
        }
        explicit AutoLocker(AudioContext*);

        ~AutoLocker() { m_handler.unlock(); }

    private:
        DeferredTaskHandler& m_handler;
    };

private:
    DeferredTaskHandler();
    void updateAutomaticPullNodes();
    void updateChangedChannelCountMode();
    void handleDirtyAudioSummingJunctions();
    void handleDirtyAudioNodeOutputs();

    // For the sake of thread safety, we maintain a seperate Vector of automatic
    // pull nodes for rendering in m_renderingAutomaticPullNodes.  It will be
    // copied from m_automaticPullNodes by updateAutomaticPullNodes() at the
    // very start or end of the rendering quantum.
    // Oilpan: Since items are added to the vector/hash set by the audio thread
    // (not registered to Oilpan), we cannot use a HeapVector/HeapHashSet.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    HashSet<AudioHandler*> m_automaticPullNodes;
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    Vector<AudioHandler*> m_renderingAutomaticPullNodes;
    // m_automaticPullNodesNeedUpdating keeps track if m_automaticPullNodes is modified.
    bool m_automaticPullNodesNeedUpdating;

    // Collection of nodes where the channel count mode has changed. We want the
    // channel count mode to change in the pre- or post-rendering phase so as
    // not to disturb the running audio thread.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    HashSet<AudioHandler*> m_deferredCountModeChange;

    // These two HashSet must be accessed only when the graph lock is held.
    // Oilpan: These HashSet should be HeapHashSet<WeakMember<AudioNodeOutput>>
    // ideally. But it's difficult to lock them correctly during GC.
    // Oilpan: Since items are added to these hash sets by the audio thread (not
    // registered to Oilpan), we cannot use HeapHashSets.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    HashSet<AudioSummingJunction*> m_dirtySummingJunctions;
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    HashSet<AudioNodeOutput*> m_dirtyAudioNodeOutputs;

    // Only accessed in the audio thread.
    // Oilpan: Since items are added to these vectors by the audio thread (not
    // registered to Oilpan), we cannot use HeapVectors.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    Vector<AudioHandler*> m_deferredBreakConnectionList;

    // Graph locking.
    RecursiveMutex m_contextGraphMutex;
    volatile ThreadIdentifier m_audioThread;
};


// AudioContext is the cornerstone of the web audio API and all AudioNodes are created from it.
// For thread safety between the audio thread and the main thread, it has a rendering graph locking mechanism.

class AudioContext : public RefCountedGarbageCollectedEventTargetWithInlineData<AudioContext>, public ActiveDOMObject {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<AudioContext>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(AudioContext);
    DEFINE_WRAPPERTYPEINFO();
public:
    // The state of an audio context.  On creation, the state is Suspended. The state is Running if
    // audio is being processed (audio graph is being pulled for data). The state is Closed if the
    // audio context has been closed.  The valid transitions are from Suspended to either Running or
    // Closed; Running to Suspended or Closed. Once Closed, there are no valid transitions.
    enum AudioContextState {
        Suspended,
        Running,
        Closed
    };

    // Create an AudioContext for rendering to the audio hardware.
    static AudioContext* create(Document&, ExceptionState&);

    virtual ~AudioContext();

    DECLARE_VIRTUAL_TRACE();

    bool isInitialized() const { return m_isInitialized; }
    bool isOfflineContext() { return m_isOfflineContext; }

    // Document notification
    virtual void stop() override final;
    virtual bool hasPendingActivity() const override;

    AudioDestinationNode* destination() { return m_destinationNode.get(); }

    // currentSampleFrame() and currentTime() can be called from both the main
    // thread and the audio thread. Note that, however, they return the cached
    // value instead of actual current ones when they are accessed from the main
    // thread. See: crbug.com/431874
    size_t currentSampleFrame() const;
    double currentTime() const;

    float sampleRate() const { return m_destinationNode ? m_destinationNode->handler().sampleRate() : 0; }

    String state() const;

    AudioBuffer* createBuffer(unsigned numberOfChannels, size_t numberOfFrames, float sampleRate, ExceptionState&);

    // Asynchronous audio file data decoding.
    void decodeAudioData(DOMArrayBuffer*, AudioBufferCallback*, AudioBufferCallback*, ExceptionState&);

    AudioListener* listener() { return m_listener.get(); }

    // The AudioNode create methods are called on the main thread (from JavaScript).
    AudioBufferSourceNode* createBufferSource(ExceptionState&);
    MediaElementAudioSourceNode* createMediaElementSource(HTMLMediaElement*, ExceptionState&);
    MediaStreamAudioSourceNode* createMediaStreamSource(MediaStream*, ExceptionState&);
    MediaStreamAudioDestinationNode* createMediaStreamDestination(ExceptionState&);
    GainNode* createGain(ExceptionState&);
    BiquadFilterNode* createBiquadFilter(ExceptionState&);
    WaveShaperNode* createWaveShaper(ExceptionState&);
    DelayNode* createDelay(ExceptionState&);
    DelayNode* createDelay(double maxDelayTime, ExceptionState&);
    PannerNode* createPanner(ExceptionState&);
    ConvolverNode* createConvolver(ExceptionState&);
    DynamicsCompressorNode* createDynamicsCompressor(ExceptionState&);
    AnalyserNode* createAnalyser(ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(size_t bufferSize, ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, ExceptionState&);
    ScriptProcessorNode* createScriptProcessor(size_t bufferSize, size_t numberOfInputChannels, size_t numberOfOutputChannels, ExceptionState&);
    StereoPannerNode* createStereoPanner(ExceptionState&);
    ChannelSplitterNode* createChannelSplitter(ExceptionState&);
    ChannelSplitterNode* createChannelSplitter(size_t numberOfOutputs, ExceptionState&);
    ChannelMergerNode* createChannelMerger(ExceptionState&);
    ChannelMergerNode* createChannelMerger(size_t numberOfInputs, ExceptionState&);
    OscillatorNode* createOscillator(ExceptionState&);
    PeriodicWave* createPeriodicWave(DOMFloat32Array* real, DOMFloat32Array* imag, ExceptionState&);

    // Close
    ScriptPromise closeContext(ScriptState*);

    // Suspend/Resume
    ScriptPromise suspendContext(ScriptState*);
    ScriptPromise resumeContext(ScriptState*);

    // When a source node has started processing and needs to be protected,
    // this method tells the context to protect the node.
    void notifyNodeStartedProcessing(AudioNode*);
    // When a source node has no more processing to do (has finished playing),
    // this method tells the context to dereference the node.
    void notifyNodeFinishedProcessing(AudioNode*);

    // Called at the start of each render quantum.
    void handlePreRenderTasks();

    // Called at the end of each render quantum.
    void handlePostRenderTasks();

    // Called periodically at the end of each render quantum to dereference finished source nodes.
    void derefFinishedSourceNodes();

    void registerLiveNode(AudioNode&);
    void unregisterLiveNode(AudioNode&);

    // Keeps track of the number of connections made.
    void incrementConnectionCount()
    {
        ASSERT(isMainThread());
        m_connectionCount++;
    }

    unsigned connectionCount() const { return m_connectionCount; }

    DeferredTaskHandler& handler() const { return *m_deferredTaskHandler; }
    //
    // Thread Safety and Graph Locking:
    //
    // The following functions call corresponding functions of
    // DeferredTaskHandler.
    bool isAudioThread() const { return handler().isAudioThread(); }
    void lock() { handler().lock(); }
    bool tryLock() { return handler().tryLock(); }
    void unlock() { handler().unlock(); }
#if ENABLE(ASSERT)
    // Returns true if this thread owns the context's lock.
    bool isGraphOwner() { return handler().isGraphOwner(); }
#endif
    using AutoLocker = DeferredTaskHandler::AutoLocker;

    // Returns the maximum numuber of channels we can support.
    static unsigned maxNumberOfChannels() { return MaxNumberOfChannels;}

    // EventTarget
    virtual const AtomicString& interfaceName() const override final;
    virtual ExecutionContext* executionContext() const override final;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(complete);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    void startRendering();
    void fireCompletionEvent();
    void notifyStateChange();

    // The context itself keeps a reference to all source nodes.  The source nodes, then reference all nodes they're connected to.
    // In turn, these nodes reference all nodes they're connected to.  All nodes are ultimately connected to the AudioDestinationNode.
    // When the context dereferences a source node, it will be deactivated from the rendering graph along with all other nodes it is
    // uniquely connected to.  See the AudioNode::ref() and AudioNode::deref() methods for more details.
    void refNode(AudioNode*);

    // A context is considered closed if closeContext() has been called, even if the audio HW has
    // not yet been stopped.  It will be stopped eventually.
    bool isContextClosed() const { return m_closeResolver; }

    static unsigned s_hardwareContextCount;
    static unsigned s_contextId;

    // Get the security origin for this audio context.
    SecurityOrigin* securityOrigin() const;

protected:
    explicit AudioContext(Document*);
    AudioContext(Document*, unsigned numberOfChannels, size_t numberOfFrames, float sampleRate);

    RefPtrWillBeMember<ScriptPromiseResolver> m_offlineResolver;
private:
    void initialize();
    void uninitialize();

    // ExecutionContext calls stop twice.
    // We'd like to schedule only one stop action for them.
    bool m_isStopScheduled;
    bool m_isCleared;
    void clear();

    void throwExceptionForClosedState(ExceptionState&);

    // Set to true when the destination node has been initialized and is ready to process data.
    bool m_isInitialized;

    void derefNode(AudioNode*);

    // When the context goes away, there might still be some sources which haven't finished playing.
    // Make sure to dereference them here.
    void derefUnfinishedSourceNodes();

    Member<AudioDestinationNode> m_destinationNode;
    Member<AudioListener> m_listener;

    // Only accessed in the audio thread.
    // Oilpan: Since items are added to the vector by the audio thread (not registered to Oilpan),
    // we cannot use a HeapVector.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    Vector<AudioNode*> m_finishedNodes;

    // List of source nodes. This is either accessed when the graph lock is
    // held, or on the main thread when the audio thread has finished.
    // Oilpan: This Vector holds connection references. We must call
    // AudioNode::makeConnection when we add an AudioNode to this, and must call
    // AudioNode::breakConnection() when we remove an AudioNode from this.
    HeapVector<Member<AudioNode>> m_referencedNodes;

    // Stop rendering the audio graph.
    void stopRendering();

    // Handle Promises for resume() and suspend()
    void resolvePromisesForResume();
    void resolvePromisesForResumeOnMainThread();

    void resolvePromisesForSuspend();
    void resolvePromisesForSuspendOnMainThread();

    // Vector of promises created by resume(). It takes time to handle them, so we collect all of
    // the promises here until they can be resolved or rejected.
    WillBeHeapVector<RefPtrWillBeMember<ScriptPromiseResolver>> m_resumeResolvers;
    // Like m_resumeResolvers but for suspend().
    WillBeHeapVector<RefPtrWillBeMember<ScriptPromiseResolver>> m_suspendResolvers;
    void rejectPendingResolvers();

    // True if we're in the process of resolving promises for resume().  Resolving can take some
    // time and the audio context process loop is very fast, so we don't want to call resolve an
    // excessive number of times.
    bool m_isResolvingResumePromises;

    // Conceptually, this should be HeapHashSet<WeakMember<AudioNode>>. However
    // AudioNode also registers its pre-finalizer to the GC system, and having
    // another weak set would make the GC system slower. The AudioNode
    // pre-finalizer removes a member of this HashSet.
    GC_PLUGIN_IGNORE("http://crbug.com/404527")
    HashSet<AudioNode*> m_liveNodes;

    unsigned m_connectionCount;

    // Graph locking.
    bool m_didInitializeContextGraphMutex;
    RefPtr<DeferredTaskHandler> m_deferredTaskHandler;

    Member<AudioBuffer> m_renderTarget;

    bool m_isOfflineContext;

    // The state of the AudioContext.
    AudioContextState m_contextState;
    void setContextState(AudioContextState);

    AsyncAudioDecoder m_audioDecoder;

    // The Promise that is returned by close();
    RefPtrWillBeMember<ScriptPromiseResolver> m_closeResolver;

    // Follows the destination's currentSampleFrame, but might be slightly behind due to locking.
    size_t m_cachedSampleFrame;

    // Tries to handle AudioBufferSourceNodes that were started but became disconnected or was never
    // connected. Because these never get pulled anymore, they will stay around forever. So if we
    // can, try to stop them so they can be collected.
    void handleStoppableSourceNodes();

    // This is considering 32 is large enough for multiple channels audio.
    // It is somewhat arbitrary and could be increased if necessary.
    enum { MaxNumberOfChannels = 32 };

    unsigned m_contextId;
};

} // namespace blink

#endif // AudioContext_h
