/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#include "web/WebViewImpl.h"

#include <memory>
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/clipboard/DataObject.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/LayoutTreeBuilderTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/serializers/HTMLInterchange.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/UIEventWithKeyState.h"
#include "core/events/WheelEvent.h"
#include "core/frame/BrowserControls.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/input/EventHandler.h"
#include "core/input/TouchActionUtil.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/page/ContextMenuController.h"
#include "core/page/ContextMenuProvider.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PagePopupClient.h"
#include "core/page/PointerLockController.h"
#include "core/page/ScopedPageSuspender.h"
#include "core/page/TouchDisambiguation.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/PaintLayer.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/credentialmanager/CredentialManagerClient.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "modules/storage/StorageNamespaceController.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "platform/ContextMenu.h"
#include "platform/ContextMenuItem.h"
#include "platform/Cursor.h"
#include "platform/Histogram.h"
#include "platform/KeyboardCodes.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/exported/WebActiveGestureAnimation.h"
#include "platform/fonts/FontCache.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/CompositorMutatorClient.h"
#include "platform/graphics/FirstPaintInvalidationTracking.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebGestureCurve.h"
#include "public/platform/WebImage.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTextInputInfo.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebVector.h"
#include "public/platform/WebViewScheduler.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebActiveWheelFlingParameters.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameWidget.h"
#include "public/web/WebHitTestResult.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebMeaningfulLayout.h"
#include "public/web/WebMediaPlayerAction.h"
#include "public/web/WebNode.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPluginAction.h"
#include "public/web/WebRange.h"
#include "public/web/WebScopedUserGesture.h"
#include "public/web/WebSelection.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWindowFeatures.h"
#include "web/AnimationWorkletProxyClientImpl.h"
#include "web/CompositionUnderlineVectorBuilder.h"
#include "web/CompositorMutatorImpl.h"
#include "web/CompositorWorkerProxyClientImpl.h"
#include "web/ContextFeaturesClientImpl.h"
#include "web/ContextMenuAllowedScope.h"
#include "web/DatabaseClientImpl.h"
#include "web/DedicatedWorkerMessagingProxyProviderImpl.h"
#include "web/DevToolsEmulator.h"
#include "web/FullscreenController.h"
#include "web/InspectorOverlay.h"
#include "web/LinkHighlightImpl.h"
#include "web/PageOverlay.h"
#include "web/PrerendererClientImpl.h"
#include "web/ResizeViewportAnchor.h"
#include "web/RotationViewportAnchor.h"
#include "web/SpeechRecognitionClientProxy.h"
#include "web/StorageQuotaClientImpl.h"
#include "web/ValidationMessageClientImpl.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebInputMethodControllerImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPagePopupImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "wtf/AutoReset.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"

#if USE(DEFAULT_RENDER_THEME)
#include "core/layout/LayoutThemeDefault.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath>  // for std::pow

// The following constants control parameters for automated scaling of webpages
// (such as due to a double tap gesture or find in page etc.). These are
// experimentally determined.
static const int touchPointPadding = 32;
static const int nonUserInitiatedPointPadding = 11;
static const float minScaleDifference = 0.01f;
static const float doubleTapZoomContentDefaultMargin = 5;
static const float doubleTapZoomContentMinimumMargin = 2;
static const double doubleTapZoomAnimationDurationInSeconds = 0.25;
static const float doubleTapZoomAlreadyLegibleRatio = 1.2f;

static const double multipleTargetsZoomAnimationDurationInSeconds = 0.25;
static const double findInPageAnimationDurationInSeconds = 0;

// Constants for viewport anchoring on resize.
static const float viewportAnchorCoordX = 0.5f;
static const float viewportAnchorCoordY = 0;

// Constants for zooming in on a focused text field.
static const double scrollAndScaleAnimationDurationInSeconds = 0.2;
static const int minReadableCaretHeight = 16;
static const int minReadableCaretHeightForTextArea = 13;
static const float minScaleChangeToTriggerZoom = 1.5f;
static const float leftBoxRatio = 0.3f;
static const int caretPadding = 10;

namespace blink {

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
const double WebView::textSizeMultiplierRatio = 1.2;
const double WebView::minTextSizeMultiplier = 0.5;
const double WebView::maxTextSizeMultiplier = 3.0;

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop
// invocations.
static Vector<std::unique_ptr<ScopedPageSuspender>>& pageSuspenderStack() {
  DEFINE_STATIC_LOCAL(Vector<std::unique_ptr<ScopedPageSuspender>>,
                      suspenderStack, ());
  return suspenderStack;
}

static bool shouldUseExternalPopupMenus = false;

namespace {

class EmptyEventListener final : public EventListener {
 public:
  static EmptyEventListener* create() { return new EmptyEventListener(); }

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

 private:
  EmptyEventListener() : EventListener(CPPEventListenerType) {}

  void handleEvent(ExecutionContext* executionContext, Event*) override {}
};

class ColorOverlay final : public PageOverlay::Delegate {
 public:
  explicit ColorOverlay(WebColor color) : m_color(color) {}

 private:
  void paintPageOverlay(const PageOverlay& pageOverlay,
                        GraphicsContext& graphicsContext,
                        const WebSize& size) const override {
    if (DrawingRecorder::useCachedDrawingIfPossible(
            graphicsContext, pageOverlay, DisplayItem::kPageOverlay))
      return;
    FloatRect rect(0, 0, size.width, size.height);
    DrawingRecorder drawingRecorder(graphicsContext, pageOverlay,
                                    DisplayItem::kPageOverlay, rect);
    graphicsContext.fillRect(rect, m_color);
  }

  WebColor m_color;
};

}  // namespace

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client,
                         WebPageVisibilityState visibilityState) {
  // Pass the WebViewImpl's self-reference to the caller.
  return WebViewImpl::create(client, visibilityState);
}

WebViewImpl* WebViewImpl::create(WebViewClient* client,
                                 WebPageVisibilityState visibilityState) {
  // Pass the WebViewImpl's self-reference to the caller.
  return adoptRef(new WebViewImpl(client, visibilityState)).leakRef();
}

void WebView::setUseExternalPopupMenus(bool useExternalPopupMenus) {
  shouldUseExternalPopupMenus = useExternalPopupMenus;
}

void WebView::updateVisitedLinkState(unsigned long long linkHash) {
  Page::visitedStateChanged(linkHash);
}

void WebView::resetVisitedLinkState(bool invalidateVisitedLinkHashes) {
  Page::allVisitedStateChanged(invalidateVisitedLinkHashes);
}

void WebView::willEnterModalLoop() {
  pageSuspenderStack().push_back(WTF::makeUnique<ScopedPageSuspender>());
}

void WebView::didExitModalLoop() {
  DCHECK(pageSuspenderStack().size());
  pageSuspenderStack().pop_back();
}

void WebViewImpl::setMainFrame(WebFrame* frame) {
  frame->toImplBase()->initializeCoreFrame(&page()->frameHost(), 0, nullAtom,
                                           nullAtom);
}

void WebViewImpl::setCredentialManagerClient(
    WebCredentialManagerClient* webCredentialManagerClient) {
  DCHECK(m_page);
  provideCredentialManagerClientTo(
      *m_page, new CredentialManagerClient(webCredentialManagerClient));
}

void WebViewImpl::setPrerendererClient(
    WebPrerendererClient* prerendererClient) {
  DCHECK(m_page);
  providePrerendererClientTo(
      *m_page, new PrerendererClientImpl(*m_page, prerendererClient));
}

void WebViewImpl::setSpellCheckClient(WebSpellCheckClient* spellCheckClient) {
  m_spellCheckClient = spellCheckClient;
}

// static
HashSet<WebViewImpl*>& WebViewImpl::allInstances() {
  DEFINE_STATIC_LOCAL(HashSet<WebViewImpl*>, allInstances, ());
  return allInstances;
}

WebViewImpl::WebViewImpl(WebViewClient* client,
                         WebPageVisibilityState visibilityState)
    : m_client(client),
      m_spellCheckClient(nullptr),
      m_chromeClientImpl(ChromeClientImpl::create(this)),
      m_contextMenuClientImpl(this),
      m_editorClientImpl(this),
      m_spellCheckerClientImpl(this),
      m_storageClientImpl(this),
      m_shouldAutoResize(false),
      m_zoomLevel(0),
      m_minimumZoomLevel(zoomFactorToZoomLevel(minTextSizeMultiplier)),
      m_maximumZoomLevel(zoomFactorToZoomLevel(maxTextSizeMultiplier)),
      m_zoomFactorForDeviceScaleFactor(0.f),
      m_maximumLegibleScale(1),
      m_doubleTapZoomPageScaleFactor(0),
      m_doubleTapZoomPending(false),
      m_enableFakePageScaleAnimationForTesting(false),
      m_fakePageScaleAnimationPageScaleFactor(0),
      m_fakePageScaleAnimationUseAnchor(false),
      m_compositorDeviceScaleFactorOverride(0),
      m_suppressNextKeypressEvent(false),
      m_imeAcceptEvents(true),
      m_devToolsEmulator(nullptr),
      m_isTransparent(false),
      m_tabsToLinks(false),
      m_layerTreeView(nullptr),
      m_rootLayer(nullptr),
      m_rootGraphicsLayer(nullptr),
      m_visualViewportContainerLayer(nullptr),
      m_matchesHeuristicsForGpuRasterization(false),
      m_flingModifier(0),
      m_flingSourceDevice(WebGestureDeviceUninitialized),
      m_fullscreenController(FullscreenController::create(this)),
      m_baseBackgroundColor(Color::white),
      m_baseBackgroundColorOverrideEnabled(false),
      m_baseBackgroundColorOverride(Color::transparent),
      m_backgroundColorOverride(Color::transparent),
      m_zoomFactorOverride(0),
      m_userGestureObserved(false),
      m_shouldDispatchFirstVisuallyNonEmptyLayout(false),
      m_shouldDispatchFirstLayoutAfterFinishedParsing(false),
      m_shouldDispatchFirstLayoutAfterFinishedLoading(false),
      m_displayMode(WebDisplayModeBrowser),
      m_elasticOverscroll(FloatSize()),
      m_mutator(nullptr),
      m_scheduler(WTF::wrapUnique(Platform::current()
                                      ->currentThread()
                                      ->scheduler()
                                      ->createWebViewScheduler(this, this)
                                      .release())),
      m_lastFrameTimeMonotonic(0),
      m_overrideCompositorVisibility(false) {
  Page::PageClients pageClients;
  pageClients.chromeClient = m_chromeClientImpl.get();
  pageClients.contextMenuClient = &m_contextMenuClientImpl;
  pageClients.editorClient = &m_editorClientImpl;
  pageClients.spellCheckerClient = &m_spellCheckerClientImpl;

  m_page = Page::createOrdinary(pageClients);
  MediaKeysController::provideMediaKeysTo(*m_page, &m_mediaKeysClientImpl);
  provideSpeechRecognitionTo(
      *m_page, SpeechRecognitionClientProxy::create(
                   client ? client->speechRecognizer() : nullptr));
  provideContextFeaturesTo(*m_page, ContextFeaturesClientImpl::create());
  provideDatabaseClientTo(*m_page, DatabaseClientImpl::create());

  provideStorageQuotaClientTo(*m_page, StorageQuotaClientImpl::create());
  m_page->setValidationMessageClient(
      ValidationMessageClientImpl::create(*this));
  provideDedicatedWorkerMessagingProxyProviderTo(
      *m_page, DedicatedWorkerMessagingProxyProviderImpl::create(*m_page));
  StorageNamespaceController::provideStorageNamespaceTo(*m_page,
                                                        &m_storageClientImpl);

  setVisibilityState(visibilityState, true);

  initializeLayerTreeView();

  m_devToolsEmulator = DevToolsEmulator::create(this);

  allInstances().insert(this);

  m_pageImportanceSignals.setObserver(client);
  m_resizeViewportAnchor = new ResizeViewportAnchor(*m_page);
}

WebViewImpl::~WebViewImpl() {
  DCHECK(!m_page);

  // Each highlight uses m_owningWebViewImpl->m_linkHighlightsTimeline
  // in destructor. m_linkHighlightsTimeline might be destroyed earlier
  // than m_linkHighlights.
  DCHECK(m_linkHighlights.isEmpty());
}

WebViewImpl::UserGestureNotifier::UserGestureNotifier(WebViewImpl* view)
    // TODO(kenrb, alexmos): |m_frame| should be set to the local root frame,
    // not the main frame. See crbug.com/589894.
    : m_frame(view->mainFrameImpl()),
      m_userGestureObserved(&view->m_userGestureObserved) {
  DCHECK(m_userGestureObserved);
}

WebViewImpl::UserGestureNotifier::~UserGestureNotifier() {
  if (!*m_userGestureObserved && m_frame &&
      m_frame->frame()->hasReceivedUserGesture()) {
    *m_userGestureObserved = true;
    if (m_frame && m_frame->autofillClient())
      m_frame->autofillClient()->firstUserGestureObserved();
  }
}

WebDevToolsAgentImpl* WebViewImpl::mainFrameDevToolsAgentImpl() {
  WebLocalFrameImpl* mainFrame = mainFrameImpl();
  return mainFrame ? mainFrame->devToolsAgentImpl() : nullptr;
}

InspectorOverlay* WebViewImpl::inspectorOverlay() {
  if (WebDevToolsAgentImpl* devtools = mainFrameDevToolsAgentImpl())
    return devtools->overlay();
  return nullptr;
}

WebLocalFrameImpl* WebViewImpl::mainFrameImpl() const {
  return m_page && m_page->mainFrame() && m_page->mainFrame()->isLocalFrame()
             ? WebLocalFrameImpl::fromFrame(m_page->deprecatedLocalMainFrame())
             : nullptr;
}

bool WebViewImpl::tabKeyCyclesThroughElements() const {
  DCHECK(m_page);
  return m_page->tabKeyCyclesThroughElements();
}

void WebViewImpl::setTabKeyCyclesThroughElements(bool value) {
  if (m_page)
    m_page->setTabKeyCyclesThroughElements(value);
}

void WebViewImpl::handleMouseLeave(LocalFrame& mainFrame,
                                   const WebMouseEvent& event) {
  m_client->setMouseOverURL(WebURL());
  PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebViewImpl::handleMouseDown(LocalFrame& mainFrame,
                                  const WebMouseEvent& event) {
  // If there is a popup open, close it as the user is clicking on the page
  // (outside of the popup). We also save it so we can prevent a click on an
  // element from immediately reopening the same popup.
  RefPtr<WebPagePopupImpl> pagePopup;
  if (event.button == WebMouseEvent::Button::Left) {
    pagePopup = m_pagePopup;
    hidePopups();
    DCHECK(!m_pagePopup);
  }

  // Take capture on a mouse down on a plugin so we can send it mouse events.
  // If the hit node is a plugin but a scrollbar is over it don't start mouse
  // capture because it will interfere with the scrollbar receiving events.
  IntPoint point(event.x, event.y);
  if (event.button == WebMouseEvent::Button::Left &&
      m_page->mainFrame()->isLocalFrame()) {
    point =
        m_page->deprecatedLocalMainFrame()->view()->rootFrameToContents(point);
    HitTestResult result(
        m_page->deprecatedLocalMainFrame()->eventHandler().hitTestResultAtPoint(
            point));
    result.setToShadowHostIfInUserAgentShadowRoot();
    Node* hitNode = result.innerNodeOrImageMapImage();

    if (!result.scrollbar() && hitNode && hitNode->layoutObject() &&
        hitNode->layoutObject()->isEmbeddedObject()) {
      m_mouseCaptureNode = hitNode;
      TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
    }
  }

  PageWidgetEventHandler::handleMouseDown(mainFrame, event);

  if (event.button == WebMouseEvent::Button::Left && m_mouseCaptureNode)
    m_mouseCaptureGestureToken =
        mainFrame.eventHandler().takeLastMouseDownGestureToken();

  if (m_pagePopup && pagePopup &&
      m_pagePopup->hasSamePopupClient(pagePopup.get())) {
    // That click triggered a page popup that is the same as the one we just
    // closed.  It needs to be closed.
    cancelPagePopup();
  }

  // Dispatch the contextmenu event regardless of if the click was swallowed.
  if (!page()->settings().getShowContextMenuOnMouseUp()) {
#if OS(MACOSX)
    if (event.button == WebMouseEvent::Button::Right ||
        (event.button == WebMouseEvent::Button::Left &&
         event.modifiers() & WebMouseEvent::ControlKey))
      mouseContextMenu(event);
#else
    if (event.button == WebMouseEvent::Button::Right)
      mouseContextMenu(event);
#endif
  }
}

void WebViewImpl::setDisplayMode(WebDisplayMode mode) {
  m_displayMode = mode;
  if (!mainFrameImpl() || !mainFrameImpl()->frameView())
    return;

  mainFrameImpl()->frameView()->setDisplayMode(mode);
}

void WebViewImpl::mouseContextMenu(const WebMouseEvent& event) {
  if (!mainFrameImpl() || !mainFrameImpl()->frameView())
    return;

  m_page->contextMenuController().clearContextMenu();

  WebMouseEvent transformedEvent =
      TransformWebMouseEvent(mainFrameImpl()->frameView(), event);
  IntPoint positionInRootFrame =
      flooredIntPoint(transformedEvent.positionInRootFrame());

  // Find the right target frame. See issue 1186900.
  HitTestResult result = hitTestResultForRootFramePos(positionInRootFrame);
  Frame* targetFrame;
  if (result.innerNodeOrImageMapImage())
    targetFrame = result.innerNodeOrImageMapImage()->document().frame();
  else
    targetFrame = m_page->focusController().focusedOrMainFrame();

  if (!targetFrame->isLocalFrame())
    return;

  LocalFrame* targetLocalFrame = toLocalFrame(targetFrame);
  {
    ContextMenuAllowedScope scope;
    targetLocalFrame->eventHandler().sendContextMenuEvent(transformedEvent,
                                                          nullptr);
  }
  // Actually showing the context menu is handled by the ContextMenuClient
  // implementation...
}

void WebViewImpl::handleMouseUp(LocalFrame& mainFrame,
                                const WebMouseEvent& event) {
  PageWidgetEventHandler::handleMouseUp(mainFrame, event);

  if (page()->settings().getShowContextMenuOnMouseUp()) {
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::Button::Right)
      mouseContextMenu(event);
  }
}

