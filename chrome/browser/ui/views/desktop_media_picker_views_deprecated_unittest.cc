// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_media_picker_views_deprecated.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/fake_desktop_media_list.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace deprecated {

class DesktopMediaPickerViewsDeprecatedTest : public testing::Test {
 public:
  DesktopMediaPickerViewsDeprecatedTest() {}
  ~DesktopMediaPickerViewsDeprecatedTest() override {}

  void SetUp() override {
    screen_list_ = new FakeDesktopMediaList();
    window_list_ = new FakeDesktopMediaList();
    tab_list_ = new FakeDesktopMediaList();
    std::unique_ptr<FakeDesktopMediaList> screen_list(screen_list_);
    std::unique_ptr<FakeDesktopMediaList> window_list(window_list_);
    std::unique_ptr<FakeDesktopMediaList> tab_list(tab_list_);

    base::string16 app_name = base::ASCIIToUTF16("foo");

    picker_views_.reset(new DesktopMediaPickerViews());
    picker_views_->Show(
        NULL, test_helper_.GetContext(), NULL, app_name, app_name,
        std::move(screen_list), std::move(window_list), std::move(tab_list),
        false, base::Bind(&DesktopMediaPickerViewsDeprecatedTest::OnPickerDone,
                          base::Unretained(this)));
  }

  void TearDown() override {
    if (GetPickerDialogView()) {
      EXPECT_CALL(*this, OnPickerDone(content::DesktopMediaID()));
      GetPickerDialogView()->GetWidget()->CloseNow();
    }
  }

  DesktopMediaPickerDialogView* GetPickerDialogView() const {
    return picker_views_->GetDialogViewForTesting();
  }

  MOCK_METHOD1(OnPickerDone, void(content::DesktopMediaID));

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  views::ScopedViewsTestHelper test_helper_;
  FakeDesktopMediaList* screen_list_;
  FakeDesktopMediaList* window_list_;
  FakeDesktopMediaList* tab_list_;
  std::unique_ptr<DesktopMediaPickerViews> picker_views_;
};

TEST_F(DesktopMediaPickerViewsDeprecatedTest,
       DoneCallbackCalledWhenWindowClosed) {
  EXPECT_CALL(*this, OnPickerDone(content::DesktopMediaID()));

  GetPickerDialogView()->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
}

TEST_F(DesktopMediaPickerViewsDeprecatedTest,
       DoneCallbackCalledOnOkButtonPressed) {
  const int kFakeId = 222;
  EXPECT_CALL(*this, OnPickerDone(content::DesktopMediaID(
                         content::DesktopMediaID::TYPE_WINDOW, kFakeId)));
  window_list_->AddSource(kFakeId);

  EXPECT_FALSE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  GetPickerDialogView()->GetMediaSourceViewForTesting(0)->OnFocus();
  EXPECT_TRUE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  GetPickerDialogView()->GetDialogClientView()->AcceptWindow();
  base::RunLoop().RunUntilIdle();
}

// Verifies that a MediaSourceView is selected with mouse left click and
// original selected MediaSourceView gets unselected.
TEST_F(DesktopMediaPickerViewsDeprecatedTest,
       SelectMediaSourceViewOnSingleClick) {
  window_list_->AddSource(0);
  window_list_->AddSource(1);

  DesktopMediaSourceView* source_view_0 =
      GetPickerDialogView()->GetMediaSourceViewForTesting(0);

  DesktopMediaSourceView* source_view_1 =
      GetPickerDialogView()->GetMediaSourceViewForTesting(1);

  // Both media source views are not selected initially.
  EXPECT_FALSE(source_view_0->is_selected());
  EXPECT_FALSE(source_view_1->is_selected());

  // Source view 0 is selected with mouse click.
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);

  GetPickerDialogView()->GetMediaSourceViewForTesting(0)->OnMousePressed(press);

  EXPECT_TRUE(source_view_0->is_selected());
  EXPECT_FALSE(source_view_1->is_selected());

  // Source view 1 is selected and source view 0 is unselected with mouse click.
  GetPickerDialogView()->GetMediaSourceViewForTesting(1)->OnMousePressed(press);

  EXPECT_FALSE(source_view_0->is_selected());
  EXPECT_TRUE(source_view_1->is_selected());
}

