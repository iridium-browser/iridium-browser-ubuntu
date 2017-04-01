/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorOverlay_h
#define InspectorOverlay_h

#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorOverlayHost.h"
#include "core/inspector/protocol/Forward.h"
#include "platform/Timer.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebInputEvent.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <v8-inspector.h>

namespace blink {

class Color;
class LocalFrame;
class Node;
class Page;
class PageOverlay;
class PlatformMouseEvent;
class PlatformTouchEvent;
class WebGestureEvent;
class WebLocalFrameImpl;

namespace protocol {
class Value;
}

class InspectorOverlay final
    : public GarbageCollectedFinalized<InspectorOverlay>,
      public InspectorDOMAgent::Client,
      public InspectorOverlayHost::Listener {
  USING_GARBAGE_COLLECTED_MIXIN(InspectorOverlay);

 public:
  explicit InspectorOverlay(WebLocalFrameImpl*);
  ~InspectorOverlay() override;
  DECLARE_TRACE();

  void init(v8_inspector::V8InspectorSession*, InspectorDOMAgent*);

  void clear();
  void suspend();
  void resume();
  bool handleInputEvent(const WebInputEvent&);
  void pageLayoutInvalidated(bool resized);
  void setShowViewportSizeOnResize(bool);
  void showReloadingBlanket();
  void hideReloadingBlanket();
  void setPausedInDebuggerMessage(const String&);

  // Does not yet include paint.
  void updateAllLifecyclePhases();

  PageOverlay* pageOverlay() { return m_pageOverlay.get(); };
  String evaluateInOverlayForTest(const String&);

 private:
  class InspectorOverlayChromeClient;
  class InspectorPageOverlayDelegate;

  // InspectorOverlayHost::Listener implementation.
  void overlayResumed() override;
  void overlaySteppedOver() override;

  // InspectorDOMAgent::Client implementation.
  void hideHighlight() override;
  void highlightNode(Node*,
                     const InspectorHighlightConfig&,
                     bool omitTooltip) override;
  void highlightQuad(std::unique_ptr<FloatQuad>,
                     const InspectorHighlightConfig&) override;
  void setInspectMode(InspectorDOMAgent::SearchMode,
                      std::unique_ptr<InspectorHighlightConfig>) override;

  void highlightNode(Node*,
                     Node* eventTarget,
                     const InspectorHighlightConfig&,
                     bool omitTooltip);
  bool isEmpty();
  void drawNodeHighlight();
  void drawQuadHighlight();
  void drawPausedInDebuggerMessage();
  void drawViewSize();

  float windowToViewportScale() const;

  Page* overlayPage();
  LocalFrame* overlayMainFrame();
  void reset(const IntSize& viewportSize, const IntPoint& documentScrollOffset);
  void evaluateInOverlay(const String& method, const String& argument);
  void evaluateInOverlay(const String& method,
                         std::unique_ptr<protocol::Value> argument);
  void onTimer(TimerBase*);
  void rebuildOverlayPage();
  void invalidate();
  void scheduleUpdate();
  void clearInternal();

  bool handleMousePress();
  bool handleGestureEvent(const WebGestureEvent&);
  bool handleTouchEvent(const PlatformTouchEvent&);
  bool handleMouseMove(const PlatformMouseEvent&);
  bool shouldSearchForNode();
  void inspect(Node*);

  Member<WebLocalFrameImpl> m_frameImpl;
  String m_pausedInDebuggerMessage;
  Member<Node> m_highlightNode;
  Member<Node> m_eventTargetNode;
  InspectorHighlightConfig m_nodeHighlightConfig;
  std::unique_ptr<FloatQuad> m_highlightQuad;
  Member<Page> m_overlayPage;
  Member<InspectorOverlayChromeClient> m_overlayChromeClient;
  Member<InspectorOverlayHost> m_overlayHost;
  InspectorHighlightConfig m_quadHighlightConfig;
  bool m_drawViewSize;
  bool m_resizeTimerActive;
  bool m_omitTooltip;
  Timer<InspectorOverlay> m_timer;
  bool m_suspended;
  bool m_showReloadingBlanket;
  bool m_inLayout;
  bool m_needsUpdate;
  v8_inspector::V8InspectorSession* m_v8Session;
  Member<InspectorDOMAgent> m_domAgent;
  std::unique_ptr<PageOverlay> m_pageOverlay;
  Member<Node> m_hoveredNodeForInspectMode;
  InspectorDOMAgent::SearchMode m_inspectMode;
  std::unique_ptr<InspectorHighlightConfig> m_inspectModeHighlightConfig;
};

}  // namespace blink

#endif  // InspectorOverlay_h
