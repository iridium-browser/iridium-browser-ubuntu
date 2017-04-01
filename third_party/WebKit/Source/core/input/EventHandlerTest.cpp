// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/EventHandler.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/PlatformMouseEvent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class EventHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Page& page() const { return m_dummyPageHolder->page(); }
  Document& document() const { return m_dummyPageHolder->document(); }

  void setHtmlInnerHTML(const char* htmlContent);

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

class TapEventBuilder : public WebGestureEvent {
 public:
  TapEventBuilder(IntPoint position, int tapCount)
      : WebGestureEvent(WebInputEvent::GestureTap,
                        WebInputEvent::NoModifiers,
                        TimeTicks::Now().InSeconds()) {
    x = globalX = position.x();
    y = globalY = position.y();
    sourceDevice = WebGestureDeviceTouchscreen;
    data.tap.tapCount = tapCount;
    data.tap.width = 5;
    data.tap.height = 5;
    m_frameScale = 1;
  }
};

void EventHandlerTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(300, 400));
}

void EventHandlerTest::setHtmlInnerHTML(const char* htmlContent) {
  document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent));
  document().view()->updateAllLifecyclePhases();
}

TEST_F(EventHandlerTest, dragSelectionAfterScroll) {
  setHtmlInnerHTML(
      "<style> body { margin: 0px; } .upper { width: 300px; height: 400px; }"
      ".lower { margin: 0px; width: 300px; height: 400px; } .line { display: "
      "block; width: 300px; height: 30px; } </style>"
      "<div class='upper'></div>"
      "<div class='lower'>"
      "<span class='line'>Line 1</span><span class='line'>Line 2</span><span "
      "class='line'>Line 3</span><span class='line'>Line 4</span><span "
      "class='line'>Line 5</span>"
      "<span class='line'>Line 6</span><span class='line'>Line 7</span><span "
      "class='line'>Line 8</span><span class='line'>Line 9</span><span "
      "class='line'>Line 10</span>"
      "</div>");

  FrameView* frameView = document().view();
  frameView->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(0, 400), ProgrammaticScroll);

  PlatformMouseEvent mouseDownEvent(
      IntPoint(0, 0), IntPoint(100, 200), WebPointerProperties::Button::Left,
      PlatformEvent::MousePressed, 1, PlatformEvent::Modifiers::LeftButtonDown,
      TimeTicks::Now());
  document().frame()->eventHandler().handleMousePressEvent(mouseDownEvent);

  PlatformMouseEvent mouseMoveEvent(
      IntPoint(100, 50), IntPoint(200, 250), WebPointerProperties::Button::Left,
      PlatformEvent::MouseMoved, 1, PlatformEvent::Modifiers::LeftButtonDown,
      TimeTicks::Now());
  document().frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveEvent, Vector<PlatformMouseEvent>());

  page().autoscrollController().animate(WTF::monotonicallyIncreasingTime());
  page().animator().serviceScriptedAnimations(
      WTF::monotonicallyIncreasingTime());

  PlatformMouseEvent mouseUpEvent(
      IntPoint(100, 50), IntPoint(200, 250), WebPointerProperties::Button::Left,
      PlatformEvent::MouseReleased, 1, static_cast<PlatformEvent::Modifiers>(0),
      TimeTicks::Now());
  document().frame()->eventHandler().handleMouseReleaseEvent(mouseUpEvent);

  FrameSelection& selection = document().frame()->selection();
  ASSERT_TRUE(selection.isRange());
  Range* range =
      createRange(selection.selection().toNormalizedEphemeralRange());
  ASSERT_TRUE(range);
  EXPECT_EQ("Line 1\nLine 2", range->text());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTap) {
  setHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<body contenteditable='true'><span class='line' id='line'>One Two "
      "Three</span></body>");

  FrameSelection& selection = document().frame()->selection();
  Node* line = document().getElementById("line")->firstChild();

  TapEventBuilder singleTapEvent(IntPoint(0, 0), 1);
  document().frame()->eventHandler().handleGestureEvent(singleTapEvent);
  ASSERT_TRUE(selection.isCaret());
  EXPECT_EQ(Position(line, 0), selection.start());

  // Multi-tap events on editable elements should trigger selection, just
  // like multi-click events.
  TapEventBuilder doubleTapEvent(IntPoint(0, 0), 2);
  document().frame()->eventHandler().handleGestureEvent(doubleTapEvent);
  ASSERT_TRUE(selection.isRange());
  EXPECT_EQ(Position(line, 0), selection.start());
  if (document().frame()->editor().isSelectTrailingWhitespaceEnabled()) {
    EXPECT_EQ(Position(line, 4), selection.end());
    EXPECT_EQ("One ", WebString(selection.selectedText()).utf8());
  } else {
    EXPECT_EQ(Position(line, 3), selection.end());
    EXPECT_EQ("One", WebString(selection.selectedText()).utf8());
  }

  TapEventBuilder tripleTapEvent(IntPoint(0, 0), 3);
  document().frame()->eventHandler().handleGestureEvent(tripleTapEvent);
  ASSERT_TRUE(selection.isRange());
  EXPECT_EQ(Position(line, 0), selection.start());
  EXPECT_EQ(Position(line, 13), selection.end());
  EXPECT_EQ("One Two Three", WebString(selection.selectedText()).utf8());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTapDisabledIfNotEditable) {
  setHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<span class='line' id='line'>One Two Three</span>");

  FrameSelection& selection = document().frame()->selection();
  Node* line = document().getElementById("line")->firstChild();

  TapEventBuilder singleTapEvent(IntPoint(0, 0), 1);
  document().frame()->eventHandler().handleGestureEvent(singleTapEvent);
  ASSERT_TRUE(selection.isCaret());
  EXPECT_EQ(Position(line, 0), selection.start());

  // As the text is readonly, multi-tap events should not trigger selection.
  TapEventBuilder doubleTapEvent(IntPoint(0, 0), 2);
  document().frame()->eventHandler().handleGestureEvent(doubleTapEvent);
  ASSERT_TRUE(selection.isCaret());
  EXPECT_EQ(Position(line, 0), selection.start());

  TapEventBuilder tripleTapEvent(IntPoint(0, 0), 3);
  document().frame()->eventHandler().handleGestureEvent(tripleTapEvent);
  ASSERT_TRUE(selection.isCaret());
  EXPECT_EQ(Position(line, 0), selection.start());
}

