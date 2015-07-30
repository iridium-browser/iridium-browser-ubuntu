/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebLocalFrameImpl_h
#define WebLocalFrameImpl_h

#include "core/frame/LocalFrame.h"
#include "platform/geometry/FloatRect.h"
#include "public/platform/WebFileSystemType.h"
#include "public/web/WebLocalFrame.h"
#include "web/FrameLoaderClientImpl.h"
#include "web/UserMediaClientImpl.h"
#include "wtf/Compiler.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ChromePrintContext;
class GeolocationClientProxy;
class InspectorOverlay;
class IntSize;
class KURL;
class Range;
class SharedWorkerRepositoryClientImpl;
class TextFinder;
class WebAutofillClient;
class WebDataSourceImpl;
class WebDevToolsAgentImpl;
class WebDevToolsFrontendImpl;
class WebFrameClient;
class WebFrameWidgetImpl;
class WebNode;
class WebPerformance;
class WebPlugin;
class WebPluginContainerImpl;
class WebScriptExecutionCallback;
class WebSuspendableTask;
class WebView;
class WebViewImpl;
struct FrameLoadRequest;
struct WebPrintParams;

template <typename T> class WebVector;

// Implementation of WebFrame, note that this is a reference counted object.
class WebLocalFrameImpl final : public RefCountedWillBeGarbageCollectedFinalized<WebLocalFrameImpl>, public WebLocalFrame {
public:
    // WebFrame methods:
    virtual bool isWebLocalFrame() const override;
    virtual WebLocalFrame* toWebLocalFrame() override;
    virtual bool isWebRemoteFrame() const override;
    virtual WebRemoteFrame* toWebRemoteFrame() override;
    virtual void close() override;
    virtual WebString uniqueName() const override;
    virtual WebString assignedName() const override;
    virtual void setName(const WebString&) override;
    virtual WebVector<WebIconURL> iconURLs(int iconTypesMask) const override;
    virtual void setRemoteWebLayer(WebLayer*) override;
    virtual void setContentSettingsClient(WebContentSettingsClient*) override;
    virtual void setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient*) override;
    virtual WebSize scrollOffset() const override;
    virtual void setScrollOffset(const WebSize&) override;
    virtual WebSize minimumScrollOffset() const override;
    virtual WebSize maximumScrollOffset() const override;
    virtual WebSize contentsSize() const override;
    virtual bool hasVisibleContent() const override;
    virtual WebRect visibleContentRect() const override;
    virtual bool hasHorizontalScrollbar() const override;
    virtual bool hasVerticalScrollbar() const override;
    virtual WebView* view() const override;
    virtual void setOpener(WebFrame*) override;
    virtual WebDocument document() const override;
    virtual WebPerformance performance() const override;
    virtual bool dispatchBeforeUnloadEvent() override;
    virtual void dispatchUnloadEvent() override;
    virtual NPObject* windowObject() const override;
    virtual void bindToWindowObject(const WebString& name, NPObject*) override;
    virtual void bindToWindowObject(const WebString& name, NPObject*, void*) override;
    virtual void executeScript(const WebScriptSource&) override;
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sources, unsigned numSources,
        int extensionGroup) override;
    virtual void setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin&) override;
    virtual void setIsolatedWorldContentSecurityPolicy(int worldID, const WebString&) override;
    virtual void setIsolatedWorldHumanReadableName(int worldID, const WebString&) override;
    virtual void addMessageToConsole(const WebConsoleMessage&) override;
    virtual void collectGarbage() override;
    virtual bool checkIfRunInsecureContent(const WebURL&) const override;
    virtual v8::Local<v8::Value> executeScriptAndReturnValue(
        const WebScriptSource&) override;
    virtual void requestExecuteScriptAndReturnValue(
        const WebScriptSource&, bool userGesture, WebScriptExecutionCallback*) override;
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sourcesIn, unsigned numSources,
        int extensionGroup, WebVector<v8::Local<v8::Value>>* results) override;
    virtual void requestExecuteScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sourceIn, unsigned numSources,
        int extensionGroup, bool userGesture, WebScriptExecutionCallback*) override;
    virtual v8::Local<v8::Value> callFunctionEvenIfScriptDisabled(
        v8::Local<v8::Function>,
        v8::Local<v8::Value>,
        int argc,
        v8::Local<v8::Value> argv[]) override;
    virtual v8::Local<v8::Context> mainWorldScriptContext() const override;
    virtual void reload(bool ignoreCache) override;
    virtual void reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache) override;
    virtual void reloadImage(const WebNode&) override;
    virtual void loadRequest(const WebURLRequest&) override;
    virtual void loadHistoryItem(const WebHistoryItem&, WebHistoryLoadType, WebURLRequest::CachePolicy) override;
    virtual void loadData(
        const WebData&, const WebString& mimeType, const WebString& textEncoding,
        const WebURL& baseURL, const WebURL& unreachableURL, bool replace) override;
    virtual void loadHTMLString(
        const WebData& html, const WebURL& baseURL, const WebURL& unreachableURL,
        bool replace) override;
    virtual void stopLoading() override;
    virtual WebDataSource* provisionalDataSource() const override;
    virtual WebDataSource* dataSource() const override;
    virtual void enableViewSourceMode(bool enable) override;
    virtual bool isViewSourceModeEnabled() const override;
    virtual void setReferrerForRequest(WebURLRequest&, const WebURL& referrer) override;
    virtual void dispatchWillSendRequest(WebURLRequest&) override;
    virtual WebURLLoader* createAssociatedURLLoader(const WebURLLoaderOptions&) override;
    virtual unsigned unloadListenerCount() const override;
    virtual void replaceSelection(const WebString&) override;
    virtual void insertText(const WebString&) override;
    virtual void setMarkedText(const WebString&, unsigned location, unsigned length) override;
    virtual void unmarkText() override;
    virtual bool hasMarkedText() const override;
    virtual WebRange markedRange() const override;
    virtual bool firstRectForCharacterRange(unsigned location, unsigned length, WebRect&) const override;
    virtual size_t characterIndexForPoint(const WebPoint&) const override;
    virtual bool executeCommand(const WebString&, const WebNode& = WebNode()) override;
    virtual bool executeCommand(const WebString&, const WebString& value, const WebNode& = WebNode()) override;
    virtual bool isCommandEnabled(const WebString&) const override;
    virtual void enableContinuousSpellChecking(bool) override;
    virtual bool isContinuousSpellCheckingEnabled() const override;
    virtual void requestTextChecking(const WebElement&) override;
    virtual void replaceMisspelledRange(const WebString&) override;
    virtual void removeSpellingMarkers() override;
    virtual bool hasSelection() const override;
    virtual WebRange selectionRange() const override;
    virtual WebString selectionAsText() const override;
    virtual WebString selectionAsMarkup() const override;
    virtual bool selectWordAroundCaret() override;
    virtual void selectRange(const WebPoint& base, const WebPoint& extent) override;
    virtual void selectRange(const WebRange&) override;
    virtual void moveRangeSelectionExtent(const WebPoint&) override;
    virtual void moveRangeSelection(const WebPoint& base, const WebPoint& extent, WebFrame::TextGranularity = CharacterGranularity) override;
    virtual void moveCaretSelection(const WebPoint&) override;
    virtual bool setEditableSelectionOffsets(int start, int end) override;
    virtual bool setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines) override;
    virtual void extendSelectionAndDelete(int before, int after) override;
    virtual void setCaretVisible(bool) override;
    virtual int printBegin(const WebPrintParams&, const WebNode& constrainToNode) override;
    virtual float printPage(int pageToPrint, WebCanvas*) override;
    virtual float getPrintPageShrink(int page) override;
    virtual void printEnd() override;
    virtual bool isPrintScalingDisabledForPlugin(const WebNode&) override;
    virtual bool getPrintPresetOptionsForPlugin(const WebNode&, WebPrintPresetOptions*) override;
    virtual bool hasCustomPageSizeStyle(int pageIndex) override;
    virtual bool isPageBoxVisible(int pageIndex) override;
    virtual void pageSizeAndMarginsInPixels(
        int pageIndex,
        WebSize& pageSize,
        int& marginTop,
        int& marginRight,
        int& marginBottom,
        int& marginLeft) override;
    virtual WebString pageProperty(const WebString& propertyName, int pageIndex) override;
    virtual void printPagesWithBoundaries(WebCanvas*, const WebSize&) override;
    virtual bool find(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool wrapWithinFrame, WebRect* selectionRect) override;
    virtual void stopFinding(bool clearSelection) override;
    virtual void scopeStringMatches(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool reset) override;
    virtual void cancelPendingScopingEffort() override;
    virtual void increaseMatchCount(int count, int identifier) override;
    virtual void resetMatchCount() override;
    virtual int findMatchMarkersVersion() const override;
    virtual WebFloatRect activeFindMatchRect() override;
    virtual void findMatchRects(WebVector<WebFloatRect>&) override;
    virtual int selectNearestFindMatch(const WebFloatPoint&, WebRect* selectionRect) override;
    virtual void setTickmarks(const WebVector<WebRect>&) override;

    virtual void dispatchMessageEventWithOriginCheck(
        const WebSecurityOrigin& intendedTargetOrigin,
        const WebDOMEvent&) override;

    virtual WebString contentAsText(size_t maxChars) const override;
    virtual WebString contentAsMarkup() const override;
    virtual WebString layoutTreeAsText(LayoutAsTextControls toShow = LayoutAsTextNormal) const override;

    virtual WebString markerTextForListItem(const WebElement&) const override;
    virtual WebRect selectionBoundsRect() const override;

    virtual bool selectionStartHasSpellingMarkerFor(int from, int length) const override;
    virtual WebString layerTreeAsText(bool showDebugInfo = false) const override;

    // WebLocalFrame methods:
    virtual void initializeToReplaceRemoteFrame(WebRemoteFrame*, const WebString& name, WebSandboxFlags) override;
    virtual void setAutofillClient(WebAutofillClient*) override;
    virtual WebAutofillClient* autofillClient() override;
    virtual void setDevToolsAgentClient(WebDevToolsAgentClient*) override;
    virtual WebDevToolsAgent* devToolsAgent() override;
    virtual void sendPings(const WebNode& linkNode, const WebURL& destinationURL) override;
    virtual bool isLoading() const override;
    virtual bool isResourceLoadInProgress() const override;
    virtual void setCommittedFirstRealLoad() override;
    virtual void addStyleSheetByURL(const WebString& url) override;
    virtual void navigateToSandboxedMarkup(const WebData& markup) override;
    virtual void sendOrientationChangeEvent() override;
    virtual void willShowInstallBannerPrompt(int requestId, const WebVector<WebString>& platforms, WebAppBannerPromptReply*) override;
    virtual void willShowInstallBannerPrompt(const WebVector<WebString>& platforms, WebAppBannerPromptReply*) override;
    void requestRunTask(WebSuspendableTask*) const override;

    void willBeDetached();
    void willDetachParent();

    static WebLocalFrameImpl* create(WebFrameClient*);
    virtual ~WebLocalFrameImpl();

    PassRefPtrWillBeRawPtr<LocalFrame> initializeCoreFrame(FrameHost*, FrameOwner*, const AtomicString& name, const AtomicString& fallbackName);

    PassRefPtrWillBeRawPtr<LocalFrame> createChildFrame(const FrameLoadRequest&, const AtomicString& name, HTMLFrameOwnerElement*);

    void didChangeContentsSize(const IntSize&);

    void createFrameView();

    static WebLocalFrameImpl* fromFrame(LocalFrame*);
    static WebLocalFrameImpl* fromFrame(LocalFrame&);
    static WebLocalFrameImpl* fromFrameOwnerElement(Element*);

    // If the frame hosts a PluginDocument, this method returns the WebPluginContainerImpl
    // that hosts the plugin.
    static WebPluginContainerImpl* pluginContainerFromFrame(LocalFrame*);

    // If the frame hosts a PluginDocument, this method returns the WebPluginContainerImpl
    // that hosts the plugin. If the provided node is a plugin, then it runs its
    // WebPluginContainerImpl.
    static WebPluginContainerImpl* pluginContainerFromNode(LocalFrame*, const WebNode&);

    WebViewImpl* viewImpl() const;

    FrameView* frameView() const { return frame() ? frame()->view() : 0; }

    InspectorOverlay* inspectorOverlay();
    WebDevToolsAgentImpl* devToolsAgentImpl() const { return m_devToolsAgent.get(); }

    // Getters for the impls corresponding to Get(Provisional)DataSource. They
    // may return 0 if there is no corresponding data source.
    WebDataSourceImpl* dataSourceImpl() const;
    WebDataSourceImpl* provisionalDataSourceImpl() const;

    // Returns which frame has an active match. This function should only be
    // called on the main frame, as it is the only frame keeping track. Returned
    // value can be 0 if no frame has an active match.
    WebLocalFrameImpl* activeMatchFrame() const;

    // Returns the active match in the current frame. Could be a null range if
    // the local frame has no active match.
    Range* activeMatch() const;

    // When a Find operation ends, we want to set the selection to what was active
    // and set focus to the first focusable node we find (starting with the first
    // node in the matched range and going up the inheritance chain). If we find
    // nothing to focus we focus the first focusable node in the range. This
    // allows us to set focus to a link (when we find text inside a link), which
    // allows us to navigate by pressing Enter after closing the Find box.
    void setFindEndstateFocusAndSelection();

    void didFail(const ResourceError&, bool wasProvisional, HistoryCommitType);

    // Sets whether the WebLocalFrameImpl allows its document to be scrolled.
    // If the parameter is true, allow the document to be scrolled.
    // Otherwise, disallow scrolling.
    virtual void setCanHaveScrollbars(bool) override;

    LocalFrame* frame() const { return m_frame.get(); }
    WebFrameClient* client() const { return m_client; }
    void setClient(WebFrameClient* client) { m_client = client; }

    WebContentSettingsClient* contentSettingsClient() { return m_contentSettingsClient; }
    SharedWorkerRepositoryClientImpl* sharedWorkerRepositoryClient() const { return m_sharedWorkerRepositoryClient.get(); }

    void setInputEventsTransformForEmulation(const IntSize&, float);

    static void selectWordAroundPosition(LocalFrame*, VisiblePosition);

    // Returns the text finder object if it already exists.
    // Otherwise creates it and then returns.
    TextFinder& ensureTextFinder();

    // Returns a hit-tested VisiblePosition for the given point
    VisiblePosition visiblePositionForViewportPoint(const WebPoint&);

    void setFrameWidget(WebFrameWidgetImpl*);
    WebFrameWidgetImpl* frameWidget() const;

    // DevTools front-end bindings.
    void setDevToolsFrontend(WebDevToolsFrontendImpl* frontend) { m_webDevToolsFrontend = frontend; }
    WebDevToolsFrontendImpl* devToolsFrontend() { return m_webDevToolsFrontend; }

