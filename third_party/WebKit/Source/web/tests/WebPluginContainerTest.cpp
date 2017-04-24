/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "public/web/WebPluginContainer.h"

#include <memory>
#include <string>
#include "core/dom/Element.h"
#include "core/events/KeyboardEvent.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/layout/LayoutObject.h"
#include "core/page/Page.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebMouseWheelEvent.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTouchEvent.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebDocument.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebPluginParams.h"
#include "public/web/WebPrintParams.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FakeWebPlugin.h"
#include "web/tests/FrameTestHelpers.h"

using blink::testing::runPendingTasks;

namespace blink {

class WebPluginContainerTest : public ::testing::Test {
 public:
  WebPluginContainerTest() : m_baseURL("http://www.test.com/") {}

  void TearDown() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

  void calculateGeometry(WebPluginContainerImpl* pluginContainerImpl,
                         IntRect& windowRect,
                         IntRect& clipRect,
                         IntRect& unobscuredRect,
                         Vector<IntRect>& cutOutRects) {
    pluginContainerImpl->calculateGeometry(windowRect, clipRect, unobscuredRect,
                                           cutOutRects);
  }

  void registerMockedURL(
      const std::string& fileName,
      const std::string& mimeType = std::string("text/html")) {
    URLTestHelpers::registerMockedURLLoadFromBase(
        WebString::fromUTF8(m_baseURL), testing::webTestDataPath(),
        WebString::fromUTF8(fileName), WebString::fromUTF8(mimeType));
  }

 protected:
  std::string m_baseURL;
};

namespace {

template <typename T>
class CustomPluginWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  WebPlugin* createPlugin(WebLocalFrame* frame,
                          const WebPluginParams& params) override {
    return new T(frame, params);
  }
};

class TestPluginWebFrameClient;

// Subclass of FakeWebPlugin that has a selection of 'x' as plain text and 'y'
// as markup text.
class TestPlugin : public FakeWebPlugin {
 public:
  TestPlugin(WebFrame* frame,
             const WebPluginParams& params,
             TestPluginWebFrameClient* testClient)
      : FakeWebPlugin(frame, params) {
    m_testClient = testClient;
  }

  bool hasSelection() const override { return true; }
  WebString selectionAsText() const override { return WebString("x"); }
  WebString selectionAsMarkup() const override { return WebString("y"); }
  bool supportsPaginatedPrint() override { return true; }
  int printBegin(const WebPrintParams& printParams) override { return 1; }
  void printPage(int pageNumber, WebCanvas*) override;

 private:
  TestPluginWebFrameClient* m_testClient;
};

class TestPluginWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
  WebPlugin* createPlugin(WebLocalFrame* frame,
                          const WebPluginParams& params) override {
    if (params.mimeType == "application/x-webkit-test-webplugin" ||
        params.mimeType == "application/pdf")
      return new TestPlugin(frame, params, this);
    return WebFrameClient::createPlugin(frame, params);
  }

 public:
  void onPrintPage() { m_printedPage = true; }
  bool printedAtLeastOnePage() { return m_printedPage; }

 private:
  bool m_printedPage = false;
};

void TestPlugin::printPage(int pageNumber, WebCanvas* canvas) {
  DCHECK(m_testClient);
  m_testClient->onPrintPage();
}

WebPluginContainer* getWebPluginContainer(WebView* webView,
                                          const WebString& id) {
  WebElement element = webView->mainFrame()->document().getElementById(id);
  return element.pluginContainer();
}

}  // namespace

TEST_F(WebPluginContainerTest, WindowToLocalPointTest) {
  registerMockedURL("plugin_container.html");
  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebPluginContainer* pluginContainerOne =
      getWebPluginContainer(webView, WebString::fromUTF8("translated-plugin"));
  DCHECK(pluginContainerOne);
  WebPoint point1 = pluginContainerOne->rootFrameToLocalPoint(WebPoint(10, 10));
  ASSERT_EQ(0, point1.x);
  ASSERT_EQ(0, point1.y);
  WebPoint point2 =
      pluginContainerOne->rootFrameToLocalPoint(WebPoint(100, 100));
  ASSERT_EQ(90, point2.x);
  ASSERT_EQ(90, point2.y);

  WebPluginContainer* pluginContainerTwo =
      getWebPluginContainer(webView, WebString::fromUTF8("rotated-plugin"));
  DCHECK(pluginContainerTwo);
  WebPoint point3 = pluginContainerTwo->rootFrameToLocalPoint(WebPoint(0, 10));
  ASSERT_EQ(10, point3.x);
  ASSERT_EQ(0, point3.y);
  WebPoint point4 =
      pluginContainerTwo->rootFrameToLocalPoint(WebPoint(-10, 10));
  ASSERT_EQ(10, point4.x);
  ASSERT_EQ(10, point4.y);
}

