/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
 * Copyright (C) 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EmptyClients_h
#define EmptyClients_h

#include "core/CoreExport.h"
#include "core/editing/UndoStep.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuClient.h"
#include "core/page/DragClient.h"
#include "core/page/EditorClient.h"
#include "core/page/Page.h"
#include "core/page/SpellCheckerClient.h"
#include "platform/DragImage.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceError.h"
#include "platform/text/TextCheckerClient.h"
#include "public/platform/WebFocusType.h"
#include "public/platform/WebScreenInfo.h"
#include "wtf/Forward.h"
#include <v8.h>

/*
 This file holds empty Client stubs for use by WebCore.
 Viewless element needs to create a dummy Page->LocalFrame->FrameView tree for use in parsing or executing JavaScript.
 This tree depends heavily on Clients (usually provided by WebKit classes).

 This file was first created for SVGImage as it had no way to access the current Page (nor should it,
 since Images are not tied to a page).
 See http://bugs.webkit.org/show_bug.cgi?id=5971 for the original discussion about this file.

 Ideally, whenever you change a Client class, you should add a stub here.
 Brittle, yes.  Unfortunate, yes.  Hopefully temporary.
*/

namespace blink {

class CORE_EXPORT EmptyChromeClient : public ChromeClient {
    WTF_MAKE_FAST_ALLOCATED(EmptyChromeClient);
public:
    virtual ~EmptyChromeClient() { }
    virtual void chromeDestroyed() override { }

    virtual void* webView() const override { return 0; }
    virtual void setWindowRect(const IntRect&) override { }
    virtual IntRect windowRect() override { return IntRect(); }

    virtual IntRect pageRect() override { return IntRect(); }

    virtual void focus() override { }

    virtual bool canTakeFocus(WebFocusType) override { return false; }
    virtual void takeFocus(WebFocusType) override { }

    virtual void focusedNodeChanged(Node*, Node*) override { }
    virtual void focusedFrameChanged(LocalFrame*) override { }
    virtual Page* createWindow(LocalFrame*, const FrameLoadRequest&, const WindowFeatures&, NavigationPolicy, ShouldSendReferrer) override { return 0; }
    virtual void show(NavigationPolicy) override { }

    virtual void setToolbarsVisible(bool) override { }
    virtual bool toolbarsVisible() override { return false; }

    virtual void setStatusbarVisible(bool) override { }
    virtual bool statusbarVisible() override { return false; }

    virtual void setScrollbarsVisible(bool) override { }
    virtual bool scrollbarsVisible() override { return false; }

    virtual void setMenubarVisible(bool) override { }
    virtual bool menubarVisible() override { return false; }

    virtual void setResizable(bool) override { }

    virtual bool shouldReportDetailedMessageForSource(LocalFrame&, const String&) override { return false; }
    virtual void addMessageToConsole(LocalFrame*, MessageSource, MessageLevel, const String&, unsigned, const String&, const String&) override { }

    virtual bool canRunBeforeUnloadConfirmPanel() override { return false; }
    virtual bool runBeforeUnloadConfirmPanel(const String&, LocalFrame*) override { return true; }

    virtual void closeWindowSoon() override { }

    virtual void runJavaScriptAlert(LocalFrame*, const String&) override { }
    virtual bool runJavaScriptConfirm(LocalFrame*, const String&) override { return false; }
    virtual bool runJavaScriptPrompt(LocalFrame*, const String&, const String&, String&) override { return false; }

    virtual bool hasOpenedPopup() const override { return false; }
    virtual PassRefPtrWillBeRawPtr<PopupMenu> createPopupMenu(LocalFrame&, PopupMenuClient*) override;
    virtual DOMWindow* pagePopupWindowForTesting() const override { return nullptr; }

    virtual void setStatusbarText(const String&) override { }

    virtual bool tabsToLinks() override { return false; }

    virtual IntRect windowResizerRect() const override { return IntRect(); }

    virtual void invalidateRect(const IntRect&) override { }
    virtual void scheduleAnimation() override { }

    virtual IntRect viewportToScreen(const IntRect& r) const override { return r; }
    virtual WebScreenInfo screenInfo() const override { return WebScreenInfo(); }
    virtual void contentsSizeChanged(LocalFrame*, const IntSize&) const override { }

    virtual void mouseDidMoveOverElement(const HitTestResult&) override { }

    virtual void setToolTip(const String&, TextDirection) override { }

    virtual void print(LocalFrame*) override { }

    virtual void enumerateChosenDirectory(FileChooser*) override { }