WebInputEventResult WebViewImpl::handleMouseWheel(
    LocalFrame& mainFrame,
    const WebMouseWheelEvent& event) {
  // Halt an in-progress fling on a wheel tick.
  if (!event.hasPreciseScrollingDeltas)
    endActiveFlingAnimation();

  hidePopups();
  return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

WebGestureEvent WebViewImpl::createGestureScrollEventFromFling(
    WebInputEvent::Type type,
    WebGestureDevice sourceDevice) const {
  WebGestureEvent gestureEvent(type, m_flingModifier,
                               WTF::monotonicallyIncreasingTime());
  gestureEvent.sourceDevice = sourceDevice;
  gestureEvent.x = m_positionOnFlingStart.x;
  gestureEvent.y = m_positionOnFlingStart.y;
  gestureEvent.globalX = m_globalPositionOnFlingStart.x;
  gestureEvent.globalY = m_globalPositionOnFlingStart.y;
  return gestureEvent;
}

bool WebViewImpl::scrollBy(const WebFloatSize& delta,
                           const WebFloatSize& velocity) {
  DCHECK_NE(m_flingSourceDevice, WebGestureDeviceUninitialized);
  if (!m_page || !m_page->mainFrame() || !m_page->mainFrame()->isLocalFrame() ||
      !m_page->deprecatedLocalMainFrame()->view())
    return false;

  if (m_flingSourceDevice == WebGestureDeviceTouchpad) {
    bool enableTouchpadScrollLatching =
        RuntimeEnabledFeatures::touchpadAndWheelScrollLatchingEnabled();
    WebMouseWheelEvent syntheticWheel(WebInputEvent::MouseWheel,
                                      m_flingModifier,
                                      WTF::monotonicallyIncreasingTime());
    const float tickDivisor = WheelEvent::TickMultiplier;

    syntheticWheel.deltaX = delta.width;
    syntheticWheel.deltaY = delta.height;
    syntheticWheel.wheelTicksX = delta.width / tickDivisor;
    syntheticWheel.wheelTicksY = delta.height / tickDivisor;
    syntheticWheel.hasPreciseScrollingDeltas = true;
    syntheticWheel.x = m_positionOnFlingStart.x;
    syntheticWheel.y = m_positionOnFlingStart.y;
    syntheticWheel.globalX = m_globalPositionOnFlingStart.x;
    syntheticWheel.globalY = m_globalPositionOnFlingStart.y;

    if (handleMouseWheel(*m_page->deprecatedLocalMainFrame(), syntheticWheel) !=
        WebInputEventResult::NotHandled)
      return true;

    if (!enableTouchpadScrollLatching) {
      WebGestureEvent syntheticScrollBegin = createGestureScrollEventFromFling(
          WebInputEvent::GestureScrollBegin, WebGestureDeviceTouchpad);
      syntheticScrollBegin.data.scrollBegin.deltaXHint = delta.width;
      syntheticScrollBegin.data.scrollBegin.deltaYHint = delta.height;
      syntheticScrollBegin.data.scrollBegin.inertialPhase =
          WebGestureEvent::MomentumPhase;
      handleGestureEvent(syntheticScrollBegin);
    }

    WebGestureEvent syntheticScrollUpdate = createGestureScrollEventFromFling(
        WebInputEvent::GestureScrollUpdate, WebGestureDeviceTouchpad);
    syntheticScrollUpdate.data.scrollUpdate.deltaX = delta.width;
    syntheticScrollUpdate.data.scrollUpdate.deltaY = delta.height;
    syntheticScrollUpdate.data.scrollUpdate.velocityX = velocity.width;
    syntheticScrollUpdate.data.scrollUpdate.velocityY = velocity.height;
    syntheticScrollUpdate.data.scrollUpdate.inertialPhase =
        WebGestureEvent::MomentumPhase;
    bool scrollUpdateHandled = handleGestureEvent(syntheticScrollUpdate) !=
                               WebInputEventResult::NotHandled;

    if (!enableTouchpadScrollLatching) {
      WebGestureEvent syntheticScrollEnd = createGestureScrollEventFromFling(
          WebInputEvent::GestureScrollEnd, WebGestureDeviceTouchpad);
      syntheticScrollEnd.data.scrollEnd.inertialPhase =
          WebGestureEvent::MomentumPhase;
      handleGestureEvent(syntheticScrollEnd);
    }

    return scrollUpdateHandled;
  } else {
    WebGestureEvent syntheticGestureEvent = createGestureScrollEventFromFling(
        WebInputEvent::GestureScrollUpdate, WebGestureDeviceTouchscreen);
    syntheticGestureEvent.data.scrollUpdate.preventPropagation = true;
    syntheticGestureEvent.data.scrollUpdate.deltaX = delta.width;
    syntheticGestureEvent.data.scrollUpdate.deltaY = delta.height;
    syntheticGestureEvent.data.scrollUpdate.velocityX = velocity.width;
    syntheticGestureEvent.data.scrollUpdate.velocityY = velocity.height;
    syntheticGestureEvent.data.scrollUpdate.inertialPhase =
        WebGestureEvent::MomentumPhase;

    return handleGestureEvent(syntheticGestureEvent) !=
           WebInputEventResult::NotHandled;
  }
}

WebInputEventResult WebViewImpl::handleGestureEvent(
    const WebGestureEvent& event) {
  if (!m_client)
    return WebInputEventResult::NotHandled;

  WebInputEventResult eventResult = WebInputEventResult::NotHandled;
  bool eventCancelled = false;  // for disambiguation

  // Special handling for slow-path fling gestures.
  switch (event.type()) {
    case WebInputEvent::GestureFlingStart: {
      if (mainFrameImpl()
              ->frame()
              ->eventHandler()
              .isScrollbarHandlingGestures())
        break;
      endActiveFlingAnimation();
      m_client->cancelScheduledContentIntents();
      m_positionOnFlingStart = WebPoint(event.x, event.y);
      m_globalPositionOnFlingStart = WebPoint(event.globalX, event.globalY);
      m_flingModifier = event.modifiers();
      m_flingSourceDevice = event.sourceDevice;
      DCHECK_NE(m_flingSourceDevice, WebGestureDeviceUninitialized);
      std::unique_ptr<WebGestureCurve> flingCurve =
          WTF::wrapUnique(Platform::current()->createFlingAnimationCurve(
              event.sourceDevice,
              WebFloatPoint(event.data.flingStart.velocityX,
                            event.data.flingStart.velocityY),
              WebSize()));
      DCHECK(flingCurve);
      m_gestureAnimation = WebActiveGestureAnimation::createAtAnimationStart(
          std::move(flingCurve), this);
      mainFrameImpl()->frameWidget()->scheduleAnimation();
      eventResult = WebInputEventResult::HandledSystem;

      WebGestureEvent scaledEvent =
          TransformWebGestureEvent(mainFrameImpl()->frameView(), event);
      // Plugins may need to see GestureFlingStart to balance
      // GestureScrollBegin (since the former replaces GestureScrollEnd when
      // transitioning to a fling).
      // TODO(dtapuska): Why isn't the response used?
      mainFrameImpl()->frame()->eventHandler().handleGestureScrollEvent(
          scaledEvent);

      m_client->didHandleGestureEvent(event, eventCancelled);
      return WebInputEventResult::HandledSystem;
    }
    case WebInputEvent::GestureFlingCancel:
      if (endActiveFlingAnimation())
        eventResult = WebInputEventResult::HandledSuppressed;

      m_client->didHandleGestureEvent(event, eventCancelled);
      return eventResult;
    default:
      break;
  }

  WebGestureEvent scaledEvent =
      TransformWebGestureEvent(mainFrameImpl()->frameView(), event);

  // Special handling for double tap and scroll events as we don't want to
  // hit test for them.
  switch (event.type()) {
    case WebInputEvent::GestureDoubleTap:
      if (m_webSettings->doubleTapToZoomEnabled() &&
          minimumPageScaleFactor() != maximumPageScaleFactor()) {
        m_client->cancelScheduledContentIntents();
        animateDoubleTapZoom(
            flooredIntPoint(scaledEvent.positionInRootFrame()));
      }
      // GestureDoubleTap is currently only used by Android for zooming. For
      // WebCore, GestureTap with tap count = 2 is used instead. So we drop
      // GestureDoubleTap here.
      eventResult = WebInputEventResult::HandledSystem;
      m_client->didHandleGestureEvent(event, eventCancelled);
      return eventResult;
    case WebInputEvent::GestureScrollBegin:
      m_client->cancelScheduledContentIntents();
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureFlingStart:
      // Scrolling-related gesture events invoke EventHandler recursively for
      // each frame down the chain, doing a single-frame hit-test per frame.
      // This matches handleWheelEvent.  Perhaps we could simplify things by
      // rewriting scroll handling to work inner frame out, and then unify with
      // other gesture events.
      eventResult =
          mainFrameImpl()->frame()->eventHandler().handleGestureScrollEvent(
              scaledEvent);
      m_client->didHandleGestureEvent(event, eventCancelled);
      return eventResult;
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
      return WebInputEventResult::NotHandled;
    default:
      break;
  }

  // Hit test across all frames and do touch adjustment as necessary for the
  // event type.
  GestureEventWithHitTestResults targetedEvent =
      m_page->deprecatedLocalMainFrame()->eventHandler().targetGestureEvent(
          scaledEvent);

  // Handle link highlighting outside the main switch to avoid getting lost in
  // the complicated set of cases handled below.
  switch (event.type()) {
    case WebInputEvent::GestureShowPress:
      // Queue a highlight animation, then hand off to regular handler.
      enableTapHighlightAtPoint(targetedEvent);
      break;
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureLongPress:
      for (size_t i = 0; i < m_linkHighlights.size(); ++i)
        m_linkHighlights[i]->startHighlightAnimationIfNeeded();
      break;
    default:
      break;
  }

  switch (event.type()) {
    case WebInputEvent::GestureTap: {
      m_client->cancelScheduledContentIntents();
      if (detectContentOnTouch(targetedEvent)) {
        eventResult = WebInputEventResult::HandledSystem;
        break;
      }

      // Don't trigger a disambiguation popup on sites designed for mobile
      // devices.  Instead, assume that the page has been designed with big
      // enough buttons and links.  Don't trigger a disambiguation popup when
      // screencasting, since it's implemented outside of compositor pipeline
      // and is not being screencasted itself. This leads to bad user
      // experience.
      WebDevToolsAgentImpl* devTools = mainFrameDevToolsAgentImpl();
      VisualViewport& visualViewport = page()->frameHost().visualViewport();
      bool screencastEnabled = devTools && devTools->screencastEnabled();
      if (event.data.tap.width > 0 &&
          !visualViewport.shouldDisableDesktopWorkarounds() &&
          !screencastEnabled) {
        IntRect boundingBox(visualViewport.viewportToRootFrame(
            IntRect(event.x - event.data.tap.width / 2,
                    event.y - event.data.tap.height / 2, event.data.tap.width,
                    event.data.tap.height)));

        // TODO(bokan): We shouldn't pass details of the VisualViewport offset
        // to render_view_impl.  crbug.com/459591
        WebSize visualViewportOffset =
            flooredIntSize(visualViewport.getScrollOffset());

        if (m_webSettings->multiTargetTapNotificationEnabled()) {
          Vector<IntRect> goodTargets;
          HeapVector<Member<Node>> highlightNodes;
          findGoodTouchTargets(boundingBox, mainFrameImpl()->frame(),
                               goodTargets, highlightNodes);
          // FIXME: replace touch adjustment code when numberOfGoodTargets == 1?
          // Single candidate case is currently handled by:
          // https://bugs.webkit.org/show_bug.cgi?id=85101
          if (goodTargets.size() >= 2 && m_client &&
              m_client->didTapMultipleTargets(visualViewportOffset, boundingBox,
                                              goodTargets)) {
            enableTapHighlights(highlightNodes);
            for (size_t i = 0; i < m_linkHighlights.size(); ++i)
              m_linkHighlights[i]->startHighlightAnimationIfNeeded();
            eventResult = WebInputEventResult::HandledSystem;
            eventCancelled = true;
            break;
          }
        }
      }

      eventResult = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(
          targetedEvent);
      if (m_pagePopup && m_lastHiddenPagePopup &&
          m_pagePopup->hasSamePopupClient(m_lastHiddenPagePopup.get())) {
        // The tap triggered a page popup that is the same as the one we just
        // closed. It needs to be closed.
        cancelPagePopup();
      }
      m_lastHiddenPagePopup = nullptr;
      break;
    }
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap: {
      if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        break;

      m_client->cancelScheduledContentIntents();
      m_page->contextMenuController().clearContextMenu();
      {
        ContextMenuAllowedScope scope;
        eventResult =
            mainFrameImpl()->frame()->eventHandler().handleGestureEvent(
                targetedEvent);
      }

      break;
    }
    case WebInputEvent::GestureTapDown: {
      // Touch pinch zoom and scroll on the page (outside of a popup) must hide
      // the popup. In case of a touch scroll or pinch zoom, this function is
      // called with GestureTapDown rather than a GSB/GSU/GSE or GPB/GPU/GPE.
      // When we close a popup because of a GestureTapDown, we also save it so
      // we can prevent the following GestureTap from immediately reopening the
      // same popup.
      m_lastHiddenPagePopup = m_pagePopup;
      hidePopups();
      DCHECK(!m_pagePopup);
      eventResult = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(
          targetedEvent);
      break;
    }
    case WebInputEvent::GestureTapCancel: {
      m_lastHiddenPagePopup = nullptr;
      eventResult = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(
          targetedEvent);
      break;
    }
    case WebInputEvent::GestureShowPress: {
      m_client->cancelScheduledContentIntents();
      eventResult = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(
          targetedEvent);
      break;
    }
    case WebInputEvent::GestureTapUnconfirmed: {
      eventResult = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(
          targetedEvent);
      break;
    }
    default: { NOTREACHED(); }
  }
  m_client->didHandleGestureEvent(event, eventCancelled);
  return eventResult;
}

WebInputEventResult WebViewImpl::handleSyntheticWheelFromTouchpadPinchEvent(
    const WebGestureEvent& pinchEvent) {
  DCHECK_EQ(pinchEvent.type(), WebInputEvent::GesturePinchUpdate);

  // For pinch gesture events, match typical trackpad behavior on Windows by
  // sending fake wheel events with the ctrl modifier set when we see trackpad
  // pinch gestures.  Ideally we'd someday get a platform 'pinch' event and
  // send that instead.
  WebMouseWheelEvent wheelEvent(
      WebInputEvent::MouseWheel,
      pinchEvent.modifiers() | WebInputEvent::ControlKey,
      pinchEvent.timeStampSeconds());
  wheelEvent.windowX = wheelEvent.x = pinchEvent.x;
  wheelEvent.windowY = wheelEvent.y = pinchEvent.y;
  wheelEvent.globalX = pinchEvent.globalX;
  wheelEvent.globalY = pinchEvent.globalY;
  wheelEvent.deltaX = 0;

  // The function to convert scales to deltaY values is designed to be
  // compatible with websites existing use of wheel events, and with existing
  // Windows trackpad behavior.  In particular, we want:
  //  - deltas should accumulate via addition: f(s1*s2)==f(s1)+f(s2)
  //  - deltas should invert via negation: f(1/s) == -f(s)
  //  - zoom in should be positive: f(s) > 0 iff s > 1
  //  - magnitude roughly matches wheels: f(2) > 25 && f(2) < 100
  //  - a formula that's relatively easy to use from JavaScript
  // Note that 'wheel' event deltaY values have their sign inverted.  So to
  // convert a wheel deltaY back to a scale use Math.exp(-deltaY/100).
  DCHECK_GT(pinchEvent.data.pinchUpdate.scale, 0);
  wheelEvent.deltaY = 100.0f * log(pinchEvent.data.pinchUpdate.scale);
  wheelEvent.hasPreciseScrollingDeltas = true;
  wheelEvent.wheelTicksX = 0;
  wheelEvent.wheelTicksY = pinchEvent.data.pinchUpdate.scale > 1 ? 1 : -1;

  return handleInputEvent(blink::WebCoalescedInputEvent(wheelEvent));
}

void WebViewImpl::transferActiveWheelFlingAnimation(
    const WebActiveWheelFlingParameters& parameters) {
  TRACE_EVENT0("blink", "WebViewImpl::transferActiveWheelFlingAnimation");
  DCHECK(!m_gestureAnimation);
  m_positionOnFlingStart = parameters.point;
  m_globalPositionOnFlingStart = parameters.globalPoint;
  m_flingModifier = parameters.modifiers;
  std::unique_ptr<WebGestureCurve> curve =
      WTF::wrapUnique(Platform::current()->createFlingAnimationCurve(
          parameters.sourceDevice, WebFloatPoint(parameters.delta),
          parameters.cumulativeScroll));
  DCHECK(curve);
  m_gestureAnimation = WebActiveGestureAnimation::createWithTimeOffset(
      std::move(curve), this, parameters.startTime);
  DCHECK_NE(parameters.sourceDevice, WebGestureDeviceUninitialized);
  m_flingSourceDevice = parameters.sourceDevice;
  mainFrameImpl()->frameWidget()->scheduleAnimation();
}

bool WebViewImpl::endActiveFlingAnimation() {
  if (m_gestureAnimation) {
    m_gestureAnimation.reset();
    m_flingSourceDevice = WebGestureDeviceUninitialized;
    if (m_layerTreeView)
      m_layerTreeView->didStopFlinging();
    return true;
  }
  return false;
}

bool WebViewImpl::startPageScaleAnimation(const IntPoint& targetPosition,
                                          bool useAnchor,
                                          float newScale,
                                          double durationInSeconds) {
  VisualViewport& visualViewport = page()->frameHost().visualViewport();
  WebPoint clampedPoint = targetPosition;
  if (!useAnchor) {
    clampedPoint =
        visualViewport.clampDocumentOffsetAtScale(targetPosition, newScale);
    if (!durationInSeconds) {
      setPageScaleFactor(newScale);

      FrameView* view = mainFrameImpl()->frameView();
      if (view && view->getScrollableArea()) {
        view->getScrollableArea()->setScrollOffset(
            ScrollOffset(clampedPoint.x, clampedPoint.y), ProgrammaticScroll);
      }

      return false;
    }
  }
  if (useAnchor && newScale == pageScaleFactor())
    return false;

  if (m_enableFakePageScaleAnimationForTesting) {
    m_fakePageScaleAnimationTargetPosition = targetPosition;
    m_fakePageScaleAnimationUseAnchor = useAnchor;
    m_fakePageScaleAnimationPageScaleFactor = newScale;
  } else {
    if (!m_layerTreeView)
      return false;
    m_layerTreeView->startPageScaleAnimation(targetPosition, useAnchor,
                                             newScale, durationInSeconds);
  }
  return true;
}

void WebViewImpl::enableFakePageScaleAnimationForTesting(bool enable) {
  m_enableFakePageScaleAnimationForTesting = enable;
}

void WebViewImpl::setShowFPSCounter(bool show) {
  if (m_layerTreeView) {
    TRACE_EVENT0("blink", "WebViewImpl::setShowFPSCounter");
    m_layerTreeView->setShowFPSCounter(show);
  }
}

void WebViewImpl::setShowPaintRects(bool show) {
  if (m_layerTreeView) {
    TRACE_EVENT0("blink", "WebViewImpl::setShowPaintRects");
    m_layerTreeView->setShowPaintRects(show);
  }
  FirstPaintInvalidationTracking::setEnabledForShowPaintRects(show);
}

void WebViewImpl::setShowDebugBorders(bool show) {
  if (m_layerTreeView)
    m_layerTreeView->setShowDebugBorders(show);
}

void WebViewImpl::setShowScrollBottleneckRects(bool show) {
  if (m_layerTreeView)
    m_layerTreeView->setShowScrollBottleneckRects(show);
}

void WebViewImpl::acceptLanguagesChanged() {
  if (m_client)
    FontCache::acceptLanguagesChanged(m_client->acceptLanguages());

  if (!page())
    return;

  page()->acceptLanguagesChanged();
}

void WebViewImpl::ReportIntervention(const WebString& message) {
  if (!mainFrameImpl())
    return;
  WebConsoleMessage consoleMessage(WebConsoleMessage::LevelWarning, message);
  mainFrameImpl()->addMessageToConsole(consoleMessage);
}

float WebViewImpl::expensiveBackgroundThrottlingCPUBudget() {
  return settingsImpl()->expensiveBackgroundThrottlingCPUBudget();
}

float WebViewImpl::expensiveBackgroundThrottlingInitialBudget() {
  return settingsImpl()->expensiveBackgroundThrottlingInitialBudget();
}

float WebViewImpl::expensiveBackgroundThrottlingMaxBudget() {
  return settingsImpl()->expensiveBackgroundThrottlingMaxBudget();
}

float WebViewImpl::expensiveBackgroundThrottlingMaxDelay() {
  return settingsImpl()->expensiveBackgroundThrottlingMaxDelay();
}

WebInputEventResult WebViewImpl::handleKeyEvent(const WebKeyboardEvent& event) {
  DCHECK((event.type() == WebInputEvent::RawKeyDown) ||
         (event.type() == WebInputEvent::KeyDown) ||
         (event.type() == WebInputEvent::KeyUp));
  TRACE_EVENT2("input", "WebViewImpl::handleKeyEvent", "type",
               WebInputEvent::GetName(event.type()), "text",
               String(event.text).utf8());

  // Halt an in-progress fling on a key event.
  endActiveFlingAnimation();

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.
  // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
  // Webkit. A keyDown event is typically associated with a keyPress(char)
  // event and a keyUp event. We reset this flag here as this is a new keyDown
  // event.
  m_suppressNextKeypressEvent = false;

  // If there is a popup, it should be the one processing the event, not the
  // page.
  if (m_pagePopup) {
    m_pagePopup->handleKeyEvent(event);
    // We need to ignore the next Char event after this otherwise pressing
    // enter when selecting an item in the popup will go to the page.
    if (WebInputEvent::RawKeyDown == event.type())
      m_suppressNextKeypressEvent = true;
    return WebInputEventResult::HandledSystem;
  }

  Frame* focusedFrame = focusedCoreFrame();
  if (focusedFrame && focusedFrame->isRemoteFrame()) {
    WebRemoteFrameImpl* webFrame =
        WebRemoteFrameImpl::fromFrame(*toRemoteFrame(focusedFrame));
    webFrame->client()->forwardInputEvent(&event);
    return WebInputEventResult::HandledSystem;
  }

  if (!focusedFrame || !focusedFrame->isLocalFrame())
    return WebInputEventResult::NotHandled;

  LocalFrame* frame = toLocalFrame(focusedFrame);

  WebInputEventResult result = frame->eventHandler().keyEvent(event);
  if (result != WebInputEventResult::NotHandled) {
    if (WebInputEvent::RawKeyDown == event.type()) {
      // Suppress the next keypress event unless the focused node is a plugin
      // node.  (Flash needs these keypress events to handle non-US keyboards.)
      Element* element = focusedElement();
      if (element && element->layoutObject() &&
          element->layoutObject()->isEmbeddedObject()) {
        if (event.windowsKeyCode == VKEY_TAB) {
          // If the plugin supports keyboard focus then we should not send a tab
          // keypress event.
          FrameViewBase* frameViewBase =
              toLayoutPart(element->layoutObject())->widget();
          if (frameViewBase && frameViewBase->isPluginContainer()) {
            WebPluginContainerImpl* plugin =
                toWebPluginContainerImpl(frameViewBase);
            if (plugin && plugin->supportsKeyboardFocus())
              m_suppressNextKeypressEvent = true;
          }
        }
      } else {
        m_suppressNextKeypressEvent = true;
      }
    }
    return result;
  }

#if !OS(MACOSX)
  const WebInputEvent::Type contextMenuKeyTriggeringEventType =
#if OS(WIN)
      WebInputEvent::KeyUp;
#else
      WebInputEvent::RawKeyDown;
#endif
  const WebInputEvent::Type shiftF10TriggeringEventType =
      WebInputEvent::RawKeyDown;

  bool isUnmodifiedMenuKey =
      !(event.modifiers() & WebInputEvent::InputModifiers) &&
      event.windowsKeyCode == VKEY_APPS;
  bool isShiftF10 = (event.modifiers() & WebInputEvent::InputModifiers) ==
                        WebInputEvent::ShiftKey &&
                    event.windowsKeyCode == VKEY_F10;
  if ((isUnmodifiedMenuKey &&
       event.type() == contextMenuKeyTriggeringEventType) ||
      (isShiftF10 && event.type() == shiftF10TriggeringEventType)) {
    sendContextMenuEvent(event);
    return WebInputEventResult::HandledSystem;
  }
#endif  // !OS(MACOSX)

  return WebInputEventResult::NotHandled;
}

WebInputEventResult WebViewImpl::handleCharEvent(
    const WebKeyboardEvent& event) {
  DCHECK_EQ(event.type(), WebInputEvent::Char);
  TRACE_EVENT1("input", "WebViewImpl::handleCharEvent", "text",
               String(event.text).utf8());

  // Please refer to the comments explaining the m_suppressNextKeypressEvent
  // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
  // handled by Webkit. A keyDown event is typically associated with a
  // keyPress(char) event and a keyUp event. We reset this flag here as it
  // only applies to the current keyPress event.
  bool suppress = m_suppressNextKeypressEvent;
  m_suppressNextKeypressEvent = false;

  // If there is a popup, it should be the one processing the event, not the
  // page.
  if (m_pagePopup)
    return m_pagePopup->handleKeyEvent(event);

  LocalFrame* frame = toLocalFrame(focusedCoreFrame());
  if (!frame)
    return suppress ? WebInputEventResult::HandledSuppressed
                    : WebInputEventResult::NotHandled;

  EventHandler& handler = frame->eventHandler();

  if (!event.isCharacterKey())
    return WebInputEventResult::HandledSuppressed;

  // Accesskeys are triggered by char events and can't be suppressed.
  if (handler.handleAccessKey(event))
    return WebInputEventResult::HandledSystem;

  // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
  // the eventHandler::keyEvent. We mimic this behavior on all platforms since
  // for now we are converting other platform's key events to windows key
  // events.
  if (event.isSystemKey)
    return WebInputEventResult::NotHandled;

  if (suppress)
    return WebInputEventResult::HandledSuppressed;

  WebInputEventResult result = handler.keyEvent(event);
  if (result != WebInputEventResult::NotHandled)
    return result;

  return WebInputEventResult::NotHandled;
}

WebRect WebViewImpl::computeBlockBound(const WebPoint& pointInRootFrame,
                                       bool ignoreClipping) {
  if (!mainFrameImpl())
    return WebRect();

  // Use the point-based hit test to find the node.
  IntPoint point = mainFrameImpl()->frameView()->rootFrameToContents(
      IntPoint(pointInRootFrame.x, pointInRootFrame.y));
  HitTestRequest::HitTestRequestType hitType =
      HitTestRequest::ReadOnly | HitTestRequest::Active |
      (ignoreClipping ? HitTestRequest::IgnoreClipping : 0);
  HitTestResult result =
      mainFrameImpl()->frame()->eventHandler().hitTestResultAtPoint(point,
                                                                    hitType);
  result.setToShadowHostIfInUserAgentShadowRoot();

  Node* node = result.innerNodeOrImageMapImage();
  if (!node)
    return WebRect();

  // Find the block type node based on the hit node.
  // FIXME: This wants to walk flat tree with
  // LayoutTreeBuilderTraversal::parent().
  while (node && (!node->layoutObject() || node->layoutObject()->isInline()))
    node = LayoutTreeBuilderTraversal::parent(*node);

  // Return the bounding box in the root frame's coordinate space.
  if (node) {
    IntRect pointInRootFrame = node->Node::pixelSnappedBoundingBox();
    LocalFrame* frame = node->document().frame();
    return frame->view()->contentsToRootFrame(pointInRootFrame);
  }
  return WebRect();
}

WebRect WebViewImpl::widenRectWithinPageBounds(const WebRect& source,
                                               int targetMargin,
                                               int minimumMargin) {
  WebSize maxSize;
  if (mainFrame())
    maxSize = mainFrame()->contentsSize();
  IntSize scrollOffset;
  if (mainFrame())
    scrollOffset = mainFrame()->getScrollOffset();
  int leftMargin = targetMargin;
  int rightMargin = targetMargin;

  const int absoluteSourceX = source.x + scrollOffset.width();
  if (leftMargin > absoluteSourceX) {
    leftMargin = absoluteSourceX;
    rightMargin = std::max(leftMargin, minimumMargin);
  }

  const int maximumRightMargin =
      maxSize.width - (source.width + absoluteSourceX);
  if (rightMargin > maximumRightMargin) {
    rightMargin = maximumRightMargin;
    leftMargin = std::min(leftMargin, std::max(rightMargin, minimumMargin));
  }

  const int newWidth = source.width + leftMargin + rightMargin;
  const int newX = source.x - leftMargin;

  DCHECK_GE(newWidth, 0);
  DCHECK_LE(scrollOffset.width() + newX + newWidth, maxSize.width);

  return WebRect(newX, source.y, newWidth, source.height);
}

float WebViewImpl::maximumLegiblePageScale() const {
  // Pages should be as legible as on desktop when at dpi scale, so no
  // need to zoom in further when automatically determining zoom level
  // (after double tap, find in page, etc), though the user should still
  // be allowed to manually pinch zoom in further if they desire.
  if (page()) {
    return m_maximumLegibleScale *
           page()->settings().getAccessibilityFontScaleFactor();
  }
  return m_maximumLegibleScale;
}

void WebViewImpl::computeScaleAndScrollForBlockRect(
    const WebPoint& hitPointInRootFrame,
    const WebRect& blockRectInRootFrame,
    float padding,
    float defaultScaleWhenAlreadyLegible,
    float& scale,
    WebPoint& scroll) {
  scale = pageScaleFactor();
  scroll.x = scroll.y = 0;

  WebRect rect = blockRectInRootFrame;

  if (!rect.isEmpty()) {
    float defaultMargin = doubleTapZoomContentDefaultMargin;
    float minimumMargin = doubleTapZoomContentMinimumMargin;
    // We want the margins to have the same physical size, which means we
    // need to express them in post-scale size. To do that we'd need to know
    // the scale we're scaling to, but that depends on the margins. Instead
    // we express them as a fraction of the target rectangle: this will be
    // correct if we end up fully zooming to it, and won't matter if we
    // don't.
    rect = widenRectWithinPageBounds(
        rect, static_cast<int>(defaultMargin * rect.width / m_size.width),
        static_cast<int>(minimumMargin * rect.width / m_size.width));
    // Fit block to screen, respecting limits.
    scale = static_cast<float>(m_size.width) / rect.width;
    scale = std::min(scale, maximumLegiblePageScale());
    if (pageScaleFactor() < defaultScaleWhenAlreadyLegible)
      scale = std::max(scale, defaultScaleWhenAlreadyLegible);
    scale = clampPageScaleFactorToLimits(scale);
  }

  // FIXME: If this is being called for auto zoom during find in page,
  // then if the user manually zooms in it'd be nice to preserve the
  // relative increase in zoom they caused (if they zoom out then it's ok
  // to zoom them back in again). This isn't compatible with our current
  // double-tap zoom strategy (fitting the containing block to the screen)
  // though.

  float screenWidth = m_size.width / scale;
  float screenHeight = m_size.height / scale;

  // Scroll to vertically align the block.
  if (rect.height < screenHeight) {
    // Vertically center short blocks.
    rect.y -= 0.5 * (screenHeight - rect.height);
  } else {
    // Ensure position we're zooming to (+ padding) isn't off the bottom of
    // the screen.
    rect.y =
        std::max<float>(rect.y, hitPointInRootFrame.y + padding - screenHeight);
  }  // Otherwise top align the block.

  // Do the same thing for horizontal alignment.
  if (rect.width < screenWidth)
    rect.x -= 0.5 * (screenWidth - rect.width);
  else
    rect.x =
        std::max<float>(rect.x, hitPointInRootFrame.x + padding - screenWidth);
  scroll.x = rect.x;
  scroll.y = rect.y;

  scale = clampPageScaleFactorToLimits(scale);
  scroll = mainFrameImpl()->frameView()->rootFrameToContents(scroll);
  scroll = page()->frameHost().visualViewport().clampDocumentOffsetAtScale(
      scroll, scale);
}

static Node* findCursorDefiningAncestor(Node* node, LocalFrame* frame) {
  // Go up the tree to find the node that defines a mouse cursor style
  while (node) {
    if (node->layoutObject()) {
      ECursor cursor = node->layoutObject()->style()->cursor();
      if (cursor != ECursor::kAuto ||
          frame->eventHandler().useHandCursor(node, node->isLink()))
        break;
    }
    node = LayoutTreeBuilderTraversal::parent(*node);
  }

  return node;
}

static bool showsHandCursor(Node* node, LocalFrame* frame) {
  if (!node || !node->layoutObject())
    return false;

  ECursor cursor = node->layoutObject()->style()->cursor();
  return cursor == ECursor::kPointer ||
         (cursor == ECursor::kAuto &&
          frame->eventHandler().useHandCursor(node, node->isLink()));
}

Node* WebViewImpl::bestTapNode(
    const GestureEventWithHitTestResults& targetedTapEvent) {
  TRACE_EVENT0("input", "WebViewImpl::bestTapNode");

  if (!m_page || !m_page->mainFrame())
    return nullptr;

  Node* bestTouchNode = targetedTapEvent.hitTestResult().innerNode();
  if (!bestTouchNode)
    return nullptr;

  // We might hit something like an image map that has no layoutObject on it
  // Walk up the tree until we have a node with an attached layoutObject
  while (!bestTouchNode->layoutObject()) {
    bestTouchNode = LayoutTreeBuilderTraversal::parent(*bestTouchNode);
    if (!bestTouchNode)
      return nullptr;
  }

  // Editable nodes should not be highlighted (e.g., <input>)
  if (hasEditableStyle(*bestTouchNode))
    return nullptr;

  Node* cursorDefiningAncestor = findCursorDefiningAncestor(
      bestTouchNode, m_page->deprecatedLocalMainFrame());
  // We show a highlight on tap only when the current node shows a hand cursor
  if (!cursorDefiningAncestor ||
      !showsHandCursor(cursorDefiningAncestor,
                       m_page->deprecatedLocalMainFrame())) {
    return nullptr;
  }

  // We should pick the largest enclosing node with hand cursor set. We do this
  // by first jumping up to cursorDefiningAncestor (which is already known to
  // have hand cursor set). Then we locate the next cursor-defining ancestor up
  // in the the tree and repeat the jumps as long as the node has hand cursor
  // set.
  do {
    bestTouchNode = cursorDefiningAncestor;
    cursorDefiningAncestor = findCursorDefiningAncestor(
        LayoutTreeBuilderTraversal::parent(*bestTouchNode),
        m_page->deprecatedLocalMainFrame());
  } while (cursorDefiningAncestor &&
           showsHandCursor(cursorDefiningAncestor,
                           m_page->deprecatedLocalMainFrame()));

  return bestTouchNode;
}

void WebViewImpl::enableTapHighlightAtPoint(
    const GestureEventWithHitTestResults& targetedTapEvent) {
  Node* touchNode = bestTapNode(targetedTapEvent);

  HeapVector<Member<Node>> highlightNodes;
  highlightNodes.push_back(touchNode);

  enableTapHighlights(highlightNodes);
}

void WebViewImpl::enableTapHighlights(
    HeapVector<Member<Node>>& highlightNodes) {
  if (highlightNodes.isEmpty())
    return;

  // Always clear any existing highlight when this is invoked, even if we
  // don't get a new target to highlight.
  m_linkHighlights.clear();

  for (size_t i = 0; i < highlightNodes.size(); ++i) {
    Node* node = highlightNodes[i];

    if (!node || !node->layoutObject())
      continue;

    Color highlightColor = node->layoutObject()->style()->tapHighlightColor();
    // Safari documentation for -webkit-tap-highlight-color says if the
    // specified color has 0 alpha, then tap highlighting is disabled.
    // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
    if (!highlightColor.alpha())
      continue;

    m_linkHighlights.push_back(LinkHighlightImpl::create(node, this));
  }

  updateAllLifecyclePhases();
}

void WebViewImpl::animateDoubleTapZoom(const IntPoint& pointInRootFrame) {
  if (!mainFrameImpl())
    return;

  WebRect blockBounds = computeBlockBound(pointInRootFrame, false);
  float scale;
  WebPoint scroll;

  computeScaleAndScrollForBlockRect(
      pointInRootFrame, blockBounds, touchPointPadding,
      minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio, scale,
      scroll);

  bool stillAtPreviousDoubleTapScale =
      (pageScaleFactor() == m_doubleTapZoomPageScaleFactor &&
       m_doubleTapZoomPageScaleFactor != minimumPageScaleFactor()) ||
      m_doubleTapZoomPending;

  bool scaleUnchanged = fabs(pageScaleFactor() - scale) < minScaleDifference;
  bool shouldZoomOut =
      blockBounds.isEmpty() || scaleUnchanged || stillAtPreviousDoubleTapScale;

  bool isAnimating;

  if (shouldZoomOut) {
    scale = minimumPageScaleFactor();
    IntPoint targetPosition =
        mainFrameImpl()->frameView()->rootFrameToContents(pointInRootFrame);
    isAnimating = startPageScaleAnimation(
        targetPosition, true, scale, doubleTapZoomAnimationDurationInSeconds);
  } else {
    isAnimating = startPageScaleAnimation(
        scroll, false, scale, doubleTapZoomAnimationDurationInSeconds);
  }

  // TODO(dglazkov): The only reason why we're using isAnimating and not just
  // checking for m_layerTreeView->hasPendingPageScaleAnimation() is because of
  // fake page scale animation plumbing for testing, which doesn't actually
  // initiate a page scale animation.
  if (isAnimating) {
    m_doubleTapZoomPageScaleFactor = scale;
    m_doubleTapZoomPending = true;
  }
}

void WebViewImpl::zoomToFindInPageRect(const WebRect& rectInRootFrame) {
  if (!mainFrameImpl())
    return;

  WebRect blockBounds = computeBlockBound(
      WebPoint(rectInRootFrame.x + rectInRootFrame.width / 2,
               rectInRootFrame.y + rectInRootFrame.height / 2),
      true);

  if (blockBounds.isEmpty()) {
    // Keep current scale (no need to scroll as x,y will normally already
    // be visible). FIXME: Revisit this if it isn't always true.
    return;
  }

  float scale;
  WebPoint scroll;

  computeScaleAndScrollForBlockRect(
      WebPoint(rectInRootFrame.x, rectInRootFrame.y), blockBounds,
      nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

  startPageScaleAnimation(scroll, false, scale,
                          findInPageAnimationDurationInSeconds);
}

bool WebViewImpl::zoomToMultipleTargetsRect(const WebRect& rectInRootFrame) {
  if (!mainFrameImpl())
    return false;

  float scale;
  WebPoint scroll;

  computeScaleAndScrollForBlockRect(
      WebPoint(rectInRootFrame.x, rectInRootFrame.y), rectInRootFrame,
      nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

  if (scale <= pageScaleFactor())
    return false;

  startPageScaleAnimation(scroll, false, scale,
                          multipleTargetsZoomAnimationDurationInSeconds);
  return true;
}

bool WebViewImpl::hasTouchEventHandlersAt(const WebPoint& point) {
  // FIXME: Implement this. Note that the point must be divided by
  // pageScaleFactor.
  return true;
}

#if !OS(MACOSX)
// Mac has no way to open a context menu based on a keyboard event.
WebInputEventResult WebViewImpl::sendContextMenuEvent(
    const WebKeyboardEvent& event) {
  // The contextMenuController() holds onto the last context menu that was
  // popped up on the page until a new one is created. We need to clear
  // this menu before propagating the event through the DOM so that we can
  // detect if we create a new menu for this event, since we won't create
  // a new menu if the DOM swallows the event and the defaultEventHandler does
  // not run.
  page()->contextMenuController().clearContextMenu();

  {
    ContextMenuAllowedScope scope;
    Frame* focusedFrame = page()->focusController().focusedOrMainFrame();
    if (!focusedFrame->isLocalFrame())
      return WebInputEventResult::NotHandled;
    // Firefox reveal focus based on "keydown" event but not "contextmenu"
    // event, we match FF.
    if (Element* focusedElement =
            toLocalFrame(focusedFrame)->document()->focusedElement())
      focusedElement->scrollIntoViewIfNeeded();
    return toLocalFrame(focusedFrame)
        ->eventHandler()
        .sendContextMenuEventForKey(nullptr);
  }
}
#endif

void WebViewImpl::showContextMenuAtPoint(float x,
                                         float y,
                                         ContextMenuProvider* menuProvider) {
  if (!page()->mainFrame()->isLocalFrame())
    return;
  {
    ContextMenuAllowedScope scope;
    page()->contextMenuController().clearContextMenu();
    page()->contextMenuController().showContextMenuAtPoint(
        page()->deprecatedLocalMainFrame(), x, y, menuProvider);
  }
}

void WebViewImpl::showContextMenuForElement(WebElement element) {
  if (!page())
    return;

  page()->contextMenuController().clearContextMenu();
  {
    ContextMenuAllowedScope scope;
    if (LocalFrame* focusedFrame =
            toLocalFrame(page()->focusController().focusedOrMainFrame()))
      focusedFrame->eventHandler().sendContextMenuEventForKey(
          element.unwrap<Element>());
  }
}

PagePopup* WebViewImpl::openPagePopup(PagePopupClient* client) {
  DCHECK(client);
  if (hasOpenedPopup())
    hidePopups();
  DCHECK(!m_pagePopup);

  WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypePage);
  // createPopupMenu returns nullptr if this renderer process is about to die.
  if (!popupWidget)
    return nullptr;
  m_pagePopup = toWebPagePopupImpl(popupWidget);
  if (!m_pagePopup->initialize(this, client)) {
    m_pagePopup->closePopup();
    m_pagePopup = nullptr;
  }
  enablePopupMouseWheelEventListener();
  return m_pagePopup.get();
}

void WebViewImpl::closePagePopup(PagePopup* popup) {
  DCHECK(popup);
  WebPagePopupImpl* popupImpl = toWebPagePopupImpl(popup);
  DCHECK_EQ(m_pagePopup.get(), popupImpl);
  if (m_pagePopup.get() != popupImpl)
    return;
  m_pagePopup->closePopup();
}

void WebViewImpl::cleanupPagePopup() {
  m_pagePopup = nullptr;
  disablePopupMouseWheelEventListener();
}

void WebViewImpl::cancelPagePopup() {
  if (m_pagePopup)
    m_pagePopup->cancel();
}

void WebViewImpl::enablePopupMouseWheelEventListener() {
  // TODO(kenrb): Popup coordination for out-of-process iframes needs to be
  // added. Because of the early return here a select element
  // popup can remain visible even when the element underneath it is
  // scrolled to a new position. This is part of a larger set of issues with
  // popups.
  // See https://crbug.com/566130
  if (!mainFrameImpl())
    return;
  DCHECK(!m_popupMouseWheelEventListener);
  Document* document = mainFrameImpl()->frame()->document();
  DCHECK(document);
  // We register an empty event listener, EmptyEventListener, so that mouse
  // wheel events get sent to the WebView.
  m_popupMouseWheelEventListener = EmptyEventListener::create();
  document->addEventListener(EventTypeNames::mousewheel,
                             m_popupMouseWheelEventListener, false);
}

void WebViewImpl::disablePopupMouseWheelEventListener() {
  // TODO(kenrb): Concerns the same as in enablePopupMouseWheelEventListener.
  // See https://crbug.com/566130
  if (!mainFrameImpl())
    return;
  DCHECK(m_popupMouseWheelEventListener);
  Document* document = mainFrameImpl()->frame()->document();
  DCHECK(document);
  // Document may have already removed the event listener, for instance, due
  // to a navigation, but remove it anyway.
  document->removeEventListener(EventTypeNames::mousewheel,
                                m_popupMouseWheelEventListener.release(),
                                false);
}

LocalDOMWindow* WebViewImpl::pagePopupWindow() const {
  return m_pagePopup ? m_pagePopup->window() : nullptr;
}

Frame* WebViewImpl::focusedCoreFrame() const {
  return m_page ? m_page->focusController().focusedOrMainFrame() : nullptr;
}

WebViewImpl* WebViewImpl::fromPage(Page* page) {
  return page ? static_cast<WebViewImpl*>(page->chromeClient().webView())
              : nullptr;
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close() {
  DCHECK(allInstances().contains(this));
  allInstances().erase(this);

  if (m_page) {
    // Initiate shutdown for the entire frameset.  This will cause a lot of
    // notifications to be sent.
    m_page->willBeDestroyed();
    m_page.clear();
  }

  // Reset the delegate to prevent notifications being sent as we're being
  // deleted.
  m_client = nullptr;

  deref();  // Balances ref() acquired in WebView::create
}

WebSize WebViewImpl::size() {
  return m_size;
}

void WebViewImpl::resizeVisualViewport(const WebSize& newSize) {
  page()->frameHost().visualViewport().setSize(newSize);
  page()->frameHost().visualViewport().clampToBoundaries();
}

void WebViewImpl::performResize() {
  // We'll keep the initial containing block size from changing when the top
  // controls hide so that the ICB will always be the same size as the
  // viewport with the browser controls shown.
  IntSize ICBSize = m_size;
  if (RuntimeEnabledFeatures::inertTopControlsEnabled() &&
      browserControls().permittedState() == WebBrowserControlsBoth &&
      !browserControls().shrinkViewport())
    ICBSize.expand(0, -browserControls().height());

  pageScaleConstraintsSet().didChangeInitialContainingBlockSize(ICBSize);

  updatePageDefinedViewportConstraints(
      mainFrameImpl()->frame()->document()->viewportDescription());
  updateMainFrameLayoutSize();

  page()->frameHost().visualViewport().setSize(m_size);

  if (mainFrameImpl()->frameView()) {
    mainFrameImpl()->frameView()->setInitialViewportSize(ICBSize);
    if (!mainFrameImpl()->frameView()->needsLayout())
      postLayoutResize(mainFrameImpl());
  }
}

void WebViewImpl::updateBrowserControlsState(WebBrowserControlsState constraint,
                                             WebBrowserControlsState current,
                                             bool animate) {
  WebBrowserControlsState oldPermittedState =
      browserControls().permittedState();

  browserControls().updateConstraintsAndState(constraint, current, animate);

  // If the controls are going from a locked hidden to unlocked state, or vice
  // versa, the ICB size needs to change but we can't rely on getting a
  // WebViewImpl::resize since the top controls shown state may not have
  // changed.
  if ((oldPermittedState == WebBrowserControlsHidden &&
       constraint == WebBrowserControlsBoth) ||
      (oldPermittedState == WebBrowserControlsBoth &&
       constraint == WebBrowserControlsHidden)) {
    performResize();
  }

  if (m_layerTreeView)
    m_layerTreeView->updateBrowserControlsState(constraint, current, animate);
}

void WebViewImpl::didUpdateBrowserControls() {
  if (m_layerTreeView) {
    m_layerTreeView->setBrowserControlsShownRatio(
        browserControls().shownRatio());
    m_layerTreeView->setBrowserControlsHeight(
        browserControls().height(), browserControls().shrinkViewport());
  }

  WebLocalFrameImpl* mainFrame = mainFrameImpl();
  if (!mainFrame)
    return;

  FrameView* view = mainFrame->frameView();
  if (!view)
    return;

  VisualViewport& visualViewport = page()->frameHost().visualViewport();

  {
    // This object will save the current visual viewport offset w.r.t. the
    // document and restore it when the object goes out of scope. It's
    // needed since the browser controls adjustment will change the maximum
    // scroll offset and we may need to reposition them to keep the user's
    // apparent position unchanged.
    ResizeViewportAnchor::ResizeScope resizeScope(*m_resizeViewportAnchor);

    float browserControlsViewportAdjustment =
        browserControls().layoutHeight() - browserControls().contentOffset();
    visualViewport.setBrowserControlsAdjustment(
        browserControlsViewportAdjustment);
  }
}

BrowserControls& WebViewImpl::browserControls() {
  return page()->frameHost().browserControls();
}

void WebViewImpl::resizeViewWhileAnchored(float browserControlsHeight,
                                          bool browserControlsShrinkLayout) {
  DCHECK(mainFrameImpl());

  browserControls().setHeight(browserControlsHeight,
                              browserControlsShrinkLayout);

  {
    // Avoids unnecessary invalidations while various bits of state in
    // TextAutosizer are updated.
    TextAutosizer::DeferUpdatePageInfo deferUpdatePageInfo(page());
    performResize();
  }

  m_fullscreenController->updateSize();

  // Update lifecyle phases immediately to recalculate the minimum scale limit
  // for rotation anchoring, and to make sure that no lifecycle states are
  // stale if this WebView is embedded in another one.
  updateAllLifecyclePhases();
}

void WebViewImpl::resizeWithBrowserControls(const WebSize& newSize,
                                            float browserControlsHeight,
                                            bool browserControlsShrinkLayout) {
  if (m_shouldAutoResize)
    return;

  if (m_size == newSize &&
      browserControls().height() == browserControlsHeight &&
      browserControls().shrinkViewport() == browserControlsShrinkLayout)
    return;

  if (page()->mainFrame() && !page()->mainFrame()->isLocalFrame()) {
    // Viewport resize for a remote main frame does not require any
    // particular action, but the state needs to reflect the correct size
    // so that it can be used for initalization if the main frame gets
    // swapped to a LocalFrame at a later time.
    m_size = newSize;
    pageScaleConstraintsSet().didChangeInitialContainingBlockSize(m_size);
    page()->frameHost().visualViewport().setSize(m_size);
    return;
  }

  WebLocalFrameImpl* mainFrame = mainFrameImpl();
  if (!mainFrame)
    return;

  FrameView* view = mainFrame->frameView();
  if (!view)
    return;

  VisualViewport& visualViewport = page()->frameHost().visualViewport();

  bool isRotation =
      page()->settings().getMainFrameResizesAreOrientationChanges() &&
      m_size.width && contentsSize().width() && newSize.width != m_size.width &&
      !m_fullscreenController->isFullscreenOrTransitioning();
  m_size = newSize;

  FloatSize viewportAnchorCoords(viewportAnchorCoordX, viewportAnchorCoordY);
  if (isRotation) {
    RotationViewportAnchor anchor(*view, visualViewport, viewportAnchorCoords,
                                  pageScaleConstraintsSet());
    resizeViewWhileAnchored(browserControlsHeight, browserControlsShrinkLayout);
  } else {
    ResizeViewportAnchor::ResizeScope resizeScope(*m_resizeViewportAnchor);
    resizeViewWhileAnchored(browserControlsHeight, browserControlsShrinkLayout);
  }
  sendResizeEventAndRepaint();
}

void WebViewImpl::resize(const WebSize& newSize) {
  if (m_shouldAutoResize || m_size == newSize)
    return;

  resizeWithBrowserControls(newSize, browserControls().height(),
                            browserControls().shrinkViewport());
}

void WebViewImpl::didEnterFullscreen() {
  m_fullscreenController->didEnterFullscreen();
}

void WebViewImpl::didExitFullscreen() {
  m_fullscreenController->didExitFullscreen();
}

void WebViewImpl::didUpdateFullscreenSize() {
  m_fullscreenController->updateSize();
}

void WebViewImpl::setSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppressFrameRequests) {
  m_page->animator().setSuppressFrameRequestsWorkaroundFor704763Only(
      suppressFrameRequests);
}
void WebViewImpl::beginFrame(double lastFrameTimeMonotonic) {
  TRACE_EVENT1("blink", "WebViewImpl::beginFrame", "frameTime",
               lastFrameTimeMonotonic);
  DCHECK(lastFrameTimeMonotonic);

  // Create synthetic wheel events as necessary for fling.
  if (m_gestureAnimation) {
    if (m_gestureAnimation->animate(lastFrameTimeMonotonic))
      mainFrameImpl()->frameWidget()->scheduleAnimation();
    else {
      DCHECK_NE(m_flingSourceDevice, WebGestureDeviceUninitialized);
      WebGestureDevice lastFlingSourceDevice = m_flingSourceDevice;
      endActiveFlingAnimation();

      WebGestureEvent endScrollEvent = createGestureScrollEventFromFling(
          WebInputEvent::GestureScrollEnd, lastFlingSourceDevice);
      mainFrameImpl()->frame()->eventHandler().handleGestureScrollEnd(
          endScrollEvent);
    }
  }

  if (!mainFrameImpl())
    return;

  m_lastFrameTimeMonotonic = lastFrameTimeMonotonic;

  DocumentLifecycle::AllowThrottlingScope throttlingScope(
      mainFrameImpl()->frame()->document()->lifecycle());
  PageWidgetDelegate::animate(*m_page, lastFrameTimeMonotonic);
}

void WebViewImpl::updateAllLifecyclePhases() {
  TRACE_EVENT0("blink", "WebViewImpl::updateAllLifecyclePhases");
  if (!mainFrameImpl())
    return;

  DocumentLifecycle::AllowThrottlingScope throttlingScope(
      mainFrameImpl()->frame()->document()->lifecycle());
  updateLayerTreeBackgroundColor();

  PageWidgetDelegate::updateAllLifecyclePhases(*m_page,
                                               *mainFrameImpl()->frame());

  if (InspectorOverlay* overlay = inspectorOverlay()) {
    overlay->updateAllLifecyclePhases();
    // TODO(chrishtr): integrate paint into the overlay's lifecycle.
    if (overlay->pageOverlay() && overlay->pageOverlay()->graphicsLayer())
      overlay->pageOverlay()->graphicsLayer()->paint(nullptr);
  }
  if (m_pageColorOverlay)
    m_pageColorOverlay->graphicsLayer()->paint(nullptr);

  // TODO(chrishtr): link highlights don't currently paint themselves, it's
  // still driven by cc.  Fix this.
  for (size_t i = 0; i < m_linkHighlights.size(); ++i)
    m_linkHighlights[i]->updateGeometry();

  if (FrameView* view = mainFrameImpl()->frameView()) {
    LocalFrame* frame = mainFrameImpl()->frame();
    WebWidgetClient* client =
        WebLocalFrameImpl::fromFrame(frame)->frameWidget()->client();

    if (m_shouldDispatchFirstVisuallyNonEmptyLayout &&
        view->isVisuallyNonEmpty()) {
      m_shouldDispatchFirstVisuallyNonEmptyLayout = false;
      // TODO(esprehn): Move users of this callback to something
      // better, the heuristic for "visually non-empty" is bad.
      client->didMeaningfulLayout(WebMeaningfulLayout::VisuallyNonEmpty);
    }

    if (m_shouldDispatchFirstLayoutAfterFinishedParsing &&
        frame->document()->hasFinishedParsing()) {
      m_shouldDispatchFirstLayoutAfterFinishedParsing = false;
      client->didMeaningfulLayout(WebMeaningfulLayout::FinishedParsing);
    }

    if (m_shouldDispatchFirstLayoutAfterFinishedLoading &&
        frame->document()->isLoadCompleted()) {
      m_shouldDispatchFirstLayoutAfterFinishedLoading = false;
      client->didMeaningfulLayout(WebMeaningfulLayout::FinishedLoading);
    }
  }
}

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect) {
  // This should only be used when compositing is not being used for this
  // WebView, and it is painting into the recording of its parent.
  DCHECK(!isAcceleratedCompositingActive());

  double paintStart = currentTime();
  PageWidgetDelegate::paint(*m_page, canvas, rect,
                            *m_page->deprecatedLocalMainFrame());
  double paintEnd = currentTime();
  double pixelsPerSec = (rect.width * rect.height) / (paintEnd - paintStart);
  DEFINE_STATIC_LOCAL(CustomCountHistogram, softwarePaintDurationHistogram,
                      ("Renderer4.SoftwarePaintDurationMS", 0, 120, 30));
  softwarePaintDurationHistogram.count((paintEnd - paintStart) * 1000);
  DEFINE_STATIC_LOCAL(CustomCountHistogram, softwarePaintRateHistogram,
                      ("Renderer4.SoftwarePaintMegapixPerSecond", 10, 210, 30));
  softwarePaintRateHistogram.count(pixelsPerSec / 1000000);
}

#if OS(ANDROID)
void WebViewImpl::paintIgnoringCompositing(WebCanvas* canvas,
                                           const WebRect& rect) {
  // This is called on a composited WebViewImpl, but we will ignore it,
  // producing all possible content of the WebViewImpl into the WebCanvas.
  DCHECK(isAcceleratedCompositingActive());
  PageWidgetDelegate::paintIgnoringCompositing(
      *m_page, canvas, rect, *m_page->deprecatedLocalMainFrame());
}
#endif

void WebViewImpl::layoutAndPaintAsync(
    WebLayoutAndPaintAsyncCallback* callback) {
  m_layerTreeView->layoutAndPaintAsync(callback);
}

void WebViewImpl::compositeAndReadbackAsync(
    WebCompositeAndReadbackAsyncCallback* callback) {
  m_layerTreeView->compositeAndReadbackAsync(callback);
}

void WebViewImpl::themeChanged() {
  if (!page())
    return;
  if (!page()->mainFrame()->isLocalFrame())
    return;
  FrameView* view = page()->deprecatedLocalMainFrame()->view();

  WebRect damagedRect(0, 0, m_size.width, m_size.height);
  view->invalidateRect(damagedRect);
}

void WebViewImpl::enterFullscreen(LocalFrame& frame) {
  m_fullscreenController->enterFullscreen(frame);
}

void WebViewImpl::exitFullscreen(LocalFrame& frame) {
  m_fullscreenController->exitFullscreen(frame);
}

void WebViewImpl::fullscreenElementChanged(Element* fromElement,
                                           Element* toElement) {
  m_fullscreenController->fullscreenElementChanged(fromElement, toElement);
}

bool WebViewImpl::hasHorizontalScrollbar() {
  return mainFrameImpl()
      ->frameView()
      ->layoutViewportScrollableArea()
      ->horizontalScrollbar();
}

bool WebViewImpl::hasVerticalScrollbar() {
  return mainFrameImpl()
      ->frameView()
      ->layoutViewportScrollableArea()
      ->verticalScrollbar();
}

const WebInputEvent* WebViewImpl::m_currentInputEvent = nullptr;

WebInputEventResult WebViewImpl::handleInputEvent(
    const WebCoalescedInputEvent& coalescedEvent) {
  const WebInputEvent& inputEvent = coalescedEvent.event();
  // TODO(dcheng): The fact that this is getting called when there is no local
  // main frame is problematic and probably indicates a bug in the input event
  // routing code.
  if (!mainFrameImpl())
    return WebInputEventResult::NotHandled;

  WebAutofillClient* autofillClient = mainFrameImpl()->autofillClient();
  UserGestureNotifier notifier(this);
  // On the first input event since page load, |notifier| instructs the
  // autofill client to unblock values of password input fields of any forms
  // on the page. There is a single input event, GestureTap, which can both
  // be the first event after page load, and cause a form submission. In that
  // case, the form submission happens before the autofill client is told
  // to unblock the password values, and so the password values are not
  // submitted. To avoid that, GestureTap is handled explicitly:
  if (inputEvent.type() == WebInputEvent::GestureTap && autofillClient) {
    m_userGestureObserved = true;
    autofillClient->firstUserGestureObserved();
  }

  page()->frameHost().visualViewport().startTrackingPinchStats();

  TRACE_EVENT1("input,rail", "WebViewImpl::handleInputEvent", "type",
               WebInputEvent::GetName(inputEvent.type()));

  // If a drag-and-drop operation is in progress, ignore input events.
  if (mainFrameImpl()->frameWidget()->doingDragAndDrop())
    return WebInputEventResult::HandledSuppressed;

  if (m_devToolsEmulator->handleInputEvent(inputEvent))
    return WebInputEventResult::HandledSuppressed;

  if (InspectorOverlay* overlay = inspectorOverlay()) {
    if (overlay->handleInputEvent(inputEvent))
      return WebInputEventResult::HandledSuppressed;
  }

  // Report the event to be NOT processed by WebKit, so that the browser can
  // handle it appropriately.
  if (WebFrameWidgetBase::ignoreInputEvents())
    return WebInputEventResult::NotHandled;

  AutoReset<const WebInputEvent*> currentEventChange(&m_currentInputEvent,
                                                     &inputEvent);
  UIEventWithKeyState::clearNewTabModifierSetFromIsolatedWorld();

  bool isPointerLocked = false;
  if (WebFrameWidgetBase* widget = mainFrameImpl()->frameWidget()) {
    if (WebWidgetClient* client = widget->client())
      isPointerLocked = client->isPointerLocked();
  }

  if (isPointerLocked && WebInputEvent::isMouseEventType(inputEvent.type())) {
    mainFrameImpl()->frameWidget()->pointerLockMouseEvent(inputEvent);
    return WebInputEventResult::HandledSystem;
  }

  if (m_mouseCaptureNode &&
      WebInputEvent::isMouseEventType(inputEvent.type())) {
    TRACE_EVENT1("input", "captured mouse event", "type", inputEvent.type());
    // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
    Node* node = m_mouseCaptureNode;

    // Not all platforms call mouseCaptureLost() directly.
    if (inputEvent.type() == WebInputEvent::MouseUp)
      mouseCaptureLost();

    std::unique_ptr<UserGestureIndicator> gestureIndicator;

    AtomicString eventType;
    switch (inputEvent.type()) {
      case WebInputEvent::MouseMove:
        eventType = EventTypeNames::mousemove;
        break;
      case WebInputEvent::MouseLeave:
        eventType = EventTypeNames::mouseout;
        break;
      case WebInputEvent::MouseDown:
        eventType = EventTypeNames::mousedown;
        gestureIndicator = WTF::wrapUnique(
            new UserGestureIndicator(DocumentUserGestureToken::create(
                &node->document(), UserGestureToken::NewGesture)));
        m_mouseCaptureGestureToken = gestureIndicator->currentToken();
        break;
      case WebInputEvent::MouseUp:
        eventType = EventTypeNames::mouseup;
        gestureIndicator = WTF::wrapUnique(
            new UserGestureIndicator(m_mouseCaptureGestureToken.release()));
        break;
      default:
        NOTREACHED();
    }

    WebMouseEvent transformedEvent =
        TransformWebMouseEvent(mainFrameImpl()->frameView(),
                               static_cast<const WebMouseEvent&>(inputEvent));
    node->dispatchMouseEvent(transformedEvent, eventType,
                             transformedEvent.clickCount);
    return WebInputEventResult::HandledSystem;
  }

  // FIXME: This should take in the intended frame, not the local frame root.
  WebInputEventResult result = PageWidgetDelegate::handleInputEvent(
      *this, coalescedEvent, mainFrameImpl()->frame());
  if (result != WebInputEventResult::NotHandled)
    return result;

  // Unhandled pinch events should adjust the scale.
  if (inputEvent.type() == WebInputEvent::GesturePinchUpdate) {
    const WebGestureEvent& pinchEvent =
        static_cast<const WebGestureEvent&>(inputEvent);

    // For touchpad gestures synthesize a Windows-like wheel event
    // to send to any handlers that may exist. Not necessary for touchscreen
    // as touch events would have already been sent for the gesture.
    if (pinchEvent.sourceDevice == WebGestureDeviceTouchpad) {
      result = handleSyntheticWheelFromTouchpadPinchEvent(pinchEvent);
      if (result != WebInputEventResult::NotHandled)
        return result;
    }

    if (pinchEvent.data.pinchUpdate.zoomDisabled)
      return WebInputEventResult::NotHandled;

    if (page()->frameHost().visualViewport().magnifyScaleAroundAnchor(
            pinchEvent.data.pinchUpdate.scale,
            FloatPoint(pinchEvent.x, pinchEvent.y)))
      return WebInputEventResult::HandledSystem;
  }

  return WebInputEventResult::NotHandled;
}

void WebViewImpl::setCursorVisibilityState(bool isVisible) {
  if (m_page)
    m_page->setIsCursorVisible(isVisible);
}

void WebViewImpl::mouseCaptureLost() {
  TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
  m_mouseCaptureNode = nullptr;
}

void WebViewImpl::setFocus(bool enable) {
  m_page->focusController().setFocused(enable);
  if (enable) {
    m_page->focusController().setActive(true);
    LocalFrame* focusedFrame = m_page->focusController().focusedFrame();
    if (focusedFrame) {
      Element* element = focusedFrame->document()->focusedElement();
      if (element &&
          focusedFrame->selection()
              .computeVisibleSelectionInDOMTreeDeprecated()
              .isNone()) {
        // If the selection was cleared while the WebView was not
        // focused, then the focus element shows with a focus ring but
        // no caret and does respond to keyboard inputs.
        focusedFrame->document()->updateStyleAndLayoutTree();
        if (element->isTextControl()) {
          element->updateFocusAppearance(SelectionBehaviorOnFocus::Restore);
        } else if (hasEditableStyle(*element)) {
          // updateFocusAppearance() selects all the text of
          // contentseditable DIVs. So we set the selection explicitly
          // instead. Note that this has the side effect of moving the
          // caret back to the beginning of the text.
          Position position(element, 0);
          focusedFrame->selection().setSelection(
              SelectionInDOMTree::Builder().collapse(position).build());
        }
      }
    }
    m_imeAcceptEvents = true;
  } else {
    hidePopups();

    // Clear focus on the currently focused frame if any.
    if (!m_page)
      return;

    LocalFrame* frame =
        m_page->mainFrame() && m_page->mainFrame()->isLocalFrame()
            ? m_page->deprecatedLocalMainFrame()
            : nullptr;
    if (!frame)
      return;

    LocalFrame* focusedFrame = focusedLocalFrameInWidget();
    if (focusedFrame) {
      // Finish an ongoing composition to delete the composition node.
      if (focusedFrame->inputMethodController().hasComposition()) {
        // TODO(xiaochengh): The use of
        // updateStyleAndLayoutIgnorePendingStylesheets
        // needs to be audited.  See http://crbug.com/590369 for more details.
        focusedFrame->document()
            ->updateStyleAndLayoutIgnorePendingStylesheets();

        focusedFrame->inputMethodController().finishComposingText(
            InputMethodController::KeepSelection);
      }
      m_imeAcceptEvents = false;
    }
  }
}

// TODO(ekaramad):This method is almost duplicated in WebFrameWidgetImpl as
// well. This code needs to be refactored  (http://crbug.com/629721).
WebRange WebViewImpl::compositionRange() {
  LocalFrame* focused = focusedLocalFrameAvailableForIme();
  if (!focused)
    return WebRange();

  const EphemeralRange range =
      focused->inputMethodController().compositionEphemeralRange();
  if (range.isNull())
    return WebRange();

  Element* editable =
      focused->selection().rootEditableElementOrDocumentElement();
  DCHECK(editable);

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  editable->document().updateStyleAndLayoutIgnorePendingStylesheets();

  return PlainTextRange::create(*editable, range);
}

// TODO(ekaramad):This method is almost duplicated in WebFrameWidgetImpl as
// well. This code needs to be refactored  (http://crbug.com/629721).
bool WebViewImpl::selectionBounds(WebRect& anchor, WebRect& focus) const {
  const Frame* frame = focusedCoreFrame();
  if (!frame || !frame->isLocalFrame())
    return false;

  const LocalFrame* localFrame = toLocalFrame(frame);
  if (!localFrame)
    return false;
  FrameSelection& selection = localFrame->selection();
  if (!selection.isAvailable() ||
      selection.computeVisibleSelectionInDOMTreeDeprecated().isNone()) {
    // plugins/mouse-capture-inside-shadow.html reaches here.
    return false;
  }

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  localFrame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      localFrame->document()->lifecycle());

  if (selection.computeVisibleSelectionInDOMTreeDeprecated().isCaret()) {
    anchor = focus = selection.absoluteCaretBounds();
  } else {
    const EphemeralRange selectedRange =
        selection.computeVisibleSelectionInDOMTreeDeprecated()
            .toNormalizedEphemeralRange();
    if (selectedRange.isNull())
      return false;
    anchor = localFrame->editor().firstRectForRange(
        EphemeralRange(selectedRange.startPosition()));
    focus = localFrame->editor().firstRectForRange(
        EphemeralRange(selectedRange.endPosition()));
  }

  anchor = localFrame->view()->contentsToViewport(anchor);
  focus = localFrame->view()->contentsToViewport(focus);

  if (!selection.computeVisibleSelectionInDOMTreeDeprecated().isBaseFirst())
    std::swap(anchor, focus);
  return true;
}

