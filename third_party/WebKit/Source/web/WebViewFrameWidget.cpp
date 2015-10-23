// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "config.h"
#include "web/WebViewFrameWidget.h"

#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

WebViewFrameWidget::WebViewFrameWidget(WebViewImpl& webView) : m_webView(&webView)
{
    m_webView->mainFrameImpl()->setFrameWidget(this);
}

WebViewFrameWidget::~WebViewFrameWidget()
{
}

void WebViewFrameWidget::close()
{
    m_webView->mainFrameImpl()->setFrameWidget(nullptr);
    m_webView = nullptr;

    // Note: this intentionally does not forward to WebView::close(), to make it
    // easier to untangle the cleanup logic later.

    delete this;
}

WebSize WebViewFrameWidget::size()
{
    return m_webView->size();
}

void WebViewFrameWidget::willStartLiveResize()
{
    return m_webView->willStartLiveResize();
}

void WebViewFrameWidget::resize(const WebSize& size)
{
    return m_webView->resize(size);
}

void WebViewFrameWidget::resizePinchViewport(const WebSize& size)
{
    return m_webView->resizePinchViewport(size);
}

void WebViewFrameWidget::willEndLiveResize()
{
    return m_webView->willEndLiveResize();
}

void WebViewFrameWidget::didEnterFullScreen()
{
    return m_webView->didEnterFullScreen();
}

void WebViewFrameWidget::didExitFullScreen()
{
    return m_webView->didExitFullScreen();
}

void WebViewFrameWidget::beginFrame(const WebBeginFrameArgs& frameTime)
{
    return m_webView->beginFrame(frameTime);
}

void WebViewFrameWidget::layout()
{
    return m_webView->layout();
}

void WebViewFrameWidget::paint(WebCanvas* canvas, const WebRect& viewPort)
{
    return m_webView->paint(canvas, viewPort);
}

void WebViewFrameWidget::paintCompositedDeprecated(WebCanvas* canvas, const WebRect& viewPort)
{
    return m_webView->paintCompositedDeprecated(canvas, viewPort);
}

void WebViewFrameWidget::layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback* callback)
{
    return m_webView->layoutAndPaintAsync(callback);
}

void WebViewFrameWidget::compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback* callback)
{
    return m_webView->compositeAndReadbackAsync(callback);
}

void WebViewFrameWidget::themeChanged()
{
    return m_webView->themeChanged();
}

bool WebViewFrameWidget::handleInputEvent(const WebInputEvent& event)
{
    return m_webView->handleInputEvent(event);
}

void WebViewFrameWidget::setCursorVisibilityState(bool isVisible)
{
    return m_webView->setCursorVisibilityState(isVisible);
}

bool WebViewFrameWidget::hasTouchEventHandlersAt(const WebPoint& point)
{
    return m_webView->hasTouchEventHandlersAt(point);
}

void WebViewFrameWidget::applyViewportDeltas(
    const WebFloatSize& pinchViewportDelta,
    const WebFloatSize& layoutViewportDelta,
    const WebFloatSize& elasticOverscrollDelta,
    float scaleFactor,
    float topControlsShownRatioDelta)
{
    return m_webView->applyViewportDeltas(pinchViewportDelta, layoutViewportDelta, elasticOverscrollDelta, scaleFactor, topControlsShownRatioDelta);
}

void WebViewFrameWidget::recordFrameTimingEvent(FrameTimingEventType eventType, int64_t rectId, const WebVector<WebFrameTimingEvent>& events)
{
    return m_webView->recordFrameTimingEvent(eventType, rectId, events);
}

void WebViewFrameWidget::mouseCaptureLost()
{
    return m_webView->mouseCaptureLost();
}

void WebViewFrameWidget::setFocus(bool enable)
{
    return m_webView->setFocus(enable);
}

bool WebViewFrameWidget::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd)
{
    return m_webView->setComposition(text, underlines, selectionStart, selectionEnd);
}

bool WebViewFrameWidget::confirmComposition()
{
    return m_webView->confirmComposition();
}

bool WebViewFrameWidget::confirmComposition(ConfirmCompositionBehavior selectionBehavior)
{
    return m_webView->confirmComposition(selectionBehavior);
}

bool WebViewFrameWidget::confirmComposition(const WebString& text)
{
    return m_webView->confirmComposition(text);
}

bool WebViewFrameWidget::compositionRange(size_t* location, size_t* length)
{
    return m_webView->compositionRange(location, length);
}

WebTextInputInfo WebViewFrameWidget::textInputInfo()
{
    return m_webView->textInputInfo();
}

bool WebViewFrameWidget::selectionBounds(WebRect& anchor, WebRect& focus) const
{
    return m_webView->selectionBounds(anchor, focus);
}

bool WebViewFrameWidget::selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const
{
    return m_webView->selectionTextDirection(start, end);
}

bool WebViewFrameWidget::isSelectionAnchorFirst() const
{
    return m_webView->isSelectionAnchorFirst();
}

bool WebViewFrameWidget::caretOrSelectionRange(size_t* location, size_t* length)
{
    return m_webView->caretOrSelectionRange(location, length);
}

void WebViewFrameWidget::setTextDirection(WebTextDirection direction)
{
    return m_webView->setTextDirection(direction);
}

bool WebViewFrameWidget::isAcceleratedCompositingActive() const
{
    return m_webView->isAcceleratedCompositingActive();
}

void WebViewFrameWidget::willCloseLayerTreeView()
{
    return m_webView->willCloseLayerTreeView();
}

void WebViewFrameWidget::didAcquirePointerLock()
{
    return m_webView->didAcquirePointerLock();
}

void WebViewFrameWidget::didNotAcquirePointerLock()
{
    return m_webView->didNotAcquirePointerLock();
}

void WebViewFrameWidget::didLosePointerLock()
{
    return m_webView->didLosePointerLock();
}

void WebViewFrameWidget::didChangeWindowResizerRect()
{
    return m_webView->didChangeWindowResizerRect();
}

WebColor WebViewFrameWidget::backgroundColor() const
{
    return m_webView->backgroundColor();
}

WebPagePopup* WebViewFrameWidget::pagePopup() const
{
    return m_webView->pagePopup();
}

void WebViewFrameWidget::setTopControlsHeight(float height, bool topControlsShrinkLayoutSize)
{
    return m_webView->setTopControlsHeight(height, topControlsShrinkLayoutSize);
}

void WebViewFrameWidget::updateTopControlsState(WebTopControlsState constraints, WebTopControlsState current, bool animate)
{
    return m_webView->updateTopControlsState(constraints, current, animate);
}

void WebViewFrameWidget::setVisibilityState(WebPageVisibilityState visibilityState, bool isInitialState)
{
    return m_webView->setVisibilityState(visibilityState, isInitialState);
}

} // namespace blink
