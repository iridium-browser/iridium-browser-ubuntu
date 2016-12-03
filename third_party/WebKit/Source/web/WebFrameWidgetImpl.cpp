/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "web/WebFrameWidgetImpl.h"

#include "core/InputTypeNames.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PlainTextRange.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/ContextMenuController.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/PointerLockController.h"
#include "platform/KeyboardCodes.h"
#include "platform/graphics/CompositorMutatorClient.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebRange.h"
#include "public/web/WebWidgetClient.h"
#include "web/CompositionUnderlineVectorBuilder.h"
#include "web/CompositorMutatorImpl.h"
#include "web/CompositorProxyClientImpl.h"
#include "web/ContextMenuAllowedScope.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/WebViewFrameWidget.h"
#include "wtf/AutoReset.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

// WebFrameWidget ----------------------------------------------------------------

WebFrameWidget* WebFrameWidget::create(WebWidgetClient* client, WebLocalFrame* localRoot)
{
    // Pass the WebFrameWidget's self-reference to the caller.
    return WebFrameWidgetImpl::create(client, localRoot);
}

WebFrameWidget* WebFrameWidget::create(WebWidgetClient* client, WebView* webView, WebLocalFrame* mainFrame)
{
    return new WebViewFrameWidget(client, toWebViewImpl(*webView), toWebLocalFrameImpl(*mainFrame));
}

WebFrameWidgetImpl* WebFrameWidgetImpl::create(WebWidgetClient* client, WebLocalFrame* localRoot)
{
    // Pass the WebFrameWidgetImpl's self-reference to the caller.
    return new WebFrameWidgetImpl(client, localRoot); // SelfKeepAlive is set in constructor.
}

// static
WebFrameWidgetsSet& WebFrameWidgetImpl::allInstances()
{
    DEFINE_STATIC_LOCAL(WebFrameWidgetsSet, allInstances, ());
    return allInstances;
}

WebFrameWidgetImpl::WebFrameWidgetImpl(WebWidgetClient* client, WebLocalFrame* localRoot)
    : m_client(client)
    , m_localRoot(toWebLocalFrameImpl(localRoot))
    , m_mutator(nullptr)
    , m_layerTreeView(nullptr)
    , m_rootLayer(nullptr)
    , m_rootGraphicsLayer(nullptr)
    , m_isAcceleratedCompositingActive(false)
    , m_layerTreeViewClosed(false)
    , m_suppressNextKeypressEvent(false)
    , m_ignoreInputEvents(false)
    , m_isTransparent(false)
    , m_imeAcceptEvents(true)
    , m_selfKeepAlive(this)
{
    DCHECK(m_localRoot->frame()->isLocalRoot());
    initializeLayerTreeView();
    m_localRoot->setFrameWidget(this);
    allInstances().add(this);

    if (localRoot->parent())
        setIsTransparent(true);
}

WebFrameWidgetImpl::~WebFrameWidgetImpl()
{
}

DEFINE_TRACE(WebFrameWidgetImpl)
{
    visitor->trace(m_localRoot);
    visitor->trace(m_mouseCaptureNode);
}

// WebWidget ------------------------------------------------------------------

void WebFrameWidgetImpl::close()
{
    WebDevToolsAgentImpl::webFrameWidgetImplClosed(this);
    DCHECK(allInstances().contains(this));
    allInstances().remove(this);

    m_localRoot->setFrameWidget(nullptr);
    m_localRoot = nullptr;
    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = nullptr;

    m_mutator = nullptr;
    m_layerTreeView = nullptr;
    m_rootLayer = nullptr;
    m_rootGraphicsLayer = nullptr;

    m_selfKeepAlive.clear();
}

WebSize WebFrameWidgetImpl::size()
{
    return m_size;
}

void WebFrameWidgetImpl::resize(const WebSize& newSize)
{
    if (m_size == newSize)
        return;

    FrameView* view = m_localRoot->frameView();
    if (!view)
        return;

    m_size = newSize;

    updateMainFrameLayoutSize();

    view->resize(m_size);

    // FIXME: In WebViewImpl this layout was a precursor to setting the minimum scale limit.
    // It is not clear if this is necessary for frame-level widget resize.
    if (view->needsLayout())
        view->layout();

    // FIXME: Investigate whether this is needed; comment from eseidel suggests that this function
    // is flawed.
    sendResizeEventAndRepaint();
}

void WebFrameWidgetImpl::sendResizeEventAndRepaint()
{
    // FIXME: This is wrong. The FrameView is responsible sending a resizeEvent
    // as part of layout. Layout is also responsible for sending invalidations
    // to the embedder. This method and all callers may be wrong. -- eseidel.
    if (m_localRoot->frameView()) {
        // Enqueues the resize event.
        m_localRoot->frame()->document()->enqueueResizeEvent();
    }

    if (m_client) {
        if (isAcceleratedCompositingActive()) {
            updateLayerTreeViewport();
        } else {
            WebRect damagedRect(0, 0, m_size.width, m_size.height);
            m_client->didInvalidateRect(damagedRect);
        }
    }
}

void WebFrameWidgetImpl::resizeVisualViewport(const WebSize& newSize)
{
    // TODO(alexmos, kenrb): resizing behavior such as this should be changed
    // to use Page messages.  https://crbug.com/599688.
    page()->frameHost().visualViewport().setSize(newSize);
    page()->frameHost().visualViewport().clampToBoundaries();

    view()->didUpdateFullScreenSize();
}

void WebFrameWidgetImpl::updateMainFrameLayoutSize()
{
    if (!m_localRoot)
        return;

    FrameView* view = m_localRoot->frameView();
    if (!view)
        return;

    WebSize layoutSize = m_size;

    view->setLayoutSize(layoutSize);
}