// TODO(ekaramad):This method is almost duplicated in WebFrameWidgetImpl as
// well. This code needs to be refactored  (http://crbug.com/629721).
WebPlugin* WebViewImpl::focusedPluginIfInputMethodSupported(LocalFrame* frame) {
  WebPluginContainerImpl* container =
      WebLocalFrameImpl::currentPluginContainer(frame);
  if (container && container->supportsInputMethod())
    return container->plugin();
  return nullptr;
}

// TODO(ekaramad):This method is almost duplicated in WebFrameWidgetImpl as
// well. This code needs to be refactored  (http://crbug.com/629721).
bool WebViewImpl::selectionTextDirection(WebTextDirection& start,
                                         WebTextDirection& end) const {
  const LocalFrame* frame = focusedLocalFrameInWidget();
  if (!frame)
    return false;

  const FrameSelection& selection = frame->selection();
  if (!selection.isAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here.
    return false;
  }

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  if (selection.computeVisibleSelectionInDOMTree()
          .toNormalizedEphemeralRange()
          .isNull())
    return false;
  start = toWebTextDirection(
      primaryDirectionOf(*selection.computeVisibleSelectionInDOMTreeDeprecated()
                              .start()
                              .anchorNode()));
  end = toWebTextDirection(
      primaryDirectionOf(*selection.computeVisibleSelectionInDOMTreeDeprecated()
                              .end()
                              .anchorNode()));
  return true;
}

