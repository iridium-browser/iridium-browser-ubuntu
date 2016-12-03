// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/TopControls.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/page/Page.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebCache.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/Vector.h"

using blink::testing::runPendingTasks;
using testing::Mock;

namespace blink {

namespace {

class RootScrollerTestWebViewClient : public FrameTestHelpers::TestWebViewClient {
public:
    MOCK_METHOD4(didOverscroll, void(const WebFloatSize&, const WebFloatSize&, const WebFloatPoint&, const WebFloatSize&));
};

class RootScrollerTest : public ::testing::Test {
public:
    RootScrollerTest()
        : m_baseURL("http://www.test.com/")
    {
        registerMockedHttpURLLoad("overflow-scrolling.html");
        registerMockedHttpURLLoad("root-scroller.html");
        registerMockedHttpURLLoad("root-scroller-iframe.html");
        registerMockedHttpURLLoad("root-scroller-child.html");
    }

    ~RootScrollerTest() override
    {
        m_featuresBackup.restore();
        Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
        WebCache::clear();
    }

    WebViewImpl* initialize(const std::string& pageName)
    {
        RuntimeEnabledFeatures::setSetRootScrollerEnabled(true);

        // Load a page with large body and set viewport size to 400x400 to
        // ensure main frame is scrollable.
        m_helper.initializeAndLoad(
            m_baseURL + pageName, true, nullptr, &m_client, nullptr,
            &configureSettings);

        // Initialize top controls to be shown.
        webViewImpl()->resizeWithTopControls(IntSize(400, 400), 50, true);
        webViewImpl()->topControls().setShownRatio(1);

        mainFrameView()->updateAllLifecyclePhases();

        return webViewImpl();
    }

    static void configureSettings(WebSettings* settings)
    {
        settings->setJavaScriptEnabled(true);
        settings->setAcceleratedCompositingEnabled(true);
        settings->setPreferCompositingToLCDTextEnabled(true);
        // Android settings.
        settings->setViewportEnabled(true);
        settings->setViewportMetaEnabled(true);
        settings->setShrinksViewportContentToFit(true);
        settings->setMainFrameResizesAreOrientationChanges(true);
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(
            WebString::fromUTF8(m_baseURL.c_str()),
            WebString::fromUTF8(fileName.c_str()));
    }

    void executeScript(const WebString& code)
    {
        mainWebFrame()->executeScript(WebScriptSource(code));
        mainWebFrame()->view()->updateAllLifecyclePhases();
        runPendingTasks();
    }

    WebGestureEvent generateEvent(
        WebInputEvent::Type type, int deltaX = 0, int deltaY = 0)
    {
        WebGestureEvent event;
        event.type = type;
        event.sourceDevice = WebGestureDeviceTouchscreen;
        event.x = 100;
        event.y = 100;
        if (type == WebInputEvent::GestureScrollUpdate) {
            event.data.scrollUpdate.deltaX = deltaX;
            event.data.scrollUpdate.deltaY = deltaY;
        }
        return event;
    }

    void verticalScroll(float deltaY)
    {
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollBegin));
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollUpdate, 0, -deltaY));
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollEnd));
    }

    WebViewImpl* webViewImpl() const
    {
        return m_helper.webView();
    }

    FrameHost& frameHost() const
    {
        return m_helper.webView()->page()->frameHost();
    }

    LocalFrame* mainFrame() const
    {
        return webViewImpl()->mainFrameImpl()->frame();
    }

    WebLocalFrame* mainWebFrame() const
    {
        return webViewImpl()->mainFrameImpl();
    }

    FrameView* mainFrameView() const
    {
        return webViewImpl()->mainFrameImpl()->frame()->view();
    }

    VisualViewport& visualViewport() const
    {
        return frameHost().visualViewport();
    }

    TopControls& topControls() const
    {
        return frameHost().topControls();
    }

    Element* effectiveRootScroller(Document* doc) const
    {
        return doc->rootScrollerController()->effectiveRootScroller();
    }

protected:
    std::string m_baseURL;
    RootScrollerTestWebViewClient m_client;
    FrameTestHelpers::WebViewHelper m_helper;
    RuntimeEnabledFeatures::Backup m_featuresBackup;
};