TEST_F(WebPluginContainerTest, PluginDocumentPluginIsFocused) {
  registerMockedURL("test.pdf", "application/pdf");

  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "test.pdf", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->updateAllLifecyclePhases();

  WebDocument document = webView->mainFrame()->document();
  EXPECT_TRUE(document.isPluginDocument());
  WebPluginContainer* pluginContainer =
      getWebPluginContainer(webView, "plugin");
  EXPECT_EQ(document.focusedElement(), pluginContainer->element());
}

TEST_F(WebPluginContainerTest, IFramePluginDocumentNotFocused) {
  registerMockedURL("test.pdf", "application/pdf");
  registerMockedURL("iframe_pdf.html", "text/html");

  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "iframe_pdf.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->updateAllLifecyclePhases();

  WebDocument document = webView->mainFrame()->document();
  WebFrame* iframe = webView->mainFrame()->firstChild();
  EXPECT_TRUE(iframe->document().isPluginDocument());
  WebPluginContainer* pluginContainer =
      iframe->document().getElementById("plugin").pluginContainer();
  EXPECT_NE(document.focusedElement(), pluginContainer->element());
  EXPECT_NE(iframe->document().focusedElement(), pluginContainer->element());
}

TEST_F(WebPluginContainerTest, PrintOnePage) {
  registerMockedURL("test.pdf", "application/pdf");

  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "test.pdf", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->updateAllLifecyclePhases();
  runPendingTasks();
  WebFrame* frame = webView->mainFrame();

  WebPrintParams printParams;
  printParams.printContentArea.width = 500;
  printParams.printContentArea.height = 500;

  frame->printBegin(printParams);
  PaintRecorder recorder;
  frame->printPage(0, recorder.beginRecording(IntRect()));
  frame->printEnd();
  DCHECK(pluginWebFrameClient.printedAtLeastOnePage());
}

TEST_F(WebPluginContainerTest, PrintAllPages) {
  registerMockedURL("test.pdf", "application/pdf");

  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "test.pdf", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->updateAllLifecyclePhases();
  runPendingTasks();
  WebFrame* frame = webView->mainFrame();

  WebPrintParams printParams;
  printParams.printContentArea.width = 500;
  printParams.printContentArea.height = 500;

  frame->printBegin(printParams);
  PaintRecorder recorder;
  frame->printPagesWithBoundaries(recorder.beginRecording(IntRect()),
                                  WebSize());
  frame->printEnd();
  DCHECK(pluginWebFrameClient.printedAtLeastOnePage());
}

TEST_F(WebPluginContainerTest, LocalToWindowPointTest) {
  registerMockedURL("plugin_container.html");
  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebPluginContainer* pluginContainerOne =
      getWebPluginContainer(webView, WebString::fromUTF8("translated-plugin"));
  DCHECK(pluginContainerOne);
  WebPoint point1 = pluginContainerOne->localToRootFramePoint(WebPoint(0, 0));
  ASSERT_EQ(10, point1.x);
  ASSERT_EQ(10, point1.y);
  WebPoint point2 = pluginContainerOne->localToRootFramePoint(WebPoint(90, 90));
  ASSERT_EQ(100, point2.x);
  ASSERT_EQ(100, point2.y);

  WebPluginContainer* pluginContainerTwo =
      getWebPluginContainer(webView, WebString::fromUTF8("rotated-plugin"));
  DCHECK(pluginContainerTwo);
  WebPoint point3 = pluginContainerTwo->localToRootFramePoint(WebPoint(10, 0));
  ASSERT_EQ(0, point3.x);
  ASSERT_EQ(10, point3.y);
  WebPoint point4 = pluginContainerTwo->localToRootFramePoint(WebPoint(10, 10));
  ASSERT_EQ(-10, point4.x);
  ASSERT_EQ(10, point4.y);
}

