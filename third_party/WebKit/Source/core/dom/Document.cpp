/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/dom/Document.h"

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/HTMLScriptElementOrSVGScriptElement.h"
#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/StringOrDictionary.h"
#include "bindings/core/v8/V0CustomElementConstructorBuilder.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8ElementCreationOptions.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/HTMLElementFactory.h"
#include "core/HTMLElementTypeHelpers.h"
#include "core/HTMLNames.h"
#include "core/SVGElementFactory.h"
#include "core/SVGNames.h"
#include "core/XMLNSNames.h"
#include "core/XMLNames.h"
#include "core/animation/CompositorPendingAnimations.h"
#include "core/animation/DocumentAnimations.h"
#include "core/animation/DocumentTimeline.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSTiming.h"
#include "core/css/FontFaceSet.h"
#include "core/css/MediaQueryMatcher.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/StyleSheetList.h"
#include "core/css/invalidation/StyleInvalidator.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Attr.h"
#include "core/dom/CDATASection.h"
#include "core/dom/ClientRect.h"
#include "core/dom/Comment.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentParserTiming.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/ElementCreationOptions.h"
#include "core/dom/ElementDataCache.h"
#include "core/dom/ElementRegistrationOptions.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/dom/IntersectionObserverController.h"
#include "core/dom/LayoutTreeBuilderTraversal.h"
#include "core/dom/LiveNodeList.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/NodeChildRemovalTracker.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/NodeFilter.h"
#include "core/dom/NodeIterator.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/NodeWithIndex.h"
#include "core/dom/NthIndexCache.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/ResizeObserverController.h"
#include "core/dom/ScriptRunner.h"
#include "core/dom/ScriptedAnimationController.h"
#include "core/dom/ScriptedIdleTaskController.h"
#include "core/dom/SelectorQuery.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/TouchList.h"
#include "core/dom/TransformSource.h"
#include "core/dom/TreeWalker.h"
#include "core/dom/VisitedLinkState.h"
#include "core/dom/XMLDocument.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "core/dom/custom/CustomElementRegistry.h"
#include "core/dom/custom/V0CustomElementMicrotaskRunQueue.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/BeforeUnloadEvent.h"
#include "core/events/Event.h"
#include "core/events/EventFactory.h"
#include "core/events/EventListener.h"
#include "core/events/HashChangeEvent.h"
#include "core/events/PageTransitionEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/events/VisualViewportResizeEvent.h"
#include "core/events/VisualViewportScrollEvent.h"
#include "core/frame/DOMTimer.h"
#include "core/frame/DOMVisualViewport.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/History.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/PerformanceMonitor.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/DocumentNameCollection.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLBaseElement.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html/HTMLScriptElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/HTMLTitleElement.h"
#include "core/html/PluginDocument.h"
#include "core/html/WindowNameCollection.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasFontCache.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/forms/FormController.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/NestingLevelIncrementer.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/layout/HitTestCanvasResult.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/CookieJar.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameFetchContext.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ImageLoader.h"
#include "core/loader/NavigationScheduler.h"
#include "core/loader/PrerendererClient.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/ChromeClient.h"
#include "core/page/EventWithHitTestResults.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/NetworkStateNotifier.h"
#include "core/page/Page.h"
#include "core/page/PointerLockController.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "core/page/scrolling/ScrollStateCallback.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/page/scrolling/SnapCoordinator.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGScriptElement.h"
#include "core/svg/SVGTitleElement.h"
#include "core/svg/SVGUseElement.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "core/workers/SharedWorkerRepositoryClient.h"
#include "core/xml/parser/XMLDocumentParser.h"
#include "platform/DateComponents.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/Histogram.h"
#include "platform/InstanceCounters.h"
#include "platform/Language.h"
#include "platform/LengthFunctions.h"
#include "platform/PluginScriptForbiddenScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/WebFrameScheduler.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/HTTPParsers.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/text/PlatformLocale.h"
#include "platform/text/SegmentedString.h"
#include "platform/weborigin/OriginAccessEntry.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebPrerenderingSupport.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/modules/sensitive_input_visibility/sensitive_input_visibility_service.mojom-blink.h"
#include "public/platform/site_engagement.mojom-blink.h"
#include "wtf/AutoReset.h"
#include "wtf/CurrentTime.h"
#include "wtf/DateMath.h"
#include "wtf/Functional.h"
#include "wtf/HashFunctions.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/TextEncodingRegistry.h"

#include <memory>

using namespace WTF;
using namespace Unicode;

#ifndef NDEBUG
using WeakDocumentSet =
    blink::PersistentHeapHashSet<blink::WeakMember<blink::Document>>;
static WeakDocumentSet& liveDocumentSet();
#endif

namespace blink {

using namespace HTMLNames;

static const unsigned cMaxWriteRecursionDepth = 21;

// This amount of time must have elapsed before we will even consider scheduling
// a layout without a delay.
// FIXME: For faster machines this value can really be lowered to 200.  250 is
// adequate, but a little high for dual G5s. :)
static const int cLayoutScheduleThreshold = 250;

// DOM Level 2 says (letters added):
//
// a) Name start characters must have one of the categories Ll, Lu, Lo, Lt, Nl.
// b) Name characters other than Name-start characters must have one of the
//    categories Mc, Me, Mn, Lm, or Nd.
// c) Characters in the compatibility area (i.e. with character code greater
//    than #xF900 and less than #xFFFE) are not allowed in XML names.
// d) Characters which have a font or compatibility decomposition (i.e. those
//    with a "compatibility formatting tag" in field 5 of the database -- marked
//    by field 5 beginning with a "<") are not allowed.
// e) The following characters are treated as name-start characters rather than
//    name characters, because the property file classifies them as Alphabetic:
//    [#x02BB-#x02C1], #x0559, #x06E5, #x06E6.
// f) Characters #x20DD-#x20E0 are excluded (in accordance with Unicode, section
//    5.14).
// g) Character #x00B7 is classified as an extender, because the property list
//    so identifies it.
// h) Character #x0387 is added as a name character, because #x00B7 is its
//    canonical equivalent.
// i) Characters ':' and '_' are allowed as name-start characters.
// j) Characters '-' and '.' are allowed as name characters.
//
// It also contains complete tables. If we decide it's better, we could include
// those instead of the following code.

static inline bool isValidNameStart(UChar32 c) {
  // rule (e) above
  if ((c >= 0x02BB && c <= 0x02C1) || c == 0x559 || c == 0x6E5 || c == 0x6E6)
    return true;

  // rule (i) above
  if (c == ':' || c == '_')
    return true;

  // rules (a) and (f) above
  const uint32_t nameStartMask = Letter_Lowercase | Letter_Uppercase |
                                 Letter_Other | Letter_Titlecase |
                                 Number_Letter;
  if (!(Unicode::category(c) & nameStartMask))
    return false;

  // rule (c) above
  if (c >= 0xF900 && c < 0xFFFE)
    return false;

  // rule (d) above
  CharDecompositionType decompType = decompositionType(c);
  if (decompType == DecompositionFont || decompType == DecompositionCompat)
    return false;

  return true;
}

static inline bool isValidNamePart(UChar32 c) {
  // rules (a), (e), and (i) above
  if (isValidNameStart(c))
    return true;

  // rules (g) and (h) above
  if (c == 0x00B7 || c == 0x0387)
    return true;

  // rule (j) above
  if (c == '-' || c == '.')
    return true;

  // rules (b) and (f) above
  const uint32_t otherNamePartMask = Mark_NonSpacing | Mark_Enclosing |
                                     Mark_SpacingCombining | Letter_Modifier |
                                     Number_DecimalDigit;
  if (!(Unicode::category(c) & otherNamePartMask))
    return false;

  // rule (c) above
  if (c >= 0xF900 && c < 0xFFFE)
    return false;

  // rule (d) above
  CharDecompositionType decompType = decompositionType(c);
  if (decompType == DecompositionFont || decompType == DecompositionCompat)
    return false;

  return true;
}

static FrameViewBase* widgetForElement(const Element& focusedElement) {
  LayoutObject* layoutObject = focusedElement.layoutObject();
  if (!layoutObject || !layoutObject->isLayoutPart())
    return 0;
  return toLayoutPart(layoutObject)->widget();
}

static bool acceptsEditingFocus(const Element& element) {
  DCHECK(hasEditableStyle(element));

  return element.document().frame() && rootEditableElement(element);
}

uint64_t Document::s_globalTreeVersion = 0;

static bool s_threadedParsingEnabledForTesting = true;

// This doesn't work with non-Document ExecutionContext.
static void runAutofocusTask(ExecutionContext* context) {
  // Document lifecycle check is done in Element::focus()
  if (!context)
    return;

  Document* document = toDocument(context);
  if (Element* element = document->autofocusElement()) {
    document->setAutofocusElement(0);
    element->focus();
  }
}

static void recordLoadReasonToHistogram(WouldLoadReason reason) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, unseenFrameHistogram,
      ("Navigation.DeferredDocumentLoading.StatesV4", WouldLoadReasonEnd));
  unseenFrameHistogram.count(reason);
}

class Document::NetworkStateObserver final
    : public GarbageCollectedFinalized<Document::NetworkStateObserver>,
      public NetworkStateNotifier::NetworkStateObserver,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(Document::NetworkStateObserver);

 public:
  explicit NetworkStateObserver(Document& document)
      : ContextLifecycleObserver(&document) {
    networkStateNotifier().addOnLineObserver(
        this,
        TaskRunnerHelper::get(TaskType::Networking, getExecutionContext()));
  }

  void onLineStateChange(bool onLine) override {
    AtomicString eventName =
        onLine ? EventTypeNames::online : EventTypeNames::offline;
    Document* document = toDocument(getExecutionContext());
    if (!document->domWindow())
      return;
    document->domWindow()->dispatchEvent(Event::create(eventName));
    probe::networkStateChanged(document->frame(), onLine);
  }

  void contextDestroyed(ExecutionContext* context) override {
    unregisterAsObserver(context);
  }

  void unregisterAsObserver(ExecutionContext* context) {
    DCHECK(context);
    networkStateNotifier().removeOnLineObserver(
        this, TaskRunnerHelper::get(TaskType::Networking, context));
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { ContextLifecycleObserver::trace(visitor); }
};

Document::Document(const DocumentInit& initializer,
                   DocumentClassFlags documentClasses)
    : ContainerNode(0, CreateDocument),
      TreeScope(*this),
      m_hasNodesWithPlaceholderStyle(false),
      m_evaluateMediaQueriesOnStyleRecalc(false),
      m_pendingSheetLayout(NoLayoutWithPendingSheets),
      m_frame(initializer.frame()),
      // TODO(dcheng): Why does this need both a LocalFrame and LocalDOMWindow
      // pointer?
      m_domWindow(m_frame ? m_frame->domWindow() : nullptr),
      m_importsController(this, initializer.importsController()),
      m_contextFeatures(ContextFeatures::defaultSwitch()),
      m_wellFormed(false),
      m_implementation(this, nullptr),
      m_printing(NotPrinting),
      m_paginatedForScreen(false),
      m_compatibilityMode(NoQuirksMode),
      m_compatibilityModeLocked(false),
      m_hasAutofocused(false),
      m_clearFocusedElementTimer(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, this),
          this,
          &Document::clearFocusedElementTimerFired),
      m_domTreeVersion(++s_globalTreeVersion),
      m_styleVersion(0),
      m_listenerTypes(0),
      m_mutationObserverTypes(0),
      m_styleEngine(this, nullptr),
      m_styleSheetList(this, nullptr),
      m_visitedLinkState(VisitedLinkState::create(*this)),
      m_visuallyOrdered(false),
      m_readyState(Complete),
      m_parsingState(FinishedParsing),
      m_gotoAnchorNeededAfterStylesheetsLoad(false),
      m_containsValidityStyleRules(false),
      m_containsPlugins(false),
      m_ignoreDestructiveWriteCount(0),
      m_throwOnDynamicMarkupInsertionCount(0),
      m_markers(new DocumentMarkerController(*this)),
      m_updateFocusAppearanceTimer(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, this),
          this,
          &Document::updateFocusAppearanceTimerFired),
      m_cssTarget(nullptr),
      m_loadEventProgress(LoadEventNotRun),
      m_startTime(currentTime()),
      m_scriptRunner(ScriptRunner::create(this)),
      m_xmlVersion("1.0"),
      m_xmlStandalone(StandaloneUnspecified),
      m_hasXMLDeclaration(0),
      m_designMode(false),
      m_isRunningExecCommand(false),
      m_hasAnnotatedRegions(false),
      m_annotatedRegionsDirty(false),
      m_documentClasses(documentClasses),
      m_isViewSource(false),
      m_sawElementsInKnownNamespaces(false),
      m_isSrcdocDocument(false),
      m_isMobileDocument(false),
      m_layoutView(0),
      m_contextDocument(initializer.contextDocument()),
      m_hasFullscreenSupplement(false),
      m_loadEventDelayCount(0),
      m_loadEventDelayTimer(TaskRunnerHelper::get(TaskType::Networking, this),
                            this,
                            &Document::loadEventDelayTimerFired),
      m_pluginLoadingTimer(
          TaskRunnerHelper::get(TaskType::UnspecedLoading, this),
          this,
          &Document::pluginLoadingTimerFired),
      m_documentTiming(*this),
      m_writeRecursionIsTooDeep(false),
      m_writeRecursionDepth(0),
      m_registrationContext(initializer.registrationContext(this)),
      m_elementDataCacheClearTimer(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, this),
          this,
          &Document::elementDataCacheClearTimerFired),
      m_timeline(DocumentTimeline::create(this)),
      m_compositorPendingAnimations(new CompositorPendingAnimations(*this)),
      m_templateDocumentHost(nullptr),
      m_didAssociateFormControlsTimer(
          TaskRunnerHelper::get(TaskType::UnspecedLoading, this),
          this,
          &Document::didAssociateFormControlsTimerFired),
      m_timers(TaskRunnerHelper::get(TaskType::Timer, this)),
      m_hasViewportUnits(false),
      m_parserSyncPolicy(AllowAsynchronousParsing),
      m_nodeCount(0),
      m_wouldLoadReason(Invalid),
      m_passwordCount(0),
      m_engagementLevel(mojom::blink::EngagementLevel::NONE) {
  if (m_frame) {
    DCHECK(m_frame->page());
    provideContextFeaturesToDocumentFrom(*this, *m_frame->page());

    m_fetcher = m_frame->loader().documentLoader()->fetcher();
    FrameFetchContext::provideDocumentToContext(m_fetcher->context(), this);

    // TODO(dcheng): Why does this need to check that DOMWindow is non-null?
    CustomElementRegistry* registry =
        m_frame->domWindow() ? m_frame->domWindow()->maybeCustomElements()
                             : nullptr;
    if (registry && m_registrationContext)
      registry->entangle(m_registrationContext);
  } else if (m_importsController) {
    m_fetcher = FrameFetchContext::createFetcherFromDocument(this);
  } else {
    m_fetcher = ResourceFetcher::create(nullptr);
  }
  DCHECK(m_fetcher);

  m_rootScrollerController = RootScrollerController::create(*this);

  // We depend on the url getting immediately set in subframes, but we
  // also depend on the url NOT getting immediately set in opened windows.
  // See fast/dom/early-frame-url.html
  // and fast/dom/location-new-window-no-crash.html, respectively.
  // FIXME: Can/should we unify this behavior?
  if (initializer.shouldSetURL())
    setURL(initializer.url());

  initSecurityContext(initializer);

  initDNSPrefetch();

  InstanceCounters::incrementCounter(InstanceCounters::DocumentCounter);

  m_lifecycle.advanceTo(DocumentLifecycle::Inactive);

  // Since CSSFontSelector requires Document::m_fetcher and StyleEngine owns
  // CSSFontSelector, need to initialize m_styleEngine after initializing
  // m_fetcher.
  m_styleEngine = StyleEngine::create(*this);

  // The parent's parser should be suspended together with all the other
  // objects, else this new Document would have a new ExecutionContext which
  // suspended state would not match the one from the parent, and could start
  // loading resources ignoring the defersLoading flag.
  DCHECK(!parentDocument() || !parentDocument()->isContextSuspended());

#ifndef NDEBUG
  liveDocumentSet().insert(this);
#endif
}

Document::~Document() {
  DCHECK(layoutViewItem().isNull());
  DCHECK(!parentTreeScope());
  // If a top document with a cache, verify that it was comprehensively
  // cleared during detach.
  DCHECK(!m_axObjectCache);
  InstanceCounters::decrementCounter(InstanceCounters::DocumentCounter);
}

SelectorQueryCache& Document::selectorQueryCache() {
  if (!m_selectorQueryCache)
    m_selectorQueryCache = WTF::makeUnique<SelectorQueryCache>();
  return *m_selectorQueryCache;
}

MediaQueryMatcher& Document::mediaQueryMatcher() {
  if (!m_mediaQueryMatcher)
    m_mediaQueryMatcher = MediaQueryMatcher::create(*this);
  return *m_mediaQueryMatcher;
}

void Document::mediaQueryAffectingValueChanged() {
  styleEngine().mediaQueryAffectingValueChanged();
  if (needsLayoutTreeUpdate())
    m_evaluateMediaQueriesOnStyleRecalc = true;
  else
    evaluateMediaQueryList();
  probe::mediaQueryResultChanged(this);
}

void Document::setCompatibilityMode(CompatibilityMode mode) {
  if (m_compatibilityModeLocked || mode == m_compatibilityMode)
    return;
  m_compatibilityMode = mode;
  selectorQueryCache().invalidate();
}

String Document::compatMode() const {
  return inQuirksMode() ? "BackCompat" : "CSS1Compat";
}

void Document::setDoctype(DocumentType* docType) {
  // This should never be called more than once.
  DCHECK(!m_docType || !docType);
  m_docType = docType;
  if (m_docType) {
    this->adoptIfNeeded(*m_docType);
    if (m_docType->publicId().startsWith("-//wapforum//dtd xhtml mobile 1.",
                                         TextCaseASCIIInsensitive)) {
      m_isMobileDocument = true;
      m_styleEngine->viewportRulesChanged();
    }
  }
}

DOMImplementation& Document::implementation() {
  if (!m_implementation)
    m_implementation = DOMImplementation::create(*this);
  return *m_implementation;
}

bool Document::hasAppCacheManifest() const {
  return isHTMLHtmlElement(documentElement()) &&
         documentElement()->hasAttribute(manifestAttr);
}

Location* Document::location() const {
  if (!frame())
    return 0;

  return domWindow()->location();
}

void Document::childrenChanged(const ChildrenChange& change) {
  ContainerNode::childrenChanged(change);
  m_documentElement = ElementTraversal::firstWithin(*this);

  // For non-HTML documents the willInsertBody notification won't happen
  // so we resume as soon as we have a document element. Even for XHTML
  // documents there may never be a <body> (since the parser won't always
  // insert one), so we resume here too. That does mean XHTML documents make
  // frames when there's only a <head>, but such documents are pretty rare.
  if (m_documentElement && !isHTMLDocument())
    beginLifecycleUpdatesIfRenderingReady();
}

void Document::setRootScroller(Element* newScroller,
                               ExceptionState& exceptionState) {
  m_rootScrollerController->set(newScroller);
}

Element* Document::rootScroller() const {
  return m_rootScrollerController->get();
}

bool Document::isInMainFrame() const {
  return frame() && frame()->isMainFrame();
}

AtomicString Document::convertLocalName(const AtomicString& name) {
  return isHTMLDocument() ? name.lowerASCII() : name;
}

// https://dom.spec.whatwg.org/#dom-document-createelement
Element* Document::createElement(const AtomicString& name,
                                 ExceptionState& exceptionState) {
  if (!isValidName(name)) {
    exceptionState.throwDOMException(
        InvalidCharacterError,
        "The tag name provided ('" + name + "') is not a valid name.");
    return nullptr;
  }

  if (isXHTMLDocument() || isHTMLDocument()) {
    // 2. If the context object is an HTML document, let localName be
    // converted to ASCII lowercase.
    AtomicString localName = convertLocalName(name);
    if (CustomElement::shouldCreateCustomElement(localName)) {
      return CustomElement::createCustomElementSync(
          *this,
          QualifiedName(nullAtom, localName, HTMLNames::xhtmlNamespaceURI));
    }
    return HTMLElementFactory::createHTMLElement(localName, *this,
                                                 CreatedByCreateElement);
  }
  return Element::create(QualifiedName(nullAtom, name, nullAtom), this);
}

String getTypeExtension(Document* document,
                        const StringOrDictionary& stringOrOptions,
                        ExceptionState& exceptionState) {
  if (stringOrOptions.isNull())
    return emptyString;

  if (stringOrOptions.isString()) {
    UseCounter::count(document,
                      UseCounter::DocumentCreateElement2ndArgStringHandling);
    return stringOrOptions.getAsString();
  }

  if (stringOrOptions.isDictionary()) {
    Dictionary dict = stringOrOptions.getAsDictionary();
    ElementCreationOptions impl;
    V8ElementCreationOptions::toImpl(dict.isolate(), dict.v8Value(), impl,
                                     exceptionState);
    if (exceptionState.hadException())
      return emptyString;

    if (impl.hasIs())
      return impl.is();
  }

  return emptyString;
}

// https://dom.spec.whatwg.org/#dom-document-createelement
Element* Document::createElement(const AtomicString& localName,
                                 const StringOrDictionary& stringOrOptions,
                                 ExceptionState& exceptionState) {
  // 1. If localName does not match Name production, throw InvalidCharacterError
  if (!isValidName(localName)) {
    exceptionState.throwDOMException(
        InvalidCharacterError,
        "The tag name provided ('" + localName + "') is not a valid name.");
    return nullptr;
  }

  // 2. localName converted to ASCII lowercase
  const AtomicString& convertedLocalName = convertLocalName(localName);

  bool isV1 = stringOrOptions.isDictionary() || !registrationContext();
  bool createV1Builtin = stringOrOptions.isDictionary() &&
                         RuntimeEnabledFeatures::customElementsBuiltinEnabled();
  bool shouldCreateBuiltin = createV1Builtin || stringOrOptions.isString();

  // 3.
  const AtomicString& is =
      AtomicString(getTypeExtension(this, stringOrOptions, exceptionState));
  const AtomicString& name = shouldCreateBuiltin ? is : convertedLocalName;

  // 4. Let definition be result of lookup up custom element definition
  CustomElementDefinition* definition = nullptr;
  if (isV1) {
    // Is the runtime flag enabled for customized builtin elements?
    const CustomElementDescriptor desc =
        RuntimeEnabledFeatures::customElementsBuiltinEnabled()
            ? CustomElementDescriptor(name, convertedLocalName)
            : CustomElementDescriptor(convertedLocalName, convertedLocalName);
    if (CustomElementRegistry* registry = CustomElement::registry(*this))
      definition = registry->definitionFor(desc);

    // 5. If 'is' is non-null and definition is null, throw NotFoundError
    // TODO(yurak): update when https://github.com/w3c/webcomponents/issues/608
    //              is resolved
    if (!definition && createV1Builtin) {
      exceptionState.throwDOMException(NotFoundError,
                                       "Custom element definition not found.");
      return nullptr;
    }
  }

  // 7. Let element be the result of creating an element
  Element* element;

  if (definition) {
    element = CustomElement::createCustomElementSync(*this, convertedLocalName,
                                                     definition);
  } else if (V0CustomElement::isValidName(localName) && registrationContext()) {
    element = registrationContext()->createCustomTagElement(
        *this, QualifiedName(nullAtom, convertedLocalName, xhtmlNamespaceURI));
  } else {
    element = createElement(localName, exceptionState);
    if (exceptionState.hadException())
      return nullptr;
  }

  // 8. If 'is' is non-null, set 'is' attribute
  if (!is.isEmpty()) {
    if (stringOrOptions.isString()) {
      V0CustomElementRegistrationContext::setIsAttributeAndTypeExtension(
          element, is);
    } else if (stringOrOptions.isDictionary()) {
      element->setAttribute(HTMLNames::isAttr, is);
    }
  }

  return element;
}

static inline QualifiedName createQualifiedName(
    const AtomicString& namespaceURI,
    const AtomicString& qualifiedName,
    ExceptionState& exceptionState) {
  AtomicString prefix, localName;
  if (!Document::parseQualifiedName(qualifiedName, prefix, localName,
                                    exceptionState))
    return QualifiedName::null();

  QualifiedName qName(prefix, localName, namespaceURI);
  if (!Document::hasValidNamespaceForElements(qName)) {
    exceptionState.throwDOMException(
        NamespaceError,
        "The namespace URI provided ('" + namespaceURI +
            "') is not valid for the qualified name provided ('" +
            qualifiedName + "').");
    return QualifiedName::null();
  }

  return qName;
}

Element* Document::createElementNS(const AtomicString& namespaceURI,
                                   const AtomicString& qualifiedName,
                                   ExceptionState& exceptionState) {
  QualifiedName qName(
      createQualifiedName(namespaceURI, qualifiedName, exceptionState));
  if (qName == QualifiedName::null())
    return nullptr;

  if (CustomElement::shouldCreateCustomElement(qName))
    return CustomElement::createCustomElementSync(*this, qName);
  return createElement(qName, CreatedByCreateElement);
}

// https://dom.spec.whatwg.org/#internal-createelementns-steps
Element* Document::createElementNS(const AtomicString& namespaceURI,
                                   const AtomicString& qualifiedName,
                                   const StringOrDictionary& stringOrOptions,
                                   ExceptionState& exceptionState) {
  // 1. Validate and extract
  QualifiedName qName(
      createQualifiedName(namespaceURI, qualifiedName, exceptionState));
  if (qName == QualifiedName::null())
    return nullptr;

  bool isV1 = stringOrOptions.isDictionary() || !registrationContext();
  bool createV1Builtin = stringOrOptions.isDictionary() &&
                         RuntimeEnabledFeatures::customElementsBuiltinEnabled();
  bool shouldCreateBuiltin = createV1Builtin || stringOrOptions.isString();

  // 2.
  const AtomicString& is =
      AtomicString(getTypeExtension(this, stringOrOptions, exceptionState));
  const AtomicString& name = shouldCreateBuiltin ? is : qualifiedName;

  if (!isValidName(qualifiedName)) {
    exceptionState.throwDOMException(
        InvalidCharacterError,
        "The tag name provided ('" + qualifiedName + "') is not a valid name.");
    return nullptr;
  }

  // 3. Let definition be result of lookup up custom element definition
  CustomElementDefinition* definition = nullptr;
  if (isV1) {
    const CustomElementDescriptor desc =
        RuntimeEnabledFeatures::customElementsBuiltinEnabled()
            ? CustomElementDescriptor(name, qualifiedName)
            : CustomElementDescriptor(qualifiedName, qualifiedName);
    if (CustomElementRegistry* registry = CustomElement::registry(*this))
      definition = registry->definitionFor(desc);

    // 4. If 'is' is non-null and definition is null, throw NotFoundError
    if (!definition && createV1Builtin) {
      exceptionState.throwDOMException(NotFoundError,
                                       "Custom element definition not found.");
      return nullptr;
    }
  }

  // 5. Let element be the result of creating an element
  Element* element;

  if (CustomElement::shouldCreateCustomElement(qName) || createV1Builtin) {
    element = CustomElement::createCustomElementSync(*this, qName, definition);
  } else if (V0CustomElement::isValidName(qName.localName()) &&
             registrationContext()) {
    element = registrationContext()->createCustomTagElement(*this, qName);
  } else {
    element = createElement(qName, CreatedByCreateElement);
  }

  // 6. If 'is' is non-null, set 'is' attribute
  if (!is.isEmpty()) {
    if (element->getCustomElementState() != CustomElementState::Custom) {
      V0CustomElementRegistrationContext::setIsAttributeAndTypeExtension(
          element, is);
    } else if (stringOrOptions.isDictionary()) {
      element->setAttribute(HTMLNames::isAttr, is);
    }
  }

  return element;
}

ScriptValue Document::registerElement(ScriptState* scriptState,
                                      const AtomicString& name,
                                      const ElementRegistrationOptions& options,
                                      ExceptionState& exceptionState,
                                      V0CustomElement::NameSet validNames) {
  HostsUsingFeatures::countMainWorldOnly(
      scriptState, *this, HostsUsingFeatures::Feature::DocumentRegisterElement);

  if (!registrationContext()) {
    exceptionState.throwDOMException(
        NotSupportedError, "No element registration context is available.");
    return ScriptValue();
  }

  V0CustomElementConstructorBuilder constructorBuilder(scriptState, options);
  registrationContext()->registerElement(this, &constructorBuilder, name,
                                         validNames, exceptionState);
  return constructorBuilder.bindingsReturnValue();
}

