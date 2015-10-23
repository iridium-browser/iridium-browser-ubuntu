// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef WebViewFrameWidget_h
#define WebViewFrameWidget_h

#include "public/web/WebFrameWidget.h"
#include "wtf/Noncopyable.h"

namespace blink {

class WebViewImpl;

// Shim class to help normalize the widget interfaces in the Blink public API.
// For OOPI, subframes have WebFrameWidgets for input and rendering.
// Unfortunately, the main frame still uses WebView's WebWidget for input and
// rendering. This results in complex code, since there are two different
// implementations of WebWidget and code needs to have branches to handle both
// cases.
// This class allows a Blink embedder to create a WebFrameWidget that can be
// used for the main frame. Internally, it currently wraps WebView's WebWidget
// and just forwards almost everything to it.
// After the embedder starts using a WebFrameWidget for the main frame,
// WebView will be updated to no longer inherit WebWidget. The eventual goal is
// to unfork the widget code duplicated in WebFrameWidgetImpl and WebViewImpl
// into one class.
// A more detailed writeup of this transition can be read at
// https://goo.gl/7yVrnb.
class WebViewFrameWidget : public WebFrameWidget {
    WTF_MAKE_NONCOPYABLE(WebViewFrameWidget);
public:
    explicit WebViewFrameWidget(WebViewImpl&);
    virtual ~WebViewFrameWidget();

    // WebFrameWidget overrides:
    void close() override;
    WebSize size() override;
    void willStartLiveResize() override;
    void resize(const WebSize&) override;
    void resizePinchViewport(const WebSize&) override;
    void willEndLiveResize() override;
    void didEnterFullScreen() override;
    void didExitFullScreen() override;
    void beginFrame(const WebBeginFrameArgs& frameTime) override;
    void layout() override;
    void paint(WebCanvas*, const WebRect& viewPort) override;
    void paintCompositedDeprecated(WebCanvas*, const WebRect&) override;
    void layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override;
    void compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback*) override;
    void themeChanged() override;
    bool handleInputEvent(const WebInputEvent&) override;
    void setCursorVisibilityState(bool isVisible) override;
    bool hasTouchEventHandlersAt(const WebPoint&) override;
    void applyViewportDeltas(
        const WebFloatSize& pinchViewportDelta,
        const WebFloatSize& layoutViewportDelta,
        const WebFloatSize& elasticOverscrollDelta,
        float scaleFactor,
        float topControlsShownRatioDelta) override;
    void recordFrameTimingEvent(FrameTimingEventType, int64_t rectId, const WebVector<WebFrameTimingEvent>& events) override;
    void mouseCaptureLost() override;
    void setFocus(bool) override;
    bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart,
        int selectionEnd) override;
    bool confirmComposition() override;
    bool confirmComposition(ConfirmCompositionBehavior selectionBehavior) override;
    bool confirmComposition(const WebString& text) override;
    bool compositionRange(size_t* location, size_t* length) override;
    WebTextInputInfo textInputInfo() override;
    bool selectionBounds(WebRect& anchor, WebRect& focus) const override;
    bool selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const override;
    bool isSelectionAnchorFirst() const override;
    bool caretOrSelectionRange(size_t* location, size_t* length) override;
    void setTextDirection(WebTextDirection) override;
    bool isAcceleratedCompositingActive() const override;
    bool isWebView() const override { return false; }
    bool isPagePopup() const override { return false; }
    void willCloseLayerTreeView() override;
    void didAcquirePointerLock() override;
    void didNotAcquirePointerLock() override;
    void didLosePointerLock() override;
    void didChangeWindowResizerRect() override;
    WebColor backgroundColor() const override;
    WebPagePopup* pagePopup() const override;
    void setTopControlsHeight(float height, bool topControlsShrinkLayoutSize) override;
    void updateTopControlsState(WebTopControlsState constraints, WebTopControlsState current, bool animate) override;
    void setVisibilityState(WebPageVisibilityState, bool isInitialState) override;
    bool forSubframe() const { return false; }

private:
    WebViewImpl* m_webView;
};

} // namespace blink

#endif // WebViewFrameWidget_h
