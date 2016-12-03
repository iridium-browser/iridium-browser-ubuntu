// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"

#include "base/macros.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/website_settings/chosen_object_view.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/ssl_status.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "device/core/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/scoped_views_test_helper.h"

const char* kUrl = "http://www.example.com/index.html";

namespace test {

class WebsiteSettingsPopupViewTestApi {
 public:
  WebsiteSettingsPopupViewTestApi(gfx::NativeView parent,
                                  Profile* profile,
                                  content::WebContents* web_contents)
      : view_(nullptr),
        parent_(parent),
        profile_(profile),
        web_contents_(web_contents) {
    CreateView();
  }

  void CreateView() {
    if (view_)
      view_->GetWidget()->CloseNow();

    security_state::SecurityStateModel::SecurityInfo security_info;
    views::View* anchor_view = nullptr;
    view_ =
        new WebsiteSettingsPopupView(anchor_view, parent_, profile_,
                                     web_contents_, GURL(kUrl), security_info);
  }

  WebsiteSettingsPopupView* view() { return view_; }
  views::View* permissions_content() { return view_->permissions_content_; }

  PermissionSelectorView* GetPermissionSelectorAt(int index) {
    return static_cast<PermissionSelectorView*>(
        permissions_content()->child_at(index));
  }

  base::string16 GetPermissionButtonTextAt(int index) {
    const int kButtonIndex = 2;  // Button should be the third child.
    views::View* view = GetPermissionSelectorAt(index)->child_at(kButtonIndex);
    if (view->GetClassName() == views::MenuButton::kViewClassName) {
      return static_cast<views::MenuButton*>(view)->GetText();
    } else if (view->GetClassName() == views::Combobox::kViewClassName) {
      views::Combobox* combobox = static_cast<views::Combobox*>(view);
      return combobox->GetTextForRow(combobox->GetSelectedRow());
    } else {
      NOTREACHED() << "Unknown class " << view->GetClassName();
      return base::ASCIIToUTF16("");
    }
  }

  // Simulates recreating the dialog with a new PermissionInfoList.
  void SetPermissionInfo(const PermissionInfoList& list) {
    for (const WebsiteSettingsPopupView::PermissionInfo& info : list)
      view_->presenter_->OnSitePermissionChanged(info.type, info.setting);
    CreateView();
  }

 private:
  WebsiteSettingsPopupView* view_;  // Weak. Owned by its Widget.

  // For recreating the view.
  gfx::NativeView parent_;
  Profile* profile_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupViewTestApi);
};

}  // namespace test

namespace {

// Helper class that wraps a TestingProfile and a TestWebContents for a test
// harness. Inspired by RenderViewHostTestHarness, but doesn't use inheritance
// so the helper can be composed with other helpers in the test harness.
class ScopedWebContentsTestHelper {
 public:
  ScopedWebContentsTestHelper() {
    web_contents_ = factory_.CreateWebContents(&profile_);
  }

  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content::TestWebContentsFactory factory_;
  content::WebContents* web_contents_;  // Weak. Owned by factory_.

  DISALLOW_COPY_AND_ASSIGN(ScopedWebContentsTestHelper);
};

class WebsiteSettingsPopupViewTest : public testing::Test {
 public:
  WebsiteSettingsPopupViewTest() {}

  // testing::Test:
  void SetUp() override {
    views::Widget::InitParams parent_params;
    parent_params.context = views_helper_.GetContext();
    parent_window_ = new views::Widget();
    parent_window_->Init(parent_params);

    content::WebContents* web_contents = web_contents_helper_.web_contents();
    TabSpecificContentSettings::CreateForWebContents(web_contents);
    api_.reset(new test::WebsiteSettingsPopupViewTestApi(
        parent_window_->GetNativeView(), web_contents_helper_.profile(),
        web_contents));
  }

  void TearDown() override { parent_window_->CloseNow(); }

 protected:
  device::MockDeviceClient device_client_;
  ScopedWebContentsTestHelper web_contents_helper_;
  views::ScopedViewsTestHelper views_helper_;

  views::Widget* parent_window_ = nullptr;  // Weak. Owned by the NativeWidget.
  std::unique_ptr<test::WebsiteSettingsPopupViewTestApi> api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupViewTest);
};

}  // namespace

// TODO(ellyjones): re-enable this test for OSX.
// This test exercises PermissionSelectorView in a way that it is not used in
// practice. In practice, every setting in PermissionSelectorView starts off
// "set", so there is always one option checked in the resulting MenuModel. This
// test creates settings that are left at their defaults, leading to zero
// checked options, and checks that the text on the MenuButtons is right. Since
// the Comboboxes the MacViews version of this dialog uses don't have separate
// text, this test doesn't work.
#if defined(OS_MACOSX)
#define MAYBE_SetPermissionInfo DISABLED_SetPermissionInfo
#else
#define MAYBE_SetPermissionInfo SetPermissionInfo
#endif