TEST_F(EventHandlerTest, draggedInlinePositionTest) {
  setHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      ".line { font-family: sans-serif; background: blue; width: 300px; "
      "height: 30px; font-size: 40px; margin-left: 250px; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<span class='line' draggable='true'>abcd</span>"
      "</div>");
  PlatformMouseEvent mouseDownEvent(
      IntPoint(262, 29), IntPoint(329, 67), WebPointerProperties::Button::Left,
      PlatformEvent::MousePressed, 1, PlatformEvent::Modifiers::LeftButtonDown,
      TimeTicks::Now());
  document().frame()->eventHandler().handleMousePressEvent(mouseDownEvent);

  PlatformMouseEvent mouseMoveEvent(
      IntPoint(618, 298), IntPoint(685, 436),
      WebPointerProperties::Button::Left, PlatformEvent::MouseMoved, 1,
      PlatformEvent::Modifiers::LeftButtonDown, TimeTicks::Now());
  document().frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveEvent, Vector<PlatformMouseEvent>());

  EXPECT_EQ(
      IntPoint(12, 29),
      document().frame()->eventHandler().dragDataTransferLocationForTesting());
}

TEST_F(EventHandlerTest, draggedSVGImagePositionTest) {
  setHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      "[draggable] {"
      "-webkit-user-select: none; user-select: none; -webkit-user-drag: "
      "element; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<svg width='500' height='500'>"
      "<rect x='100' y='100' width='100px' height='100px' fill='blue' "
      "draggable='true'/>"
      "</svg>"
      "</div>");
  PlatformMouseEvent mouseDownEvent(
      IntPoint(145, 144), IntPoint(212, 282),
      WebPointerProperties::Button::Left, PlatformEvent::MousePressed, 1,
      PlatformEvent::Modifiers::LeftButtonDown, TimeTicks::Now());
  document().frame()->eventHandler().handleMousePressEvent(mouseDownEvent);

  PlatformMouseEvent mouseMoveEvent(
      IntPoint(618, 298), IntPoint(685, 436),
      WebPointerProperties::Button::Left, PlatformEvent::MouseMoved, 1,
      PlatformEvent::Modifiers::LeftButtonDown, TimeTicks::Now());
  document().frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveEvent, Vector<PlatformMouseEvent>());

  EXPECT_EQ(
      IntPoint(45, 44),
      document().frame()->eventHandler().dragDataTransferLocationForTesting());
}

// Regression test for http://crbug.com/641403 to verify we use up-to-date
// layout tree for dispatching "contextmenu" event.
TEST_F(EventHandlerTest, sendContextMenuEventWithHover) {
  setHtmlInnerHTML(
      "<style>*:hover { color: red; }</style>"
      "<div>foo</div>");
  document().settings()->setScriptEnabled(true);
  Element* script = document().createElement("script");
  script->setInnerHTML(
      "document.addEventListener('contextmenu', event => "
      "event.preventDefault());");
  document().body()->appendChild(script);
  document().updateStyleAndLayout();
  document().frame()->selection().setSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position(document().body(), 0))
          .build());
  PlatformMouseEvent mouseDownEvent(
      IntPoint(0, 0), IntPoint(100, 200), WebPointerProperties::Button::Right,
      PlatformEvent::MousePressed, 1, PlatformEvent::Modifiers::RightButtonDown,
      TimeTicks::Now());
  EXPECT_EQ(
      WebInputEventResult::HandledApplication,
      document().frame()->eventHandler().sendContextMenuEvent(mouseDownEvent));
}

}  // namespace blink