void WebFrameWidgetImpl::setIgnoreInputEvents(bool newValue)
{
    DCHECK_NE(m_ignoreInputEvents, newValue);
    m_ignoreInputEvents = newValue;
}

void WebFrameWidgetImpl::didEnterFullscreen()
{
    view()->didEnterFullscreen();
}

void WebFrameWidgetImpl::didExitFullscreen()
{
    view()->didExitFullscreen();
}

void WebFrameWidgetImpl::beginFrame(double lastFrameTimeMonotonic)
{
    TRACE_EVENT1("blink", "WebFrameWidgetImpl::beginFrame", "frameTime", lastFrameTimeMonotonic);
    DCHECK(lastFrameTimeMonotonic);
    PageWidgetDelegate::animate(*page(), lastFrameTimeMonotonic);
}

void WebFrameWidgetImpl::updateAllLifecyclePhases()
{
    TRACE_EVENT0("blink", "WebFrameWidgetImpl::updateAllLifecyclePhases");
    if (!m_localRoot)
        return;

    PageWidgetDelegate::updateAllLifecyclePhases(*page(), *m_localRoot->frame());
    updateLayerTreeBackgroundColor();
}

void WebFrameWidgetImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    // Out-of-process iframes require compositing.
    NOTREACHED();
}


void WebFrameWidgetImpl::updateLayerTreeViewport()
{
    if (!page() || !m_layerTreeView)
        return;

    // FIXME: We need access to page scale information from the WebView.
    m_layerTreeView->setPageScaleFactorAndLimits(1, 1, 1);
}

void WebFrameWidgetImpl::updateLayerTreeBackgroundColor()
{
    if (!m_layerTreeView)
        return;

    m_layerTreeView->setBackgroundColor(backgroundColor());
}

void WebFrameWidgetImpl::updateLayerTreeDeviceScaleFactor()
{
    DCHECK(page());
    DCHECK(m_layerTreeView);

    float deviceScaleFactor = page()->deviceScaleFactor();
    m_layerTreeView->setDeviceScaleFactor(deviceScaleFactor);
}

void WebFrameWidgetImpl::setIsTransparent(bool isTransparent)
{
    m_isTransparent = isTransparent;

    if (m_layerTreeView)
        m_layerTreeView->setHasTransparentBackground(isTransparent);
}

bool WebFrameWidgetImpl::isTransparent() const
{
    return m_isTransparent;
}

void WebFrameWidgetImpl::layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback* callback)
{
    m_layerTreeView->layoutAndPaintAsync(callback);
}

void WebFrameWidgetImpl::compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback* callback)
{
    m_layerTreeView->compositeAndReadbackAsync(callback);
}

void WebFrameWidgetImpl::themeChanged()
{
    FrameView* view = m_localRoot->frameView();

    WebRect damagedRect(0, 0, m_size.width, m_size.height);
    view->invalidateRect(damagedRect);
}

const WebInputEvent* WebFrameWidgetImpl::m_currentInputEvent = nullptr;

WebInputEventResult WebFrameWidgetImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    TRACE_EVENT1("input", "WebFrameWidgetImpl::handleInputEvent", "type", WebInputEvent::GetName(inputEvent.type));

    // Don't handle events once we've started shutting down.
    if (!page())
        return WebInputEventResult::NotHandled;

    // Report the event to be NOT processed by WebKit, so that the browser can handle it appropriately.
    if (m_ignoreInputEvents)
        return WebInputEventResult::NotHandled;

    // FIXME: pass event to m_localRoot's WebDevToolsAgentImpl once available.

    AutoReset<const WebInputEvent*> currentEventChange(&m_currentInputEvent, &inputEvent);

    if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
        TRACE_EVENT1("input", "captured mouse event", "type", inputEvent.type);
        // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
        Node* node = m_mouseCaptureNode;

        // Not all platforms call mouseCaptureLost() directly.
        if (inputEvent.type == WebInputEvent::MouseUp)
            mouseCaptureLost();

        std::unique_ptr<UserGestureIndicator> gestureIndicator;

        AtomicString eventType;
        switch (inputEvent.type) {
        case WebInputEvent::MouseMove:
            eventType = EventTypeNames::mousemove;
            break;
        case WebInputEvent::MouseLeave:
            eventType = EventTypeNames::mouseout;
            break;
        case WebInputEvent::MouseDown:
            eventType = EventTypeNames::mousedown;
            gestureIndicator = wrapUnique(new UserGestureIndicator(DefinitelyProcessingNewUserGesture));
            m_mouseCaptureGestureToken = gestureIndicator->currentToken();
            break;
        case WebInputEvent::MouseUp:
            eventType = EventTypeNames::mouseup;
            gestureIndicator = wrapUnique(new UserGestureIndicator(m_mouseCaptureGestureToken.release()));
            break;
        default:
            NOTREACHED();
        }

        node->dispatchMouseEvent(
            PlatformMouseEventBuilder(m_localRoot->frameView(), static_cast<const WebMouseEvent&>(inputEvent)),
            eventType, static_cast<const WebMouseEvent&>(inputEvent).clickCount);
        return WebInputEventResult::HandledSystem;
    }

    return PageWidgetDelegate::handleInputEvent(*this, inputEvent, m_localRoot->frame());
}

void WebFrameWidgetImpl::setCursorVisibilityState(bool isVisible)
{
    page()->setIsCursorVisible(isVisible);
}

bool WebFrameWidgetImpl::hasTouchEventHandlersAt(const WebPoint& point)
{
    // FIXME: Implement this. Note that the point must be divided by pageScaleFactor.
    return true;
}

