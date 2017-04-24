/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#ifndef Document_h
#define Document_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/CoreExport.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DocumentEncodingData.h"
#include "core/dom/DocumentInit.h"
#include "core/dom/DocumentLifecycle.h"
#include "core/dom/DocumentTiming.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/StyleReattachData.h"
#include "core/dom/SynchronousMutationNotifier.h"
#include "core/dom/SynchronousMutationObserver.h"
#include "core/dom/Text.h"
#include "core/dom/TextLinkColors.h"
#include "core/dom/TreeScope.h"
#include "core/dom/UserActionElementSet.h"
#include "core/dom/ViewportDescription.h"
#include "core/dom/custom/V0CustomElement.h"
#include "core/frame/DOMTimerCoordinator.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/html/parser/ParserSynchronizationPolicy.h"
#include "core/page/PageVisibilityState.h"
#include "platform/Length.h"
#include "platform/Timer.h"
#include "platform/WebTaskRunner.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "public/platform/WebFocusType.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "wtf/Compiler.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include <memory>

namespace blink {

namespace mojom {
enum class EngagementLevel : int32_t;
}

class AnimationClock;
class DocumentTimeline;
class AXObjectCache;
class Attr;
class CDATASection;
class CSSStyleSheet;
class CanvasFontCache;
class ChromeClient;
class CompositorPendingAnimations;
class Comment;
class ComputedStyle;
class ConsoleMessage;
class ContextFeatures;
class V0CustomElementMicrotaskRunQueue;
class V0CustomElementRegistrationContext;
class DOMImplementation;
class DOMWindow;
class DocumentFragment;
class DocumentLoader;
class DocumentMarkerController;
class DocumentNameCollection;
class DocumentParser;
class DocumentState;
class DocumentType;
class Element;
class ElementDataCache;
class ElementRegistrationOptions;
class Event;
class EventFactoryBase;
class EventListener;
template <typename EventType>
class EventWithHitTestResults;
class ExceptionState;
class FloatQuad;
class FloatRect;
class FormController;
class FrameHost;
class FrameRequestCallback;
class FrameView;
class HTMLAllCollection;
class HTMLBodyElement;
class HTMLCollection;
class HTMLDialogElement;
class HTMLElement;
class HTMLFrameOwnerElement;
class HTMLHeadElement;
class HTMLImportLoader;
class HTMLImportsController;
class HTMLLinkElement;
class HTMLScriptElementOrSVGScriptElement;
class HitTestRequest;
class IdleRequestCallback;
class IdleRequestOptions;
class IntersectionObserverController;
class LayoutPoint;
class LayoutView;
class LayoutViewItem;
class LiveNodeListBase;
class LocalDOMWindow;
class Locale;
class LocalFrame;
class Location;
class MediaQueryListListener;
class MediaQueryMatcher;
class NodeFilter;
class NodeIterator;
class NthIndexCache;
class OriginAccessEntry;
class Page;
class ProcessingInstruction;
class PropertyRegistry;
class QualifiedName;
class Range;
class ResizeObserverController;
class ResourceFetcher;
class RootScrollerController;
class SVGDocumentExtensions;
class SVGUseElement;
class ScriptRunner;
class ScriptableDocumentParser;
class ScriptedAnimationController;
class ScriptedIdleTaskController;
class Scrollbar;
class SecurityOrigin;
class SegmentedString;
class SelectorQueryCache;
class SerializedScriptValue;
class Settings;
class SnapCoordinator;
class StringOrDictionary;
class StyleEngine;
class StyleResolver;
class StyleSheetList;
class TextAutosizer;
class Touch;
class TouchList;
class TransformSource;
class TreeWalker;
class VisitedLinkState;
class WebMouseEvent;
struct AnnotatedRegionValue;
struct FocusParams;
struct IconURL;

using MouseEventWithHitTestResults = EventWithHitTestResults<WebMouseEvent>;
using ExceptionCode = int;

enum NodeListInvalidationType {
  DoNotInvalidateOnAttributeChanges = 0,
  InvalidateOnClassAttrChange,
  InvalidateOnIdNameAttrChange,
  InvalidateOnNameAttrChange,
  InvalidateOnForAttrChange,
  InvalidateForFormControls,
  InvalidateOnHRefAttrChange,
  InvalidateOnAnyAttrChange,
};
const int numNodeListInvalidationTypes = InvalidateOnAnyAttrChange + 1;

enum DocumentClass {
  DefaultDocumentClass = 0,
  HTMLDocumentClass = 1,
  XHTMLDocumentClass = 1 << 1,
  ImageDocumentClass = 1 << 2,
  PluginDocumentClass = 1 << 3,
  MediaDocumentClass = 1 << 4,
  SVGDocumentClass = 1 << 5,
  XMLDocumentClass = 1 << 6,
};

enum ShadowCascadeOrder { ShadowCascadeNone, ShadowCascadeV0, ShadowCascadeV1 };

enum CreateElementFlags {
  CreatedByParser = 1 << 0,
  // Synchronous custom elements flag:
  // https://dom.spec.whatwg.org/#concept-create-element
  // TODO(kojii): Remove these flags, add an option not to queue upgrade, and
  // let parser/DOM methods to upgrade synchronously when necessary.
  SynchronousCustomElements = 0 << 1,
  AsynchronousCustomElements = 1 << 1,

  // Aliases by callers.
  // Clone a node: https://dom.spec.whatwg.org/#concept-node-clone
  CreatedByCloneNode = AsynchronousCustomElements,
  CreatedByImportNode = CreatedByCloneNode,
  // https://dom.spec.whatwg.org/#dom-document-createelement
  CreatedByCreateElement = SynchronousCustomElements,
  // https://html.spec.whatwg.org/#create-an-element-for-the-token
  CreatedByFragmentParser = CreatedByParser | AsynchronousCustomElements,
};

// Collect data about deferred loading of offscreen cross-origin documents. All
// cross-origin documents log Created. Only those that would load log a reason.
// We can then see the % of cross-origin documents that never have to load.
// See https://crbug.com/635105.
// Logged to UMA, don't re-arrange entries without creating a new histogram.
enum WouldLoadReason {
  Invalid,
  Created,
  WouldLoad3ScreensAway,
  WouldLoad2ScreensAway,
  WouldLoad1ScreenAway,
  WouldLoadVisible,
  // If outer and inner frames aren't in the same process we can't determine
  // if the inner frame is visible, so just load it.
  // TODO(dgrogan): Revisit after https://crbug.com/650433 is fixed.
  WouldLoadNoParent,