V0CustomElementMicrotaskRunQueue* Document::customElementMicrotaskRunQueue() {
  if (!m_customElementMicrotaskRunQueue)
    m_customElementMicrotaskRunQueue =
        V0CustomElementMicrotaskRunQueue::create();
  return m_customElementMicrotaskRunQueue.get();
}

void Document::clearImportsController() {
  m_importsController = nullptr;
  if (!loader())
    m_fetcher->clearContext();
}

void Document::createImportsController() {
  DCHECK(!m_importsController);
  m_importsController = HTMLImportsController::create(*this);
}

HTMLImportLoader* Document::importLoader() const {
  if (!m_importsController)
    return 0;
  return m_importsController->loaderFor(*this);
}

bool Document::haveImportsLoaded() const {
  if (!m_importsController)
    return true;
  return !m_importsController->shouldBlockScriptExecution(*this);
}

LocalDOMWindow* Document::executingWindow() const {
  if (LocalDOMWindow* owningWindow = domWindow())
    return owningWindow;
  if (HTMLImportsController* import = this->importsController())
    return import->master()->domWindow();
  return 0;
}

LocalFrame* Document::executingFrame() {
  LocalDOMWindow* window = executingWindow();
  if (!window)
    return 0;
  return window->frame();
}

DocumentFragment* Document::createDocumentFragment() {
  return DocumentFragment::create(*this);
}

Text* Document::createTextNode(const String& data) {
  return Text::create(*this, data);
}

Comment* Document::createComment(const String& data) {
  return Comment::create(*this, data);
}

CDATASection* Document::createCDATASection(const String& data,
                                           ExceptionState& exceptionState) {
  if (isHTMLDocument()) {
    exceptionState.throwDOMException(
        NotSupportedError,
        "This operation is not supported for HTML documents.");
    return nullptr;
  }
  if (data.contains("]]>")) {
    exceptionState.throwDOMException(InvalidCharacterError,
                                     "String cannot contain ']]>' since that "
                                     "is the end delimiter of a CData "
                                     "section.");
    return nullptr;
  }
  return CDATASection::create(*this, data);
}

ProcessingInstruction* Document::createProcessingInstruction(
    const String& target,
    const String& data,
    ExceptionState& exceptionState) {
  if (!isValidName(target)) {
    exceptionState.throwDOMException(
        InvalidCharacterError,
        "The target provided ('" + target + "') is not a valid name.");
    return nullptr;
  }
  if (data.contains("?>")) {
    exceptionState.throwDOMException(
        InvalidCharacterError,
        "The data provided ('" + data + "') contains '?>'.");
    return nullptr;
  }
  if (isHTMLDocument()) {
    UseCounter::count(*this,
                      UseCounter::HTMLDocumentCreateProcessingInstruction);
  }
  return ProcessingInstruction::create(*this, target, data);
}

Text* Document::createEditingTextNode(const String& text) {
  return Text::createEditingText(*this, text);
}

bool Document::importContainerNodeChildren(ContainerNode* oldContainerNode,
                                           ContainerNode* newContainerNode,
                                           ExceptionState& exceptionState) {
  for (Node& oldChild : NodeTraversal::childrenOf(*oldContainerNode)) {
    Node* newChild = importNode(&oldChild, true, exceptionState);
    if (exceptionState.hadException())
      return false;
    newContainerNode->appendChild(newChild, exceptionState);
    if (exceptionState.hadException())
      return false;
  }

  return true;
}

Node* Document::importNode(Node* importedNode,
                           bool deep,
                           ExceptionState& exceptionState) {
  switch (importedNode->getNodeType()) {
    case kTextNode:
      return createTextNode(importedNode->nodeValue());
    case kCdataSectionNode:
      return CDATASection::create(*this, importedNode->nodeValue());
    case kProcessingInstructionNode:
      return createProcessingInstruction(
          importedNode->nodeName(), importedNode->nodeValue(), exceptionState);
    case kCommentNode:
      return createComment(importedNode->nodeValue());
    case kDocumentTypeNode: {
      DocumentType* doctype = toDocumentType(importedNode);
      return DocumentType::create(this, doctype->name(), doctype->publicId(),
                                  doctype->systemId());
    }
    case kElementNode: {
      Element* oldElement = toElement(importedNode);
      // FIXME: The following check might be unnecessary. Is it possible that
      // oldElement has mismatched prefix/namespace?
      if (!hasValidNamespaceForElements(oldElement->tagQName())) {
        exceptionState.throwDOMException(
            NamespaceError, "The imported node has an invalid namespace.");
        return nullptr;
      }
      Element* newElement =
          createElement(oldElement->tagQName(), CreatedByImportNode);

      newElement->cloneDataFromElement(*oldElement);

      if (deep) {
        if (!importContainerNodeChildren(oldElement, newElement,
                                         exceptionState))
          return nullptr;
        if (isHTMLTemplateElement(*oldElement) &&
            !ensureTemplateDocument().importContainerNodeChildren(
                toHTMLTemplateElement(oldElement)->content(),
                toHTMLTemplateElement(newElement)->content(), exceptionState))
          return nullptr;
      }

      return newElement;
    }
    case kAttributeNode:
      return Attr::create(
          *this,
          QualifiedName(nullAtom, AtomicString(toAttr(importedNode)->name()),
                        nullAtom),
          toAttr(importedNode)->value());
    case kDocumentFragmentNode: {
      if (importedNode->isShadowRoot()) {
        // ShadowRoot nodes should not be explicitly importable.
        // Either they are imported along with their host node, or created
        // implicitly.
        exceptionState.throwDOMException(
            NotSupportedError,
            "The node provided is a shadow root, which may not be imported.");
        return nullptr;
      }
      DocumentFragment* oldFragment = toDocumentFragment(importedNode);
      DocumentFragment* newFragment = createDocumentFragment();
      if (deep &&
          !importContainerNodeChildren(oldFragment, newFragment,
                                       exceptionState))
        return nullptr;

      return newFragment;
    }
    case kDocumentNode:
      exceptionState.throwDOMException(
          NotSupportedError,
          "The node provided is a document, which may not be imported.");
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

Node* Document::adoptNode(Node* source, ExceptionState& exceptionState) {
  EventQueueScope scope;

  switch (source->getNodeType()) {
    case kDocumentNode:
      exceptionState.throwDOMException(NotSupportedError,
                                       "The node provided is of type '" +
                                           source->nodeName() +
                                           "', which may not be adopted.");
      return nullptr;
    case kAttributeNode: {
      Attr* attr = toAttr(source);
      if (Element* ownerElement = attr->ownerElement())
        ownerElement->removeAttributeNode(attr, exceptionState);
      break;
    }
    default:
      if (source->isShadowRoot()) {
        // ShadowRoot cannot disconnect itself from the host node.
        exceptionState.throwDOMException(
            HierarchyRequestError,
            "The node provided is a shadow root, which may not be adopted.");
        return nullptr;
      }

      if (source->isFrameOwnerElement()) {
        HTMLFrameOwnerElement* frameOwnerElement =
            toHTMLFrameOwnerElement(source);
        if (frame() &&
            frame()->tree().isDescendantOf(frameOwnerElement->contentFrame())) {
          exceptionState.throwDOMException(
              HierarchyRequestError,
              "The node provided is a frame which contains this document.");
          return nullptr;
        }
      }
      if (source->parentNode()) {
        source->parentNode()->removeChild(source, exceptionState);
        if (exceptionState.hadException())
          return nullptr;
        CHECK(!source->parentNode());
      }
  }

  this->adoptIfNeeded(*source);

  return source;
}

bool Document::hasValidNamespaceForElements(const QualifiedName& qName) {
  // These checks are from DOM Core Level 2, createElementNS
  // http://www.w3.org/TR/DOM-Level-2-Core/core.html#ID-DocCrElNS
  // createElementNS(null, "html:div")
  if (!qName.prefix().isEmpty() && qName.namespaceURI().isNull())
    return false;
  // createElementNS("http://www.example.com", "xml:lang")
  if (qName.prefix() == xmlAtom &&
      qName.namespaceURI() != XMLNames::xmlNamespaceURI)
    return false;

  // Required by DOM Level 3 Core and unspecified by DOM Level 2 Core:
  // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core.html#ID-DocCrElNS
  // createElementNS("http://www.w3.org/2000/xmlns/", "foo:bar"),
  // createElementNS(null, "xmlns:bar"), createElementNS(null, "xmlns")
  if (qName.prefix() == xmlnsAtom ||
      (qName.prefix().isEmpty() && qName.localName() == xmlnsAtom))
    return qName.namespaceURI() == XMLNSNames::xmlnsNamespaceURI;
  return qName.namespaceURI() != XMLNSNames::xmlnsNamespaceURI;
}

bool Document::hasValidNamespaceForAttributes(const QualifiedName& qName) {
  return hasValidNamespaceForElements(qName);
}

// FIXME: This should really be in a possible ElementFactory class
Element* Document::createElement(const QualifiedName& qName,
                                 CreateElementFlags flags) {
  Element* e = nullptr;

  // FIXME: Use registered namespaces and look up in a hash to find the right
  // factory.
  if (qName.namespaceURI() == xhtmlNamespaceURI)
    e = HTMLElementFactory::createHTMLElement(qName.localName(), *this, flags);
  else if (qName.namespaceURI() == SVGNames::svgNamespaceURI)
    e = SVGElementFactory::createSVGElement(qName.localName(), *this, flags);

  if (e)
    m_sawElementsInKnownNamespaces = true;
  else
    e = Element::create(qName, this);

  if (e->prefix() != qName.prefix())
    e->setTagNameForCreateElementNS(qName);

  DCHECK(qName == e->tagQName());

  return e;
}

String Document::readyState() const {
  DEFINE_STATIC_LOCAL(const String, loading, ("loading"));
  DEFINE_STATIC_LOCAL(const String, interactive, ("interactive"));
  DEFINE_STATIC_LOCAL(const String, complete, ("complete"));

  switch (m_readyState) {
    case Loading:
      return loading;
    case Interactive:
      return interactive;
    case Complete:
      return complete;
  }

  NOTREACHED();
  return String();
}

void Document::setReadyState(DocumentReadyState readyState) {
  if (readyState == m_readyState)
    return;

  switch (readyState) {
    case Loading:
      if (!m_documentTiming.domLoading()) {
        m_documentTiming.markDomLoading();
      }
      break;
    case Interactive:
      if (!m_documentTiming.domInteractive())
        m_documentTiming.markDomInteractive();
      break;
    case Complete:
      if (!m_documentTiming.domComplete())
        m_documentTiming.markDomComplete();
      break;
  }

  m_readyState = readyState;
  dispatchEvent(Event::create(EventTypeNames::readystatechange));
}

bool Document::isLoadCompleted() {
  return m_readyState == Complete;
}

AtomicString Document::encodingName() const {
  // TextEncoding::name() returns a char*, no need to allocate a new
  // String for it each time.
  // FIXME: We should fix TextEncoding to speak AtomicString anyway.
  return AtomicString(encoding().name());
}

void Document::setContentLanguage(const AtomicString& language) {
  if (m_contentLanguage == language)
    return;
  m_contentLanguage = language;

  // Document's style depends on the content language.
  setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                              StyleChangeReason::Language));
}

void Document::setXMLVersion(const String& version,
                             ExceptionState& exceptionState) {
  if (!XMLDocumentParser::supportsXMLVersion(version)) {
    exceptionState.throwDOMException(
        NotSupportedError,
        "This document does not support the XML version '" + version + "'.");
    return;
  }

  m_xmlVersion = version;
}

void Document::setXMLStandalone(bool standalone,
                                ExceptionState& exceptionState) {
  m_xmlStandalone = standalone ? Standalone : NotStandalone;
}

void Document::setContent(const String& content) {
  open();
  m_parser->append(content);
  close();
}

String Document::suggestedMIMEType() const {
  if (isXMLDocument()) {
    if (isXHTMLDocument())
      return "application/xhtml+xml";
    if (isSVGDocument())
      return "image/svg+xml";
    return "application/xml";
  }
  if (xmlStandalone())
    return "text/xml";
  if (isHTMLDocument())
    return "text/html";

  if (DocumentLoader* documentLoader = loader())
    return documentLoader->responseMIMEType();
  return String();
}

void Document::setMimeType(const AtomicString& mimeType) {
  m_mimeType = mimeType;
}

AtomicString Document::contentType() const {
  if (!m_mimeType.isEmpty())
    return m_mimeType;

  if (DocumentLoader* documentLoader = loader())
    return documentLoader->mimeType();

  String mimeType = suggestedMIMEType();
  if (!mimeType.isEmpty())
    return AtomicString(mimeType);

  return AtomicString("application/xml");
}

Element* Document::elementFromPoint(int x, int y) const {
  if (layoutViewItem().isNull())
    return 0;

  return TreeScope::elementFromPoint(x, y);
}

HeapVector<Member<Element>> Document::elementsFromPoint(int x, int y) const {
  if (layoutViewItem().isNull())
    return HeapVector<Member<Element>>();
  return TreeScope::elementsFromPoint(x, y);
}

Range* Document::caretRangeFromPoint(int x, int y) {
  if (layoutViewItem().isNull())
    return nullptr;

  HitTestResult result = hitTestInDocument(this, x, y);
  PositionWithAffinity positionWithAffinity = result.position();
  if (positionWithAffinity.isNull())
    return nullptr;

  Position rangeCompliantPosition =
      positionWithAffinity.position().parentAnchoredEquivalent();
  return Range::createAdjustedToTreeScope(*this, rangeCompliantPosition);
}

Element* Document::scrollingElement() {
  if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled() && inQuirksMode())
    updateStyleAndLayoutTree();
  return scrollingElementNoLayout();
}

Element* Document::scrollingElementNoLayout() {
  if (RuntimeEnabledFeatures::scrollTopLeftInteropEnabled()) {
    if (inQuirksMode()) {
      DCHECK(m_lifecycle.state() >= DocumentLifecycle::StyleClean);
      HTMLBodyElement* body = firstBodyElement();
      if (body && body->layoutObject() &&
          body->layoutObject()->hasOverflowClip())
        return nullptr;

      return body;
    }

    return documentElement();
  }

  return body();
}

// We use HashMap::set over HashMap::add here as we want to
// replace the ComputedStyle but not the Node if the Node is
// already present.
void Document::addStyleReattachData(const Node& node,
                                    StyleReattachData& styleReattachData) {
  DCHECK(node.isElementNode() || node.isTextNode());
  m_styleReattachDataMap.set(&node, styleReattachData);
}

StyleReattachData Document::getStyleReattachData(const Node& node) const {
  return m_styleReattachDataMap.at(&node);
}

/*
 * Performs three operations:
 *  1. Convert control characters to spaces
 *  2. Trim leading and trailing spaces
 *  3. Collapse internal whitespace.
 */
template <typename CharacterType>
static inline String canonicalizedTitle(Document* document,
                                        const String& title) {
  unsigned length = title.length();
  unsigned builderIndex = 0;
  const CharacterType* characters = title.getCharacters<CharacterType>();

  StringBuffer<CharacterType> buffer(length);

  // Replace control characters with spaces and collapse whitespace.
  bool pendingWhitespace = false;
  for (unsigned i = 0; i < length; ++i) {
    UChar32 c = characters[i];
    if ((c <= spaceCharacter && c != lineTabulationCharacter) ||
        c == deleteCharacter) {
      if (builderIndex != 0)
        pendingWhitespace = true;
    } else {
      if (pendingWhitespace) {
        buffer[builderIndex++] = ' ';
        pendingWhitespace = false;
      }
      buffer[builderIndex++] = c;
    }
  }
  buffer.shrink(builderIndex);

  return String::adopt(buffer);
}

void Document::updateTitle(const String& title) {
  if (m_rawTitle == title)
    return;

  m_rawTitle = title;

  String oldTitle = m_title;
  if (m_rawTitle.isEmpty())
    m_title = String();
  else if (m_rawTitle.is8Bit())
    m_title = canonicalizedTitle<LChar>(this, m_rawTitle);
  else
    m_title = canonicalizedTitle<UChar>(this, m_rawTitle);

  if (!m_frame || oldTitle == m_title)
    return;
  m_frame->loader().client()->dispatchDidReceiveTitle(m_title);
}

void Document::setTitle(const String& title) {
  // Title set by JavaScript -- overrides any title elements.
  if (!m_titleElement) {
    if (isHTMLDocument() || isXHTMLDocument()) {
      HTMLElement* headElement = head();
      if (!headElement)
        return;
      m_titleElement = HTMLTitleElement::create(*this);
      headElement->appendChild(m_titleElement.get());
    } else if (isSVGDocument()) {
      Element* element = documentElement();
      if (!isSVGSVGElement(element))
        return;
      m_titleElement = SVGTitleElement::create(*this);
      element->insertBefore(m_titleElement.get(), element->firstChild());
    }
  } else {
    if (!isHTMLDocument() && !isXHTMLDocument() && !isSVGDocument())
      m_titleElement = nullptr;
  }

  if (isHTMLTitleElement(m_titleElement))
    toHTMLTitleElement(m_titleElement)->setText(title);
  else if (isSVGTitleElement(m_titleElement))
    toSVGTitleElement(m_titleElement)->setText(title);
  else
    updateTitle(title);
}

void Document::setTitleElement(Element* titleElement) {
  // If the root element is an svg element in the SVG namespace, then let value
  // be the child text content of the first title element in the SVG namespace
  // that is a child of the root element.
  if (isSVGSVGElement(documentElement())) {
    m_titleElement = Traversal<SVGTitleElement>::firstChild(*documentElement());
  } else {
    if (m_titleElement && m_titleElement != titleElement)
      m_titleElement = Traversal<HTMLTitleElement>::firstWithin(*this);
    else
      m_titleElement = titleElement;

    // If the root element isn't an svg element in the SVG namespace and the
    // title element is in the SVG namespace, it is ignored.
    if (isSVGTitleElement(m_titleElement)) {
      m_titleElement = nullptr;
      return;
    }
  }

  if (isHTMLTitleElement(m_titleElement))
    updateTitle(toHTMLTitleElement(m_titleElement)->text());
  else if (isSVGTitleElement(m_titleElement))
    updateTitle(toSVGTitleElement(m_titleElement)->textContent());
}

void Document::removeTitle(Element* titleElement) {
  if (m_titleElement != titleElement)
    return;

  m_titleElement = nullptr;

  // Update title based on first title element in the document, if one exists.
  if (isHTMLDocument() || isXHTMLDocument()) {
    if (HTMLTitleElement* title =
            Traversal<HTMLTitleElement>::firstWithin(*this))
      setTitleElement(title);
  } else if (isSVGDocument()) {
    if (SVGTitleElement* title = Traversal<SVGTitleElement>::firstWithin(*this))
      setTitleElement(title);
  }

  if (!m_titleElement)
    updateTitle(String());
}

const AtomicString& Document::dir() {
  Element* rootElement = documentElement();
  if (isHTMLHtmlElement(rootElement))
    return toHTMLHtmlElement(rootElement)->dir();
  return nullAtom;
}

void Document::setDir(const AtomicString& value) {
  Element* rootElement = documentElement();
  if (isHTMLHtmlElement(rootElement))
    toHTMLHtmlElement(rootElement)->setDir(value);
}

PageVisibilityState Document::pageVisibilityState() const {
  // The visibility of the document is inherited from the visibility of the
  // page. If there is no page associated with the document, we will assume
  // that the page is hidden, as specified by the spec:
  // https://w3c.github.io/page-visibility/#hidden-attribute
  if (!m_frame || !m_frame->page())
    return PageVisibilityStateHidden;
  // While visibilitychange is being dispatched during unloading it is
  // expected that the visibility is hidden regardless of the page's
  // visibility.
  if (m_loadEventProgress >= UnloadVisibilityChangeInProgress)
    return PageVisibilityStateHidden;
  return m_frame->page()->visibilityState();
}

bool Document::isPrefetchOnly() const {
  if (!m_frame || !m_frame->page())
    return false;

  PrerendererClient* prerendererClient =
      PrerendererClient::from(m_frame->page());
  return prerendererClient && prerendererClient->isPrefetchOnly();
}

String Document::visibilityState() const {
  return pageVisibilityStateString(pageVisibilityState());
}

bool Document::hidden() const {
  return pageVisibilityState() != PageVisibilityStateVisible;
}

void Document::didChangeVisibilityState() {
  dispatchEvent(Event::createBubble(EventTypeNames::visibilitychange));
  // Also send out the deprecated version until it can be removed.
  dispatchEvent(Event::createBubble(EventTypeNames::webkitvisibilitychange));

  if (pageVisibilityState() == PageVisibilityStateVisible)
    timeline().setAllCompositorPending();

  if (hidden() && m_canvasFontCache)
    m_canvasFontCache->pruneAll();
}

String Document::nodeName() const {
  return "#document";
}

Node::NodeType Document::getNodeType() const {
  return kDocumentNode;
}

FormController& Document::formController() {
  if (!m_formController) {
    m_formController = FormController::create();
    if (m_frame && m_frame->loader().currentItem() &&
        m_frame->loader().currentItem()->isCurrentDocument(this))
      m_frame->loader().currentItem()->setDocumentState(
          m_formController->formElementsState());
  }
  return *m_formController;
}

DocumentState* Document::formElementsState() const {
  if (!m_formController)
    return 0;
  return m_formController->formElementsState();
}

void Document::setStateForNewFormElements(const Vector<String>& stateVector) {
  if (!stateVector.size() && !m_formController)
    return;
  formController().setStateForNewFormElements(stateVector);
}

FrameView* Document::view() const {
  return m_frame ? m_frame->view() : nullptr;
}

Page* Document::page() const {
  return m_frame ? m_frame->page() : nullptr;
}

FrameHost* Document::frameHost() const {
  return m_frame ? m_frame->host() : nullptr;
}

Settings* Document::settings() const {
  return m_frame ? m_frame->settings() : nullptr;
}

Range* Document::createRange() {
  return Range::create(*this);
}

NodeIterator* Document::createNodeIterator(Node* root,
                                           unsigned whatToShow,
                                           NodeFilter* filter) {
  DCHECK(root);
  return NodeIterator::create(root, whatToShow, filter);
}

TreeWalker* Document::createTreeWalker(Node* root,
                                       unsigned whatToShow,
                                       NodeFilter* filter) {
  DCHECK(root);
  return TreeWalker::create(root, whatToShow, filter);
}

bool Document::needsLayoutTreeUpdate() const {
  if (!isActive() || !view())
    return false;
  if (needsFullLayoutTreeUpdate())
    return true;
  if (childNeedsStyleRecalc())
    return true;
  if (childNeedsStyleInvalidation())
    return true;
  if (layoutViewItem().wasNotifiedOfSubtreeChange())
    return true;
  return false;
}

bool Document::needsFullLayoutTreeUpdate() const {
  if (!isActive() || !view())
    return false;
  if (m_styleEngine->needsActiveStyleUpdate())
    return true;
  if (!m_useElementsNeedingUpdate.isEmpty())
    return true;
  if (needsStyleRecalc())
    return true;
  if (needsStyleInvalidation())
    return true;
  // FIXME: The childNeedsDistributionRecalc bit means either self or children,
  // we should fix that.
  if (childNeedsDistributionRecalc())
    return true;
  if (DocumentAnimations::needsAnimationTimingUpdate(*this))
    return true;
  return false;
}

bool Document::shouldScheduleLayoutTreeUpdate() const {
  if (!isActive())
    return false;
  if (inStyleRecalc())
    return false;
  // InPreLayout will recalc style itself. There's no reason to schedule another
  // recalc.
  if (m_lifecycle.state() == DocumentLifecycle::InPreLayout)
    return false;
  if (!shouldScheduleLayout())
    return false;
  return true;
}

void Document::scheduleLayoutTreeUpdate() {
  DCHECK(!hasPendingVisualUpdate());
  DCHECK(shouldScheduleLayoutTreeUpdate());
  DCHECK(needsLayoutTreeUpdate());

  if (!view()->canThrottleRendering())
    page()->animator().scheduleVisualUpdate(frame());
  m_lifecycle.ensureStateAtMost(DocumentLifecycle::VisualUpdatePending);

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "ScheduleStyleRecalculation", TRACE_EVENT_SCOPE_THREAD,
                       "data", InspectorRecalculateStylesEvent::data(frame()));
  ++m_styleVersion;
}

bool Document::hasPendingForcedStyleRecalc() const {
  return hasPendingVisualUpdate() && !inStyleRecalc() &&
         getStyleChangeType() >= SubtreeStyleChange;
}

void Document::updateStyleInvalidationIfNeeded() {
  DCHECK(isActive());
  ScriptForbiddenScope forbidScript;

  if (!childNeedsStyleInvalidation() && !needsStyleInvalidation())
    return;
  TRACE_EVENT0("blink", "Document::updateStyleInvalidationIfNeeded");
  styleEngine().styleInvalidator().invalidate(*this);
}

void Document::setupFontBuilder(ComputedStyle& documentStyle) {
  FontBuilder fontBuilder(*this);
  CSSFontSelector* selector = styleEngine().fontSelector();
  fontBuilder.createFontForDocument(selector, documentStyle);
}