// TODO(ekaramad):This method is almost duplicated in WebFrameWidgetImpl as
// well. This code needs to be refactored  (http://crbug.com/629721).
bool WebViewImpl::isSelectionAnchorFirst() const {
  const LocalFrame* frame = focusedLocalFrameInWidget();
  if (!frame)
    return false;

  FrameSelection& selection = frame->selection();
  if (!selection.isAvailable()) {
    // plugins/mouse-capture-inside-shadow.html reaches here.
    return false;
  }
  return selection.computeVisibleSelectionInDOMTreeDeprecated().isBaseFirst();
}

WebColor WebViewImpl::backgroundColor() const {
  if (isTransparent())
    return Color::transparent;
  if (!m_page)
    return baseBackgroundColor().rgb();
  if (!m_page->mainFrame())
    return baseBackgroundColor().rgb();
  if (!m_page->mainFrame()->isLocalFrame())
    return baseBackgroundColor().rgb();
  FrameView* view = m_page->deprecatedLocalMainFrame()->view();
  return view->documentBackgroundColor().rgb();
}

WebPagePopup* WebViewImpl::pagePopup() const {
  return m_pagePopup.get();
}

// TODO(ekaramad):This method is almost duplicated in WebFrameWidgetImpl as
// well. This code needs to be refactored  (http://crbug.com/629721).
WebRange WebViewImpl::caretOrSelectionRange() {
  const LocalFrame* focused = focusedLocalFrameInWidget();
  if (!focused)
    return WebRange();

  // TODO(xiaochengh): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  focused->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  return focused->inputMethodController().getSelectionOffsets();
}

