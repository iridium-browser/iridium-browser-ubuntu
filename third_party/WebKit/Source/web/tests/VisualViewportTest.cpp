// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/VisualViewport.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/TopControls.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/Page.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/geometry/DoublePoint.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebCache.h"
#include "public/web/WebContextMenuData.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

#include <string>

#define ASSERT_POINT_EQ(expected, actual) \
    do { \
        ASSERT_EQ((expected).x(), (actual).x()); \
        ASSERT_EQ((expected).y(), (actual).y()); \
    } while (false)

#define EXPECT_POINT_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).x(), (actual).x()); \
        EXPECT_EQ((expected).y(), (actual).y()); \
    } while (false)

#define EXPECT_FLOAT_POINT_EQ(expected, actual) \
    do { \
        EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
        EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
    } while (false)

#define EXPECT_POINT_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).x(), (actual).x()); \
        EXPECT_EQ((expected).y(), (actual).y()); \
    } while (false)

#define EXPECT_SIZE_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).width(), (actual).width()); \
        EXPECT_EQ((expected).height(), (actual).height()); \
    } while (false)

#define EXPECT_FLOAT_SIZE_EQ(expected, actual) \
    do { \
        EXPECT_FLOAT_EQ((expected).width(), (actual).width()); \
        EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
    } while (false)

#define EXPECT_FLOAT_RECT_EQ(expected, actual) \
    do { \
        EXPECT_FLOAT_EQ((expected).x(), (actual).x()); \
        EXPECT_FLOAT_EQ((expected).y(), (actual).y()); \
        EXPECT_FLOAT_EQ((expected).width(), (actual).width()); \
        EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
    } while (false)


using namespace blink;

using ::testing::_;
using ::testing::PrintToString;
using ::testing::Mock;

namespace blink {
::std::ostream& operator<<(::std::ostream& os, const WebContextMenuData& data)
{
    return os << "Context menu location: ["
        << data.mousePosition.x << ", " << data.mousePosition.y << "]";
}
}


namespace {

class VisualViewportTest
    : public testing::Test
    , public FrameTestHelpers::SettingOverrider {
public:
    VisualViewportTest()
        : m_baseURL("http://www.test.com/")
        , m_helper(this)
    {
    }

    void overrideSettings(WebSettings *settings) override
    {
    }

    void initializeWithDesktopSettings(void (*overrideSettingsFunc)(WebSettings*) = 0)
    {
        if (!overrideSettingsFunc)
            overrideSettingsFunc = &configureSettings;
        m_helper.initialize(true, nullptr, &m_mockWebViewClient, nullptr, overrideSettingsFunc);
        webViewImpl()->setDefaultPageScaleLimits(1, 4);
    }

    void initializeWithAndroidSettings(void (*overrideSettingsFunc)(WebSettings*) = 0)
    {
        if (!overrideSettingsFunc)
            overrideSettingsFunc = &configureAndroidSettings;
        m_helper.initialize(true, nullptr, &m_mockWebViewClient, nullptr, overrideSettingsFunc);
        webViewImpl()->setDefaultPageScaleLimits(0.25f, 5);
    }

    ~VisualViewportTest() override
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

    static void configureSettings(WebSettings* settings)
    {
        settings->setJavaScriptEnabled(true);
        settings->setPreferCompositingToLCDTextEnabled(true);
    }

    static void configureAndroidSettings(WebSettings* settings)
    {
        configureSettings(settings);
        settings->setViewportEnabled(true);
        settings->setViewportMetaEnabled(true);
        settings->setShrinksViewportContentToFit(true);
        settings->setMainFrameResizesAreOrientationChanges(true);
    }

protected:
    std::string m_baseURL;
    FrameTestHelpers::TestWebViewClient m_mockWebViewClient;

private:
    FrameTestHelpers::WebViewHelper m_helper;
};

class ParameterizedVisualViewportTest
    : public VisualViewportTest
    , public testing::WithParamInterface<FrameTestHelpers::SettingOverrideFunction> {
public:
    void overrideSettings(WebSettings *settings) override
    {
        GetParam()(settings);
    }
};

INSTANTIATE_TEST_CASE_P(All, ParameterizedVisualViewportTest, ::testing::Values(
    FrameTestHelpers::DefaultSettingOverride,
    FrameTestHelpers::RootLayerScrollsSettingOverride));

// Test that resizing the VisualViewport works as expected and that resizing the
// WebView resizes the VisualViewport.
TEST_P(ParameterizedVisualViewportTest, TestResize)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    navigateTo("about:blank");
    forceFullCompositingUpdate();

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();

    IntSize webViewSize = webViewImpl()->size();

    // Make sure the visual viewport was initialized.
    EXPECT_SIZE_EQ(webViewSize, visualViewport.size());

    // Resizing the WebView should change the VisualViewport.
    webViewSize = IntSize(640, 480);
    webViewImpl()->resize(webViewSize);
    EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(webViewSize, visualViewport.size());

    // Resizing the visual viewport shouldn't affect the WebView.
    IntSize newViewportSize = IntSize(320, 200);
    visualViewport.setSize(newViewportSize);
    EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(newViewportSize, visualViewport.size());
}

// This tests that shrinking the WebView while the page is fully scrolled
// doesn't move the viewport up/left, it should keep the visible viewport
// unchanged from the user's perspective (shrinking the FrameView will clamp
// the VisualViewport so we need to counter scroll the FrameView to make it
// appear to stay still). This caused bugs like crbug.com/453859.
TEST_P(ParameterizedVisualViewportTest, TestResizeAtFullyScrolledPreservesViewportLocation)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(800, 600));

    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");

    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();

    visualViewport.setScale(2);

    // Fully scroll both viewports.
    frameView.layoutViewportScrollableArea()->setScrollPosition(DoublePoint(10000, 10000), ProgrammaticScroll);
    visualViewport.move(FloatSize(10000, 10000));

    // Sanity check.
    ASSERT_POINT_EQ(FloatPoint(400, 300), visualViewport.location());
    ASSERT_POINT_EQ(DoublePoint(200, 1400), frameView.layoutViewportScrollableArea()->scrollPositionDouble());

    DoublePoint expectedLocation = frameView.getScrollableArea()->visibleContentRectDouble().location();

    // Shrink the WebView, this should cause both viewports to shrink and
    // WebView should do whatever it needs to do to preserve the visible
    // location.
    webViewImpl()->resize(IntSize(700, 550));

    EXPECT_POINT_EQ(expectedLocation, frameView.getScrollableArea()->visibleContentRectDouble().location());

    webViewImpl()->resize(IntSize(800, 600));

    EXPECT_POINT_EQ(expectedLocation, frameView.getScrollableArea()->visibleContentRectDouble().location());
}


// Test that the VisualViewport works as expected in case of a scaled
// and scrolled viewport - scroll down.
TEST_P(ParameterizedVisualViewportTest, TestResizeAfterVerticalScroll)
{
    /*
                 200                                 200
        |                   |               |                   |
        |                   |               |                   |
        |                   | 800           |                   | 800
        |-------------------|               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |   -------->   |                   |
        | 300               |               |                   |
        |                   |               |                   |
        |               400 |               |                   |
        |                   |               |-------------------|
        |                   |               |      75           |
        | 50                |               | 50             100|
        o-----              |               o----               |
        |    |              |               |   |  25           |
        |    |100           |               |-------------------|
        |    |              |               |                   |
        |    |              |               |                   |
        --------------------                --------------------

     */

    // Disable the test on Mac OSX until futher investigation.
    // Local build on Mac is OK but thes bot fails.
#if OS(MACOSX)
    return;
#endif

    initializeWithAndroidSettings();

    registerMockedHttpURLLoad("200-by-800-viewport.html");
    navigateTo(m_baseURL + "200-by-800-viewport.html");

    webViewImpl()->resize(IntSize(100, 200));

    // Scroll main frame to the bottom of the document
    webViewImpl()->mainFrame()->setScrollOffset(WebSize(0, 400));
    EXPECT_POINT_EQ(IntPoint(0, 400), frame()->view()->layoutViewportScrollableArea()->scrollPosition());

    webViewImpl()->setPageScaleFactor(2.0);

    // Scroll visual viewport to the bottom of the main frame
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    visualViewport.setLocation(FloatPoint(0, 300));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 300), visualViewport.location());

    // Verify the initial size of the visual viewport in the CSS pixels
    EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 100), visualViewport.visibleRect().size());

    // Perform the resizing
    webViewImpl()->resize(IntSize(200, 100));

    // After resizing the scale changes 2.0 -> 4.0
    EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 25), visualViewport.visibleRect().size());

    EXPECT_POINT_EQ(IntPoint(0, 625), frame()->view()->layoutViewportScrollableArea()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 75), visualViewport.location());
}