void Document::inheritHtmlAndBodyElementStyles(StyleRecalcChange change) {
  DCHECK(inStyleRecalc());
  DCHECK(documentElement());

  bool didRecalcDocumentElement = false;
  RefPtr<ComputedStyle> documentElementStyle =
      documentElement()->mutableComputedStyle();
  if (change == Force)
    documentElement()->clearAnimationStyleChange();
  if (!documentElementStyle || documentElement()->needsStyleRecalc() ||
      change == Force) {
    documentElementStyle =
        ensureStyleResolver().styleForElement(documentElement());
    didRecalcDocumentElement = true;
  }

  WritingMode rootWritingMode = documentElementStyle->getWritingMode();
  TextDirection rootDirection = documentElementStyle->direction();

  HTMLElement* body = this->body();
  RefPtr<ComputedStyle> bodyStyle;

  if (body) {
    bodyStyle = body->mutableComputedStyle();
    if (didRecalcDocumentElement)
      body->clearAnimationStyleChange();
    if (!bodyStyle || body->needsStyleRecalc() || didRecalcDocumentElement) {
      bodyStyle = ensureStyleResolver().styleForElement(
          body, documentElementStyle.get(), documentElementStyle.get());
    }
    rootWritingMode = bodyStyle->getWritingMode();
    rootDirection = bodyStyle->direction();
  }

  const ComputedStyle* backgroundStyle = documentElementStyle.get();
  // http://www.w3.org/TR/css3-background/#body-background
  // <html> root element with no background steals background from its first
  // <body> child.
  // Also see LayoutBoxModelObject::backgroundStolenForBeingBody()
  if (isHTMLHtmlElement(documentElement()) && isHTMLBodyElement(body) &&
      !backgroundStyle->hasBackground())
    backgroundStyle = bodyStyle.get();
  Color backgroundColor =
      backgroundStyle->visitedDependentColor(CSSPropertyBackgroundColor);
  FillLayer backgroundLayers = backgroundStyle->backgroundLayers();
  for (auto currentLayer = &backgroundLayers; currentLayer;
       currentLayer = currentLayer->next()) {
    // http://www.w3.org/TR/css3-background/#root-background
    // The root element background always have painting area of the whole
    // canvas.
    currentLayer->setClip(BorderFillBox);

    // The root element doesn't scroll. It always propagates its layout overflow
    // to the viewport. Positioning background against either box is equivalent
    // to positioning against the scrolled box of the viewport.
    if (currentLayer->attachment() == ScrollBackgroundAttachment)
      currentLayer->setAttachment(LocalBackgroundAttachment);
  }
  EImageRendering imageRendering = backgroundStyle->imageRendering();

  const ComputedStyle* overflowStyle = nullptr;
  if (Element* element = viewportDefiningElement(documentElementStyle.get())) {
    if (element == body) {
      overflowStyle = bodyStyle.get();
    } else {
      DCHECK_EQ(element, documentElement());
      overflowStyle = documentElementStyle.get();

      // The body element has its own scrolling box, independent from the
      // viewport.  This is a bit of a weird edge case in the CSS spec that we
      // might want to try to eliminate some day (eg. for ScrollTopLeftInterop -
      // see http://crbug.com/157855).
      if (bodyStyle && !bodyStyle->isOverflowVisible())
        UseCounter::count(*this, UseCounter::BodyScrollsInAdditionToViewport);
    }
  }

  // Resolved rem units are stored in the matched properties cache so we need to
  // make sure to invalidate the cache if the documentElement needed to reattach
  // or the font size changed and then trigger a full document recalc. We also
  // need to clear it here since the call to styleForElement on the body above
  // can cache bad values for rem units if the documentElement's style was
  // dirty. We could keep track of which elements depend on rem units like we do
  // for viewport styles, but we assume root font size changes are rare and just
  // invalidate the cache for now.
  if (styleEngine().usesRemUnits() &&
      (documentElement()->needsAttach() ||
       !documentElement()->computedStyle() ||
       documentElement()->computedStyle()->fontSize() !=
           documentElementStyle->fontSize())) {
    ensureStyleResolver().invalidateMatchedPropertiesCache();
    documentElement()->setNeedsStyleRecalc(
        SubtreeStyleChange,
        StyleChangeReasonForTracing::create(StyleChangeReason::FontSizeChange));
  }

  EOverflowAnchor overflowAnchor = EOverflowAnchor::kAuto;
  EOverflow overflowX = EOverflow::kAuto;
  EOverflow overflowY = EOverflow::kAuto;
  float columnGap = 0;
  if (overflowStyle) {
    overflowAnchor = overflowStyle->overflowAnchor();
    overflowX = overflowStyle->overflowX();
    overflowY = overflowStyle->overflowY();
    // Visible overflow on the viewport is meaningless, and the spec says to
    // treat it as 'auto':
    if (overflowX == EOverflow::kVisible)
      overflowX = EOverflow::kAuto;
    if (overflowY == EOverflow::kVisible)
      overflowY = EOverflow::kAuto;
    if (overflowAnchor == EOverflowAnchor::kVisible)
      overflowAnchor = EOverflowAnchor::kAuto;
    // Column-gap is (ab)used by the current paged overflow implementation (in
    // lack of other ways to specify gaps between pages), so we have to
    // propagate it too.
    columnGap = overflowStyle->columnGap();
  }

  ScrollSnapType snapType = overflowStyle->getScrollSnapType();
  const LengthPoint& snapDestination = overflowStyle->scrollSnapDestination();

  RefPtr<ComputedStyle> documentStyle = layoutViewItem().mutableStyle();
  if (documentStyle->getWritingMode() != rootWritingMode ||
      documentStyle->direction() != rootDirection ||
      documentStyle->visitedDependentColor(CSSPropertyBackgroundColor) !=
          backgroundColor ||
      documentStyle->backgroundLayers() != backgroundLayers ||
      documentStyle->imageRendering() != imageRendering ||
      documentStyle->overflowAnchor() != overflowAnchor ||
      documentStyle->overflowX() != overflowX ||
      documentStyle->overflowY() != overflowY ||
      documentStyle->columnGap() != columnGap ||
      documentStyle->getScrollSnapType() != snapType ||
      documentStyle->scrollSnapDestination() != snapDestination) {
    RefPtr<ComputedStyle> newStyle = ComputedStyle::clone(*documentStyle);
    newStyle->setWritingMode(rootWritingMode);
    newStyle->setDirection(rootDirection);
    newStyle->setBackgroundColor(backgroundColor);
    newStyle->accessBackgroundLayers() = backgroundLayers;
    newStyle->setImageRendering(imageRendering);
    newStyle->setOverflowAnchor(overflowAnchor);
    newStyle->setOverflowX(overflowX);
    newStyle->setOverflowY(overflowY);
    newStyle->setColumnGap(columnGap);
    newStyle->setScrollSnapType(snapType);
    newStyle->setScrollSnapDestination(snapDestination);
    layoutViewItem().setStyle(newStyle);
    setupFontBuilder(*newStyle);
  }

  if (body) {
    if (const ComputedStyle* style = body->computedStyle()) {
      if (style->direction() != rootDirection ||
          style->getWritingMode() != rootWritingMode)
        body->setNeedsStyleRecalc(SubtreeStyleChange,
                                  StyleChangeReasonForTracing::create(
                                      StyleChangeReason::WritingModeChange));
    }
  }

  if (const ComputedStyle* style = documentElement()->computedStyle()) {
    if (style->direction() != rootDirection ||
        style->getWritingMode() != rootWritingMode)
      documentElement()->setNeedsStyleRecalc(
          SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                  StyleChangeReason::WritingModeChange));
  }
}

#if DCHECK_IS_ON()
static void assertLayoutTreeUpdated(Node& root) {
  for (Node& node : NodeTraversal::inclusiveDescendantsOf(root)) {
    // We leave some nodes with dirty bits in the tree because they don't
    // matter like Comment and ProcessingInstruction nodes.
    // TODO(esprehn): Don't even mark those nodes as needing recalcs in the
    // first place.
    if (!node.isElementNode() && !node.isTextNode() && !node.isShadowRoot() &&
        !node.isDocumentNode())
      continue;
    DCHECK(!node.needsStyleRecalc());
    DCHECK(!node.childNeedsStyleRecalc());
    DCHECK(!node.needsReattachLayoutTree());
    DCHECK(!node.childNeedsReattachLayoutTree());
    DCHECK(!node.childNeedsDistributionRecalc());
    DCHECK(!node.needsStyleInvalidation());
    DCHECK(!node.childNeedsStyleInvalidation());
    for (ShadowRoot* shadowRoot = node.youngestShadowRoot(); shadowRoot;
         shadowRoot = shadowRoot->olderShadowRoot())
      assertLayoutTreeUpdated(*shadowRoot);
  }
}
#endif

void Document::updateStyleAndLayoutTree() {
  DCHECK(isMainThread());

  ScriptForbiddenScope forbidScript;
  // We should forbid script execution for plugins here because update while
  // layout is changing, HTMLPlugin element can be reattached and plugin can be
  // destroyed. Plugin can execute scripts on destroy. It produces crash without
  // PluginScriptForbiddenScope: crbug.com/550427.
  PluginScriptForbiddenScope pluginForbidScript;

  if (!view() || !isActive())
    return;

  if (view()->shouldThrottleRendering())
    return;

  if (!needsLayoutTreeUpdate()) {
    if (lifecycle().state() < DocumentLifecycle::StyleClean) {
      // needsLayoutTreeUpdate may change to false without any actual layout
      // tree update.  For example, needsAnimationTimingUpdate may change to
      // false when time elapses.  Advance lifecycle to StyleClean because style
      // is actually clean now.
      lifecycle().advanceTo(DocumentLifecycle::InStyleRecalc);
      lifecycle().advanceTo(DocumentLifecycle::StyleClean);
    }
    return;
  }

  if (inStyleRecalc())
    return;

  // Entering here from inside layout, paint etc. would be catastrophic since
  // recalcStyle can tear down the layout tree or (unfortunately) run
  // script. Kill the whole layoutObject if someone managed to get into here in
  // states not allowing tree mutations.
  CHECK(lifecycle().stateAllowsTreeMutations());

  TRACE_EVENT_BEGIN1("blink,devtools.timeline", "UpdateLayoutTree", "beginData",
                     InspectorRecalculateStylesEvent::data(frame()));

  unsigned startElementCount = styleEngine().styleForElementCount();

  probe::RecalculateStyle recalculateStyleScope(this);

  DocumentAnimations::updateAnimationTimingIfNeeded(*this);
  evaluateMediaQueryListIfNeeded();
  updateUseShadowTreesIfNeeded();
  updateDistribution();
  updateActiveStyle();
  updateStyleInvalidationIfNeeded();

  // FIXME: We should update style on our ancestor chain before proceeding
  // however doing so currently causes several tests to crash, as
  // LocalFrame::setDocument calls Document::attach before setting the
  // LocalDOMWindow on the LocalFrame, or the SecurityOrigin on the
  // document. The attach, in turn resolves style (here) and then when we
  // resolve style on the parent chain, we may end up re-attaching our
  // containing iframe, which when asked HTMLFrameElementBase::isURLAllowed hits
  // a null-dereference due to security code always assuming the document has a
  // SecurityOrigin.

  updateStyle();

  notifyLayoutTreeOfSubtreeChanges();

  // As a result of the style recalculation, the currently hovered element might
  // have been detached (for example, by setting display:none in the :hover
  // style), schedule another mouseMove event to check if any other elements
  // ended up under the mouse pointer due to re-layout.
  if (hoverNode() && !hoverNode()->layoutObject() && frame())
    frame()->eventHandler().dispatchFakeMouseMoveEventSoon();

  if (m_focusedElement && !m_focusedElement->isFocusable())
    clearFocusedElementSoon();
  layoutViewItem().clearHitTestCache();

  DCHECK(!DocumentAnimations::needsAnimationTimingUpdate(*this));

  unsigned elementCount =
      styleEngine().styleForElementCount() - startElementCount;

  TRACE_EVENT_END1("blink,devtools.timeline", "UpdateLayoutTree",
                   "elementCount", elementCount);

#if DCHECK_IS_ON()
  assertLayoutTreeUpdated(*this);
#endif
}

void Document::updateActiveStyle() {
  DCHECK(isActive());
  DCHECK(isMainThread());
  TRACE_EVENT0("blink", "Document::updateActiveStyle");
  styleEngine().updateActiveStyle();
}

void Document::updateStyle() {
  DCHECK(!view()->shouldThrottleRendering());
  TRACE_EVENT_BEGIN0("blink,blink_style", "Document::updateStyle");
  double startTime = monotonicallyIncreasingTime();

  unsigned initialElementCount = styleEngine().styleForElementCount();

  HTMLFrameOwnerElement::UpdateSuspendScope
      suspendFrameViewBaseHierarchyUpdates;
  m_lifecycle.advanceTo(DocumentLifecycle::InStyleRecalc);

  StyleRecalcChange change = NoChange;
  if (getStyleChangeType() >= SubtreeStyleChange)
    change = Force;

  NthIndexCache nthIndexCache(*this);

  // FIXME: Cannot access the ensureStyleResolver() before calling
  // styleForDocument below because apparently the StyleResolver's constructor
  // has side effects. We should fix it.  See printing/setPrinting.html,
  // printing/width-overflow.html though they only fail on mac when accessing
  // the resolver by what appears to be a viewport size difference.

  if (change == Force) {
    m_hasNodesWithPlaceholderStyle = false;
    RefPtr<ComputedStyle> documentStyle =
        StyleResolver::styleForDocument(*this);
    StyleRecalcChange localChange = ComputedStyle::stylePropagationDiff(
        documentStyle.get(), layoutViewItem().style());
    if (localChange != NoChange)
      layoutViewItem().setStyle(std::move(documentStyle));
  }

  clearNeedsStyleRecalc();
  clearNeedsReattachLayoutTree();

  StyleResolver& resolver = ensureStyleResolver();

  bool shouldRecordStats;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink,blink_style", &shouldRecordStats);
  styleEngine().setStatsEnabled(shouldRecordStats);

  if (Element* documentElement = this->documentElement()) {
    inheritHtmlAndBodyElementStyles(change);
    if (documentElement->shouldCallRecalcStyle(change))
      documentElement->recalcStyle(change);
  }

  view()->recalcOverflowAfterStyleChange();

  // Only retain the HashMap for the duration of StyleRecalc and
  // LayoutTreeConstruction.
  m_styleReattachDataMap.clear();
  clearChildNeedsStyleRecalc();
  clearChildNeedsReattachLayoutTree();

  resolver.clearStyleSharingList();

  DCHECK(!needsStyleRecalc());
  DCHECK(!childNeedsStyleRecalc());
  DCHECK(!needsReattachLayoutTree());
  DCHECK(!childNeedsReattachLayoutTree());
  DCHECK(inStyleRecalc());
  DCHECK_EQ(styleResolver(), &resolver);
  DCHECK(m_styleReattachDataMap.isEmpty());
  m_lifecycle.advanceTo(DocumentLifecycle::StyleClean);
  if (shouldRecordStats) {
    TRACE_EVENT_END2("blink,blink_style", "Document::updateStyle",
                     "resolverAccessCount",
                     styleEngine().styleForElementCount() - initialElementCount,
                     "counters", styleEngine().stats()->toTracedValue());
  } else {
    TRACE_EVENT_END1(
        "blink,blink_style", "Document::updateStyle", "resolverAccessCount",
        styleEngine().styleForElementCount() - initialElementCount);
  }

  double updateDurationSeconds = monotonicallyIncreasingTime() - startTime;
  DEFINE_STATIC_LOCAL(CustomCountHistogram, updateHistogram,
                      ("Style.UpdateTime", 0, 10000000, 50));
  updateHistogram.count(updateDurationSeconds * 1000 * 1000);
  CSSTiming::from(*this).recordUpdateDuration(updateDurationSeconds);
}

void Document::notifyLayoutTreeOfSubtreeChanges() {
  if (!layoutViewItem().wasNotifiedOfSubtreeChange())
    return;

  m_lifecycle.advanceTo(DocumentLifecycle::InLayoutSubtreeChange);

  layoutViewItem().handleSubtreeModifications();
  DCHECK(!layoutViewItem().wasNotifiedOfSubtreeChange());

  m_lifecycle.advanceTo(DocumentLifecycle::LayoutSubtreeChangeClean);
}

bool Document::needsLayoutTreeUpdateForNode(const Node& node) const {
  if (!node.canParticipateInFlatTree())
    return false;
  if (!needsLayoutTreeUpdate())
    return false;
  if (!node.isConnected())
    return false;

  if (needsFullLayoutTreeUpdate() || node.needsStyleRecalc() ||
      node.needsStyleInvalidation())
    return true;
  for (const ContainerNode* ancestor = LayoutTreeBuilderTraversal::parent(node);
       ancestor; ancestor = LayoutTreeBuilderTraversal::parent(*ancestor)) {
    if (ancestor->needsStyleRecalc() || ancestor->needsStyleInvalidation() ||
        ancestor->needsAdjacentStyleRecalc())
      return true;
  }
  return false;
}

void Document::updateStyleAndLayoutTreeForNode(const Node* node) {
  DCHECK(node);
  if (!needsLayoutTreeUpdateForNode(*node))
    return;
  updateStyleAndLayoutTree();
}

void Document::updateStyleAndLayoutIgnorePendingStylesheetsForNode(Node* node) {
  DCHECK(node);
  if (!node->inActiveDocument())
    return;
  updateStyleAndLayoutIgnorePendingStylesheets();
}

void Document::updateStyleAndLayout() {
  DCHECK(isMainThread());

  ScriptForbiddenScope forbidScript;

  FrameView* frameView = view();
  if (frameView && frameView->isInPerformLayout()) {
    // View layout should not be re-entrant.
    NOTREACHED();
    return;
  }

  if (HTMLFrameOwnerElement* owner = localOwner())
    owner->document().updateStyleAndLayout();

  updateStyleAndLayoutTree();

  if (!isActive())
    return;

  if (frameView->needsLayout())
    frameView->layout();

  if (lifecycle().state() < DocumentLifecycle::LayoutClean)
    lifecycle().advanceTo(DocumentLifecycle::LayoutClean);

  if (FrameView* frameView = view())
    frameView->performScrollAnchoringAdjustments();
}

void Document::layoutUpdated() {
  // Plugins can run script inside layout which can detach the page.
  // TODO(esprehn): Can this still happen now that all plugins are out of
  // process?
  if (frame() && frame()->page())
    frame()->page()->chromeClient().layoutUpdated(frame());

  markers().invalidateRectsForAllMarkers();

  // The layout system may perform layouts with pending stylesheets. When
  // recording first layout time, we ignore these layouts, since painting is
  // suppressed for them. We're interested in tracking the time of the
  // first real or 'paintable' layout.
  // TODO(esprehn): This doesn't really make sense, why not track the first
  // beginFrame? This will catch the first layout in a page that does lots
  // of layout thrashing even though that layout might not be followed by
  // a paint for many seconds.
  if (isRenderingReady() && body() &&
      !styleEngine().hasPendingScriptBlockingSheets()) {
    if (!m_documentTiming.firstLayout())
      m_documentTiming.markFirstLayout();
  }

  m_rootScrollerController->didUpdateLayout();
}

void Document::clearFocusedElementSoon() {
  if (!m_clearFocusedElementTimer.isActive())
    m_clearFocusedElementTimer.startOneShot(0, BLINK_FROM_HERE);
}

void Document::clearFocusedElementTimerFired(TimerBase*) {
  updateStyleAndLayoutTree();

  if (m_focusedElement && !m_focusedElement->isFocusable())
    m_focusedElement->blur();
}

// FIXME: This is a bad idea and needs to be removed eventually.
// Other browsers load stylesheets before they continue parsing the web page.
// Since we don't, we can run JavaScript code that needs answers before the
// stylesheets are loaded. Doing a layout ignoring the pending stylesheets
// lets us get reasonable answers. The long term solution to this problem is
// to instead suspend JavaScript execution.
void Document::updateStyleAndLayoutTreeIgnorePendingStylesheets() {
  StyleEngine::IgnoringPendingStylesheet ignoring(styleEngine());

  if (styleEngine().hasPendingScriptBlockingSheets()) {
    // FIXME: We are willing to attempt to suppress painting with outdated style
    // info only once.  Our assumption is that it would be dangerous to try to
    // stop it a second time, after page content has already been loaded and
    // displayed with accurate style information. (Our suppression involves
    // blanking the whole page at the moment. If it were more refined, we might
    // be able to do something better.) It's worth noting though that this
    // entire method is a hack, since what we really want to do is suspend JS
    // instead of doing a layout with inaccurate information.
    HTMLElement* bodyElement = body();
    if (bodyElement && !bodyElement->layoutObject() &&
        m_pendingSheetLayout == NoLayoutWithPendingSheets) {
      m_pendingSheetLayout = DidLayoutWithPendingSheets;
      styleEngine().markAllTreeScopesDirty();
    }
    if (m_hasNodesWithPlaceholderStyle) {
      // If new nodes have been added or style recalc has been done with style
      // sheets still pending, some nodes may not have had their real style
      // calculated yet.  Normally this gets cleaned when style sheets arrive
      // but here we need up-to-date style immediately.
      setNeedsStyleRecalc(SubtreeStyleChange,
                          StyleChangeReasonForTracing::create(
                              StyleChangeReason::CleanupPlaceholderStyles));
    }
  }
  updateStyleAndLayoutTree();
}

void Document::updateStyleAndLayoutIgnorePendingStylesheets(
    Document::RunPostLayoutTasks runPostLayoutTasks) {
  updateStyleAndLayoutTreeIgnorePendingStylesheets();
  updateStyleAndLayout();

  if (runPostLayoutTasks == RunPostLayoutTasksSynchronously && view())
    view()->flushAnyPendingPostLayoutTasks();
}

PassRefPtr<ComputedStyle> Document::styleForElementIgnoringPendingStylesheets(
    Element* element) {
  DCHECK_EQ(element->document(), this);
  StyleEngine::IgnoringPendingStylesheet ignoring(styleEngine());
  if (!element->canParticipateInFlatTree())
    return ensureStyleResolver().styleForElement(element, nullptr);

  ContainerNode* parent = LayoutTreeBuilderTraversal::parent(*element);
  const ComputedStyle* parentStyle =
      parent ? parent->ensureComputedStyle() : nullptr;

  ContainerNode* layoutParent =
      parent ? LayoutTreeBuilderTraversal::layoutParent(*element) : nullptr;
  const ComputedStyle* layoutParentStyle =
      layoutParent ? layoutParent->ensureComputedStyle() : parentStyle;

  return ensureStyleResolver().styleForElement(element, parentStyle,
                                               layoutParentStyle);
}

PassRefPtr<ComputedStyle> Document::styleForPage(int pageIndex) {
  updateDistribution();
  return ensureStyleResolver().styleForPage(pageIndex);
}

bool Document::isPageBoxVisible(int pageIndex) {
  return styleForPage(pageIndex)->visibility() !=
         EVisibility::kHidden;  // display property doesn't apply to @page.
}

void Document::pageSizeAndMarginsInPixels(int pageIndex,
                                          DoubleSize& pageSize,
                                          int& marginTop,
                                          int& marginRight,
                                          int& marginBottom,
                                          int& marginLeft) {
  RefPtr<ComputedStyle> style = styleForPage(pageIndex);

  double width = pageSize.width();
  double height = pageSize.height();
  switch (style->getPageSizeType()) {
    case PAGE_SIZE_AUTO:
      break;
    case PAGE_SIZE_AUTO_LANDSCAPE:
      if (width < height)
        std::swap(width, height);
      break;
    case PAGE_SIZE_AUTO_PORTRAIT:
      if (width > height)
        std::swap(width, height);
      break;
    case PAGE_SIZE_RESOLVED: {
      FloatSize size = style->pageSize();
      width = size.width();
      height = size.height();
      break;
    }
    default:
      NOTREACHED();
  }
  pageSize = DoubleSize(width, height);

  // The percentage is calculated with respect to the width even for margin top
  // and bottom.
  // http://www.w3.org/TR/CSS2/box.html#margin-properties
  marginTop = style->marginTop().isAuto()
                  ? marginTop
                  : intValueForLength(style->marginTop(), width);
  marginRight = style->marginRight().isAuto()
                    ? marginRight
                    : intValueForLength(style->marginRight(), width);
  marginBottom = style->marginBottom().isAuto()
                     ? marginBottom
                     : intValueForLength(style->marginBottom(), width);
  marginLeft = style->marginLeft().isAuto()
                   ? marginLeft
                   : intValueForLength(style->marginLeft(), width);
}

void Document::setIsViewSource(bool isViewSource) {
  m_isViewSource = isViewSource;
  if (!m_isViewSource)
    return;

  setSecurityOrigin(SecurityOrigin::createUnique());
  didUpdateSecurityOrigin();
}

void Document::scheduleUseShadowTreeUpdate(SVGUseElement& element) {
  m_useElementsNeedingUpdate.insert(&element);
  scheduleLayoutTreeUpdateIfNeeded();
}

void Document::unscheduleUseShadowTreeUpdate(SVGUseElement& element) {
  m_useElementsNeedingUpdate.erase(&element);
}

void Document::updateUseShadowTreesIfNeeded() {
  ScriptForbiddenScope forbidScript;

  if (m_useElementsNeedingUpdate.isEmpty())
    return;

  HeapHashSet<Member<SVGUseElement>> elements;
  m_useElementsNeedingUpdate.swap(elements);
  for (SVGUseElement* element : elements)
    element->buildPendingResource();
}

StyleResolver* Document::styleResolver() const {
  return m_styleEngine->resolver();
}

StyleResolver& Document::ensureStyleResolver() const {
  return m_styleEngine->ensureResolver();
}

void Document::initialize() {
  DCHECK_EQ(m_lifecycle.state(), DocumentLifecycle::Inactive);
  DCHECK(!m_axObjectCache || this != &axObjectCacheOwner());

  m_layoutView = new LayoutView(this);
  setLayoutObject(m_layoutView);

  m_layoutView->setIsInWindow(true);
  m_layoutView->setStyle(StyleResolver::styleForDocument(*this));
  m_layoutView->compositor()->setNeedsCompositingUpdate(
      CompositingUpdateAfterCompositingInputChange);

  ContainerNode::attachLayoutTree();

  // The TextAutosizer can't update layout view info while the Document is
  // detached, so update now in case anything changed.
  if (TextAutosizer* autosizer = textAutosizer())
    autosizer->updatePageInfo();

  m_frame->documentAttached();
  m_lifecycle.advanceTo(DocumentLifecycle::StyleClean);

  if (view())
    view()->didAttachDocument();

  // Observer(s) should not be initialized until the document is initialized /
  // attached to a frame. Otherwise ContextLifecycleObserver::contextDestroyed
  // wouldn't be fired.
  m_networkStateObserver = new NetworkStateObserver(*this);
}

void Document::shutdown() {
  TRACE_EVENT0("blink", "Document::shutdown");
  CHECK(!m_frame || m_frame->tree().childCount() == 0);
  if (!isActive())
    return;

  // Frame navigation can cause a new Document to be attached. Don't allow that,
  // since that will cause a situation where LocalFrame still has a Document
  // attached after this finishes!  Normally, it shouldn't actually be possible
  // to trigger navigation here.  However, plugins (see below) can cause lots of
  // crazy things to happen, since plugin detach involves nested message loops.
  FrameNavigationDisabler navigationDisabler(*m_frame);
  // Defer FrameViewBase updates to avoid plugins trying to run script inside
  // ScriptForbiddenScope, which will crash the renderer after
  // https://crrev.com/200984
  HTMLFrameOwnerElement::UpdateSuspendScope
      suspendFrameViewBaseHierarchyUpdates;
  // Don't allow script to run in the middle of detachLayoutTree() because a
  // detaching Document is not in a consistent state.
  ScriptForbiddenScope forbidScript;

  view()->dispose();

  // If the FrameViewBase of the document's frame owner doesn't match view()
  // then FrameView::dispose() didn't clear the owner's FrameViewBase. If we
  // don't clear it here, it may be clobbered later in LocalFrame::createView().
  // See also https://crbug.com/673170 and the comment in FrameView::dispose().
  HTMLFrameOwnerElement* ownerElement = m_frame->deprecatedLocalOwner();
  if (ownerElement)
    ownerElement->setWidget(nullptr);

  m_markers->prepareForDestruction();

  m_lifecycle.advanceTo(DocumentLifecycle::Stopping);

  if (page())
    page()->documentDetached(this);
  probe::documentDetached(this);

  if (m_frame->loader().client()->sharedWorkerRepositoryClient())
    m_frame->loader()
        .client()
        ->sharedWorkerRepositoryClient()
        ->documentDetached(this);

  // FIXME: consider using SuspendableObject.
  if (m_scriptedAnimationController)
    m_scriptedAnimationController->clearDocumentPointer();
  m_scriptedAnimationController.clear();

  m_scriptedIdleTaskController.clear();

  if (svgExtensions())
    accessSVGExtensions().pauseAnimations();

  // FIXME: This shouldn't be needed once LocalDOMWindow becomes
  // ExecutionContext.
  if (m_domWindow)
    m_domWindow->clearEventQueue();

  if (m_layoutView)
    m_layoutView->setIsInWindow(false);

  if (registrationContext())
    registrationContext()->documentWasDetached();

  MutationObserver::cleanSlotChangeList(*this);

  m_hoverNode = nullptr;
  m_activeHoverElement = nullptr;
  m_autofocusElement = nullptr;

  if (m_focusedElement.get()) {
    Element* oldFocusedElement = m_focusedElement;
    m_focusedElement = nullptr;
    if (page())
      page()->chromeClient().focusedNodeChanged(oldFocusedElement, nullptr);
  }
  m_sequentialFocusNavigationStartingPoint = nullptr;

  if (this == &axObjectCacheOwner())
    clearAXObjectCache();

  m_layoutView = nullptr;
  ContainerNode::detachLayoutTree();

  if (this != &axObjectCacheOwner()) {
    if (AXObjectCache* cache = existingAXObjectCache()) {
      // Documents that are not a root document use the AXObjectCache in
      // their root document. Node::removedFrom is called after the
      // document has been detached so it can't find the root document.
      // We do the removals here instead.
      for (Node& node : NodeTraversal::descendantsOf(*this)) {
        cache->remove(&node);
      }
    }
  }

  styleEngine().didDetach();

  frameHost()->eventHandlerRegistry().documentDetached(*this);

  // Signal destruction to mutation observers.
  SynchronousMutationNotifier::notifyContextDestroyed();

  // If this Document is associated with a live DocumentLoader, the
  // DocumentLoader will take care of clearing the FetchContext. Deferring
  // to the DocumentLoader when possible also prevents prematurely clearing
  // the context in the case where multiple Documents end up associated with
  // a single DocumentLoader (e.g., navigating to a javascript: url).
  if (!loader())
    m_fetcher->clearContext();
  // If this document is the master for an HTMLImportsController, sever that
  // relationship. This ensures that we don't leave import loads in flight,
  // thinking they should have access to a valid frame when they don't.
  if (m_importsController) {
    m_importsController->dispose();
    clearImportsController();
  }

  m_timers.setTimerTaskRunner(
      Platform::current()->currentThread()->scheduler()->timerTaskRunner());

  if (m_mediaQueryMatcher)
    m_mediaQueryMatcher->documentDetached();

  m_lifecycle.advanceTo(DocumentLifecycle::Stopped);

  // TODO(haraken): Call contextDestroyed() before we start any disruptive
  // operations.
  // TODO(haraken): Currently we call notifyContextDestroyed() only in
  // Document::detachLayoutTree(), which means that we don't call
  // notifyContextDestroyed() for a document that doesn't get detached.
  // If such a document has any observer, the observer won't get
  // a contextDestroyed() notification. This can happen for a document
  // created by DOMImplementation::createDocument().
  ExecutionContext::notifyContextDestroyed();

  // This is required, as our LocalFrame might delete itself as soon as it
  // detaches us. However, this violates Node::detachLayoutTree() semantics, as
  // it's never possible to re-attach. Eventually Document::detachLayoutTree()
  // should be renamed, or this setting of the frame to 0 could be made
  // explicit in each of the callers of Document::detachLayoutTree().
  m_frame = nullptr;
}