// TODO(ekaramad):This method is almost duplicated in WebFrameWidgetImpl as
// well. This code needs to be refactored  (http://crbug.com/629721).
void WebViewImpl::setTextDirection(WebTextDirection direction) {
  // The Editor::setBaseWritingDirection() function checks if we can change
  // the text direction of the selected node and updates its DOM "dir"
  // attribute and its CSS "direction" property.
  // So, we just call the function as Safari does.
  const LocalFrame* focused = focusedLocalFrameInWidget();
  if (!focused)
    return;

  Editor& editor = focused->editor();
  if (!editor.canEdit())
    return;

  switch (direction) {
    case WebTextDirectionDefault:
      editor.setBaseWritingDirection(NaturalWritingDirection);
      break;

    case WebTextDirectionLeftToRight:
      editor.setBaseWritingDirection(LeftToRightWritingDirection);
      break;

    case WebTextDirectionRightToLeft:
      editor.setBaseWritingDirection(RightToLeftWritingDirection);
      break;

    default:
      NOTIMPLEMENTED();
      break;
  }
}

bool WebViewImpl::isAcceleratedCompositingActive() const {
  return m_rootLayer;
}

void WebViewImpl::willCloseLayerTreeView() {
  if (m_linkHighlightsTimeline) {
    m_linkHighlights.clear();
    detachCompositorAnimationTimeline(m_linkHighlightsTimeline.get());
    m_linkHighlightsTimeline.reset();
  }

  if (m_layerTreeView)
    page()->willCloseLayerTreeView(*m_layerTreeView, nullptr);

  setRootLayer(nullptr);
  m_animationHost = nullptr;

  m_mutator = nullptr;
  m_layerTreeView = nullptr;
}

void WebViewImpl::didAcquirePointerLock() {
  mainFrameImpl()->frameWidget()->didAcquirePointerLock();
}

void WebViewImpl::didNotAcquirePointerLock() {
  mainFrameImpl()->frameWidget()->didNotAcquirePointerLock();
}

void WebViewImpl::didLosePointerLock() {
  mainFrameImpl()->frameWidget()->didLosePointerLock();
}

// TODO(ekaramad):This method is almost duplicated in WebFrameWidgetImpl as
// well. This code needs to be refactored  (http://crbug.com/629721).
bool WebViewImpl::getCompositionCharacterBounds(WebVector<WebRect>& bounds) {
  WebRange range = compositionRange();
  if (range.isEmpty())
    return false;

  WebLocalFrame* frame = focusedFrame();

  // Only consider frames whose local root is the main frame. For other
  // local frames which have different local roots, the corresponding
  // WebFrameWidget will handle this task.
  if (frame->localRoot() != mainFrameImpl())
    return false;

  size_t characterCount = range.length();
  size_t offset = range.startOffset();
  WebVector<WebRect> result(characterCount);
  WebRect webrect;
  for (size_t i = 0; i < characterCount; ++i) {
    if (!frame->firstRectForCharacterRange(offset + i, 1, webrect)) {
      DLOG(ERROR) << "Could not retrieve character rectangle at " << i;
      return false;
    }
    result[i] = webrect;
  }
  bounds.swap(result);
  return true;
}

// WebView --------------------------------------------------------------------

WebSettingsImpl* WebViewImpl::settingsImpl() {
  if (!m_webSettings)
    m_webSettings = WTF::wrapUnique(
        new WebSettingsImpl(&m_page->settings(), m_devToolsEmulator.get()));
  DCHECK(m_webSettings);
  return m_webSettings.get();
}

WebSettings* WebViewImpl::settings() {
  return settingsImpl();
}

WebString WebViewImpl::pageEncoding() const {
  if (!m_page)
    return WebString();

  if (!m_page->mainFrame()->isLocalFrame())
    return WebString();

  // FIXME: Is this check needed?
  if (!m_page->deprecatedLocalMainFrame()->document()->loader())
    return WebString();

  return m_page->deprecatedLocalMainFrame()->document()->encodingName();
}

WebFrame* WebViewImpl::mainFrame() {
  return WebFrame::fromFrame(m_page ? m_page->mainFrame() : nullptr);
}

