/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/dom/ClientRect.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/api/LayoutViewItem.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebCache.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "public/web/WebHitTestResult.h"
#include "public/web/WebTouchAction.h"
#include "public/web/WebView.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWidgetClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebFrameImplBase.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"

using blink::testing::runPendingTasks;

namespace blink {

class TouchActionTrackingWebWidgetClient
    : public FrameTestHelpers::TestWebWidgetClient {
 public:
  TouchActionTrackingWebWidgetClient()
      : m_actionSetCount(0), m_action(WebTouchActionAuto) {}

  // WebWidgetClient methods
  void setTouchAction(WebTouchAction touchAction) override {
    m_actionSetCount++;
    m_action = touchAction;
  }

  // Local methods
  void reset() {
    m_actionSetCount = 0;
    m_action = WebTouchActionAuto;
  }

  int touchActionSetCount() { return m_actionSetCount; }

  WebTouchAction lastTouchAction() { return m_action; }

 private:
  int m_actionSetCount;
  WebTouchAction m_action;
};

const int kfakeTouchId = 7;

class TouchActionTest : public ::testing::Test {
 public:
  TouchActionTest() : m_baseURL("http://www.test.com/") {
    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL),
                                                 "touch-action-tests.css");
    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL),
                                                 "touch-action-tests.js");
    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL),
                                                 "white-1x1.png");
  }

  void TearDown() override {
    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    WebCache::clear();
  }

 protected:
  void runTouchActionTest(std::string file);
  void runShadowDOMTest(std::string file);
  void runIFrameTest(std::string file);
  void sendTouchEvent(WebView*, WebInputEvent::Type, IntPoint clientPoint);
  WebView* setupTest(std::string file, TouchActionTrackingWebWidgetClient&);
  void runTestOnTree(ContainerNode* root,
                     WebView*,
                     TouchActionTrackingWebWidgetClient&);

  std::string m_baseURL;
  FrameTestHelpers::WebViewHelper m_webViewHelper;
};

void TouchActionTest::runTouchActionTest(std::string file) {
  TouchActionTrackingWebWidgetClient client;

  // runTouchActionTest() loads a document in a frame, setting up a
  // nested message loop. Should any Oilpan GC happen while it is in
  // effect, the implicit assumption that we're outside any event
  // loop (=> there being no pointers on the stack needing scanning)
  // when that GC strikes will no longer hold.
  //
  // To ensure that the references on the stack are also traced, we
  // turn them into persistent, stack allocated references. This
  // workaround is sufficient to handle this artificial test
  // scenario.
  WebView* webView = setupTest(file, client);

  Persistent<Document> document =
      static_cast<Document*>(webView->mainFrame()->document());
  runTestOnTree(document.get(), webView, client);

  // Explicitly reset to break dependency on locally scoped client.
  m_webViewHelper.reset();
}

void TouchActionTest::runShadowDOMTest(std::string file) {
  TouchActionTrackingWebWidgetClient client;

  WebView* webView = setupTest(file, client);

  DummyExceptionStateForTesting es;

  // Oilpan: see runTouchActionTest() comment why these are persistent
  // references.
  Persistent<Document> document =
      static_cast<Document*>(webView->mainFrame()->document());
  Persistent<StaticElementList> hostNodes =
      document->querySelectorAll("[shadow-host]", es);
  ASSERT_FALSE(es.hadException());
  ASSERT_GE(hostNodes->length(), 1u);

  for (unsigned index = 0; index < hostNodes->length(); index++) {
    ShadowRoot* shadowRoot = hostNodes->item(index)->openShadowRoot();
    runTestOnTree(shadowRoot, webView, client);
  }

  // Projections show up in the main document.
  runTestOnTree(document.get(), webView, client);

  // Explicitly reset to break dependency on locally scoped client.
  m_webViewHelper.reset();
}

void TouchActionTest::runIFrameTest(std::string file) {
  TouchActionTrackingWebWidgetClient client;

  WebView* webView = setupTest(file, client);
  WebFrame* curFrame = webView->mainFrame()->firstChild();
  ASSERT_TRUE(curFrame);

  for (; curFrame; curFrame = curFrame->nextSibling()) {
    // Oilpan: see runTouchActionTest() comment why these are persistent
    // references.
    Persistent<Document> contentDoc =
        static_cast<Document*>(curFrame->document());
    runTestOnTree(contentDoc.get(), webView, client);
  }

  // Explicitly reset to break dependency on locally scoped client.
  m_webViewHelper.reset();
}