void Document::removeAllEventListeners() {
  ContainerNode::removeAllEventListeners();

  if (LocalDOMWindow* domWindow = this->domWindow())
    domWindow->removeAllEventListeners();
}

Document& Document::axObjectCacheOwner() const {
  // Every document has its own axObjectCache if accessibility is enabled,
  // except for page popups, which share the axObjectCache of their owner.
  Document* doc = const_cast<Document*>(this);
  if (doc->frame() && doc->frame()->pagePopupOwner()) {
    DCHECK(!doc->m_axObjectCache);
    return doc->frame()->pagePopupOwner()->document().axObjectCacheOwner();
  }
  return *doc;
}

void Document::clearAXObjectCache() {
  DCHECK_EQ(&axObjectCacheOwner(), this);
  // Clear the cache member variable before calling delete because attempts
  // are made to access it during destruction.
  if (m_axObjectCache)
    m_axObjectCache->dispose();
  m_axObjectCache.clear();
}

AXObjectCache* Document::existingAXObjectCache() const {
  // If the layoutObject is gone then we are in the process of destruction.
  // This method will be called before m_frame = nullptr.
  if (!axObjectCacheOwner().layoutView())
    return 0;

  return axObjectCacheOwner().m_axObjectCache.get();
}

AXObjectCache* Document::axObjectCache() const {
  Settings* settings = this->settings();
  if (!settings || !settings->getAccessibilityEnabled())
    return 0;

  // Every document has its own AXObjectCache if accessibility is enabled,
  // except for page popups (such as select popups or context menus),
  // which share the AXObjectCache of their owner.
  //
  // See http://crbug.com/532249
  Document& cacheOwner = this->axObjectCacheOwner();

  // If the document has already been detached, do not make a new axObjectCache.
  if (!cacheOwner.layoutView())
    return 0;

  DCHECK(&cacheOwner == this || !m_axObjectCache);
  if (!cacheOwner.m_axObjectCache)
    cacheOwner.m_axObjectCache = AXObjectCache::create(cacheOwner);
  return cacheOwner.m_axObjectCache.get();
}

CanvasFontCache* Document::canvasFontCache() {
  if (!m_canvasFontCache)
    m_canvasFontCache = CanvasFontCache::create(*this);

  return m_canvasFontCache.get();
}

DocumentParser* Document::createParser() {
  if (isHTMLDocument())
    return HTMLDocumentParser::create(toHTMLDocument(*this),
                                      m_parserSyncPolicy);
  // FIXME: this should probably pass the frame instead
  return XMLDocumentParser::create(*this, view());
}

bool Document::isFrameSet() const {
  if (!isHTMLDocument())
    return false;
  return isHTMLFrameSetElement(body());
}

ScriptableDocumentParser* Document::scriptableDocumentParser() const {
  return parser() ? parser()->asScriptableDocumentParser() : nullptr;
}

void Document::open(Document* enteredDocument, ExceptionState& exceptionState) {
  if (importLoader()) {
    exceptionState.throwDOMException(
        InvalidStateError, "Imported document doesn't support open().");
    return;
  }

  if (!isHTMLDocument()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "Only HTML documents support open().");
    return;
  }

  if (m_throwOnDynamicMarkupInsertionCount) {
    exceptionState.throwDOMException(
        InvalidStateError, "Custom Element constructor should not use open().");
    return;
  }

  if (enteredDocument) {
    if (!getSecurityOrigin()->isSameSchemeHostPortAndSuborigin(
            enteredDocument->getSecurityOrigin())) {
      exceptionState.throwSecurityError(
          "Can only call open() on same-origin documents.");
      return;
    }
    setSecurityOrigin(enteredDocument->getSecurityOrigin());
    setURL(enteredDocument->url());
    m_cookieURL = enteredDocument->cookieURL();
  }

  open();
}

void Document::open() {
  DCHECK(!importLoader());

  if (m_frame) {
    if (ScriptableDocumentParser* parser = scriptableDocumentParser()) {
      if (parser->isParsing()) {
        // FIXME: HTML5 doesn't tell us to check this, it might not be correct.
        if (parser->isExecutingScript())
          return;

        if (!parser->wasCreatedByScript() && parser->hasInsertionPoint())
          return;
      }
    }

    // PlzNavigate: We should abort ongoing navigations handled by the client.
    if (m_frame->loader().hasProvisionalNavigation())
      m_frame->loader().stopAllLoaders();
  }

  removeAllEventListenersRecursively();
  resetTreeScope();
  if (m_frame)
    m_frame->selection().clear();
  implicitOpen(ForceSynchronousParsing);
  if (ScriptableDocumentParser* parser = scriptableDocumentParser())
    parser->setWasCreatedByScript(true);

  if (m_frame)
    m_frame->loader().didExplicitOpen();
  if (m_loadEventProgress != LoadEventInProgress &&
      pageDismissalEventBeingDispatched() == NoDismissal)
    m_loadEventProgress = LoadEventNotRun;
}

void Document::detachParser() {
  if (!m_parser)
    return;
  m_parser->detach();
  m_parser.clear();
  DocumentParserTiming::from(*this).markParserDetached();
}

void Document::cancelParsing() {
  detachParser();
  setParsingState(FinishedParsing);
  setReadyState(Complete);
}

DocumentParser* Document::implicitOpen(
    ParserSynchronizationPolicy parserSyncPolicy) {
  detachParser();

  removeChildren();
  DCHECK(!m_focusedElement);

  setCompatibilityMode(NoQuirksMode);

  if (!threadedParsingEnabledForTesting()) {
    parserSyncPolicy = ForceSynchronousParsing;
  } else if (parserSyncPolicy == AllowAsynchronousParsing && isPrefetchOnly()) {
    // Prefetch must be synchronous.
    parserSyncPolicy = ForceSynchronousParsing;
  }

  m_parserSyncPolicy = parserSyncPolicy;
  m_parser = createParser();
  DocumentParserTiming::from(*this).markParserStart();
  setParsingState(Parsing);
  setReadyState(Loading);

  return m_parser;
}

HTMLElement* Document::body() const {
  if (!documentElement() || !isHTMLHtmlElement(documentElement()))
    return 0;

  for (HTMLElement* child =
           Traversal<HTMLElement>::firstChild(*documentElement());
       child; child = Traversal<HTMLElement>::nextSibling(*child)) {
    if (isHTMLFrameSetElement(*child) || isHTMLBodyElement(*child))
      return child;
  }

  return 0;
}

HTMLBodyElement* Document::firstBodyElement() const {
  if (!documentElement())
    return 0;

  for (HTMLElement* child =
           Traversal<HTMLElement>::firstChild(*documentElement());
       child; child = Traversal<HTMLElement>::nextSibling(*child)) {
    if (isHTMLBodyElement(*child))
      return toHTMLBodyElement(child);
  }

  return 0;
}

void Document::setBody(HTMLElement* prpNewBody,
                       ExceptionState& exceptionState) {
  HTMLElement* newBody = prpNewBody;

  if (!newBody) {
    exceptionState.throwDOMException(
        HierarchyRequestError,
        ExceptionMessages::argumentNullOrIncorrectType(1, "HTMLElement"));
    return;
  }
  if (!documentElement()) {
    exceptionState.throwDOMException(HierarchyRequestError,
                                     "No document element exists.");
    return;
  }

  if (!isHTMLBodyElement(*newBody) && !isHTMLFrameSetElement(*newBody)) {
    exceptionState.throwDOMException(
        HierarchyRequestError,
        "The new body element is of type '" + newBody->tagName() +
            "'. It must be either a 'BODY' or 'FRAMESET' element.");
    return;
  }

  HTMLElement* oldBody = body();
  if (oldBody == newBody)
    return;

  if (oldBody)
    documentElement()->replaceChild(newBody, oldBody, exceptionState);
  else
    documentElement()->appendChild(newBody, exceptionState);
}

void Document::willInsertBody() {
  if (frame())
    frame()->loader().client()->dispatchWillInsertBody();
  // If we get to the <body> try to resume commits since we should have content
  // to paint now.
  // TODO(esprehn): Is this really optimal? We might start producing frames
  // for very little content, should we wait for some heuristic like
  // isVisuallyNonEmpty() ?
  beginLifecycleUpdatesIfRenderingReady();
}

HTMLHeadElement* Document::head() const {
  Node* de = documentElement();
  if (!de)
    return 0;

  return Traversal<HTMLHeadElement>::firstChild(*de);
}

Element* Document::viewportDefiningElement(
    const ComputedStyle* rootStyle) const {
  // If a BODY element sets non-visible overflow, it is to be propagated to the
  // viewport, as long as the following conditions are all met:
  // (1) The root element is HTML.
  // (2) It is the primary BODY element (we only assert for this, expecting
  //     callers to behave).
  // (3) The root element has visible overflow.
  // Otherwise it's the root element's properties that are to be propagated.
  Element* rootElement = documentElement();
  Element* bodyElement = body();
  if (!rootElement)
    return 0;
  if (!rootStyle) {
    rootStyle = rootElement->computedStyle();
    if (!rootStyle)
      return 0;
  }
  if (bodyElement && rootStyle->isOverflowVisible() &&
      isHTMLHtmlElement(*rootElement))
    return bodyElement;
  return rootElement;
}

void Document::close(ExceptionState& exceptionState) {
  // FIXME: We should follow the specification more closely:
  //        http://www.whatwg.org/specs/web-apps/current-work/#dom-document-close

  if (importLoader()) {
    exceptionState.throwDOMException(
        InvalidStateError, "Imported document doesn't support close().");
    return;
  }

  if (!isHTMLDocument()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "Only HTML documents support close().");
    return;
  }

  if (m_throwOnDynamicMarkupInsertionCount) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Custom Element constructor should not use close().");
    return;
  }

  close();
}

void Document::close() {
  if (!scriptableDocumentParser() ||
      !scriptableDocumentParser()->wasCreatedByScript() ||
      !scriptableDocumentParser()->isParsing())
    return;

  if (DocumentParser* parser = m_parser)
    parser->finish();

  if (!m_frame) {
    // Because we have no frame, we don't know if all loading has completed,
    // so we just call implicitClose() immediately. FIXME: This might fire
    // the load event prematurely
    // <http://bugs.webkit.org/show_bug.cgi?id=14568>.
    implicitClose();
    return;
  }

  m_frame->loader().checkCompleted();
}

void Document::implicitClose() {
  DCHECK(!inStyleRecalc());
  if (processingLoadEvent() || !m_parser)
    return;
  if (frame() && frame()->navigationScheduler().locationChangePending()) {
    suppressLoadEvent();
    return;
  }

  m_loadEventProgress = LoadEventInProgress;

  ScriptableDocumentParser* parser = scriptableDocumentParser();
  m_wellFormed = parser && parser->wellFormed();

  // We have to clear the parser, in case someone document.write()s from the
  // onLoad event handler, as in Radar 3206524.
  detachParser();

  if (frame() && canExecuteScripts(NotAboutToExecuteScript)) {
    ImageLoader::dispatchPendingLoadEvents();
    ImageLoader::dispatchPendingErrorEvents();
  }

  // JS running below could remove the frame or destroy the LayoutView so we
  // call those two functions repeatedly and don't save them on the stack.

  // To align the HTML load event and the SVGLoad event for the outermost <svg>
  // element, fire it from here, instead of doing it from
  // SVGElement::finishedParsingChildren.
  if (svgExtensions())
    accessSVGExtensions().dispatchSVGLoadEventToOutermostSVGElements();

  if (this->domWindow())
    this->domWindow()->documentWasClosed();

  if (frame()) {
    frame()->loader().client()->dispatchDidHandleOnloadEvents();
    loader()->applicationCacheHost()->stopDeferringEvents();
  }

  if (!frame()) {
    m_loadEventProgress = LoadEventCompleted;
    return;
  }

  // Make sure both the initial layout and reflow happen after the onload
  // fires. This will improve onload scores, and other browsers do it.
  // If they wanna cheat, we can too. -dwh

  if (frame()->navigationScheduler().locationChangePending() &&
      elapsedTime() < cLayoutScheduleThreshold) {
    // Just bail out. Before or during the onload we were shifted to another
    // page.  The old i-Bench suite does this. When this happens don't bother
    // painting or laying out.
    m_loadEventProgress = LoadEventCompleted;
    return;
  }

  // We used to force a synchronous display and flush here.  This really isn't
  // necessary and can in fact be actively harmful if pages are loading at a
  // rate of > 60fps
  // (if your platform is syncing flushes and limiting them to 60fps).
  if (!localOwner() || (localOwner()->layoutObject() &&
                        !localOwner()->layoutObject()->needsLayout())) {
    updateStyleAndLayoutTree();

    // Always do a layout after loading if needed.
    if (view() && !layoutViewItem().isNull() &&
        (!layoutViewItem().firstChild() || layoutViewItem().needsLayout()))
      view()->layout();
  }

  m_loadEventProgress = LoadEventCompleted;

  if (frame() && !layoutViewItem().isNull() &&
      settings()->getAccessibilityEnabled()) {
    if (AXObjectCache* cache = axObjectCache()) {
      if (this == &axObjectCacheOwner())
        cache->handleLoadComplete(this);
      else
        cache->handleLayoutComplete(this);
    }
  }

  if (svgExtensions())
    accessSVGExtensions().startAnimations();
}

bool Document::dispatchBeforeUnloadEvent(ChromeClient& chromeClient,
                                         bool isReload,
                                         bool& didAllowNavigation) {
  if (!m_domWindow)
    return true;

  if (!body())
    return true;

  if (processingBeforeUnload())
    return false;

  BeforeUnloadEvent* beforeUnloadEvent = BeforeUnloadEvent::create();
  beforeUnloadEvent->initEvent(EventTypeNames::beforeunload, false, true);
  m_loadEventProgress = BeforeUnloadEventInProgress;
  m_domWindow->dispatchEvent(beforeUnloadEvent, this);
  m_loadEventProgress = BeforeUnloadEventCompleted;
  if (!beforeUnloadEvent->defaultPrevented())
    defaultEventHandler(beforeUnloadEvent);
  if (!frame() || beforeUnloadEvent->returnValue().isNull())
    return true;

  if (didAllowNavigation) {
    addConsoleMessage(ConsoleMessage::create(
        JSMessageSource, ErrorMessageLevel,
        "Blocked attempt to show multiple 'beforeunload' confirmation panels "
        "for a single navigation."));
    return true;
  }

  String text = beforeUnloadEvent->returnValue();
  if (chromeClient.openBeforeUnloadConfirmPanel(text, m_frame, isReload)) {
    didAllowNavigation = true;
    return true;
  }
  return false;
}

void Document::dispatchUnloadEvents() {
  PluginScriptForbiddenScope forbidPluginDestructorScripting;
  if (m_parser)
    m_parser->stopParsing();

  if (m_loadEventProgress == LoadEventNotRun)
    return;

  if (m_loadEventProgress <= UnloadEventInProgress) {
    if (page())
      page()->willUnloadDocument(*this);
    Element* currentFocusedElement = focusedElement();
    if (isHTMLInputElement(currentFocusedElement))
      toHTMLInputElement(*currentFocusedElement).endEditing();
    if (m_loadEventProgress < PageHideInProgress) {
      m_loadEventProgress = PageHideInProgress;
      if (LocalDOMWindow* window = domWindow())
        window->dispatchEvent(
            PageTransitionEvent::create(EventTypeNames::pagehide, false), this);
      if (!m_frame)
        return;

      PageVisibilityState visibilityState = pageVisibilityState();
      m_loadEventProgress = UnloadVisibilityChangeInProgress;
      if (visibilityState != PageVisibilityStateHidden &&
          RuntimeEnabledFeatures::visibilityChangeOnUnloadEnabled()) {
        // Dispatch visibilitychange event, but don't bother doing
        // other notifications as we're about to be unloaded.
        dispatchEvent(Event::createBubble(EventTypeNames::visibilitychange));
        dispatchEvent(
            Event::createBubble(EventTypeNames::webkitvisibilitychange));
      }
      if (!m_frame)
        return;

      DocumentLoader* documentLoader =
          m_frame->loader().provisionalDocumentLoader();
      m_loadEventProgress = UnloadEventInProgress;
      Event* unloadEvent(Event::create(EventTypeNames::unload));
      if (documentLoader && !documentLoader->timing().unloadEventStart() &&
          !documentLoader->timing().unloadEventEnd()) {
        DocumentLoadTiming& timing = documentLoader->timing();
        DCHECK(timing.navigationStart());
        timing.markUnloadEventStart();
        m_frame->domWindow()->dispatchEvent(unloadEvent, this);
        timing.markUnloadEventEnd();
      } else {
        m_frame->domWindow()->dispatchEvent(unloadEvent, m_frame->document());
      }
    }
    m_loadEventProgress = UnloadEventHandled;
  }

  if (!m_frame)
    return;

  // Don't remove event listeners from a transitional empty document (see
  // https://bugs.webkit.org/show_bug.cgi?id=28716 for more information).
  bool keepEventListeners =
      m_frame->loader().provisionalDocumentLoader() &&
      m_frame->shouldReuseDefaultView(
          m_frame->loader().provisionalDocumentLoader()->url());
  if (!keepEventListeners)
    removeAllEventListenersRecursively();
}

Document::PageDismissalType Document::pageDismissalEventBeingDispatched()
    const {
  switch (m_loadEventProgress) {
    case BeforeUnloadEventInProgress:
      return BeforeUnloadDismissal;
    case PageHideInProgress:
      return PageHideDismissal;
    case UnloadVisibilityChangeInProgress:
      return UnloadVisibilityChangeDismissal;
    case UnloadEventInProgress:
      return UnloadDismissal;

    case LoadEventNotRun:
    case LoadEventInProgress:
    case LoadEventCompleted:
    case BeforeUnloadEventCompleted:
    case UnloadEventHandled:
      return NoDismissal;
  }
  NOTREACHED();
  return NoDismissal;
}

void Document::setParsingState(ParsingState parsingState) {
  m_parsingState = parsingState;

  if (parsing() && !m_elementDataCache)
    m_elementDataCache = ElementDataCache::create();
}

bool Document::shouldScheduleLayout() const {
  // This function will only be called when FrameView thinks a layout is needed.
  // This enforces a couple extra rules.
  //
  //    (a) Only schedule a layout once the stylesheets are loaded.
  //    (b) Only schedule layout once we have a body element.
  if (!isActive())
    return false;

  if (isRenderingReady() && body())
    return true;

  if (documentElement() && !isHTMLHtmlElement(*documentElement()))
    return true;

  return false;
}

int Document::elapsedTime() const {
  return static_cast<int>((currentTime() - m_startTime) * 1000);
}

void Document::write(const SegmentedString& text,
                     Document* enteredDocument,
                     ExceptionState& exceptionState) {
  if (importLoader()) {
    exceptionState.throwDOMException(
        InvalidStateError, "Imported document doesn't support write().");
    return;
  }

  if (!isHTMLDocument()) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "Only HTML documents support write().");
    return;
  }

  if (m_throwOnDynamicMarkupInsertionCount) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Custom Element constructor should not use write().");
    return;
  }

  if (enteredDocument &&
      !getSecurityOrigin()->isSameSchemeHostPortAndSuborigin(
          enteredDocument->getSecurityOrigin())) {
    exceptionState.throwSecurityError(
        "Can only call write() on same-origin documents.");
    return;
  }

  NestingLevelIncrementer nestingLevelIncrementer(m_writeRecursionDepth);

  m_writeRecursionIsTooDeep =
      (m_writeRecursionDepth > 1) && m_writeRecursionIsTooDeep;
  m_writeRecursionIsTooDeep =
      (m_writeRecursionDepth > cMaxWriteRecursionDepth) ||
      m_writeRecursionIsTooDeep;

  if (m_writeRecursionIsTooDeep)
    return;

  bool hasInsertionPoint = m_parser && m_parser->hasInsertionPoint();

  if (!hasInsertionPoint && m_ignoreDestructiveWriteCount) {
    addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
                               ExceptionMessages::failedToExecute(
                                   "write", "Document",
                                   "It isn't possible to write into a document "
                                   "from an asynchronously-loaded external "
                                   "script unless it is explicitly opened.")));
    return;
  }

  if (!hasInsertionPoint)
    open(enteredDocument, ASSERT_NO_EXCEPTION);

  DCHECK(m_parser);
  PerformanceMonitor::reportGenericViolation(
      this, PerformanceMonitor::kDiscouragedAPIUse,
      "Avoid using document.write().", 0, nullptr);
  probe::breakIfNeeded(this, "Document.write");
  m_parser->insert(text);
}

void Document::write(const String& text,
                     Document* enteredDocument,
                     ExceptionState& exceptionState) {
  write(SegmentedString(text), enteredDocument, exceptionState);
}

void Document::writeln(const String& text,
                       Document* enteredDocument,
                       ExceptionState& exceptionState) {
  write(text, enteredDocument, exceptionState);
  if (exceptionState.hadException())
    return;
  write("\n", enteredDocument);
}

void Document::write(LocalDOMWindow* callingWindow,
                     const Vector<String>& text,
                     ExceptionState& exceptionState) {
  DCHECK(callingWindow);
  StringBuilder builder;
  for (const String& string : text)
    builder.append(string);
  write(builder.toString(), callingWindow->document(), exceptionState);
}

void Document::writeln(LocalDOMWindow* callingWindow,
                       const Vector<String>& text,
                       ExceptionState& exceptionState) {
  DCHECK(callingWindow);
  StringBuilder builder;
  for (const String& string : text)
    builder.append(string);
  writeln(builder.toString(), callingWindow->document(), exceptionState);
}

const KURL& Document::virtualURL() const {
  return m_url;
}

KURL Document::virtualCompleteURL(const String& url) const {
  return completeURL(url);
}

DOMTimerCoordinator* Document::timers() {
  return &m_timers;
}

EventTarget* Document::errorEventTarget() {
  return domWindow();
}

void Document::exceptionThrown(ErrorEvent* event) {
  MainThreadDebugger::instance()->exceptionThrown(this, event);
}

void Document::setURL(const KURL& url) {
  const KURL& newURL = url.isEmpty() ? blankURL() : url;
  if (newURL == m_url)
    return;

  m_url = newURL;
  m_accessEntryFromURL = nullptr;
  updateBaseURL();
  contextFeatures().urlDidChange(this);
}

KURL Document::validBaseElementURL() const {
  if (m_baseElementURL.isValid())
    return m_baseElementURL;

  return KURL();
}

void Document::updateBaseURL() {
  KURL oldBaseURL = m_baseURL;
  // DOM 3 Core: When the Document supports the feature "HTML" [DOM Level 2
  // HTML], the base URI is computed using first the value of the href attribute
  // of the HTML BASE element if any, and the value of the documentURI attribute
  // from the Document interface otherwise (which we store, preparsed, in
  // m_url).
  if (!m_baseElementURL.isEmpty())
    m_baseURL = m_baseElementURL;
  else if (!m_baseURLOverride.isEmpty())
    m_baseURL = m_baseURLOverride;
  else
    m_baseURL = m_url;

  selectorQueryCache().invalidate();

  if (!m_baseURL.isValid())
    m_baseURL = KURL();

  if (m_elemSheet) {
    // Element sheet is silly. It never contains anything.
    DCHECK(!m_elemSheet->contents()->ruleCount());
    m_elemSheet = CSSStyleSheet::createInline(*this, m_baseURL);
  }

  if (!equalIgnoringFragmentIdentifier(oldBaseURL, m_baseURL)) {
    // Base URL change changes any relative visited links.
    // FIXME: There are other URLs in the tree that would need to be
    // re-evaluated on dynamic base URL change. Style should be invalidated too.
    for (HTMLAnchorElement& anchor :
         Traversal<HTMLAnchorElement>::startsAfter(*this))
      anchor.invalidateCachedVisitedLinkHash();
  }
}

void Document::setBaseURLOverride(const KURL& url) {
  m_baseURLOverride = url;
  updateBaseURL();
}

void Document::processBaseElement() {
  UseCounter::count(*this, UseCounter::BaseElement);

  // Find the first href attribute in a base element and the first target
  // attribute in a base element.
  const AtomicString* href = 0;
  const AtomicString* target = 0;
  for (HTMLBaseElement* base = Traversal<HTMLBaseElement>::firstWithin(*this);
       base && (!href || !target);
       base = Traversal<HTMLBaseElement>::next(*base)) {
    if (!href) {
      const AtomicString& value = base->fastGetAttribute(hrefAttr);
      if (!value.isNull())
        href = &value;
    }
    if (!target) {
      const AtomicString& value = base->fastGetAttribute(targetAttr);
      if (!value.isNull())
        target = &value;
    }
    if (contentSecurityPolicy()->isActive()) {
      UseCounter::count(*this,
                        UseCounter::ContentSecurityPolicyWithBaseElement);
    }
  }

  // FIXME: Since this doesn't share code with completeURL it may not handle
  // encodings correctly.
  KURL baseElementURL;
  if (href) {
    String strippedHref = stripLeadingAndTrailingHTMLSpaces(*href);
    if (!strippedHref.isEmpty())
      baseElementURL = KURL(url(), strippedHref);
  }

  if (!baseElementURL.isEmpty()) {
    if (baseElementURL.protocolIsData()) {
      UseCounter::count(*this, UseCounter::BaseWithDataHref);
      addConsoleMessage(ConsoleMessage::create(
          SecurityMessageSource, ErrorMessageLevel,
          "'data:' URLs may not be used as base URLs for a document."));
    }
    if (!this->getSecurityOrigin()->canRequest(baseElementURL))
      UseCounter::count(*this, UseCounter::BaseWithCrossOriginHref);
  }

  if (baseElementURL != m_baseElementURL && !baseElementURL.protocolIsData() &&
      contentSecurityPolicy()->allowBaseURI(baseElementURL)) {
    m_baseElementURL = baseElementURL;
    updateBaseURL();
  }

  if (target) {
    if (target->contains('\n') || target->contains('\r'))
      UseCounter::count(*this, UseCounter::BaseWithNewlinesInTarget);
    if (target->contains('<'))
      UseCounter::count(*this, UseCounter::BaseWithOpenBracketInTarget);
    m_baseTarget = *target;
  } else {
    m_baseTarget = nullAtom;
  }
}

String Document::userAgent() const {
  return frame() ? frame()->loader().userAgent() : String();
}

void Document::disableEval(const String& errorMessage) {
  if (!frame())
    return;

  frame()->script().disableEval(errorMessage);
}

void Document::didLoadAllImports() {
  if (!haveScriptBlockingStylesheetsLoaded())
    return;
  if (!importLoader())
    styleResolverMayHaveChanged();
  didLoadAllScriptBlockingResources();
}

void Document::didAddPendingStylesheetInBody() {
  if (ScriptableDocumentParser* parser = scriptableDocumentParser())
    parser->didAddPendingStylesheetInBody();
}

void Document::didRemoveAllPendingStylesheet() {
  styleResolverMayHaveChanged();

  // Only imports on master documents can trigger rendering.
  if (HTMLImportLoader* import = importLoader())
    import->didRemoveAllPendingStylesheet();
  if (!haveImportsLoaded())
    return;
  didLoadAllScriptBlockingResources();
}

void Document::didRemoveAllPendingBodyStylesheets() {
  if (ScriptableDocumentParser* parser = scriptableDocumentParser())
    parser->didLoadAllBodyStylesheets();
}