void WebFrameWidgetImpl::setBaseBackgroundColor(WebColor color)
{
    if (m_baseBackgroundColor == color)
        return;

    m_baseBackgroundColor = color;

    m_localRoot->frameView()->setBaseBackgroundColor(color);
}

void WebFrameWidgetImpl::scheduleAnimation()
{
    if (m_layerTreeView) {
        m_layerTreeView->setNeedsBeginFrame();
        return;
    }
    if (m_client)
        m_client->scheduleAnimation();
}

CompositorProxyClient* WebFrameWidgetImpl::createCompositorProxyClient()
{
    if (!m_mutator) {
        std::unique_ptr<CompositorMutatorClient> mutatorClient = CompositorMutatorImpl::createClient();
        m_mutator = static_cast<CompositorMutatorImpl*>(mutatorClient->mutator());
        m_layerTreeView->setMutatorClient(std::move(mutatorClient));
    }
    return new CompositorProxyClientImpl(m_mutator);
}

void WebFrameWidgetImpl::applyViewportDeltas(
    const WebFloatSize& visualViewportDelta,
    const WebFloatSize& mainFrameDelta,
    const WebFloatSize& elasticOverscrollDelta,
    float pageScaleDelta,
    float topControlsDelta)
{
    // FIXME: To be implemented.
}

void WebFrameWidgetImpl::mouseCaptureLost()
{
    TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
    m_mouseCaptureNode = nullptr;
}

void WebFrameWidgetImpl::setFocus(bool enable)
{
    page()->focusController().setFocused(enable);
    if (enable) {
        page()->focusController().setActive(true);
        LocalFrame* focusedFrame = page()->focusController().focusedFrame();
        if (focusedFrame) {
            Element* element = focusedFrame->document()->focusedElement();
            if (element && focusedFrame->selection().selection().isNone()) {
                // If the selection was cleared while the WebView was not
                // focused, then the focus element shows with a focus ring but
                // no caret and does respond to keyboard inputs.
                focusedFrame->document()->updateStyleAndLayoutTree();
                if (element->isTextFormControl()) {
                    element->updateFocusAppearance(SelectionBehaviorOnFocus::Restore);
                } else if (hasEditableStyle(*element)) {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the beginning of the text.
                    Position position(element, 0);
                    focusedFrame->selection().setSelection(VisibleSelection(position, SelDefaultAffinity));
                }
            }
        }
    } else {
        LocalFrame* focusedFrame = focusedLocalFrameInWidget();
        if (focusedFrame) {
            // Finish an ongoing composition to delete the composition node.
            if (focusedFrame->inputMethodController().hasComposition()) {
                WebAutofillClient* autofillClient = WebLocalFrameImpl::fromFrame(focusedFrame)->autofillClient();

                if (autofillClient)
                    autofillClient->setIgnoreTextChanges(true);

                focusedFrame->inputMethodController().confirmComposition();

                if (autofillClient)
                    autofillClient->setIgnoreTextChanges(false);
            }
            m_imeAcceptEvents = false;
        }
    }
}

bool WebFrameWidgetImpl::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd)
{
    LocalFrame* focused = focusedLocalFrameAvailableForIme();
    if (!focused)
        return false;

    if (WebPlugin* plugin = focusedPluginIfInputMethodSupported(focused))
        return plugin->setComposition(text, underlines, selectionStart, selectionEnd);

    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    InputMethodController& inputMethodController = focused->inputMethodController();
    if (!focused->editor().canEdit() && !inputMethodController.hasComposition())
        return false;

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    const EphemeralRange range = inputMethodController.compositionEphemeralRange();
    if (range.isNotNull()) {
        Node* node = range.startPosition().computeContainerNode();
        focused->document()->updateStyleAndLayoutTree();
        if (!node || !hasEditableStyle(*node))
            return false;
    }

    // A keypress event is canceled. If an ongoing composition exists, then the
    // keydown event should have arisen from a handled key (e.g., backspace).
    // In this case we ignore the cancellation and continue; otherwise (no
    // ongoing composition) we exit and signal success only for attempts to
    // clear the composition.
    if (m_suppressNextKeypressEvent && !inputMethodController.hasComposition())
        return text.isEmpty();

    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);

    // When the range of composition underlines overlap with the range between
    // selectionStart and selectionEnd, WebKit somehow won't paint the selection
    // at all (see InlineTextBox::paint() function in InlineTextBox.cpp).
    // But the selection range actually takes effect.
    inputMethodController.setComposition(String(text),
        CompositionUnderlineVectorBuilder(underlines),
        selectionStart, selectionEnd);

    return text.isEmpty() || inputMethodController.hasComposition();
}

bool WebFrameWidgetImpl::confirmComposition()
{
    return confirmComposition(DoNotKeepSelection);
}

bool WebFrameWidgetImpl::confirmComposition(ConfirmCompositionBehavior selectionBehavior)
{
    return confirmComposition(WebString(), selectionBehavior);
}

bool WebFrameWidgetImpl::confirmComposition(const WebString& text)
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    return confirmComposition(text, DoNotKeepSelection);
}

bool WebFrameWidgetImpl::confirmComposition(const WebString& text, ConfirmCompositionBehavior selectionBehavior) const
{
    LocalFrame* focused = focusedLocalFrameAvailableForIme();
    if (!focused)
        return false;

    if (WebPlugin* plugin = focusedPluginIfInputMethodSupported(focused))
        return plugin->confirmComposition(text, selectionBehavior);

    return focused->inputMethodController().confirmCompositionOrInsertText(text, selectionBehavior == KeepSelection ? InputMethodController::KeepSelection : InputMethodController::DoNotKeepSelection);
}