// Test that the VisualViewport works as expected in case if a scaled
// and scrolled viewport - scroll right.
TEST_P(ParameterizedVisualViewportTest, TestResizeAfterHorizontalScroll)
{
    /*
                 200                                 200
        ---------------o-----               ---------------o-----
        |              |    |               |            25|    |
        |              |    |               |              -----|
        |           100|    |               |100             50 |
        |              |    |               |                   |
        |              ---- |               |-------------------|
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |400                |   --------->  |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |-------------------|               |                   |
        |                   |               |                   |

     */

    // Disable the test on Mac OSX until futher investigation.
    // Local build on Mac is OK but thes bot fails.
#if OS(MACOSX)
    return;
#endif

    initializeWithAndroidSettings();

    registerMockedHttpURLLoad("200-by-800-viewport.html");
    navigateTo(m_baseURL + "200-by-800-viewport.html");

    webViewImpl()->resize(IntSize(100, 200));

    // Outer viewport takes the whole width of the document.

    webViewImpl()->setPageScaleFactor(2.0);

    // Scroll visual viewport to the right edge of the frame
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    visualViewport.setLocation(FloatPoint(150, 0));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(150, 0), visualViewport.location());

    // Verify the initial size of the visual viewport in the CSS pixels
    EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 100), visualViewport.visibleRect().size());

    webViewImpl()->resize(IntSize(200, 100));

    // After resizing the scale changes 2.0 -> 4.0
    EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 25), visualViewport.visibleRect().size());

    EXPECT_POINT_EQ(IntPoint(0, 0), frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(150, 0), visualViewport.location());
}

// Test that the container layer gets sized properly if the WebView is resized
// prior to the VisualViewport being attached to the layer tree.
TEST_P(ParameterizedVisualViewportTest, TestWebViewResizedBeforeAttachment)
{
    initializeWithDesktopSettings();
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();
    GraphicsLayer* rootGraphicsLayer =
        frameView.layoutViewItem().compositor()->rootGraphicsLayer();

    // Make sure that a resize that comes in while there's no root layer is
    // honoured when we attach to the layer tree.
    WebFrameWidgetBase* mainFrameWidget = webViewImpl()->mainFrameImpl()->frameWidget();
    mainFrameWidget->setRootGraphicsLayer(nullptr);
    webViewImpl()->resize(IntSize(320, 240));
    mainFrameWidget->setRootGraphicsLayer(rootGraphicsLayer);

    navigateTo("about:blank");
    webViewImpl()->updateAllLifecyclePhases();

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    EXPECT_FLOAT_SIZE_EQ(FloatSize(320, 240), visualViewport.containerLayer()->size());
}

// Make sure that the visibleRect method acurately reflects the scale and scroll location
// of the viewport.
TEST_P(ParameterizedVisualViewportTest, TestVisibleRect)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    navigateTo("about:blank");
    forceFullCompositingUpdate();

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();

    // Initial visible rect should be the whole frame.
    EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()), visualViewport.size());

    // Viewport is whole frame.
    IntSize size = IntSize(400, 200);
    webViewImpl()->resize(size);
    webViewImpl()->updateAllLifecyclePhases();
    visualViewport.setSize(size);

    // Scale the viewport to 2X; size should not change.
    FloatRect expectedRect(FloatPoint(0, 0), FloatSize(size));
    expectedRect.scale(0.5);
    visualViewport.setScale(2);
    EXPECT_EQ(2, visualViewport.scale());
    EXPECT_SIZE_EQ(size, visualViewport.size());
    EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.visibleRect());

    // Move the viewport.
    expectedRect.setLocation(FloatPoint(5, 7));
    visualViewport.setLocation(expectedRect.location());
    EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.visibleRect());

    expectedRect.setLocation(FloatPoint(200, 100));
    visualViewport.setLocation(expectedRect.location());
    EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.visibleRect());

    // Scale the viewport to 3X to introduce some non-int values.
    FloatPoint oldLocation = expectedRect.location();
    expectedRect = FloatRect(FloatPoint(), FloatSize(size));
    expectedRect.scale(1 / 3.0f);
    expectedRect.setLocation(oldLocation);
    visualViewport.setScale(3);
    EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.visibleRect());

    expectedRect.setLocation(FloatPoint(0.25f, 0.333f));
    visualViewport.setLocation(expectedRect.location());
    EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.visibleRect());
}

// Make sure that the visibleRectInDocument method acurately reflects the scale
// and scroll location of the viewport relative to the document.
TEST_P(ParameterizedVisualViewportTest, TestVisibleRectInDocument)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(100, 400));

    registerMockedHttpURLLoad("200-by-800-viewport.html");
    navigateTo(m_baseURL + "200-by-800-viewport.html");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();

    // Scale the viewport to 2X and move it.
    visualViewport.setScale(2);
    visualViewport.setLocation(FloatPoint(10, 15));
    EXPECT_FLOAT_RECT_EQ(FloatRect(10, 15, 50, 200), visualViewport.visibleRectInDocument());

    // Scroll the layout viewport. Ensure its offset is reflected in visibleRectInDocument().
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();
    frameView.layoutViewportScrollableArea()->setScrollPosition(DoublePoint(40, 100), ProgrammaticScroll);
    EXPECT_FLOAT_RECT_EQ(FloatRect(50, 115, 50, 200), visualViewport.visibleRectInDocument());
}

TEST_P(ParameterizedVisualViewportTest, TestFractionalScrollOffsetIsNotOverwritten)
{
    bool origFractionalOffsetsEnabled = RuntimeEnabledFeatures::fractionalScrollOffsetsEnabled();
    RuntimeEnabledFeatures::setFractionalScrollOffsetsEnabled(true);

    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(200, 250));

    registerMockedHttpURLLoad("200-by-800-viewport.html");
    navigateTo(m_baseURL + "200-by-800-viewport.html");

    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();
    frameView.layoutViewportScrollableArea()->setScrollPosition(DoublePoint(0, 10.5), ProgrammaticScroll);
    frameView.layoutViewportScrollableArea()->ScrollableArea::setScrollPosition(
        DoublePoint(10, 30.5), CompositorScroll);

    EXPECT_EQ(30.5, frameView.layoutViewportScrollableArea()->scrollPositionDouble().y());

    RuntimeEnabledFeatures::setFractionalScrollOffsetsEnabled(origFractionalOffsetsEnabled);
}

// Test that the viewport's scroll offset is always appropriately bounded such that the
// visual viewport always stays within the bounds of the main frame.
TEST_P(ParameterizedVisualViewportTest, TestOffsetClamping)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    navigateTo("about:blank");
    forceFullCompositingUpdate();

    // Visual viewport should be initialized to same size as frame so no scrolling possible.
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());

    visualViewport.setLocation(FloatPoint(-1, -2));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());

    visualViewport.setLocation(FloatPoint(100, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());

    visualViewport.setLocation(FloatPoint(-5, 10));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());

    // Scale by 2x. The viewport's visible rect should now have a size of 160x120.
    visualViewport.setScale(2);
    FloatPoint location(10, 50);
    visualViewport.setLocation(location);
    EXPECT_FLOAT_POINT_EQ(location, visualViewport.visibleRect().location());

    visualViewport.setLocation(FloatPoint(1000, 2000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120), visualViewport.visibleRect().location());

    visualViewport.setLocation(FloatPoint(-1000, -2000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());

    // Make sure offset gets clamped on scale out. Scale to 1.25 so the viewport is 256x192.
    visualViewport.setLocation(FloatPoint(160, 120));
    visualViewport.setScale(1.25);
    EXPECT_FLOAT_POINT_EQ(FloatPoint(64, 48), visualViewport.visibleRect().location());

    // Scale out smaller than 1.
    visualViewport.setScale(0.25);
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());
}

// Test that the viewport can be scrolled around only within the main frame in the presence
// of viewport resizes, as would be the case if the on screen keyboard came up.
TEST_P(ParameterizedVisualViewportTest, TestOffsetClampingWithResize)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    navigateTo("about:blank");
    forceFullCompositingUpdate();

    // Visual viewport should be initialized to same size as frame so no scrolling possible.
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());

    // Shrink the viewport vertically. The resize shouldn't affect the location, but it
    // should allow vertical scrolling.
    visualViewport.setSize(IntSize(320, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(10, 20));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 20), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(0, 100));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 40), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(0, 10));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 10), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(0, -100));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());

    // Repeat the above but for horizontal dimension.
    visualViewport.setSize(IntSize(280, 240));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(10, 20));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 0), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(100, 0));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(40, 0), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(10, 0));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 0), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(-100, 0));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());

    // Now with both dimensions.
    visualViewport.setSize(IntSize(280, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(10, 20));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 20), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(100, 100));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(40, 40), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(10, 3));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 3), visualViewport.visibleRect().location());
    visualViewport.setLocation(FloatPoint(-10, -4));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());
}