void Document::didLoadAllScriptBlockingResources() {
  // Use wrapWeakPersistent because the task should not keep this Document alive
  // just for executing scripts.
  m_executeScriptsWaitingForResourcesTaskHandle =
      TaskRunnerHelper::get(TaskType::Networking, this)
          ->postCancellableTask(
              BLINK_FROM_HERE,
              WTF::bind(&Document::executeScriptsWaitingForResources,
                        wrapWeakPersistent(this)));

  if (isHTMLDocument() && body()) {
    // For HTML if we have no more stylesheets to load and we're past the body
    // tag, we should have something to paint so resume.
    beginLifecycleUpdatesIfRenderingReady();
  } else if (!isHTMLDocument() && documentElement()) {
    // For non-HTML there is no body so resume as soon as the sheets are loaded.
    beginLifecycleUpdatesIfRenderingReady();
  }

  if (m_gotoAnchorNeededAfterStylesheetsLoad && view())
    view()->processUrlFragment(m_url);
}

void Document::executeScriptsWaitingForResources() {
  if (!isScriptExecutionReady())
    return;
  if (ScriptableDocumentParser* parser = scriptableDocumentParser())
    parser->executeScriptsWaitingForResources();
}

CSSStyleSheet& Document::elementSheet() {
  if (!m_elemSheet)
    m_elemSheet = CSSStyleSheet::createInline(*this, m_baseURL);
  return *m_elemSheet;
}

void Document::maybeHandleHttpRefresh(const String& content,
                                      HttpRefreshType httpRefreshType) {
  if (m_isViewSource || !m_frame)
    return;

  double delay;
  String refreshURLString;
  if (!parseHTTPRefresh(content, httpRefreshType == HttpRefreshFromMetaTag
                                     ? isHTMLSpace<UChar>
                                     : nullptr,
                        delay, refreshURLString))
    return;
  KURL refreshURL =
      refreshURLString.isEmpty() ? url() : completeURL(refreshURLString);

  if (refreshURL.protocolIsJavaScript()) {
    String message =
        "Refused to refresh " + m_url.elidedString() + " to a javascript: URL";
    addConsoleMessage(ConsoleMessage::create(SecurityMessageSource,
                                             ErrorMessageLevel, message));
    return;
  }

  if (httpRefreshType == HttpRefreshFromMetaTag &&
      isSandboxed(SandboxAutomaticFeatures)) {
    String message =
        "Refused to execute the redirect specified via '<meta "
        "http-equiv='refresh' content='...'>'. The document is sandboxed, and "
        "the 'allow-scripts' keyword is not set.";
    addConsoleMessage(ConsoleMessage::create(SecurityMessageSource,
                                             ErrorMessageLevel, message));
    return;
  }
  m_frame->navigationScheduler().scheduleRedirect(delay, refreshURL);
}

bool Document::shouldMergeWithLegacyDescription(
    ViewportDescription::Type origin) const {
  return settings() && settings()->getViewportMetaMergeContentQuirk() &&
         m_legacyViewportDescription.isMetaViewportType() &&
         m_legacyViewportDescription.type == origin;
}

void Document::setViewportDescription(
    const ViewportDescription& viewportDescription) {
  if (viewportDescription.isLegacyViewportType()) {
    if (viewportDescription == m_legacyViewportDescription)
      return;
    m_legacyViewportDescription = viewportDescription;
  } else {
    if (viewportDescription == m_viewportDescription)
      return;
    m_viewportDescription = viewportDescription;

    // The UA-defined min-width is considered specifically by Android WebView
    // quirks mode.
    if (!viewportDescription.isSpecifiedByAuthor())
      m_viewportDefaultMinWidth = viewportDescription.minWidth;
  }

  updateViewportDescription();
}

ViewportDescription Document::viewportDescription() const {
  ViewportDescription appliedViewportDescription = m_viewportDescription;
  bool viewportMetaEnabled = settings() && settings()->getViewportMetaEnabled();
  if (m_legacyViewportDescription.type !=
          ViewportDescription::UserAgentStyleSheet &&
      viewportMetaEnabled)
    appliedViewportDescription = m_legacyViewportDescription;
  if (shouldOverrideLegacyDescription(m_viewportDescription.type))
    appliedViewportDescription = m_viewportDescription;

  return appliedViewportDescription;
}

void Document::updateViewportDescription() {
  if (frame() && frame()->isMainFrame()) {
    page()->chromeClient().dispatchViewportPropertiesDidChange(
        viewportDescription());
  }
}

String Document::outgoingReferrer() const {
  if (getSecurityOrigin()->isUnique()) {
    // Return |no-referrer|.
    return String();
  }

  // See http://www.whatwg.org/specs/web-apps/current-work/#fetching-resources
  // for why we walk the parent chain for srcdoc documents.
  const Document* referrerDocument = this;
  if (LocalFrame* frame = m_frame) {
    while (frame->document()->isSrcdocDocument()) {
      // Srcdoc documents must be local within the containing frame.
      frame = toLocalFrame(frame->tree().parent());
      // Srcdoc documents cannot be top-level documents, by definition,
      // because they need to be contained in iframes with the srcdoc.
      DCHECK(frame);
    }
    referrerDocument = frame->document();
  }
  return referrerDocument->m_url.strippedForUseAsReferrer();
}

ReferrerPolicy Document::getReferrerPolicy() const {
  ReferrerPolicy policy = ExecutionContext::getReferrerPolicy();
  // For srcdoc documents without their own policy, walk up the frame
  // tree to find the document that is either not a srcdoc or doesn't
  // have its own policy. This algorithm is defined in
  // https://html.spec.whatwg.org/multipage/browsers.html#set-up-a-browsing-context-environment-settings-object.
  if (!m_frame || policy != ReferrerPolicyDefault || !isSrcdocDocument()) {
    return policy;
  }
  LocalFrame* frame = toLocalFrame(m_frame->tree().parent());
  DCHECK(frame);
  return frame->document()->getReferrerPolicy();
}

MouseEventWithHitTestResults Document::performMouseEventHitTest(
    const HitTestRequest& request,
    const LayoutPoint& documentPoint,
    const WebMouseEvent& event) {
  DCHECK(layoutViewItem().isNull() || layoutViewItem().isLayoutView());

  // LayoutView::hitTest causes a layout, and we don't want to hit that until
  // the first layout because until then, there is nothing shown on the screen -
  // the user can't have intentionally clicked on something belonging to this
  // page.  Furthermore, mousemove events before the first layout should not
  // lead to a premature layout() happening, which could show a flash of white.
  // See also the similar code in EventHandler::hitTestResultAtPoint.
  if (layoutViewItem().isNull() || !view() || !view()->didFirstLayout())
    return MouseEventWithHitTestResults(event,
                                        HitTestResult(request, LayoutPoint()));

  HitTestResult result(request, documentPoint);
  layoutViewItem().hitTest(result);

  if (!request.readOnly())
    updateHoverActiveState(request, result.innerElement(), result.scrollbar());

  if (isHTMLCanvasElement(result.innerNode())) {
    HitTestCanvasResult* hitTestCanvasResult =
        toHTMLCanvasElement(result.innerNode())
            ->getControlAndIdIfHitRegionExists(result.pointInInnerNodeFrame());
    if (hitTestCanvasResult->getControl()) {
      result.setInnerNode(hitTestCanvasResult->getControl());
    }
    result.setCanvasRegionId(hitTestCanvasResult->getId());
  }

  return MouseEventWithHitTestResults(event, result);
}

// DOM Section 1.1.1
bool Document::childTypeAllowed(NodeType type) const {
  switch (type) {
    case kAttributeNode:
    case kCdataSectionNode:
    case kDocumentFragmentNode:
    case kDocumentNode:
    case kTextNode:
      return false;
    case kCommentNode:
    case kProcessingInstructionNode:
      return true;
    case kDocumentTypeNode:
    case kElementNode:
      // Documents may contain no more than one of each of these.
      // (One Element and one DocumentType.)
      for (Node& c : NodeTraversal::childrenOf(*this)) {
        if (c.getNodeType() == type)
          return false;
      }
      return true;
  }
  return false;
}

bool Document::canAcceptChild(const Node& newChild,
                              const Node* oldChild,
                              ExceptionState& exceptionState) const {
  if (oldChild && oldChild->getNodeType() == newChild.getNodeType())
    return true;

  int numDoctypes = 0;
  int numElements = 0;

  // First, check how many doctypes and elements we have, not counting
  // the child we're about to remove.
  for (Node& child : NodeTraversal::childrenOf(*this)) {
    if (oldChild && *oldChild == child)
      continue;

    switch (child.getNodeType()) {
      case kDocumentTypeNode:
        numDoctypes++;
        break;
      case kElementNode:
        numElements++;
        break;
      default:
        break;
    }
  }

  // Then, see how many doctypes and elements might be added by the new child.
  if (newChild.isDocumentFragment()) {
    for (Node& child :
         NodeTraversal::childrenOf(toDocumentFragment(newChild))) {
      switch (child.getNodeType()) {
        case kAttributeNode:
        case kCdataSectionNode:
        case kDocumentFragmentNode:
        case kDocumentNode:
        case kTextNode:
          exceptionState.throwDOMException(
              HierarchyRequestError,
              "Nodes of type '" + newChild.nodeName() +
                  "' may not be inserted inside nodes of type '#document'.");
          return false;
        case kCommentNode:
        case kProcessingInstructionNode:
          break;
        case kDocumentTypeNode:
          numDoctypes++;
          break;
        case kElementNode:
          numElements++;
          break;
      }
    }
  } else {
    switch (newChild.getNodeType()) {
      case kAttributeNode:
      case kCdataSectionNode:
      case kDocumentFragmentNode:
      case kDocumentNode:
      case kTextNode:
        exceptionState.throwDOMException(
            HierarchyRequestError,
            "Nodes of type '" + newChild.nodeName() +
                "' may not be inserted inside nodes of type '#document'.");
        return false;
      case kCommentNode:
      case kProcessingInstructionNode:
        return true;
      case kDocumentTypeNode:
        numDoctypes++;
        break;
      case kElementNode:
        numElements++;
        break;
    }
  }

  if (numElements > 1 || numDoctypes > 1) {
    exceptionState.throwDOMException(
        HierarchyRequestError,
        String::format("Only one %s on document allowed.",
                       numElements > 1 ? "element" : "doctype"));
    return false;
  }

  return true;
}

Node* Document::cloneNode(bool deep, ExceptionState&) {
  Document* clone = cloneDocumentWithoutChildren();
  clone->cloneDataFromDocument(*this);
  if (deep)
    cloneChildNodes(clone);
  return clone;
}

Document* Document::cloneDocumentWithoutChildren() {
  DocumentInit init(url());
  if (isXMLDocument()) {
    if (isXHTMLDocument())
      return XMLDocument::createXHTML(
          init.withRegistrationContext(registrationContext()));
    return XMLDocument::create(init);
  }
  return create(init);
}

void Document::cloneDataFromDocument(const Document& other) {
  setCompatibilityMode(other.getCompatibilityMode());
  setEncodingData(other.m_encodingData);
  setContextFeatures(other.contextFeatures());
  setSecurityOrigin(other.getSecurityOrigin()->isolatedCopy());
  setMimeType(other.contentType());
}

bool Document::isSecureContextImpl(
    const SecureContextCheck privilegeContextCheck) const {
  // There may be exceptions for the secure context check defined for certain
  // schemes. The exceptions are applied only to the special scheme and to
  // sandboxed URLs from those origins, but *not* to any children.
  //
  // For example:
  //   <iframe src="http://host">
  //     <iframe src="scheme-has-exception://host"></iframe>
  //     <iframe sandbox src="scheme-has-exception://host"></iframe>
  //   </iframe>
  // both inner iframes pass this check, assuming that the scheme
  // "scheme-has-exception:" is granted an exception.
  //
  // However,
  //   <iframe src="http://host">
  //     <iframe sandbox src="http://host"></iframe>
  //   </iframe>
  // would fail the check (that is, sandbox does not grant an exception itself).
  //
  // Additionally, with
  //   <iframe src="scheme-has-exception://host">
  //     <iframe src="http://host"></iframe>
  //     <iframe sandbox src="http://host"></iframe>
  //   </iframe>
  // both inner iframes would fail the check, even though the outermost iframe
  // passes.
  //
  // In all cases, a frame must be potentially trustworthy in addition to
  // having an exception listed in order for the exception to be granted.
  if (!getSecurityOrigin()->isPotentiallyTrustworthy())
    return false;

  if (SchemeRegistry::schemeShouldBypassSecureContextCheck(
          getSecurityOrigin()->protocol()))
    return true;

  if (privilegeContextCheck == StandardSecureContextCheck) {
    if (!m_frame)
      return true;
    Frame* parent = m_frame->tree().parent();
    while (parent) {
      if (!parent->securityContext()
               ->getSecurityOrigin()
               ->isPotentiallyTrustworthy())
        return false;
      parent = parent->tree().parent();
    }
  }
  return true;
}

StyleSheetList& Document::styleSheets() {
  if (!m_styleSheetList)
    m_styleSheetList = StyleSheetList::create(this);
  return *m_styleSheetList;
}

String Document::preferredStylesheetSet() const {
  return m_styleEngine->preferredStylesheetSetName();
}

String Document::selectedStylesheetSet() const {
  return m_styleEngine->selectedStylesheetSetName();
}

void Document::setSelectedStylesheetSet(const String& aString) {
  styleEngine().setSelectedStylesheetSetName(aString);
}

void Document::evaluateMediaQueryListIfNeeded() {
  if (!m_evaluateMediaQueriesOnStyleRecalc)
    return;
  evaluateMediaQueryList();
  m_evaluateMediaQueriesOnStyleRecalc = false;
}

void Document::evaluateMediaQueryList() {
  if (m_mediaQueryMatcher)
    m_mediaQueryMatcher->mediaFeaturesChanged();
}

void Document::setResizedForViewportUnits() {
  if (m_mediaQueryMatcher)
    m_mediaQueryMatcher->viewportChanged();
  if (!hasViewportUnits())
    return;
  ensureStyleResolver().setResizedForViewportUnits();
  setNeedsStyleRecalcForViewportUnits();
}

void Document::clearResizedForViewportUnits() {
  ensureStyleResolver().clearResizedForViewportUnits();
}

void Document::styleResolverMayHaveChanged() {
  if (hasNodesWithPlaceholderStyle()) {
    setNeedsStyleRecalc(SubtreeStyleChange,
                        StyleChangeReasonForTracing::create(
                            StyleChangeReason::CleanupPlaceholderStyles));
  }

  if (didLayoutWithPendingStylesheets() &&
      !styleEngine().hasPendingScriptBlockingSheets()) {
    // We need to manually repaint because we avoid doing all repaints in layout
    // or style recalc while sheets are still loading to avoid FOUC.
    m_pendingSheetLayout = IgnoreLayoutWithPendingSheets;

    DCHECK(!layoutViewItem().isNull() || importsController());
    if (!layoutViewItem().isNull())
      layoutViewItem().invalidatePaintForViewAndCompositedLayers();
  }
}

void Document::setHoverNode(Node* newHoverNode) {
  m_hoverNode = newHoverNode;
}

void Document::setActiveHoverElement(Element* newActiveElement) {
  if (!newActiveElement) {
    m_activeHoverElement.clear();
    return;
  }

  m_activeHoverElement = newActiveElement;
}

void Document::removeFocusedElementOfSubtree(Node* node,
                                             bool amongChildrenOnly) {
  if (!m_focusedElement)
    return;

  // We can't be focused if we're not in the document.
  if (!node->isConnected())
    return;
  bool contains =
      node->isShadowIncludingInclusiveAncestorOf(m_focusedElement.get());
  if (contains && (m_focusedElement != node || !amongChildrenOnly))
    clearFocusedElement();
}

void Document::hoveredNodeDetached(Element& element) {
  if (!m_hoverNode)
    return;

  m_hoverNode->updateDistribution();
  if (element != m_hoverNode &&
      (!m_hoverNode->isTextNode() ||
       element != FlatTreeTraversal::parent(*m_hoverNode)))
    return;

  m_hoverNode = FlatTreeTraversal::parent(element);
  while (m_hoverNode && !m_hoverNode->layoutObject())
    m_hoverNode = FlatTreeTraversal::parent(*m_hoverNode);

  // If the mouse cursor is not visible, do not clear existing
  // hover effects on the ancestors of |element| and do not invoke
  // new hover effects on any other element.
  if (!page()->isCursorVisible())
    return;

  if (frame())
    frame()->eventHandler().scheduleHoverStateUpdate();
}

void Document::activeChainNodeDetached(Element& element) {
  if (!m_activeHoverElement)
    return;

  if (element != m_activeHoverElement)
    return;

  Node* activeNode = FlatTreeTraversal::parent(element);
  while (activeNode && activeNode->isElementNode() &&
         !activeNode->layoutObject())
    activeNode = FlatTreeTraversal::parent(*activeNode);

  m_activeHoverElement = activeNode && activeNode->isElementNode()
                             ? toElement(activeNode)
                             : nullptr;
}

const Vector<AnnotatedRegionValue>& Document::annotatedRegions() const {
  return m_annotatedRegions;
}

void Document::setAnnotatedRegions(
    const Vector<AnnotatedRegionValue>& regions) {
  m_annotatedRegions = regions;
  setAnnotatedRegionsDirty(false);
}

bool Document::setFocusedElement(Element* prpNewFocusedElement,
                                 const FocusParams& params) {
  DCHECK(!m_lifecycle.inDetach());

  m_clearFocusedElementTimer.stop();

  Element* newFocusedElement = prpNewFocusedElement;

  // Make sure newFocusedNode is actually in this document
  if (newFocusedElement && (newFocusedElement->document() != this))
    return true;

  if (NodeChildRemovalTracker::isBeingRemoved(newFocusedElement))
    return true;

  if (m_focusedElement == newFocusedElement)
    return true;

  bool focusChangeBlocked = false;
  Element* oldFocusedElement = m_focusedElement;
  m_focusedElement = nullptr;

  // Remove focus from the existing focus node (if any)
  if (oldFocusedElement) {
    oldFocusedElement->setFocused(false);

    // Dispatch the blur event and let the node do any other blur related
    // activities (important for text fields)
    // If page lost focus, blur event will have already been dispatched
    if (page() && (page()->focusController().isFocused())) {
      oldFocusedElement->dispatchBlurEvent(newFocusedElement, params.type,
                                           params.sourceCapabilities);

      if (m_focusedElement) {
        // handler shifted focus
        focusChangeBlocked = true;
        newFocusedElement = nullptr;
      }

      // 'focusout' is a DOM level 3 name for the bubbling blur event.
      oldFocusedElement->dispatchFocusOutEvent(EventTypeNames::focusout,
                                               newFocusedElement,
                                               params.sourceCapabilities);
      // 'DOMFocusOut' is a DOM level 2 name for compatibility.
      // FIXME: We should remove firing DOMFocusOutEvent event when we are sure
      // no content depends on it, probably when <rdar://problem/8503958> is
      // resolved.
      oldFocusedElement->dispatchFocusOutEvent(EventTypeNames::DOMFocusOut,
                                               newFocusedElement,
                                               params.sourceCapabilities);

      if (m_focusedElement) {
        // handler shifted focus
        focusChangeBlocked = true;
        newFocusedElement = nullptr;
      }
    }

    if (view()) {
      FrameViewBase* oldFrameViewBase = widgetForElement(*oldFocusedElement);
      if (oldFrameViewBase)
        oldFrameViewBase->setFocused(false, params.type);
      else
        view()->setFocused(false, params.type);
    }
  }

  if (newFocusedElement)
    updateStyleAndLayoutTreeForNode(newFocusedElement);
  if (newFocusedElement && newFocusedElement->isFocusable()) {
    if (isRootEditableElement(*newFocusedElement) &&
        !acceptsEditingFocus(*newFocusedElement)) {
      // delegate blocks focus change
      focusChangeBlocked = true;
      goto SetFocusedElementDone;
    }
    // Set focus on the new node
    m_focusedElement = newFocusedElement;
    setSequentialFocusNavigationStartingPoint(m_focusedElement.get());

    m_focusedElement->setFocused(true);
    // Element::setFocused for frames can dispatch events.
    if (m_focusedElement != newFocusedElement) {
      focusChangeBlocked = true;
      goto SetFocusedElementDone;
    }
    cancelFocusAppearanceUpdate();
    m_focusedElement->updateFocusAppearance(params.selectionBehavior);

    // Dispatch the focus event and let the node do any other focus related
    // activities (important for text fields)
    // If page lost focus, event will be dispatched on page focus, don't
    // duplicate
    if (page() && (page()->focusController().isFocused())) {
      m_focusedElement->dispatchFocusEvent(oldFocusedElement, params.type,
                                           params.sourceCapabilities);

      if (m_focusedElement != newFocusedElement) {
        // handler shifted focus
        focusChangeBlocked = true;
        goto SetFocusedElementDone;
      }
      // DOM level 3 bubbling focus event.
      m_focusedElement->dispatchFocusInEvent(EventTypeNames::focusin,
                                             oldFocusedElement, params.type,
                                             params.sourceCapabilities);

      if (m_focusedElement != newFocusedElement) {
        // handler shifted focus
        focusChangeBlocked = true;
        goto SetFocusedElementDone;
      }

      // For DOM level 2 compatibility.
      // FIXME: We should remove firing DOMFocusInEvent event when we are sure
      // no content depends on it, probably when <rdar://problem/8503958> is m.
      m_focusedElement->dispatchFocusInEvent(EventTypeNames::DOMFocusIn,
                                             oldFocusedElement, params.type,
                                             params.sourceCapabilities);

      if (m_focusedElement != newFocusedElement) {
        // handler shifted focus
        focusChangeBlocked = true;
        goto SetFocusedElementDone;
      }
    }

    if (isRootEditableElement(*m_focusedElement))
      frame()->spellChecker().didBeginEditing(m_focusedElement.get());

    // eww, I suck. set the qt focus correctly
    // ### find a better place in the code for this
    if (view()) {
      FrameViewBase* focusFrameViewBase = widgetForElement(*m_focusedElement);
      if (focusFrameViewBase) {
        // Make sure a FrameViewBase has the right size before giving it focus.
        // Otherwise, we are testing edge cases of the FrameViewBase code.
        // Specifically, in WebCore this does not work well for text fields.
        updateStyleAndLayout();
        // Re-get the FrameViewBase in case updating the layout changed things.
        focusFrameViewBase = widgetForElement(*m_focusedElement);
      }
      if (focusFrameViewBase)
        focusFrameViewBase->setFocused(true, params.type);
      else
        view()->setFocused(true, params.type);
    }
  }

  if (!focusChangeBlocked && m_focusedElement) {
    // Create the AXObject cache in a focus change because Chromium relies on
    // it.
    if (AXObjectCache* cache = axObjectCache())
      cache->handleFocusedUIElementChanged(oldFocusedElement,
                                           newFocusedElement);
  }

  if (!focusChangeBlocked && page()) {
    page()->chromeClient().focusedNodeChanged(oldFocusedElement,
                                              m_focusedElement.get());
  }

SetFocusedElementDone:
  updateStyleAndLayoutTree();
  if (LocalFrame* frame = this->frame())
    frame->selection().didChangeFocus();
  return !focusChangeBlocked;
}

void Document::clearFocusedElement() {
  setFocusedElement(nullptr, FocusParams(SelectionBehaviorOnFocus::None,
                                         WebFocusTypeNone, nullptr));
}

void Document::setSequentialFocusNavigationStartingPoint(Node* node) {
  if (!m_frame)
    return;
  if (!node) {
    m_sequentialFocusNavigationStartingPoint = nullptr;
    return;
  }
  DCHECK_EQ(node->document(), this);
  if (!m_sequentialFocusNavigationStartingPoint)
    m_sequentialFocusNavigationStartingPoint = Range::create(*this);
  m_sequentialFocusNavigationStartingPoint->selectNodeContents(
      node, ASSERT_NO_EXCEPTION);
}

Element* Document::sequentialFocusNavigationStartingPoint(
    WebFocusType type) const {
  if (m_focusedElement)
    return m_focusedElement.get();
  if (!m_sequentialFocusNavigationStartingPoint)
    return nullptr;
  if (!m_sequentialFocusNavigationStartingPoint->collapsed()) {
    Node* node = m_sequentialFocusNavigationStartingPoint->startContainer();
    DCHECK_EQ(node, m_sequentialFocusNavigationStartingPoint->endContainer());
    if (node->isElementNode())
      return toElement(node);
    if (Element* neighborElement = type == WebFocusTypeForward
                                       ? ElementTraversal::previous(*node)
                                       : ElementTraversal::next(*node))
      return neighborElement;
    return node->parentOrShadowHostElement();
  }

  // Range::selectNodeContents didn't select contents because the element had
  // no children.
  if (m_sequentialFocusNavigationStartingPoint->startContainer()
          ->isElementNode() &&
      !m_sequentialFocusNavigationStartingPoint->startContainer()
           ->hasChildren() &&
      m_sequentialFocusNavigationStartingPoint->startOffset() == 0)
    return toElement(
        m_sequentialFocusNavigationStartingPoint->startContainer());

  // A node selected by Range::selectNodeContents was removed from the
  // document tree.
  if (Node* nextNode = m_sequentialFocusNavigationStartingPoint->firstNode()) {
    if (type == WebFocusTypeForward)
      return ElementTraversal::previous(*nextNode);
    if (nextNode->isElementNode())
      return toElement(nextNode);
    return ElementTraversal::next(*nextNode);
  }
  return nullptr;
}

void Document::setCSSTarget(Element* newTarget) {
  if (m_cssTarget)
    m_cssTarget->pseudoStateChanged(CSSSelector::PseudoTarget);
  m_cssTarget = newTarget;
  if (m_cssTarget)
    m_cssTarget->pseudoStateChanged(CSSSelector::PseudoTarget);
}

static void liveNodeListBaseWriteBarrier(void* parent,
                                         const LiveNodeListBase* list) {
  if (isHTMLCollectionType(list->type())) {
    ScriptWrappableVisitor::writeBarrier(
        parent, static_cast<const HTMLCollection*>(list));
  } else {
    ScriptWrappableVisitor::writeBarrier(
        parent, static_cast<const LiveNodeList*>(list));
  }
}

void Document::registerNodeList(const LiveNodeListBase* list) {
  DCHECK(!m_nodeLists[list->invalidationType()].contains(list));
  m_nodeLists[list->invalidationType()].insert(list);
  liveNodeListBaseWriteBarrier(this, list);
  if (list->isRootedAtTreeScope())
    m_listsInvalidatedAtDocument.insert(list);
}

void Document::unregisterNodeList(const LiveNodeListBase* list) {
  DCHECK(m_nodeLists[list->invalidationType()].contains(list));
  m_nodeLists[list->invalidationType()].erase(list);
  if (list->isRootedAtTreeScope()) {
    DCHECK(m_listsInvalidatedAtDocument.contains(list));
    m_listsInvalidatedAtDocument.erase(list);
  }
}

void Document::registerNodeListWithIdNameCache(const LiveNodeListBase* list) {
  DCHECK(!m_nodeLists[InvalidateOnIdNameAttrChange].contains(list));
  m_nodeLists[InvalidateOnIdNameAttrChange].insert(list);
  liveNodeListBaseWriteBarrier(this, list);
}

void Document::unregisterNodeListWithIdNameCache(const LiveNodeListBase* list) {
  DCHECK(m_nodeLists[InvalidateOnIdNameAttrChange].contains(list));
  m_nodeLists[InvalidateOnIdNameAttrChange].erase(list);
}

void Document::attachNodeIterator(NodeIterator* ni) {
  m_nodeIterators.insert(ni);
}

void Document::detachNodeIterator(NodeIterator* ni) {
  // The node iterator can be detached without having been attached if its root
  // node didn't have a document when the iterator was created, but has it now.
  m_nodeIterators.erase(ni);
}

void Document::moveNodeIteratorsToNewDocument(Node& node,
                                              Document& newDocument) {
  HeapHashSet<WeakMember<NodeIterator>> nodeIteratorsList = m_nodeIterators;
  for (NodeIterator* ni : nodeIteratorsList) {
    if (ni->root() == node) {
      detachNodeIterator(ni);
      newDocument.attachNodeIterator(ni);
    }
  }
}

void Document::didMoveTreeToNewDocument(const Node& root) {
  DCHECK_NE(root.document(), this);
  if (!m_ranges.isEmpty()) {
    AttachedRangeSet ranges = m_ranges;
    for (Range* range : ranges)
      range->updateOwnerDocumentIfNeeded();
  }
  notifyMoveTreeToNewDocument(root);
}