WebFrame* WebViewImpl::findFrameByName(const WebString& name,
                                       WebFrame* relativeToFrame) {
  // FIXME: Either this should only deal with WebLocalFrames or it should move
  // to WebFrame.
  if (!relativeToFrame)
    relativeToFrame = mainFrame();
  Frame* frame = toWebLocalFrameImpl(relativeToFrame)->frame();
  frame = frame->tree().find(name);
  if (!frame || !frame->isLocalFrame())
    return nullptr;
  return WebLocalFrameImpl::fromFrame(toLocalFrame(frame));
}

WebLocalFrame* WebViewImpl::focusedFrame() {
  Frame* frame = focusedCoreFrame();
  // TODO(yabinh): focusedCoreFrame() should always return a local frame, and
  // the following check should be unnecessary.
  // See crbug.com/625068
  if (!frame || !frame->isLocalFrame())
    return nullptr;
  return WebLocalFrameImpl::fromFrame(toLocalFrame(frame));
}

void WebViewImpl::setFocusedFrame(WebFrame* frame) {
  if (!frame) {
    // Clears the focused frame if any.
    Frame* focusedFrame = focusedCoreFrame();
    if (focusedFrame && focusedFrame->isLocalFrame())
      toLocalFrame(focusedFrame)->selection().setFocused(false);
    return;
  }
  LocalFrame* coreFrame = toWebLocalFrameImpl(frame)->frame();
  coreFrame->page()->focusController().setFocusedFrame(coreFrame);
}

void WebViewImpl::focusDocumentView(WebFrame* frame) {
  // This is currently only used when replicating focus changes for
  // cross-process frames, and |notifyEmbedder| is disabled to avoid sending
  // duplicate frameFocused updates from FocusController to the browser
  // process, which already knows the latest focused frame.
  page()->focusController().focusDocumentView(frame->toImplBase()->frame(),
                                              false /* notifyEmbedder */);
}

void WebViewImpl::setInitialFocus(bool reverse) {
  if (!m_page)
    return;
  Frame* frame = page()->focusController().focusedOrMainFrame();
  if (frame->isLocalFrame()) {
    if (Document* document = toLocalFrame(frame)->document())
      document->clearFocusedElement();
  }
  page()->focusController().setInitialFocus(reverse ? WebFocusTypeBackward
                                                    : WebFocusTypeForward);
}

void WebViewImpl::clearFocusedElement() {
  Frame* frame = focusedCoreFrame();
  if (!frame || !frame->isLocalFrame())
    return;

  LocalFrame* localFrame = toLocalFrame(frame);

  Document* document = localFrame->document();
  if (!document)
    return;

  Element* oldFocusedElement = document->focusedElement();
  document->clearFocusedElement();
  if (!oldFocusedElement)
    return;

  // If a text field has focus, we need to make sure the selection controller
  // knows to remove selection from it. Otherwise, the text field is still
  // processing keyboard events even though focus has been moved to the page and
  // keystrokes get eaten as a result.
  document->updateStyleAndLayoutTree();
  if (hasEditableStyle(*oldFocusedElement) ||
      oldFocusedElement->isTextControl())
    localFrame->selection().clear();
}

// TODO(dglazkov): Remove and replace with Node:hasEditableStyle.
// http://crbug.com/612560
static bool isElementEditable(const Element* element) {
  element->document().updateStyleAndLayoutTree();
  if (hasEditableStyle(*element))
    return true;

  if (element->isTextControl()) {
    if (!toTextControlElement(element)->isDisabledOrReadOnly())
      return true;
  }

  return equalIgnoringASCIICase(element->getAttribute(HTMLNames::roleAttr),
                                "textbox");
}

bool WebViewImpl::scrollFocusedEditableElementIntoRect(
    const WebRect& rectInViewport) {
  LocalFrame* frame = page()->mainFrame() && page()->mainFrame()->isLocalFrame()
                          ? page()->deprecatedLocalMainFrame()
                          : nullptr;
  Element* element = focusedElement();
  if (!frame || !frame->view() || !element)
    return false;

  if (!isElementEditable(element))
    return false;

  element->document().updateStyleAndLayoutIgnorePendingStylesheets();

  bool zoomInToLegibleScale =
      m_webSettings->autoZoomFocusedNodeToLegibleScale() &&
      !page()->frameHost().visualViewport().shouldDisableDesktopWorkarounds();

  if (zoomInToLegibleScale) {
    // When deciding whether to zoom in on a focused text box, we should decide
    // not to zoom in if the user won't be able to zoom out. e.g if the textbox
    // is within a touch-action: none container the user can't zoom back out.
    TouchAction action = TouchActionUtil::computeEffectiveTouchAction(*element);
    if (!(action & TouchActionPinchZoom))
      zoomInToLegibleScale = false;
  }

  float scale;
  IntPoint scroll;
  bool needAnimation;
  computeScaleAndScrollForFocusedNode(element, zoomInToLegibleScale, scale,
                                      scroll, needAnimation);
  if (needAnimation)
    startPageScaleAnimation(scroll, false, scale,
                            scrollAndScaleAnimationDurationInSeconds);

  return true;
}

void WebViewImpl::smoothScroll(int targetX, int targetY, long durationMs) {
  IntPoint targetPosition(targetX, targetY);
  startPageScaleAnimation(targetPosition, false, pageScaleFactor(),
                          (double)durationMs / 1000);
}

void WebViewImpl::computeScaleAndScrollForFocusedNode(Node* focusedNode,
                                                      bool zoomInToLegibleScale,
                                                      float& newScale,
                                                      IntPoint& newScroll,
                                                      bool& needAnimation) {
  VisualViewport& visualViewport = page()->frameHost().visualViewport();

  WebRect caretInViewport, unusedEnd;
  selectionBounds(caretInViewport, unusedEnd);

  // 'caretInDocument' is rect encompassing the blinking cursor relative to the
  // root document.
  IntRect caretInDocument = mainFrameImpl()->frameView()->frameToContents(
      visualViewport.viewportToRootFrame(caretInViewport));
  IntRect textboxRectInDocument = mainFrameImpl()->frameView()->frameToContents(
      focusedNode->document().view()->contentsToRootFrame(
          pixelSnappedIntRect(focusedNode->Node::boundingBox())));

  if (!zoomInToLegibleScale) {
    newScale = pageScaleFactor();
  } else {
    // Pick a scale which is reasonably readable. This is the scale at which
    // the caret height will become minReadableCaretHeightForNode (adjusted
    // for dpi and font scale factor).
    const int minReadableCaretHeightForNode =
        textboxRectInDocument.height() >= 2 * caretInDocument.height()
            ? minReadableCaretHeightForTextArea
            : minReadableCaretHeight;
    newScale = clampPageScaleFactorToLimits(maximumLegiblePageScale() *
                                            minReadableCaretHeightForNode /
                                            caretInDocument.height());
    newScale = std::max(newScale, pageScaleFactor());
  }
  const float deltaScale = newScale / pageScaleFactor();

  needAnimation = false;

  // If we are at less than the target zoom level, zoom in.
  if (deltaScale > minScaleChangeToTriggerZoom)
    needAnimation = true;
  else
    newScale = pageScaleFactor();

  // If the caret is offscreen, then animate.
  if (!visualViewport.visibleRectInDocument().contains(caretInDocument))
    needAnimation = true;

  // If the box is partially offscreen and it's possible to bring it fully
  // onscreen, then animate.
  if (visualViewport.visibleRect().width() >= textboxRectInDocument.width() &&
      visualViewport.visibleRect().height() >= textboxRectInDocument.height() &&
      !visualViewport.visibleRectInDocument().contains(textboxRectInDocument))
    needAnimation = true;

  if (!needAnimation)
    return;

  FloatSize targetViewportSize(visualViewport.size());
  targetViewportSize.scale(1 / newScale);

  if (textboxRectInDocument.width() <= targetViewportSize.width()) {
    // Field is narrower than screen. Try to leave padding on left so field's
    // label is visible, but it's more important to ensure entire field is
    // onscreen.
    int idealLeftPadding = targetViewportSize.width() * leftBoxRatio;
    int maxLeftPaddingKeepingBoxOnscreen =
        targetViewportSize.width() - textboxRectInDocument.width();
    newScroll.setX(
        textboxRectInDocument.x() -
        std::min<int>(idealLeftPadding, maxLeftPaddingKeepingBoxOnscreen));
  } else {
    // Field is wider than screen. Try to left-align field, unless caret would
    // be offscreen, in which case right-align the caret.
    newScroll.setX(std::max<int>(textboxRectInDocument.x(),
                                 caretInDocument.x() + caretInDocument.width() +
                                     caretPadding -
                                     targetViewportSize.width()));
  }
  if (textboxRectInDocument.height() <= targetViewportSize.height()) {
    // Field is shorter than screen. Vertically center it.
    newScroll.setY(
        textboxRectInDocument.y() -
        (targetViewportSize.height() - textboxRectInDocument.height()) / 2);
  } else {
    // Field is taller than screen. Try to top align field, unless caret would
    // be offscreen, in which case bottom-align the caret.
    newScroll.setY(std::max<int>(textboxRectInDocument.y(),
                                 caretInDocument.y() +
                                     caretInDocument.height() + caretPadding -
                                     targetViewportSize.height()));
  }
}

void WebViewImpl::advanceFocus(bool reverse) {
  page()->focusController().advanceFocus(reverse ? WebFocusTypeBackward
                                                 : WebFocusTypeForward);
}

void WebViewImpl::advanceFocusAcrossFrames(WebFocusType type,
                                           WebRemoteFrame* from,
                                           WebLocalFrame* to) {
  // TODO(alexmos): Pass in proper with sourceCapabilities.
  page()->focusController().advanceFocusAcrossFrames(
      type, toWebRemoteFrameImpl(from)->frame(),
      toWebLocalFrameImpl(to)->frame());
}

double WebViewImpl::zoomLevel() {
  return m_zoomLevel;
}

void WebViewImpl::propagateZoomFactorToLocalFrameRoots(Frame* frame,
                                                       float zoomFactor) {
  if (frame->isLocalRoot()) {
    LocalFrame* localFrame = toLocalFrame(frame);
    if (!WebLocalFrameImpl::pluginContainerFromFrame(localFrame))
      localFrame->setPageZoomFactor(zoomFactor);
  }

  for (Frame* child = frame->tree().firstChild(); child;
       child = child->tree().nextSibling())
    propagateZoomFactorToLocalFrameRoots(child, zoomFactor);
}

double WebViewImpl::setZoomLevel(double zoomLevel) {
  if (zoomLevel < m_minimumZoomLevel)
    m_zoomLevel = m_minimumZoomLevel;
  else if (zoomLevel > m_maximumZoomLevel)
    m_zoomLevel = m_maximumZoomLevel;
  else
    m_zoomLevel = zoomLevel;

  float zoomFactor =
      m_zoomFactorOverride
          ? m_zoomFactorOverride
          : static_cast<float>(zoomLevelToZoomFactor(m_zoomLevel));
  if (m_zoomFactorForDeviceScaleFactor) {
    if (m_compositorDeviceScaleFactorOverride) {
      // Adjust the page's DSF so that DevicePixelRatio becomes
      // m_zoomFactorForDeviceScaleFactor.
      page()->setDeviceScaleFactorDeprecated(
          m_zoomFactorForDeviceScaleFactor /
          m_compositorDeviceScaleFactorOverride);
      zoomFactor *= m_compositorDeviceScaleFactorOverride;
    } else {
      page()->setDeviceScaleFactorDeprecated(1.f);
      zoomFactor *= m_zoomFactorForDeviceScaleFactor;
    }
  }
  propagateZoomFactorToLocalFrameRoots(m_page->mainFrame(), zoomFactor);

  return m_zoomLevel;
}

void WebViewImpl::zoomLimitsChanged(double minimumZoomLevel,
                                    double maximumZoomLevel) {
  m_minimumZoomLevel = minimumZoomLevel;
  m_maximumZoomLevel = maximumZoomLevel;
  m_client->zoomLimitsChanged(m_minimumZoomLevel, m_maximumZoomLevel);
}

float WebViewImpl::textZoomFactor() {
  return mainFrameImpl()->frame()->textZoomFactor();
}

float WebViewImpl::setTextZoomFactor(float textZoomFactor) {
  LocalFrame* frame = mainFrameImpl()->frame();
  if (WebLocalFrameImpl::pluginContainerFromFrame(frame))
    return 1;

  frame->setTextZoomFactor(textZoomFactor);

  return textZoomFactor;
}

double WebView::zoomLevelToZoomFactor(double zoomLevel) {
  return pow(textSizeMultiplierRatio, zoomLevel);
}

double WebView::zoomFactorToZoomLevel(double factor) {
  // Since factor = 1.2^level, level = log(factor) / log(1.2)
  return log(factor) / log(textSizeMultiplierRatio);
}

float WebViewImpl::pageScaleFactor() const {
  if (!page())
    return 1;

  return page()->frameHost().visualViewport().scale();
}

float WebViewImpl::clampPageScaleFactorToLimits(float scaleFactor) const {
  return pageScaleConstraintsSet().finalConstraints().clampToConstraints(
      scaleFactor);
}

void WebViewImpl::setVisualViewportOffset(const WebFloatPoint& offset) {
  DCHECK(page());
  page()->frameHost().visualViewport().setLocation(offset);
}

WebFloatPoint WebViewImpl::visualViewportOffset() const {
  DCHECK(page());
  return page()->frameHost().visualViewport().visibleRect().location();
}

WebFloatSize WebViewImpl::visualViewportSize() const {
  DCHECK(page());
  return page()->frameHost().visualViewport().visibleRect().size();
}

void WebViewImpl::scrollAndRescaleViewports(
    float scaleFactor,
    const IntPoint& mainFrameOrigin,
    const FloatPoint& visualViewportOrigin) {
  if (!page())
    return;

  if (!mainFrameImpl())
    return;

  FrameView* view = mainFrameImpl()->frameView();
  if (!view)
    return;

  // Order is important: visual viewport location is clamped based on
  // main frame scroll position and visual viewport scale.

  view->setScrollOffset(toScrollOffset(mainFrameOrigin), ProgrammaticScroll);

  setPageScaleFactor(scaleFactor);

  page()->frameHost().visualViewport().setLocation(visualViewportOrigin);
}

void WebViewImpl::setPageScaleFactorAndLocation(float scaleFactor,
                                                const FloatPoint& location) {
  DCHECK(page());

  page()->frameHost().visualViewport().setScaleAndLocation(
      clampPageScaleFactorToLimits(scaleFactor), location);
}

void WebViewImpl::setPageScaleFactor(float scaleFactor) {
  DCHECK(page());

  scaleFactor = clampPageScaleFactorToLimits(scaleFactor);
  if (scaleFactor == pageScaleFactor())
    return;

  page()->frameHost().visualViewport().setScale(scaleFactor);
}

void WebViewImpl::setDeviceScaleFactor(float scaleFactor) {
  if (!page())
    return;

  page()->setDeviceScaleFactorDeprecated(scaleFactor);

  if (m_layerTreeView)
    updateLayerTreeDeviceScaleFactor();
}

void WebViewImpl::setZoomFactorForDeviceScaleFactor(
    float zoomFactorForDeviceScaleFactor) {
  m_zoomFactorForDeviceScaleFactor = zoomFactorForDeviceScaleFactor;
  if (!m_layerTreeView)
    return;
  setZoomLevel(m_zoomLevel);
}

void WebViewImpl::setDeviceColorProfile(const gfx::ICCProfile& colorProfile) {
  ColorBehavior::setGlobalTargetColorProfile(colorProfile);
}

void WebViewImpl::enableAutoResizeMode(const WebSize& minSize,
                                       const WebSize& maxSize) {
  m_shouldAutoResize = true;
  m_minAutoSize = minSize;
  m_maxAutoSize = maxSize;
  configureAutoResizeMode();
}

void WebViewImpl::disableAutoResizeMode() {
  m_shouldAutoResize = false;
  configureAutoResizeMode();
}

void WebViewImpl::setDefaultPageScaleLimits(float minScale, float maxScale) {
  return page()->frameHost().setDefaultPageScaleLimits(minScale, maxScale);
}