// Test that no root scroller element is set if setRootScroller isn't called on
// any elements. The document element should be the default effective root
// scroller.
TEST_F(RootScrollerTest, TestDefaultRootScroller)
{
    initialize("overflow-scrolling.html");

    ASSERT_EQ(nullptr, mainFrame()->document()->rootScroller());

    Element* htmlElement = mainFrame()->document()->documentElement();
    EXPECT_EQ(htmlElement, effectiveRootScroller(mainFrame()->document()));
}

// Tests that setting an element as the root scroller causes it to control url
// bar hiding and overscroll.
TEST_F(RootScrollerTest, TestSetRootScroller)
{
    initialize("root-scroller.html");

    Element* container = mainFrame()->document()->getElementById("container");
    TrackExceptionState exceptionState;
    mainFrame()->document()->setRootScroller(container, exceptionState);
    ASSERT_EQ(container, mainFrame()->document()->rootScroller());

    // Content is 1000x1000, WebView size is 400x400 so max scroll is 600px.
    double maximumScroll = 600;

    webViewImpl()->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollBegin));

    {
        // Scrolling over the #container DIV should cause the top controls to
        // hide.
        ASSERT_FLOAT_EQ(1, topControls().shownRatio());
        webViewImpl()->handleInputEvent(generateEvent(
            WebInputEvent::GestureScrollUpdate, 0, -topControls().height()));
        ASSERT_FLOAT_EQ(0, topControls().shownRatio());
    }

    {
        // Make sure we're actually scrolling the DIV and not the FrameView.
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollUpdate, 0, -100));
        ASSERT_FLOAT_EQ(100, container->scrollTop());
        ASSERT_FLOAT_EQ(0, mainFrameView()->scrollPositionDouble().y());
    }

    {
        // Scroll 50 pixels past the end. Ensure we report the 50 pixels as
        // overscroll.
        EXPECT_CALL(m_client,
            didOverscroll(
                WebFloatSize(0, 50),
                WebFloatSize(0, 50),
                WebFloatPoint(100, 100),
                WebFloatSize()));
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollUpdate, 0, -550));
        ASSERT_FLOAT_EQ(maximumScroll, container->scrollTop());
        ASSERT_FLOAT_EQ(0, mainFrameView()->scrollPositionDouble().y());
        Mock::VerifyAndClearExpectations(&m_client);
    }

    {
        // Continue the gesture overscroll.
        EXPECT_CALL(m_client,
            didOverscroll(
                WebFloatSize(0, 20),
                WebFloatSize(0, 70),
                WebFloatPoint(100, 100),
                WebFloatSize()));
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollUpdate, 0, -20));
        ASSERT_FLOAT_EQ(maximumScroll, container->scrollTop());
        ASSERT_FLOAT_EQ(0, mainFrameView()->scrollPositionDouble().y());
        Mock::VerifyAndClearExpectations(&m_client);
    }

    webViewImpl()->handleInputEvent(
        generateEvent(WebInputEvent::GestureScrollEnd));

    {
        // Make sure a new gesture scroll still won't scroll the frameview and
        // overscrolls.
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollBegin));

        EXPECT_CALL(m_client,
            didOverscroll(
                WebFloatSize(0, 30),
                WebFloatSize(0, 30),
                WebFloatPoint(100, 100),
                WebFloatSize()));
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollUpdate, 0, -30));
        ASSERT_FLOAT_EQ(maximumScroll, container->scrollTop());
        ASSERT_FLOAT_EQ(0, mainFrameView()->scrollPositionDouble().y());
        Mock::VerifyAndClearExpectations(&m_client);

        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollEnd));
    }

    {
        // Scrolling up should show the top controls.
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollBegin));

        ASSERT_FLOAT_EQ(0, topControls().shownRatio());
        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollUpdate, 0, 30));
        ASSERT_FLOAT_EQ(0.6, topControls().shownRatio());

        webViewImpl()->handleInputEvent(
            generateEvent(WebInputEvent::GestureScrollEnd));
    }
}

