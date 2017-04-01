/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebViewImpl_h
#define WebViewImpl_h

#include "core/page/ContextMenuProvider.h"
#include "core/page/EventWithHitTestResults.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDisplayMode.h"
#include "public/platform/WebFloatSize.h"
#include "public/platform/WebGestureCurveTarget.h"
#include "public/platform/WebGestureEvent.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/web/WebNavigationPolicy.h"
#include "public/web/WebPageImportanceSignals.h"
#include "public/web/WebView.h"
#include "web/ChromeClientImpl.h"
#include "web/ContextMenuClientImpl.h"
#include "web/EditorClientImpl.h"
#include "web/MediaKeysClientImpl.h"
#include "web/PageWidgetDelegate.h"
#include "web/ResizeViewportAnchor.h"
#include "web/SpellCheckerClientImpl.h"
#include "web/StorageClientImpl.h"
#include "web/WebExport.h"
#include "wtf/Compiler.h"
#include "wtf/HashSet.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class BrowserControls;
class CompositorAnimationHost;
class DevToolsEmulator;
class Frame;
class FullscreenController;
class InspectorOverlay;
class LinkHighlightImpl;
class PageOverlay;
class PageScaleConstraintsSet;
class PaintLayerCompositor;
class UserGestureToken;
class WebActiveGestureAnimation;
class WebDevToolsAgentImpl;
class WebElement;
class WebInputMethodControllerImpl;
class WebLayerTreeView;
class WebLocalFrame;
class WebLocalFrameImpl;
class CompositorMutatorImpl;
class WebPagePopupImpl;
class WebPlugin;
class WebRemoteFrame;
class WebSettingsImpl;
class WebViewScheduler;

