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

#ifndef FrameHost_h
#define FrameHost_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

class BrowserControls;
class ConsoleMessageStorage;
class EventHandlerRegistry;
class OverscrollController;
class Page;
struct PageScaleConstraints;
class PageScaleConstraintsSet;
class TopDocumentRootScrollerController;
class VisualViewport;

// FrameHost is the set of global data shared between multiple frames
// and is provided by the embedder to each frame when created.
// FrameHost currently corresponds to the Page object in core/page
// however the concept of a Page is moving up out of Blink.
// In an out-of-process iframe world, a single Page may have
// multiple frames in different process, thus Page becomes a
// browser-level concept and Blink core/ only knows about its LocalFrame (and
// FrameHost).  Separating Page from the rest of core/ through this indirection
// allows us to slowly refactor Page without breaking the rest of core.
// TODO(sashab): Merge FrameHost back into Page. crbug.com/688614
class CORE_EXPORT FrameHost final
    : public GarbageCollectedFinalized<FrameHost> {
  WTF_MAKE_NONCOPYABLE(FrameHost);

 public:
  static FrameHost* create(Page&);
  ~FrameHost();

  Page& page();
  const Page& page() const;

  BrowserControls& browserControls();
  const BrowserControls& browserControls() const;

  OverscrollController& overscrollController();
  const OverscrollController& overscrollController() const;

  VisualViewport& visualViewport();
  const VisualViewport& visualViewport() const;

  PageScaleConstraintsSet& pageScaleConstraintsSet();
  const PageScaleConstraintsSet& pageScaleConstraintsSet() const;

  EventHandlerRegistry& eventHandlerRegistry();
  const EventHandlerRegistry& eventHandlerRegistry() const;

  ConsoleMessageStorage& consoleMessageStorage();
  const ConsoleMessageStorage& consoleMessageStorage() const;

  TopDocumentRootScrollerController& globalRootScrollerController() const;

  DECLARE_TRACE();

  // Don't allow more than a certain number of frames in a page.
  // This seems like a reasonable upper bound, and otherwise mutually
  // recursive frameset pages can quickly bring the program to its knees
  // with exponential growth in the number of frames.
  static const int maxNumberOfFrames = 1000;
  void incrementSubframeCount() { ++m_subframeCount; }
  void decrementSubframeCount() {
    ASSERT(m_subframeCount);
    --m_subframeCount;
  }
  int subframeCount() const;

  void setDefaultPageScaleLimits(float minScale, float maxScale);
  void setUserAgentPageScaleConstraints(
      const PageScaleConstraints& newConstraints);

 private:
  explicit FrameHost(Page&);

  const Member<Page> m_page;
  const Member<BrowserControls> m_browserControls;
  const Member<VisualViewport> m_visualViewport;
  const Member<OverscrollController> m_overscrollController;
  const Member<EventHandlerRegistry> m_eventHandlerRegistry;
  const Member<ConsoleMessageStorage> m_consoleMessageStorage;
  const Member<TopDocumentRootScrollerController>
      m_globalRootScrollerController;

  AtomicString m_overrideEncoding;
  int m_subframeCount;
};

}  // namespace blink

#endif  // FrameHost_h