// Test that the viewport is scrollable but bounded appropriately within the main frame
// when we apply both scaling and resizes.
TEST_P(ParameterizedVisualViewportTest, TestOffsetClampingWithResizeAndScale)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    navigateTo("about:blank");
    forceFullCompositingUpdate();

    // Visual viewport should be initialized to same size as WebView so no scrolling possible.
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), visualViewport.visibleRect().location());

    // Zoom in to 2X so we can scroll the viewport to 160x120.
    visualViewport.setScale(2);
    visualViewport.setLocation(FloatPoint(200, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120), visualViewport.visibleRect().location());

    // Now resize the viewport to make it 10px smaller. Since we're zoomed in by 2X it should
    // allow us to scroll by 5px more.
    visualViewport.setSize(IntSize(310, 230));
    visualViewport.setLocation(FloatPoint(200, 200));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(165, 125), visualViewport.visibleRect().location());

    // The viewport can be larger than the main frame (currently 320, 240) though typically
    // the scale will be clamped to prevent it from actually being larger.
    visualViewport.setSize(IntSize(330, 250));
    EXPECT_SIZE_EQ(IntSize(330, 250), visualViewport.size());

    // Resize both the viewport and the frame to be larger.
    webViewImpl()->resize(IntSize(640, 480));
    webViewImpl()->updateAllLifecyclePhases();
    EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()), visualViewport.size());
    EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()), frame()->view()->frameRect().size());
    visualViewport.setLocation(FloatPoint(1000, 1000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(320, 240), visualViewport.visibleRect().location());

    // Make sure resizing the viewport doesn't change its offset if the resize doesn't make
    // the viewport go out of bounds.
    visualViewport.setLocation(FloatPoint(200, 200));
    visualViewport.setSize(IntSize(880, 560));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(200, 200), visualViewport.visibleRect().location());
}

// The main FrameView's size should be set such that its the size of the visual viewport
// at minimum scale. If there's no explicit minimum scale set, the FrameView should be
// set to the content width and height derived by the aspect ratio.
TEST_P(ParameterizedVisualViewportTest, TestFrameViewSizedToContent)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(320, 240));

    registerMockedHttpURLLoad("200-by-300-viewport.html");
    navigateTo(m_baseURL + "200-by-300-viewport.html");

    webViewImpl()->resize(IntSize(600, 800));
    webViewImpl()->updateAllLifecyclePhases();

    // Note: the size is ceiled and should match the behavior in CC's LayerImpl::bounds().
    EXPECT_SIZE_EQ(IntSize(200, 267),
        webViewImpl()->mainFrameImpl()->frameView()->frameRect().size());
}

// The main FrameView's size should be set such that its the size of the visual viewport
// at minimum scale. On Desktop, the minimum scale is set at 1 so make sure the FrameView
// is sized to the viewport.
TEST_P(ParameterizedVisualViewportTest, TestFrameViewSizedToMinimumScale)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(320, 240));

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    webViewImpl()->resize(IntSize(100, 160));
    webViewImpl()->updateAllLifecyclePhases();

    EXPECT_SIZE_EQ(IntSize(100, 160),
        webViewImpl()->mainFrameImpl()->frameView()->frameRect().size());
}

// Test that attaching a new frame view resets the size of the inner viewport scroll
// layer. crbug.com/423189.
TEST_P(ParameterizedVisualViewportTest, TestAttachingNewFrameSetsInnerScrollLayerSize)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(320, 240));

    // Load a wider page first, the navigation should resize the scroll layer to
    // the smaller size on the second navigation.
    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");
    webViewImpl()->updateAllLifecyclePhases();

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    visualViewport.setScale(2);
    visualViewport.move(FloatPoint(50, 60));

    // Move and scale the viewport to make sure it gets reset in the navigation.
    EXPECT_POINT_EQ(FloatPoint(50, 60), visualViewport.location());
    EXPECT_EQ(2, visualViewport.scale());

    // Navigate again, this time the FrameView should be smaller.
    registerMockedHttpURLLoad("viewport-device-width.html");
    navigateTo(m_baseURL + "viewport-device-width.html");

    // Ensure the scroll layer matches the frame view's size.
    EXPECT_SIZE_EQ(FloatSize(320, 240), visualViewport.scrollLayer()->size());

    // Ensure the location and scale were reset.
    EXPECT_POINT_EQ(FloatPoint(), visualViewport.location());
    EXPECT_EQ(1, visualViewport.scale());
}

// The main FrameView's size should be set such that its the size of the visual viewport
// at minimum scale. Test that the FrameView is appropriately sized in the presence
// of a viewport <meta> tag.
TEST_P(ParameterizedVisualViewportTest, TestFrameViewSizedToViewportMetaMinimumScale)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(320, 240));

    registerMockedHttpURLLoad("200-by-300-min-scale-2.html");
    navigateTo(m_baseURL + "200-by-300-min-scale-2.html");

    webViewImpl()->resize(IntSize(100, 160));
    webViewImpl()->updateAllLifecyclePhases();

    EXPECT_SIZE_EQ(IntSize(50, 80),
        webViewImpl()->mainFrameImpl()->frameView()->frameRect().size());
}

// Test that the visual viewport still gets sized in AutoSize/AutoResize mode.
TEST_P(ParameterizedVisualViewportTest, TestVisualViewportGetsSizeInAutoSizeMode)
{
    initializeWithDesktopSettings();

    EXPECT_SIZE_EQ(IntSize(0, 0), IntSize(webViewImpl()->size()));
    EXPECT_SIZE_EQ(IntSize(0, 0), frame()->page()->frameHost().visualViewport().size());

    webViewImpl()->enableAutoResizeMode(WebSize(10, 10), WebSize(1000, 1000));

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    EXPECT_SIZE_EQ(IntSize(200, 300), frame()->page()->frameHost().visualViewport().size());
}

// Test that the text selection handle's position accounts for the visual viewport.
TEST_P(ParameterizedVisualViewportTest, TestTextSelectionHandles)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(500, 800));

    registerMockedHttpURLLoad("pinch-viewport-input-field.html");
    navigateTo(m_baseURL + "pinch-viewport-input-field.html");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    webViewImpl()->setInitialFocus(false);

    WebRect originalAnchor;
    WebRect originalFocus;
    webViewImpl()->selectionBounds(originalAnchor, originalFocus);

    webViewImpl()->setPageScaleFactor(2);
    visualViewport.setLocation(FloatPoint(100, 400));

    WebRect anchor;
    WebRect focus;
    webViewImpl()->selectionBounds(anchor, focus);

    IntPoint expected(IntRect(originalAnchor).location());
    expected.moveBy(-flooredIntPoint(visualViewport.visibleRect().location()));
    expected.scale(visualViewport.scale(), visualViewport.scale());

    EXPECT_POINT_EQ(expected, IntRect(anchor).location());
    EXPECT_POINT_EQ(expected, IntRect(focus).location());

    // FIXME(bokan) - http://crbug.com/364154 - Figure out how to test text selection
    // as well rather than just carret.
}

// Test that the HistoryItem for the page stores the visual viewport's offset and scale.
TEST_P(ParameterizedVisualViewportTest, TestSavedToHistoryItem)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(200, 300));
    webViewImpl()->updateAllLifecyclePhases();

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
        toLocalFrame(webViewImpl()->page()->mainFrame())->loader().currentItem()->visualViewportScrollPoint());

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    visualViewport.setScale(2);

    EXPECT_EQ(2, toLocalFrame(webViewImpl()->page()->mainFrame())->loader().currentItem()->pageScaleFactor());

    visualViewport.setLocation(FloatPoint(10, 20));

    EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 20),
        toLocalFrame(webViewImpl()->page()->mainFrame())->loader().currentItem()->visualViewportScrollPoint());
}

// Test restoring a HistoryItem properly restores the visual viewport's state.
TEST_P(ParameterizedVisualViewportTest, TestRestoredFromHistoryItem)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(200, 300));

    registerMockedHttpURLLoad("200-by-300.html");

    WebHistoryItem item;
    item.initialize();
    WebURL destinationURL(URLTestHelpers::toKURL(m_baseURL + "200-by-300.html"));
    item.setURLString(destinationURL.string());
    item.setVisualViewportScrollOffset(WebFloatPoint(100, 120));
    item.setPageScaleFactor(2);

    FrameTestHelpers::loadHistoryItem(webViewImpl()->mainFrame(), item, WebHistoryDifferentDocumentLoad, WebCachePolicy::UseProtocolCachePolicy);

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    EXPECT_EQ(2, visualViewport.scale());

    EXPECT_FLOAT_POINT_EQ(FloatPoint(100, 120), visualViewport.visibleRect().location());
}

// Test restoring a HistoryItem without the visual viewport offset falls back to distributing
// the scroll offset between the main frame and the visual viewport.
TEST_F(VisualViewportTest, TestRestoredFromLegacyHistoryItem)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(100, 150));

    registerMockedHttpURLLoad("200-by-300-viewport.html");

    WebHistoryItem item;
    item.initialize();
    WebURL destinationURL(URLTestHelpers::toKURL(m_baseURL + "200-by-300-viewport.html"));
    item.setURLString(destinationURL.string());
    // (-1, -1) will be used if the HistoryItem is an older version prior to having
    // visual viewport scroll offset.
    item.setVisualViewportScrollOffset(WebFloatPoint(-1, -1));
    item.setScrollOffset(WebPoint(120, 180));
    item.setPageScaleFactor(2);

    FrameTestHelpers::loadHistoryItem(webViewImpl()->mainFrame(), item, WebHistoryDifferentDocumentLoad, WebCachePolicy::UseProtocolCachePolicy);

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    EXPECT_EQ(2, visualViewport.scale());
    EXPECT_POINT_EQ(IntPoint(100, 150), frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(20, 30), visualViewport.visibleRect().location());
}