WebView* TouchActionTest::setupTest(
    std::string file,
    TouchActionTrackingWebWidgetClient& client) {
  URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL),
                                               WebString::fromUTF8(file));
  // Note that JavaScript must be enabled for shadow DOM tests.
  WebView* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + file, true, 0, 0, &client);

  // Set size to enable hit testing, and avoid line wrapping for consistency
  // with browser.
  webView->resize(WebSize(800, 1200));

  // Scroll to verify the code properly transforms windows to client co-ords.
  const int kScrollOffset = 100;
  Document* document = static_cast<Document*>(webView->mainFrame()->document());
  document->frame()->view()->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(0, kScrollOffset), ProgrammaticScroll);

  return webView;
}

IntRect windowClipRect(const FrameView& frameView) {
  LayoutRect clipRect(
      LayoutPoint(),
      LayoutSize(frameView.visibleContentSize(ExcludeScrollbars)));
  frameView.layoutViewItem().mapToVisualRectInAncestorSpace(
      &frameView.layoutView()->containerForPaintInvalidation(), clipRect);
  return enclosingIntRect(clipRect);
}

void TouchActionTest::runTestOnTree(
    ContainerNode* root,
    WebView* webView,
    TouchActionTrackingWebWidgetClient& client) {
  // Find all elements to test the touch-action of in the document.
  DummyExceptionStateForTesting es;

  // Oilpan: see runTouchActionTest() comment why these are persistent
  // references.
  Persistent<StaticElementList> elements =
      root->querySelectorAll("[expected-action]", es);
  ASSERT_FALSE(es.hadException());

  for (unsigned index = 0; index < elements->length(); index++) {
    Element* element = elements->item(index);
    element->scrollIntoViewIfNeeded();

    std::string failureContext("Test case: ");
    if (element->hasID()) {
      failureContext.append(element->getIdAttribute().ascii().data());
    } else if (element->firstChild()) {
      failureContext.append("\"");
      failureContext.append(element->firstChild()
                                ->textContent(false)
                                .stripWhiteSpace()
                                .ascii()
                                .data());
      failureContext.append("\"");
    } else {
      failureContext += "<missing ID>";
    }

    // Run each test three times at different positions in the element.
    // Note that we don't want the bounding box because our tests sometimes have
    // elements with multiple border boxes with other elements in between. Use
    // the first border box (which we can easily visualize in a browser for
    // debugging).
    Persistent<ClientRectList> rects = element->getClientRects();
    ASSERT_GE(rects->length(), 0u) << failureContext;
    Persistent<ClientRect> r = rects->item(0);
    FloatRect clientFloatRect =
        FloatRect(r->left(), r->top(), r->width(), r->height());
    IntRect clientRect = enclosedIntRect(clientFloatRect);
    for (int locIdx = 0; locIdx < 3; locIdx++) {
      IntPoint framePoint;
      std::stringstream contextStream;
      contextStream << failureContext << " (";
      switch (locIdx) {
        case 0:
          framePoint = clientRect.center();
          contextStream << "center";
          break;
        case 1:
          framePoint = clientRect.location();
          contextStream << "top-left";
          break;
        case 2:
          framePoint = clientRect.maxXMaxYCorner();
          framePoint.move(-1, -1);
          contextStream << "bottom-right";
          break;
        default:
          FAIL() << "Invalid location index.";
      }

      IntPoint windowPoint =
          root->document().frame()->view()->convertToRootFrame(framePoint);
      contextStream << "=" << windowPoint.x() << "," << windowPoint.y() << ").";
      std::string failureContextPos = contextStream.str();

      LocalFrame* mainFrame =
          static_cast<LocalFrame*>(webView->mainFrame()->toImplBase()->frame());
      FrameView* mainFrameView = mainFrame->view();
      IntRect visibleRect = windowClipRect(*mainFrameView);
      ASSERT_TRUE(visibleRect.contains(windowPoint))
          << failureContextPos
          << " Test point not contained in visible area: " << visibleRect.x()
          << "," << visibleRect.y() << "-" << visibleRect.maxX() << ","
          << visibleRect.maxY();

      // First validate that a hit test at this point will really hit the
      // element we intended. This is the easiest way for a test to be broken,
      // but has nothing really to do with touch action.  Note that we can't use
      // WebView's hit test API because it doesn't look into shadow DOM.
      IntPoint docPoint(mainFrameView->frameToContents(windowPoint));
      HitTestResult result = mainFrame->eventHandler().hitTestResultAtPoint(
          docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
      ASSERT_EQ(element, result.innerElement())
          << "Unexpected hit test result " << failureContextPos
          << "  Got element: \""
          << result.innerElement()
                 ->outerHTML()
                 .stripWhiteSpace()
                 .left(80)
                 .ascii()
                 .data()
          << "\"" << std::endl
          << "Document render tree:" << std::endl
          << externalRepresentation(root->document().frame()).utf8().data();

      // Now send the touch event and check any touch action result.
      sendTouchEvent(webView, WebInputEvent::TouchStart, windowPoint);

      AtomicString expectedAction = element->getAttribute("expected-action");
      if (expectedAction == "auto") {
        // Auto is the default - no action set.
        EXPECT_EQ(0, client.touchActionSetCount()) << failureContextPos;
        EXPECT_EQ(WebTouchActionAuto, client.lastTouchAction())
            << failureContextPos;
      } else {
        // Should have received exactly one touch action.
        EXPECT_EQ(1, client.touchActionSetCount()) << failureContextPos;
        if (client.touchActionSetCount()) {
          if (expectedAction == "none") {
            EXPECT_EQ(WebTouchActionNone, client.lastTouchAction())
                << failureContextPos;
          } else if (expectedAction == "pan-x") {
            EXPECT_EQ(WebTouchActionPanX, client.lastTouchAction())
                << failureContextPos;
          } else if (expectedAction == "pan-y") {
            EXPECT_EQ(WebTouchActionPanY, client.lastTouchAction())
                << failureContextPos;
          } else if (expectedAction == "pan-x-y") {
            EXPECT_EQ((WebTouchActionPan), client.lastTouchAction())
                << failureContextPos;
          } else if (expectedAction == "manipulation") {
            EXPECT_EQ((WebTouchActionManipulation), client.lastTouchAction())
                << failureContextPos;
          } else {
            FAIL() << "Unrecognized expected-action \""
                   << expectedAction.ascii().data() << "\" "
                   << failureContextPos;
          }
        }
      }

      // Reset webview touch state.
      client.reset();
      sendTouchEvent(webView, WebInputEvent::TouchCancel, windowPoint);
      EXPECT_EQ(0, client.touchActionSetCount());
    }
  }
}
void TouchActionTest::sendTouchEvent(WebView* webView,
                                     WebInputEvent::Type type,
                                     IntPoint clientPoint) {
  ASSERT_TRUE(type == WebInputEvent::TouchStart ||
              type == WebInputEvent::TouchCancel);

  WebTouchEvent webTouchEvent(type, WebInputEvent::NoModifiers,
                              WebInputEvent::TimeStampForTesting);
  if (type == WebInputEvent::TouchCancel)
    webTouchEvent.dispatchType = WebInputEvent::EventNonBlocking;
  webTouchEvent.touchesLength = 1;
  webTouchEvent.touches[0].state =
      (type == WebInputEvent::TouchStart ? WebTouchPoint::StatePressed
                                         : WebTouchPoint::StateCancelled);
  webTouchEvent.touches[0].id = kfakeTouchId;
  webTouchEvent.touches[0].screenPosition.x = clientPoint.x();
  webTouchEvent.touches[0].screenPosition.y = clientPoint.y();
  webTouchEvent.touches[0].position.x = clientPoint.x();
  webTouchEvent.touches[0].position.y = clientPoint.y();
  webTouchEvent.touches[0].radiusX = 10;
  webTouchEvent.touches[0].radiusY = 10;
  webTouchEvent.touches[0].force = 1.0;

  webView->handleInputEvent(webTouchEvent);
  runPendingTasks();
}

// crbug.com/411038
TEST_F(TouchActionTest, Simple) {
  runTouchActionTest("touch-action-simple.html");
}

TEST_F(TouchActionTest, Overflow) {
  runTouchActionTest("touch-action-overflow.html");
}

TEST_F(TouchActionTest, IFrame) {
  runIFrameTest("touch-action-iframe.html");
}

TEST_F(TouchActionTest, ShadowDOM) {
  runShadowDOMTest("touch-action-shadow-dom.html");
}

TEST_F(TouchActionTest, Pan) {
  runTouchActionTest("touch-action-pan.html");
}

}  // namespace blink