#if ENABLE(OILPAN)
    DECLARE_TRACE();
#endif

private:
    friend class FrameLoaderClientImpl;

    explicit WebLocalFrameImpl(WebFrameClient*);

    // Sets the local core frame and registers destruction observers.
    void setCoreFrame(PassRefPtrWillBeRawPtr<LocalFrame>);

    void loadJavaScriptURL(const KURL&);

    WebPlugin* focusedPluginIfInputMethodSupported();

    FrameLoaderClientImpl m_frameLoaderClientImpl;

    // The embedder retains a reference to the WebCore LocalFrame while it is active in the DOM. This
    // reference is released when the frame is removed from the DOM or the entire page is closed.
    // FIXME: These will need to change to WebFrame when we introduce WebFrameProxy.
    RefPtrWillBeMember<LocalFrame> m_frame;

    OwnPtrWillBeMember<InspectorOverlay> m_inspectorOverlay;
    OwnPtrWillBeMember<WebDevToolsAgentImpl> m_devToolsAgent;

    // This is set if the frame is the root of a local frame tree, and requires a widget for layout.
    WebFrameWidgetImpl* m_frameWidget;

    WebFrameClient* m_client;
    WebAutofillClient* m_autofillClient;
    WebContentSettingsClient* m_contentSettingsClient;
    OwnPtr<SharedWorkerRepositoryClientImpl> m_sharedWorkerRepositoryClient;

    // Will be initialized after first call to find() or scopeStringMatches().
    OwnPtrWillBeMember<TextFinder> m_textFinder;

    // Valid between calls to BeginPrint() and EndPrint(). Containts the print
    // information. Is used by PrintPage().
    OwnPtrWillBeMember<ChromePrintContext> m_printContext;

    // Stores the additional input events offset and scale when device metrics emulation is enabled.
    IntSize m_inputEventsOffsetForEmulation;
    float m_inputEventsScaleFactorForEmulation;

    UserMediaClientImpl m_userMediaClientImpl;

    OwnPtrWillBeMember<GeolocationClientProxy> m_geolocationClientProxy;

    WebDevToolsFrontendImpl* m_webDevToolsFrontend;

#if ENABLE(OILPAN)
    // Oilpan: to provide the guarantee of having the frame live until
    // close() is called, an instance keep a self-persistent. It is
    // cleared upon calling close(). This avoids having to assume that
    // an embedder's WebFrame references are all discovered via thread
    // state (stack, registers) should an Oilpan GC strike while we're
    // in the process of detaching.
    GC_PLUGIN_IGNORE("340522")
    Persistent<WebLocalFrameImpl> m_selfKeepAlive;
#endif
};

DEFINE_TYPE_CASTS(WebLocalFrameImpl, WebFrame, frame, frame->isWebLocalFrame(), frame.isWebLocalFrame());

} // namespace blink

#endif