// Test that navigation to a new page with a different sized main frame doesn't
// clobber the history item's main frame scroll offset. crbug.com/371867
TEST_P(ParameterizedVisualViewportTest, TestNavigateToSmallerFrameViewHistoryItemClobberBug)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(400, 400));
    webViewImpl()->updateAllLifecyclePhases();

    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");

    FrameView* frameView = webViewImpl()->mainFrameImpl()->frameView();
    frameView->layoutViewportScrollableArea()->setScrollPosition(IntPoint(0, 1000), ProgrammaticScroll);

    EXPECT_SIZE_EQ(IntSize(1000, 1000), frameView->frameRect().size());

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    visualViewport.setScale(2);
    visualViewport.setLocation(FloatPoint(350, 350));

    Persistent<HistoryItem> firstItem = webViewImpl()->mainFrameImpl()->frame()->loader().currentItem();
    EXPECT_POINT_EQ(IntPoint(0, 1000), firstItem->scrollPoint());

    // Now navigate to a page which causes a smaller frameView. Make sure that
    // navigating doesn't cause the history item to set a new scroll offset
    // before the item was replaced.
    navigateTo("about:blank");
    frameView = webViewImpl()->mainFrameImpl()->frameView();

    EXPECT_NE(firstItem, webViewImpl()->mainFrameImpl()->frame()->loader().currentItem());
    EXPECT_LT(frameView->frameRect().size().width(), 1000);
    EXPECT_POINT_EQ(IntPoint(0, 1000), firstItem->scrollPoint());
}

// Test that the coordinates sent into moveRangeSelection are offset by the
// visual viewport's location.
TEST_P(ParameterizedVisualViewportTest, DISABLED_TestWebFrameRangeAccountsForVisualViewportScroll)
{
    initializeWithDesktopSettings();
    webViewImpl()->settings()->setDefaultFontSize(12);
    webViewImpl()->resize(WebSize(640, 480));
    registerMockedHttpURLLoad("move_range.html");
    navigateTo(m_baseURL + "move_range.html");

    WebRect baseRect;
    WebRect extentRect;

    webViewImpl()->setPageScaleFactor(2);
    WebFrame* mainFrame = webViewImpl()->mainFrame();

    // Select some text and get the base and extent rects (that's the start of
    // the range and its end). Do a sanity check that the expected text is
    // selected
    mainFrame->executeScript(WebScriptSource("selectRange();"));
    EXPECT_EQ("ir", mainFrame->toWebLocalFrame()->selectionAsText().utf8());

    webViewImpl()->selectionBounds(baseRect, extentRect);
    WebPoint initialPoint(baseRect.x, baseRect.y);
    WebPoint endPoint(extentRect.x, extentRect.y);

    // Move the visual viewport over and make the selection in the same
    // screen-space location. The selection should change to two characters to
    // the right and down one line.
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    visualViewport.move(FloatPoint(60, 25));
    mainFrame->toWebLocalFrame()->moveRangeSelection(initialPoint, endPoint);
    EXPECT_EQ("t ", mainFrame->toWebLocalFrame()->selectionAsText().utf8());
}

// Test that the scrollFocusedEditableElementIntoRect method works with the visual viewport.
TEST_P(ParameterizedVisualViewportTest, DISABLED_TestScrollFocusedEditableElementIntoRect)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(500, 300));

    registerMockedHttpURLLoad("pinch-viewport-input-field.html");
    navigateTo(m_baseURL + "pinch-viewport-input-field.html");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    webViewImpl()->resizeVisualViewport(IntSize(200, 100));
    webViewImpl()->setInitialFocus(false);
    visualViewport.setLocation(FloatPoint());
    webViewImpl()->scrollFocusedEditableElementIntoRect(IntRect(0, 0, 500, 200));

    EXPECT_POINT_EQ(IntPoint(0, frame()->view()->maximumScrollPosition().y()),
        frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(150, 200), visualViewport.visibleRect().location());

    // Try it again but with the page zoomed in
    frame()->view()->setScrollPosition(IntPoint(0, 0), ProgrammaticScroll);
    webViewImpl()->resizeVisualViewport(IntSize(500, 300));
    visualViewport.setLocation(FloatPoint(0, 0));

    webViewImpl()->setPageScaleFactor(2);
    webViewImpl()->scrollFocusedEditableElementIntoRect(IntRect(0, 0, 500, 200));
    EXPECT_POINT_EQ(IntPoint(0, frame()->view()->maximumScrollPosition().y()),
        frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(125, 150), visualViewport.visibleRect().location());

    // Once more but make sure that we don't move the visual viewport unless necessary.
    registerMockedHttpURLLoad("pinch-viewport-input-field-long-and-wide.html");
    navigateTo(m_baseURL + "pinch-viewport-input-field-long-and-wide.html");
    webViewImpl()->setInitialFocus(false);
    visualViewport.setLocation(FloatPoint());
    frame()->view()->setScrollPosition(IntPoint(0, 0), ProgrammaticScroll);
    webViewImpl()->resizeVisualViewport(IntSize(500, 300));
    visualViewport.setLocation(FloatPoint(30, 50));

    webViewImpl()->setPageScaleFactor(2);
    webViewImpl()->scrollFocusedEditableElementIntoRect(IntRect(0, 0, 500, 200));
    EXPECT_POINT_EQ(IntPoint(200-30-75, 600-50-65), frame()->view()->scrollPosition());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(30, 50), visualViewport.visibleRect().location());
}

// Test that resizing the WebView causes ViewportConstrained objects to relayout.
TEST_P(ParameterizedVisualViewportTest, TestWebViewResizeCausesViewportConstrainedLayout)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(500, 300));

    registerMockedHttpURLLoad("pinch-viewport-fixed-pos.html");
    navigateTo(m_baseURL + "pinch-viewport-fixed-pos.html");

    LayoutObject* navbar = frame()->document()->getElementById("navbar")->layoutObject();

    EXPECT_FALSE(navbar->needsLayout());

    frame()->view()->resize(IntSize(500, 200));

    EXPECT_TRUE(navbar->needsLayout());
}

class MockWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
public:
    MOCK_METHOD1(showContextMenu, void(const WebContextMenuData&));
    MOCK_METHOD1(didChangeScrollOffset, void(WebLocalFrame*));
};

MATCHER_P2(ContextMenuAtLocation, x, y,
    std::string(negation ? "is" : "isn't")
    + " at expected location ["
    + PrintToString(x) + ", " + PrintToString(y) + "]")
{
    return arg.mousePosition.x == x && arg.mousePosition.y == y;
}

// Test that the context menu's location is correct in the presence of visual
// viewport offset.
TEST_P(ParameterizedVisualViewportTest, TestContextMenuShownInCorrectLocation)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(200, 300));

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    WebMouseEvent mouseDownEvent;
    mouseDownEvent.type = WebInputEvent::MouseDown;
    mouseDownEvent.x = 10;
    mouseDownEvent.y = 10;
    mouseDownEvent.windowX = 10;
    mouseDownEvent.windowY = 10;
    mouseDownEvent.globalX = 110;
    mouseDownEvent.globalY = 210;
    mouseDownEvent.clickCount = 1;
    mouseDownEvent.button = WebMouseEvent::Button::Right;

    // Corresponding release event (Windows shows context menu on release).
    WebMouseEvent mouseUpEvent(mouseDownEvent);
    mouseUpEvent.type = WebInputEvent::MouseUp;

    WebFrameClient* oldClient = webViewImpl()->mainFrameImpl()->client();
    MockWebFrameClient mockWebFrameClient;
    EXPECT_CALL(mockWebFrameClient, showContextMenu(ContextMenuAtLocation(mouseDownEvent.x, mouseDownEvent.y)));

    // Do a sanity check with no scale applied.
    webViewImpl()->mainFrameImpl()->setClient(&mockWebFrameClient);
    webViewImpl()->handleInputEvent(mouseDownEvent);
    webViewImpl()->handleInputEvent(mouseUpEvent);

    Mock::VerifyAndClearExpectations(&mockWebFrameClient);
    mouseDownEvent.button = WebMouseEvent::Button::Left;
    webViewImpl()->handleInputEvent(mouseDownEvent);

    // Now pinch zoom into the page and move the visual viewport. The context
    // menu should still appear at the location of the event, relative to the
    // WebView.
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    webViewImpl()->setPageScaleFactor(2);
    visualViewport.setLocation(FloatPoint(60, 80));
    EXPECT_CALL(mockWebFrameClient, showContextMenu(ContextMenuAtLocation(mouseDownEvent.x, mouseDownEvent.y)));

    mouseDownEvent.button = WebMouseEvent::Button::Right;
    webViewImpl()->handleInputEvent(mouseDownEvent);
    webViewImpl()->handleInputEvent(mouseUpEvent);

    // Reset the old client so destruction can occur naturally.
    webViewImpl()->mainFrameImpl()->setClient(oldClient);
}