    virtual PassOwnPtrWillBeRawPtr<ColorChooser> createColorChooser(LocalFrame*, ColorChooserClient*, const Color&) override;
    virtual PassRefPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) override;
    virtual void openTextDataListChooser(HTMLInputElement&) override;

    virtual void runOpenPanel(LocalFrame*, PassRefPtr<FileChooser>) override;

    virtual void setCursor(const Cursor&) override { }

    virtual void attachRootGraphicsLayer(GraphicsLayer*, LocalFrame* localRoot) override { }

    virtual void needTouchEvents(bool) override { }
    virtual void setTouchAction(TouchAction touchAction) override { };

    virtual void didAssociateFormControls(const WillBeHeapVector<RefPtrWillBeMember<Element>>&, LocalFrame* frame) override { }

    virtual void annotatedRegionsChanged() override { }
    virtual String acceptLanguages() override;
};

class CORE_EXPORT EmptyFrameLoaderClient : public FrameLoaderClient {
    WTF_MAKE_NONCOPYABLE(EmptyFrameLoaderClient); WTF_MAKE_FAST_ALLOCATED(EmptyFrameLoaderClient);
public:
    EmptyFrameLoaderClient() { }
    virtual ~EmptyFrameLoaderClient() {  }

    virtual bool hasWebView() const override { return true; } // mainly for assertions

    virtual Frame* opener() const override { return 0; }
    virtual void setOpener(Frame*) override { }

    virtual Frame* parent() const override { return 0; }
    virtual Frame* top() const override { return 0; }
    virtual Frame* previousSibling() const override { return 0; }
    virtual Frame* nextSibling() const override { return 0; }
    virtual Frame* firstChild() const override { return 0; }
    virtual Frame* lastChild() const override { return 0; }
    virtual void willBeDetached() override { }
    virtual void detached() override { }

    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&) override { }
    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse&) override { }
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long) override { }
    virtual void dispatchDidLoadResourceFromMemoryCache(const ResourceRequest&, const ResourceResponse&) override { }

    virtual void dispatchDidHandleOnloadEvents() override { }
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() override { }
    virtual void dispatchWillClose() override { }
    virtual void dispatchDidStartProvisionalLoad(bool isTransitionNavigation, double triggeringEventTime) override { }
    virtual void dispatchDidReceiveTitle(const String&) override { }
    virtual void dispatchDidChangeIcons(IconType) override { }
    virtual void dispatchDidCommitLoad(HistoryItem*, HistoryCommitType) override { }
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&, HistoryCommitType) override { }
    virtual void dispatchDidFailLoad(const ResourceError&, HistoryCommitType) override { }
    virtual void dispatchDidFinishDocumentLoad() override { }
    virtual void dispatchDidFinishLoad() override { }
    virtual void dispatchDidFirstVisuallyNonEmptyLayout() override { }
    virtual void dispatchDidChangeThemeColor() override { };

    virtual NavigationPolicy decidePolicyForNavigation(const ResourceRequest&, DocumentLoader*, NavigationPolicy, bool isTransitionNavigation) override;

    virtual void dispatchWillSendSubmitEvent(HTMLFormElement*) override;
    virtual void dispatchWillSubmitForm(HTMLFormElement*) override;

    virtual void didStartLoading(LoadStartType) override { }
    virtual void progressEstimateChanged(double) override { }
    virtual void didStopLoading() override { }

    virtual void loadURLExternally(const ResourceRequest&, NavigationPolicy, const String& = String()) override { }

    virtual PassRefPtr<DocumentLoader> createDocumentLoader(LocalFrame*, const ResourceRequest&, const SubstituteData&) override;

    virtual String userAgent(const KURL&) override { return ""; }

    virtual String doNotTrackValue() override { return String(); }

    virtual void transitionToCommittedForNewPage() override { }

    virtual bool navigateBackForward(int offset) const override { return false; }
    virtual void didDisplayInsecureContent() override { }
    virtual void didRunInsecureContent(SecurityOrigin*, const KURL&) override { }
    virtual void didDetectXSS(const KURL&, bool) override { }
    virtual void didDispatchPingLoader(const KURL&) override { }
    virtual void selectorMatchChanged(const Vector<String>&, const Vector<String>&) override { }
    virtual PassRefPtrWillBeRawPtr<LocalFrame> createFrame(const FrameLoadRequest&, const AtomicString&, HTMLFrameOwnerElement*) override;
    virtual PassOwnPtrWillBeRawPtr<PluginPlaceholder> createPluginPlaceholder(Document&, const KURL&, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType, bool loadManually) override;
    virtual PassRefPtrWillBeRawPtr<Widget> createPlugin(HTMLPlugInElement*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool, DetachedPluginPolicy) override;
    virtual bool canCreatePluginWithoutRenderer(const String& mimeType) const override { return false; }
    virtual PassRefPtrWillBeRawPtr<Widget> createJavaAppletWidget(HTMLAppletElement*, const KURL&, const Vector<String>&, const Vector<String>&) override;

    virtual ObjectContentType objectContentType(const KURL&, const String&, bool) override { return ObjectContentType(); }

    virtual void didCreateNewDocument() override { }
    virtual void dispatchDidClearWindowObjectInMainWorld() override { }
    virtual void documentElementAvailable() override { }

    virtual void didCreateScriptContext(v8::Local<v8::Context>, int extensionGroup, int worldId) override { }
    virtual void willReleaseScriptContext(v8::Local<v8::Context>, int worldId) override { }
    virtual bool allowScriptExtension(const String& extensionName, int extensionGroup, int worldId) override { return false; }

    virtual WebCookieJar* cookieJar() const override { return 0; }

    virtual void didRequestAutocomplete(HTMLFormElement*) override;

    virtual PassOwnPtr<WebServiceWorkerProvider> createServiceWorkerProvider() override;
    virtual bool isControlledByServiceWorker(DocumentLoader&) override { return false; }
    virtual int64_t serviceWorkerID(DocumentLoader&) override { return -1; }
    virtual PassOwnPtr<WebApplicationCacheHost> createApplicationCacheHost(WebApplicationCacheHostClient*) override;
};