// Tests that removing the element that is the root scroller from the DOM tree
// doesn't remove it as the root scroller but it does change the effective root
// scroller.
TEST_F(RootScrollerTest, TestRemoveRootScrollerFromDom)
{
    initialize("root-scroller.html");

    ASSERT_EQ(nullptr, mainFrame()->document()->rootScroller());

    Element* container = mainFrame()->document()->getElementById("container");
    TrackExceptionState exceptionState;
    mainFrame()->document()->setRootScroller(container, exceptionState);

    ASSERT_EQ(container, mainFrame()->document()->rootScroller());
    ASSERT_EQ(container, effectiveRootScroller(mainFrame()->document()));

    mainFrame()->document()->body()->removeChild(container);
    mainFrameView()->updateAllLifecyclePhases();

    ASSERT_EQ(container, mainFrame()->document()->rootScroller());
    ASSERT_NE(container, effectiveRootScroller(mainFrame()->document()));
}

// Tests that setting an element that isn't a valid scroller as the root
// scroller doesn't change the effective root scroller.
TEST_F(RootScrollerTest, TestSetRootScrollerOnInvalidElement)
{
    initialize("root-scroller.html");

    {
        // Set to a non-block element. Should be rejected and a console message
        // logged.
        Element* element = mainFrame()->document()->getElementById("nonBlock");
        TrackExceptionState exceptionState;
        mainFrame()->document()->setRootScroller(element, exceptionState);
        mainFrameView()->updateAllLifecyclePhases();
        ASSERT_EQ(element, mainFrame()->document()->rootScroller());
        ASSERT_NE(element, effectiveRootScroller(mainFrame()->document()));
    }

    {
        // Set to an element with no size.
        Element* element = mainFrame()->document()->getElementById("empty");
        TrackExceptionState exceptionState;
        mainFrame()->document()->setRootScroller(element, exceptionState);
        mainFrameView()->updateAllLifecyclePhases();
        ASSERT_EQ(element, mainFrame()->document()->rootScroller());
        ASSERT_NE(element, effectiveRootScroller(mainFrame()->document()));
    }
}

// Test that the effective root scroller resets to the default element when the
// current root scroller element becomes invalid as a scroller.
TEST_F(RootScrollerTest, TestRootScrollerBecomesInvalid)
{
    initialize("root-scroller.html");

    Element* htmlElement = mainFrame()->document()->documentElement();
    Element* container = mainFrame()->document()->getElementById("container");
    TrackExceptionState exceptionState;

    ASSERT_EQ(nullptr, mainFrame()->document()->rootScroller());
    ASSERT_EQ(htmlElement, effectiveRootScroller(mainFrame()->document()));

    {
        mainFrame()->document()->setRootScroller(container, exceptionState);
        mainFrameView()->updateAllLifecyclePhases();

        ASSERT_EQ(container, mainFrame()->document()->rootScroller());
        ASSERT_EQ(container, effectiveRootScroller(mainFrame()->document()));

        executeScript(
            "document.querySelector('#container').style.display = 'inline'");
        mainFrameView()->updateAllLifecyclePhases();

        ASSERT_EQ(container, mainFrame()->document()->rootScroller());
        ASSERT_EQ(htmlElement,
            effectiveRootScroller(mainFrame()->document()));
    }

    executeScript(
        "document.querySelector('#container').style.display = 'block'");
    mainFrame()->document()->setRootScroller(nullptr, exceptionState);
    mainFrameView()->updateAllLifecyclePhases();
    ASSERT_EQ(nullptr, mainFrame()->document()->rootScroller());
    ASSERT_EQ(htmlElement, effectiveRootScroller(mainFrame()->document()));

    {
        mainFrame()->document()->setRootScroller(container, exceptionState);
        mainFrameView()->updateAllLifecyclePhases();

        ASSERT_EQ(container, mainFrame()->document()->rootScroller());
        ASSERT_EQ(container, effectiveRootScroller(mainFrame()->document()));

        executeScript(
            "document.querySelector('#container').style.width = '98%'");
        mainFrameView()->updateAllLifecyclePhases();

        ASSERT_EQ(container, mainFrame()->document()->rootScroller());
        ASSERT_EQ(htmlElement,
            effectiveRootScroller(mainFrame()->document()));
    }
}