// Test that the client is notified if page scroll events.
TEST_P(ParameterizedVisualViewportTest, TestClientNotifiedOfScrollEvents)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(200, 300));

    registerMockedHttpURLLoad("200-by-300.html");
    navigateTo(m_baseURL + "200-by-300.html");

    WebFrameClient* oldClient = webViewImpl()->mainFrameImpl()->client();
    MockWebFrameClient mockWebFrameClient;
    webViewImpl()->mainFrameImpl()->setClient(&mockWebFrameClient);

    webViewImpl()->setPageScaleFactor(2);
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();

    EXPECT_CALL(mockWebFrameClient, didChangeScrollOffset(_));
    visualViewport.setLocation(FloatPoint(60, 80));
    Mock::VerifyAndClearExpectations(&mockWebFrameClient);

    // Scroll vertically.
    EXPECT_CALL(mockWebFrameClient, didChangeScrollOffset(_));
    visualViewport.setLocation(FloatPoint(60, 90));
    Mock::VerifyAndClearExpectations(&mockWebFrameClient);

    // Scroll horizontally.
    EXPECT_CALL(mockWebFrameClient, didChangeScrollOffset(_));
    visualViewport.setLocation(FloatPoint(70, 90));

    // Reset the old client so destruction can occur naturally.
    webViewImpl()->mainFrameImpl()->setClient(oldClient);
}

// Tests that calling scroll into view on a visible element doesn't cause
// a scroll due to a fractional offset. Bug crbug.com/463356.
TEST_P(ParameterizedVisualViewportTest, ScrollIntoViewFractionalOffset)
{
    initializeWithAndroidSettings();

    webViewImpl()->resize(IntSize(1000, 1000));

    registerMockedHttpURLLoad("scroll-into-view.html");
    navigateTo(m_baseURL + "scroll-into-view.html");

    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();
    ScrollableArea* layoutViewportScrollableArea = frameView.layoutViewportScrollableArea();
    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    Element* inputBox = frame()->document()->getElementById("box");

    webViewImpl()->setPageScaleFactor(2);

    // The element is already in the view so the scrollIntoView shouldn't move
    // the viewport at all.
    webViewImpl()->setVisualViewportOffset(WebFloatPoint(250.25f, 100.25f));
    layoutViewportScrollableArea->setScrollPosition(DoublePoint(0, 900.75), ProgrammaticScroll);
    inputBox->scrollIntoViewIfNeeded(false);

    EXPECT_POINT_EQ(DoublePoint(0, 900), layoutViewportScrollableArea->scrollPositionDouble());
    EXPECT_POINT_EQ(FloatPoint(250.25f, 100.25f), visualViewport.location());

    // Change the fractional part of the frameview to one that would round down.
    layoutViewportScrollableArea->setScrollPosition(DoublePoint(0, 900.125), ProgrammaticScroll);
    inputBox->scrollIntoViewIfNeeded(false);

    EXPECT_POINT_EQ(DoublePoint(0, 900), layoutViewportScrollableArea->scrollPositionDouble());
    EXPECT_POINT_EQ(FloatPoint(250.25f, 100.25f), visualViewport.location());

    // Repeat both tests above with the visual viewport at a high fractional.
    webViewImpl()->setVisualViewportOffset(WebFloatPoint(250.875f, 100.875f));
    layoutViewportScrollableArea->setScrollPosition(DoublePoint(0, 900.75), ProgrammaticScroll);
    inputBox->scrollIntoViewIfNeeded(false);

    EXPECT_POINT_EQ(DoublePoint(0, 900), layoutViewportScrollableArea->scrollPositionDouble());
    EXPECT_POINT_EQ(FloatPoint(250.875f, 100.875f), visualViewport.location());

    // Change the fractional part of the frameview to one that would round down.
    layoutViewportScrollableArea->setScrollPosition(DoublePoint(0, 900.125), ProgrammaticScroll);
    inputBox->scrollIntoViewIfNeeded(false);

    EXPECT_POINT_EQ(DoublePoint(0, 900), layoutViewportScrollableArea->scrollPositionDouble());
    EXPECT_POINT_EQ(FloatPoint(250.875f, 100.875f), visualViewport.location());

    // Both viewports with a 0.5 fraction.
    webViewImpl()->setVisualViewportOffset(WebFloatPoint(250.5f, 100.5f));
    layoutViewportScrollableArea->setScrollPosition(DoublePoint(0, 900.5), ProgrammaticScroll);
    inputBox->scrollIntoViewIfNeeded(false);

    EXPECT_POINT_EQ(DoublePoint(0, 900), layoutViewportScrollableArea->scrollPositionDouble());
    EXPECT_POINT_EQ(FloatPoint(250.5f, 100.5f), visualViewport.location());
}

static IntPoint expectedMaxFrameViewScrollOffset(VisualViewport& visualViewport, FrameView& frameView)
{
    float aspectRatio = visualViewport.visibleRect().width() / visualViewport.visibleRect().height();
    float newHeight = frameView.frameRect().width() / aspectRatio;
    return IntPoint(
        frameView.contentsSize().width() - frameView.frameRect().width(),
        frameView.contentsSize().height() - newHeight);
}

TEST_F(VisualViewportTest, TestTopControlsAdjustment)
{
    initializeWithAndroidSettings();
    webViewImpl()->resizeWithTopControls(IntSize(500, 450), 20, false);

    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();

    visualViewport.setScale(1);
    EXPECT_SIZE_EQ(IntSize(500, 450), visualViewport.visibleRect().size());
    EXPECT_SIZE_EQ(IntSize(1000, 900), frameView.frameRect().size());

    // Simulate bringing down the top controls by 20px.
    webViewImpl()->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(), 1, 1);
    EXPECT_SIZE_EQ(IntSize(500, 430), visualViewport.visibleRect().size());

    // Test that the scroll bounds are adjusted appropriately: the visual viewport
    // should be shrunk by 20px to 430px. The outer viewport was shrunk to maintain the
    // aspect ratio so it's height is 860px.
    visualViewport.move(FloatPoint(10000, 10000));
    EXPECT_POINT_EQ(FloatPoint(500, 860 - 430), visualViewport.location());

    // The outer viewport (FrameView) should be affected as well.
    frameView.scrollBy(IntSize(10000, 10000), UserScroll);
    EXPECT_POINT_EQ(
        expectedMaxFrameViewScrollOffset(visualViewport, frameView),
        frameView.scrollPosition());

    // Simulate bringing up the top controls by 10.5px.
    webViewImpl()->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(), 1, -10.5f / 20);
    EXPECT_FLOAT_SIZE_EQ(FloatSize(500, 440.5f), visualViewport.visibleRect().size());

    // maximumScrollPosition |ceil|s the top controls adjustment.
    visualViewport.move(FloatPoint(10000, 10000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(500, 881 - 441), visualViewport.location());

    // The outer viewport (FrameView) should be affected as well.
    frameView.scrollBy(IntSize(10000, 10000), UserScroll);
    EXPECT_POINT_EQ(
        expectedMaxFrameViewScrollOffset(visualViewport, frameView),
        frameView.scrollPosition());
}

TEST_F(VisualViewportTest, TestTopControlsAdjustmentWithScale)
{
    initializeWithAndroidSettings();
    webViewImpl()->resizeWithTopControls(IntSize(500, 450), 20, false);

    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();

    visualViewport.setScale(2);
    EXPECT_SIZE_EQ(IntSize(250, 225), visualViewport.visibleRect().size());
    EXPECT_SIZE_EQ(IntSize(1000, 900), frameView.frameRect().size());

    // Simulate bringing down the top controls by 20px. Since we're zoomed in,
    // the top controls take up half as much space (in document-space) than
    // they do at an unzoomed level.
    webViewImpl()->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(), 1, 1);
    EXPECT_SIZE_EQ(IntSize(250, 215), visualViewport.visibleRect().size());

    // Test that the scroll bounds are adjusted appropriately.
    visualViewport.move(FloatPoint(10000, 10000));
    EXPECT_POINT_EQ(FloatPoint(750, 860 - 215), visualViewport.location());

    // The outer viewport (FrameView) should be affected as well.
    frameView.scrollBy(IntSize(10000, 10000), UserScroll);
    IntPoint expected = expectedMaxFrameViewScrollOffset(visualViewport, frameView);
    EXPECT_POINT_EQ(expected, frameView.scrollPosition());

    // Scale back out, FrameView max scroll shouldn't have changed. Visual
    // viewport should be moved up to accomodate larger view.
    webViewImpl()->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(), 0.5f, 0);
    EXPECT_EQ(1, visualViewport.scale());
    EXPECT_POINT_EQ(expected, frameView.scrollPosition());
    frameView.scrollBy(IntSize(10000, 10000), UserScroll);
    EXPECT_POINT_EQ(expected, frameView.scrollPosition());

    EXPECT_POINT_EQ(FloatPoint(500, 860 - 430), visualViewport.location());
    visualViewport.move(FloatPoint(10000, 10000));
    EXPECT_POINT_EQ(FloatPoint(500, 860 - 430), visualViewport.location());

    // Scale out, use a scale that causes fractional rects.
    webViewImpl()->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(), 0.8f, -1);
    EXPECT_SIZE_EQ(FloatSize(625, 562.5), visualViewport.visibleRect().size());

    // Bring out the top controls by 11
    webViewImpl()->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(), 1, 11 / 20.f);
    EXPECT_SIZE_EQ(FloatSize(625, 548.75), visualViewport.visibleRect().size());

    // Ensure max scroll offsets are updated properly.
    visualViewport.move(FloatPoint(10000, 10000));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(375, 877.5 - 548.75), visualViewport.location());

    frameView.scrollBy(IntSize(10000, 10000), UserScroll);
    EXPECT_POINT_EQ(
        expectedMaxFrameViewScrollOffset(visualViewport, frameView),
        frameView.scrollPosition());

}