void Document::nodeChildrenWillBeRemoved(ContainerNode& container) {
  EventDispatchForbiddenScope assertNoEventDispatch;
  for (Range* range : m_ranges)
    range->nodeChildrenWillBeRemoved(container);

  for (NodeIterator* ni : m_nodeIterators) {
    for (Node& n : NodeTraversal::childrenOf(container))
      ni->nodeWillBeRemoved(n);
  }

  notifyNodeChildrenWillBeRemoved(container);

  if (containsV1ShadowTree()) {
    for (Node& n : NodeTraversal::childrenOf(container))
      n.checkSlotChangeBeforeRemoved();
  }
}

void Document::nodeWillBeRemoved(Node& n) {
  for (NodeIterator* ni : m_nodeIterators)
    ni->nodeWillBeRemoved(n);

  for (Range* range : m_ranges)
    range->nodeWillBeRemoved(n);

  notifyNodeWillBeRemoved(n);

  if (containsV1ShadowTree())
    n.checkSlotChangeBeforeRemoved();

  if (n.inActiveDocument() && n.isElementNode())
    styleEngine().elementWillBeRemoved(toElement(n));
}

void Document::didInsertText(Node* text, unsigned offset, unsigned length) {
  for (Range* range : m_ranges)
    range->didInsertText(text, offset, length);

  m_markers->shiftMarkers(text, offset, length);
}

void Document::didRemoveText(Node* text, unsigned offset, unsigned length) {
  for (Range* range : m_ranges)
    range->didRemoveText(text, offset, length);

  m_markers->removeMarkers(text, offset, length);
  m_markers->shiftMarkers(text, offset + length, 0 - length);
}

void Document::didMergeTextNodes(const Text& mergedNode,
                                 const Text& nodeToBeRemoved,
                                 unsigned oldLength) {
  NodeWithIndex nodeToBeRemovedWithIndex(const_cast<Text&>(nodeToBeRemoved));
  if (!m_ranges.isEmpty()) {
    for (Range* range : m_ranges)
      range->didMergeTextNodes(nodeToBeRemovedWithIndex, oldLength);
  }

  notifyMergeTextNodes(mergedNode, nodeToBeRemovedWithIndex, oldLength);

  // FIXME: This should update markers for spelling and grammar checking.
}

void Document::didSplitTextNode(const Text& oldNode) {
  for (Range* range : m_ranges)
    range->didSplitTextNode(oldNode);

  notifySplitTextNode(oldNode);

  // FIXME: This should update markers for spelling and grammar checking.
}

void Document::setWindowAttributeEventListener(const AtomicString& eventType,
                                               EventListener* listener) {
  LocalDOMWindow* domWindow = this->domWindow();
  if (!domWindow)
    return;
  domWindow->setAttributeEventListener(eventType, listener);
}

EventListener* Document::getWindowAttributeEventListener(
    const AtomicString& eventType) {
  LocalDOMWindow* domWindow = this->domWindow();
  if (!domWindow)
    return 0;
  return domWindow->getAttributeEventListener(eventType);
}

EventQueue* Document::getEventQueue() const {
  if (!m_domWindow)
    return 0;
  return m_domWindow->getEventQueue();
}

void Document::enqueueAnimationFrameTask(std::unique_ptr<WTF::Closure> task) {
  ensureScriptedAnimationController().enqueueTask(std::move(task));
}

void Document::enqueueAnimationFrameEvent(Event* event) {
  ensureScriptedAnimationController().enqueueEvent(event);
}

void Document::enqueueUniqueAnimationFrameEvent(Event* event) {
  ensureScriptedAnimationController().enqueuePerFrameEvent(event);
}

void Document::enqueueScrollEventForNode(Node* target) {
  // Per the W3C CSSOM View Module only scroll events fired at the document
  // should bubble.
  Event* scrollEvent = target->isDocumentNode()
                           ? Event::createBubble(EventTypeNames::scroll)
                           : Event::create(EventTypeNames::scroll);
  scrollEvent->setTarget(target);
  ensureScriptedAnimationController().enqueuePerFrameEvent(scrollEvent);
}

void Document::enqueueResizeEvent() {
  Event* event = Event::create(EventTypeNames::resize);
  event->setTarget(domWindow());
  ensureScriptedAnimationController().enqueuePerFrameEvent(event);
}

void Document::enqueueMediaQueryChangeListeners(
    HeapVector<Member<MediaQueryListListener>>& listeners) {
  ensureScriptedAnimationController().enqueueMediaQueryChangeListeners(
      listeners);
}

void Document::enqueueVisualViewportScrollEvent() {
  VisualViewportScrollEvent* event = VisualViewportScrollEvent::create();
  event->setTarget(domWindow()->visualViewport());
  ensureScriptedAnimationController().enqueuePerFrameEvent(event);
}

void Document::enqueueVisualViewportResizeEvent() {
  VisualViewportResizeEvent* event = VisualViewportResizeEvent::create();
  event->setTarget(domWindow()->visualViewport());
  ensureScriptedAnimationController().enqueuePerFrameEvent(event);
}

void Document::dispatchEventsForPrinting() {
  if (!m_scriptedAnimationController)
    return;
  m_scriptedAnimationController->dispatchEventsAndCallbacksForPrinting();
}

Document::EventFactorySet& Document::eventFactories() {
  DEFINE_STATIC_LOCAL(EventFactorySet, s_eventFactory, ());
  return s_eventFactory;
}

const OriginAccessEntry& Document::accessEntryFromURL() {
  if (!m_accessEntryFromURL) {
    m_accessEntryFromURL = WTF::wrapUnique(
        new OriginAccessEntry(url().protocol(), url().host(),
                              OriginAccessEntry::AllowRegisterableDomains));
  }
  return *m_accessEntryFromURL;
}

void Document::sendSensitiveInputVisibility() {
  if (m_sensitiveInputVisibilityTask.isActive())
    return;

  m_sensitiveInputVisibilityTask =
      TaskRunnerHelper::get(TaskType::UnspecedLoading, this)
          ->postCancellableTask(
              BLINK_FROM_HERE,
              WTF::bind(&Document::sendSensitiveInputVisibilityInternal,
                        wrapWeakPersistent(this)));
}

void Document::sendSensitiveInputVisibilityInternal() {
  if (!frame())
    return;

  mojom::blink::SensitiveInputVisibilityServicePtr sensitiveInputServicePtr;
  frame()->interfaceProvider()->getInterface(
      mojo::MakeRequest(&sensitiveInputServicePtr));
  if (m_passwordCount > 0) {
    sensitiveInputServicePtr->PasswordFieldVisibleInInsecureContext();
    return;
  }
  sensitiveInputServicePtr->AllPasswordFieldsInInsecureContextInvisible();
}

void Document::runExecutionContextTask(
    std::unique_ptr<ExecutionContextTask> task,
    bool isInstrumented) {
  probe::AsyncTask asyncTask(this, task.get(), isInstrumented);
  task->performTask(this);
}

void Document::registerEventFactory(
    std::unique_ptr<EventFactoryBase> eventFactory) {
  DCHECK(!eventFactories().contains(eventFactory.get()));
  eventFactories().insert(std::move(eventFactory));
}

Event* Document::createEvent(ScriptState* scriptState,
                             const String& eventType,
                             ExceptionState& exceptionState) {
  Event* event = nullptr;
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  for (const auto& factory : eventFactories()) {
    event = factory->create(executionContext, eventType);
    if (event) {
      // createEvent for TouchEvent should throw DOM exception if touch event
      // feature detection is not enabled. See crbug.com/392584#c22
      if (equalIgnoringCase(eventType, "TouchEvent") &&
          !RuntimeEnabledFeatures::touchEventFeatureDetectionEnabled())
        break;
      return event;
    }
  }
  exceptionState.throwDOMException(
      NotSupportedError,
      "The provided event type ('" + eventType + "') is invalid.");
  return nullptr;
}

void Document::addMutationEventListenerTypeIfEnabled(
    ListenerType listenerType) {
  if (ContextFeatures::mutationEventsEnabled(this))
    addListenerType(listenerType);
}

void Document::addListenerTypeIfNeeded(const AtomicString& eventType) {
  if (eventType == EventTypeNames::DOMSubtreeModified) {
    UseCounter::count(*this, UseCounter::DOMSubtreeModifiedEvent);
    addMutationEventListenerTypeIfEnabled(DOMSUBTREEMODIFIED_LISTENER);
  } else if (eventType == EventTypeNames::DOMNodeInserted) {
    UseCounter::count(*this, UseCounter::DOMNodeInsertedEvent);
    addMutationEventListenerTypeIfEnabled(DOMNODEINSERTED_LISTENER);
  } else if (eventType == EventTypeNames::DOMNodeRemoved) {
    UseCounter::count(*this, UseCounter::DOMNodeRemovedEvent);
    addMutationEventListenerTypeIfEnabled(DOMNODEREMOVED_LISTENER);
  } else if (eventType == EventTypeNames::DOMNodeRemovedFromDocument) {
    UseCounter::count(*this, UseCounter::DOMNodeRemovedFromDocumentEvent);
    addMutationEventListenerTypeIfEnabled(DOMNODEREMOVEDFROMDOCUMENT_LISTENER);
  } else if (eventType == EventTypeNames::DOMNodeInsertedIntoDocument) {
    UseCounter::count(*this, UseCounter::DOMNodeInsertedIntoDocumentEvent);
    addMutationEventListenerTypeIfEnabled(DOMNODEINSERTEDINTODOCUMENT_LISTENER);
  } else if (eventType == EventTypeNames::DOMCharacterDataModified) {
    UseCounter::count(*this, UseCounter::DOMCharacterDataModifiedEvent);
    addMutationEventListenerTypeIfEnabled(DOMCHARACTERDATAMODIFIED_LISTENER);
  } else if (eventType == EventTypeNames::webkitAnimationStart ||
             eventType == EventTypeNames::animationstart) {
    addListenerType(ANIMATIONSTART_LISTENER);
  } else if (eventType == EventTypeNames::webkitAnimationEnd ||
             eventType == EventTypeNames::animationend) {
    addListenerType(ANIMATIONEND_LISTENER);
  } else if (eventType == EventTypeNames::webkitAnimationIteration ||
             eventType == EventTypeNames::animationiteration) {
    addListenerType(ANIMATIONITERATION_LISTENER);
    if (view()) {
      // Need to re-evaluate time-to-effect-change for any running animations.
      view()->scheduleAnimation();
    }
  } else if (eventType == EventTypeNames::webkitTransitionEnd ||
             eventType == EventTypeNames::transitionend) {
    addListenerType(TRANSITIONEND_LISTENER);
  } else if (eventType == EventTypeNames::scroll) {
    addListenerType(SCROLL_LISTENER);
  }
}

HTMLFrameOwnerElement* Document::localOwner() const {
  if (!frame())
    return 0;
  // FIXME: This probably breaks the attempts to layout after a load is finished
  // in implicitClose(), and probably tons of other things...
  return frame()->deprecatedLocalOwner();
}

void Document::willChangeFrameOwnerProperties(int marginWidth,
                                              int marginHeight,
                                              ScrollbarMode scrollingMode) {
  if (!body())
    return;

  DCHECK(frame() && frame()->owner());
  FrameOwner* owner = frame()->owner();

  if (marginWidth != owner->marginWidth())
    body()->setIntegralAttribute(marginwidthAttr, marginWidth);
  if (marginHeight != owner->marginHeight())
    body()->setIntegralAttribute(marginheightAttr, marginHeight);
  if (scrollingMode != owner->scrollingMode() && view())
    view()->setNeedsLayout();
}

bool Document::isInInvisibleSubframe() const {
  if (!localOwner())
    return false;  // this is a local root element

  // TODO(bokan): This looks like it doesn't work in OOPIF.
  DCHECK(frame());
  return frame()->ownerLayoutItem().isNull();
}

String Document::cookie(ExceptionState& exceptionState) const {
  if (settings() && !settings()->getCookieEnabled())
    return String();

  // FIXME: The HTML5 DOM spec states that this attribute can raise an
  // InvalidStateError exception on getting if the Document has no
  // browsing context.

  if (!getSecurityOrigin()->canAccessCookies()) {
    if (isSandboxed(SandboxOrigin))
      exceptionState.throwSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (url().protocolIs("data"))
      exceptionState.throwSecurityError(
          "Cookies are disabled inside 'data:' URLs.");
    else
      exceptionState.throwSecurityError("Access is denied for this document.");
    return String();
  }

  // Suborigins are cookie-averse and thus should always return the empty
  // string, unless the 'unsafe-cookies' option is provided.
  if (getSecurityOrigin()->hasSuborigin() &&
      !getSecurityOrigin()->suborigin()->policyContains(
          Suborigin::SuboriginPolicyOptions::UnsafeCookies))
    return String();

  KURL cookieURL = this->cookieURL();
  if (cookieURL.isEmpty())
    return String();

  return cookies(this, cookieURL);
}

void Document::setCookie(const String& value, ExceptionState& exceptionState) {
  if (settings() && !settings()->getCookieEnabled())
    return;

  // FIXME: The HTML5 DOM spec states that this attribute can raise an
  // InvalidStateError exception on setting if the Document has no
  // browsing context.

  if (!getSecurityOrigin()->canAccessCookies()) {
    if (isSandboxed(SandboxOrigin))
      exceptionState.throwSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (url().protocolIs("data"))
      exceptionState.throwSecurityError(
          "Cookies are disabled inside 'data:' URLs.");
    else
      exceptionState.throwSecurityError("Access is denied for this document.");
    return;
  }

  // Suborigins are cookie-averse and thus setting should be a no-op, unless
  // the 'unsafe-cookies' option is provided.
  if (getSecurityOrigin()->hasSuborigin() &&
      !getSecurityOrigin()->suborigin()->policyContains(
          Suborigin::SuboriginPolicyOptions::UnsafeCookies))
    return;

  KURL cookieURL = this->cookieURL();
  if (cookieURL.isEmpty())
    return;

  setCookies(this, cookieURL, value);
}

const AtomicString& Document::referrer() const {
  if (loader())
    return loader()->getRequest().httpReferrer();
  return nullAtom;
}

String Document::domain() const {
  return getSecurityOrigin()->domain();
}

void Document::setDomain(const String& rawDomain,
                         ExceptionState& exceptionState) {
  UseCounter::count(*this, UseCounter::DocumentSetDomain);

  if (isSandboxed(SandboxDocumentDomain)) {
    exceptionState.throwSecurityError(
        "Assignment is forbidden for sandboxed iframes.");
    return;
  }

  if (SchemeRegistry::isDomainRelaxationForbiddenForURLScheme(
          getSecurityOrigin()->protocol())) {
    exceptionState.throwSecurityError("Assignment is forbidden for the '" +
                                      getSecurityOrigin()->protocol() +
                                      "' scheme.");
    return;
  }

  bool success = false;
  String newDomain = SecurityOrigin::canonicalizeHost(rawDomain, &success);
  if (!success) {
    exceptionState.throwSecurityError("'" + rawDomain +
                                      "' could not be parsed properly.");
    return;
  }

  if (newDomain.isEmpty()) {
    exceptionState.throwSecurityError("'" + newDomain +
                                      "' is an empty domain.");
    return;
  }

  OriginAccessEntry accessEntry(getSecurityOrigin()->protocol(), newDomain,
                                OriginAccessEntry::AllowSubdomains);
  OriginAccessEntry::MatchResult result =
      accessEntry.matchesOrigin(*getSecurityOrigin());
  if (result == OriginAccessEntry::DoesNotMatchOrigin) {
    exceptionState.throwSecurityError(
        "'" + newDomain + "' is not a suffix of '" + domain() + "'.");
    return;
  }

  if (result == OriginAccessEntry::MatchesOriginButIsPublicSuffix) {
    exceptionState.throwSecurityError("'" + newDomain +
                                      "' is a top-level domain.");
    return;
  }

  if (m_frame) {
    bool wasCrossDomain = m_frame->isCrossOriginSubframe();
    getSecurityOrigin()->setDomainFromDOM(newDomain);
    if (view() && (wasCrossDomain != m_frame->isCrossOriginSubframe()))
      view()->crossOriginStatusChanged();

    m_frame->script().updateSecurityOrigin(getSecurityOrigin());
  }
}

// http://www.whatwg.org/specs/web-apps/current-work/#dom-document-lastmodified
String Document::lastModified() const {
  DateComponents date;
  bool foundDate = false;
  if (m_frame) {
    if (DocumentLoader* documentLoader = loader()) {
      const AtomicString& httpLastModified =
          documentLoader->response().httpHeaderField(HTTPNames::Last_Modified);
      if (!httpLastModified.isEmpty()) {
        double dateValue = parseDate(httpLastModified);
        if (!std::isnan(dateValue)) {
          date.setMillisecondsSinceEpochForDateTime(
              convertToLocalTime(dateValue));
          foundDate = true;
        }
      }
    }
  }
  // FIXME: If this document came from the file system, the HTML5
  // specificiation tells us to read the last modification date from the file
  // system.
  if (!foundDate)
    date.setMillisecondsSinceEpochForDateTime(
        convertToLocalTime(currentTimeMS()));
  return String::format("%02d/%02d/%04d %02d:%02d:%02d", date.month() + 1,
                        date.monthDay(), date.fullYear(), date.hour(),
                        date.minute(), date.second());
}

const KURL Document::firstPartyForCookies() const {
  // TODO(mkwst): This doesn't properly handle HTML Import documents.

  // If this is an imported document, grab its master document's first-party:
  if (importsController() && importsController()->master() &&
      importsController()->master() != this)
    return importsController()->master()->firstPartyForCookies();

  if (!frame())
    return SecurityOrigin::urlWithUniqueSecurityOrigin();

  // TODO(mkwst): This doesn't correctly handle sandboxed documents; we want to
  // look at their URL, but we can't because we don't know what it is.
  Frame* top = frame()->tree().top();
  KURL topDocumentURL =
      top->isLocalFrame()
          ? toLocalFrame(top)->document()->url()
          : KURL(KURL(),
                 top->securityContext()->getSecurityOrigin()->toString());
  if (SchemeRegistry::shouldTreatURLSchemeAsFirstPartyWhenTopLevel(
          topDocumentURL.protocol()))
    return topDocumentURL;

  // We're intentionally using the URL of each document rather than the
  // document's SecurityOrigin.  Sandboxing a document into a unique origin
  // shouldn't effect first-/third-party status for cookies and site data.
  const OriginAccessEntry& accessEntry =
      top->isLocalFrame()
          ? toLocalFrame(top)->document()->accessEntryFromURL()
          : OriginAccessEntry(topDocumentURL.protocol(), topDocumentURL.host(),
                              OriginAccessEntry::AllowRegisterableDomains);
  const Frame* currentFrame = frame();
  while (currentFrame) {
    // Skip over srcdoc documents, as they are always same-origin with their
    // closest non-srcdoc parent.
    while (currentFrame->isLocalFrame() &&
           toLocalFrame(currentFrame)->document()->isSrcdocDocument())
      currentFrame = currentFrame->tree().parent();
    DCHECK(currentFrame);

    // We use 'matchesDomain' here, as it turns out that some folks embed HTTPS
    // login forms
    // into HTTP pages; we should allow this kind of upgrade.
    if (accessEntry.matchesDomain(
            *currentFrame->securityContext()->getSecurityOrigin()) ==
        OriginAccessEntry::DoesNotMatchOrigin)
      return SecurityOrigin::urlWithUniqueSecurityOrigin();

    currentFrame = currentFrame->tree().parent();
  }

  return topDocumentURL;
}

static bool isValidNameNonASCII(const LChar* characters, unsigned length) {
  if (!isValidNameStart(characters[0]))
    return false;

  for (unsigned i = 1; i < length; ++i) {
    if (!isValidNamePart(characters[i]))
      return false;
  }

  return true;
}

static bool isValidNameNonASCII(const UChar* characters, unsigned length) {
  for (unsigned i = 0; i < length;) {
    bool first = i == 0;
    UChar32 c;
    U16_NEXT(characters, i, length, c);  // Increments i.
    if (first ? !isValidNameStart(c) : !isValidNamePart(c))
      return false;
  }

  return true;
}

template <typename CharType>
static inline bool isValidNameASCII(const CharType* characters,
                                    unsigned length) {
  CharType c = characters[0];
  if (!(isASCIIAlpha(c) || c == ':' || c == '_'))
    return false;

  for (unsigned i = 1; i < length; ++i) {
    c = characters[i];
    if (!(isASCIIAlphanumeric(c) || c == ':' || c == '_' || c == '-' ||
          c == '.'))
      return false;
  }

  return true;
}

bool Document::isValidName(const String& name) {
  unsigned length = name.length();
  if (!length)
    return false;

  if (name.is8Bit()) {
    const LChar* characters = name.characters8();

    if (isValidNameASCII(characters, length))
      return true;

    return isValidNameNonASCII(characters, length);
  }

  const UChar* characters = name.characters16();

  if (isValidNameASCII(characters, length))
    return true;

  return isValidNameNonASCII(characters, length);
}

enum QualifiedNameStatus {
  QNValid,
  QNMultipleColons,
  QNInvalidStartChar,
  QNInvalidChar,
  QNEmptyPrefix,
  QNEmptyLocalName
};

struct ParseQualifiedNameResult {
  QualifiedNameStatus status;
  UChar32 character;
  ParseQualifiedNameResult() {}
  explicit ParseQualifiedNameResult(QualifiedNameStatus status)
      : status(status) {}
  ParseQualifiedNameResult(QualifiedNameStatus status, UChar32 character)
      : status(status), character(character) {}
};

template <typename CharType>
static ParseQualifiedNameResult parseQualifiedNameInternal(
    const AtomicString& qualifiedName,
    const CharType* characters,
    unsigned length,
    AtomicString& prefix,
    AtomicString& localName) {
  bool nameStart = true;
  bool sawColon = false;
  int colonPos = 0;

  for (unsigned i = 0; i < length;) {
    UChar32 c;
    U16_NEXT(characters, i, length, c)
    if (c == ':') {
      if (sawColon)
        return ParseQualifiedNameResult(QNMultipleColons);
      nameStart = true;
      sawColon = true;
      colonPos = i - 1;
    } else if (nameStart) {
      if (!isValidNameStart(c))
        return ParseQualifiedNameResult(QNInvalidStartChar, c);
      nameStart = false;
    } else {
      if (!isValidNamePart(c))
        return ParseQualifiedNameResult(QNInvalidChar, c);
    }
  }

  if (!sawColon) {
    prefix = nullAtom;
    localName = qualifiedName;
  } else {
    prefix = AtomicString(characters, colonPos);
    if (prefix.isEmpty())
      return ParseQualifiedNameResult(QNEmptyPrefix);
    int prefixStart = colonPos + 1;
    localName = AtomicString(characters + prefixStart, length - prefixStart);
  }

  if (localName.isEmpty())
    return ParseQualifiedNameResult(QNEmptyLocalName);

  return ParseQualifiedNameResult(QNValid);
}

bool Document::parseQualifiedName(const AtomicString& qualifiedName,
                                  AtomicString& prefix,
                                  AtomicString& localName,
                                  ExceptionState& exceptionState) {
  unsigned length = qualifiedName.length();

  if (!length) {
    exceptionState.throwDOMException(InvalidCharacterError,
                                     "The qualified name provided is empty.");
    return false;
  }

  ParseQualifiedNameResult returnValue;
  if (qualifiedName.is8Bit())
    returnValue = parseQualifiedNameInternal(
        qualifiedName, qualifiedName.characters8(), length, prefix, localName);
  else
    returnValue = parseQualifiedNameInternal(
        qualifiedName, qualifiedName.characters16(), length, prefix, localName);
  if (returnValue.status == QNValid)
    return true;

  StringBuilder message;
  message.append("The qualified name provided ('");
  message.append(qualifiedName);
  message.append("') ");

  if (returnValue.status == QNMultipleColons) {
    message.append("contains multiple colons.");
  } else if (returnValue.status == QNInvalidStartChar) {
    message.append("contains the invalid name-start character '");
    message.append(returnValue.character);
    message.append("'.");
  } else if (returnValue.status == QNInvalidChar) {
    message.append("contains the invalid character '");
    message.append(returnValue.character);
    message.append("'.");
  } else if (returnValue.status == QNEmptyPrefix) {
    message.append("has an empty namespace prefix.");
  } else {
    DCHECK_EQ(returnValue.status, QNEmptyLocalName);
    message.append("has an empty local name.");
  }

  if (returnValue.status == QNInvalidStartChar ||
      returnValue.status == QNInvalidChar)
    exceptionState.throwDOMException(InvalidCharacterError, message.toString());
  else
    exceptionState.throwDOMException(NamespaceError, message.toString());
  return false;
}

void Document::setEncodingData(const DocumentEncodingData& newData) {
  // It's possible for the encoding of the document to change while we're
  // decoding data. That can only occur while we're processing the <head>
  // portion of the document. There isn't much user-visible content in the
  // <head>, but there is the <title> element. This function detects that
  // situation and re-decodes the document's title so that the user doesn't see
  // an incorrectly decoded title in the title bar.
  if (m_titleElement && encoding() != newData.encoding() &&
      !ElementTraversal::firstWithin(*m_titleElement) &&
      encoding() == Latin1Encoding() &&
      m_titleElement->textContent().containsOnlyLatin1()) {
    CString originalBytes = m_titleElement->textContent().latin1();
    std::unique_ptr<TextCodec> codec = newTextCodec(newData.encoding());
    String correctlyDecodedTitle =
        codec->decode(originalBytes.data(), originalBytes.length(), DataEOF);
    m_titleElement->setTextContent(correctlyDecodedTitle);
  }

  DCHECK(newData.encoding().isValid());
  m_encodingData = newData;

  // FIXME: Should be removed as part of
  // https://code.google.com/p/chromium/issues/detail?id=319643
  bool shouldUseVisualOrdering = m_encodingData.encoding().usesVisualOrdering();
  if (shouldUseVisualOrdering != m_visuallyOrdered) {
    m_visuallyOrdered = shouldUseVisualOrdering;
    // FIXME: How is possible to not have a layoutObject here?
    if (!layoutViewItem().isNull()) {
      layoutViewItem().mutableStyleRef().setRtlOrdering(
          m_visuallyOrdered ? EOrder::kVisual : EOrder::kLogical);
    }
    setNeedsStyleRecalc(SubtreeStyleChange,
                        StyleChangeReasonForTracing::create(
                            StyleChangeReason::VisuallyOrdered));
  }
}

KURL Document::completeURL(const String& url) const {
  KURL completed = completeURLWithOverride(url, m_baseURL);

  if (completed.whitespaceRemoved()) {
    if (completed.protocolIsInHTTPFamily()) {
      UseCounter::count(*this,
                        UseCounter::DocumentCompleteURLHTTPContainingNewline);
      bool lessThan = url.contains('<');
      if (lessThan) {
        UseCounter::count(
            *this,
            UseCounter::DocumentCompleteURLHTTPContainingNewlineAndLessThan);

        if (RuntimeEnabledFeatures::restrictCompleteURLCharacterSetEnabled())
          return KURL();
      }
    } else {
      UseCounter::count(
          *this, UseCounter::DocumentCompleteURLNonHTTPContainingNewline);
    }
  }
  return completed;
}

KURL Document::completeURLWithOverride(const String& url,
                                       const KURL& baseURLOverride) const {
  DCHECK(baseURLOverride.isEmpty() || baseURLOverride.isValid());

  // Always return a null URL when passed a null string.
  // FIXME: Should we change the KURL constructor to have this behavior?
  // See also [CSS]StyleSheet::completeURL(const String&)
  if (url.isNull())
    return KURL();
  // This logic is deliberately spread over many statements in an attempt to
  // track down http://crbug.com/312410.
  const KURL& baseURL = baseURLForOverride(baseURLOverride);
  if (!encoding().isValid())
    return KURL(baseURL, url);
  return KURL(baseURL, url, encoding());
}

const KURL& Document::baseURLForOverride(const KURL& baseURLOverride) const {
  // This logic is deliberately spread over many statements in an attempt to
  // track down http://crbug.com/312410.
  const KURL* baseURLFromParent = 0;
  bool shouldUseParentBaseURL = baseURLOverride.isEmpty();
  if (!shouldUseParentBaseURL) {
    const KURL& aboutBlankURL = blankURL();
    shouldUseParentBaseURL = (baseURLOverride == aboutBlankURL);
  }
  if (shouldUseParentBaseURL) {
    if (Document* parent = parentDocument())
      baseURLFromParent = &parent->baseURL();
  }
  return baseURLFromParent ? *baseURLFromParent : baseURLOverride;
}

