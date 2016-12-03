/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/page/scrolling/ScrollingCoordinator.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetList.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/Page.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerPositionConstraint.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebCache.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

class ScrollingCoordinatorTest : public testing::Test {
public:
    ScrollingCoordinatorTest()
        : m_baseURL("http://www.test.com/")
    {
        m_helper.initialize(true, nullptr, &m_mockWebViewClient, nullptr, &configureSettings);
        webViewImpl()->resize(IntSize(320, 240));

        // OSX attaches main frame scrollbars to the VisualViewport so the VisualViewport layers need
        // to be initialized.
        webViewImpl()->updateAllLifecyclePhases();
        WebFrameWidgetBase* mainFrameWidget = webViewImpl()->mainFrameImpl()->frameWidget();
        mainFrameWidget->setRootGraphicsLayer(
            webViewImpl()->mainFrameImpl()->frame()->view()->layoutViewItem().compositor()->rootGraphicsLayer());
    }

    ~ScrollingCoordinatorTest() override
    {
        Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
        WebCache::clear();
    }

    void navigateTo(const std::string& url)
    {
        FrameTestHelpers::loadFrame(webViewImpl()->mainFrame(), url);
    }

    void forceFullCompositingUpdate()
    {
        webViewImpl()->updateAllLifecyclePhases();
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

    WebLayer* getRootScrollLayer()
    {
        PaintLayerCompositor* compositor = frame()->contentLayoutItem().compositor();
        DCHECK(compositor);
        DCHECK(compositor->scrollLayer());

        WebLayer* webScrollLayer = compositor->scrollLayer()->platformLayer();
        return webScrollLayer;
    }

    WebViewImpl* webViewImpl() const { return m_helper.webView(); }
    LocalFrame* frame() const { return m_helper.webView()->mainFrameImpl()->frame(); }

    WebLayerTreeView* webLayerTreeView() const { return webViewImpl()->layerTreeView(); }

protected:
    std::string m_baseURL;
    FrameTestHelpers::TestWebViewClient m_mockWebViewClient;

private:
    static void configureSettings(WebSettings* settings)
    {
        settings->setJavaScriptEnabled(true);
        settings->setAcceleratedCompositingEnabled(true);
        settings->setPreferCompositingToLCDTextEnabled(true);
    }

    FrameTestHelpers::WebViewHelper m_helper;
};

TEST_F(ScrollingCoordinatorTest, fastScrollingByDefault)
{
    navigateTo("about:blank");
    forceFullCompositingUpdate();

    // Make sure the scrolling coordinator is active.
    FrameView* frameView = frame()->view();
    Page* page = frame()->page();
    ASSERT_TRUE(page->scrollingCoordinator());
    ASSERT_TRUE(page->scrollingCoordinator()->coordinatesScrollingForFrameView(frameView));

    // Fast scrolling should be enabled by default.
    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_TRUE(rootScrollLayer->scrollable());
    ASSERT_FALSE(rootScrollLayer->shouldScrollOnMainThread());
    ASSERT_EQ(WebEventListenerProperties::Nothing, webLayerTreeView()->eventListenerProperties(WebEventListenerClass::TouchStartOrMove));
    ASSERT_EQ(WebEventListenerProperties::Nothing, webLayerTreeView()->eventListenerProperties(WebEventListenerClass::MouseWheel));

    WebLayer* innerViewportScrollLayer = page->frameHost().visualViewport().scrollLayer()->platformLayer();
    ASSERT_TRUE(innerViewportScrollLayer->scrollable());
    ASSERT_FALSE(innerViewportScrollLayer->shouldScrollOnMainThread());
}

TEST_F(ScrollingCoordinatorTest, fastScrollingCanBeDisabledWithSetting)
{
    navigateTo("about:blank");
    webViewImpl()->settings()->setThreadedScrollingEnabled(false);
    forceFullCompositingUpdate();

    // Make sure the scrolling coordinator is active.
    FrameView* frameView = frame()->view();
    Page* page = frame()->page();
    ASSERT_TRUE(page->scrollingCoordinator());
    ASSERT_TRUE(page->scrollingCoordinator()->coordinatesScrollingForFrameView(frameView));

    // Main scrolling should be enabled with the setting override.
    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_TRUE(rootScrollLayer->scrollable());
    ASSERT_TRUE(rootScrollLayer->shouldScrollOnMainThread());

    // Main scrolling should also propagate to inner viewport layer.
    WebLayer* innerViewportScrollLayer = page->frameHost().visualViewport().scrollLayer()->platformLayer();
    ASSERT_TRUE(innerViewportScrollLayer->scrollable());
    ASSERT_TRUE(innerViewportScrollLayer->shouldScrollOnMainThread());
}


TEST_F(ScrollingCoordinatorTest, fastFractionalScrollingDiv)
{
    bool origFractionalOffsetsEnabled = RuntimeEnabledFeatures::fractionalScrollOffsetsEnabled();
    RuntimeEnabledFeatures::setFractionalScrollOffsetsEnabled(true);

    registerMockedHttpURLLoad("fractional-scroll-div.html");
    navigateTo(m_baseURL + "fractional-scroll-div.html");
    forceFullCompositingUpdate();

    Document* document = frame()->document();
    Element* scrollableElement = document->getElementById("scroller");
    DCHECK(scrollableElement);

    scrollableElement->setScrollTop(1.0);
    scrollableElement->setScrollLeft(1.0);
    forceFullCompositingUpdate();

    // Make sure the fractional scroll offset change 1.0 -> 1.2 gets propagated
    // to compositor.
    scrollableElement->setScrollTop(1.2);
    scrollableElement->setScrollLeft(1.2);
    forceFullCompositingUpdate();

    LayoutObject* layoutObject = scrollableElement->layoutObject();
    ASSERT_TRUE(layoutObject->isBox());
    LayoutBox* box = toLayoutBox(layoutObject);
    ASSERT_TRUE(box->usesCompositedScrolling());
    CompositedLayerMapping* compositedLayerMapping = box->layer()->compositedLayerMapping();
    ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
    DCHECK(compositedLayerMapping->scrollingContentsLayer());
    WebLayer* webScrollLayer = compositedLayerMapping->scrollingContentsLayer()->platformLayer();
    ASSERT_TRUE(webScrollLayer);
    ASSERT_NEAR(1.2, webScrollLayer->scrollPositionDouble().x, 0.01);
    ASSERT_NEAR(1.2, webScrollLayer->scrollPositionDouble().y, 0.01);

    RuntimeEnabledFeatures::setFractionalScrollOffsetsEnabled(origFractionalOffsetsEnabled);
}

static WebLayer* webLayerFromElement(Element* element)
{
    if (!element)
        return 0;
    LayoutObject* layoutObject = element->layoutObject();
    if (!layoutObject || !layoutObject->isBoxModelObject())
        return 0;
    PaintLayer* layer = toLayoutBoxModelObject(layoutObject)->layer();
    if (!layer)
        return 0;
    if (!layer->hasCompositedLayerMapping())
        return 0;
    CompositedLayerMapping* compositedLayerMapping = layer->compositedLayerMapping();
    GraphicsLayer* graphicsLayer = compositedLayerMapping->mainGraphicsLayer();
    if (!graphicsLayer)
        return 0;
    return graphicsLayer->platformLayer();
}

TEST_F(ScrollingCoordinatorTest, fastScrollingForFixedPosition)
{
    registerMockedHttpURLLoad("fixed-position.html");
    navigateTo(m_baseURL + "fixed-position.html");
    forceFullCompositingUpdate();

    // Fixed position should not fall back to main thread scrolling.
    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_FALSE(rootScrollLayer->shouldScrollOnMainThread());

    Document* document = frame()->document();
    {
        Element* element = document->getElementById("div-tl");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(!constraint.isFixedToRightEdge && !constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("div-tr");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(constraint.isFixedToRightEdge && !constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("div-bl");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(!constraint.isFixedToRightEdge && constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("div-br");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(constraint.isFixedToRightEdge && constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("span-tl");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(!constraint.isFixedToRightEdge && !constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("span-tr");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(constraint.isFixedToRightEdge && !constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("span-bl");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(!constraint.isFixedToRightEdge && constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("span-br");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(constraint.isFixedToRightEdge && constraint.isFixedToBottomEdge);
    }
}

TEST_F(ScrollingCoordinatorTest, touchEventHandler)
{
    registerMockedHttpURLLoad("touch-event-handler.html");
    navigateTo(m_baseURL + "touch-event-handler.html");
    forceFullCompositingUpdate();

    ASSERT_EQ(WebEventListenerProperties::Blocking, webLayerTreeView()->eventListenerProperties(WebEventListenerClass::TouchStartOrMove));
}

TEST_F(ScrollingCoordinatorTest, touchEventHandlerPassive)
{
    registerMockedHttpURLLoad("touch-event-handler-passive.html");
    navigateTo(m_baseURL + "touch-event-handler-passive.html");
    forceFullCompositingUpdate();

    ASSERT_EQ(WebEventListenerProperties::Passive, webLayerTreeView()->eventListenerProperties(WebEventListenerClass::TouchStartOrMove));
}

TEST_F(ScrollingCoordinatorTest, touchEventHandlerBoth)
{
    registerMockedHttpURLLoad("touch-event-handler-both.html");
    navigateTo(m_baseURL + "touch-event-handler-both.html");
    forceFullCompositingUpdate();

    ASSERT_EQ(WebEventListenerProperties::BlockingAndPassive, webLayerTreeView()->eventListenerProperties(WebEventListenerClass::TouchStartOrMove));
}

TEST_F(ScrollingCoordinatorTest, wheelEventHandler)
{
    registerMockedHttpURLLoad("wheel-event-handler.html");
    navigateTo(m_baseURL + "wheel-event-handler.html");
    forceFullCompositingUpdate();

    ASSERT_EQ(WebEventListenerProperties::Blocking, webLayerTreeView()->eventListenerProperties(WebEventListenerClass::MouseWheel));
}

TEST_F(ScrollingCoordinatorTest, wheelEventHandlerPassive)
{
    registerMockedHttpURLLoad("wheel-event-handler-passive.html");
    navigateTo(m_baseURL + "wheel-event-handler-passive.html");
    forceFullCompositingUpdate();

    ASSERT_EQ(WebEventListenerProperties::Passive, webLayerTreeView()->eventListenerProperties(WebEventListenerClass::MouseWheel));
}

TEST_F(ScrollingCoordinatorTest, wheelEventHandlerBoth)
{
    registerMockedHttpURLLoad("wheel-event-handler-both.html");
    navigateTo(m_baseURL + "wheel-event-handler-both.html");
    forceFullCompositingUpdate();

    ASSERT_EQ(WebEventListenerProperties::BlockingAndPassive, webLayerTreeView()->eventListenerProperties(WebEventListenerClass::MouseWheel));
}

TEST_F(ScrollingCoordinatorTest, scrollEventHandler)
{
    registerMockedHttpURLLoad("scroll-event-handler.html");
    navigateTo(m_baseURL + "scroll-event-handler.html");
    forceFullCompositingUpdate();

    ASSERT_TRUE(webLayerTreeView()->haveScrollEventHandlers());
}

TEST_F(ScrollingCoordinatorTest, updateEventHandlersDuringTeardown)
{
    registerMockedHttpURLLoad("scroll-event-handler-window.html");
    navigateTo(m_baseURL + "scroll-event-handler-window.html");
    forceFullCompositingUpdate();

    // Simulate detaching the document from its DOM window. This should not
    // cause a crash when the WebViewImpl is closed by the test runner.
    frame()->document()->detachLayoutTree();
}

TEST_F(ScrollingCoordinatorTest, clippedBodyTest)
{
    registerMockedHttpURLLoad("clipped-body.html");
    navigateTo(m_baseURL + "clipped-body.html");
    forceFullCompositingUpdate();

    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_EQ(0u, rootScrollLayer->nonFastScrollableRegion().size());
}

TEST_F(ScrollingCoordinatorTest, overflowScrolling)
{
    registerMockedHttpURLLoad("overflow-scrolling.html");
    navigateTo(m_baseURL + "overflow-scrolling.html");
    forceFullCompositingUpdate();

    // Verify the properties of the accelerated scrolling element starting from the LayoutObject
    // all the way to the WebLayer.
    Element* scrollableElement = frame()->document()->getElementById("scrollable");
    DCHECK(scrollableElement);

    LayoutObject* layoutObject = scrollableElement->layoutObject();
    ASSERT_TRUE(layoutObject->isBox());
    ASSERT_TRUE(layoutObject->hasLayer());

    LayoutBox* box = toLayoutBox(layoutObject);
    ASSERT_TRUE(box->usesCompositedScrolling());
    ASSERT_EQ(PaintsIntoOwnBacking, box->layer()->compositingState());

    CompositedLayerMapping* compositedLayerMapping = box->layer()->compositedLayerMapping();
    ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
    DCHECK(compositedLayerMapping->scrollingContentsLayer());

    GraphicsLayer* graphicsLayer = compositedLayerMapping->scrollingContentsLayer();
    ASSERT_EQ(box->layer()->getScrollableArea(), graphicsLayer->getScrollableArea());

    WebLayer* webScrollLayer = compositedLayerMapping->scrollingContentsLayer()->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());
    ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
    ASSERT_TRUE(webScrollLayer->userScrollableVertical());

#if OS(ANDROID)
    // Now verify we've attached impl-side scrollbars onto the scrollbar layers
    ASSERT_TRUE(compositedLayerMapping->layerForHorizontalScrollbar());
    ASSERT_TRUE(compositedLayerMapping->layerForHorizontalScrollbar()->hasContentsLayer());
    ASSERT_TRUE(compositedLayerMapping->layerForVerticalScrollbar());
    ASSERT_TRUE(compositedLayerMapping->layerForVerticalScrollbar()->hasContentsLayer());
#endif
}

TEST_F(ScrollingCoordinatorTest, overflowHidden)
{
    registerMockedHttpURLLoad("overflow-hidden.html");
    navigateTo(m_baseURL + "overflow-hidden.html");
    forceFullCompositingUpdate();

    // Verify the properties of the accelerated scrolling element starting from the LayoutObject
    // all the way to the WebLayer.
    Element* overflowElement = frame()->document()->getElementById("unscrollable-y");
    DCHECK(overflowElement);

    LayoutObject* layoutObject = overflowElement->layoutObject();
    ASSERT_TRUE(layoutObject->isBox());
    ASSERT_TRUE(layoutObject->hasLayer());

    LayoutBox* box = toLayoutBox(layoutObject);
    ASSERT_TRUE(box->usesCompositedScrolling());
    ASSERT_EQ(PaintsIntoOwnBacking, box->layer()->compositingState());

    CompositedLayerMapping* compositedLayerMapping = box->layer()->compositedLayerMapping();
    ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
    DCHECK(compositedLayerMapping->scrollingContentsLayer());

    GraphicsLayer* graphicsLayer = compositedLayerMapping->scrollingContentsLayer();
    ASSERT_EQ(box->layer()->getScrollableArea(), graphicsLayer->getScrollableArea());

    WebLayer* webScrollLayer = compositedLayerMapping->scrollingContentsLayer()->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());
    ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
    ASSERT_FALSE(webScrollLayer->userScrollableVertical());

    overflowElement = frame()->document()->getElementById("unscrollable-x");
    DCHECK(overflowElement);

    layoutObject = overflowElement->layoutObject();
    ASSERT_TRUE(layoutObject->isBox());
    ASSERT_TRUE(layoutObject->hasLayer());

    box = toLayoutBox(layoutObject);
    ASSERT_TRUE(box->getScrollableArea()->usesCompositedScrolling());
    ASSERT_EQ(PaintsIntoOwnBacking, box->layer()->compositingState());

    compositedLayerMapping = box->layer()->compositedLayerMapping();
    ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
    DCHECK(compositedLayerMapping->scrollingContentsLayer());

    graphicsLayer = compositedLayerMapping->scrollingContentsLayer();
    ASSERT_EQ(box->layer()->getScrollableArea(), graphicsLayer->getScrollableArea());

    webScrollLayer = compositedLayerMapping->scrollingContentsLayer()->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());
    ASSERT_FALSE(webScrollLayer->userScrollableHorizontal());
    ASSERT_TRUE(webScrollLayer->userScrollableVertical());
}

TEST_F(ScrollingCoordinatorTest, iframeScrolling)
{
    registerMockedHttpURLLoad("iframe-scrolling.html");
    registerMockedHttpURLLoad("iframe-scrolling-inner.html");
    navigateTo(m_baseURL + "iframe-scrolling.html");
    forceFullCompositingUpdate();

    // Verify the properties of the accelerated scrolling element starting from the LayoutObject
    // all the way to the WebLayer.
    Element* scrollableFrame = frame()->document()->getElementById("scrollable");
    ASSERT_TRUE(scrollableFrame);

    LayoutObject* layoutObject = scrollableFrame->layoutObject();
    ASSERT_TRUE(layoutObject);
    ASSERT_TRUE(layoutObject->isLayoutPart());

    LayoutPart* layoutPart = toLayoutPart(layoutObject);
    ASSERT_TRUE(layoutPart);
    ASSERT_TRUE(layoutPart->widget());
    ASSERT_TRUE(layoutPart->widget()->isFrameView());

    FrameView* innerFrameView = toFrameView(layoutPart->widget());
    LayoutViewItem innerLayoutViewItem = innerFrameView->layoutViewItem();
    ASSERT_FALSE(innerLayoutViewItem.isNull());

    PaintLayerCompositor* innerCompositor = innerLayoutViewItem.compositor();
    ASSERT_TRUE(innerCompositor->inCompositingMode());
    ASSERT_TRUE(innerCompositor->scrollLayer());

    GraphicsLayer* scrollLayer = innerCompositor->scrollLayer();
    ASSERT_EQ(innerFrameView, scrollLayer->getScrollableArea());

    WebLayer* webScrollLayer = scrollLayer->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());

#if OS(ANDROID)
    // Now verify we've attached impl-side scrollbars onto the scrollbar layers
    ASSERT_TRUE(innerCompositor->layerForHorizontalScrollbar());
    ASSERT_TRUE(innerCompositor->layerForHorizontalScrollbar()->hasContentsLayer());
    ASSERT_TRUE(innerCompositor->layerForVerticalScrollbar());
    ASSERT_TRUE(innerCompositor->layerForVerticalScrollbar()->hasContentsLayer());
#endif
}

TEST_F(ScrollingCoordinatorTest, rtlIframe)
{
    registerMockedHttpURLLoad("rtl-iframe.html");
    registerMockedHttpURLLoad("rtl-iframe-inner.html");
    navigateTo(m_baseURL + "rtl-iframe.html");
    forceFullCompositingUpdate();

    // Verify the properties of the accelerated scrolling element starting from the LayoutObject
    // all the way to the WebLayer.
    Element* scrollableFrame = frame()->document()->getElementById("scrollable");
    ASSERT_TRUE(scrollableFrame);

    LayoutObject* layoutObject = scrollableFrame->layoutObject();
    ASSERT_TRUE(layoutObject);
    ASSERT_TRUE(layoutObject->isLayoutPart());

    LayoutPart* layoutPart = toLayoutPart(layoutObject);
    ASSERT_TRUE(layoutPart);
    ASSERT_TRUE(layoutPart->widget());
    ASSERT_TRUE(layoutPart->widget()->isFrameView());

    FrameView* innerFrameView = toFrameView(layoutPart->widget());
    LayoutViewItem innerLayoutViewItem = innerFrameView->layoutViewItem();
    ASSERT_FALSE(innerLayoutViewItem.isNull());

    PaintLayerCompositor* innerCompositor = innerLayoutViewItem.compositor();
    ASSERT_TRUE(innerCompositor->inCompositingMode());
    ASSERT_TRUE(innerCompositor->scrollLayer());

    GraphicsLayer* scrollLayer = innerCompositor->scrollLayer();
    ASSERT_EQ(innerFrameView, scrollLayer->getScrollableArea());

    WebLayer* webScrollLayer = scrollLayer->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());

    int expectedScrollPosition = 958 + (innerFrameView->verticalScrollbar()->isOverlayScrollbar() ? 0 : 15);
    ASSERT_EQ(expectedScrollPosition, webScrollLayer->scrollPositionDouble().x);
}

TEST_F(ScrollingCoordinatorTest, setupScrollbarLayerShouldNotCrash)
{
    registerMockedHttpURLLoad("setup_scrollbar_layer_crash.html");
    navigateTo(m_baseURL + "setup_scrollbar_layer_crash.html");
    forceFullCompositingUpdate();
    // This test document setup an iframe with scrollbars, then switch to
    // an empty document by javascript.
}

TEST_F(ScrollingCoordinatorTest, scrollbarsForceMainThreadOrHaveWebScrollbarLayer)
{
    registerMockedHttpURLLoad("trivial-scroller.html");
    navigateTo(m_baseURL + "trivial-scroller.html");
    forceFullCompositingUpdate();

    Document* document = frame()->document();
    Element* scrollableElement = document->getElementById("scroller");
    DCHECK(scrollableElement);

    LayoutObject* layoutObject = scrollableElement->layoutObject();
    ASSERT_TRUE(layoutObject->isBox());
    LayoutBox* box = toLayoutBox(layoutObject);
    ASSERT_TRUE(box->usesCompositedScrolling());
    CompositedLayerMapping* compositedLayerMapping = box->layer()->compositedLayerMapping();
    GraphicsLayer* scrollbarGraphicsLayer = compositedLayerMapping->layerForVerticalScrollbar();
    ASSERT_TRUE(scrollbarGraphicsLayer);

    bool hasWebScrollbarLayer = !scrollbarGraphicsLayer->drawsContent();
    ASSERT_TRUE(hasWebScrollbarLayer || scrollbarGraphicsLayer->platformLayer()->shouldScrollOnMainThread());
}

#if OS(MACOSX) || OS(ANDROID)
TEST_F(ScrollingCoordinatorTest, DISABLED_setupScrollbarLayerShouldSetScrollLayerOpaque)
#else
TEST_F(ScrollingCoordinatorTest, setupScrollbarLayerShouldSetScrollLayerOpaque)
#endif
{
    registerMockedHttpURLLoad("wide_document.html");
    navigateTo(m_baseURL + "wide_document.html");
    forceFullCompositingUpdate();

    FrameView* frameView = frame()->view();
    ASSERT_TRUE(frameView);

    GraphicsLayer* scrollbarGraphicsLayer = frameView->layerForHorizontalScrollbar();
    ASSERT_TRUE(scrollbarGraphicsLayer);

    WebLayer* platformLayer = scrollbarGraphicsLayer->platformLayer();
    ASSERT_TRUE(platformLayer);

    WebLayer* contentsLayer = scrollbarGraphicsLayer->contentsLayer();
    ASSERT_TRUE(contentsLayer);

    // After scrollableAreaScrollbarLayerDidChange,
    // if the main frame's scrollbarLayer is opaque,
    // contentsLayer should be opaque too.
    ASSERT_EQ(platformLayer->opaque(), contentsLayer->opaque());
}

TEST_F(ScrollingCoordinatorTest, FixedPositionLosingBackingShouldTriggerMainThreadScroll)
{
    webViewImpl()->settings()->setPreferCompositingToLCDTextEnabled(false);
    registerMockedHttpURLLoad("fixed-position-losing-backing.html");
    navigateTo(m_baseURL + "fixed-position-losing-backing.html");
    forceFullCompositingUpdate();

    WebLayer* scrollLayer = frame()->page()->deprecatedLocalMainFrame()->view()->layerForScrolling()->platformLayer();
    Document* document = frame()->document();
    Element* fixedPos = document->getElementById("fixed");

    EXPECT_TRUE(static_cast<LayoutBoxModelObject*>(fixedPos->layoutObject())->layer()->hasCompositedLayerMapping());
    EXPECT_FALSE(scrollLayer->shouldScrollOnMainThread());

    fixedPos->setInlineStyleProperty(CSSPropertyTransform, CSSValueNone);
    forceFullCompositingUpdate();

    EXPECT_FALSE(static_cast<LayoutBoxModelObject*>(fixedPos->layoutObject())->layer()->hasCompositedLayerMapping());
    EXPECT_TRUE(scrollLayer->shouldScrollOnMainThread());
}

TEST_F(ScrollingCoordinatorTest, CustomScrollbarShouldTriggerMainThreadScroll)
{
    webViewImpl()->settings()->setPreferCompositingToLCDTextEnabled(true);
    webViewImpl()->setDeviceScaleFactor(2.f);
    registerMockedHttpURLLoad("custom_scrollbar.html");
    navigateTo(m_baseURL + "custom_scrollbar.html");
    forceFullCompositingUpdate();

    Document* document = frame()->document();
    Element* container = document->getElementById("container");
    Element* content = document->getElementById("content");
    DCHECK_EQ(container->getAttribute(HTMLNames::classAttr), "custom_scrollbar");
    DCHECK(container);
    DCHECK(content);

    LayoutObject* layoutObject = container->layoutObject();
    ASSERT_TRUE(layoutObject->isBox());
    LayoutBox* box = toLayoutBox(layoutObject);
    ASSERT_TRUE(box->usesCompositedScrolling());
    CompositedLayerMapping* compositedLayerMapping = box->layer()->compositedLayerMapping();
    GraphicsLayer* scrollbarGraphicsLayer = compositedLayerMapping->layerForVerticalScrollbar();
    ASSERT_TRUE(scrollbarGraphicsLayer);
    ASSERT_TRUE(scrollbarGraphicsLayer->platformLayer()->shouldScrollOnMainThread());
    ASSERT_TRUE(scrollbarGraphicsLayer->platformLayer()->mainThreadScrollingReasons() & MainThreadScrollingReason::kCustomScrollbarScrolling);

    // remove custom scrollbar class, the scrollbar is expected to scroll on
    // impl thread as it is an overlay scrollbar.
    container->removeAttribute("class");
    forceFullCompositingUpdate();
    scrollbarGraphicsLayer = compositedLayerMapping->layerForVerticalScrollbar();
    ASSERT_FALSE(scrollbarGraphicsLayer->platformLayer()->shouldScrollOnMainThread());
    ASSERT_FALSE(scrollbarGraphicsLayer->platformLayer()->mainThreadScrollingReasons() & MainThreadScrollingReason::kCustomScrollbarScrolling);
}

} // namespace blink
