// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller_client_aura.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/touch_selection/touch_selection_controller_test_api.h"

namespace content {
namespace {

bool JSONToPoint(const std::string& str, gfx::PointF* point) {
  scoped_ptr<base::Value> value = base::JSONReader::Read(str);
  if (!value)
    return false;
  base::DictionaryValue* root;
  if (!value->GetAsDictionary(&root))
    return false;
  double x, y;
  if (!root->GetDouble("x", &x))
    return false;
  if (!root->GetDouble("y", &y))
    return false;
  point->set_x(x);
  point->set_y(y);
  return true;
}

// A mock touch selection menu runner to use whenever a default one is not
// installed.
class TestTouchSelectionMenuRunner : public ui::TouchSelectionMenuRunner {
 public:
  TestTouchSelectionMenuRunner() : menu_opened_(false) {}
  ~TestTouchSelectionMenuRunner() override {}

 private:
  void OpenMenu(ui::TouchSelectionMenuClient* client,
                const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size,
                aura::Window* context) override {
    menu_opened_ = true;
  }

  void CloseMenu() override { menu_opened_ = false; }

  bool IsRunning() const override { return menu_opened_; }

  bool menu_opened_;

  DISALLOW_COPY_AND_ASSIGN(TestTouchSelectionMenuRunner);
};

}  // namespace

class TestTouchSelectionControllerClientAura
    : public TouchSelectionControllerClientAura {
 public:
  explicit TestTouchSelectionControllerClientAura(
      RenderWidgetHostViewAura* rwhva)
      : TouchSelectionControllerClientAura(rwhva),
        expected_event_(ui::SELECTION_HANDLES_SHOWN) {
    show_quick_menu_immediately_for_test_ = true;
  }

  ~TestTouchSelectionControllerClientAura() override {}

  void InitWaitForSelectionEvent(ui::SelectionEventType expected_event) {
    DCHECK(!run_loop_);
    expected_event_ = expected_event;
    run_loop_.reset(new base::RunLoop());
  }

  void Wait() {
    DCHECK(run_loop_);
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  // TouchSelectionControllerClientAura:
  void OnSelectionEvent(ui::SelectionEventType event) override {
    TouchSelectionControllerClientAura::OnSelectionEvent(event);
    if (run_loop_ && event == expected_event_)
      run_loop_->Quit();
  }

  bool IsCommandIdEnabled(int command_id) const override {
    // Return true so that quick menu has something to show.
    return true;
  }

  ui::SelectionEventType expected_event_;
  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestTouchSelectionControllerClientAura);
};

class TouchSelectionControllerClientAuraTest : public ContentBrowserTest {
 public:
  TouchSelectionControllerClientAuraTest() {}
  ~TouchSelectionControllerClientAuraTest() override {}

 protected:
  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window.  Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(test_server()->Start());
    GURL test_url(test_server()->GetURL(url));
    NavigateToURL(shell(), test_url);
    aura::Window* content = shell()->web_contents()->GetContentNativeView();
    content->GetHost()->SetBounds(gfx::Rect(800, 600));
  }

  bool GetPointInsideText(gfx::PointF* point) {
    std::string str;
    if (ExecuteScriptAndExtractString(shell()->web_contents()->GetMainFrame(),
                                      "get_point_inside_text()", &str)) {
      return JSONToPoint(str, point);
    }
    return false;
  }

  bool GetPointInsideTextfield(gfx::PointF* point) {
    std::string str;
    if (ExecuteScriptAndExtractString(shell()->web_contents()->GetMainFrame(),
                                      "get_point_inside_textfield()", &str)) {
      return JSONToPoint(str, point);
    }
    return false;
  }

 private:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    if (!ui::TouchSelectionMenuRunner::GetInstance())
      menu_runner_.reset(new TestTouchSelectionMenuRunner);
  }

  void TearDownOnMainThread() override {
    menu_runner_ = nullptr;
    ContentBrowserTest::TearDownOnMainThread();
  }

  scoped_ptr<TestTouchSelectionMenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionControllerClientAuraTest);
};