TEST_F(DesktopMediaPickerViewsDeprecatedTest, DoneCallbackCalledOnDoubleClick) {
  const int kFakeId = 222;
  EXPECT_CALL(*this, OnPickerDone(content::DesktopMediaID(
                         content::DesktopMediaID::TYPE_WINDOW, kFakeId)));

  window_list_->AddSource(kFakeId);

  ui::MouseEvent double_click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                              ui::EventTimeForNow(),
                              ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_DOUBLE_CLICK,
                              ui::EF_LEFT_MOUSE_BUTTON);

  GetPickerDialogView()->GetMediaSourceViewForTesting(0)->OnMousePressed(
      double_click);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DesktopMediaPickerViewsDeprecatedTest, DoneCallbackCalledOnDoubleTap) {
  const int kFakeId = 222;
  EXPECT_CALL(*this, OnPickerDone(content::DesktopMediaID(
                         content::DesktopMediaID::TYPE_WINDOW, kFakeId)));

  window_list_->AddSource(kFakeId);
  ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
  details.set_tap_count(2);
  ui::GestureEvent double_tap(10, 10, 0, base::TimeTicks(), details);

  GetPickerDialogView()->GetMediaSourceViewForTesting(0)->OnGestureEvent(
      &double_tap);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DesktopMediaPickerViewsDeprecatedTest, CancelButtonAlwaysEnabled) {
  EXPECT_TRUE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

// Verifies that the MediaSourceView is added or removed when |media_list_| is
// updated.
TEST_F(DesktopMediaPickerViewsDeprecatedTest, AddAndRemoveMediaSource) {
  // No media source at first.
  EXPECT_EQ(NULL, GetPickerDialogView()->GetMediaSourceViewForTesting(0));

  for (int i = 0; i < 3; ++i) {
    window_list_->AddSource(i);
    EXPECT_TRUE(GetPickerDialogView()->GetMediaSourceViewForTesting(i));
  }

  for (int i = 2; i >= 0; --i) {
    window_list_->RemoveSource(i);
    EXPECT_EQ(NULL, GetPickerDialogView()->GetMediaSourceViewForTesting(i));
  }
}

// Verifies that focusing the MediaSourceView marks it selected and the
// original selected MediaSourceView gets unselected.
TEST_F(DesktopMediaPickerViewsDeprecatedTest, FocusMediaSourceViewToSelect) {
  window_list_->AddSource(0);
  window_list_->AddSource(1);

  DesktopMediaSourceView* source_view_0 =
      GetPickerDialogView()->GetMediaSourceViewForTesting(0);

  DesktopMediaSourceView* source_view_1 =
      GetPickerDialogView()->GetMediaSourceViewForTesting(1);

  EXPECT_FALSE(source_view_0->is_selected());
  EXPECT_FALSE(source_view_1->is_selected());

  source_view_0->OnFocus();
  EXPECT_TRUE(source_view_0->is_selected());

  // Removing the focus does not undo the selection.
  source_view_0->OnBlur();
  EXPECT_TRUE(source_view_0->is_selected());

  source_view_1->OnFocus();
  EXPECT_FALSE(source_view_0->is_selected());
  EXPECT_TRUE(source_view_1->is_selected());
}

TEST_F(DesktopMediaPickerViewsDeprecatedTest, OkButtonDisabledWhenNoSelection) {
  window_list_->AddSource(111);

  EXPECT_FALSE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  GetPickerDialogView()->GetMediaSourceViewForTesting(0)->OnFocus();
  EXPECT_TRUE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  window_list_->RemoveSource(0);
  EXPECT_FALSE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
}

// Verifies that the MediaListView get the initial focus.
TEST_F(DesktopMediaPickerViewsDeprecatedTest, ListViewHasInitialFocus) {
  EXPECT_TRUE(GetPickerDialogView()->GetMediaListViewForTesting()->HasFocus());
}

}  // namespace deprecated
