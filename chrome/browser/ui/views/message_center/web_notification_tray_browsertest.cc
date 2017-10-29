// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/web_notification_tray.h"

#include <stddef.h>

#include <set>

#include "ash/root_window_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {

class WebNotificationTrayTest : public InProcessBrowserTest {
 public:
  WebNotificationTrayTest() {}
  ~WebNotificationTrayTest() override {}

  void TearDownOnMainThread() override {
    message_center::MessageCenter::Get()->RemoveAllNotifications(false);
  }

 protected:
  class TestNotificationDelegate : public ::NotificationDelegate {
   public:
    explicit TestNotificationDelegate(const std::string& id) : id_(id) {}
    std::string id() const override { return id_; }

   private:
    ~TestNotificationDelegate() override {}

    std::string id_;
  };

  void AddNotification(const std::string& delegate_id,
                       const std::string& replace_id) {
    ::Notification notification(
        GURL("chrome-extension://abbccedd"),
        base::ASCIIToUTF16("Test Web Notification"),
        base::ASCIIToUTF16("Notification message body."),
        gfx::Image(),
        base::string16(),
        replace_id,
        new TestNotificationDelegate(delegate_id));

    g_browser_process->notification_ui_manager()->Add(notification,
                                                      browser()->profile());
  }

  std::string FindNotificationIdByDelegateId(const std::string& delegate_id) {
    const ::Notification* notification =
        g_browser_process->notification_ui_manager()->FindById(
            delegate_id,
            NotificationUIManager::GetProfileID(browser()->profile()));
    EXPECT_TRUE(notification);
    return notification->id();
  }

  void UpdateNotification(const std::string& replace_id,
                          const std::string& new_id) {
    ::Notification notification(GURL("chrome-extension://abbccedd"),
                                base::ASCIIToUTF16("Updated Web Notification"),
                                base::ASCIIToUTF16("Updated message body."),
                                gfx::Image(),
                                base::string16(),
                                replace_id,
                                new TestNotificationDelegate(new_id));

    g_browser_process->notification_ui_manager()->Add(notification,
                                                      browser()->profile());
  }

  void RemoveNotification(const std::string& id) {
    g_browser_process->notification_ui_manager()->CancelById(
        id, NotificationUIManager::GetProfileID(browser()->profile()));
  }

  bool HasNotification(message_center::MessageCenter* message_center,
                       const std::string& id) {
    return message_center->FindVisibleNotificationById(id) != NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationTrayTest);
};

}  // namespace


// TODO(dewittj): More exhaustive testing.
IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest, WebNotifications) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // Add a notification.
  AddNotification("test_id1", "replace_id1");
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(HasNotification(message_center,
                              FindNotificationIdByDelegateId("test_id1")));
  EXPECT_FALSE(HasNotification(message_center, "test_id2"));
  AddNotification("test_id2", "replace_id2");
  AddNotification("test_id2", "replace_id2");
  EXPECT_EQ(2u, message_center->NotificationCount());
  EXPECT_TRUE(HasNotification(message_center,
                              FindNotificationIdByDelegateId("test_id1")));

  // Ensure that updating a notification does not affect the count.
  UpdateNotification("replace_id2", "test_id3");
  UpdateNotification("replace_id2", "test_id3");
  EXPECT_EQ(2u, message_center->NotificationCount());
  EXPECT_FALSE(HasNotification(message_center, "test_id2"));

  // Ensure that Removing the first notification removes it from the tray.
  RemoveNotification("test_id1");
  EXPECT_FALSE(HasNotification(message_center, "test_id1"));
  EXPECT_EQ(1u, message_center->NotificationCount());

  // Remove the remaining notification.
  RemoveNotification("test_id3");
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(HasNotification(message_center, "test_id1"));
}

IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest, WebNotificationPopupBubble) {
  std::unique_ptr<WebNotificationTray> tray(new WebNotificationTray());
  tray->message_center();

  // Adding a notification should show the popup bubble.
  AddNotification("test_id1", "replace_id1");
  EXPECT_TRUE(tray->message_center_tray_->popups_visible());

  // Updating a notification should not hide the popup bubble.
  AddNotification("test_id2", "replace_id2");
  UpdateNotification("replace_id2", "test_id3");
  EXPECT_TRUE(tray->message_center_tray_->popups_visible());

  // Removing the first notification should not hide the popup bubble.
  RemoveNotification("test_id1");
  EXPECT_TRUE(tray->message_center_tray_->popups_visible());

  // Removing the visible notification should hide the popup bubble.
  RemoveNotification("test_id3");
  EXPECT_FALSE(tray->message_center_tray_->popups_visible());
}

using message_center::NotificationList;

IN_PROC_BROWSER_TEST_F(WebNotificationTrayTest, ManyPopupNotifications) {
  std::unique_ptr<WebNotificationTray> tray(new WebNotificationTray());
  message_center::MessageCenter* message_center = tray->message_center();

  // Add the max visible popup notifications +1, ensure the correct num visible.
  size_t notifications_to_add = kMaxVisiblePopupNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("test_id%d", static_cast<int>(i));
    std::string replace_id =
        base::StringPrintf("replace_id%d", static_cast<int>(i));
    AddNotification(id, replace_id);
  }
  // Hide and reshow the bubble so that it is updated immediately, not delayed.
  tray->message_center_tray_->HidePopupBubble();
  tray->message_center_tray_->ShowPopupBubble();
  EXPECT_TRUE(tray->message_center_tray_->popups_visible());
  EXPECT_EQ(notifications_to_add, message_center->NotificationCount());
  NotificationList::PopupNotifications popups =
      message_center->GetPopupNotifications();
  EXPECT_EQ(kMaxVisiblePopupNotifications, popups.size());
}

}  // namespace message_center