// Verifies executing the command 'Copy' results in copying to the clipboard.
TEST_F(WebPluginContainerTest, Copy) {
  registerMockedURL("plugin_container.html");
  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  webView->mainFrame()
      ->document()
      .unwrap<Document>()
      ->body()
      ->getElementById("translated-plugin")
      ->focus();
  EXPECT_TRUE(webView->mainFrame()->toWebLocalFrame()->executeCommand("Copy"));
  EXPECT_EQ(WebString("x"), Platform::current()->clipboard()->readPlainText(
                                WebClipboard::Buffer()));
}

TEST_F(WebPluginContainerTest, CopyFromContextMenu) {
  registerMockedURL("plugin_container.html");
  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  auto event = FrameTestHelpers::createMouseEvent(WebMouseEvent::MouseDown,
                                                  WebMouseEvent::Button::Right,
                                                  WebPoint(30, 30), 0);
  event.clickCount = 1;

  // Make sure the right-click + Copy works in common scenario.
  webView->handleInputEvent(WebCoalescedInputEvent(event));
  EXPECT_TRUE(webView->mainFrame()->toWebLocalFrame()->executeCommand("Copy"));
  EXPECT_EQ(WebString("x"), Platform::current()->clipboard()->readPlainText(
                                WebClipboard::Buffer()));

  // Clear the clipboard buffer.
  Platform::current()->clipboard()->writePlainText(WebString(""));
  EXPECT_EQ(WebString(""), Platform::current()->clipboard()->readPlainText(
                               WebClipboard::Buffer()));

  // Now, let's try a more complex scenario:
  // 1) open the context menu. This will focus the plugin.
  webView->handleInputEvent(WebCoalescedInputEvent(event));
  // 2) document blurs the plugin, because it can.
  webView->clearFocusedElement();
  // 3) Copy should still operate on the context node, even though the focus had
  //    shifted.
  EXPECT_TRUE(webView->mainFrame()->toWebLocalFrame()->executeCommand("Copy"));
  EXPECT_EQ(WebString("x"), Platform::current()->clipboard()->readPlainText(
                                WebClipboard::Buffer()));
}

// Verifies |Ctrl-C| and |Ctrl-Insert| keyboard events, results in copying to
// the clipboard.
TEST_F(WebPluginContainerTest, CopyInsertKeyboardEventsTest) {
  registerMockedURL("plugin_container.html");
  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement pluginContainerOneElement =
      webView->mainFrame()->document().getElementById(
          WebString::fromUTF8("translated-plugin"));
  WebInputEvent::Modifiers modifierKey = static_cast<WebInputEvent::Modifiers>(
      WebInputEvent::ControlKey | WebInputEvent::NumLockOn |
      WebInputEvent::IsLeft);
#if OS(MACOSX)
  modifierKey = static_cast<WebInputEvent::Modifiers>(WebInputEvent::MetaKey |
                                                      WebInputEvent::NumLockOn |
                                                      WebInputEvent::IsLeft);
#endif
  WebKeyboardEvent webKeyboardEventC(WebInputEvent::RawKeyDown, modifierKey,
                                     WebInputEvent::TimeStampForTesting);
  webKeyboardEventC.windowsKeyCode = 67;
  KeyboardEvent* keyEventC = KeyboardEvent::create(webKeyboardEventC, 0);
  toWebPluginContainerImpl(pluginContainerOneElement.pluginContainer())
      ->handleEvent(keyEventC);
  EXPECT_EQ(WebString("x"), Platform::current()->clipboard()->readPlainText(
                                WebClipboard::Buffer()));

  // Clearing |Clipboard::Buffer()|.
  Platform::current()->clipboard()->writePlainText(WebString(""));
  EXPECT_EQ(WebString(""), Platform::current()->clipboard()->readPlainText(
                               WebClipboard::Buffer()));

  WebKeyboardEvent webKeyboardEventInsert(WebInputEvent::RawKeyDown,
                                          modifierKey,
                                          WebInputEvent::TimeStampForTesting);
  webKeyboardEventInsert.windowsKeyCode = 45;
  KeyboardEvent* keyEventInsert =
      KeyboardEvent::create(webKeyboardEventInsert, 0);
  toWebPluginContainerImpl(pluginContainerOneElement.pluginContainer())
      ->handleEvent(keyEventInsert);
  EXPECT_EQ(WebString("x"), Platform::current()->clipboard()->readPlainText(
                                WebClipboard::Buffer()));
}