// Tests that a scroll all the way to the bottom of the page, while hiding the
// top controls doesn't cause a clamp in the viewport scroll offset when the
// top controls initiated resize occurs.
TEST_F(VisualViewportTest, TestTopControlsAdjustmentAndResize)
{
    int topControlsHeight = 20;
    int visualViewportHeight = 450;
    int layoutViewportHeight = 900;
    float pageScale = 2;
    float minPageScale = 0.5;

    initializeWithAndroidSettings();

    // Initialize with top controls showing and shrinking the Blink size.
    webViewImpl()->resizeWithTopControls(
        WebSize(500, visualViewportHeight - topControlsHeight),
        20,
        true);
    webViewImpl()->topControls().setShownRatio(1);

    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();

    visualViewport.setScale(pageScale);
    EXPECT_SIZE_EQ(
        IntSize(250, (visualViewportHeight - topControlsHeight) / pageScale),
        visualViewport.visibleRect().size());
    EXPECT_SIZE_EQ(
        IntSize(1000, layoutViewportHeight - topControlsHeight / minPageScale),
        frameView.frameRect().size());
    EXPECT_SIZE_EQ(
        IntSize(500, visualViewportHeight - topControlsHeight),
        visualViewport.size());

    // Scroll all the way to the bottom, hiding the top controls in the process.
    visualViewport.move(FloatPoint(10000, 10000));
    frameView.scrollBy(IntSize(10000, 10000), UserScroll);
    webViewImpl()->topControls().setShownRatio(0);

    EXPECT_SIZE_EQ(
        IntSize(250, visualViewportHeight / pageScale),
        visualViewport.visibleRect().size());

    IntPoint frameViewExpected =
        expectedMaxFrameViewScrollOffset(visualViewport, frameView);
    FloatPoint visualViewportExpected =
        FloatPoint(
            750,
            layoutViewportHeight - visualViewportHeight / pageScale);

    EXPECT_POINT_EQ(visualViewportExpected, visualViewport.location());
    EXPECT_POINT_EQ(frameViewExpected, frameView.scrollPosition());

    FloatPoint totalExpected = visualViewportExpected + frameViewExpected;

    // Resize the widget to match the top controls adjustment. Ensure that the
    // total offset (i.e. what the user sees) doesn't change because of clamping
    // the offsets to valid values.
    webViewImpl()->resizeWithTopControls(
        WebSize(500, visualViewportHeight),
        20,
        false);

    EXPECT_SIZE_EQ(IntSize(500, visualViewportHeight), visualViewport.size());
    EXPECT_SIZE_EQ(
        IntSize(250, visualViewportHeight / pageScale),
        visualViewport.visibleRect().size());
    EXPECT_SIZE_EQ(
        IntSize(1000, layoutViewportHeight),
        frameView.frameRect().size());
    EXPECT_POINT_EQ(
        totalExpected,
        frameView.scrollPosition() + visualViewport.location());
}

// Tests that a scroll all the way to the bottom while showing the top controls
// doesn't cause a clamp to the viewport scroll offset when the top controls
// initiated resize occurs.
TEST_F(VisualViewportTest, TestTopControlsShrinkAdjustmentAndResize)
{
    int topControlsHeight = 20;
    int visualViewportHeight = 500;
    int layoutViewportHeight = 1000;
    int contentHeight = 2000;
    float pageScale = 2;
    float minPageScale = 0.5;

    initializeWithAndroidSettings();

    // Initialize with top controls hidden and not shrinking the Blink size.
    webViewImpl()->resizeWithTopControls(
        IntSize(500, visualViewportHeight),
        20,
        false);
    webViewImpl()->topControls().setShownRatio(0);

    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();

    visualViewport.setScale(pageScale);
    EXPECT_SIZE_EQ(
        IntSize(250, visualViewportHeight / pageScale),
        visualViewport.visibleRect().size());
    EXPECT_SIZE_EQ(
        IntSize(1000, layoutViewportHeight),
        frameView.frameRect().size());
    EXPECT_SIZE_EQ(
        IntSize(500, visualViewportHeight),
        visualViewport.size());

    // Scroll all the way to the bottom, showing the the top controls in the
    // process. (This could happen via window.scrollTo during a scroll, for
    // example).
    webViewImpl()->topControls().setShownRatio(1);
    visualViewport.move(FloatPoint(10000, 10000));
    frameView.scrollBy(IntSize(10000, 10000), UserScroll);

    EXPECT_SIZE_EQ(
        IntSize(250, (visualViewportHeight - topControlsHeight) / pageScale),
        visualViewport.visibleRect().size());

    IntPoint frameViewExpected = IntPoint(0,
        contentHeight
            - (layoutViewportHeight - topControlsHeight / minPageScale));
    FloatPoint visualViewportExpected =
        FloatPoint(750,
            (layoutViewportHeight - topControlsHeight / minPageScale
            - visualViewport.visibleRect().height()));

    EXPECT_POINT_EQ(visualViewportExpected, visualViewport.location());
    EXPECT_POINT_EQ(frameViewExpected, frameView.scrollPosition());

    FloatPoint totalExpected = visualViewportExpected + frameViewExpected;

    // Resize the widget to match the top controls adjustment. Ensure that the
    // total offset (i.e. what the user sees) doesn't change because of clamping
    // the offsets to valid values.
    webViewImpl()->resizeWithTopControls(
        WebSize(500, visualViewportHeight - topControlsHeight),
        20,
        true);

    EXPECT_SIZE_EQ(
        IntSize(500, visualViewportHeight - topControlsHeight),
        visualViewport.size());
    EXPECT_SIZE_EQ(
        IntSize(250, (visualViewportHeight - topControlsHeight) / pageScale),
        visualViewport.visibleRect().size());
    EXPECT_SIZE_EQ(
        IntSize(1000, layoutViewportHeight - topControlsHeight / minPageScale),
        frameView.frameRect().size());
    EXPECT_POINT_EQ(
        totalExpected,
        frameView.scrollPosition() + visualViewport.location());
}

// Tests that a resize due to top controls hiding doesn't incorrectly clamp the
// main frame's scroll offset. crbug.com/428193.
TEST_F(VisualViewportTest, TestTopControlHidingResizeDoesntClampMainFrame)
{
    initializeWithAndroidSettings();
    webViewImpl()->resizeWithTopControls(webViewImpl()->size(), 500, false);
    webViewImpl()->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(), 1, 1);
    webViewImpl()->resizeWithTopControls(WebSize(1000, 1000), 500, true);

    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");

    // Scroll the FrameView to the bottom of the page but "hide" the top
    // controls on the compositor side so the max scroll position should account
    // for the full viewport height.
    webViewImpl()->applyViewportDeltas(WebFloatSize(), WebFloatSize(), WebFloatSize(), 1, -1);
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();
    frameView.setScrollPosition(IntPoint(0, 10000), ProgrammaticScroll);
    EXPECT_EQ(500, frameView.scrollPositionDouble().y());

    // Now send the resize, make sure the scroll offset doesn't change.
    webViewImpl()->resizeWithTopControls(WebSize(1000, 1500), 500, false);
    EXPECT_EQ(500, frameView.scrollPositionDouble().y());
}

// Tests that the layout viewport's scroll layer bounds are updated in a compositing
// change update. crbug.com/423188.
TEST_P(ParameterizedVisualViewportTest, TestChangingContentSizeAffectsScrollBounds)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(100, 150));

    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");

    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();
    WebLayer* scrollLayer = frameView.layerForScrolling()->platformLayer();

    webViewImpl()->mainFrame()->executeScript(WebScriptSource(
        "var content = document.getElementById(\"content\");"
        "content.style.width = \"1500px\";"
        "content.style.height = \"2400px\";"));
    frameView.updateAllLifecyclePhases();

    EXPECT_SIZE_EQ(IntSize(1500, 2400), IntSize(scrollLayer->bounds()));
}