bool WebFrameWidgetImpl::compositionRange(size_t* location, size_t* length)
{
    LocalFrame* focused = focusedLocalFrameAvailableForIme();
    if (!focused)
        return false;

    const EphemeralRange range = focused->inputMethodController().compositionEphemeralRange();
    if (range.isNull())
        return false;

    Element* editable = focused->selection().rootEditableElementOrDocumentElement();
    DCHECK(editable);
    PlainTextRange plainTextRange(PlainTextRange::create(*editable, range));
    if (plainTextRange.isNull())
        return false;
    *location = plainTextRange.start();
    *length = plainTextRange.length();
    return true;
}

WebTextInputInfo WebFrameWidgetImpl::textInputInfo()
{
    WebTextInputInfo info;

    LocalFrame* focused = focusedLocalFrameInWidget();
    if (!focused)
        return info;

    FrameSelection& selection = focused->selection();
    if (!selection.isAvailable()) {
        // plugins/mouse-capture-inside-shadow.html reaches here.
        return info;
    }
    Element* element = selection.selection().rootEditableElement();
    if (!element)
        return info;

    info.inputMode = inputModeOfFocusedElement();

    info.type = textInputType();
    info.flags = textInputFlags();
    if (info.type == WebTextInputTypeNone)
        return info;

    if (!focused->editor().canEdit())
        return info;

    // Emits an object replacement character for each replaced element so that
    // it is exposed to IME and thus could be deleted by IME on android.
    info.value = plainText(EphemeralRange::rangeOfContents(*element), TextIteratorEmitsObjectReplacementCharacter);

    if (info.value.isEmpty())
        return info;

    EphemeralRange firstRange = firstEphemeralRangeOf(selection.selection());
    if (firstRange.isNotNull()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*element, firstRange));
        if (plainTextRange.isNotNull()) {
            info.selectionStart = plainTextRange.start();
            info.selectionEnd = plainTextRange.end();
        }
    }

    EphemeralRange range = focused->inputMethodController().compositionEphemeralRange();
    if (range.isNotNull()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*element, range));
        if (plainTextRange.isNotNull()) {
            info.compositionStart = plainTextRange.start();
            info.compositionEnd = plainTextRange.end();
        }
    }

    return info;
}

WebTextInputType WebFrameWidgetImpl::textInputType()
{
    LocalFrame* focusedFrame = focusedLocalFrameInWidget();
    if (!focusedFrame)
        return WebTextInputTypeNone;

    if (!focusedFrame->selection().isAvailable()) {
        // "mouse-capture-inside-shadow.html" reaches here.
        return WebTextInputTypeNone;
    }

    // It's important to preserve the equivalence of textInputInfo().type and textInputType(),
    // so perform the same rootEditableElement() existence check here for consistency.
    if (!focusedFrame->selection().selection().rootEditableElement())
        return WebTextInputTypeNone;

    Document* document = focusedFrame->document();
    if (!document)
        return WebTextInputTypeNone;

    Element* element = document->focusedElement();
    if (!element)
        return WebTextInputTypeNone;

    if (isHTMLInputElement(*element)) {
        HTMLInputElement& input = toHTMLInputElement(*element);
        const AtomicString& type = input.type();

        if (input.isDisabledOrReadOnly())
            return WebTextInputTypeNone;

        if (type == InputTypeNames::password)
            return WebTextInputTypePassword;
        if (type == InputTypeNames::search)
            return WebTextInputTypeSearch;
        if (type == InputTypeNames::email)
            return WebTextInputTypeEmail;
        if (type == InputTypeNames::number)
            return WebTextInputTypeNumber;
        if (type == InputTypeNames::tel)
            return WebTextInputTypeTelephone;
        if (type == InputTypeNames::url)
            return WebTextInputTypeURL;
        if (type == InputTypeNames::date)
            return WebTextInputTypeDate;
        if (type == InputTypeNames::datetime_local)
            return WebTextInputTypeDateTimeLocal;
        if (type == InputTypeNames::month)
            return WebTextInputTypeMonth;
        if (type == InputTypeNames::time)
            return WebTextInputTypeTime;
        if (type == InputTypeNames::week)
            return WebTextInputTypeWeek;
        if (type == InputTypeNames::text)
            return WebTextInputTypeText;

        return WebTextInputTypeNone;
    }

    if (isHTMLTextAreaElement(*element)) {
        if (toHTMLTextAreaElement(*element).isDisabledOrReadOnly())
            return WebTextInputTypeNone;
        return WebTextInputTypeTextArea;
    }

    if (element->isHTMLElement()) {
        if (toHTMLElement(element)->isDateTimeFieldElement())
            return WebTextInputTypeDateTimeField;
    }

    document->updateStyleAndLayoutTree();
    if (hasEditableStyle(*element))
        return WebTextInputTypeContentEditable;

    return WebTextInputTypeNone;
}

WebColor WebFrameWidgetImpl::backgroundColor() const
{
    if (isTransparent())
        return Color::transparent;
    if (!m_localRoot->frameView())
        return m_baseBackgroundColor;
    FrameView* view = m_localRoot->frameView();
    return view->documentBackgroundColor().rgb();
}

bool WebFrameWidgetImpl::selectionBounds(WebRect& anchor, WebRect& focus) const
{
    const LocalFrame* localFrame = focusedLocalFrameInWidget();
    if (!localFrame)
        return false;

    FrameSelection& selection = localFrame->selection();

    if (selection.isCaret()) {
        anchor = focus = selection.absoluteCaretBounds();
    } else {
        const EphemeralRange selectedRange = selection.selection().toNormalizedEphemeralRange();
        if (selectedRange.isNull())
            return false;
        anchor = localFrame->editor().firstRectForRange(EphemeralRange(selectedRange.startPosition()));
        focus = localFrame->editor().firstRectForRange(EphemeralRange(selectedRange.endPosition()));
    }

    // FIXME: This doesn't apply page scale. This should probably be contents to viewport. crbug.com/459293.
    IntRect scaledAnchor(localFrame->view()->contentsToRootFrame(anchor));
    IntRect scaledFocus(localFrame->view()->contentsToRootFrame(focus));

    anchor = scaledAnchor;
    focus = scaledFocus;

    if (!selection.selection().isBaseFirst())
        std::swap(anchor, focus);
    return true;
}