// A class to facilitate testing that events are correctly received by plugins.
class EventTestPlugin : public FakeWebPlugin {
 public:
  EventTestPlugin(WebFrame* frame, const WebPluginParams& params)
      : FakeWebPlugin(frame, params),
        m_lastEventType(WebInputEvent::Undefined) {}

  WebInputEventResult handleInputEvent(const WebInputEvent& event,
                                       WebCursorInfo&) override {
    m_lastEventType = event.type();
    if (WebInputEvent::isMouseEventType(event.type()) ||
        event.type() == WebInputEvent::MouseWheel) {
      const WebMouseEvent& mouseEvent =
          static_cast<const WebMouseEvent&>(event);
      m_lastEventLocation = IntPoint(mouseEvent.x, mouseEvent.y);
    } else if (WebInputEvent::isTouchEventType(event.type())) {
      const WebTouchEvent& touchEvent =
          static_cast<const WebTouchEvent&>(event);
      if (touchEvent.touchesLength == 1) {
        m_lastEventLocation = IntPoint(touchEvent.touches[0].position.x,
                                       touchEvent.touches[0].position.y);
      } else {
        m_lastEventLocation = IntPoint();
      }
    }

    return WebInputEventResult::HandledSystem;
  }
  WebInputEvent::Type getLastInputEventType() { return m_lastEventType; }

  IntPoint getLastEventLocation() { return m_lastEventLocation; }

  void ClearLastEventType() { m_lastEventType = WebInputEvent::Undefined; }

 private:
  WebInputEvent::Type m_lastEventType;
  IntPoint m_lastEventLocation;
};

TEST_F(WebPluginContainerTest, GestureLongPressReachesPlugin) {
  registerMockedURL("plugin_container.html");
  CustomPluginWebFrameClient<EventTestPlugin>
      pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement pluginContainerOneElement =
      webView->mainFrame()->document().getElementById(
          WebString::fromUTF8("translated-plugin"));
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          pluginContainerOneElement.pluginContainer())
                          ->plugin();
  EventTestPlugin* testPlugin = static_cast<EventTestPlugin*>(plugin);

  WebGestureEvent event(WebInputEvent::GestureLongPress,
                        WebInputEvent::NoModifiers,
                        WebInputEvent::TimeStampForTesting);
  event.sourceDevice = WebGestureDeviceTouchscreen;

  // First, send an event that doesn't hit the plugin to verify that the
  // plugin doesn't receive it.
  event.x = 0;
  event.y = 0;

  webView->handleInputEvent(WebCoalescedInputEvent(event));
  runPendingTasks();

  EXPECT_EQ(WebInputEvent::Undefined, testPlugin->getLastInputEventType());

  // Next, send an event that does hit the plugin, and verify it does receive
  // it.
  WebRect rect = pluginContainerOneElement.boundsInViewport();
  event.x = rect.x + rect.width / 2;
  event.y = rect.y + rect.height / 2;

  webView->handleInputEvent(WebCoalescedInputEvent(event));
  runPendingTasks();

  EXPECT_EQ(WebInputEvent::GestureLongPress,
            testPlugin->getLastInputEventType());
}