// Tests that resizing the visual viepwort keeps its bounds within the outer
// viewport.
TEST_P(ParameterizedVisualViewportTest, ResizeVisualViewportStaysWithinOuterViewport)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(100, 200));

    navigateTo("about:blank");
    webViewImpl()->updateAllLifecyclePhases();

    webViewImpl()->resizeVisualViewport(IntSize(100, 100));

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    visualViewport.move(FloatPoint(0, 100));

    EXPECT_EQ(100, visualViewport.location().y());

    webViewImpl()->resizeVisualViewport(IntSize(100, 200));

    EXPECT_EQ(0, visualViewport.location().y());
}

TEST_P(ParameterizedVisualViewportTest, ElementBoundsInViewportSpaceAccountsForViewport)
{
    initializeWithAndroidSettings();

    webViewImpl()->resize(IntSize(500, 800));

    registerMockedHttpURLLoad("pinch-viewport-input-field.html");
    navigateTo(m_baseURL + "pinch-viewport-input-field.html");

    webViewImpl()->setInitialFocus(false);
    Element* inputElement = webViewImpl()->focusedElement();

    IntRect bounds = inputElement->layoutObject()->absoluteBoundingBoxRect();

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    IntPoint scrollDelta(250, 400);
    visualViewport.setScale(2);
    visualViewport.setLocation(scrollDelta);

    const IntRect boundsInViewport = inputElement->boundsInViewport();
    IntRect expectedBounds = bounds;
    expectedBounds.scale(2.f);
    IntPoint expectedScrollDelta = scrollDelta;
    expectedScrollDelta.scale(2.f, 2.f);

    EXPECT_POINT_EQ(IntPoint(expectedBounds.location() - expectedScrollDelta),
        boundsInViewport.location());
    EXPECT_SIZE_EQ(expectedBounds.size(), boundsInViewport.size());
}

TEST_P(ParameterizedVisualViewportTest, ElementVisibleBoundsInVisualViewport)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(640, 1080));
    registerMockedHttpURLLoad("viewport-select.html");
    navigateTo(m_baseURL + "viewport-select.html");

    ASSERT_EQ(2.0f, webViewImpl()->pageScaleFactor());
    webViewImpl()->setInitialFocus(false);
    Element* element = webViewImpl()->focusedElement();
    EXPECT_FALSE(element->visibleBoundsInVisualViewport().isEmpty());

    webViewImpl()->setPageScaleFactor(4.0);
    EXPECT_TRUE(element->visibleBoundsInVisualViewport().isEmpty());
}

// Test that the various window.scroll and document.body.scroll properties and
// methods work unchanged from the pre-virtual viewport mode.
TEST_P(ParameterizedVisualViewportTest, bodyAndWindowScrollPropertiesAccountForViewport)
{
    initializeWithAndroidSettings();

    webViewImpl()->resize(IntSize(200, 300));

    // Load page with no main frame scrolling.
    registerMockedHttpURLLoad("200-by-300-viewport.html");
    navigateTo(m_baseURL + "200-by-300-viewport.html");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    visualViewport.setScale(2);

    // Chrome's quirky behavior regarding viewport scrolling means we treat the
    // body element as the viewport and don't apply scrolling to the HTML
    // element.
    RuntimeEnabledFeatures::setScrollTopLeftInteropEnabled(false);

    LocalDOMWindow* window = webViewImpl()->mainFrameImpl()->frame()->localDOMWindow();
    window->scrollTo(100, 150);
    EXPECT_EQ(100, window->scrollX());
    EXPECT_EQ(150, window->scrollY());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(100, 150), visualViewport.location());

    HTMLElement* body = toHTMLBodyElement(window->document()->body());
    body->setScrollLeft(50);
    body->setScrollTop(130);
    EXPECT_EQ(50, body->scrollLeft());
    EXPECT_EQ(130, body->scrollTop());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 130), visualViewport.location());

    HTMLElement* documentElement = toHTMLElement(window->document()->documentElement());
    documentElement->setScrollLeft(40);
    documentElement->setScrollTop(50);
    EXPECT_EQ(0, documentElement->scrollLeft());
    EXPECT_EQ(0, documentElement->scrollTop());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 130), visualViewport.location());

    visualViewport.setLocation(FloatPoint(10, 20));
    EXPECT_EQ(10, body->scrollLeft());
    EXPECT_EQ(20, body->scrollTop());
    EXPECT_EQ(0, documentElement->scrollLeft());
    EXPECT_EQ(0, documentElement->scrollTop());
    EXPECT_EQ(10, window->scrollX());
    EXPECT_EQ(20, window->scrollY());

    // Turning on the standards-compliant viewport scrolling impl should make
    // the document element the viewport and not body.
    RuntimeEnabledFeatures::setScrollTopLeftInteropEnabled(true);

    window->scrollTo(100, 150);
    EXPECT_EQ(100, window->scrollX());
    EXPECT_EQ(150, window->scrollY());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(100, 150), visualViewport.location());

    body->setScrollLeft(50);
    body->setScrollTop(130);
    EXPECT_EQ(0, body->scrollLeft());
    EXPECT_EQ(0, body->scrollTop());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(100, 150), visualViewport.location());

    documentElement->setScrollLeft(40);
    documentElement->setScrollTop(50);
    EXPECT_EQ(40, documentElement->scrollLeft());
    EXPECT_EQ(50, documentElement->scrollTop());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(40, 50), visualViewport.location());

    visualViewport.setLocation(FloatPoint(10, 20));
    EXPECT_EQ(0, body->scrollLeft());
    EXPECT_EQ(0, body->scrollTop());
    EXPECT_EQ(10, documentElement->scrollLeft());
    EXPECT_EQ(20, documentElement->scrollTop());
    EXPECT_EQ(10, window->scrollX());
    EXPECT_EQ(20, window->scrollY());
}

// Tests that when a new frame is created, it is created with the intended
// size (i.e. viewport at minimum scale, 100x200 / 0.5).
TEST_P(ParameterizedVisualViewportTest, TestMainFrameInitializationSizing)
{
    initializeWithAndroidSettings();

    webViewImpl()->resize(IntSize(100, 200));

    registerMockedHttpURLLoad("content-width-1000-min-scale.html");
    navigateTo(m_baseURL + "content-width-1000-min-scale.html");

    WebLocalFrameImpl* localFrame = webViewImpl()->mainFrameImpl();
    // The detachLayoutTree() and dispose() calls are a hack to prevent this test
    // from violating invariants about frame state during navigation/detach.
    localFrame->frame()->document()->detachLayoutTree();
    localFrame->createFrameView();

    FrameView& frameView = *localFrame->frameView();
    EXPECT_SIZE_EQ(IntSize(200, 400), frameView.frameRect().size());
    frameView.dispose();
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(ParameterizedVisualViewportTest, FractionalMaxScrollOffset)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(101, 201));
    navigateTo("about:blank");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();
    ScrollableArea* scrollableArea = &visualViewport;

    webViewImpl()->setPageScaleFactor(1.0);
    EXPECT_FLOAT_POINT_EQ(DoublePoint(), scrollableArea->maximumScrollPositionDouble());

    webViewImpl()->setPageScaleFactor(2);
    EXPECT_FLOAT_POINT_EQ(DoublePoint(101. / 2., 201. / 2.), scrollableArea->maximumScrollPositionDouble());
}

// Tests that the slow scrolling after an impl scroll on the visual viewport
// is continuous. crbug.com/453460 was caused by the impl-path not updating the
// ScrollAnimatorBase class.
TEST_P(ParameterizedVisualViewportTest, SlowScrollAfterImplScroll)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(800, 600));
    navigateTo("about:blank");

    VisualViewport& visualViewport = frame()->page()->frameHost().visualViewport();

    // Apply some scroll and scale from the impl-side.
    webViewImpl()->applyViewportDeltas(
        WebFloatSize(300, 200),
        WebFloatSize(0, 0),
        WebFloatSize(0, 0),
        2,
        0);

    EXPECT_POINT_EQ(FloatPoint(300, 200), visualViewport.location());

    // Send a scroll event on the main thread path.
    PlatformGestureEvent gsu(
        PlatformEvent::GestureScrollUpdate,
        IntPoint(0, 0),
        IntPoint(0, 0),
        IntSize(5, 5),
        0, PlatformEvent::NoModifiers, PlatformGestureSourceTouchpad);
    gsu.setScrollGestureData(-50, -60, ScrollByPrecisePixel, 1, 1, ScrollInertialPhaseUnknown, false, -1 /* null plugin id */);

    frame()->eventHandler().handleGestureEvent(gsu);

    // The scroll sent from the impl-side must not be overwritten.
    EXPECT_POINT_EQ(FloatPoint(350, 260), visualViewport.location());
}

static void accessibilitySettings(WebSettings* settings)
{
    VisualViewportTest::configureSettings(settings);
    settings->setAccessibilityEnabled(true);
}