KURL Document::openSearchDescriptionURL() {
  static const char openSearchMIMEType[] =
      "application/opensearchdescription+xml";
  static const char openSearchRelation[] = "search";

  // FIXME: Why do only top-level frames have openSearchDescriptionURLs?
  if (!frame() || frame()->tree().parent())
    return KURL();

  // FIXME: Why do we need to wait for load completion?
  if (!loadEventFinished())
    return KURL();

  if (!head())
    return KURL();

  for (HTMLLinkElement* linkElement =
           Traversal<HTMLLinkElement>::firstChild(*head());
       linkElement;
       linkElement = Traversal<HTMLLinkElement>::nextSibling(*linkElement)) {
    if (!equalIgnoringCase(linkElement->type(), openSearchMIMEType) ||
        !equalIgnoringCase(linkElement->rel(), openSearchRelation))
      continue;
    if (linkElement->href().isEmpty())
      continue;

    // Count usage; perhaps we can lock this to secure contexts.
    UseCounter::Feature osdDisposition;
    RefPtr<SecurityOrigin> target = SecurityOrigin::create(linkElement->href());
    if (isSecureContext()) {
      osdDisposition = target->isPotentiallyTrustworthy()
                           ? UseCounter::OpenSearchSecureOriginSecureTarget
                           : UseCounter::OpenSearchSecureOriginInsecureTarget;
    } else {
      osdDisposition = target->isPotentiallyTrustworthy()
                           ? UseCounter::OpenSearchInsecureOriginSecureTarget
                           : UseCounter::OpenSearchInsecureOriginInsecureTarget;
    }
    UseCounter::count(*this, osdDisposition);

    return linkElement->href();
  }

  return KURL();
}

void Document::currentScriptForBinding(
    HTMLScriptElementOrSVGScriptElement& scriptElement) const {
  if (Element* script = currentScript()) {
    if (script->isInV1ShadowTree())
      return;
    if (isHTMLScriptElement(script))
      scriptElement.setHTMLScriptElement(toHTMLScriptElement(script));
    else if (isSVGScriptElement(script))
      scriptElement.setSVGScriptElement(toSVGScriptElement(script));
  }
}

void Document::pushCurrentScript(Element* newCurrentScript) {
  DCHECK(isHTMLScriptElement(newCurrentScript) ||
         isSVGScriptElement(newCurrentScript));
  m_currentScriptStack.push_back(newCurrentScript);
}

void Document::popCurrentScript() {
  DCHECK(!m_currentScriptStack.isEmpty());
  m_currentScriptStack.pop_back();
}

void Document::setTransformSource(std::unique_ptr<TransformSource> source) {
  m_transformSource = std::move(source);
}

String Document::designMode() const {
  return inDesignMode() ? "on" : "off";
}

void Document::setDesignMode(const String& value) {
  bool newValue = m_designMode;
  if (equalIgnoringCase(value, "on")) {
    newValue = true;
    UseCounter::count(*this, UseCounter::DocumentDesignModeEnabeld);
  } else if (equalIgnoringCase(value, "off")) {
    newValue = false;
  }
  if (newValue == m_designMode)
    return;
  m_designMode = newValue;
  setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                              StyleChangeReason::DesignMode));
}

Document* Document::parentDocument() const {
  if (!m_frame)
    return 0;
  Frame* parent = m_frame->tree().parent();
  if (!parent || !parent->isLocalFrame())
    return 0;
  return toLocalFrame(parent)->document();
}

Document& Document::topDocument() const {
  // FIXME: Not clear what topDocument() should do in the OOPI case--should it
  // return the topmost available Document, or something else?
  Document* doc = const_cast<Document*>(this);
  for (HTMLFrameOwnerElement* element = doc->localOwner(); element;
       element = doc->localOwner())
    doc = &element->document();

  DCHECK(doc);
  return *doc;
}

Document* Document::contextDocument() {
  if (m_contextDocument)
    return m_contextDocument;
  if (m_frame)
    return this;
  return nullptr;
}

Attr* Document::createAttribute(const AtomicString& name,
                                ExceptionState& exceptionState) {
  return createAttributeNS(nullAtom, convertLocalName(name), exceptionState,
                           true);
}

Attr* Document::createAttributeNS(const AtomicString& namespaceURI,
                                  const AtomicString& qualifiedName,
                                  ExceptionState& exceptionState,
                                  bool shouldIgnoreNamespaceChecks) {
  AtomicString prefix, localName;
  if (!parseQualifiedName(qualifiedName, prefix, localName, exceptionState))
    return nullptr;

  QualifiedName qName(prefix, localName, namespaceURI);

  if (!shouldIgnoreNamespaceChecks && !hasValidNamespaceForAttributes(qName)) {
    exceptionState.throwDOMException(
        NamespaceError,
        "The namespace URI provided ('" + namespaceURI +
            "') is not valid for the qualified name provided ('" +
            qualifiedName + "').");
    return nullptr;
  }

  return Attr::create(*this, qName, emptyAtom);
}

const SVGDocumentExtensions* Document::svgExtensions() {
  return m_svgExtensions.get();
}

SVGDocumentExtensions& Document::accessSVGExtensions() {
  if (!m_svgExtensions)
    m_svgExtensions = new SVGDocumentExtensions(this);
  return *m_svgExtensions;
}

bool Document::hasSVGRootNode() const {
  return isSVGSVGElement(documentElement());
}

HTMLCollection* Document::images() {
  return ensureCachedCollection<HTMLCollection>(DocImages);
}

HTMLCollection* Document::applets() {
  return ensureCachedCollection<HTMLCollection>(DocApplets);
}

HTMLCollection* Document::embeds() {
  return ensureCachedCollection<HTMLCollection>(DocEmbeds);
}

HTMLCollection* Document::scripts() {
  return ensureCachedCollection<HTMLCollection>(DocScripts);
}

HTMLCollection* Document::links() {
  return ensureCachedCollection<HTMLCollection>(DocLinks);
}

HTMLCollection* Document::forms() {
  return ensureCachedCollection<HTMLCollection>(DocForms);
}

HTMLCollection* Document::anchors() {
  return ensureCachedCollection<HTMLCollection>(DocAnchors);
}

HTMLAllCollection* Document::all() {
  return ensureCachedCollection<HTMLAllCollection>(DocAll);
}

HTMLCollection* Document::windowNamedItems(const AtomicString& name) {
  return ensureCachedCollection<WindowNameCollection>(WindowNamedItems, name);
}

DocumentNameCollection* Document::documentNamedItems(const AtomicString& name) {
  return ensureCachedCollection<DocumentNameCollection>(DocumentNamedItems,
                                                        name);
}

void Document::finishedParsing() {
  DCHECK(!scriptableDocumentParser() || !m_parser->isParsing());
  DCHECK(!scriptableDocumentParser() || m_readyState != Loading);
  setParsingState(InDOMContentLoaded);
  DocumentParserTiming::from(*this).markParserStop();

  // FIXME: DOMContentLoaded is dispatched synchronously, but this should be
  // dispatched in a queued task, see https://crbug.com/425790
  if (!m_documentTiming.domContentLoadedEventStart())
    m_documentTiming.markDomContentLoadedEventStart();
  dispatchEvent(Event::createBubble(EventTypeNames::DOMContentLoaded));
  if (!m_documentTiming.domContentLoadedEventEnd())
    m_documentTiming.markDomContentLoadedEventEnd();
  setParsingState(FinishedParsing);

  // Ensure Custom Element callbacks are drained before DOMContentLoaded.
  // FIXME: Remove this ad-hoc checkpoint when DOMContentLoaded is dispatched in
  // a queued task, which will do a checkpoint anyway. https://crbug.com/425790
  Microtask::performCheckpoint(V8PerIsolateData::mainThreadIsolate());

  if (LocalFrame* frame = this->frame()) {
    // Don't update the layout tree if we haven't requested the main resource
    // yet to avoid adding extra latency. Note that the first layout tree update
    // can be expensive since it triggers the parsing of the default stylesheets
    // which are compiled-in.
    const bool mainResourceWasAlreadyRequested =
        frame->loader().stateMachine()->committedFirstRealDocumentLoad();

    // FrameLoader::finishedParsing() might end up calling
    // Document::implicitClose() if all resource loads are
    // complete. HTMLObjectElements can start loading their resources from post
    // attach callbacks triggered by recalcStyle().  This means if we parse out
    // an <object> tag and then reach the end of the document without updating
    // styles, we might not have yet started the resource load and might fire
    // the window load event too early.  To avoid this we force the styles to be
    // up to date before calling FrameLoader::finishedParsing().  See
    // https://bugs.webkit.org/show_bug.cgi?id=36864 starting around comment 35.
    if (mainResourceWasAlreadyRequested)
      updateStyleAndLayoutTree();

    beginLifecycleUpdatesIfRenderingReady();

    frame->loader().finishedParsing();

    TRACE_EVENT_INSTANT1("devtools.timeline", "MarkDOMContent",
                         TRACE_EVENT_SCOPE_THREAD, "data",
                         InspectorMarkLoadEvent::data(frame));
    probe::domContentLoadedEventFired(frame);
  }

  // Schedule dropping of the ElementDataCache. We keep it alive for a while
  // after parsing finishes so that dynamically inserted content can also
  // benefit from sharing optimizations.  Note that we don't refresh the timer
  // on cache access since that could lead to huge caches being kept alive
  // indefinitely by something innocuous like JS setting .innerHTML repeatedly
  // on a timer.
  m_elementDataCacheClearTimer.startOneShot(10, BLINK_FROM_HERE);

  // Parser should have picked up all preloads by now
  m_fetcher->clearPreloads(ResourceFetcher::ClearSpeculativeMarkupPreloads);

  if (isPrefetchOnly())
    WebPrerenderingSupport::current()->prefetchFinished();
}

void Document::elementDataCacheClearTimerFired(TimerBase*) {
  m_elementDataCache.clear();
}

void Document::beginLifecycleUpdatesIfRenderingReady() {
  if (!isActive())
    return;
  if (!isRenderingReady())
    return;
  view()->beginLifecycleUpdates();
}

Vector<IconURL> Document::iconURLs(int iconTypesMask) {
  IconURL firstFavicon;
  IconURL firstTouchIcon;
  IconURL firstTouchPrecomposedIcon;
  Vector<IconURL> secondaryIcons;

  using TraversalFunction = HTMLLinkElement* (*)(const Node&);
  TraversalFunction findNextCandidate =
      &Traversal<HTMLLinkElement>::nextSibling;

  HTMLLinkElement* firstElement = nullptr;
  if (head()) {
    firstElement = Traversal<HTMLLinkElement>::firstChild(*head());
  } else if (isSVGDocument() && isSVGSVGElement(documentElement())) {
    firstElement = Traversal<HTMLLinkElement>::firstWithin(*documentElement());
    findNextCandidate = &Traversal<HTMLLinkElement>::next;
  }

  // Start from the first child node so that icons seen later take precedence as
  // required by the spec.
  for (HTMLLinkElement* linkElement = firstElement; linkElement;
       linkElement = findNextCandidate(*linkElement)) {
    if (!(linkElement->getIconType() & iconTypesMask))
      continue;
    if (linkElement->href().isEmpty())
      continue;

    IconURL newURL(linkElement->href(), linkElement->iconSizes(),
                   linkElement->type(), linkElement->getIconType());
    if (linkElement->getIconType() == Favicon) {
      if (firstFavicon.m_iconType != InvalidIcon)
        secondaryIcons.push_back(firstFavicon);
      firstFavicon = newURL;
    } else if (linkElement->getIconType() == TouchIcon) {
      if (firstTouchIcon.m_iconType != InvalidIcon)
        secondaryIcons.push_back(firstTouchIcon);
      firstTouchIcon = newURL;
    } else if (linkElement->getIconType() == TouchPrecomposedIcon) {
      if (firstTouchPrecomposedIcon.m_iconType != InvalidIcon)
        secondaryIcons.push_back(firstTouchPrecomposedIcon);
      firstTouchPrecomposedIcon = newURL;
    } else {
      NOTREACHED();
    }
  }

  Vector<IconURL> iconURLs;
  if (firstFavicon.m_iconType != InvalidIcon)
    iconURLs.push_back(firstFavicon);
  else if (m_url.protocolIsInHTTPFamily() && iconTypesMask & Favicon)
    iconURLs.push_back(IconURL::defaultFavicon(m_url));

  if (firstTouchIcon.m_iconType != InvalidIcon)
    iconURLs.push_back(firstTouchIcon);
  if (firstTouchPrecomposedIcon.m_iconType != InvalidIcon)
    iconURLs.push_back(firstTouchPrecomposedIcon);
  for (int i = secondaryIcons.size() - 1; i >= 0; --i)
    iconURLs.push_back(secondaryIcons[i]);
  return iconURLs;
}

Color Document::themeColor() const {
  auto rootElement = documentElement();
  if (!rootElement)
    return Color();
  for (HTMLMetaElement& metaElement :
       Traversal<HTMLMetaElement>::descendantsOf(*rootElement)) {
    Color color = Color::transparent;
    if (equalIgnoringCase(metaElement.name(), "theme-color") &&
        CSSParser::parseColor(
            color, metaElement.content().getString().stripWhiteSpace(), true))
      return color;
  }
  return Color();
}

HTMLLinkElement* Document::linkManifest() const {
  HTMLHeadElement* head = this->head();
  if (!head)
    return 0;

  // The first link element with a manifest rel must be used. Others are
  // ignored.
  for (HTMLLinkElement* linkElement =
           Traversal<HTMLLinkElement>::firstChild(*head);
       linkElement;
       linkElement = Traversal<HTMLLinkElement>::nextSibling(*linkElement)) {
    if (!linkElement->relAttribute().isManifest())
      continue;
    return linkElement;
  }

  return 0;
}

void Document::initSecurityContext(const DocumentInit& initializer) {
  DCHECK(!getSecurityOrigin());

  if (!initializer.hasSecurityContext()) {
    // No source for a security context.
    // This can occur via document.implementation.createDocument().
    m_cookieURL = KURL(ParsedURLString, emptyString);
    setSecurityOrigin(SecurityOrigin::createUnique());
    initContentSecurityPolicy();
    // Unique security origins cannot have a suborigin
    return;
  }

  // In the common case, create the security context from the currently
  // loading URL with a fresh content security policy.
  enforceSandboxFlags(initializer.getSandboxFlags());
  setInsecureRequestPolicy(initializer.getInsecureRequestPolicy());
  if (initializer.insecureNavigationsToUpgrade()) {
    for (auto toUpgrade : *initializer.insecureNavigationsToUpgrade())
      addInsecureNavigationUpgrade(toUpgrade);
  }

  if (isSandboxed(SandboxOrigin)) {
    m_cookieURL = m_url;
    setSecurityOrigin(SecurityOrigin::createUnique());
    // If we're supposed to inherit our security origin from our
    // owner, but we're also sandboxed, the only things we inherit are
    // the origin's potential trustworthiness and the ability to
    // load local resources. The latter lets about:blank iframes in
    // file:// URL documents load images and other resources from
    // the file system.
    if (initializer.owner() &&
        initializer.owner()->getSecurityOrigin()->isPotentiallyTrustworthy())
      getSecurityOrigin()->setUniqueOriginIsPotentiallyTrustworthy(true);
    if (initializer.owner() &&
        initializer.owner()->getSecurityOrigin()->canLoadLocalResources())
      getSecurityOrigin()->grantLoadLocalResources();
  } else if (initializer.owner()) {
    m_cookieURL = initializer.owner()->cookieURL();
    // We alias the SecurityOrigins to match Firefox, see Bug 15313
    // https://bugs.webkit.org/show_bug.cgi?id=15313
    setSecurityOrigin(initializer.owner()->getSecurityOrigin());
  } else {
    m_cookieURL = m_url;
    setSecurityOrigin(SecurityOrigin::create(m_url));
  }

  // Set the address space before setting up CSP, as the latter may override
  // the former via the 'treat-as-public-address' directive (see
  // https://mikewest.github.io/cors-rfc1918/#csp).
  if (initializer.isHostedInReservedIPRange()) {
    setAddressSpace(getSecurityOrigin()->isLocalhost()
                        ? WebAddressSpaceLocal
                        : WebAddressSpacePrivate);
  } else if (getSecurityOrigin()->isLocal()) {
    // "Local" security origins (like 'file://...') are treated as having
    // a local address space.
    //
    // TODO(mkwst): It's not entirely clear that this is a good idea.
    setAddressSpace(WebAddressSpaceLocal);
  } else {
    setAddressSpace(WebAddressSpacePublic);
  }

  if (importsController()) {
    // If this document is an HTML import, grab a reference to it's master
    // document's Content Security Policy. We don't call
    // 'initContentSecurityPolicy' in this case, as we can't rebind the master
    // document's policy object: its ExecutionContext needs to remain tied to
    // the master document.
    setContentSecurityPolicy(
        importsController()->master()->contentSecurityPolicy());
  } else {
    initContentSecurityPolicy();
  }

  if (getSecurityOrigin()->hasSuborigin())
    enforceSuborigin(*getSecurityOrigin()->suborigin());

  if (Settings* settings = initializer.settings()) {
    if (!settings->getWebSecurityEnabled()) {
      // Web security is turned off. We should let this document access every
      // other document. This is used primary by testing harnesses for web
      // sites.
      getSecurityOrigin()->grantUniversalAccess();
    } else if (getSecurityOrigin()->isLocal()) {
      if (settings->getAllowUniversalAccessFromFileURLs()) {
        // Some clients want local URLs to have universal access, but that
        // setting is dangerous for other clients.
        getSecurityOrigin()->grantUniversalAccess();
      } else if (!settings->getAllowFileAccessFromFileURLs()) {
        // Some clients do not want local URLs to have access to other local
        // URLs.
        getSecurityOrigin()->blockLocalAccessFromLocalOrigin();
      }
    }
  }

  if (initializer.shouldTreatURLAsSrcdocDocument()) {
    m_isSrcdocDocument = true;
    setBaseURLOverride(initializer.parentBaseURL());
  }

  if (getSecurityOrigin()->isUnique() &&
      SecurityOrigin::create(m_url)->isPotentiallyTrustworthy())
    getSecurityOrigin()->setUniqueOriginIsPotentiallyTrustworthy(true);

  if (getSecurityOrigin()->hasSuborigin())
    enforceSuborigin(*getSecurityOrigin()->suborigin());
}

void Document::initContentSecurityPolicy(ContentSecurityPolicy* csp) {
  setContentSecurityPolicy(csp ? csp : ContentSecurityPolicy::create());

  // We inherit the parent/opener's CSP for documents with "local" schemes:
  // 'about', 'blob', 'data', and 'filesystem'. We also inherit CSP for
  // documents with empty/invalid URLs because we treat those URLs as
  // 'about:blank' in Blink.
  //
  // https://w3c.github.io/webappsec-csp/#initialize-document-csp
  //
  // TODO(dcheng): This is similar enough to work we're doing in
  // 'DocumentLoader::ensureWriter' that it might make sense to combine them.
  if (m_frame) {
    Frame* inheritFrom = m_frame->tree().parent() ? m_frame->tree().parent()
                                                  : m_frame->client()->opener();
    if (inheritFrom && m_frame != inheritFrom) {
      DCHECK(inheritFrom->securityContext() &&
             inheritFrom->securityContext()->contentSecurityPolicy());
      ContentSecurityPolicy* policyToInherit =
          inheritFrom->securityContext()->contentSecurityPolicy();
      if (m_url.isEmpty() || m_url.protocolIsAbout() ||
          m_url.protocolIsData() || m_url.protocolIs("blob") ||
          m_url.protocolIs("filesystem")) {
        contentSecurityPolicy()->copyStateFrom(policyToInherit);
      }
      // Plugin documents inherit their parent/opener's 'plugin-types' directive
      // regardless of URL.
      if (isPluginDocument())
        contentSecurityPolicy()->copyPluginTypesFrom(policyToInherit);
    }
  }
  contentSecurityPolicy()->bindToExecutionContext(this);
}

bool Document::isSecureTransitionTo(const KURL& url) const {
  RefPtr<SecurityOrigin> other = SecurityOrigin::create(url);
  return getSecurityOrigin()->canAccess(other.get());
}

bool Document::canExecuteScripts(ReasonForCallingCanExecuteScripts reason) {
  if (isSandboxed(SandboxScripts)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    if (reason == AboutToExecuteScript) {
      addConsoleMessage(ConsoleMessage::create(
          SecurityMessageSource, ErrorMessageLevel,
          "Blocked script execution in '" + url().elidedString() +
              "' because the document's frame is sandboxed and the "
              "'allow-scripts' permission is not set."));
    }
    return false;
  }

  if (isViewSource()) {
    DCHECK(getSecurityOrigin()->isUnique());
    return true;
  }

  DCHECK(frame())
      << "you are querying canExecuteScripts on a non contextDocument.";

  LocalFrameClient* client = frame()->loader().client();
  if (!client)
    return false;

  Settings* settings = frame()->settings();
  if (!client->allowScript(settings && settings->getScriptEnabled())) {
    if (reason == AboutToExecuteScript)
      client->didNotAllowScript();

    return false;
  }

  return true;
}

bool Document::allowInlineEventHandler(Node* node,
                                       EventListener* listener,
                                       const String& contextURL,
                                       const WTF::OrdinalNumber& contextLine) {
  Element* element = node && node->isElementNode() ? toElement(node) : nullptr;
  if (!ContentSecurityPolicy::shouldBypassMainWorld(this) &&
      !contentSecurityPolicy()->allowInlineEventHandler(
          element, listener->code(), contextURL, contextLine))
    return false;

  // HTML says that inline script needs browsing context to create its execution
  // environment.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/webappapis.html#event-handler-attributes
  // Also, if the listening node came from other document, which happens on
  // context-less event dispatching, we also need to ask the owner document of
  // the node.
  LocalFrame* frame = executingFrame();
  if (!frame)
    return false;
  if (!contextDocument()->canExecuteScripts(NotAboutToExecuteScript))
    return false;
  if (node && node->document() != this &&
      !node->document().allowInlineEventHandler(node, listener, contextURL,
                                                contextLine))
    return false;

  return true;
}

bool Document::allowExecutingScripts(Node* node) {
  // FIXME: Eventually we'd like to evaluate scripts which are inserted into a
  // viewless document but this'll do for now.
  // See http://bugs.webkit.org/show_bug.cgi?id=5727
  LocalFrame* frame = executingFrame();
  if (!frame)
    return false;
  if (!node->document().executingFrame())
    return false;
  if (!canExecuteScripts(AboutToExecuteScript))
    return false;
  return true;
}

void Document::enforceSandboxFlags(SandboxFlags mask) {
  RefPtr<SecurityOrigin> standInOrigin = getSecurityOrigin();
  applySandboxFlags(mask);
  // Send a notification if the origin has been updated.
  if (standInOrigin && !standInOrigin->isUnique() &&
      getSecurityOrigin()->isUnique()) {
    getSecurityOrigin()->setUniqueOriginIsPotentiallyTrustworthy(
        standInOrigin->isPotentiallyTrustworthy());
    if (frame())
      frame()->loader().client()->didUpdateToUniqueOrigin();
  }
}

void Document::updateSecurityOrigin(PassRefPtr<SecurityOrigin> origin) {
  setSecurityOrigin(std::move(origin));
  didUpdateSecurityOrigin();
}

void Document::didUpdateSecurityOrigin() {
  if (!m_frame)
    return;
  m_frame->script().updateSecurityOrigin(getSecurityOrigin());
}

bool Document::isContextThread() const {
  return isMainThread();
}

void Document::updateFocusAppearanceLater() {
  if (!m_updateFocusAppearanceTimer.isActive())
    m_updateFocusAppearanceTimer.startOneShot(0, BLINK_FROM_HERE);
}

void Document::cancelFocusAppearanceUpdate() {
  m_updateFocusAppearanceTimer.stop();
}

void Document::updateFocusAppearanceTimerFired(TimerBase*) {
  Element* element = focusedElement();
  if (!element)
    return;
  updateStyleAndLayout();
  if (element->isFocusable())
    element->updateFocusAppearance(SelectionBehaviorOnFocus::Restore);
}

void Document::attachRange(Range* range) {
  DCHECK(!m_ranges.contains(range));
  m_ranges.insert(range);
}

void Document::detachRange(Range* range) {
  // We don't ASSERT m_ranges.contains(range) to allow us to call this
  // unconditionally to fix: https://bugs.webkit.org/show_bug.cgi?id=26044
  m_ranges.erase(range);
}

void Document::initDNSPrefetch() {
  Settings* settings = this->settings();

  m_haveExplicitlyDisabledDNSPrefetch = false;
  m_isDNSPrefetchEnabled = settings && settings->getDNSPrefetchingEnabled() &&
                           getSecurityOrigin()->protocol() == "http";

  // Inherit DNS prefetch opt-out from parent frame
  if (Document* parent = parentDocument()) {
    if (!parent->isDNSPrefetchEnabled())
      m_isDNSPrefetchEnabled = false;
  }
}

void Document::parseDNSPrefetchControlHeader(const String& dnsPrefetchControl) {
  if (equalIgnoringCase(dnsPrefetchControl, "on") &&
      !m_haveExplicitlyDisabledDNSPrefetch) {
    m_isDNSPrefetchEnabled = true;
    return;
  }

  m_isDNSPrefetchEnabled = false;
  m_haveExplicitlyDisabledDNSPrefetch = true;
}

IntersectionObserverController* Document::intersectionObserverController() {
  return m_intersectionObserverController;
}

IntersectionObserverController&
Document::ensureIntersectionObserverController() {
  if (!m_intersectionObserverController)
    m_intersectionObserverController =
        IntersectionObserverController::create(this);
  return *m_intersectionObserverController;
}

ResizeObserverController& Document::ensureResizeObserverController() {
  if (!m_resizeObserverController)
    m_resizeObserverController = new ResizeObserverController();
  return *m_resizeObserverController;
}

static void runAddConsoleMessageTask(MessageSource source,
                                     MessageLevel level,
                                     const String& message,
                                     ExecutionContext* context) {
  context->addConsoleMessage(ConsoleMessage::create(source, level, message));
}

void Document::addConsoleMessage(ConsoleMessage* consoleMessage) {
  if (!isContextThread()) {
    TaskRunnerHelper::get(TaskType::Unthrottled, this)
        ->postTask(
            BLINK_FROM_HERE,
            crossThreadBind(&runAddConsoleMessageTask, consoleMessage->source(),
                            consoleMessage->level(), consoleMessage->message(),
                            wrapCrossThreadPersistent(this)));
    return;
  }

  if (!m_frame)
    return;

  if (consoleMessage->location()->isUnknown()) {
    // TODO(dgozman): capture correct location at call places instead.
    unsigned lineNumber = 0;
    if (!isInDocumentWrite() && scriptableDocumentParser()) {
      ScriptableDocumentParser* parser = scriptableDocumentParser();
      if (parser->isParsingAtLineNumber())
        lineNumber = parser->lineNumber().oneBasedInt();
    }
    consoleMessage = ConsoleMessage::create(
        consoleMessage->source(), consoleMessage->level(),
        consoleMessage->message(),
        SourceLocation::create(url().getString(), lineNumber, 0, nullptr));
  }
  m_frame->console().addMessage(consoleMessage);
}

void Document::postTask(TaskType taskType,
                        const WebTraceLocation& location,
                        std::unique_ptr<ExecutionContextTask> task,
                        const String& taskNameForInstrumentation) {
  if (!taskNameForInstrumentation.isEmpty()) {
    probe::asyncTaskScheduled(this, taskNameForInstrumentation, task.get());
  }

  TaskRunnerHelper::get(taskType, this)
      ->postTask(location,
                 crossThreadBind(&Document::runExecutionContextTask,
                                 wrapCrossThreadWeakPersistent(this),
                                 WTF::passed(std::move(task)),
                                 !taskNameForInstrumentation.isEmpty()));
}

void Document::tasksWereSuspended() {
  scriptRunner()->suspend();

  if (m_parser)
    m_parser->suspendScheduledTasks();
  if (m_scriptedAnimationController)
    m_scriptedAnimationController->suspend();
}

void Document::tasksWereResumed() {
  scriptRunner()->resume();

  if (m_parser)
    m_parser->resumeScheduledTasks();
  if (m_scriptedAnimationController)
    m_scriptedAnimationController->resume();

  MutationObserver::resumeSuspendedObservers();
  if (m_domWindow)
    DOMWindowPerformance::performance(*m_domWindow)->resumeSuspendedObservers();
}