// Tests if long-pressing on a text brings up selection handles and the quick
// menu properly.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest, BasicSelection) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContents* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  TestTouchSelectionControllerClientAura* selection_controller_client =
      new TestTouchSelectionControllerClientAura(rwhva);
  rwhva->SetSelectionControllerClientForTest(
      make_scoped_ptr(selection_controller_client));

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Long-press on the text and wait for handles to appear.
  selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideText(&point));
  ui::GestureEvent long_press(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client->Wait();

  // Check if selection is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests if tapping in a textfield brings up the insertion handle and the quick
// menu properly.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest, BasicInsertion) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContents* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  TestTouchSelectionControllerClientAura* selection_controller_client =
      new TestTouchSelectionControllerClientAura(rwhva);
  rwhva->SetSelectionControllerClientForTest(
      make_scoped_ptr(selection_controller_client));

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap inside the textfield and wait for the insertion handle to appear.
  selection_controller_client->InitWaitForSelectionEvent(
      ui::INSERTION_HANDLE_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideTextfield(&point));
  ui::GestureEventDetails tap_details(ui::ET_GESTURE_TAP);
  tap_details.set_tap_count(1);
  ui::GestureEvent tap(point.x(), point.y(), 0, ui::EventTimeForNow(),
                       tap_details);
  rwhva->OnGestureEvent(&tap);

  selection_controller_client->Wait();

  // Check if insertion is active and the quick menu is showing.
  EXPECT_EQ(ui::TouchSelectionController::INSERTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests if the quick menu is hidden whenever a touch point is active.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       QuickMenuHiddenOnTouch) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContents* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  TestTouchSelectionControllerClientAura* selection_controller_client =
      new TestTouchSelectionControllerClientAura(rwhva);
  rwhva->SetSelectionControllerClientForTest(
      make_scoped_ptr(selection_controller_client));

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Long-press on the text and wait for selection handles to appear.
  selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideText(&point));
  ui::GestureEvent long_press(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client->Wait();

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  ui::test::EventGenerator generator(
      web_contents->GetContentNativeView()->GetRootWindow(),
      web_contents->GetContentNativeView());

  // Put the first finger down: the quick menu should get hidden.
  generator.PressTouchId(0);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Put a second finger down: the quick menu should remain hidden.
  generator.PressTouchId(1);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the first finger up: the quick menu should still remain hidden.
  generator.ReleaseTouchId(0);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the second finger up: the quick menu should re-appear.
  generator.ReleaseTouchId(1);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests if the quick menu and touch handles are hidden during an scroll.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest, HiddenOnScroll) {
  // Set the test page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContents* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  TestTouchSelectionControllerClientAura* selection_controller_client =
      new TestTouchSelectionControllerClientAura(rwhva);
  rwhva->SetSelectionControllerClientForTest(
      make_scoped_ptr(selection_controller_client));
  ui::TouchSelectionControllerTestApi selection_controller_test_api(
      rwhva->selection_controller());

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Long-press on the text and wait for selection handles to appear.
  selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideText(&point));
  ui::GestureEvent long_press(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client->Wait();

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Put a finger down: the quick menu should go away, while touch handles stay
  // there.
  ui::TouchEvent touch_down(ui::ET_TOUCH_PRESSED, gfx::PointF(10, 10), 0,
                            ui::EventTimeForNow());
  rwhva->OnTouchEvent(&touch_down);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Start scrolling: touch handles should get hidden, while touch selection is
  // still active.
  ui::GestureEvent scroll_begin(
      10, 10, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  rwhva->OnGestureEvent(&scroll_begin);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // End scrolling: touch handles should re-appear.
  ui::GestureEvent scroll_end(
      10, 10, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  rwhva->OnGestureEvent(&scroll_end);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Lift the finger up: the quick menu should re-appear.
  ui::TouchEvent touch_up(ui::ET_TOUCH_RELEASED, gfx::PointF(10, 10), 0,
                          ui::EventTimeForNow());
  rwhva->OnTouchEvent(&touch_up);
  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(selection_controller_test_api.temporarily_hidden());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests if touch selection gets deactivated after an overscroll completes.
IN_PROC_BROWSER_TEST_F(TouchSelectionControllerClientAuraTest,
                       HiddenAfterOverscroll) {
  // Set the page up.
  ASSERT_NO_FATAL_FAILURE(StartTestWithPage("files/touch_selection.html"));
  WebContents* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  TestTouchSelectionControllerClientAura* selection_controller_client =
      new TestTouchSelectionControllerClientAura(rwhva);
  rwhva->SetSelectionControllerClientForTest(
      make_scoped_ptr(selection_controller_client));

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Long-press on the text and wait for touch handles to appear.
  selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_SHOWN);

  gfx::PointF point;
  ASSERT_TRUE(GetPointInsideText(&point));
  ui::GestureEvent long_press(
      point.x(), point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  rwhva->OnGestureEvent(&long_press);

  selection_controller_client->Wait();

  EXPECT_EQ(ui::TouchSelectionController::SELECTION_ACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Scroll such that an overscroll is initiated and wait for it to complete:
  // touch selection should not be active at the end.
  selection_controller_client->InitWaitForSelectionEvent(
      ui::SELECTION_HANDLES_CLEARED);

  ui::GestureEvent scroll_begin(
      10, 10, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  rwhva->OnGestureEvent(&scroll_begin);

  ui::GestureEvent scroll_update(
      210, 10, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 200, 0));
  rwhva->OnGestureEvent(&scroll_update);

  ui::GestureEvent scroll_end(
      210, 10, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  rwhva->OnGestureEvent(&scroll_end);

  selection_controller_client->Wait();

  EXPECT_EQ(ui::TouchSelectionController::INACTIVE,
            rwhva->selection_controller()->active_status());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

}  // namespace content