void WebViewImpl::setInitialPageScaleOverride(
    float initialPageScaleFactorOverride) {
  PageScaleConstraints constraints =
      pageScaleConstraintsSet().userAgentConstraints();
  constraints.initialScale = initialPageScaleFactorOverride;

  if (constraints == pageScaleConstraintsSet().userAgentConstraints())
    return;

  pageScaleConstraintsSet().setNeedsReset(true);
  page()->frameHost().setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::setMaximumLegibleScale(float maximumLegibleScale) {
  m_maximumLegibleScale = maximumLegibleScale;
}

void WebViewImpl::setIgnoreViewportTagScaleLimits(bool ignore) {
  PageScaleConstraints constraints =
      pageScaleConstraintsSet().userAgentConstraints();
  if (ignore) {
    constraints.minimumScale =
        pageScaleConstraintsSet().defaultConstraints().minimumScale;
    constraints.maximumScale =
        pageScaleConstraintsSet().defaultConstraints().maximumScale;
  } else {
    constraints.minimumScale = -1;
    constraints.maximumScale = -1;
  }
  page()->frameHost().setUserAgentPageScaleConstraints(constraints);
}

IntSize WebViewImpl::mainFrameSize() {
  // The frame size should match the viewport size at minimum scale, since the
  // viewport must always be contained by the frame.
  FloatSize frameSize(m_size);
  frameSize.scale(1 / minimumPageScaleFactor());
  return expandedIntSize(frameSize);
}

PageScaleConstraintsSet& WebViewImpl::pageScaleConstraintsSet() const {
  return page()->frameHost().pageScaleConstraintsSet();
}

void WebViewImpl::refreshPageScaleFactorAfterLayout() {
  if (!mainFrame() || !page() || !page()->mainFrame() ||
      !page()->mainFrame()->isLocalFrame() ||
      !page()->deprecatedLocalMainFrame()->view())
    return;
  FrameView* view = page()->deprecatedLocalMainFrame()->view();

  updatePageDefinedViewportConstraints(
      mainFrameImpl()->frame()->document()->viewportDescription());
  pageScaleConstraintsSet().computeFinalConstraints();

  int verticalScrollbarWidth = 0;
  if (view->verticalScrollbar() &&
      !view->verticalScrollbar()->isOverlayScrollbar())
    verticalScrollbarWidth = view->verticalScrollbar()->width();
  pageScaleConstraintsSet().adjustFinalConstraintsToContentsSize(
      contentsSize(), verticalScrollbarWidth,
      settings()->shrinksViewportContentToFit());

  float newPageScaleFactor = pageScaleFactor();
  if (pageScaleConstraintsSet().needsReset() &&
      pageScaleConstraintsSet().finalConstraints().initialScale != -1) {
    newPageScaleFactor =
        pageScaleConstraintsSet().finalConstraints().initialScale;
    pageScaleConstraintsSet().setNeedsReset(false);
  }
  setPageScaleFactor(newPageScaleFactor);

  updateLayerTreeViewport();

  // Changes to page-scale during layout may require an additional frame.
  // We can't update the lifecycle here because we may be in the middle of
  // layout in the caller of this method.
  // TODO(chrishtr): clean all this up. All layout should happen in one
  // lifecycle run (crbug.com/578239).
  if (mainFrameImpl()->frameView()->needsLayout())
    mainFrameImpl()->frameWidget()->scheduleAnimation();
}

void WebViewImpl::updatePageDefinedViewportConstraints(
    const ViewportDescription& description) {
  if (!page() || (!m_size.width && !m_size.height) ||
      !page()->mainFrame()->isLocalFrame())
    return;

  if (!settings()->viewportEnabled()) {
    pageScaleConstraintsSet().clearPageDefinedConstraints();
    updateMainFrameLayoutSize();

    // If we don't support mobile viewports, allow GPU rasterization.
    m_matchesHeuristicsForGpuRasterization = true;
    if (m_layerTreeView)
      m_layerTreeView->heuristicsForGpuRasterizationUpdated(
          m_matchesHeuristicsForGpuRasterization);
    return;
  }

  Document* document = page()->deprecatedLocalMainFrame()->document();

  m_matchesHeuristicsForGpuRasterization =
      description.matchesHeuristicsForGpuRasterization();
  if (m_layerTreeView)
    m_layerTreeView->heuristicsForGpuRasterizationUpdated(
        m_matchesHeuristicsForGpuRasterization);

  Length defaultMinWidth = document->viewportDefaultMinWidth();
  if (defaultMinWidth.isAuto())
    defaultMinWidth = Length(ExtendToZoom);

  ViewportDescription adjustedDescription = description;
  if (settingsImpl()->viewportMetaLayoutSizeQuirk() &&
      adjustedDescription.type == ViewportDescription::ViewportMeta) {
    const int legacyWidthSnappingMagicNumber = 320;
    if (adjustedDescription.maxWidth.isFixed() &&
        adjustedDescription.maxWidth.value() <= legacyWidthSnappingMagicNumber)
      adjustedDescription.maxWidth = Length(DeviceWidth);
    if (adjustedDescription.maxHeight.isFixed() &&
        adjustedDescription.maxHeight.value() <= m_size.height)
      adjustedDescription.maxHeight = Length(DeviceHeight);
    adjustedDescription.minWidth = adjustedDescription.maxWidth;
    adjustedDescription.minHeight = adjustedDescription.maxHeight;
  }

  float oldInitialScale =
      pageScaleConstraintsSet().pageDefinedConstraints().initialScale;
  pageScaleConstraintsSet().updatePageDefinedConstraints(adjustedDescription,
                                                         defaultMinWidth);

  if (settingsImpl()->clobberUserAgentInitialScaleQuirk() &&
      pageScaleConstraintsSet().userAgentConstraints().initialScale != -1 &&
      pageScaleConstraintsSet().userAgentConstraints().initialScale *
              deviceScaleFactor() <=
          1) {
    if (description.maxWidth == Length(DeviceWidth) ||
        (description.maxWidth.type() == Auto &&
         pageScaleConstraintsSet().pageDefinedConstraints().initialScale ==
             1.0f))
      setInitialPageScaleOverride(-1);
  }

  Settings& pageSettings = page()->settings();
  pageScaleConstraintsSet().adjustForAndroidWebViewQuirks(
      adjustedDescription, defaultMinWidth.intValue(), deviceScaleFactor(),
      settingsImpl()->supportDeprecatedTargetDensityDPI(),
      pageSettings.getWideViewportQuirkEnabled(),
      pageSettings.getUseWideViewport(), pageSettings.getLoadWithOverviewMode(),
      settingsImpl()->viewportMetaNonUserScalableQuirk());
  float newInitialScale =
      pageScaleConstraintsSet().pageDefinedConstraints().initialScale;
  if (oldInitialScale != newInitialScale && newInitialScale != -1) {
    pageScaleConstraintsSet().setNeedsReset(true);
    if (mainFrameImpl() && mainFrameImpl()->frameView())
      mainFrameImpl()->frameView()->setNeedsLayout();
  }

  if (LocalFrame* frame = page()->deprecatedLocalMainFrame()) {
    if (TextAutosizer* textAutosizer = frame->document()->textAutosizer())
      textAutosizer->updatePageInfoInAllFrames();
  }

  updateMainFrameLayoutSize();
}

void WebViewImpl::updateMainFrameLayoutSize() {
  if (m_shouldAutoResize || !mainFrameImpl())
    return;

  FrameView* view = mainFrameImpl()->frameView();
  if (!view)
    return;

  WebSize layoutSize = m_size;

  if (settings()->viewportEnabled())
    layoutSize = pageScaleConstraintsSet().layoutSize();

  if (page()->settings().getForceZeroLayoutHeight())
    layoutSize.height = 0;

  view->setLayoutSize(layoutSize);
}

IntSize WebViewImpl::contentsSize() const {
  if (!page()->mainFrame()->isLocalFrame())
    return IntSize();
  LayoutViewItem root = page()->deprecatedLocalMainFrame()->contentLayoutItem();
  if (root.isNull())
    return IntSize();
  return root.documentRect().size();
}

WebSize WebViewImpl::contentsPreferredMinimumSize() {
  if (mainFrameImpl())
    mainFrameImpl()
        ->frame()
        ->view()
        ->updateLifecycleToCompositingCleanPlusScrolling();

  Document* document = m_page->mainFrame()->isLocalFrame()
                           ? m_page->deprecatedLocalMainFrame()->document()
                           : nullptr;
  if (!document || document->layoutViewItem().isNull() ||
      !document->documentElement() || !document->documentElement()->layoutBox())
    return WebSize();

  int widthScaled = document->layoutViewItem()
                        .minPreferredLogicalWidth()
                        .round();  // Already accounts for zoom.
  int heightScaled =
      document->documentElement()->layoutBox()->scrollHeight().round();
  return IntSize(widthScaled, heightScaled);
}

float WebViewImpl::defaultMinimumPageScaleFactor() const {
  return pageScaleConstraintsSet().defaultConstraints().minimumScale;
}

float WebViewImpl::defaultMaximumPageScaleFactor() const {
  return pageScaleConstraintsSet().defaultConstraints().maximumScale;
}

float WebViewImpl::minimumPageScaleFactor() const {
  return pageScaleConstraintsSet().finalConstraints().minimumScale;
}

float WebViewImpl::maximumPageScaleFactor() const {
  return pageScaleConstraintsSet().finalConstraints().maximumScale;
}

void WebViewImpl::resetScaleStateImmediately() {
  pageScaleConstraintsSet().setNeedsReset(true);
}

void WebViewImpl::resetScrollAndScaleState() {
  page()->frameHost().visualViewport().reset();

  if (!page()->mainFrame()->isLocalFrame())
    return;

  if (FrameView* frameView = toLocalFrame(page()->mainFrame())->view()) {
    ScrollableArea* scrollableArea = frameView->layoutViewportScrollableArea();

    if (!scrollableArea->getScrollOffset().isZero())
      scrollableArea->setScrollOffset(ScrollOffset(), ProgrammaticScroll);
  }

  pageScaleConstraintsSet().setNeedsReset(true);
}

void WebViewImpl::performMediaPlayerAction(const WebMediaPlayerAction& action,
                                           const WebPoint& location) {
  HitTestResult result = hitTestResultForViewportPos(location);
  Node* node = result.innerNode();
  if (!isHTMLVideoElement(*node) && !isHTMLAudioElement(*node))
    return;

  HTMLMediaElement* mediaElement = toHTMLMediaElement(node);
  switch (action.type) {
    case WebMediaPlayerAction::Play:
      if (action.enable)
        mediaElement->play();
      else
        mediaElement->pause();
      break;
    case WebMediaPlayerAction::Mute:
      mediaElement->setMuted(action.enable);
      break;
    case WebMediaPlayerAction::Loop:
      mediaElement->setLoop(action.enable);
      break;
    case WebMediaPlayerAction::Controls:
      mediaElement->setBooleanAttribute(HTMLNames::controlsAttr, action.enable);
      break;
    default:
      NOTREACHED();
  }
}

void WebViewImpl::performPluginAction(const WebPluginAction& action,
                                      const WebPoint& location) {
  // FIXME: Location is probably in viewport coordinates
  HitTestResult result = hitTestResultForRootFramePos(location);
  Node* node = result.innerNode();
  if (!isHTMLObjectElement(*node) && !isHTMLEmbedElement(*node))
    return;

  LayoutObject* object = node->layoutObject();
  if (object && object->isLayoutPart()) {
    FrameViewBase* frameViewWidget = toLayoutPart(object)->widget();
    if (frameViewWidget && frameViewWidget->isPluginContainer()) {
      WebPluginContainerImpl* plugin =
          toWebPluginContainerImpl(frameViewWidget);
      switch (action.type) {
        case WebPluginAction::Rotate90Clockwise:
          plugin->plugin()->rotateView(WebPlugin::RotationType90Clockwise);
          break;
        case WebPluginAction::Rotate90Counterclockwise:
          plugin->plugin()->rotateView(
              WebPlugin::RotationType90Counterclockwise);
          break;
        default:
          NOTREACHED();
      }
    }
  }
}

void WebViewImpl::audioStateChanged(bool isAudioPlaying) {
  m_scheduler->audioStateChanged(isAudioPlaying);
}

WebHitTestResult WebViewImpl::hitTestResultAt(const WebPoint& point) {
  return coreHitTestResultAt(point);
}

HitTestResult WebViewImpl::coreHitTestResultAt(
    const WebPoint& pointInViewport) {
  DocumentLifecycle::AllowThrottlingScope throttlingScope(
      mainFrameImpl()->frame()->document()->lifecycle());
  FrameView* view = mainFrameImpl()->frameView();
  IntPoint pointInRootFrame =
      view->contentsToFrame(view->viewportToContents(pointInViewport));
  return hitTestResultForRootFramePos(pointInRootFrame);
}

void WebViewImpl::spellingMarkerOffsetsForTest(WebVector<unsigned>* offsets) {
  Vector<unsigned> result;
  for (Frame* frame = m_page->mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    const DocumentMarkerVector& documentMarkers =
        toLocalFrame(frame)->document()->markers().markers();
    for (size_t i = 0; i < documentMarkers.size(); ++i)
      result.push_back(documentMarkers[i]->startOffset());
  }
  offsets->assign(result);
}

void WebViewImpl::removeSpellingMarkersUnderWords(
    const WebVector<WebString>& words) {
  Vector<String> convertedWords;
  convertedWords.append(words.data(), words.size());

  for (Frame* frame = m_page->mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (frame->isLocalFrame())
      toLocalFrame(frame)->removeSpellingMarkersUnderWords(convertedWords);
  }
}

void WebViewImpl::sendResizeEventAndRepaint() {
  // FIXME: This is wrong. The FrameView is responsible sending a resizeEvent
  // as part of layout. Layout is also responsible for sending invalidations
  // to the embedder. This method and all callers may be wrong. -- eseidel.
  if (mainFrameImpl()->frameView()) {
    // Enqueues the resize event.
    mainFrameImpl()->frame()->document()->enqueueResizeEvent();
  }

  if (m_client) {
    if (m_layerTreeView) {
      updateLayerTreeViewport();
    } else {
      WebRect damagedRect(0, 0, m_size.width, m_size.height);
      m_client->widgetClient()->didInvalidateRect(damagedRect);
    }
  }
}

void WebViewImpl::configureAutoResizeMode() {
  if (!mainFrameImpl() || !mainFrameImpl()->frame() ||
      !mainFrameImpl()->frame()->view())
    return;

  if (m_shouldAutoResize)
    mainFrameImpl()->frame()->view()->enableAutoSizeMode(m_minAutoSize,
                                                         m_maxAutoSize);
  else
    mainFrameImpl()->frame()->view()->disableAutoSizeMode();
}

unsigned long WebViewImpl::createUniqueIdentifierForRequest() {
  return createUniqueIdentifier();
}

void WebViewImpl::setCompositorDeviceScaleFactorOverride(
    float deviceScaleFactor) {
  if (m_compositorDeviceScaleFactorOverride == deviceScaleFactor)
    return;
  m_compositorDeviceScaleFactorOverride = deviceScaleFactor;
  if (m_zoomFactorForDeviceScaleFactor) {
    setZoomLevel(zoomLevel());
    return;
  }
  if (page() && m_layerTreeView)
    updateLayerTreeDeviceScaleFactor();
}

void WebViewImpl::setDeviceEmulationTransform(
    const TransformationMatrix& transform) {
  if (transform == m_deviceEmulationTransform)
    return;
  m_deviceEmulationTransform = transform;
  updateDeviceEmulationTransform();
}

TransformationMatrix WebViewImpl::getDeviceEmulationTransformForTesting()
    const {
  return m_deviceEmulationTransform;
}

void WebViewImpl::enableDeviceEmulation(
    const WebDeviceEmulationParams& params) {
  m_devToolsEmulator->enableDeviceEmulation(params);
}

void WebViewImpl::disableDeviceEmulation() {
  m_devToolsEmulator->disableDeviceEmulation();
}

WebAXObject WebViewImpl::accessibilityObject() {
  if (!mainFrameImpl())
    return WebAXObject();

  Document* document = mainFrameImpl()->frame()->document();
  return WebAXObject(toAXObjectCacheImpl(document->axObjectCache())->root());
}

void WebViewImpl::performCustomContextMenuAction(unsigned action) {
  if (!m_page)
    return;
  ContextMenu* menu = m_page->contextMenuController().contextMenu();
  if (!menu)
    return;
  const ContextMenuItem* item = menu->itemWithAction(
      static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + action));
  if (item)
    m_page->contextMenuController().contextMenuItemSelected(item);
  m_page->contextMenuController().clearContextMenu();
}

void WebViewImpl::showContextMenu() {
  if (!page())
    return;

  page()->contextMenuController().clearContextMenu();
  {
    ContextMenuAllowedScope scope;
    if (LocalFrame* focusedFrame =
            toLocalFrame(page()->focusController().focusedOrMainFrame()))
      focusedFrame->eventHandler().sendContextMenuEventForKey(nullptr);
  }
}

void WebViewImpl::didCloseContextMenu() {
  LocalFrame* frame = m_page->focusController().focusedFrame();
  if (frame)
    frame->selection().setCaretBlinkingSuspended(false);
}

void WebViewImpl::hidePopups() {
  cancelPagePopup();
}

void WebViewImpl::setIsTransparent(bool isTransparent) {
  // Set any existing frames to be transparent.
  Frame* frame = m_page->mainFrame();
  while (frame) {
    if (frame->isLocalFrame())
      toLocalFrame(frame)->view()->setTransparent(isTransparent);
    frame = frame->tree().traverseNext();
  }

  // Future frames check this to know whether to be transparent.
  m_isTransparent = isTransparent;

  if (m_layerTreeView)
    m_layerTreeView->setHasTransparentBackground(this->isTransparent());
}

bool WebViewImpl::isTransparent() const {
  return m_isTransparent;
}

WebInputMethodControllerImpl* WebViewImpl::getActiveWebInputMethodController()
    const {
  return WebInputMethodControllerImpl::fromFrame(focusedLocalFrameInWidget());
}

Color WebViewImpl::baseBackgroundColor() const {
  return m_baseBackgroundColorOverrideEnabled ? m_baseBackgroundColorOverride
                                              : m_baseBackgroundColor;
}

void WebViewImpl::setBaseBackgroundColor(WebColor color) {
  if (m_baseBackgroundColor == color)
    return;

  m_baseBackgroundColor = color;
  updateBaseBackgroundColor();
}

void WebViewImpl::setBaseBackgroundColorOverride(WebColor color) {
  m_baseBackgroundColorOverrideEnabled = true;
  m_baseBackgroundColorOverride = color;
  if (mainFrameImpl()) {
    // Force lifecycle update to ensure we're good to call
    // FrameView::setBaseBackgroundColor().
    mainFrameImpl()
        ->frame()
        ->view()
        ->updateLifecycleToCompositingCleanPlusScrolling();
  }
  updateBaseBackgroundColor();
}

void WebViewImpl::clearBaseBackgroundColorOverride() {
  m_baseBackgroundColorOverrideEnabled = false;
  if (mainFrameImpl()) {
    // Force lifecycle update to ensure we're good to call
    // FrameView::setBaseBackgroundColor().
    mainFrameImpl()
        ->frame()
        ->view()
        ->updateLifecycleToCompositingCleanPlusScrolling();
  }
  updateBaseBackgroundColor();
}

void WebViewImpl::updateBaseBackgroundColor() {
  Color color = baseBackgroundColor();
  if (m_page->mainFrame() && m_page->mainFrame()->isLocalFrame())
    m_page->deprecatedLocalMainFrame()->view()->setBaseBackgroundColor(color);
}

void WebViewImpl::setIsActive(bool active) {
  if (page())
    page()->focusController().setActive(active);
}

bool WebViewImpl::isActive() const {
  return page() ? page()->focusController().isActive() : false;
}

void WebViewImpl::setDomainRelaxationForbidden(bool forbidden,
                                               const WebString& scheme) {
  SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(forbidden,
                                                           String(scheme));
}

void WebViewImpl::setWindowFeatures(const WebWindowFeatures& features) {
  m_page->chromeClient().setWindowFeatures(features);
}

void WebViewImpl::setOpenedByDOM() {
  m_page->setOpenedByDOM();
}

void WebViewImpl::setSelectionColors(unsigned activeBackgroundColor,
                                     unsigned activeForegroundColor,
                                     unsigned inactiveBackgroundColor,
                                     unsigned inactiveForegroundColor) {
#if USE(DEFAULT_RENDER_THEME)
  LayoutThemeDefault::setSelectionColors(
      activeBackgroundColor, activeForegroundColor, inactiveBackgroundColor,
      inactiveForegroundColor);
  LayoutTheme::theme().platformColorsDidChange();
#endif
}

void WebViewImpl::didCommitLoad(bool isNewNavigation,
                                bool isNavigationWithinPage) {
  if (!isNavigationWithinPage) {
    m_shouldDispatchFirstVisuallyNonEmptyLayout = true;
    m_shouldDispatchFirstLayoutAfterFinishedParsing = true;
    m_shouldDispatchFirstLayoutAfterFinishedLoading = true;

    if (isNewNavigation) {
      pageScaleConstraintsSet().setNeedsReset(true);
      m_pageImportanceSignals.onCommitLoad();
    }
  }

  // Give the visual viewport's scroll layer its initial size.
  page()->frameHost().visualViewport().mainFrameDidChangeSize();

  // Make sure link highlight from previous page is cleared.
  m_linkHighlights.clear();
  endActiveFlingAnimation();
  m_userGestureObserved = false;
}

void WebViewImpl::postLayoutResize(WebLocalFrameImpl* webframe) {
  FrameView* view = webframe->frame()->view();
  if (webframe == mainFrame())
    m_resizeViewportAnchor->resizeFrameView(mainFrameSize());
  else
    view->resize(webframe->frameView()->size());
}