// Tests that setting the root scroller of the top document to an element that
// belongs to a nested document works.
TEST_F(RootScrollerTest, TestSetRootScrollerOnElementInIframe)
{
    initialize("root-scroller-iframe.html");

    ASSERT_EQ(nullptr, mainFrame()->document()->rootScroller());

    {
        // Trying to set an element from a nested document should fail.
        HTMLFrameOwnerElement* iframe = toHTMLFrameOwnerElement(
            mainFrame()->document()->getElementById("iframe"));
        Element* innerContainer =
            iframe->contentDocument()->getElementById("container");

        TrackExceptionState exceptionState;
        mainFrame()->document()->setRootScroller(
            innerContainer,
            exceptionState);
        mainFrameView()->updateAllLifecyclePhases();

        ASSERT_EQ(innerContainer, mainFrame()->document()->rootScroller());
        ASSERT_EQ(innerContainer,
            effectiveRootScroller(mainFrame()->document()));
    }

    {
        // Setting the iframe itself should also work.
        HTMLFrameOwnerElement* iframe = toHTMLFrameOwnerElement(
            mainFrame()->document()->getElementById("iframe"));

        TrackExceptionState exceptionState;
        mainFrame()->document()->setRootScroller(iframe, exceptionState);
        mainFrameView()->updateAllLifecyclePhases();

        ASSERT_EQ(iframe, mainFrame()->document()->rootScroller());
        ASSERT_EQ(iframe, effectiveRootScroller(mainFrame()->document()));
    }
}

// Tests that setting a valid element as the root scroller on a document within
// an iframe works as expected.
TEST_F(RootScrollerTest, TestRootScrollerWithinIframe)
{
    initialize("root-scroller-iframe.html");

    ASSERT_EQ(nullptr, mainFrame()->document()->rootScroller());

    {
        HTMLFrameOwnerElement* iframe = toHTMLFrameOwnerElement(
            mainFrame()->document()->getElementById("iframe"));

        ASSERT_EQ(iframe->contentDocument()->documentElement(),
            effectiveRootScroller(iframe->contentDocument()));

        Element* innerContainer =
            iframe->contentDocument()->getElementById("container");
        TrackExceptionState exceptionState;
        iframe->contentDocument()->setRootScroller(
            innerContainer,
            exceptionState);
        mainFrameView()->updateAllLifecyclePhases();

        ASSERT_EQ(innerContainer, iframe->contentDocument()->rootScroller());
        ASSERT_EQ(innerContainer,
            effectiveRootScroller(iframe->contentDocument()));
    }
}

// Tests that trying to set an element as the root scroller of a document inside
// an iframe fails when that element belongs to the parent document.
// TODO(bokan): Recent changes mean this is now possible but should be fixed.
TEST_F(RootScrollerTest, DISABLED_TestSetRootScrollerOnElementFromOutsideIframe)
{
    initialize("root-scroller-iframe.html");

    ASSERT_EQ(nullptr, mainFrame()->document()->rootScroller());
    {
        // Try to set the the root scroller of the child document to be the
        // <iframe> element in the parent document.
        HTMLFrameOwnerElement* iframe = toHTMLFrameOwnerElement(
            mainFrame()->document()->getElementById("iframe"));
        NonThrowableExceptionState nonThrow;
        Element* body =
            mainFrame()->document()->querySelector("body", nonThrow);

        ASSERT_EQ(nullptr, iframe->contentDocument()->rootScroller());

        TrackExceptionState exceptionState;
        iframe->contentDocument()->setRootScroller(
            iframe,
            exceptionState);

        ASSERT_EQ(iframe, iframe->contentDocument()->rootScroller());

        // Try to set the root scroller of the child document to be the
        // <body> element of the parent document.
        iframe->contentDocument()->setRootScroller(
            body,
            exceptionState);

        ASSERT_EQ(body, iframe->contentDocument()->rootScroller());
    }
}

} // namespace

} // namespace blink