  WouldLoadReasonEnd
};

using DocumentClassFlags = unsigned char;

class CORE_EXPORT Document : public ContainerNode,
                             public TreeScope,
                             public SecurityContext,
                             public ExecutionContext,
                             public SynchronousMutationNotifier,
                             public Supplementable<Document> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Document);

 public:
  static Document* create(const DocumentInit& initializer = DocumentInit()) {
    return new Document(initializer);
  }
  ~Document() override;

  MediaQueryMatcher& mediaQueryMatcher();

  void mediaQueryAffectingValueChanged();

  using SecurityContext::getSecurityOrigin;
  using SecurityContext::contentSecurityPolicy;
  using TreeScope::getElementById;

  bool canContainRangeEndPoint() const override { return true; }

  SelectorQueryCache& selectorQueryCache();

  // Focus Management.
  Element* activeElement() const;
  bool hasFocus() const;

  // DOM methods & attributes for Document

  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(copy);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(cut);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(paste);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pointerlockchange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pointerlockerror);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(readystatechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(securitypolicyviolation);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectionchange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(wheel);

  bool shouldMergeWithLegacyDescription(ViewportDescription::Type) const;
  bool shouldOverrideLegacyDescription(ViewportDescription::Type) const;
  void setViewportDescription(const ViewportDescription&);
  ViewportDescription viewportDescription() const;
  Length viewportDefaultMinWidth() const { return m_viewportDefaultMinWidth; }

  String outgoingReferrer() const override;
  ReferrerPolicy getReferrerPolicy() const override;

  void setDoctype(DocumentType*);
  DocumentType* doctype() const { return m_docType.get(); }

  DOMImplementation& implementation();

  Element* documentElement() const { return m_documentElement.get(); }

  // Returns whether the Document has an AppCache manifest.
  bool hasAppCacheManifest() const;

  Location* location() const;

  Element* createElement(const AtomicString& name,
                         ExceptionState& = ASSERT_NO_EXCEPTION);
  DocumentFragment* createDocumentFragment();
  Text* createTextNode(const String& data);
  Comment* createComment(const String& data);
  CDATASection* createCDATASection(const String& data, ExceptionState&);
  ProcessingInstruction* createProcessingInstruction(const String& target,
                                                     const String& data,
                                                     ExceptionState&);
  Attr* createAttribute(const AtomicString& name, ExceptionState&);
  Attr* createAttributeNS(const AtomicString& namespaceURI,
                          const AtomicString& qualifiedName,
                          ExceptionState&,
                          bool shouldIgnoreNamespaceChecks = false);
  Node* importNode(Node* importedNode, bool deep, ExceptionState&);
  Element* createElementNS(const AtomicString& namespaceURI,
                           const AtomicString& qualifiedName,
                           ExceptionState&);
  Element* createElement(const QualifiedName&, CreateElementFlags);

  Element* elementFromPoint(int x, int y) const;
  HeapVector<Member<Element>> elementsFromPoint(int x, int y) const;
  Range* caretRangeFromPoint(int x, int y);
  Element* scrollingElement();
  // When calling from C++ code, use this method. scrollingElement() is
  // just for the web IDL implementation.
  Element* scrollingElementNoLayout();

  void addStyleReattachData(const Node&, StyleReattachData&);
  StyleReattachData getStyleReattachData(const Node&) const;

  String readyState() const;

  AtomicString characterSet() const { return Document::encodingName(); }

  AtomicString encodingName() const;

  void setContent(const String&);

  String suggestedMIMEType() const;
  void setMimeType(const AtomicString&);
  AtomicString contentType() const;  // DOM 4 document.contentType

  const AtomicString& contentLanguage() const { return m_contentLanguage; }
  void setContentLanguage(const AtomicString&);

  String xmlEncoding() const { return m_xmlEncoding; }
  String xmlVersion() const { return m_xmlVersion; }
  enum StandaloneStatus { StandaloneUnspecified, Standalone, NotStandalone };
  bool xmlStandalone() const { return m_xmlStandalone == Standalone; }
  StandaloneStatus xmlStandaloneStatus() const {
    return static_cast<StandaloneStatus>(m_xmlStandalone);
  }
  bool hasXMLDeclaration() const { return m_hasXMLDeclaration; }

  void setXMLEncoding(const String& encoding) {
    m_xmlEncoding = encoding;
  }  // read-only property, only to be set from XMLDocumentParser
  void setXMLVersion(const String&, ExceptionState&);
  void setXMLStandalone(bool, ExceptionState&);
  void setHasXMLDeclaration(bool hasXMLDeclaration) {
    m_hasXMLDeclaration = hasXMLDeclaration ? 1 : 0;
  }

  String origin() const { return getSecurityOrigin()->toString(); }
  String suborigin() const {
    return getSecurityOrigin()->hasSuborigin()
               ? getSecurityOrigin()->suborigin()->name()
               : String();
  }

  String visibilityState() const;
  PageVisibilityState pageVisibilityState() const;
  bool hidden() const;
  void didChangeVisibilityState();

  // If the document is "prefetch only", it will not be fully contstructed,
  // and should never be displayed. Only a few resources will be loaded and
  // scanned, in order to warm up caches.
  bool isPrefetchOnly() const;

  Node* adoptNode(Node* source, ExceptionState&);

  HTMLCollection* images();
  HTMLCollection* embeds();
  HTMLCollection* applets();
  HTMLCollection* links();
  HTMLCollection* forms();
  HTMLCollection* anchors();
  HTMLCollection* scripts();
  HTMLAllCollection* all();

  HTMLCollection* windowNamedItems(const AtomicString& name);
  DocumentNameCollection* documentNamedItems(const AtomicString& name);

  bool isHTMLDocument() const { return m_documentClasses & HTMLDocumentClass; }
  bool isXHTMLDocument() const {
    return m_documentClasses & XHTMLDocumentClass;
  }
  bool isXMLDocument() const { return m_documentClasses & XMLDocumentClass; }
  bool isImageDocument() const {
    return m_documentClasses & ImageDocumentClass;
  }
  bool isSVGDocument() const { return m_documentClasses & SVGDocumentClass; }
  bool isPluginDocument() const {
    return m_documentClasses & PluginDocumentClass;
  }
  bool isMediaDocument() const {
    return m_documentClasses & MediaDocumentClass;
  }

  bool hasSVGRootNode() const;

  bool isFrameSet() const;

  bool isSrcdocDocument() const { return m_isSrcdocDocument; }
  bool isMobileDocument() const { return m_isMobileDocument; }

  StyleResolver* styleResolver() const;
  StyleResolver& ensureStyleResolver() const;

  bool isViewSource() const { return m_isViewSource; }
  void setIsViewSource(bool);

  bool sawElementsInKnownNamespaces() const {
    return m_sawElementsInKnownNamespaces;
  }

  bool canExecuteScripts(ReasonForCallingCanExecuteScripts) override;
  bool isRenderingReady() const {
    return haveImportsLoaded() && haveRenderBlockingStylesheetsLoaded();
  }
  bool isScriptExecutionReady() const {
    return haveImportsLoaded() && haveScriptBlockingStylesheetsLoaded();
  }

  // This is a DOM function.
  StyleSheetList& styleSheets();

  StyleEngine& styleEngine() {
    DCHECK(m_styleEngine.get());
    return *m_styleEngine.get();
  }

  bool gotoAnchorNeededAfterStylesheetsLoad() {
    return m_gotoAnchorNeededAfterStylesheetsLoad;
  }
  void setGotoAnchorNeededAfterStylesheetsLoad(bool b) {
    m_gotoAnchorNeededAfterStylesheetsLoad = b;
  }

  void scheduleUseShadowTreeUpdate(SVGUseElement&);
  void unscheduleUseShadowTreeUpdate(SVGUseElement&);

  void evaluateMediaQueryList();

  FormController& formController();
  DocumentState* formElementsState() const;
  void setStateForNewFormElements(const Vector<String>&);

  FrameView* view() const;                       // can be null
  LocalFrame* frame() const { return m_frame; }  // can be null
  FrameHost* frameHost() const;                  // can be null
  Page* page() const;                            // can be null
  Settings* settings() const;                    // can be null

  float devicePixelRatio() const;

  Range* createRange();

  NodeIterator* createNodeIterator(Node* root,
                                   unsigned whatToShow,
                                   NodeFilter*);
  TreeWalker* createTreeWalker(Node* root, unsigned whatToShow, NodeFilter*);

  // Special support for editing
  Text* createEditingTextNode(const String&);

  void setupFontBuilder(ComputedStyle& documentStyle);

  bool needsLayoutTreeUpdate() const;
  bool needsLayoutTreeUpdateForNode(const Node&) const;
  // Update ComputedStyles and attach LayoutObjects if necessary, but don't
  // lay out.
  void updateStyleAndLayoutTree();
  // Same as updateStyleAndLayoutTree() except ignoring pending stylesheets.
  void updateStyleAndLayoutTreeIgnorePendingStylesheets();
  void updateStyleAndLayoutTreeForNode(const Node*);
  void updateStyleAndLayout();
  void layoutUpdated();
  enum RunPostLayoutTasks {
    RunPostLayoutTasksAsyhnchronously,
    RunPostLayoutTasksSynchronously,
  };
  void updateStyleAndLayoutIgnorePendingStylesheets(
      RunPostLayoutTasks = RunPostLayoutTasksAsyhnchronously);
  void updateStyleAndLayoutIgnorePendingStylesheetsForNode(Node*);
  PassRefPtr<ComputedStyle> styleForElementIgnoringPendingStylesheets(Element*);
  PassRefPtr<ComputedStyle> styleForPage(int pageIndex);

  // Returns true if page box (margin boxes and page borders) is visible.
  bool isPageBoxVisible(int pageIndex);

  // Returns the preferred page size and margins in pixels, assuming 96
  // pixels per inch. pageSize, marginTop, marginRight, marginBottom,
  // marginLeft must be initialized to the default values that are used if
  // auto is specified.
  void pageSizeAndMarginsInPixels(int pageIndex,
                                  DoubleSize& pageSize,
                                  int& marginTop,
                                  int& marginRight,
                                  int& marginBottom,
                                  int& marginLeft);

  ResourceFetcher* fetcher() const { return m_fetcher.get(); }

  void initialize();
  virtual void shutdown();

  void attachLayoutTree(const AttachContext& = AttachContext()) override {
    NOTREACHED();
  }
  void detachLayoutTree(const AttachContext& = AttachContext()) override {
    NOTREACHED();
  }

  // If you have a Document, use layoutView() instead which is faster.
  void layoutObject() const = delete;

  LayoutView* layoutView() const { return m_layoutView; }
  LayoutViewItem layoutViewItem() const;

  Document& axObjectCacheOwner() const;
  AXObjectCache* existingAXObjectCache() const;
  AXObjectCache* axObjectCache() const;
  void clearAXObjectCache();

  // to get visually ordered hebrew and arabic pages right
  bool visuallyOrdered() const { return m_visuallyOrdered; }

  DocumentLoader* loader() const;

  // This is the DOM API document.open(). enteredDocument is the responsible
  // document of the entry settings object.
  void open(Document* enteredDocument, ExceptionState&);
  // This is used internally and does not handle exceptions.
  void open();
  DocumentParser* implicitOpen(ParserSynchronizationPolicy);

  // This is the DOM API document.close()
  void close(ExceptionState&);
  // This is used internally and does not handle exceptions.
  void close();
  // implicitClose() actually does the work of closing the input stream.
  void implicitClose();

  bool dispatchBeforeUnloadEvent(ChromeClient&,
                                 bool isReload,
                                 bool& didAllowNavigation);
  void dispatchUnloadEvents();

  enum PageDismissalType {
    NoDismissal,
    BeforeUnloadDismissal,
    PageHideDismissal,
    UnloadVisibilityChangeDismissal,
    UnloadDismissal
  };
  PageDismissalType pageDismissalEventBeingDispatched() const;

  void cancelParsing();

  void write(const SegmentedString& text,
             Document* enteredDocument = nullptr,
             ExceptionState& = ASSERT_NO_EXCEPTION);
  void write(const String& text,
             Document* enteredDocument = nullptr,
             ExceptionState& = ASSERT_NO_EXCEPTION);
  void writeln(const String& text,
               Document* enteredDocument = nullptr,
               ExceptionState& = ASSERT_NO_EXCEPTION);
  void write(LocalDOMWindow*, const Vector<String>& text, ExceptionState&);
  void writeln(LocalDOMWindow*, const Vector<String>& text, ExceptionState&);

  bool wellFormed() const { return m_wellFormed; }

  const KURL& url() const { return m_url; }
  void setURL(const KURL&);

  // To understand how these concepts relate to one another, please see the
  // comments surrounding their declaration.
  const KURL& baseURL() const { return m_baseURL; }
  void setBaseURLOverride(const KURL&);
  const KURL& baseURLOverride() const { return m_baseURLOverride; }
  KURL validBaseElementURL() const;
  const AtomicString& baseTarget() const { return m_baseTarget; }
  void processBaseElement();

  // Creates URL based on passed relative url and this documents base URL.
  // Depending on base URL value it is possible that parent document
  // base URL will be used instead. Uses completeURLWithOverride internally.
  KURL completeURL(const String&) const;
  // Creates URL based on passed relative url and passed base URL override.
  // Depending on baseURLOverride value it is possible that parent document
  // base URL will be used instead of it. See baseURLForOverride function
  // for details.
  KURL completeURLWithOverride(const String&,
                               const KURL& baseURLOverride) const;
  // Determines which base URL should be used given specified override.
  // If override is empty or is about:blank url and parent document exists
  // base URL of parent will be returned, passed base URL override otherwise.
  const KURL& baseURLForOverride(const KURL& baseURLOverride) const;

  String userAgent() const final;
  void disableEval(const String& errorMessage) final;

  CSSStyleSheet& elementSheet();

  virtual DocumentParser* createParser();
  DocumentParser* parser() const { return m_parser.get(); }
  ScriptableDocumentParser* scriptableDocumentParser() const;

  // FinishingPrinting denotes that the non-printing layout state is being
  // restored.
  enum PrintingState { NotPrinting, Printing, FinishingPrinting };
  bool printing() const { return m_printing == Printing; }
  bool finishingOrIsPrinting() {
    return m_printing == Printing || m_printing == FinishingPrinting;
  }
  void setPrinting(PrintingState state) { m_printing = state; }

  bool paginatedForScreen() const { return m_paginatedForScreen; }
  void setPaginatedForScreen(bool p) { m_paginatedForScreen = p; }

  bool paginated() const { return printing() || paginatedForScreen(); }

  enum CompatibilityMode { QuirksMode, LimitedQuirksMode, NoQuirksMode };

  void setCompatibilityMode(CompatibilityMode);
  CompatibilityMode getCompatibilityMode() const { return m_compatibilityMode; }

  String compatMode() const;

  bool inQuirksMode() const { return m_compatibilityMode == QuirksMode; }
  bool inLimitedQuirksMode() const {
    return m_compatibilityMode == LimitedQuirksMode;
  }
  bool inNoQuirksMode() const { return m_compatibilityMode == NoQuirksMode; }

  // https://html.spec.whatwg.org/multipage/dom.html#documentreadystate
  enum DocumentReadyState { Loading, Interactive, Complete };

  void setReadyState(DocumentReadyState);
  bool isLoadCompleted();

  enum ParsingState { Parsing, InDOMContentLoaded, FinishedParsing };
  void setParsingState(ParsingState);
  bool parsing() const { return m_parsingState == Parsing; }
  bool isInDOMContentLoaded() const {
    return m_parsingState == InDOMContentLoaded;
  }
  bool hasFinishedParsing() const { return m_parsingState == FinishedParsing; }

  bool shouldScheduleLayout() const;
  int elapsedTime() const;

  TextLinkColors& textLinkColors() { return m_textLinkColors; }
  VisitedLinkState& visitedLinkState() const { return *m_visitedLinkState; }

  MouseEventWithHitTestResults performMouseEventHitTest(const HitTestRequest&,
                                                        const LayoutPoint&,
                                                        const WebMouseEvent&);

  /* Newly proposed CSS3 mechanism for selecting alternate
       stylesheets using the DOM. May be subject to change as
       spec matures. - dwh
    */
  String preferredStylesheetSet() const;
  String selectedStylesheetSet() const;
  void setSelectedStylesheetSet(const String&);

  bool setFocusedElement(Element*, const FocusParams&);
  void clearFocusedElement();
  Element* focusedElement() const { return m_focusedElement.get(); }
  UserActionElementSet& userActionElements() { return m_userActionElements; }
  const UserActionElementSet& userActionElements() const {
    return m_userActionElements;
  }
  void setAutofocusElement(Element*);
  Element* autofocusElement() const { return m_autofocusElement.get(); }
  void setSequentialFocusNavigationStartingPoint(Node*);
  Element* sequentialFocusNavigationStartingPoint(WebFocusType) const;

  void setActiveHoverElement(Element*);
  Element* activeHoverElement() const { return m_activeHoverElement.get(); }

  Node* hoverNode() const { return m_hoverNode.get(); }

  void removeFocusedElementOfSubtree(Node*, bool amongChildrenOnly = false);
  void hoveredNodeDetached(Element&);
  void activeChainNodeDetached(Element&);

  void updateHoverActiveState(const HitTestRequest&, Element*, Scrollbar*);

  // Updates for :target (CSS3 selector).
  void setCSSTarget(Element*);
  Element* cssTarget() const { return m_cssTarget; }

  void scheduleLayoutTreeUpdateIfNeeded();
  bool hasPendingForcedStyleRecalc() const;

  void registerNodeList(const LiveNodeListBase*);
  void unregisterNodeList(const LiveNodeListBase*);
  void registerNodeListWithIdNameCache(const LiveNodeListBase*);
  void unregisterNodeListWithIdNameCache(const LiveNodeListBase*);
  bool shouldInvalidateNodeListCaches(
      const QualifiedName* attrName = nullptr) const;
  void invalidateNodeListCaches(const QualifiedName* attrName);

  void attachNodeIterator(NodeIterator*);
  void detachNodeIterator(NodeIterator*);
  void moveNodeIteratorsToNewDocument(Node&, Document&);

  void attachRange(Range*);
  void detachRange(Range*);

  void didMoveTreeToNewDocument(const Node& root);
  // nodeChildrenWillBeRemoved is used when removing all node children at once.
  void nodeChildrenWillBeRemoved(ContainerNode&);
  // nodeWillBeRemoved is only safe when removing one node at a time.
  void nodeWillBeRemoved(Node&);
  bool canAcceptChild(const Node& newChild,
                      const Node* oldChild,
                      ExceptionState&) const;

  void didInsertText(Node*, unsigned offset, unsigned length);
  void didRemoveText(Node*, unsigned offset, unsigned length);
  void didMergeTextNodes(const Text& mergedNode,
                         const Text& nodeToBeRemoved,
                         unsigned oldLength);
  void didSplitTextNode(const Text& oldNode);

  void clearDOMWindow() { m_domWindow = nullptr; }
  LocalDOMWindow* domWindow() const { return m_domWindow; }

  // Helper functions for forwarding LocalDOMWindow event related tasks to the
  // LocalDOMWindow if it exists.
  void setWindowAttributeEventListener(const AtomicString& eventType,
                                       EventListener*);
  EventListener* getWindowAttributeEventListener(const AtomicString& eventType);

  static void registerEventFactory(std::unique_ptr<EventFactoryBase>);
  static Event* createEvent(ScriptState*,
                            const String& eventType,
                            ExceptionState&);

  // keep track of what types of event listeners are registered, so we don't
  // dispatch events unnecessarily
  enum ListenerType {
    DOMSUBTREEMODIFIED_LISTENER = 1,
    DOMNODEINSERTED_LISTENER = 1 << 1,
    DOMNODEREMOVED_LISTENER = 1 << 2,
    DOMNODEREMOVEDFROMDOCUMENT_LISTENER = 1 << 3,
    DOMNODEINSERTEDINTODOCUMENT_LISTENER = 1 << 4,
    DOMCHARACTERDATAMODIFIED_LISTENER = 1 << 5,
    ANIMATIONEND_LISTENER = 1 << 6,
    ANIMATIONSTART_LISTENER = 1 << 7,
    ANIMATIONITERATION_LISTENER = 1 << 8,
    TRANSITIONEND_LISTENER = 1 << 9,
    SCROLL_LISTENER = 1 << 10
    // 5 bits remaining
  };

  bool hasListenerType(ListenerType listenerType) const {
    return (m_listenerTypes & listenerType);
  }
  void addListenerTypeIfNeeded(const AtomicString& eventType);

  bool hasMutationObserversOfType(MutationObserver::MutationType type) const {
    return m_mutationObserverTypes & type;
  }
  bool hasMutationObservers() const { return m_mutationObserverTypes; }
  void addMutationObserverTypes(MutationObserverOptions types) {
    m_mutationObserverTypes |= types;
  }

  IntersectionObserverController* intersectionObserverController();
  IntersectionObserverController& ensureIntersectionObserverController();

  ResizeObserverController* resizeObserverController() const {
    return m_resizeObserverController;
  }
  ResizeObserverController& ensureResizeObserverController();

  void updateViewportDescription();

  // Returns the owning element in the parent document. Returns nullptr if
  // this is the top level document or the owner is remote.
  HTMLFrameOwnerElement* localOwner() const;

  void willChangeFrameOwnerProperties(int marginWidth,
                                      int marginHeight,
                                      ScrollbarMode);

  // Returns true if this document belongs to a frame that the parent document
  // made invisible (for instance by setting as style display:none).
  bool isInInvisibleSubframe() const;

  String title() const { return m_title; }
  void setTitle(const String&);

  Element* titleElement() const { return m_titleElement.get(); }
  void setTitleElement(Element*);
  void removeTitle(Element* titleElement);

  const AtomicString& dir();
  void setDir(const AtomicString&);

  String cookie(ExceptionState&) const;
  void setCookie(const String&, ExceptionState&);

  const AtomicString& referrer() const;

  String domain() const;
  void setDomain(const String& newDomain, ExceptionState&);

  String lastModified() const;

  // The cookieURL is used to query the cookie database for this document's
  // cookies. For example, if the cookie URL is http://example.com, we'll
  // use the non-Secure cookies for example.com when computing
  // document.cookie.
  //
  // Q: How is the cookieURL different from the document's URL?
  // A: The two URLs are the same almost all the time.  However, if one
  //    document inherits the security context of another document, it
  //    inherits its cookieURL but not its URL.
  //
  const KURL& cookieURL() const { return m_cookieURL; }
  void setCookieURL(const KURL& url) { m_cookieURL = url; }

  const KURL firstPartyForCookies() const;

  // The following implements the rule from HTML 4 for what valid names are.
  // To get this right for all the XML cases, we probably have to improve this
  // or move it and make it sensitive to the type of document.
  static bool isValidName(const String&);

  // The following breaks a qualified name into a prefix and a local name.
  // It also does a validity check, and returns false if the qualified name
  // is invalid.  It also sets ExceptionCode when name is invalid.
  static bool parseQualifiedName(const AtomicString& qualifiedName,
                                 AtomicString& prefix,
                                 AtomicString& localName,
                                 ExceptionState&);

  // Checks to make sure prefix and namespace do not conflict (per DOM Core 3)
  static bool hasValidNamespaceForElements(const QualifiedName&);
  static bool hasValidNamespaceForAttributes(const QualifiedName&);

  // "body element" as defined by HTML5
  // (https://html.spec.whatwg.org/multipage/dom.html#the-body-element-2).
  // That is, the first body or frameset child of the document element.
  HTMLElement* body() const;

  // "HTML body element" as defined by CSSOM View spec
  // (http://dev.w3.org/csswg/cssom-view/#the-html-body-element).
  // That is, the first body child of the document element.
  HTMLBodyElement* firstBodyElement() const;

  void setBody(HTMLElement*, ExceptionState&);
  void willInsertBody();

  HTMLHeadElement* head() const;

  // Decide which element is to define the viewport's overflow policy. If
  // |rootStyle| is set, use that as the style for the root element, rather than
  // obtaining it on our own. The reason for this is that style may not have
  // been associated with the elements yet - in which case it may have been
  // calculated on the fly (without associating it with the actual element)
  // somewhere.
  Element* viewportDefiningElement(
      const ComputedStyle* rootStyle = nullptr) const;

  DocumentMarkerController& markers() const { return *m_markers; }

  // Support for Javascript execCommand, and related methods
  // See "core/editing/commands/DocumentExecCommand.cpp" for implementations.
  bool execCommand(const String& command,
                   bool showUI,
                   const String& value,
                   ExceptionState&);
  bool isRunningExecCommand() const { return m_isRunningExecCommand; }
  bool queryCommandEnabled(const String& command, ExceptionState&);
  bool queryCommandIndeterm(const String& command, ExceptionState&);
  bool queryCommandState(const String& command, ExceptionState&);
  bool queryCommandSupported(const String& command, ExceptionState&);
  String queryCommandValue(const String& command, ExceptionState&);

  KURL openSearchDescriptionURL();

  // designMode support
  bool inDesignMode() const { return m_designMode; }
  String designMode() const;
  void setDesignMode(const String&);

  Document* parentDocument() const;
  Document& topDocument() const;
  Document* contextDocument();

  ScriptRunner* scriptRunner() { return m_scriptRunner.get(); }

  Element* currentScript() const {
    return !m_currentScriptStack.isEmpty() ? m_currentScriptStack.back().get()
                                           : nullptr;
  }
  void currentScriptForBinding(HTMLScriptElementOrSVGScriptElement&) const;
  void pushCurrentScript(Element*);
  void popCurrentScript();

  void setTransformSource(std::unique_ptr<TransformSource>);
  TransformSource* transformSource() const { return m_transformSource.get(); }

  void incDOMTreeVersion() {
    DCHECK(m_lifecycle.stateAllowsTreeMutations());
    m_domTreeVersion = ++s_globalTreeVersion;
  }
  uint64_t domTreeVersion() const { return m_domTreeVersion; }

  uint64_t styleVersion() const { return m_styleVersion; }

  enum PendingSheetLayout {
    NoLayoutWithPendingSheets,
    DidLayoutWithPendingSheets,
    IgnoreLayoutWithPendingSheets
  };

  bool didLayoutWithPendingStylesheets() const {
    return m_pendingSheetLayout == DidLayoutWithPendingSheets;
  }
  bool ignoreLayoutWithPendingStylesheets() const {
    return m_pendingSheetLayout == IgnoreLayoutWithPendingSheets;
  }

  bool hasNodesWithPlaceholderStyle() const {
    return m_hasNodesWithPlaceholderStyle;
  }
  void setHasNodesWithPlaceholderStyle() {
    m_hasNodesWithPlaceholderStyle = true;
  }

  Vector<IconURL> iconURLs(int iconTypesMask);

  Color themeColor() const;

  // Returns the HTMLLinkElement currently in use for the Web Manifest.
  // Returns null if there is no such element.
  HTMLLinkElement* linkManifest() const;

  void updateFocusAppearanceLater();
  void cancelFocusAppearanceUpdate();

  bool isDNSPrefetchEnabled() const { return m_isDNSPrefetchEnabled; }
  void parseDNSPrefetchControlHeader(const String&);

  void postTask(TaskType,
                const WebTraceLocation&,
                std::unique_ptr<ExecutionContextTask>,
                const String& taskNameForInstrumentation = emptyString)
      override;  // Executes the task on context's thread asynchronously.

  void tasksWereSuspended() final;
  void tasksWereResumed() final;
  bool tasksNeedSuspension() final;

  void finishedParsing();

  void setEncodingData(const DocumentEncodingData& newData);
  const WTF::TextEncoding& encoding() const {
    return m_encodingData.encoding();
  }

  bool encodingWasDetectedHeuristically() const {
    return m_encodingData.wasDetectedHeuristically();
  }
  bool sawDecodingError() const { return m_encodingData.sawDecodingError(); }

  void setAnnotatedRegionsDirty(bool f) { m_annotatedRegionsDirty = f; }
  bool annotatedRegionsDirty() const { return m_annotatedRegionsDirty; }
  bool hasAnnotatedRegions() const { return m_hasAnnotatedRegions; }
  void setHasAnnotatedRegions(bool f) { m_hasAnnotatedRegions = f; }
  const Vector<AnnotatedRegionValue>& annotatedRegions() const;
  void setAnnotatedRegions(const Vector<AnnotatedRegionValue>&);

  void removeAllEventListeners() final;

  const SVGDocumentExtensions* svgExtensions();
  SVGDocumentExtensions& accessSVGExtensions();

  void initContentSecurityPolicy(ContentSecurityPolicy* = nullptr);

  bool isSecureTransitionTo(const KURL&) const;

  bool allowInlineEventHandler(Node*,
                               EventListener*,
                               const String& contextURL,
                               const WTF::OrdinalNumber& contextLine);
  bool allowExecutingScripts(Node*);

  void enforceSandboxFlags(SandboxFlags mask) override;

  void statePopped(PassRefPtr<SerializedScriptValue>);

  enum LoadEventProgress {
    LoadEventNotRun,
    LoadEventInProgress,
    LoadEventCompleted,
    BeforeUnloadEventInProgress,
    BeforeUnloadEventCompleted,
    PageHideInProgress,
    UnloadVisibilityChangeInProgress,
    UnloadEventInProgress,
    UnloadEventHandled
  };
  bool loadEventStillNeeded() const {
    return m_loadEventProgress == LoadEventNotRun;
  }
  bool processingLoadEvent() const {
    return m_loadEventProgress == LoadEventInProgress;
  }
  bool loadEventFinished() const {
    return m_loadEventProgress >= LoadEventCompleted;
  }
  bool unloadStarted() const {
    return m_loadEventProgress >= PageHideInProgress;
  }
  bool processingBeforeUnload() const {
    return m_loadEventProgress == BeforeUnloadEventInProgress;
  }
  void suppressLoadEvent();

  void setContainsPlugins() { m_containsPlugins = true; }
  bool containsPlugins() const { return m_containsPlugins; }

  bool isContextThread() const final;
  bool isJSExecutionForbidden() const final { return false; }

  bool containsValidityStyleRules() const {
    return m_containsValidityStyleRules;
  }
  void setContainsValidityStyleRules() { m_containsValidityStyleRules = true; }

  void enqueueResizeEvent();
  void enqueueScrollEventForNode(Node*);
  void enqueueAnimationFrameTask(std::unique_ptr<WTF::Closure>);
  void enqueueAnimationFrameEvent(Event*);
  // Only one event for a target/event type combination will be dispatched per
  // frame.
  void enqueueUniqueAnimationFrameEvent(Event*);
  void enqueueMediaQueryChangeListeners(
      HeapVector<Member<MediaQueryListListener>>&);
  void enqueueVisualViewportScrollEvent();
  void enqueueVisualViewportResizeEvent();

  void dispatchEventsForPrinting();

  bool hasFullscreenSupplement() const { return m_hasFullscreenSupplement; }
  void setHasFullscreenSupplement() { m_hasFullscreenSupplement = true; }

  void exitPointerLock();
  Element* pointerLockElement() const;

  // Used to allow element that loads data without going through a FrameLoader
  // to delay the 'load' event.
  void incrementLoadEventDelayCount() { ++m_loadEventDelayCount; }
  void decrementLoadEventDelayCount();
  void checkLoadEventSoon();
  bool isDelayingLoadEvent();
  void loadPluginsSoon();
  // This calls checkCompleted() sync and thus can cause JavaScript execution.
  void decrementLoadEventDelayCountAndCheckLoadEvent();

  Touch* createTouch(DOMWindow*,
                     EventTarget*,
                     int identifier,
                     double pageX,
                     double pageY,
                     double screenX,
                     double screenY,
                     double radiusX,
                     double radiusY,
                     float rotationAngle,
                     float force) const;
  TouchList* createTouchList(HeapVector<Member<Touch>>&) const;

  const DocumentTiming& timing() const { return m_documentTiming; }

  int requestAnimationFrame(FrameRequestCallback*);
  void cancelAnimationFrame(int id);
  void serviceScriptedAnimations(double monotonicAnimationStartTime);

  int requestIdleCallback(IdleRequestCallback*, const IdleRequestOptions&);
  void cancelIdleCallback(int id);

  EventTarget* errorEventTarget() final;
  void exceptionThrown(ErrorEvent*) final;

  void initDNSPrefetch();

  bool isInDocumentWrite() const { return m_writeRecursionDepth > 0; }

  TextAutosizer* textAutosizer();

  Element* createElement(const AtomicString& localName,
                         const StringOrDictionary&,
                         ExceptionState& = ASSERT_NO_EXCEPTION);
  Element* createElementNS(const AtomicString& namespaceURI,
                           const AtomicString& qualifiedName,
                           const StringOrDictionary&,
                           ExceptionState&);
  ScriptValue registerElement(
      ScriptState*,
      const AtomicString& name,
      const ElementRegistrationOptions&,
      ExceptionState&,
      V0CustomElement::NameSet validNames = V0CustomElement::StandardNames);
  V0CustomElementRegistrationContext* registrationContext() {
    return m_registrationContext.get();
  }
  V0CustomElementMicrotaskRunQueue* customElementMicrotaskRunQueue();

  void clearImportsController();
  void createImportsController();
  HTMLImportsController* importsController() const {
    return m_importsController;
  }
  HTMLImportLoader* importLoader() const;

  bool haveImportsLoaded() const;
  void didLoadAllImports();

  void adjustFloatQuadsForScrollAndAbsoluteZoom(Vector<FloatQuad>&,
                                                LayoutObject&);
  void adjustFloatRectForScrollAndAbsoluteZoom(FloatRect&, LayoutObject&);

  void setContextFeatures(ContextFeatures&);
  ContextFeatures& contextFeatures() const { return *m_contextFeatures; }

  ElementDataCache* elementDataCache() { return m_elementDataCache.get(); }

  void didLoadAllScriptBlockingResources();
  void didAddPendingStylesheetInBody();
  void didRemoveAllPendingStylesheet();
  void didRemoveAllPendingBodyStylesheets();

  bool inStyleRecalc() const {
    return m_lifecycle.state() == DocumentLifecycle::InStyleRecalc;
  }

  // Return a Locale for the default locale if the argument is null or empty.
  Locale& getCachedLocale(const AtomicString& locale = nullAtom);

  AnimationClock& animationClock();
  DocumentTimeline& timeline() const { return *m_timeline; }
  CompositorPendingAnimations& compositorPendingAnimations() {
    return *m_compositorPendingAnimations;
  }

  void addToTopLayer(Element*, const Element* before = nullptr);
  void removeFromTopLayer(Element*);
  const HeapVector<Member<Element>>& topLayerElements() const {
    return m_topLayerElements;
  }
  HTMLDialogElement* activeModalDialog() const;

  // A non-null m_templateDocumentHost implies that |this| was created by
  // ensureTemplateDocument().
  bool isTemplateDocument() const { return !!m_templateDocumentHost; }
  Document& ensureTemplateDocument();
  Document* templateDocumentHost() { return m_templateDocumentHost; }

  mojom::EngagementLevel getEngagementLevel() const {
    return m_engagementLevel;
  }
  void setEngagementLevel(mojom::EngagementLevel level) {
    m_engagementLevel = level;
  }

  // TODO(thestig): Rename these and related functions, since we can call them
  // for controls outside of forms as well.
  void didAssociateFormControl(Element*);

  void addConsoleMessage(ConsoleMessage*) final;

  LocalDOMWindow* executingWindow() const final;
  LocalFrame* executingFrame();

  DocumentLifecycle& lifecycle() { return m_lifecycle; }
  bool isActive() const { return m_lifecycle.isActive(); }
  bool isDetached() const {
    return m_lifecycle.state() >= DocumentLifecycle::Stopping;
  }
  bool isStopped() const {
    return m_lifecycle.state() == DocumentLifecycle::Stopped;
  }

  enum HttpRefreshType { HttpRefreshFromHeader, HttpRefreshFromMetaTag };
  void maybeHandleHttpRefresh(const String&, HttpRefreshType);

  void updateSecurityOrigin(PassRefPtr<SecurityOrigin>);

  void setHasViewportUnits() { m_hasViewportUnits = true; }
  bool hasViewportUnits() const { return m_hasViewportUnits; }
  void setResizedForViewportUnits();
  void clearResizedForViewportUnits();

  void updateActiveStyle();

  DECLARE_VIRTUAL_TRACE();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  AtomicString convertLocalName(const AtomicString&);

  void platformColorsChanged();

  DOMTimerCoordinator* timers() final;

  HostsUsingFeatures::Value& HostsUsingFeaturesValue() {
    return m_hostsUsingFeaturesValue;
  }

  NthIndexCache* nthIndexCache() const { return m_nthIndexCache; }

  bool isSecureContext(
      String& errorMessage,
      const SecureContextCheck = StandardSecureContextCheck) const override;
  bool isSecureContext(
      const SecureContextCheck = StandardSecureContextCheck) const override;

  ClientHintsPreferences& clientHintsPreferences() {
    return m_clientHintsPreferences;
  }

  CanvasFontCache* canvasFontCache();

  // Used by unit tests so that all parsing will be main thread for
  // controlling parsing and chunking precisely.
  static void setThreadedParsingEnabledForTesting(bool);
  static bool threadedParsingEnabledForTesting();

  void incrementNodeCount() { m_nodeCount++; }
  void decrementNodeCount() {
    DCHECK_GT(m_nodeCount, 0);
    m_nodeCount--;
  }
  int nodeCount() const { return m_nodeCount; }

  SnapCoordinator* snapCoordinator();

  void enforceInsecureRequestPolicy(WebInsecureRequestPolicy);

  bool mayContainV0Shadow() const { return m_mayContainV0Shadow; }

  ShadowCascadeOrder shadowCascadeOrder() const { return m_shadowCascadeOrder; }
  void setShadowCascadeOrder(ShadowCascadeOrder);

  bool containsV1ShadowTree() const {
    return m_shadowCascadeOrder == ShadowCascadeOrder::ShadowCascadeV1;
  }

  Element* rootScroller() const;
  void setRootScroller(Element*, ExceptionState&);
  RootScrollerController& rootScrollerController() const {
    DCHECK(m_rootScrollerController);
    return *m_rootScrollerController;
  }

  bool isInMainFrame() const;

  void recordDeferredLoadReason(WouldLoadReason);
  WouldLoadReason deferredLoadReason() { return m_wouldLoadReason; }

  const PropertyRegistry* propertyRegistry() const;
  PropertyRegistry* propertyRegistry();

  // Document maintains a counter of visible non-secure password
  // fields in the page. Used to notify the embedder when all visible
  // non-secure passwords fields are no longer visible.
  void incrementPasswordCount();
  void decrementPasswordCount();

 protected:
  Document(const DocumentInit&, DocumentClassFlags = DefaultDocumentClass);

  void didUpdateSecurityOrigin() final;

  void clearXMLVersion() { m_xmlVersion = String(); }

  virtual Document* cloneDocumentWithoutChildren();

  bool importContainerNodeChildren(ContainerNode* oldContainerNode,
                                   ContainerNode* newContainerNode,
                                   ExceptionState&);
  void lockCompatibilityMode() { m_compatibilityModeLocked = true; }
  ParserSynchronizationPolicy getParserSynchronizationPolicy() const {
    return m_parserSyncPolicy;
  }

 private:
  friend class IgnoreDestructiveWriteCountIncrementer;
  friend class ThrowOnDynamicMarkupInsertionCountIncrementer;
  friend class NthIndexCache;
  class NetworkStateObserver;

  bool isDocumentFragment() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool isDocumentNode() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool isElementNode() const =
      delete;  // This will catch anyone doing an unnecessary check.

  ScriptedAnimationController& ensureScriptedAnimationController();
  ScriptedIdleTaskController& ensureScriptedIdleTaskController();
  void initSecurityContext(const DocumentInit&);
  SecurityContext& securityContext() final { return *this; }
  EventQueue* getEventQueue() const final;

  bool hasPendingVisualUpdate() const {
    return m_lifecycle.state() == DocumentLifecycle::VisualUpdatePending;
  }

  bool shouldScheduleLayoutTreeUpdate() const;
  void scheduleLayoutTreeUpdate();

  bool needsFullLayoutTreeUpdate() const;

  void inheritHtmlAndBodyElementStyles(StyleRecalcChange);

  void updateUseShadowTreesIfNeeded();
  void evaluateMediaQueryListIfNeeded();

  void updateStyleInvalidationIfNeeded();
  void updateStyle();
  void notifyLayoutTreeOfSubtreeChanges();

  void detachParser();

  void beginLifecycleUpdatesIfRenderingReady();

  bool isDocument() const final { return true; }

  void childrenChanged(const ChildrenChange&) override;

  String nodeName() const final;
  NodeType getNodeType() const final;
  bool childTypeAllowed(NodeType) const final;
  Node* cloneNode(bool deep, ExceptionState&) final;
  void cloneDataFromDocument(const Document&);
  bool isSecureContextImpl(
      const SecureContextCheck priviligeContextCheck) const;

  ShadowCascadeOrder m_shadowCascadeOrder = ShadowCascadeNone;

  // Same as url(), but needed for ExecutionContext to implement it without a
  // performance loss for direct calls.
  const KURL& virtualURL() const final;
  // Same as completeURL() for the same reason as above.
  KURL virtualCompleteURL(const String&) const final;

  void updateTitle(const String&);
  void updateFocusAppearanceTimerFired(TimerBase*);
  void updateBaseURL();

  void executeScriptsWaitingForResources();

  void loadEventDelayTimerFired(TimerBase*);
  void pluginLoadingTimerFired(TimerBase*);

  void addListenerType(ListenerType listenerType) {
    m_listenerTypes |= listenerType;
  }
  void addMutationEventListenerTypeIfEnabled(ListenerType);

  void didAssociateFormControlsTimerFired(TimerBase*);

  void clearFocusedElementSoon();
  void clearFocusedElementTimerFired(TimerBase*);

  bool haveScriptBlockingStylesheetsLoaded() const;
  bool haveRenderBlockingStylesheetsLoaded() const;
  void styleResolverMayHaveChanged();

  void setHoverNode(Node*);

  using EventFactorySet = HashSet<std::unique_ptr<EventFactoryBase>>;
  static EventFactorySet& eventFactories();

  void setNthIndexCache(NthIndexCache* nthIndexCache) {
    DCHECK(!m_nthIndexCache || !nthIndexCache);
    m_nthIndexCache = nthIndexCache;
  }

  const OriginAccessEntry& accessEntryFromURL();

  void sendSensitiveInputVisibility();
  void sendSensitiveInputVisibilityInternal();

  void runExecutionContextTask(std::unique_ptr<ExecutionContextTask>,
                               bool instrumenting);

  DocumentLifecycle m_lifecycle;

  bool m_hasNodesWithPlaceholderStyle;
  bool m_evaluateMediaQueriesOnStyleRecalc;

  // If we do ignore the pending stylesheet count, then we need to add a boolean
  // to track that this happened so that we can do a full repaint when the
  // stylesheets do eventually load.
  PendingSheetLayout m_pendingSheetLayout;

  Member<LocalFrame> m_frame;
  Member<LocalDOMWindow> m_domWindow;
  TraceWrapperMember<HTMLImportsController> m_importsController;

  Member<ResourceFetcher> m_fetcher;
  Member<DocumentParser> m_parser;
  Member<ContextFeatures> m_contextFeatures;

  // This HashMap is used to stash information (ComputedStyle, nextTextSibling)
  // generated in the Style Resolution phase that is required in the
  // Layout Tree construction phase.
  HeapHashMap<Member<const Node>, StyleReattachData> m_styleReattachDataMap;

  bool m_wellFormed;

  // Document URLs.
  KURL m_url;  // Document.URL: The URL from which this document was retrieved.
  KURL m_baseURL;  // Node.baseURI: The URL to use when resolving relative URLs.
  KURL m_baseURLOverride;  // An alternative base URL that takes precedence over
                           // m_baseURL (but not m_baseElementURL).
  KURL m_baseElementURL;   // The URL set by the <base> element.
  KURL m_cookieURL;        // The URL to use for cookie access.
  std::unique_ptr<OriginAccessEntry> m_accessEntryFromURL;

  AtomicString m_baseTarget;

  // Mime-type of the document in case it was cloned or created by XHR.
  AtomicString m_mimeType;

  Member<DocumentType> m_docType;
  TraceWrapperMember<DOMImplementation> m_implementation;

  Member<CSSStyleSheet> m_elemSheet;

  PrintingState m_printing;
  bool m_paginatedForScreen;

  CompatibilityMode m_compatibilityMode;
  // This is cheaper than making setCompatibilityMode virtual.
  bool m_compatibilityModeLocked;

  TaskHandle m_executeScriptsWaitingForResourcesTaskHandle;

  bool m_hasAutofocused;
  TaskRunnerTimer<Document> m_clearFocusedElementTimer;
  Member<Element> m_autofocusElement;
  Member<Element> m_focusedElement;
  Member<Range> m_sequentialFocusNavigationStartingPoint;
  Member<Node> m_hoverNode;
  Member<Element> m_activeHoverElement;
  Member<Element> m_documentElement;
  UserActionElementSet m_userActionElements;
  Member<RootScrollerController> m_rootScrollerController;

  uint64_t m_domTreeVersion;
  static uint64_t s_globalTreeVersion;

  uint64_t m_styleVersion;

  HeapHashSet<WeakMember<NodeIterator>> m_nodeIterators;
  using AttachedRangeSet = HeapHashSet<WeakMember<Range>>;
  AttachedRangeSet m_ranges;

  unsigned short m_listenerTypes;

  MutationObserverOptions m_mutationObserverTypes;

  TraceWrapperMember<StyleEngine> m_styleEngine;
  TraceWrapperMember<StyleSheetList> m_styleSheetList;

  Member<FormController> m_formController;

  TextLinkColors m_textLinkColors;
  const Member<VisitedLinkState> m_visitedLinkState;

  bool m_visuallyOrdered;

  DocumentReadyState m_readyState;
  ParsingState m_parsingState;

  bool m_gotoAnchorNeededAfterStylesheetsLoad;
  bool m_isDNSPrefetchEnabled;
  bool m_haveExplicitlyDisabledDNSPrefetch;
  bool m_containsValidityStyleRules;
  bool m_containsPlugins;

  // http://www.whatwg.org/specs/web-apps/current-work/#ignore-destructive-writes-counter
  unsigned m_ignoreDestructiveWriteCount;
  // https://html.spec.whatwg.org/#throw-on-dynamic-markup-insertion-counter
  unsigned m_throwOnDynamicMarkupInsertionCount;

  String m_title;
  String m_rawTitle;
  Member<Element> m_titleElement;

  Member<AXObjectCache> m_axObjectCache;
  Member<DocumentMarkerController> m_markers;

  TaskRunnerTimer<Document> m_updateFocusAppearanceTimer;

  Member<Element> m_cssTarget;

  LoadEventProgress m_loadEventProgress;

  double m_startTime;

  Member<ScriptRunner> m_scriptRunner;

  HeapVector<Member<Element>> m_currentScriptStack;

  std::unique_ptr<TransformSource> m_transformSource;

  String m_xmlEncoding;
  String m_xmlVersion;
  unsigned m_xmlStandalone : 2;
  unsigned m_hasXMLDeclaration : 1;

  AtomicString m_contentLanguage;

  DocumentEncodingData m_encodingData;

  bool m_designMode;
  bool m_isRunningExecCommand;

  HeapHashSet<WeakMember<const LiveNodeListBase>> m_listsInvalidatedAtDocument;
  // Oilpan keeps track of all registered NodeLists.
  // TODO(Oilpan): improve - only need to know if a NodeList
  // is currently alive or not for the different types.
  HeapHashSet<WeakMember<const LiveNodeListBase>>
      m_nodeLists[numNodeListInvalidationTypes];

  Member<SVGDocumentExtensions> m_svgExtensions;

  Vector<AnnotatedRegionValue> m_annotatedRegions;
  bool m_hasAnnotatedRegions;
  bool m_annotatedRegionsDirty;

  std::unique_ptr<SelectorQueryCache> m_selectorQueryCache;

  // It is safe to keep a raw, untraced pointer to this stack-allocated
  // cache object: it is set upon the cache object being allocated on
  // the stack and cleared upon leaving its allocated scope. Hence it
  // is acceptable not to trace it -- should a conservative GC occur,
  // the cache object's references will be traced by a stack walk.
  GC_PLUGIN_IGNORE("461878")
  NthIndexCache* m_nthIndexCache = nullptr;

  DocumentClassFlags m_documentClasses;

  bool m_isViewSource;
  bool m_sawElementsInKnownNamespaces;
  bool m_isSrcdocDocument;
  bool m_isMobileDocument;

  LayoutView* m_layoutView;

  WeakMember<Document> m_contextDocument;

  // For early return in Fullscreen::fromIfExists()
  bool m_hasFullscreenSupplement;

  HeapVector<Member<Element>> m_topLayerElements;

  int m_loadEventDelayCount;
  TaskRunnerTimer<Document> m_loadEventDelayTimer;
  TaskRunnerTimer<Document> m_pluginLoadingTimer;

  ViewportDescription m_viewportDescription;
  ViewportDescription m_legacyViewportDescription;
  Length m_viewportDefaultMinWidth;

  ReferrerPolicy m_referrerPolicy;

  DocumentTiming m_documentTiming;
  Member<MediaQueryMatcher> m_mediaQueryMatcher;
  bool m_writeRecursionIsTooDeep;
  unsigned m_writeRecursionDepth;

  Member<ScriptedAnimationController> m_scriptedAnimationController;
  Member<ScriptedIdleTaskController> m_scriptedIdleTaskController;
  Member<TextAutosizer> m_textAutosizer;

  Member<V0CustomElementRegistrationContext> m_registrationContext;
  Member<V0CustomElementMicrotaskRunQueue> m_customElementMicrotaskRunQueue;

  void elementDataCacheClearTimerFired(TimerBase*);
  TaskRunnerTimer<Document> m_elementDataCacheClearTimer;

  Member<ElementDataCache> m_elementDataCache;

  using LocaleIdentifierToLocaleMap =
      HashMap<AtomicString, std::unique_ptr<Locale>>;
  LocaleIdentifierToLocaleMap m_localeCache;

  Member<DocumentTimeline> m_timeline;
  Member<CompositorPendingAnimations> m_compositorPendingAnimations;

  Member<Document> m_templateDocument;
  Member<Document> m_templateDocumentHost;

  TaskRunnerTimer<Document> m_didAssociateFormControlsTimer;

  HeapHashSet<Member<SVGUseElement>> m_useElementsNeedingUpdate;

  DOMTimerCoordinator m_timers;

  bool m_hasViewportUnits;

  ParserSynchronizationPolicy m_parserSyncPolicy;

  HostsUsingFeatures::Value m_hostsUsingFeaturesValue;

  ClientHintsPreferences m_clientHintsPreferences;

  Member<CanvasFontCache> m_canvasFontCache;

  Member<IntersectionObserverController> m_intersectionObserverController;
  Member<ResizeObserverController> m_resizeObserverController;

  int m_nodeCount;

  bool m_mayContainV0Shadow = false;

  Member<SnapCoordinator> m_snapCoordinator;

  WouldLoadReason m_wouldLoadReason;

  Member<PropertyRegistry> m_propertyRegistry;

  unsigned m_passwordCount;

  TaskHandle m_sensitiveInputVisibilityTask;

  mojom::EngagementLevel m_engagementLevel;

  Member<NetworkStateObserver> m_networkStateObserver;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<Document>;

inline bool Document::shouldOverrideLegacyDescription(
    ViewportDescription::Type origin) const {
  // The different (legacy) meta tags have different priorities based on the
  // type regardless of which order they appear in the DOM. The priority is
  // given by the ViewportDescription::Type enum.
  return origin >= m_legacyViewportDescription.type;
}

inline void Document::scheduleLayoutTreeUpdateIfNeeded() {
  // Inline early out to avoid the function calls below.
  if (hasPendingVisualUpdate())
    return;
  if (shouldScheduleLayoutTreeUpdate() && needsLayoutTreeUpdate())
    scheduleLayoutTreeUpdate();
}

DEFINE_TYPE_CASTS(Document,
                  ExecutionContext,
                  context,
                  context->isDocument(),
                  context.isDocument());
DEFINE_NODE_TYPE_CASTS(Document, isDocumentNode());

#define DEFINE_DOCUMENT_TYPE_CASTS(thisType)                                \
  DEFINE_TYPE_CASTS(thisType, Document, document, document->is##thisType(), \
                    document.is##thisType())

// This is needed to avoid ambiguous overloads with the Node and TreeScope
// versions.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(Document)

// Put these methods here, because they require the Document definition, but we
// really want to inline them.

inline bool Node::isDocumentNode() const {
  return this == document();
}

Node* eventTargetNodeForDocument(Document*);

DEFINE_TYPE_CASTS(TreeScope, Document, document, true, true);

}  // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
CORE_EXPORT void showLiveDocumentInstances();
#endif

#endif  // Document_h