// Test UI construction and reconstruction via
// WebsiteSettingsPopupView::SetPermissionInfo().
TEST_F(WebsiteSettingsPopupViewTest, MAYBE_SetPermissionInfo) {
  PermissionInfoList list(1);
  list.back().type = CONTENT_SETTINGS_TYPE_GEOLOCATION;
  list.back().source = content_settings::SETTING_SOURCE_USER;
  list.back().is_incognito = false;
  list.back().setting = CONTENT_SETTING_DEFAULT;

  const int kExpectedChildren =
      ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled() ? 11 : 13;
  EXPECT_EQ(kExpectedChildren, api_->permissions_content()->child_count());

  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->permissions_content()->child_count());

  PermissionSelectorView* selector = api_->GetPermissionSelectorAt(0);
  EXPECT_EQ(3, selector->child_count());

  // Verify labels match the settings on the PermissionInfoList.
  const int kLabelIndex = 1;
  EXPECT_EQ(views::Label::kViewClassName,
            selector->child_at(kLabelIndex)->GetClassName());
  views::Label* label =
      static_cast<views::Label*>(selector->child_at(kLabelIndex));
  EXPECT_EQ(base::ASCIIToUTF16("Location:"), label->text());
  EXPECT_EQ(base::ASCIIToUTF16("Allowed by you"),
            api_->GetPermissionButtonTextAt(0));

  // Verify calling SetPermisisonInfo() directly updates the UI.
  list.back().setting = CONTENT_SETTING_BLOCK;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(base::ASCIIToUTF16("Blocked by you"),
            api_->GetPermissionButtonTextAt(0));

  // Simulate a user selection via the UI. Note this will also cover logic in
  // WebsiteSettings to update the pref.
  list.back().setting = CONTENT_SETTING_ALLOW;
  api_->GetPermissionSelectorAt(0)->PermissionChanged(list.back());
  EXPECT_EQ(kExpectedChildren, api_->permissions_content()->child_count());
  EXPECT_EQ(base::ASCIIToUTF16("Allowed by you"),
            api_->GetPermissionButtonTextAt(0));

  // Setting to the default via the UI should keep the button around.
  list.back().setting = CONTENT_SETTING_ASK;
  api_->GetPermissionSelectorAt(0)->PermissionChanged(list.back());
  EXPECT_EQ(kExpectedChildren, api_->permissions_content()->child_count());
  EXPECT_EQ(base::ASCIIToUTF16("Ask by you"),
            api_->GetPermissionButtonTextAt(0));

  // However, since the setting is now default, recreating the dialog with those
  // settings should omit the permission from the UI.
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->permissions_content()->child_count());
}

// Test UI construction and reconstruction with USB devices.
TEST_F(WebsiteSettingsPopupViewTest, SetPermissionInfoWithUsbDevice) {
  const int kExpectedChildren =
      ExclusiveAccessManager::IsSimplifiedFullscreenUIEnabled() ? 11 : 13;
  EXPECT_EQ(kExpectedChildren, api_->permissions_content()->child_count());

  const GURL origin = GURL(kUrl).GetOrigin();
  scoped_refptr<device::UsbDevice> device =
      new device::MockUsbDevice(0, 0, "Google", "Gizmo", "1234567890");
  device_client_.usb_service()->AddDevice(device);
  UsbChooserContext* store =
      UsbChooserContextFactory::GetForProfile(web_contents_helper_.profile());
  store->GrantDevicePermission(origin, origin, device->guid());

  PermissionInfoList list;
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren + 1, api_->permissions_content()->child_count());

  ChosenObjectView* object_view = static_cast<ChosenObjectView*>(
      api_->permissions_content()->child_at(kExpectedChildren));
  EXPECT_EQ(3, object_view->child_count());

  const int kLabelIndex = 1;
  views::Label* label =
      static_cast<views::Label*>(object_view->child_at(kLabelIndex));
  EXPECT_EQ(base::ASCIIToUTF16("Gizmo"), label->text());

  const int kButtonIndex = 2;
  views::Button* button =
      static_cast<views::Button*>(object_view->child_at(kButtonIndex));

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  views::ButtonListener* button_listener =
      static_cast<views::ButtonListener*>(object_view);
  button_listener->ButtonPressed(button, event);
  api_->SetPermissionInfo(list);
  EXPECT_EQ(kExpectedChildren, api_->permissions_content()->child_count());
  EXPECT_FALSE(store->HasDevicePermission(origin, origin, device));
}