class WEB_EXPORT WebViewImpl final
    : NON_EXPORTED_BASE(public WebView),
      public RefCounted<WebViewImpl>,
      NON_EXPORTED_BASE(public WebGestureCurveTarget),
      public PageWidgetEventHandler,
      public WebScheduler::InterventionReporter,
      public WebViewScheduler::WebViewSchedulerSettings {
 public:
  static WebViewImpl* create(WebViewClient*, WebPageVisibilityState);
  static HashSet<WebViewImpl*>& allInstances();

  class UserGestureNotifier {
   public:
    // If a UserGestureIndicator is created for a user gesture since the last
    // page load and the WebViewImpl's |m_userGestureObserved| is false, the
    // UserGestureNotifier will notify the client and set
    // |m_userGestureObserved| to true.
    UserGestureNotifier(WebViewImpl*);
    ~UserGestureNotifier();

   private:
    Persistent<WebLocalFrameImpl> m_frame;
    bool* const m_userGestureObserved;
  };

  // WebWidget methods:
  void close() override;
  WebSize size() override;
  void resize(const WebSize&) override;
  void resizeVisualViewport(const WebSize&) override;
  void didEnterFullscreen() override;
  void didExitFullscreen() override;

  void beginFrame(double lastFrameTimeMonotonic) override;

  void updateAllLifecyclePhases() override;
  void paint(WebCanvas*, const WebRect&) override;
#if OS(ANDROID)
  void paintIgnoringCompositing(WebCanvas*, const WebRect&) override;
#endif
  void layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override;
  void compositeAndReadbackAsync(
      WebCompositeAndReadbackAsyncCallback*) override;
  void themeChanged() override;
  WebInputEventResult handleInputEvent(const WebInputEvent&) override;
  void setCursorVisibilityState(bool isVisible) override;
  bool hasTouchEventHandlersAt(const WebPoint&) override;

  void applyViewportDeltas(const WebFloatSize& visualViewportDelta,
                           const WebFloatSize& layoutViewportDelta,
                           const WebFloatSize& elasticOverscrollDelta,
                           float pageScaleDelta,
                           float browserControlsShownRatioDelta) override;
  void mouseCaptureLost() override;
  void setFocus(bool enable) override;
  WebRange compositionRange() override;
  WebColor backgroundColor() const override;
  WebPagePopup* pagePopup() const override;
  bool selectionBounds(WebRect& anchor, WebRect& focus) const override;
  bool selectionTextDirection(WebTextDirection& start,
                              WebTextDirection& end) const override;
  bool isSelectionAnchorFirst() const override;
  WebRange caretOrSelectionRange() override;
  void setTextDirection(WebTextDirection) override;
  bool isAcceleratedCompositingActive() const override;
  void willCloseLayerTreeView() override;
  void didAcquirePointerLock() override;
  void didNotAcquirePointerLock() override;
  void didLosePointerLock() override;

  // WebView methods:
  virtual bool isWebView() const { return true; }
  void setMainFrame(WebFrame*) override;
  void setCredentialManagerClient(WebCredentialManagerClient*) override;
  void setPrerendererClient(WebPrerendererClient*) override;
  void setSpellCheckClient(WebSpellCheckClient*) override;
  WebSettings* settings() override;
  WebString pageEncoding() const override;
  bool tabsToLinks() const override;
  void setTabsToLinks(bool value) override;
  bool tabKeyCyclesThroughElements() const override;
  void setTabKeyCyclesThroughElements(bool value) override;
  bool isActive() const override;
  void setIsActive(bool value) override;
  void setDomainRelaxationForbidden(bool, const WebString& scheme) override;
  void setWindowFeatures(const WebWindowFeatures&) override;
  void setOpenedByDOM() override;
  void resizeWithBrowserControls(const WebSize&,
                                 float browserControlsHeight,
                                 bool browserControlsShrinkLayout) override;
  WebFrame* mainFrame() override;
  WebFrame* findFrameByName(const WebString& name,
                            WebFrame* relativeToFrame) override;
  WebLocalFrame* focusedFrame() override;
  void setFocusedFrame(WebFrame*) override;
  void focusDocumentView(WebFrame*) override;
  void setInitialFocus(bool reverse) override;
  void clearFocusedElement() override;
  bool scrollFocusedEditableElementIntoRect(const WebRect&) override;
  void smoothScroll(int targetX, int targetY, long durationMs) override;
  void zoomToFindInPageRect(const WebRect&);
  void advanceFocus(bool reverse) override;
  void advanceFocusAcrossFrames(WebFocusType,
                                WebRemoteFrame* from,
                                WebLocalFrame* to) override;
  double zoomLevel() override;
  double setZoomLevel(double) override;
  void zoomLimitsChanged(double minimumZoomLevel,
                         double maximumZoomLevel) override;
  float textZoomFactor() override;
  float setTextZoomFactor(float) override;
  bool zoomToMultipleTargetsRect(const WebRect&) override;
  float pageScaleFactor() const override;
  void setDefaultPageScaleLimits(float minScale, float maxScale) override;
  void setInitialPageScaleOverride(float) override;
  void setMaximumLegibleScale(float) override;
  void setPageScaleFactor(float) override;
  void setVisualViewportOffset(const WebFloatPoint&) override;
  WebFloatPoint visualViewportOffset() const override;
  WebFloatSize visualViewportSize() const override;
  void resetScrollAndScaleState() override;
  void setIgnoreViewportTagScaleLimits(bool) override;
  WebSize contentsPreferredMinimumSize() override;
  void setDisplayMode(WebDisplayMode) override;

  void setDeviceScaleFactor(float) override;
  void setZoomFactorForDeviceScaleFactor(float) override;

  void setDeviceColorProfile(const WebVector<char>&) override;

  void enableAutoResizeMode(const WebSize& minSize,
                            const WebSize& maxSize) override;
  void disableAutoResizeMode() override;
  void performMediaPlayerAction(const WebMediaPlayerAction& action,
                                const WebPoint& location) override;
  void performPluginAction(const WebPluginAction&, const WebPoint&) override;
  void audioStateChanged(bool isAudioPlaying) override;
  WebHitTestResult hitTestResultAt(const WebPoint&) override;
  WebHitTestResult hitTestResultForTap(const WebPoint&,
                                       const WebSize&) override;
  void spellingMarkers(WebVector<uint32_t>* markers) override;
  void removeSpellingMarkersUnderWords(
      const WebVector<WebString>& words) override;
  unsigned long createUniqueIdentifierForRequest() override;
  void enableDeviceEmulation(const WebDeviceEmulationParams&) override;
  void disableDeviceEmulation() override;
  WebAXObject accessibilityObject() override;
  void setSelectionColors(unsigned activeBackgroundColor,
                          unsigned activeForegroundColor,
                          unsigned inactiveBackgroundColor,
                          unsigned inactiveForegroundColor) override;
  void performCustomContextMenuAction(unsigned action) override;
  void showContextMenu() override;
  void didCloseContextMenu() override;
  void extractSmartClipData(WebRect, WebString&, WebString&, WebRect&) override;
  void hidePopups() override;
  void setPageOverlayColor(WebColor) override;
  WebPageImportanceSignals* pageImportanceSignals() override;
  void transferActiveWheelFlingAnimation(
      const WebActiveWheelFlingParameters&) override;
  bool endActiveFlingAnimation() override;
  bool isFlinging() const override { return !!m_gestureAnimation.get(); }
  void setShowPaintRects(bool) override;
  void setShowDebugBorders(bool);
  void setShowFPSCounter(bool) override;
  void setShowScrollBottleneckRects(bool) override;
  void acceptLanguagesChanged() override;

  // WebScheduler::InterventionReporter implementation:
  void ReportIntervention(const WebString& message) override;

  // WebViewScheduler::WebViewSchedulerSettings implementation:
  float expensiveBackgroundThrottlingCPUBudget() override;
  float expensiveBackgroundThrottlingInitialBudget() override;
  float expensiveBackgroundThrottlingMaxBudget() override;
  float expensiveBackgroundThrottlingMaxDelay() override;

  void didUpdateFullscreenSize();

  float defaultMinimumPageScaleFactor() const;
  float defaultMaximumPageScaleFactor() const;
  float minimumPageScaleFactor() const;
  float maximumPageScaleFactor() const;
  float clampPageScaleFactorToLimits(float) const;
  void resetScaleStateImmediately();

  HitTestResult coreHitTestResultAt(const WebPoint&);
  void invalidateRect(const IntRect&);

  void setBaseBackgroundColor(WebColor);
  void setBackgroundColorOverride(WebColor);
  void setZoomFactorOverride(float);
  void setCompositorDeviceScaleFactorOverride(float);
  void setDeviceEmulationTransform(const TransformationMatrix&);
  TransformationMatrix getDeviceEmulationTransformForTesting() const;

  Color baseBackgroundColor() const { return m_baseBackgroundColor; }

  WebColor backgroundColorOverride() const { return m_backgroundColorOverride; }

  Frame* focusedCoreFrame() const;

  // Returns the currently focused Element or null if no element has focus.
  Element* focusedElement() const;

  static WebViewImpl* fromPage(Page*);

  WebViewClient* client() { return m_client; }

  WebSpellCheckClient* spellCheckClient() { return m_spellCheckClient; }

  // Returns the page object associated with this view. This may be null when
  // the page is shutting down, but will be valid at all other times.
  Page* page() const { return m_page.get(); }

  WebDevToolsAgentImpl* mainFrameDevToolsAgentImpl();

  DevToolsEmulator* devToolsEmulator() const {
    return m_devToolsEmulator.get();
  }

  // Returns the main frame associated with this view. This may be null when
  // the page is shutting down, but will be valid at all other times.
  WebLocalFrameImpl* mainFrameImpl() const;

  // Event related methods:
  void mouseContextMenu(const WebMouseEvent&);
  void mouseDoubleClick(const WebMouseEvent&);

  bool detectContentOnTouch(
      const GestureEventWithHitTestResults& targetedEvent);
  bool startPageScaleAnimation(const IntPoint& targetPosition,
                               bool useAnchor,
                               float newScale,
                               double durationInSeconds);

  void hasTouchEventHandlers(bool);

  // WebGestureCurveTarget implementation for fling.
  bool scrollBy(const WebFloatSize& delta,
                const WebFloatSize& velocity) override;

  // Handles context menu events orignated via the the keyboard. These
  // include the VK_APPS virtual key and the Shift+F10 combine. Code is
  // based on the Webkit function bool WebView::handleContextMenuEvent(WPARAM
  // wParam, LPARAM lParam) in webkit\webkit\win\WebView.cpp. The only
  // significant change in this function is the code to convert from a
  // Keyboard event to the Right Mouse button down event.
  WebInputEventResult sendContextMenuEvent(const WebKeyboardEvent&);

  void showContextMenuAtPoint(float x, float y, ContextMenuProvider*);

  void showContextMenuForElement(WebElement);

  // Notifies the WebView that a load has been committed. isNewNavigation
  // will be true if a new session history item should be created for that
  // load. isNavigationWithinPage will be true if the navigation does
  // not take the user away from the current page.
  void didCommitLoad(bool isNewNavigation, bool isNavigationWithinPage);

  void postLayoutResize(WebLocalFrameImpl* webframe);

  // Indicates two things:
  //   1) This view may have a new layout now.
  //   2) Calling updateAllLifecyclePhases() is a no-op.
  // After calling WebWidget::updateAllLifecyclePhases(), expect to get this
  // notification unless the view did not need a layout.
  void layoutUpdated(WebLocalFrameImpl*);

  void didChangeContentsSize();
  void pageScaleFactorChanged();
  void mainFrameScrollOffsetChanged();

  // Returns true if popup menus should be rendered by the browser, false if
  // they should be rendered by WebKit (which is the default).
  static bool useExternalPopupMenus();

  bool shouldAutoResize() const { return m_shouldAutoResize; }

  IntSize minAutoSize() const { return m_minAutoSize; }

  IntSize maxAutoSize() const { return m_maxAutoSize; }

  void updateMainFrameLayoutSize();
  void updatePageDefinedViewportConstraints(const ViewportDescription&);

  PagePopup* openPagePopup(PagePopupClient*);
  void closePagePopup(PagePopup*);
  void cleanupPagePopup();
  LocalDOMWindow* pagePopupWindow() const;

  // Returns the input event we're currently processing. This is used in some
  // cases where the WebCore DOM event doesn't have the information we need.
  static const WebInputEvent* currentInputEvent() {
    return m_currentInputEvent;
  }

  GraphicsLayer* rootGraphicsLayer();
  void registerViewportLayersWithCompositor();
  PaintLayerCompositor* compositor() const;
  CompositorAnimationTimeline* linkHighlightsTimeline() const {
    return m_linkHighlightsTimeline.get();
  }

  WebViewScheduler* scheduler() const override;
  void setVisibilityState(WebPageVisibilityState, bool) override;

  bool hasOpenedPopup() const { return m_pagePopup.get(); }

  // Called by a full frame plugin inside this view to inform it that its
  // zoom level has been updated.  The plugin should only call this function
  // if the zoom change was triggered by the browser, it's only needed in case
  // a plugin can update its own zoom, say because of its own UI.
  void fullFramePluginZoomLevelChanged(double zoomLevel);

  void computeScaleAndScrollForBlockRect(const WebPoint& hitPoint,
                                         const WebRect& blockRect,
                                         float padding,
                                         float defaultScaleWhenAlreadyLegible,
                                         float& scale,
                                         WebPoint& scroll);
  Node* bestTapNode(const GestureEventWithHitTestResults& targetedTapEvent);
  void enableTapHighlightAtPoint(
      const GestureEventWithHitTestResults& targetedTapEvent);
  void enableTapHighlights(HeapVector<Member<Node>>&);
  void computeScaleAndScrollForFocusedNode(Node* focusedNode,
                                           bool zoomInToLegibleScale,
                                           float& scale,
                                           IntPoint& scroll,
                                           bool& needAnimation);

  void animateDoubleTapZoom(const IntPoint&);

  void enableFakePageScaleAnimationForTesting(bool);
  bool fakeDoubleTapAnimationPendingForTesting() const {
    return m_doubleTapZoomPending;
  }
  IntPoint fakePageScaleAnimationTargetPositionForTesting() const {
    return m_fakePageScaleAnimationTargetPosition;
  }
  float fakePageScaleAnimationPageScaleForTesting() const {
    return m_fakePageScaleAnimationPageScaleFactor;
  }
  bool fakePageScaleAnimationUseAnchorForTesting() const {
    return m_fakePageScaleAnimationUseAnchor;
  }

  void enterFullscreen(LocalFrame&);
  void exitFullscreen(LocalFrame&);
  void fullscreenElementChanged(Element*, Element*);

  // Exposed for the purpose of overriding device metrics.
  void sendResizeEventAndRepaint();

  // Exposed for testing purposes.
  bool hasHorizontalScrollbar();
  bool hasVerticalScrollbar();

  // Exposed for tests.
  unsigned numLinkHighlights() { return m_linkHighlights.size(); }
  LinkHighlightImpl* getLinkHighlight(int i) {
    return m_linkHighlights[i].get();
  }

  WebSettingsImpl* settingsImpl();

  // Returns the bounding box of the block type node touched by the WebPoint.
  WebRect computeBlockBound(const WebPoint&, bool ignoreClipping);

  WebLayerTreeView* layerTreeView() const { return m_layerTreeView; }
  CompositorAnimationHost* animationHost() const {
    return m_animationHost.get();
  }

  bool matchesHeuristicsForGpuRasterizationForTesting() const {
    return m_matchesHeuristicsForGpuRasterization;
  }

  void updateBrowserControlsState(WebBrowserControlsState constraint,
                                  WebBrowserControlsState current,
                                  bool animate) override;

  BrowserControls& browserControls();
  // Called anytime browser controls layout height or content offset have
  // changed.
  void didUpdateBrowserControls();

  void forceNextWebGLContextCreationToFail() override;
  void forceNextDrawingBufferCreationToFail() override;

  CompositorProxyClient* createCompositorProxyClient();
  IntSize mainFrameSize();
  WebDisplayMode displayMode() const { return m_displayMode; }

  PageScaleConstraintsSet& pageScaleConstraintsSet() const;

  FloatSize elasticOverscroll() const { return m_elasticOverscroll; }

  bool isTransparent() const;
  void setIsTransparent(bool value);

  double lastFrameTimeMonotonic() const { return m_lastFrameTimeMonotonic; }

  ChromeClientImpl& chromeClient() const { return *m_chromeClientImpl.get(); }

  // Returns the currently active WebInputMethodController which is the one
  // corresponding to the focused frame. It will return nullptr if there is no
  // focused frame, or if the there is one but it belongs to a different local
  // root.
  WebInputMethodControllerImpl* getActiveWebInputMethodController() const;

 private:
  InspectorOverlay* inspectorOverlay();

  void setPageScaleFactorAndLocation(float, const FloatPoint&);
  void propagateZoomFactorToLocalFrameRoots(Frame*, float);

  void scrollAndRescaleViewports(float scaleFactor,
                                 const IntPoint& mainFrameOrigin,
                                 const FloatPoint& visualViewportOrigin);

  float maximumLegiblePageScale() const;
  void refreshPageScaleFactorAfterLayout();
  IntSize contentsSize() const;

  void performResize();
  void resizeViewWhileAnchored(float browserControlsHeight,
                               bool browserControlsShrinkLayout);

  // Overrides the compositor visibility. See the description of
  // m_overrideCompositorVisibility for more details.
  void setCompositorVisibility(bool);

  // TODO(lfg): Remove once WebViewFrameWidget is deleted.
  void scheduleAnimationForWidget();
  bool getCompositionCharacterBounds(WebVector<WebRect>&) override;
  void applyReplacementRange(const WebRange&) override;

  friend class WebView;  // So WebView::Create can call our constructor
  friend class WebViewFrameWidget;
  friend class WTF::RefCounted<WebViewImpl>;
  friend void setCurrentInputEventForTest(const WebInputEvent*);

  explicit WebViewImpl(WebViewClient*, WebPageVisibilityState);
  ~WebViewImpl() override;

  void hideSelectPopup();

  HitTestResult hitTestResultForRootFramePos(const IntPoint&);
  HitTestResult hitTestResultForViewportPos(const IntPoint&);

  void configureAutoResizeMode();

  void initializeLayerTreeView();

  void setIsAcceleratedCompositingActive(bool);
  void doComposite();
  void reallocateRenderer();
  void updateLayerTreeViewport();
  void updateLayerTreeBackgroundColor();
  void updateDeviceEmulationTransform();
  void updateLayerTreeDeviceScaleFactor();

  // Helper function: Widens the width of |source| by the specified margins
  // while keeping it smaller than page width.
  WebRect widenRectWithinPageBounds(const WebRect& source,
                                    int targetMargin,
                                    int minimumMargin);

  void pointerLockMouseEvent(const WebInputEvent&);

  // PageWidgetEventHandler functions
  void handleMouseLeave(LocalFrame&, const WebMouseEvent&) override;
  void handleMouseDown(LocalFrame&, const WebMouseEvent&) override;
  void handleMouseUp(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult handleMouseWheel(LocalFrame&,
                                       const WebMouseWheelEvent&) override;
  WebInputEventResult handleGestureEvent(const WebGestureEvent&) override;
  WebInputEventResult handleKeyEvent(const WebKeyboardEvent&) override;
  WebInputEventResult handleCharEvent(const WebKeyboardEvent&) override;

  WebInputEventResult handleSyntheticWheelFromTouchpadPinchEvent(
      const WebGestureEvent&);

  WebPlugin* focusedPluginIfInputMethodSupported(LocalFrame*);

  WebGestureEvent createGestureScrollEventFromFling(WebInputEvent::Type,
                                                    WebGestureDevice) const;

  void enablePopupMouseWheelEventListener();
  void disablePopupMouseWheelEventListener();

  void cancelPagePopup();
  void updatePageOverlays();

  float deviceScaleFactor() const;

  void setRootGraphicsLayer(GraphicsLayer*);
  void setRootLayer(WebLayer*);
  void attachCompositorAnimationTimeline(CompositorAnimationTimeline*);
  void detachCompositorAnimationTimeline(CompositorAnimationTimeline*);

  LocalFrame* focusedLocalFrameInWidget() const;
  LocalFrame* focusedLocalFrameAvailableForIme() const;

  WebViewClient* m_client;  // Can be 0 (e.g. unittests, shared workers, etc.)
  WebSpellCheckClient* m_spellCheckClient;

  Persistent<ChromeClientImpl> m_chromeClientImpl;
  ContextMenuClientImpl m_contextMenuClientImpl;
  EditorClientImpl m_editorClientImpl;
  SpellCheckerClientImpl m_spellCheckerClientImpl;
  StorageClientImpl m_storageClientImpl;

  WebSize m_size;
  // If true, automatically resize the layout view around its content.
  bool m_shouldAutoResize;
  // The lower bound on the size when auto-resizing.
  IntSize m_minAutoSize;
  // The upper bound on the size when auto-resizing.
  IntSize m_maxAutoSize;

  Persistent<Page> m_page;

  // An object that can be used to manipulate m_page->settings() without linking
  // against WebCore. This is lazily allocated the first time GetWebSettings()
  // is called.
  std::unique_ptr<WebSettingsImpl> m_webSettings;

  // Keeps track of the current zoom level. 0 means no zoom, positive numbers
  // mean zoom in, negative numbers mean zoom out.
  double m_zoomLevel;

  double m_minimumZoomLevel;

  double m_maximumZoomLevel;

  // Additional zoom factor used to scale the content by device scale factor.
  double m_zoomFactorForDeviceScaleFactor;

  // This value, when multiplied by the font scale factor, gives the maximum
  // page scale that can result from automatic zooms.
  float m_maximumLegibleScale;

  // The scale moved to by the latest double tap zoom, if any.
  float m_doubleTapZoomPageScaleFactor;
  // Have we sent a double-tap zoom and not yet heard back the scale?
  bool m_doubleTapZoomPending;

  // Used for testing purposes.
  bool m_enableFakePageScaleAnimationForTesting;
  IntPoint m_fakePageScaleAnimationTargetPosition;
  float m_fakePageScaleAnimationPageScaleFactor;
  bool m_fakePageScaleAnimationUseAnchor;

  float m_compositorDeviceScaleFactorOverride;
  TransformationMatrix m_deviceEmulationTransform;

  // Webkit expects keyPress events to be suppressed if the associated keyDown
  // event was handled. Safari implements this behavior by peeking out the
  // associated WM_CHAR event if the keydown was handled. We emulate
  // this behavior by setting this flag if the keyDown was handled.
  bool m_suppressNextKeypressEvent;

  // TODO(ekaramad): Can we remove this and make sure IME events are not called
  // when there is no page focus?
  // Represents whether or not this object should process incoming IME events.
  bool m_imeAcceptEvents;

  // The popup associated with an input/select element.
  RefPtr<WebPagePopupImpl> m_pagePopup;

  // This stores the last hidden page popup. If a GestureTap attempts to open
  // the popup that is closed by its previous GestureTapDown, the popup remains
  // closed.
  RefPtr<WebPagePopupImpl> m_lastHiddenPagePopup;

  Persistent<DevToolsEmulator> m_devToolsEmulator;
  std::unique_ptr<PageOverlay> m_pageColorOverlay;

  // Whether the webview is rendering transparently.
  bool m_isTransparent;

  // Whether the user can press tab to focus links.
  bool m_tabsToLinks;

  // If set, the (plugin) node which has mouse capture.
  Persistent<Node> m_mouseCaptureNode;
  RefPtr<UserGestureToken> m_mouseCaptureGestureToken;

  RefPtr<UserGestureToken> m_pointerLockGestureToken;

  WebLayerTreeView* m_layerTreeView;
  std::unique_ptr<CompositorAnimationHost> m_animationHost;

  WebLayer* m_rootLayer;
  GraphicsLayer* m_rootGraphicsLayer;
  GraphicsLayer* m_visualViewportContainerLayer;
  bool m_matchesHeuristicsForGpuRasterization;
  static const WebInputEvent* m_currentInputEvent;

  MediaKeysClientImpl m_mediaKeysClientImpl;
  std::unique_ptr<WebActiveGestureAnimation> m_gestureAnimation;
  WebPoint m_positionOnFlingStart;
  WebPoint m_globalPositionOnFlingStart;
  int m_flingModifier;
  WebGestureDevice m_flingSourceDevice;
  Vector<std::unique_ptr<LinkHighlightImpl>> m_linkHighlights;
  std::unique_ptr<CompositorAnimationTimeline> m_linkHighlightsTimeline;
  std::unique_ptr<FullscreenController> m_fullscreenController;

  WebColor m_baseBackgroundColor;
  WebColor m_backgroundColorOverride;
  float m_zoomFactorOverride;

  bool m_userGestureObserved;
  bool m_shouldDispatchFirstVisuallyNonEmptyLayout;
  bool m_shouldDispatchFirstLayoutAfterFinishedParsing;
  bool m_shouldDispatchFirstLayoutAfterFinishedLoading;
  WebDisplayMode m_displayMode;

  FloatSize m_elasticOverscroll;

  // This is owned by the LayerTreeHostImpl, and should only be used on the
  // compositor thread. The LayerTreeHostImpl is indirectly owned by this
  // class so this pointer should be valid until this class is destructed.
  CrossThreadPersistent<CompositorMutatorImpl> m_mutator;

  Persistent<EventListener> m_popupMouseWheelEventListener;

  WebPageImportanceSignals m_pageImportanceSignals;

  const std::unique_ptr<WebViewScheduler> m_scheduler;

  double m_lastFrameTimeMonotonic;

  // TODO(lfg): This is used in order to disable compositor visibility while
  // the page is still visible. This is needed until the WebView and WebWidget
  // split is complete, since in out-of-process iframes the page can be
  // visible, but the WebView should not be used as a widget.
  bool m_overrideCompositorVisibility;

  Persistent<ResizeViewportAnchor> m_resizeViewportAnchor;
};

// We have no ways to check if the specified WebView is an instance of
// WebViewImpl because WebViewImpl is the only implementation of WebView.
DEFINE_TYPE_CASTS(WebViewImpl, WebView, webView, true, true);

}  // namespace blink

#endif