class CORE_EXPORT EmptyTextCheckerClient : public TextCheckerClient {
public:
    ~EmptyTextCheckerClient() { }

    virtual bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const override { return true; }
    virtual void checkSpellingOfString(const String&, int*, int*) override { }
    virtual String getAutoCorrectSuggestionForMisspelledWord(const String&) override { return String(); }
    virtual void checkGrammarOfString(const String&, Vector<GrammarDetail>&, int*, int*) override { }
    virtual void requestCheckingOfString(PassRefPtrWillBeRawPtr<TextCheckingRequest>) override;
};

class EmptySpellCheckerClient : public SpellCheckerClient {
    WTF_MAKE_NONCOPYABLE(EmptySpellCheckerClient); WTF_MAKE_FAST_ALLOCATED(EmptySpellCheckerClient);
public:
    EmptySpellCheckerClient() { }
    virtual ~EmptySpellCheckerClient() { }

    virtual bool isContinuousSpellCheckingEnabled() override { return false; }
    virtual void toggleContinuousSpellChecking() override { }
    virtual bool isGrammarCheckingEnabled() override { return false; }

    virtual TextCheckerClient& textChecker() override { return m_textCheckerClient; }

    virtual void updateSpellingUIWithMisspelledWord(const String&) override { }
    virtual void showSpellingUI(bool) override { }
    virtual bool spellingUIIsShowing() override { return false; }

private:
    EmptyTextCheckerClient m_textCheckerClient;
};

class EmptyEditorClient final : public EditorClient {
    WTF_MAKE_NONCOPYABLE(EmptyEditorClient); WTF_MAKE_FAST_ALLOCATED(EmptyEditorClient);
public:
    EmptyEditorClient() { }
    virtual ~EmptyEditorClient() { }

    virtual void respondToChangedContents() override { }
    virtual void respondToChangedSelection(LocalFrame*, SelectionType) override { }

    virtual bool canCopyCut(LocalFrame*, bool defaultValue) const override { return defaultValue; }
    virtual bool canPaste(LocalFrame*, bool defaultValue) const override { return defaultValue; }

    virtual bool handleKeyboardEvent() override { return false; }
};

class EmptyContextMenuClient final : public ContextMenuClient {
    WTF_MAKE_NONCOPYABLE(EmptyContextMenuClient); WTF_MAKE_FAST_ALLOCATED(EmptyContextMenuClient);
public:
    EmptyContextMenuClient() { }
    virtual ~EmptyContextMenuClient() {  }
    virtual void showContextMenu(const ContextMenu*) override { }
    virtual void clearContextMenu() override { }
};

class EmptyDragClient final : public DragClient {
    WTF_MAKE_NONCOPYABLE(EmptyDragClient); WTF_MAKE_FAST_ALLOCATED(EmptyDragClient);
public:
    EmptyDragClient() { }
    virtual ~EmptyDragClient() {}
    virtual DragDestinationAction actionMaskForDrag(DragData*) override { return DragDestinationActionNone; }
    virtual void startDrag(DragImage*, const IntPoint&, const IntPoint&, DataTransfer*, LocalFrame*, bool) override { }
};

CORE_EXPORT void fillWithEmptyClients(Page::PageClients&);

} // namespace blink

#endif // EmptyClients_h