TEST_P(ParameterizedVisualViewportTest, AccessibilityHitTestWhileZoomedIn)
{
    initializeWithDesktopSettings(accessibilitySettings);

    registerMockedHttpURLLoad("hit-test.html");
    navigateTo(m_baseURL + "hit-test.html");

    webViewImpl()->resize(IntSize(500, 500));
    webViewImpl()->updateAllLifecyclePhases();

    WebDocument webDoc = webViewImpl()->mainFrame()->document();
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();

    webViewImpl()->setPageScaleFactor(2);
    webViewImpl()->setVisualViewportOffset(WebFloatPoint(200, 230));
    frameView.layoutViewportScrollableArea()->setScrollPosition(DoublePoint(400, 1100), ProgrammaticScroll);

    // FIXME(504057): PaintLayerScrollableArea dirties the compositing state.
    forceFullCompositingUpdate();

    // Because of where the visual viewport is located, this should hit the bottom right
    // target (target 4).
    WebAXObject hitNode = webDoc.accessibilityObject().hitTest(WebPoint(154, 165));
    WebAXNameFrom nameFrom;
    WebVector<WebAXObject> nameObjects;
    EXPECT_EQ(std::string("Target4"), hitNode.name(nameFrom, nameObjects).utf8());
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(ParameterizedVisualViewportTest, TestCoordinateTransforms)
{
    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(800, 600));
    registerMockedHttpURLLoad("content-width-1000.html");
    navigateTo(m_baseURL + "content-width-1000.html");

    VisualViewport& visualViewport = webViewImpl()->page()->frameHost().visualViewport();
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();

    // At scale = 1 the transform should be a no-op.
    visualViewport.setScale(1);
    EXPECT_FLOAT_POINT_EQ(FloatPoint(314, 273), visualViewport.viewportToRootFrame(FloatPoint(314, 273)));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(314, 273), visualViewport.rootFrameToViewport(FloatPoint(314, 273)));

    // At scale = 2.
    visualViewport.setScale(2);
    EXPECT_FLOAT_POINT_EQ(FloatPoint(55, 75), visualViewport.viewportToRootFrame(FloatPoint(110, 150)));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(110, 150), visualViewport.rootFrameToViewport(FloatPoint(55, 75)));

    // At scale = 2 and with the visual viewport offset.
    visualViewport.setLocation(FloatPoint(10, 12));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 62), visualViewport.viewportToRootFrame(FloatPoint(80, 100)));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(80, 100), visualViewport.rootFrameToViewport(FloatPoint(50, 62)));

    // Test points that will cause non-integer values.
    EXPECT_FLOAT_POINT_EQ(FloatPoint(50.5, 62.4), visualViewport.viewportToRootFrame(FloatPoint(81, 100.8)));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(81, 100.8), visualViewport.rootFrameToViewport(FloatPoint(50.5, 62.4)));


    // Scrolling the main frame should have no effect.
    frameView.layoutViewportScrollableArea()->setScrollPosition(DoublePoint(100, 120), ProgrammaticScroll);
    EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 62), visualViewport.viewportToRootFrame(FloatPoint(80, 100)));
    EXPECT_FLOAT_POINT_EQ(FloatPoint(80, 100), visualViewport.rootFrameToViewport(FloatPoint(50, 62)));
}

// Tests that the window dimensions are available before a full layout occurs.
// More specifically, it checks that the innerWidth and innerHeight window
// properties will trigger a layout which will cause an update to viewport
// constraints and a refreshed initial scale. crbug.com/466718
TEST_P(ParameterizedVisualViewportTest, WindowDimensionsOnLoad)
{
    initializeWithAndroidSettings();
    registerMockedHttpURLLoad("window_dimensions.html");
    webViewImpl()->resize(IntSize(800, 600));
    navigateTo(m_baseURL + "window_dimensions.html");

    Element* output = frame()->document()->getElementById("output");
    DCHECK(output);
    EXPECT_EQ(std::string("1600x1200"), std::string(output->innerHTML().ascii().data()));
}

// Similar to above but make sure the initial scale is updated with the content
// width for a very wide page. That is, make that innerWidth/Height actually
// trigger a layout of the content, and not just an update of the viepwort.
// crbug.com/466718
TEST_P(ParameterizedVisualViewportTest, WindowDimensionsOnLoadWideContent)
{
    initializeWithAndroidSettings();
    registerMockedHttpURLLoad("window_dimensions_wide_div.html");
    webViewImpl()->resize(IntSize(800, 600));
    navigateTo(m_baseURL + "window_dimensions_wide_div.html");

    Element* output = frame()->document()->getElementById("output");
    DCHECK(output);
    EXPECT_EQ(std::string("2000x1500"), std::string(output->innerHTML().ascii().data()));
}

TEST_P(ParameterizedVisualViewportTest, PinchZoomGestureScrollsVisualViewportOnly)
{
    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(100, 100));

    registerMockedHttpURLLoad("200-by-800-viewport.html");
    navigateTo(m_baseURL + "200-by-800-viewport.html");

    WebGestureEvent pinchUpdate;
    pinchUpdate.type = WebInputEvent::GesturePinchUpdate;
    pinchUpdate.sourceDevice = WebGestureDeviceTouchpad;
    pinchUpdate.x = 100;
    pinchUpdate.y = 100;
    pinchUpdate.data.pinchUpdate.scale = 2;
    pinchUpdate.data.pinchUpdate.zoomDisabled = false;

    webViewImpl()->handleInputEvent(pinchUpdate);

    VisualViewport& visualViewport = webViewImpl()->page()->frameHost().visualViewport();
    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();

    EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 50), visualViewport.location());
    EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0), frameView.scrollPositionDouble());
}

TEST_P(ParameterizedVisualViewportTest, ResizeWithScrollAnchoring)
{
    bool wasScrollAnchoringEnabled = RuntimeEnabledFeatures::scrollAnchoringEnabled();
    RuntimeEnabledFeatures::setScrollAnchoringEnabled(true);

    initializeWithDesktopSettings();
    webViewImpl()->resize(IntSize(800, 600));

    registerMockedHttpURLLoad("icb-relative-content.html");
    navigateTo(m_baseURL + "icb-relative-content.html");

    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();
    frameView.layoutViewportScrollableArea()->setScrollPosition(DoublePoint(700, 500), ProgrammaticScroll);

    webViewImpl()->resize(IntSize(400, 300));
    EXPECT_POINT_EQ(DoublePoint(300, 200), frameView.layoutViewportScrollableArea()->scrollPositionDouble());

    RuntimeEnabledFeatures::setScrollAnchoringEnabled(wasScrollAnchoringEnabled);
}

// Ensure that resize anchoring as happens when top controls hide/show affects
// the scrollable area that's currently set as the root scroller.
TEST_P(ParameterizedVisualViewportTest, ResizeAnchoringWithRootScroller)
{
    bool wasRootScrollerEnabled =
        RuntimeEnabledFeatures::setRootScrollerEnabled();
    RuntimeEnabledFeatures::setSetRootScrollerEnabled(true);

    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(800, 600));

    registerMockedHttpURLLoad("root-scroller-div.html");
    navigateTo(m_baseURL + "root-scroller-div.html");

    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();

    Element* scroller = frame()->document()->getElementById("rootScroller");
    NonThrowableExceptionState nonThrow;
    frame()->document()->setRootScroller(scroller, nonThrow);

    webViewImpl()->setPageScaleFactor(3.f);
    frameView.getScrollableArea()->setScrollPosition(
        DoublePoint(0, 400), ProgrammaticScroll);

    VisualViewport& visualViewport =
        webViewImpl()->page()->frameHost().visualViewport();
    visualViewport.setScrollPosition(DoublePoint(0, 400), ProgrammaticScroll);

    webViewImpl()->resize(IntSize(800, 500));

    EXPECT_POINT_EQ(
        DoublePoint(),
        frameView.layoutViewportScrollableArea()->scrollPositionDouble());

    RuntimeEnabledFeatures::setSetRootScrollerEnabled(wasRootScrollerEnabled);
}

// Ensure that resize anchoring as happens when the device is rotated affects
// the scrollable area that's currently set as the root scroller.
TEST_P(ParameterizedVisualViewportTest, RotationAnchoringWithRootScroller)
{
    bool wasRootScrollerEnabled =
        RuntimeEnabledFeatures::setRootScrollerEnabled();
    RuntimeEnabledFeatures::setSetRootScrollerEnabled(true);

    initializeWithAndroidSettings();
    webViewImpl()->resize(IntSize(800, 600));

    registerMockedHttpURLLoad("root-scroller-div.html");
    navigateTo(m_baseURL + "root-scroller-div.html");

    FrameView& frameView = *webViewImpl()->mainFrameImpl()->frameView();

    Element* scroller = frame()->document()->getElementById("rootScroller");
    NonThrowableExceptionState nonThrow;
    frame()->document()->setRootScroller(scroller, nonThrow);
    webViewImpl()->updateAllLifecyclePhases();

    scroller->setScrollTop(800);

    webViewImpl()->resize(IntSize(600, 800));

    EXPECT_POINT_EQ(
        DoublePoint(),
        frameView.layoutViewportScrollableArea()->scrollPositionDouble());
    EXPECT_EQ(600, scroller->scrollTop());

    RuntimeEnabledFeatures::setSetRootScrollerEnabled(wasRootScrollerEnabled);
}

} // namespace
