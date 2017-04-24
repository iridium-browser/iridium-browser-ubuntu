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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/webaudio/DeferredTaskHandler.h"
#include "modules/webaudio/AudioNode.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "platform/CrossThreadFunctional.h"
#include "public/platform/Platform.h"

namespace blink {

void DeferredTaskHandler::lock() {
  // Don't allow regular lock in real-time audio thread.
  DCHECK(!isAudioThread());
  m_contextGraphMutex.lock();
}

bool DeferredTaskHandler::tryLock() {
  // Try to catch cases of using try lock on main thread
  // - it should use regular lock.
  DCHECK(isAudioThread());
  if (!isAudioThread()) {
    // In release build treat tryLock() as lock() (since above
    // DCHECK(isAudioThread) never fires) - this is the best we can do.
    lock();
    return true;
  }
  return m_contextGraphMutex.tryLock();
}

void DeferredTaskHandler::unlock() {
  m_contextGraphMutex.unlock();
}

void DeferredTaskHandler::offlineLock() {
  // CHECK is here to make sure to explicitly crash if this is called from
  // other than the offline render thread, which is considered as the audio
  // thread in OfflineAudioContext.
  CHECK(isAudioThread()) << "DeferredTaskHandler::offlineLock() must be called "
                            "within the offline audio thread.";

  m_contextGraphMutex.lock();
}

#if DCHECK_IS_ON()
bool DeferredTaskHandler::isGraphOwner() {
  return m_contextGraphMutex.locked();
}
#endif

void DeferredTaskHandler::addDeferredBreakConnection(AudioHandler& node) {
  DCHECK(isAudioThread());
  m_deferredBreakConnectionList.push_back(&node);
}

void DeferredTaskHandler::breakConnections() {
  DCHECK(isAudioThread());
  ASSERT(isGraphOwner());

  for (unsigned i = 0; i < m_deferredBreakConnectionList.size(); ++i)
    m_deferredBreakConnectionList[i]->breakConnectionWithLock();
  m_deferredBreakConnectionList.clear();
}

void DeferredTaskHandler::markSummingJunctionDirty(
    AudioSummingJunction* summingJunction) {
  ASSERT(isGraphOwner());
  m_dirtySummingJunctions.insert(summingJunction);
}

void DeferredTaskHandler::removeMarkedSummingJunction(
    AudioSummingJunction* summingJunction) {
  DCHECK(isMainThread());
  AutoLocker locker(*this);
  m_dirtySummingJunctions.erase(summingJunction);
}

void DeferredTaskHandler::markAudioNodeOutputDirty(AudioNodeOutput* output) {
  ASSERT(isGraphOwner());
  DCHECK(isMainThread());
  m_dirtyAudioNodeOutputs.insert(output);
}

void DeferredTaskHandler::removeMarkedAudioNodeOutput(AudioNodeOutput* output) {
  ASSERT(isGraphOwner());
  DCHECK(isMainThread());
  m_dirtyAudioNodeOutputs.erase(output);
}

void DeferredTaskHandler::handleDirtyAudioSummingJunctions() {
  ASSERT(isGraphOwner());

  for (AudioSummingJunction* junction : m_dirtySummingJunctions)
    junction->updateRenderingState();
  m_dirtySummingJunctions.clear();
}

void DeferredTaskHandler::handleDirtyAudioNodeOutputs() {
  ASSERT(isGraphOwner());

  HashSet<AudioNodeOutput*> dirtyOutputs;
  m_dirtyAudioNodeOutputs.swap(dirtyOutputs);

  // Note: the updating of rendering state may cause output nodes
  // further down the chain to be marked as dirty. These will not
  // be processed in this render quantum.
  for (AudioNodeOutput* output : dirtyOutputs)
    output->updateRenderingState();
}

void DeferredTaskHandler::addAutomaticPullNode(AudioHandler* node) {
  ASSERT(isGraphOwner());

  if (!m_automaticPullNodes.contains(node)) {
    m_automaticPullNodes.insert(node);
    m_automaticPullNodesNeedUpdating = true;
  }
}

void DeferredTaskHandler::removeAutomaticPullNode(AudioHandler* node) {
  ASSERT(isGraphOwner());

  if (m_automaticPullNodes.contains(node)) {
    m_automaticPullNodes.erase(node);
    m_automaticPullNodesNeedUpdating = true;
  }
}

void DeferredTaskHandler::updateAutomaticPullNodes() {
  ASSERT(isGraphOwner());

  if (m_automaticPullNodesNeedUpdating) {
    copyToVector(m_automaticPullNodes, m_renderingAutomaticPullNodes);
    m_automaticPullNodesNeedUpdating = false;
  }
}

void DeferredTaskHandler::processAutomaticPullNodes(size_t framesToProcess) {
  DCHECK(isAudioThread());

  for (unsigned i = 0; i < m_renderingAutomaticPullNodes.size(); ++i)
    m_renderingAutomaticPullNodes[i]->processIfNecessary(framesToProcess);
}

void DeferredTaskHandler::addChangedChannelCountMode(AudioHandler* node) {
  ASSERT(isGraphOwner());
  DCHECK(isMainThread());
  m_deferredCountModeChange.insert(node);
}

void DeferredTaskHandler::removeChangedChannelCountMode(AudioHandler* node) {
  ASSERT(isGraphOwner());

  m_deferredCountModeChange.erase(node);
}

void DeferredTaskHandler::addChangedChannelInterpretation(AudioHandler* node) {
  ASSERT(isGraphOwner());
  DCHECK(isMainThread());
  m_deferredChannelInterpretationChange.insert(node);
}

void DeferredTaskHandler::removeChangedChannelInterpretation(
    AudioHandler* node) {
  ASSERT(isGraphOwner());

  m_deferredChannelInterpretationChange.erase(node);
}

void DeferredTaskHandler::updateChangedChannelCountMode() {
  ASSERT(isGraphOwner());

  for (AudioHandler* node : m_deferredCountModeChange)
    node->updateChannelCountMode();
  m_deferredCountModeChange.clear();
}

void DeferredTaskHandler::updateChangedChannelInterpretation() {
  ASSERT(isGraphOwner());

  for (AudioHandler* node : m_deferredChannelInterpretationChange)
    node->updateChannelInterpretation();
  m_deferredChannelInterpretationChange.clear();
}

DeferredTaskHandler::DeferredTaskHandler()
    : m_automaticPullNodesNeedUpdating(false), m_audioThread(0) {}

PassRefPtr<DeferredTaskHandler> DeferredTaskHandler::create() {
  return adoptRef(new DeferredTaskHandler());
}

DeferredTaskHandler::~DeferredTaskHandler() {
  DCHECK(!m_automaticPullNodes.size());
  if (m_automaticPullNodesNeedUpdating)
    m_renderingAutomaticPullNodes.resize(m_automaticPullNodes.size());
  DCHECK(!m_renderingAutomaticPullNodes.size());
}

void DeferredTaskHandler::handleDeferredTasks() {
  updateChangedChannelCountMode();
  updateChangedChannelInterpretation();
  handleDirtyAudioSummingJunctions();
  handleDirtyAudioNodeOutputs();
  updateAutomaticPullNodes();
}

void DeferredTaskHandler::contextWillBeDestroyed() {
  for (auto& handler : m_renderingOrphanHandlers)
    handler->clearContext();
  for (auto& handler : m_deletableOrphanHandlers)
    handler->clearContext();
  clearHandlersToBeDeleted();
  // Some handlers might live because of their cross thread tasks.
}

DeferredTaskHandler::AutoLocker::AutoLocker(BaseAudioContext* context)
    : m_handler(context->deferredTaskHandler()) {
  m_handler.lock();
}

DeferredTaskHandler::OfflineGraphAutoLocker::OfflineGraphAutoLocker(
    OfflineAudioContext* context)
    : m_handler(context->deferredTaskHandler()) {
  m_handler.offlineLock();
}

void DeferredTaskHandler::addRenderingOrphanHandler(
    PassRefPtr<AudioHandler> handler) {
  DCHECK(handler);
  DCHECK(!m_renderingOrphanHandlers.contains(handler));
  m_renderingOrphanHandlers.push_back(handler);
}

void DeferredTaskHandler::requestToDeleteHandlersOnMainThread() {
  ASSERT(isGraphOwner());
  DCHECK(isAudioThread());
  if (m_renderingOrphanHandlers.isEmpty())
    return;
  m_deletableOrphanHandlers.appendVector(m_renderingOrphanHandlers);
  m_renderingOrphanHandlers.clear();
  Platform::current()->mainThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(&DeferredTaskHandler::deleteHandlersOnMainThread,
                      PassRefPtr<DeferredTaskHandler>(this)));
}

void DeferredTaskHandler::deleteHandlersOnMainThread() {
  DCHECK(isMainThread());
  AutoLocker locker(*this);
  m_deletableOrphanHandlers.clear();
}

void DeferredTaskHandler::clearHandlersToBeDeleted() {
  DCHECK(isMainThread());
  AutoLocker locker(*this);
  m_renderingOrphanHandlers.clear();
  m_deletableOrphanHandlers.clear();
}

void DeferredTaskHandler::setAudioThreadToCurrentThread() {
  DCHECK(!isMainThread());
  ThreadIdentifier thread = currentThread();
  releaseStore(&m_audioThread, thread);
}

}  // namespace blink