bool WebFrameWidgetImpl::selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const
{
    const LocalFrame* frame = focusedLocalFrameInWidget();
    if (!frame)
        return false;

    FrameSelection& selection = frame->selection();
    if (selection.selection().toNormalizedEphemeralRange().isNull())
        return false;
    start = toWebTextDirection(primaryDirectionOf(*selection.start().anchorNode()));
    end = toWebTextDirection(primaryDirectionOf(*selection.end().anchorNode()));
    return true;
}

bool WebFrameWidgetImpl::isSelectionAnchorFirst() const
{
    if (const LocalFrame* frame = focusedLocalFrameInWidget())
        return frame->selection().selection().isBaseFirst();
    return false;
}

bool WebFrameWidgetImpl::caretOrSelectionRange(size_t* location, size_t* length)
{
    LocalFrame* focused = focusedLocalFrameInWidget();
    if (!focused)
        return false;

    PlainTextRange selectionOffsets = focused->inputMethodController().getSelectionOffsets();
    if (selectionOffsets.isNull())
        return false;

    *location = selectionOffsets.start();
    *length = selectionOffsets.length();
    return true;
}

void WebFrameWidgetImpl::setTextDirection(WebTextDirection direction)
{
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

bool WebFrameWidgetImpl::isAcceleratedCompositingActive() const
{
    return m_isAcceleratedCompositingActive;
}

void WebFrameWidgetImpl::willCloseLayerTreeView()
{
    if (m_layerTreeView)
        page()->willCloseLayerTreeView(*m_layerTreeView);

    setIsAcceleratedCompositingActive(false);
    m_mutator = nullptr;
    m_layerTreeView = nullptr;
    m_layerTreeViewClosed = true;
}

void WebFrameWidgetImpl::didChangeWindowResizerRect()
{
    if (m_localRoot->frameView())
        m_localRoot->frameView()->windowResizerRectChanged();
}

void WebFrameWidgetImpl::didAcquirePointerLock()
{
    page()->pointerLockController().didAcquirePointerLock();
}

void WebFrameWidgetImpl::didNotAcquirePointerLock()
{
    page()->pointerLockController().didNotAcquirePointerLock();
}

void WebFrameWidgetImpl::didLosePointerLock()
{
    page()->pointerLockController().didLosePointerLock();
}

bool WebFrameWidgetImpl::getCompositionCharacterBounds(WebVector<WebRect>& bounds)
{
    size_t offset = 0;
    size_t characterCount = 0;
    if (!compositionRange(&offset, &characterCount))
        return false;

    if (characterCount == 0)
        return false;

    LocalFrame* frame = focusedLocalFrameInWidget();
    if (!frame)
        return false;

    WebLocalFrameImpl* webLocalFrame = WebLocalFrameImpl::fromFrame(frame);
    WebVector<WebRect> result(characterCount);
    WebRect webrect;
    for (size_t i = 0; i < characterCount; ++i) {
        if (!webLocalFrame->firstRectForCharacterRange(offset + i, 1, webrect)) {
            DLOG(ERROR) << "Could not retrieve character rectangle at " << i;
            return false;
        }
        result[i] = webrect;
    }
    bounds.swap(result);
    return true;
}

void WebFrameWidgetImpl::applyReplacementRange(int start, int length)
{
    if (LocalFrame* frame = focusedLocalFrameInWidget()) {
        // TODO(dglazkov): Going from LocalFrame to WebLocalFrameImpl seems
        // silly. What is going on here?
        WebLocalFrameImpl::fromFrame(frame)->selectRange(WebRange(start, length));
    }
}

void WebFrameWidgetImpl::handleMouseLeave(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    // FIXME: WebWidget doesn't have the method below.
    // m_client->setMouseOverURL(WebURL());
    PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebFrameWidgetImpl::handleMouseDown(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    // Take capture on a mouse down on a plugin so we can send it mouse events.
    // If the hit node is a plugin but a scrollbar is over it don't start mouse
    // capture because it will interfere with the scrollbar receiving events.
    IntPoint point(event.x, event.y);
    if (event.button == WebMouseEvent::Button::Left) {
        point = m_localRoot->frameView()->rootFrameToContents(point);
        HitTestResult result(m_localRoot->frame()->eventHandler().hitTestResultAtPoint(point));
        result.setToShadowHostIfInUserAgentShadowRoot();
        Node* hitNode = result.innerNode();

        if (!result.scrollbar() && hitNode && hitNode->layoutObject() && hitNode->layoutObject()->isEmbeddedObject()) {
            m_mouseCaptureNode = hitNode;
            TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
        }
    }

    PageWidgetEventHandler::handleMouseDown(mainFrame, event);

    if (event.button == WebMouseEvent::Button::Left && m_mouseCaptureNode)
        m_mouseCaptureGestureToken = mainFrame.eventHandler().takeLastMouseDownGestureToken();

    // Dispatch the contextmenu event regardless of if the click was swallowed.
    if (!page()->settings().showContextMenuOnMouseUp()) {
#if OS(MACOSX)
        if (event.button == WebMouseEvent::Button::Right || (event.button == WebMouseEvent::Button::Left && event.modifiers & WebMouseEvent::ControlKey))
            mouseContextMenu(event);
#else
        if (event.button == WebMouseEvent::Button::Right)
            mouseContextMenu(event);
#endif
    }
}

void WebFrameWidgetImpl::mouseContextMenu(const WebMouseEvent& event)
{
    page()->contextMenuController().clearContextMenu();

    PlatformMouseEventBuilder pme(m_localRoot->frameView(), event);

    // Find the right target frame. See issue 1186900.
    HitTestResult result = hitTestResultForRootFramePos(pme.position());
    Frame* targetFrame;
    if (result.innerNodeOrImageMapImage())
        targetFrame = result.innerNodeOrImageMapImage()->document().frame();
    else
        targetFrame = page()->focusController().focusedOrMainFrame();

    // This will need to be changed to a nullptr check when focus control
    // is refactored, at which point focusedOrMainFrame will never return a
    // RemoteFrame.
    // See https://crbug.com/341918.
    if (!targetFrame->isLocalFrame())
        return;

    LocalFrame* targetLocalFrame = toLocalFrame(targetFrame);

#if OS(WIN)
    targetLocalFrame->view()->setCursor(pointerCursor());
#endif

    {
        ContextMenuAllowedScope scope;
        targetLocalFrame->eventHandler().sendContextMenuEvent(pme, nullptr);
    }
    // Actually showing the context menu is handled by the ContextMenuClient
    // implementation...
}

void WebFrameWidgetImpl::handleMouseUp(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    PageWidgetEventHandler::handleMouseUp(mainFrame, event);

    if (page()->settings().showContextMenuOnMouseUp()) {
        // Dispatch the contextmenu event regardless of if the click was swallowed.
        // On Mac/Linux, we handle it on mouse down, not up.
        if (event.button == WebMouseEvent::Button::Right)
            mouseContextMenu(event);
    }
}

WebInputEventResult WebFrameWidgetImpl::handleMouseWheel(LocalFrame& mainFrame, const WebMouseWheelEvent& event)
{
    return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

WebInputEventResult WebFrameWidgetImpl::handleGestureEvent(const WebGestureEvent& event)
{
    WebInputEventResult eventResult = WebInputEventResult::NotHandled;
    bool eventCancelled = false;
    switch (event.type) {
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureShowPress:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureDoubleTap:
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap:
        break;
    case WebInputEvent::GestureFlingStart:
    case WebInputEvent::GestureFlingCancel:
        m_client->didHandleGestureEvent(event, eventCancelled);
        return WebInputEventResult::NotHandled;
    default:
        NOTREACHED();
    }
    LocalFrame* frame = m_localRoot->frame();
    eventResult = frame->eventHandler().handleGestureEvent(PlatformGestureEventBuilder(frame->view(), event));
    m_client->didHandleGestureEvent(event, eventCancelled);
    return eventResult;
}

WebInputEventResult WebFrameWidgetImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    DCHECK((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    Frame* focusedFrame = focusedCoreFrame();
    if (focusedFrame && focusedFrame->isRemoteFrame()) {
        WebRemoteFrameImpl* webFrame = WebRemoteFrameImpl::fromFrame(*toRemoteFrame(focusedFrame));
        webFrame->client()->forwardInputEvent(&event);
        return WebInputEventResult::HandledSystem;
    }

    if (!focusedFrame || !focusedFrame->isLocalFrame())
        return WebInputEventResult::NotHandled;

    LocalFrame* frame = toLocalFrame(focusedFrame);

    PlatformKeyboardEventBuilder evt(event);

    WebInputEventResult result = frame->eventHandler().keyEvent(evt);
    if (result != WebInputEventResult::NotHandled) {
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plugin node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            Element* element = focusedElement();
            if (!element || !element->layoutObject() || !element->layoutObject()->isEmbeddedObject())
                m_suppressNextKeypressEvent = true;
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
    const WebInputEvent::Type shiftF10TriggeringEventType = WebInputEvent::RawKeyDown;

    bool isUnmodifiedMenuKey = !(event.modifiers & WebInputEvent::InputModifiers) && event.windowsKeyCode == VKEY_APPS;
    bool isShiftF10 = (event.modifiers & WebInputEvent::InputModifiers) == WebInputEvent::ShiftKey && event.windowsKeyCode == VKEY_F10;
    if ((isUnmodifiedMenuKey && event.type == contextMenuKeyTriggeringEventType)
        || (isShiftF10 && event.type == shiftF10TriggeringEventType)) {
        view()->sendContextMenuEvent(event);
        return WebInputEventResult::HandledSystem;
    }
#endif // !OS(MACOSX)

    return keyEventDefault(event);
}

WebInputEventResult WebFrameWidgetImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    DCHECK_EQ(event.type, WebInputEvent::Char);

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    LocalFrame* frame = toLocalFrame(focusedCoreFrame());
    if (!frame)
        return suppress ? WebInputEventResult::HandledSuppressed : WebInputEventResult::NotHandled;

    EventHandler& handler = frame->eventHandler();

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return WebInputEventResult::HandledSuppressed;

    // Accesskeys are triggered by char events and can't be suppressed.
    // It is unclear whether a keypress should be dispatched as well
    // crbug.com/563507
    if (handler.handleAccessKey(evt))
        return WebInputEventResult::HandledSystem;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return WebInputEventResult::NotHandled;

    if (suppress)
        return WebInputEventResult::HandledSuppressed;

    WebInputEventResult result = handler.keyEvent(evt);
    if (result != WebInputEventResult::NotHandled)
        return result;

    return keyEventDefault(event);
}

WebInputEventResult WebFrameWidgetImpl::keyEventDefault(const WebKeyboardEvent& event)
{
    LocalFrame* frame = toLocalFrame(focusedCoreFrame());
    if (!frame)
        return WebInputEventResult::NotHandled;

    switch (event.type) {
    case WebInputEvent::Char:
        if (event.windowsKeyCode == VKEY_SPACE) {
            int keyCode = ((event.modifiers & WebInputEvent::ShiftKey) ? VKEY_PRIOR : VKEY_NEXT);
            return scrollViewWithKeyboard(keyCode, event.modifiers);
        }
        break;
    case WebInputEvent::RawKeyDown:
        if (event.modifiers == WebInputEvent::ControlKey) {
            switch (event.windowsKeyCode) {
#if !OS(MACOSX)
            case 'A':
                WebFrame::fromFrame(focusedCoreFrame())->toWebLocalFrame()->executeCommand(WebString::fromUTF8("SelectAll"));
                return WebInputEventResult::HandledSystem;
            case VKEY_INSERT:
            case 'C':
                WebFrame::fromFrame(focusedCoreFrame())->toWebLocalFrame()->executeCommand(WebString::fromUTF8("Copy"));
                return WebInputEventResult::HandledSystem;
#endif
            // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
            // key combinations which affect scrolling. Safari is buggy in the
            // sense that it scrolls the page for all Ctrl+scrolling key
            // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
            case VKEY_HOME:
            case VKEY_END:
                break;
            default:
                return WebInputEventResult::NotHandled;
            }
        }
        if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey))
            return scrollViewWithKeyboard(event.windowsKeyCode, event.modifiers);
        break;
    default:
        break;
    }
    return WebInputEventResult::NotHandled;
}

WebInputEventResult WebFrameWidgetImpl::scrollViewWithKeyboard(int keyCode, int modifiers)
{
    ScrollDirection scrollDirection;
    ScrollGranularity scrollGranularity;
#if OS(MACOSX)
    // Control-Up/Down should be PageUp/Down on Mac.
    if (modifiers & WebMouseEvent::ControlKey) {
        if (keyCode == VKEY_UP)
            keyCode = VKEY_PRIOR;
        else if (keyCode == VKEY_DOWN)
            keyCode = VKEY_NEXT;
    }
#endif
    if (!mapKeyCodeForScroll(keyCode, &scrollDirection, &scrollGranularity))
        return WebInputEventResult::NotHandled;

    LocalFrame* frame = toLocalFrame(focusedCoreFrame());
    if (frame && frame->eventHandler().bubblingScroll(scrollDirection, scrollGranularity))
        return WebInputEventResult::HandledSystem;
    return WebInputEventResult::NotHandled;
}

bool WebFrameWidgetImpl::mapKeyCodeForScroll(
    int keyCode,
    ScrollDirection* scrollDirection,
    ScrollGranularity* scrollGranularity)
{
    switch (keyCode) {
    case VKEY_LEFT:
        *scrollDirection = ScrollLeftIgnoringWritingMode;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_RIGHT:
        *scrollDirection = ScrollRightIgnoringWritingMode;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_UP:
        *scrollDirection = ScrollUpIgnoringWritingMode;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_DOWN:
        *scrollDirection = ScrollDownIgnoringWritingMode;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_HOME:
        *scrollDirection = ScrollUpIgnoringWritingMode;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_END:
        *scrollDirection = ScrollDownIgnoringWritingMode;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_PRIOR: // page up
        *scrollDirection = ScrollUpIgnoringWritingMode;
        *scrollGranularity = ScrollByPage;
        break;
    case VKEY_NEXT: // page down
        *scrollDirection = ScrollDownIgnoringWritingMode;
        *scrollGranularity = ScrollByPage;
        break;
    default:
        return false;
    }

    return true;
}

Frame* WebFrameWidgetImpl::focusedCoreFrame() const
{
    return page() ? page()->focusController().focusedOrMainFrame() : nullptr;
}

Element* WebFrameWidgetImpl::focusedElement() const
{
    LocalFrame* frame = page()->focusController().focusedFrame();
    if (!frame)
        return nullptr;

    Document* document = frame->document();
    if (!document)
        return nullptr;

    return document->focusedElement();
}

void WebFrameWidgetImpl::initializeLayerTreeView()
{
    if (m_client) {
        DCHECK(!m_mutator);
        m_client->initializeLayerTreeView();
        m_layerTreeView = m_client->layerTreeView();
    }

    if (WebDevToolsAgentImpl* devTools = m_localRoot->devToolsAgentImpl())
        devTools->layerTreeViewChanged(m_layerTreeView);

    page()->settings().setAcceleratedCompositingEnabled(m_layerTreeView);
    if (m_layerTreeView)
        page()->layerTreeViewInitialized(*m_layerTreeView);

    // FIXME: only unittests, click to play, Android priting, and printing (for headers and footers)
    // make this assert necessary. We should make them not hit this code and then delete allowsBrokenNullLayerTreeView.
    DCHECK(m_layerTreeView || !m_client || m_client->allowsBrokenNullLayerTreeView());
}

void WebFrameWidgetImpl::setIsAcceleratedCompositingActive(bool active)
{
    // In the middle of shutting down; don't try to spin back up a compositor.
    // FIXME: compositing startup/shutdown should be refactored so that it
    // turns on explicitly rather than lazily, which causes this awkwardness.
    if (m_layerTreeViewClosed)
        return;

    DCHECK(!active || m_layerTreeView);

    if (m_isAcceleratedCompositingActive == active)
        return;

    if (!m_client)
        return;

    if (active) {
        TRACE_EVENT0("blink", "WebViewImpl::setIsAcceleratedCompositingActive(true)");
        m_layerTreeView->setRootLayer(*m_rootLayer);

        m_layerTreeView->setVisible(page()->isPageVisible());
        updateLayerTreeDeviceScaleFactor();
        updateLayerTreeBackgroundColor();
        m_layerTreeView->setHasTransparentBackground(isTransparent());
        updateLayerTreeViewport();
        m_isAcceleratedCompositingActive = true;
    }
}

PaintLayerCompositor* WebFrameWidgetImpl::compositor() const
{
    LocalFrame* frame = m_localRoot->frame();
    if (!frame || !frame->document() || frame->document()->layoutViewItem().isNull())
        return nullptr;

    return frame->document()->layoutViewItem().compositor();
}

void WebFrameWidgetImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    m_rootGraphicsLayer = layer;
    m_rootLayer = layer ? layer->platformLayer() : nullptr;

    setIsAcceleratedCompositingActive(layer);

    if (!m_layerTreeView)
        return;

    if (m_rootLayer)
        m_layerTreeView->setRootLayer(*m_rootLayer);
    else
        m_layerTreeView->clearRootLayer();
}

void WebFrameWidgetImpl::attachCompositorAnimationTimeline(CompositorAnimationTimeline* compositorTimeline)
{
    if (m_layerTreeView)
        m_layerTreeView->attachCompositorAnimationTimeline(compositorTimeline->animationTimeline());

}

void WebFrameWidgetImpl::detachCompositorAnimationTimeline(CompositorAnimationTimeline* compositorTimeline)
{
    if (m_layerTreeView)
        m_layerTreeView->detachCompositorAnimationTimeline(compositorTimeline->animationTimeline());
}

void WebFrameWidgetImpl::setVisibilityState(WebPageVisibilityState visibilityState)
{
    if (m_layerTreeView)
        m_layerTreeView->setVisible(visibilityState == WebPageVisibilityStateVisible);
}

HitTestResult WebFrameWidgetImpl::hitTestResultForRootFramePos(const IntPoint& posInRootFrame)
{
    IntPoint docPoint(m_localRoot->frame()->view()->rootFrameToContents(posInRootFrame));
    HitTestResult result = m_localRoot->frame()->eventHandler().hitTestResultAtPoint(docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
    result.setToShadowHostIfInUserAgentShadowRoot();
    return result;
}

LocalFrame* WebFrameWidgetImpl::focusedLocalFrameInWidget() const
{
    LocalFrame* frame = page()->focusController().focusedFrame();
    return (frame && frame->localFrameRoot() == m_localRoot->frame()) ? frame : nullptr;
}

WebPlugin* WebFrameWidgetImpl::focusedPluginIfInputMethodSupported(LocalFrame* frame) const
{
    WebPluginContainerImpl* container = WebLocalFrameImpl::currentPluginContainer(frame);
    if (container && container->supportsInputMethod())
        return container->plugin();
    return nullptr;
}

WebString WebFrameWidgetImpl::inputModeOfFocusedElement() const
{
    if (!RuntimeEnabledFeatures::inputModeAttributeEnabled())
        return WebString();

    Element* element = focusedElement();
    if (!element)
        return WebString();

    if (isHTMLInputElement(*element)) {
        const HTMLInputElement& input = toHTMLInputElement(*element);
        if (input.supportsInputModeAttribute())
            return input.fastGetAttribute(HTMLNames::inputmodeAttr).lower();
        return WebString();
    }
    if (isHTMLTextAreaElement(*element)) {
        const HTMLTextAreaElement& textarea = toHTMLTextAreaElement(*element);
        return textarea.fastGetAttribute(HTMLNames::inputmodeAttr).lower();
    }

    return WebString();
}

int WebFrameWidgetImpl::textInputFlags() const
{
    Element* element = focusedElement();
    if (!element)
        return WebTextInputFlagNone;

    DEFINE_STATIC_LOCAL(AtomicString, autocompleteString, ("autocomplete"));
    DEFINE_STATIC_LOCAL(AtomicString, autocorrectString, ("autocorrect"));
    int flags = 0;

    const AtomicString& autocomplete = element->getAttribute(autocompleteString);
    if (autocomplete == "on")
        flags |= WebTextInputFlagAutocompleteOn;
    else if (autocomplete == "off")
        flags |= WebTextInputFlagAutocompleteOff;

    const AtomicString& autocorrect = element->getAttribute(autocorrectString);
    if (autocorrect == "on")
        flags |= WebTextInputFlagAutocorrectOn;
    else if (autocorrect == "off")
        flags |= WebTextInputFlagAutocorrectOff;

    SpellcheckAttributeState spellcheck = element->spellcheckAttributeState();
    if (spellcheck == SpellcheckAttributeTrue)
        flags |= WebTextInputFlagSpellcheckOn;
    else if (spellcheck == SpellcheckAttributeFalse)
        flags |= WebTextInputFlagSpellcheckOff;

    if (isHTMLTextFormControlElement(element)) {
        HTMLTextFormControlElement* formElement = static_cast<HTMLTextFormControlElement*>(element);
        if (formElement->supportsAutocapitalize()) {
            DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
            DEFINE_STATIC_LOCAL(const AtomicString, characters, ("characters"));
            DEFINE_STATIC_LOCAL(const AtomicString, words, ("words"));
            DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));

            const AtomicString& autocapitalize = formElement->autocapitalize();
            if (autocapitalize == none)
                flags |= WebTextInputFlagAutocapitalizeNone;
            else if (autocapitalize == characters)
                flags |= WebTextInputFlagAutocapitalizeCharacters;
            else if (autocapitalize == words)
                flags |= WebTextInputFlagAutocapitalizeWords;
            else if (autocapitalize == sentences)
                flags |= WebTextInputFlagAutocapitalizeSentences;
            else
                NOTREACHED();
        }
    }

    return flags;
}

LocalFrame* WebFrameWidgetImpl::focusedLocalFrameAvailableForIme() const
{
    if (!m_imeAcceptEvents)
        return nullptr;
    return focusedLocalFrameInWidget();
}

} // namespace blink