void WebViewImpl::layoutUpdated(WebLocalFrameImpl* webframe) {
  LocalFrame* frame = webframe->frame();
  if (!m_client || !frame->isMainFrame())
    return;

  if (m_shouldAutoResize) {
    WebSize frameSize = frame->view()->frameRect().size();
    if (frameSize != m_size) {
      m_size = frameSize;

      page()->frameHost().visualViewport().setSize(m_size);
      pageScaleConstraintsSet().didChangeInitialContainingBlockSize(m_size);
      frame->view()->setInitialViewportSize(m_size);

      m_client->didAutoResize(m_size);
      sendResizeEventAndRepaint();
    }
  }

  if (pageScaleConstraintsSet().constraintsDirty())
    refreshPageScaleFactorAfterLayout();

  FrameView* view = webframe->frame()->view();

  postLayoutResize(webframe);

  // Relayout immediately to avoid violating the rule that needsLayout()
  // isn't set at the end of a layout.
  if (view->needsLayout())
    view->layout();

  updatePageOverlays();

  m_fullscreenController->didUpdateLayout();
  m_client->didUpdateLayout();
}

void WebViewImpl::didChangeContentsSize() {
  pageScaleConstraintsSet().didChangeContentsSize(contentsSize(),
                                                  pageScaleFactor());
}

void WebViewImpl::pageScaleFactorChanged() {
  pageScaleConstraintsSet().setNeedsReset(false);
  updateLayerTreeViewport();
  m_client->pageScaleFactorChanged();
  m_devToolsEmulator->mainFrameScrollOrScaleChanged();
}

void WebViewImpl::mainFrameScrollOffsetChanged() {
  m_devToolsEmulator->mainFrameScrollOrScaleChanged();
}

bool WebViewImpl::useExternalPopupMenus() {
  return shouldUseExternalPopupMenus;
}

void WebViewImpl::setBackgroundColorOverride(WebColor color) {
  m_backgroundColorOverride = color;
  updateLayerTreeBackgroundColor();
}

void WebViewImpl::setZoomFactorOverride(float zoomFactor) {
  m_zoomFactorOverride = zoomFactor;
  setZoomLevel(zoomLevel());
}

void WebViewImpl::setPageOverlayColor(WebColor color) {
  if (m_pageColorOverlay)
    m_pageColorOverlay.reset();

  if (color == Color::transparent)
    return;

  m_pageColorOverlay = PageOverlay::create(
      mainFrameImpl(), WTF::makeUnique<ColorOverlay>(color));
  m_pageColorOverlay->update();
}

WebPageImportanceSignals* WebViewImpl::pageImportanceSignals() {
  return &m_pageImportanceSignals;
}

Element* WebViewImpl::focusedElement() const {
  LocalFrame* frame = m_page->focusController().focusedFrame();
  if (!frame)
    return nullptr;

  Document* document = frame->document();
  if (!document)
    return nullptr;

  return document->focusedElement();
}

HitTestResult WebViewImpl::hitTestResultForViewportPos(
    const IntPoint& posInViewport) {
  IntPoint rootFramePoint(
      m_page->frameHost().visualViewport().viewportToRootFrame(posInViewport));
  return hitTestResultForRootFramePos(rootFramePoint);
}

HitTestResult WebViewImpl::hitTestResultForRootFramePos(
    const IntPoint& posInRootFrame) {
  if (!m_page->mainFrame()->isLocalFrame())
    return HitTestResult();
  IntPoint docPoint(
      m_page->deprecatedLocalMainFrame()->view()->rootFrameToContents(
          posInRootFrame));
  HitTestResult result =
      m_page->deprecatedLocalMainFrame()->eventHandler().hitTestResultAtPoint(
          docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
  result.setToShadowHostIfInUserAgentShadowRoot();
  return result;
}

WebHitTestResult WebViewImpl::hitTestResultForTap(
    const WebPoint& tapPointWindowPos,
    const WebSize& tapArea) {
  if (!m_page->mainFrame()->isLocalFrame())
    return HitTestResult();

  WebGestureEvent tapEvent(WebInputEvent::GestureTap,
                           WebInputEvent::NoModifiers,
                           WTF::monotonicallyIncreasingTime());
  tapEvent.x = tapPointWindowPos.x;
  tapEvent.y = tapPointWindowPos.y;
  // GestureTap is only ever from a touchscreen.
  tapEvent.sourceDevice = WebGestureDeviceTouchscreen;
  tapEvent.data.tap.tapCount = 1;
  tapEvent.data.tap.width = tapArea.width;
  tapEvent.data.tap.height = tapArea.height;

  WebGestureEvent scaledEvent =
      TransformWebGestureEvent(mainFrameImpl()->frameView(), tapEvent);

  HitTestResult result =
      m_page->deprecatedLocalMainFrame()
          ->eventHandler()
          .hitTestResultForGestureEvent(
              scaledEvent, HitTestRequest::ReadOnly | HitTestRequest::Active)
          .hitTestResult();

  result.setToShadowHostIfInUserAgentShadowRoot();
  return result;
}

void WebViewImpl::setTabsToLinks(bool enable) {
  m_tabsToLinks = enable;
}

bool WebViewImpl::tabsToLinks() const {
  return m_tabsToLinks;
}

void WebViewImpl::registerViewportLayersWithCompositor() {
  DCHECK(m_layerTreeView);

  if (!page()->mainFrame() || !page()->mainFrame()->isLocalFrame())
    return;

  Document* document = page()->deprecatedLocalMainFrame()->document();

  DCHECK(document);

  // Get the outer viewport scroll layer.
  GraphicsLayer* layoutViewportScrollLayer =
      page()->frameHost().globalRootScrollerController().rootScrollerLayer();
  WebLayer* layoutViewportWebLayer =
      layoutViewportScrollLayer ? layoutViewportScrollLayer->platformLayer()
                                : nullptr;

  VisualViewport& visualViewport = page()->frameHost().visualViewport();

  // TODO(bokan): This was moved here from when registerViewportLayers was a
  // part of VisualViewport and maybe doesn't belong here. See comment inside
  // the mehtod.
  visualViewport.setScrollLayerOnScrollbars(layoutViewportWebLayer);

  m_layerTreeView->registerViewportLayers(
      visualViewport.overscrollElasticityLayer()->platformLayer(),
      visualViewport.pageScaleLayer()->platformLayer(),
      visualViewport.scrollLayer()->platformLayer(), layoutViewportWebLayer);
}

void WebViewImpl::setRootGraphicsLayer(GraphicsLayer* graphicsLayer) {
  if (!m_layerTreeView)
    return;

  // In SPv2, setRootLayer is used instead.
  DCHECK(!RuntimeEnabledFeatures::slimmingPaintV2Enabled());

  VisualViewport& visualViewport = page()->frameHost().visualViewport();
  visualViewport.attachToLayerTree(graphicsLayer);
  if (graphicsLayer) {
    m_rootGraphicsLayer = visualViewport.rootGraphicsLayer();
    m_visualViewportContainerLayer = visualViewport.containerLayer();
    m_rootLayer = m_rootGraphicsLayer->platformLayer();
    updateDeviceEmulationTransform();
    m_layerTreeView->setRootLayer(*m_rootLayer);
    // We register viewport layers here since there may not be a layer
    // tree view prior to this point.
    registerViewportLayersWithCompositor();

    // TODO(enne): Work around page visibility changes not being
    // propagated to the WebView in some circumstances.  This needs to
    // be refreshed here when setting a new root layer to avoid being
    // stuck in a presumed incorrectly invisible state.
    m_layerTreeView->setVisible(page()->isPageVisible());
  } else {
    m_rootGraphicsLayer = nullptr;
    m_visualViewportContainerLayer = nullptr;
    m_rootLayer = nullptr;
    // This means that we're transitioning to a new page. Suppress
    // commits until Blink generates invalidations so we don't
    // attempt to paint too early in the next page load.
    m_layerTreeView->setDeferCommits(true);
    m_layerTreeView->clearRootLayer();
    m_layerTreeView->clearViewportLayers();
    if (WebDevToolsAgentImpl* devTools = mainFrameDevToolsAgentImpl())
      devTools->rootLayerCleared();
  }
}

void WebViewImpl::setRootLayer(WebLayer* layer) {
  if (!m_layerTreeView)
    return;

  if (layer) {
    m_rootLayer = layer;
    m_layerTreeView->setRootLayer(*m_rootLayer);
    m_layerTreeView->setVisible(page()->isPageVisible());
  } else {
    m_rootLayer = nullptr;
    // This means that we're transitioning to a new page. Suppress
    // commits until Blink generates invalidations so we don't
    // attempt to paint too early in the next page load.
    m_layerTreeView->setDeferCommits(true);
    m_layerTreeView->clearRootLayer();
    m_layerTreeView->clearViewportLayers();
    if (WebDevToolsAgentImpl* devTools = mainFrameDevToolsAgentImpl())
      devTools->rootLayerCleared();
  }
}

void WebViewImpl::invalidateRect(const IntRect& rect) {
  if (m_layerTreeView) {
    updateLayerTreeViewport();
  } else if (m_client) {
    // This is only for WebViewPlugin.
    m_client->widgetClient()->didInvalidateRect(rect);
  }
}

PaintLayerCompositor* WebViewImpl::compositor() const {
  WebLocalFrameImpl* frame = mainFrameImpl();
  if (!frame)
    return nullptr;

  Document* document = frame->frame()->document();
  if (!document || document->layoutViewItem().isNull())
    return nullptr;

  return document->layoutViewItem().compositor();
}

GraphicsLayer* WebViewImpl::rootGraphicsLayer() {
  return m_rootGraphicsLayer;
}

void WebViewImpl::scheduleAnimationForWidget() {
  if (m_layerTreeView) {
    m_layerTreeView->setNeedsBeginFrame();
    return;
  }
  if (m_client)
    m_client->widgetClient()->scheduleAnimation();
}

void WebViewImpl::attachCompositorAnimationTimeline(
    CompositorAnimationTimeline* timeline) {
  if (m_animationHost)
    m_animationHost->addTimeline(*timeline);
}

void WebViewImpl::detachCompositorAnimationTimeline(
    CompositorAnimationTimeline* timeline) {
  if (m_animationHost)
    m_animationHost->removeTimeline(*timeline);
}

void WebViewImpl::initializeLayerTreeView() {
  if (m_client) {
    m_layerTreeView = m_client->initializeLayerTreeView();
    if (m_layerTreeView && m_layerTreeView->compositorAnimationHost()) {
      m_animationHost = WTF::makeUnique<CompositorAnimationHost>(
          m_layerTreeView->compositorAnimationHost());
    }
  }

  if (WebDevToolsAgentImpl* devTools = mainFrameDevToolsAgentImpl())
    devTools->layerTreeViewChanged(m_layerTreeView);

  m_page->settings().setAcceleratedCompositingEnabled(m_layerTreeView);
  if (m_layerTreeView)
    m_page->layerTreeViewInitialized(*m_layerTreeView, nullptr);

  // FIXME: only unittests, click to play, Android printing, and printing (for
  // headers and footers) make this assert necessary. We should make them not
  // hit this code and then delete allowsBrokenNullLayerTreeView.
  DCHECK(m_layerTreeView || !m_client ||
         m_client->widgetClient()->allowsBrokenNullLayerTreeView());

  if (Platform::current()->isThreadedAnimationEnabled() && m_layerTreeView) {
    m_linkHighlightsTimeline = CompositorAnimationTimeline::create();
    attachCompositorAnimationTimeline(m_linkHighlightsTimeline.get());
  }
}

void WebViewImpl::applyViewportDeltas(
    const WebFloatSize& visualViewportDelta,
    // TODO(bokan): This parameter is to be removed but requires adjusting many
    // callsites.
    const WebFloatSize&,
    const WebFloatSize& elasticOverscrollDelta,
    float pageScaleDelta,
    float browserControlsShownRatioDelta) {
  VisualViewport& visualViewport = page()->frameHost().visualViewport();

  // Store the desired offsets the visual viewport before setting the top
  // controls ratio since doing so will change the bounds and move the
  // viewports to keep the offsets valid. The compositor may have already
  // done that so we don't want to double apply the deltas here.
  FloatPoint visualViewportOffset = visualViewport.visibleRect().location();
  visualViewportOffset.move(visualViewportDelta.width,
                            visualViewportDelta.height);

  browserControls().setShownRatio(browserControls().shownRatio() +
                                  browserControlsShownRatioDelta);

  setPageScaleFactorAndLocation(pageScaleFactor() * pageScaleDelta,
                                visualViewportOffset);

  if (pageScaleDelta != 1) {
    m_doubleTapZoomPending = false;
    visualViewport.userDidChangeScale();
  }

  m_elasticOverscroll += elasticOverscrollDelta;

  if (mainFrameImpl() && mainFrameImpl()->frameView())
    mainFrameImpl()->frameView()->didUpdateElasticOverscroll();
}

void WebViewImpl::updateLayerTreeViewport() {
  if (!page() || !m_layerTreeView)
    return;

  m_layerTreeView->setPageScaleFactorAndLimits(
      pageScaleFactor(), minimumPageScaleFactor(), maximumPageScaleFactor());
}

void WebViewImpl::updateLayerTreeBackgroundColor() {
  if (!m_layerTreeView)
    return;

  m_layerTreeView->setBackgroundColor(alphaChannel(m_backgroundColorOverride)
                                          ? m_backgroundColorOverride
                                          : backgroundColor());
}

void WebViewImpl::updateLayerTreeDeviceScaleFactor() {
  DCHECK(page());
  DCHECK(m_layerTreeView);

  float deviceScaleFactor = m_compositorDeviceScaleFactorOverride
                                ? m_compositorDeviceScaleFactorOverride
                                : page()->deviceScaleFactorDeprecated();
  m_layerTreeView->setDeviceScaleFactor(deviceScaleFactor);
}

void WebViewImpl::updateDeviceEmulationTransform() {
  if (!m_visualViewportContainerLayer)
    return;

  // When the device emulation transform is updated, to avoid incorrect
  // scales and fuzzy raster from the compositor, force all content to
  // pick ideal raster scales.
  m_visualViewportContainerLayer->setTransform(m_deviceEmulationTransform);
  m_layerTreeView->forceRecalculateRasterScales();
}

bool WebViewImpl::detectContentOnTouch(
    const GestureEventWithHitTestResults& targetedEvent) {
  if (!m_page->mainFrame()->isLocalFrame())
    return false;

  // Need a local copy of the hit test as
  // setToShadowHostIfInUserAgentShadowRoot() will modify it.
  HitTestResult touchHit = targetedEvent.hitTestResult();
  touchHit.setToShadowHostIfInUserAgentShadowRoot();

  if (touchHit.isContentEditable())
    return false;

  Node* node = touchHit.innerNode();
  if (!node || !node->isTextNode())
    return false;

  // Ignore when tapping on links or nodes listening to click events, unless
  // the click event is on the body element, in which case it's unlikely that
  // the original node itself was intended to be clickable.
  for (; node && !isHTMLBodyElement(*node);
       node = LayoutTreeBuilderTraversal::parent(*node)) {
    if (node->isLink() || node->willRespondToTouchEvents() ||
        node->willRespondToMouseClickEvents())
      return false;
  }

  WebURL intent = m_client->detectContentIntentAt(touchHit);
  if (!intent.isValid())
    return false;

  // This code is called directly after hit test code, with no user code
  // running in between, thus it is assumed that the frame pointer is non-null.
  bool isMainFrame = node ? node->document().frame()->isMainFrame() : true;
  m_client->scheduleContentIntent(intent, isMainFrame);
  return true;
}

WebViewScheduler* WebViewImpl::scheduler() const {
  return m_scheduler.get();
}

void WebViewImpl::setVisibilityState(WebPageVisibilityState visibilityState,
                                     bool isInitialState) {
  DCHECK(visibilityState == WebPageVisibilityStateVisible ||
         visibilityState == WebPageVisibilityStateHidden ||
         visibilityState == WebPageVisibilityStatePrerender);

  if (page())
    m_page->setVisibilityState(
        static_cast<PageVisibilityState>(static_cast<int>(visibilityState)),
        isInitialState);

  bool visible = visibilityState == WebPageVisibilityStateVisible;
  if (m_layerTreeView && !m_overrideCompositorVisibility)
    m_layerTreeView->setVisible(visible);
  m_scheduler->setPageVisible(visible);
}

void WebViewImpl::setCompositorVisibility(bool isVisible) {
  if (!isVisible)
    m_overrideCompositorVisibility = true;
  else
    m_overrideCompositorVisibility = false;
  if (m_layerTreeView)
    m_layerTreeView->setVisible(isVisible);
}

void WebViewImpl::forceNextWebGLContextCreationToFail() {
  WebGLRenderingContext::forceNextWebGLContextCreationToFail();
}

void WebViewImpl::forceNextDrawingBufferCreationToFail() {
  DrawingBuffer::forceNextDrawingBufferCreationToFail();
}

CompositorMutatorImpl& WebViewImpl::mutator() {
  if (!m_mutator) {
    std::unique_ptr<CompositorMutatorClient> mutatorClient =
        CompositorMutatorImpl::createClient();
    m_mutator = static_cast<CompositorMutatorImpl*>(mutatorClient->mutator());
    m_layerTreeView->setMutatorClient(std::move(mutatorClient));
  }

  return *m_mutator;
}

CompositorWorkerProxyClient* WebViewImpl::createCompositorWorkerProxyClient() {
  return new CompositorWorkerProxyClientImpl(&mutator());
}

AnimationWorkletProxyClient* WebViewImpl::createAnimationWorkletProxyClient() {
  return new AnimationWorkletProxyClientImpl(&mutator());
}

void WebViewImpl::updatePageOverlays() {
  if (m_pageColorOverlay)
    m_pageColorOverlay->update();
  if (InspectorOverlay* overlay = inspectorOverlay()) {
    PageOverlay* inspectorPageOverlay = overlay->pageOverlay();
    if (inspectorPageOverlay)
      inspectorPageOverlay->update();
  }
}

float WebViewImpl::deviceScaleFactor() const {
  // TODO(oshima): Investigate if this should return the ScreenInfo's scale
  // factor rather than page's scale factor, which can be 1 in use-zoom-for-dsf
  // mode.
  if (!page())
    return 1;

  return page()->deviceScaleFactorDeprecated();
}

LocalFrame* WebViewImpl::focusedLocalFrameInWidget() const {
  if (!mainFrameImpl())
    return nullptr;

  LocalFrame* focusedFrame = toLocalFrame(focusedCoreFrame());
  if (focusedFrame->localFrameRoot() != mainFrameImpl()->frame())
    return nullptr;
  return focusedFrame;
}

LocalFrame* WebViewImpl::focusedLocalFrameAvailableForIme() const {
  return m_imeAcceptEvents ? focusedLocalFrameInWidget() : nullptr;
}

}  // namespace blink