bool Document::tasksNeedSuspension() {
  Page* page = this->page();
  return page && page->suspended();
}

void Document::addToTopLayer(Element* element, const Element* before) {
  if (element->isInTopLayer())
    return;

  DCHECK(!m_topLayerElements.contains(element));
  DCHECK(!before || m_topLayerElements.contains(before));
  if (before) {
    size_t beforePosition = m_topLayerElements.find(before);
    m_topLayerElements.insert(beforePosition, element);
  } else {
    m_topLayerElements.push_back(element);
  }
  element->setIsInTopLayer(true);
}

void Document::removeFromTopLayer(Element* element) {
  if (!element->isInTopLayer())
    return;
  size_t position = m_topLayerElements.find(element);
  DCHECK_NE(position, kNotFound);
  m_topLayerElements.remove(position);
  element->setIsInTopLayer(false);
}

HTMLDialogElement* Document::activeModalDialog() const {
  if (m_topLayerElements.isEmpty())
    return 0;
  return toHTMLDialogElement(m_topLayerElements.back().get());
}

void Document::exitPointerLock() {
  if (!page())
    return;
  if (Element* target = page()->pointerLockController().element()) {
    if (target->document() != this)
      return;
    page()->pointerLockController().requestPointerUnlock();
  }
}

Element* Document::pointerLockElement() const {
  if (!page() || page()->pointerLockController().lockPending())
    return 0;
  if (Element* element = page()->pointerLockController().element()) {
    if (element->document() == this)
      return element;
  }
  return 0;
}

void Document::suppressLoadEvent() {
  if (!loadEventFinished())
    m_loadEventProgress = LoadEventCompleted;
}

void Document::decrementLoadEventDelayCount() {
  DCHECK(m_loadEventDelayCount);
  --m_loadEventDelayCount;

  if (!m_loadEventDelayCount)
    checkLoadEventSoon();
}

void Document::decrementLoadEventDelayCountAndCheckLoadEvent() {
  DCHECK(m_loadEventDelayCount);
  --m_loadEventDelayCount;

  if (!m_loadEventDelayCount && frame())
    frame()->loader().checkCompleted();
}

void Document::checkLoadEventSoon() {
  if (frame() && !m_loadEventDelayTimer.isActive())
    m_loadEventDelayTimer.startOneShot(0, BLINK_FROM_HERE);
}

bool Document::isDelayingLoadEvent() {
  // Always delay load events until after garbage collection.
  // This way we don't have to explicitly delay load events via
  // incrementLoadEventDelayCount and decrementLoadEventDelayCount in
  // Node destructors.
  if (ThreadState::current()->sweepForbidden()) {
    if (!m_loadEventDelayCount)
      checkLoadEventSoon();
    return true;
  }
  return m_loadEventDelayCount;
}

void Document::loadEventDelayTimerFired(TimerBase*) {
  if (frame())
    frame()->loader().checkCompleted();
}

void Document::loadPluginsSoon() {
  // FIXME: Remove this timer once we don't need to compute layout to load
  // plugins.
  if (!m_pluginLoadingTimer.isActive())
    m_pluginLoadingTimer.startOneShot(0, BLINK_FROM_HERE);
}

void Document::pluginLoadingTimerFired(TimerBase*) {
  updateStyleAndLayout();
}

ScriptedAnimationController& Document::ensureScriptedAnimationController() {
  if (!m_scriptedAnimationController) {
    m_scriptedAnimationController = ScriptedAnimationController::create(this);
    // We need to make sure that we don't start up the animation controller on a
    // background tab, for example.
    if (!page())
      m_scriptedAnimationController->suspend();
  }
  return *m_scriptedAnimationController;
}

int Document::requestAnimationFrame(FrameRequestCallback* callback) {
  return ensureScriptedAnimationController().registerCallback(callback);
}

void Document::cancelAnimationFrame(int id) {
  if (!m_scriptedAnimationController)
    return;
  m_scriptedAnimationController->cancelCallback(id);
}

void Document::serviceScriptedAnimations(double monotonicAnimationStartTime) {
  if (!m_scriptedAnimationController)
    return;
  m_scriptedAnimationController->serviceScriptedAnimations(
      monotonicAnimationStartTime);
}

ScriptedIdleTaskController& Document::ensureScriptedIdleTaskController() {
  if (!m_scriptedIdleTaskController)
    m_scriptedIdleTaskController = ScriptedIdleTaskController::create(this);
  return *m_scriptedIdleTaskController;
}

int Document::requestIdleCallback(IdleRequestCallback* callback,
                                  const IdleRequestOptions& options) {
  return ensureScriptedIdleTaskController().registerCallback(callback, options);
}

void Document::cancelIdleCallback(int id) {
  if (!m_scriptedIdleTaskController)
    return;
  m_scriptedIdleTaskController->cancelCallback(id);
}

Touch* Document::createTouch(DOMWindow* window,
                             EventTarget* target,
                             int identifier,
                             double pageX,
                             double pageY,
                             double screenX,
                             double screenY,
                             double radiusX,
                             double radiusY,
                             float rotationAngle,
                             float force) const {
  // Match behavior from when these types were integers, and avoid surprises
  // from someone explicitly
  // passing Infinity/NaN.
  if (!std::isfinite(pageX))
    pageX = 0;
  if (!std::isfinite(pageY))
    pageY = 0;
  if (!std::isfinite(screenX))
    screenX = 0;
  if (!std::isfinite(screenY))
    screenY = 0;
  if (!std::isfinite(radiusX))
    radiusX = 0;
  if (!std::isfinite(radiusY))
    radiusY = 0;
  if (!std::isfinite(rotationAngle))
    rotationAngle = 0;
  if (!std::isfinite(force))
    force = 0;

  if (radiusX || radiusY || rotationAngle || force)
    UseCounter::count(*this,
                      UseCounter::DocumentCreateTouchMoreThanSevenArguments);

  // FIXME: It's not clear from the documentation at
  // http://developer.apple.com/library/safari/#documentation/UserExperience/Reference/DocumentAdditionsReference/DocumentAdditions/DocumentAdditions.html
  // when this method should throw and nor is it by inspection of iOS behavior.
  // It would be nice to verify any cases where it throws under iOS and
  // implement them here. See https://bugs.webkit.org/show_bug.cgi?id=47819
  LocalFrame* frame = window && window->isLocalDOMWindow()
                          ? blink::toLocalDOMWindow(window)->frame()
                          : this->frame();
  return Touch::create(frame, target, identifier, FloatPoint(screenX, screenY),
                       FloatPoint(pageX, pageY), FloatSize(radiusX, radiusY),
                       rotationAngle, force, String());
}

TouchList* Document::createTouchList(HeapVector<Member<Touch>>& touches) const {
  return TouchList::adopt(touches);
}

DocumentLoader* Document::loader() const {
  if (!m_frame)
    return 0;

  DocumentLoader* loader = m_frame->loader().documentLoader();
  if (!loader)
    return 0;

  if (m_frame->document() != this)
    return 0;

  return loader;
}

Node* eventTargetNodeForDocument(Document* doc) {
  if (!doc)
    return 0;
  Node* node = doc->focusedElement();
  if (!node && doc->isPluginDocument()) {
    PluginDocument* pluginDocument = toPluginDocument(doc);
    node = pluginDocument->pluginNode();
  }
  if (!node && doc->isHTMLDocument())
    node = doc->body();
  if (!node)
    node = doc->documentElement();
  return node;
}

void Document::adjustFloatQuadsForScrollAndAbsoluteZoom(
    Vector<FloatQuad>& quads,
    LayoutObject& layoutObject) {
  if (!view())
    return;

  LayoutRect visibleContentRect(view()->visibleContentRect());
  for (size_t i = 0; i < quads.size(); ++i) {
    quads[i].move(-FloatSize(visibleContentRect.x().toFloat(),
                             visibleContentRect.y().toFloat()));
    adjustFloatQuadForAbsoluteZoom(quads[i], layoutObject);
  }
}

void Document::adjustFloatRectForScrollAndAbsoluteZoom(
    FloatRect& rect,
    LayoutObject& layoutObject) {
  if (!view())
    return;

  LayoutRect visibleContentRect(view()->visibleContentRect());
  rect.move(-FloatSize(visibleContentRect.x().toFloat(),
                       visibleContentRect.y().toFloat()));
  adjustFloatRectForAbsoluteZoom(rect, layoutObject);
}

void Document::setThreadedParsingEnabledForTesting(bool enabled) {
  s_threadedParsingEnabledForTesting = enabled;
}

bool Document::threadedParsingEnabledForTesting() {
  return s_threadedParsingEnabledForTesting;
}

SnapCoordinator* Document::snapCoordinator() {
  if (RuntimeEnabledFeatures::cssScrollSnapPointsEnabled() &&
      !m_snapCoordinator)
    m_snapCoordinator = SnapCoordinator::create();

  return m_snapCoordinator.get();
}

void Document::setContextFeatures(ContextFeatures& features) {
  m_contextFeatures = &features;
}

static LayoutObject* nearestCommonHoverAncestor(LayoutObject* obj1,
                                                LayoutObject* obj2) {
  if (!obj1 || !obj2)
    return 0;

  for (LayoutObject* currObj1 = obj1; currObj1;
       currObj1 = currObj1->hoverAncestor()) {
    for (LayoutObject* currObj2 = obj2; currObj2;
         currObj2 = currObj2->hoverAncestor()) {
      if (currObj1 == currObj2)
        return currObj1;
    }
  }

  return 0;
}

void Document::updateHoverActiveState(const HitTestRequest& request,
                                      Element* innerElement,
                                      Scrollbar* hitScrollbar) {
  DCHECK(!request.readOnly());

  if (request.active() && m_frame)
    m_frame->eventHandler().notifyElementActivated();

  Element* innerElementInDocument = hitScrollbar ? nullptr : innerElement;
  // Replace the innerElementInDocument to be srollbar's parent when hit
  // scrollbar
  if (hitScrollbar) {
    ScrollableArea* scrollableArea = hitScrollbar->getScrollableArea();
    if (scrollableArea && scrollableArea->layoutBox() &&
        scrollableArea->layoutBox()->node() &&
        scrollableArea->layoutBox()->node()->isElementNode()) {
      innerElementInDocument =
          toElement(hitScrollbar->getScrollableArea()->layoutBox()->node());
    }
  }

  while (innerElementInDocument && innerElementInDocument->document() != this) {
    innerElementInDocument->document().updateHoverActiveState(
        request, innerElementInDocument, hitScrollbar);
    innerElementInDocument = innerElementInDocument->document().localOwner();
  }

  updateDistribution();
  Element* oldActiveElement = activeHoverElement();
  if (oldActiveElement && !request.active()) {
    // The oldActiveElement layoutObject is null, dropped on :active by setting
    // display: none, for instance. We still need to clear the ActiveChain as
    // the mouse is released.
    for (Node* node = oldActiveElement; node;
         node = FlatTreeTraversal::parent(*node)) {
      DCHECK(!node->isTextNode());
      node->setActive(false);
      m_userActionElements.setInActiveChain(node, false);
    }
    setActiveHoverElement(nullptr);
  } else {
    Element* newActiveElement = innerElementInDocument;
    if (!oldActiveElement && newActiveElement &&
        !newActiveElement->isDisabledFormControl() && request.active() &&
        !request.touchMove()) {
      // We are setting the :active chain and freezing it. If future moves
      // happen, they will need to reference this chain.
      for (Node* node = newActiveElement; node;
           node = FlatTreeTraversal::parent(*node)) {
        DCHECK(!node->isTextNode());
        m_userActionElements.setInActiveChain(node, true);
      }
      setActiveHoverElement(newActiveElement);
    }
  }
  // If the mouse has just been pressed, set :active on the chain. Those (and
  // only those) nodes should remain :active until the mouse is released.
  bool allowActiveChanges = !oldActiveElement && activeHoverElement();

  // If the mouse is down and if this is a mouse move event, we want to restrict
  // changes in :hover/:active to only apply to elements that are in the :active
  // chain that we froze at the time the mouse went down.
  bool mustBeInActiveChain = request.active() && request.move();

  Node* oldHoverNode = hoverNode();

  // Check to see if the hovered node has changed.
  // If it hasn't, we do not need to do anything.
  Node* newHoverNode = innerElementInDocument;
  while (newHoverNode && !newHoverNode->layoutObject())
    newHoverNode = newHoverNode->parentOrShadowHostNode();

  // Update our current hover node.
  setHoverNode(newHoverNode);

  // We have two different objects. Fetch their layoutObjects.
  LayoutObject* oldHoverObj =
      oldHoverNode ? oldHoverNode->layoutObject() : nullptr;
  LayoutObject* newHoverObj =
      newHoverNode ? newHoverNode->layoutObject() : nullptr;

  // Locate the common ancestor layout object for the two layoutObjects.
  LayoutObject* ancestor = nearestCommonHoverAncestor(oldHoverObj, newHoverObj);
  Node* ancestorNode(ancestor ? ancestor->node() : nullptr);

  HeapVector<Member<Node>, 32> nodesToRemoveFromChain;
  HeapVector<Member<Node>, 32> nodesToAddToChain;

  if (oldHoverObj != newHoverObj) {
    // If the old hovered node is not nil but it's layoutObject is, it was
    // probably detached as part of the :hover style (for instance by setting
    // display:none in the :hover pseudo-class). In this case, the old hovered
    // element (and its ancestors) must be updated, to ensure it's normal style
    // is re-applied.
    if (oldHoverNode && !oldHoverObj) {
      for (Node& node : NodeTraversal::inclusiveAncestorsOf(*oldHoverNode)) {
        if (!mustBeInActiveChain ||
            (node.isElementNode() && toElement(node).inActiveChain()))
          nodesToRemoveFromChain.push_back(node);
      }
    }

    // The old hover path only needs to be cleared up to (and not including) the
    // common ancestor;
    for (LayoutObject* curr = oldHoverObj; curr && curr != ancestor;
         curr = curr->hoverAncestor()) {
      if (curr->node() && !curr->isText() &&
          (!mustBeInActiveChain || curr->node()->inActiveChain()))
        nodesToRemoveFromChain.push_back(curr->node());
    }

    // TODO(mustaq): The two loops above may push a single node twice into
    // nodesToRemoveFromChain. There must be a better way.
  }

  // Now set the hover state for our new object up to the root.
  for (LayoutObject* curr = newHoverObj; curr; curr = curr->hoverAncestor()) {
    if (curr->node() && !curr->isText() &&
        (!mustBeInActiveChain || curr->node()->inActiveChain()))
      nodesToAddToChain.push_back(curr->node());
  }

  size_t removeCount = nodesToRemoveFromChain.size();
  for (size_t i = 0; i < removeCount; ++i) {
    nodesToRemoveFromChain[i]->setHovered(false);
  }

  bool sawCommonAncestor = false;
  size_t addCount = nodesToAddToChain.size();
  for (size_t i = 0; i < addCount; ++i) {
    // Elements past the common ancestor do not change hover state, but might
    // change active state.
    if (ancestorNode && nodesToAddToChain[i] == ancestorNode)
      sawCommonAncestor = true;
    if (allowActiveChanges)
      nodesToAddToChain[i]->setActive(true);
    if (!sawCommonAncestor || nodesToAddToChain[i] == m_hoverNode) {
      nodesToAddToChain[i]->setHovered(true);
    }
  }
}

bool Document::haveScriptBlockingStylesheetsLoaded() const {
  return m_styleEngine->haveScriptBlockingStylesheetsLoaded();
}

bool Document::haveRenderBlockingStylesheetsLoaded() const {
  if (RuntimeEnabledFeatures::cssInBodyDoesNotBlockPaintEnabled())
    return m_styleEngine->haveRenderBlockingStylesheetsLoaded();
  return m_styleEngine->haveScriptBlockingStylesheetsLoaded();
}

Locale& Document::getCachedLocale(const AtomicString& locale) {
  AtomicString localeKey = locale;
  if (locale.isEmpty() ||
      !RuntimeEnabledFeatures::langAttributeAwareFormControlUIEnabled())
    return Locale::defaultLocale();
  LocaleIdentifierToLocaleMap::AddResult result =
      m_localeCache.insert(localeKey, nullptr);
  if (result.isNewEntry)
    result.storedValue->value = Locale::create(localeKey);
  return *(result.storedValue->value);
}

AnimationClock& Document::animationClock() {
  DCHECK(page());
  return page()->animator().clock();
}

Document& Document::ensureTemplateDocument() {
  if (isTemplateDocument())
    return *this;

  if (m_templateDocument)
    return *m_templateDocument;

  if (isHTMLDocument()) {
    DocumentInit init = DocumentInit::fromContext(contextDocument(), blankURL())
                            .withNewRegistrationContext();
    m_templateDocument = HTMLDocument::create(init);
  } else {
    m_templateDocument = Document::create(DocumentInit(blankURL()));
  }

  m_templateDocument->m_templateDocumentHost = this;  // balanced in dtor.

  return *m_templateDocument.get();
}

void Document::didAssociateFormControl(Element* element) {
  if (!frame() || !frame()->page() || !loadEventFinished())
    return;

  // We add a slight delay because this could be called rapidly.
  if (!m_didAssociateFormControlsTimer.isActive())
    m_didAssociateFormControlsTimer.startOneShot(0.3, BLINK_FROM_HERE);
}

void Document::didAssociateFormControlsTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &m_didAssociateFormControlsTimer);
  if (!frame() || !frame()->page())
    return;

  frame()->page()->chromeClient().didAssociateFormControlsAfterLoad(frame());
}

float Document::devicePixelRatio() const {
  return m_frame ? m_frame->devicePixelRatio() : 1.0;
}

TextAutosizer* Document::textAutosizer() {
  if (!m_textAutosizer)
    m_textAutosizer = TextAutosizer::create(this);
  return m_textAutosizer.get();
}

void Document::setAutofocusElement(Element* element) {
  if (!element) {
    m_autofocusElement = nullptr;
    return;
  }
  if (m_hasAutofocused)
    return;
  m_hasAutofocused = true;
  DCHECK(!m_autofocusElement);
  m_autofocusElement = element;
  TaskRunnerHelper::get(TaskType::UserInteraction, this)
      ->postTask(BLINK_FROM_HERE,
                 WTF::bind(&runAutofocusTask, wrapWeakPersistent(this)));
}

Element* Document::activeElement() const {
  if (Element* element = adjustedFocusedElement())
    return element;
  return body();
}

bool Document::hasFocus() const {
  return page() && page()->focusController().isDocumentFocused(*this);
}

template <unsigned type>
bool shouldInvalidateNodeListCachesForAttr(
    const HeapHashSet<WeakMember<const LiveNodeListBase>> nodeLists[],
    const QualifiedName& attrName) {
  if (!nodeLists[type].isEmpty() &&
      LiveNodeListBase::shouldInvalidateTypeOnAttributeChange(
          static_cast<NodeListInvalidationType>(type), attrName))
    return true;
  return shouldInvalidateNodeListCachesForAttr<type + 1>(nodeLists, attrName);
}

template <>
bool shouldInvalidateNodeListCachesForAttr<numNodeListInvalidationTypes>(
    const HeapHashSet<WeakMember<const LiveNodeListBase>>[],
    const QualifiedName&) {
  return false;
}

bool Document::shouldInvalidateNodeListCaches(
    const QualifiedName* attrName) const {
  if (attrName) {
    return shouldInvalidateNodeListCachesForAttr<
        DoNotInvalidateOnAttributeChanges + 1>(m_nodeLists, *attrName);
  }

  for (int type = 0; type < numNodeListInvalidationTypes; ++type) {
    if (!m_nodeLists[type].isEmpty())
      return true;
  }

  return false;
}

void Document::invalidateNodeListCaches(const QualifiedName* attrName) {
  for (const LiveNodeListBase* list : m_listsInvalidatedAtDocument)
    list->invalidateCacheForAttribute(attrName);
}

void Document::platformColorsChanged() {
  if (!isActive())
    return;

  styleEngine().platformColorsChanged();
}

bool Document::isSecureContext(
    String& errorMessage,
    const SecureContextCheck privilegeContextCheck) const {
  if (!isSecureContext(privilegeContextCheck)) {
    errorMessage = SecurityOrigin::isPotentiallyTrustworthyErrorMessage();
    return false;
  }
  return true;
}

bool Document::isSecureContext(
    const SecureContextCheck privilegeContextCheck) const {
  bool isSecure = isSecureContextImpl(privilegeContextCheck);
  if (getSandboxFlags() != SandboxNone) {
    UseCounter::count(
        *this, isSecure
                   ? UseCounter::SecureContextCheckForSandboxedOriginPassed
                   : UseCounter::SecureContextCheckForSandboxedOriginFailed);
  }
  UseCounter::count(*this, isSecure ? UseCounter::SecureContextCheckPassed
                                    : UseCounter::SecureContextCheckFailed);
  return isSecure;
}

void Document::enforceInsecureRequestPolicy(WebInsecureRequestPolicy policy) {
  // Combine the new policy with the existing policy, as a base policy may be
  // inherited from a remote parent before this page's policy is set. In other
  // words, insecure requests should be upgraded or blocked if _either_ the
  // existing policy or the newly enforced policy triggers upgrades or
  // blockage.
  setInsecureRequestPolicy(getInsecureRequestPolicy() | policy);
  if (frame())
    frame()->loader().client()->didEnforceInsecureRequestPolicy(
        getInsecureRequestPolicy());
}

void Document::setShadowCascadeOrder(ShadowCascadeOrder order) {
  DCHECK_NE(order, ShadowCascadeOrder::ShadowCascadeNone);

  if (order == m_shadowCascadeOrder)
    return;

  if (order == ShadowCascadeOrder::ShadowCascadeV0) {
    m_mayContainV0Shadow = true;
    if (m_shadowCascadeOrder == ShadowCascadeOrder::ShadowCascadeV1)
      UseCounter::count(*this, UseCounter::MixedShadowRootV0AndV1);
  }

  // For V0 -> V1 upgrade, we need style recalculation for the whole document.
  if (m_shadowCascadeOrder == ShadowCascadeOrder::ShadowCascadeV0 &&
      order == ShadowCascadeOrder::ShadowCascadeV1) {
    this->setNeedsStyleRecalc(
        SubtreeStyleChange,
        StyleChangeReasonForTracing::create(StyleChangeReason::Shadow));
    UseCounter::count(*this, UseCounter::MixedShadowRootV0AndV1);
  }

  if (order > m_shadowCascadeOrder)
    m_shadowCascadeOrder = order;
}

LayoutViewItem Document::layoutViewItem() const {
  return LayoutViewItem(m_layoutView);
}

PropertyRegistry* Document::propertyRegistry() {
  // TODO(timloh): When the flag is removed, return a reference instead.
  if (!m_propertyRegistry && RuntimeEnabledFeatures::cssVariables2Enabled())
    m_propertyRegistry = PropertyRegistry::create();
  return m_propertyRegistry;
}

const PropertyRegistry* Document::propertyRegistry() const {
  return const_cast<Document*>(this)->propertyRegistry();
}

void Document::incrementPasswordCount() {
  ++m_passwordCount;
  if (isSecureContext() || m_passwordCount != 1) {
    // The browser process only cares about passwords on pages where the
    // top-level URL is not secure. Secure contexts must have a top-level
    // URL that is secure, so there is no need to send notifications for
    // password fields in secure contexts.
    //
    // Also, only send a message on the first visible password field; the
    // browser process doesn't care about the presence of additional
    // password fields beyond that.
    return;
  }
  sendSensitiveInputVisibility();
}

void Document::decrementPasswordCount() {
  DCHECK_GT(m_passwordCount, 0u);
  --m_passwordCount;
  if (isSecureContext() || m_passwordCount > 0)
    return;
  sendSensitiveInputVisibility();
}

DEFINE_TRACE(Document) {
  visitor->trace(m_importsController);
  visitor->trace(m_docType);
  visitor->trace(m_implementation);
  visitor->trace(m_autofocusElement);
  visitor->trace(m_focusedElement);
  visitor->trace(m_sequentialFocusNavigationStartingPoint);
  visitor->trace(m_hoverNode);
  visitor->trace(m_activeHoverElement);
  visitor->trace(m_documentElement);
  visitor->trace(m_rootScrollerController);
  visitor->trace(m_titleElement);
  visitor->trace(m_axObjectCache);
  visitor->trace(m_markers);
  visitor->trace(m_cssTarget);
  visitor->trace(m_currentScriptStack);
  visitor->trace(m_scriptRunner);
  visitor->trace(m_listsInvalidatedAtDocument);
  for (int i = 0; i < numNodeListInvalidationTypes; ++i)
    visitor->trace(m_nodeLists[i]);
  visitor->trace(m_topLayerElements);
  visitor->trace(m_elemSheet);
  visitor->trace(m_nodeIterators);
  visitor->trace(m_ranges);
  visitor->trace(m_styleEngine);
  visitor->trace(m_formController);
  visitor->trace(m_visitedLinkState);
  visitor->trace(m_frame);
  visitor->trace(m_domWindow);
  visitor->trace(m_fetcher);
  visitor->trace(m_parser);
  visitor->trace(m_contextFeatures);
  visitor->trace(m_styleSheetList);
  visitor->trace(m_documentTiming);
  visitor->trace(m_mediaQueryMatcher);
  visitor->trace(m_scriptedAnimationController);
  visitor->trace(m_scriptedIdleTaskController);
  visitor->trace(m_textAutosizer);
  visitor->trace(m_registrationContext);
  visitor->trace(m_customElementMicrotaskRunQueue);
  visitor->trace(m_elementDataCache);
  visitor->trace(m_useElementsNeedingUpdate);
  visitor->trace(m_timers);
  visitor->trace(m_templateDocument);
  visitor->trace(m_templateDocumentHost);
  visitor->trace(m_userActionElements);
  visitor->trace(m_svgExtensions);
  visitor->trace(m_timeline);
  visitor->trace(m_compositorPendingAnimations);
  visitor->trace(m_contextDocument);
  visitor->trace(m_canvasFontCache);
  visitor->trace(m_intersectionObserverController);
  visitor->trace(m_snapCoordinator);
  visitor->trace(m_resizeObserverController);
  visitor->trace(m_propertyRegistry);
  visitor->trace(m_styleReattachDataMap);
  visitor->trace(m_networkStateObserver);
  Supplementable<Document>::trace(visitor);
  TreeScope::trace(visitor);
  ContainerNode::trace(visitor);
  ExecutionContext::trace(visitor);
  SecurityContext::trace(visitor);
  SynchronousMutationNotifier::trace(visitor);
}

void Document::recordDeferredLoadReason(WouldLoadReason reason) {
  DCHECK(m_wouldLoadReason == Invalid || reason != Created);
  DCHECK(reason != Invalid);
  DCHECK(frame());
  DCHECK(frame()->isCrossOriginSubframe());
  if (reason <= m_wouldLoadReason ||
      !frame()->loader().stateMachine()->committedFirstRealDocumentLoad())
    return;
  for (int i = m_wouldLoadReason + 1; i <= reason; ++i)
    recordLoadReasonToHistogram(static_cast<WouldLoadReason>(i));
  m_wouldLoadReason = reason;
}

DEFINE_TRACE_WRAPPERS(Document) {
  // m_nodeLists are traced in their corresponding NodeListsNodeData, keeping
  // them only alive for live nodes. Otherwise we would keep lists of dead
  // nodes alive that have not yet been invalidated.
  visitor->traceWrappers(m_importsController);
  visitor->traceWrappers(m_implementation);
  visitor->traceWrappers(m_styleSheetList);
  visitor->traceWrappers(m_styleEngine);
  // Cannot trace in Supplementable<Document> as it is part of platform/ and
  // thus cannot refer to ScriptWrappableVisitor.
  visitor->traceWrappers(
      static_cast<FontFaceSet*>(Supplementable<Document>::m_supplements.at(
          FontFaceSet::supplementName())));
  ContainerNode::traceWrappers(visitor);
}

template class CORE_TEMPLATE_EXPORT Supplement<Document>;

}  // namespace blink

#ifndef NDEBUG
static WeakDocumentSet& liveDocumentSet() {
  DEFINE_STATIC_LOCAL(WeakDocumentSet, set, ());
  return set;
}

void showLiveDocumentInstances() {
  WeakDocumentSet& set = liveDocumentSet();
  fprintf(stderr, "There are %u documents currently alive:\n", set.size());
  for (blink::Document* document : set)
    fprintf(stderr, "- Document %p URL: %s\n", document,
            document->url().getString().utf8().data());
}
#endif
