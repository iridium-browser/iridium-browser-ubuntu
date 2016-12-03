// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf.h"

#include <utility>

#include "ash/common/shelf/shelf_button.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/test_shelf_item_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {

class ShelfTest : public test::AshTestBase {
 public:
  ShelfTest() : shelf_(nullptr), shelf_view_(nullptr), shelf_model_(nullptr) {}

  ~ShelfTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();

    shelf_ = Shelf::ForPrimaryDisplay();
    ASSERT_TRUE(shelf_);

    test::ShelfTestAPI test(shelf_);
    shelf_view_ = test.shelf_view();
    shelf_model_ = shelf_view_->model();

    test_.reset(new test::ShelfViewTestAPI(shelf_view_));
  }

  Shelf* shelf() { return shelf_; }

  ShelfView* shelf_view() { return shelf_view_; }

  ShelfModel* shelf_model() { return shelf_model_; }

  test::ShelfViewTestAPI* test_api() { return test_.get(); }

 private:
  Shelf* shelf_;
  ShelfView* shelf_view_;
  ShelfModel* shelf_model_;
  std::unique_ptr<test::ShelfViewTestAPI> test_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTest);
};

// Confirms that ShelfItem reflects the appropriated state.
TEST_F(ShelfTest, StatusReflection) {
  // Initially we have the app list.
  int button_count = test_api()->GetButtonCount();

  // Add running platform app.
  ShelfItem item;
  item.type = TYPE_PLATFORM_APP;
  item.status = STATUS_RUNNING;
  int index = shelf_model()->Add(item);
  ASSERT_EQ(++button_count, test_api()->GetButtonCount());
  ShelfButton* button = test_api()->GetButton(index);
  EXPECT_EQ(ShelfButton::STATE_RUNNING, button->state());

  // Remove it.
  shelf_model()->RemoveItemAt(index);
  ASSERT_EQ(--button_count, test_api()->GetButtonCount());
}

// Confirm that using the menu will clear the hover attribute. To avoid another
// browser test we check this here.
TEST_F(ShelfTest, CheckHoverAfterMenu) {
  // Initially we have the app list.
  int button_count = test_api()->GetButtonCount();

  // Add running platform app.
  ShelfItem item;
  item.type = TYPE_PLATFORM_APP;
  item.status = STATUS_RUNNING;
  int index = shelf_model()->Add(item);

  std::unique_ptr<ShelfItemDelegate> delegate(
      new test::TestShelfItemDelegate(NULL));
  shelf_model()->SetShelfItemDelegate(shelf_model()->items()[index].id,
                                      std::move(delegate));

  ASSERT_EQ(++button_count, test_api()->GetButtonCount());
  ShelfButton* button = test_api()->GetButton(index);
  button->AddState(ShelfButton::STATE_HOVERED);
  button->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);
  EXPECT_FALSE(button->state() & ShelfButton::STATE_HOVERED);

  // Remove it.
  shelf_model()->RemoveItemAt(index);
}

TEST_F(ShelfTest, ShowOverflowBubble) {
  ShelfWidget* shelf_widget = shelf()->shelf_widget();
  ShelfID first_item_id = shelf_model()->next_id();

  // Add platform app button until overflow.
  int items_added = 0;
  while (!test_api()->IsOverflowButtonVisible()) {
    ShelfItem item;
    item.type = TYPE_PLATFORM_APP;
    item.status = STATUS_RUNNING;
    shelf_model()->Add(item);

    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Shows overflow bubble.
  test_api()->ShowOverflowBubble();
  EXPECT_TRUE(shelf_widget->IsShowingOverflowBubble());

  // Removes the first item in main shelf view.
  shelf_model()->RemoveItemAt(shelf_model()->ItemIndexByID(first_item_id));

  // Waits for all transitions to finish and there should be no crash.
  test_api()->RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(shelf_widget->IsShowingOverflowBubble());
}

}  // namespace ash