TEST_F(WebPluginContainerTest, MouseWheelEventTranslated) {
  registerMockedURL("plugin_container.html");
  CustomPluginWebFrameClient<EventTestPlugin>
      pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement pluginContainerOneElement =
      webView->mainFrame()->document().getElementById(
          WebString::fromUTF8("translated-plugin"));
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          pluginContainerOneElement.pluginContainer())
                          ->plugin();
  EventTestPlugin* testPlugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseWheelEvent event(WebInputEvent::MouseWheel,
                           WebInputEvent::NoModifiers,
                           WebInputEvent::TimeStampForTesting);

  WebRect rect = pluginContainerOneElement.boundsInViewport();
  event.x = rect.x + rect.width / 2;
  event.y = rect.y + rect.height / 2;

  webView->handleInputEvent(WebCoalescedInputEvent(event));
  runPendingTasks();

  EXPECT_EQ(WebInputEvent::MouseWheel, testPlugin->getLastInputEventType());
  EXPECT_EQ(rect.width / 2, testPlugin->getLastEventLocation().x());
  EXPECT_EQ(rect.height / 2, testPlugin->getLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, TouchEventScrolled) {
  registerMockedURL("plugin_scroll.html");
  CustomPluginWebFrameClient<EventTestPlugin>
      plugin_web_frame_client;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.initializeAndLoad(
      m_baseURL + "plugin_scroll.html", true, &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->settings()->setPluginsEnabled(true);
  web_view->resize(WebSize(300, 300));
  web_view->updateAllLifecyclePhases();
  runPendingTasks();
  web_view->smoothScroll(0, 200, 0);
  web_view->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement plugin_container_one_element =
      web_view->mainFrame()->document().getElementById(
          WebString::fromUTF8("scrolled-plugin"));
  plugin_container_one_element.pluginContainer()->requestTouchEventType(
      WebPluginContainer::TouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.pluginContainer())
                          ->plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebTouchEvent event(WebInputEvent::TouchStart, WebInputEvent::NoModifiers,
                      WebInputEvent::TimeStampForTesting);
  event.touchesLength = 1;
  WebRect rect = plugin_container_one_element.boundsInViewport();
  event.touches[0].state = WebTouchPoint::StatePressed;
  event.touches[0].position =
      WebFloatPoint(rect.x + rect.width / 2, rect.y + rect.height / 2);

  web_view->handleInputEvent(WebCoalescedInputEvent(event));
  runPendingTasks();

  EXPECT_EQ(WebInputEvent::TouchStart, test_plugin->getLastInputEventType());
  EXPECT_EQ(rect.width / 2, test_plugin->getLastEventLocation().x());
  EXPECT_EQ(rect.height / 2, test_plugin->getLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, MouseWheelEventScrolled) {
  registerMockedURL("plugin_scroll.html");
  CustomPluginWebFrameClient<EventTestPlugin>
      plugin_web_frame_client;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.initializeAndLoad(
      m_baseURL + "plugin_scroll.html", true, &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->settings()->setPluginsEnabled(true);
  web_view->resize(WebSize(300, 300));
  web_view->updateAllLifecyclePhases();
  runPendingTasks();
  web_view->smoothScroll(0, 200, 0);
  web_view->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement plugin_container_one_element =
      web_view->mainFrame()->document().getElementById(
          WebString::fromUTF8("scrolled-plugin"));
  plugin_container_one_element.pluginContainer()->requestTouchEventType(
      WebPluginContainer::TouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.pluginContainer())
                          ->plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseWheelEvent event(WebInputEvent::MouseWheel,
                           WebInputEvent::NoModifiers,
                           WebInputEvent::TimeStampForTesting);

  WebRect rect = plugin_container_one_element.boundsInViewport();
  event.x = rect.x + rect.width / 2;
  event.y = rect.y + rect.height / 2;

  web_view->handleInputEvent(WebCoalescedInputEvent(event));
  runPendingTasks();

  EXPECT_EQ(WebInputEvent::MouseWheel, test_plugin->getLastInputEventType());
  EXPECT_EQ(rect.width / 2, test_plugin->getLastEventLocation().x());
  EXPECT_EQ(rect.height / 2, test_plugin->getLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, MouseEventScrolled) {
  registerMockedURL("plugin_scroll.html");
  CustomPluginWebFrameClient<EventTestPlugin>
      plugin_web_frame_client;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.initializeAndLoad(
      m_baseURL + "plugin_scroll.html", true, &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->settings()->setPluginsEnabled(true);
  web_view->resize(WebSize(300, 300));
  web_view->updateAllLifecyclePhases();
  runPendingTasks();
  web_view->smoothScroll(0, 200, 0);
  web_view->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement plugin_container_one_element =
      web_view->mainFrame()->document().getElementById(
          WebString::fromUTF8("scrolled-plugin"));
  plugin_container_one_element.pluginContainer()->requestTouchEventType(
      WebPluginContainer::TouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.pluginContainer())
                          ->plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseEvent event(WebInputEvent::MouseMove, WebInputEvent::NoModifiers,
                      WebInputEvent::TimeStampForTesting);

  WebRect rect = plugin_container_one_element.boundsInViewport();
  event.x = rect.x + rect.width / 2;
  event.y = rect.y + rect.height / 2;

  web_view->handleInputEvent(WebCoalescedInputEvent(event));
  runPendingTasks();

  EXPECT_EQ(WebInputEvent::MouseMove, test_plugin->getLastInputEventType());
  EXPECT_EQ(rect.width / 2, test_plugin->getLastEventLocation().x());
  EXPECT_EQ(rect.height / 2, test_plugin->getLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, MouseEventZoomed) {
  registerMockedURL("plugin_scroll.html");
  CustomPluginWebFrameClient<EventTestPlugin>
      plugin_web_frame_client;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.initializeAndLoad(
      m_baseURL + "plugin_scroll.html", true, &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->settings()->setPluginsEnabled(true);
  web_view->resize(WebSize(300, 300));
  web_view->setPageScaleFactor(2);
  web_view->smoothScroll(0, 300, 0);
  web_view->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement plugin_container_one_element =
      web_view->mainFrame()->document().getElementById(
          WebString::fromUTF8("scrolled-plugin"));
  plugin_container_one_element.pluginContainer()->requestTouchEventType(
      WebPluginContainer::TouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.pluginContainer())
                          ->plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseEvent event(WebInputEvent::MouseMove, WebInputEvent::NoModifiers,
                      WebInputEvent::TimeStampForTesting);

  WebRect rect = plugin_container_one_element.boundsInViewport();
  event.x = rect.x + rect.width / 2;
  event.y = rect.y + rect.height / 2;

  web_view->handleInputEvent(WebCoalescedInputEvent(event));
  runPendingTasks();

  // rect.width/height divided by 4 because the rect is in viewport bounds and
  // there is a scale of 2 set.
  EXPECT_EQ(WebInputEvent::MouseMove, test_plugin->getLastInputEventType());
  EXPECT_EQ(rect.width / 4, test_plugin->getLastEventLocation().x());
  EXPECT_EQ(rect.height / 4, test_plugin->getLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, MouseWheelEventZoomed) {
  registerMockedURL("plugin_scroll.html");
  CustomPluginWebFrameClient<EventTestPlugin>
      plugin_web_frame_client;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.initializeAndLoad(
      m_baseURL + "plugin_scroll.html", true, &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->settings()->setPluginsEnabled(true);
  web_view->resize(WebSize(300, 300));
  web_view->setPageScaleFactor(2);
  web_view->smoothScroll(0, 300, 0);
  web_view->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement plugin_container_one_element =
      web_view->mainFrame()->document().getElementById(
          WebString::fromUTF8("scrolled-plugin"));
  plugin_container_one_element.pluginContainer()->requestTouchEventType(
      WebPluginContainer::TouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.pluginContainer())
                          ->plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseWheelEvent event(WebInputEvent::MouseWheel,
                           WebInputEvent::NoModifiers,
                           WebInputEvent::TimeStampForTesting);

  WebRect rect = plugin_container_one_element.boundsInViewport();
  event.x = rect.x + rect.width / 2;
  event.y = rect.y + rect.height / 2;

  web_view->handleInputEvent(WebCoalescedInputEvent(event));
  runPendingTasks();

  // rect.width/height divided by 4 because the rect is in viewport bounds and
  // there is a scale of 2 set.
  EXPECT_EQ(WebInputEvent::MouseWheel, test_plugin->getLastInputEventType());
  EXPECT_EQ(rect.width / 4, test_plugin->getLastEventLocation().x());
  EXPECT_EQ(rect.height / 4, test_plugin->getLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, TouchEventZoomed) {
  registerMockedURL("plugin_scroll.html");
  CustomPluginWebFrameClient<EventTestPlugin>
      plugin_web_frame_client;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.initializeAndLoad(
      m_baseURL + "plugin_scroll.html", true, &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->settings()->setPluginsEnabled(true);
  web_view->resize(WebSize(300, 300));
  web_view->setPageScaleFactor(2);
  web_view->smoothScroll(0, 300, 0);
  web_view->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement plugin_container_one_element =
      web_view->mainFrame()->document().getElementById(
          WebString::fromUTF8("scrolled-plugin"));
  plugin_container_one_element.pluginContainer()->requestTouchEventType(
      WebPluginContainer::TouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.pluginContainer())
                          ->plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebTouchEvent event(WebInputEvent::TouchStart, WebInputEvent::NoModifiers,
                      WebInputEvent::TimeStampForTesting);
  event.touchesLength = 1;
  WebRect rect = plugin_container_one_element.boundsInViewport();

  event.touches[0].state = WebTouchPoint::StatePressed;
  event.touches[0].position =
      WebFloatPoint(rect.x + rect.width / 2, rect.y + rect.height / 2);

  web_view->handleInputEvent(WebCoalescedInputEvent(event));
  runPendingTasks();

  // rect.width/height divided by 4 because the rect is in viewport bounds and
  // there is a scale of 2 set.
  EXPECT_EQ(WebInputEvent::TouchStart, test_plugin->getLastInputEventType());
  EXPECT_EQ(rect.width / 4, test_plugin->getLastEventLocation().x());
  EXPECT_EQ(rect.height / 4, test_plugin->getLastEventLocation().y());
}

// Verify that isRectTopmost returns false when the document is detached.
TEST_F(WebPluginContainerTest, IsRectTopmostTest) {
  registerMockedURL("plugin_container.html");
  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebPluginContainerImpl* pluginContainerImpl = toWebPluginContainerImpl(
      getWebPluginContainer(webView, WebString::fromUTF8("translated-plugin")));
  pluginContainerImpl->setFrameRect(IntRect(0, 0, 300, 300));

  WebRect rect = pluginContainerImpl->element().boundsInViewport();
  EXPECT_TRUE(pluginContainerImpl->isRectTopmost(rect));

  // Cause the plugin's frame to be detached.
  webViewHelper.reset();

  EXPECT_FALSE(pluginContainerImpl->isRectTopmost(rect));
}

#define EXPECT_RECT_EQ(expected, actual)               \
  do {                                                 \
    const IntRect& actualRect = actual;                \
    EXPECT_EQ(expected.x(), actualRect.x());           \
    EXPECT_EQ(expected.y(), actualRect.y());           \
    EXPECT_EQ(expected.width(), actualRect.width());   \
    EXPECT_EQ(expected.height(), actualRect.height()); \
  } while (false)

TEST_F(WebPluginContainerTest, ClippedRectsForIframedElement) {
  registerMockedURL("plugin_container.html");
  registerMockedURL("plugin_containing_page.html");

  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_containing_page.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement pluginElement =
      webView->mainFrame()->firstChild()->document().getElementById(
          "translated-plugin");
  WebPluginContainerImpl* pluginContainerImpl =
      toWebPluginContainerImpl(pluginElement.pluginContainer());

  DCHECK(pluginContainerImpl);

  IntRect windowRect, clipRect, unobscuredRect;
  Vector<IntRect> cutOutRects;
  calculateGeometry(pluginContainerImpl, windowRect, clipRect, unobscuredRect,
                    cutOutRects);
  EXPECT_RECT_EQ(IntRect(20, 220, 40, 40), windowRect);
  EXPECT_RECT_EQ(IntRect(0, 0, 40, 40), clipRect);
  EXPECT_RECT_EQ(IntRect(0, 0, 40, 40), unobscuredRect);

  // Cause the plugin's frame to be detached.
  webViewHelper.reset();
}

TEST_F(WebPluginContainerTest, ClippedRectsForSubpixelPositionedPlugin) {
  registerMockedURL("plugin_container.html");

  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement pluginElement = webView->mainFrame()->document().getElementById(
      "subpixel-positioned-plugin");
  WebPluginContainerImpl* pluginContainerImpl =
      toWebPluginContainerImpl(pluginElement.pluginContainer());

  DCHECK(pluginContainerImpl);

  IntRect windowRect, clipRect, unobscuredRect;
  Vector<IntRect> cutOutRects;

  calculateGeometry(pluginContainerImpl, windowRect, clipRect, unobscuredRect,
                    cutOutRects);
  EXPECT_RECT_EQ(IntRect(0, 0, 40, 40), windowRect);
  EXPECT_RECT_EQ(IntRect(0, 0, 40, 40), clipRect);
  EXPECT_RECT_EQ(IntRect(0, 0, 40, 40), unobscuredRect);

  // Cause the plugin's frame to be detached.
  webViewHelper.reset();
}

TEST_F(WebPluginContainerTest, TopmostAfterDetachTest) {
  static WebRect topmostRect(10, 10, 40, 40);

  // Plugin that checks isRectTopmost in destroy().
  class TopmostPlugin : public FakeWebPlugin {
   public:
    TopmostPlugin(WebFrame* frame, const WebPluginParams& params)
        : FakeWebPlugin(frame, params) {}

    bool isRectTopmost() { return container()->isRectTopmost(topmostRect); }

    void destroy() override {
      // In destroy, isRectTopmost is no longer valid.
      EXPECT_FALSE(container()->isRectTopmost(topmostRect));
      FakeWebPlugin::destroy();
    }
  };

  registerMockedURL("plugin_container.html");
  CustomPluginWebFrameClient<TopmostPlugin>
      pluginWebFrameClient;  // Must outlive webViewHelper.
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebPluginContainerImpl* pluginContainerImpl = toWebPluginContainerImpl(
      getWebPluginContainer(webView, WebString::fromUTF8("translated-plugin")));
  pluginContainerImpl->setFrameRect(IntRect(0, 0, 300, 300));

  EXPECT_TRUE(pluginContainerImpl->isRectTopmost(topmostRect));

  TopmostPlugin* testPlugin =
      static_cast<TopmostPlugin*>(pluginContainerImpl->plugin());
  EXPECT_TRUE(testPlugin->isRectTopmost());

  // Cause the plugin's frame to be detached.
  webViewHelper.reset();

  EXPECT_FALSE(pluginContainerImpl->isRectTopmost(topmostRect));
}

namespace {

class CompositedPlugin : public FakeWebPlugin {
 public:
  CompositedPlugin(WebLocalFrame* frame, const WebPluginParams& params)
      : FakeWebPlugin(frame, params),
        m_layer(Platform::current()->compositorSupport()->createLayer()) {}

  WebLayer* getWebLayer() const { return m_layer.get(); }

  // WebPlugin

  bool initialize(WebPluginContainer* container) override {
    if (!FakeWebPlugin::initialize(container))
      return false;
    container->setWebLayer(m_layer.get());
    return true;
  }

  void destroy() override {
    container()->setWebLayer(nullptr);
    FakeWebPlugin::destroy();
  }

 private:
  std::unique_ptr<WebLayer> m_layer;
};

}  // namespace

TEST_F(WebPluginContainerTest, CompositedPluginSPv2) {
  ScopedSlimmingPaintV2ForTest enableSPv2(true);
  registerMockedURL("plugin.html");
  CustomPluginWebFrameClient<CompositedPlugin> webFrameClient;
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(m_baseURL + "plugin.html",
                                                     true, &webFrameClient);
  ASSERT_TRUE(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(800, 600));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebPluginContainerImpl* container = static_cast<WebPluginContainerImpl*>(
      getWebPluginContainer(webView, WebString::fromUTF8("plugin")));
  ASSERT_TRUE(container);
  Element* element = static_cast<Element*>(container->element());
  const auto* plugin =
      static_cast<const CompositedPlugin*>(container->plugin());

  std::unique_ptr<PaintController> paintController = PaintController::create();
  PropertyTreeState propertyTreeState(TransformPaintPropertyNode::root(),
                                      ClipPaintPropertyNode::root(),
                                      EffectPaintPropertyNode::root());
  PaintChunkProperties properties(propertyTreeState);

  paintController->updateCurrentPaintChunkProperties(nullptr, properties);
  GraphicsContext graphicsContext(*paintController);
  container->paint(graphicsContext, CullRect(IntRect(10, 10, 400, 300)));
  paintController->commitNewDisplayItems();

  const auto& displayItems =
      paintController->paintArtifact().getDisplayItemList();
  ASSERT_EQ(1u, displayItems.size());
  EXPECT_EQ(element->layoutObject(), &displayItems[0].client());
  ASSERT_EQ(DisplayItem::kForeignLayerPlugin, displayItems[0].getType());
  const auto& foreignLayerDisplayItem =
      static_cast<const ForeignLayerDisplayItem&>(displayItems[0]);
  EXPECT_EQ(plugin->getWebLayer()->ccLayer(), foreignLayerDisplayItem.layer());
}

TEST_F(WebPluginContainerTest, NeedsWheelEvents) {
  registerMockedURL("plugin_container.html");
  TestPluginWebFrameClient pluginWebFrameClient;  // Must outlive webViewHelper
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "plugin_container.html", true, &pluginWebFrameClient);
  DCHECK(webView);
  webView->settings()->setPluginsEnabled(true);
  webView->resize(WebSize(300, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebElement pluginContainerOneElement =
      webView->mainFrame()->document().getElementById(
          WebString::fromUTF8("translated-plugin"));
  pluginContainerOneElement.pluginContainer()->setWantsWheelEvents(true);

  runPendingTasks();
  EXPECT_TRUE(
      webView->page()->frameHost().eventHandlerRegistry().hasEventHandlers(
          EventHandlerRegistry::WheelEventBlocking));
}

}  // namespace blink
