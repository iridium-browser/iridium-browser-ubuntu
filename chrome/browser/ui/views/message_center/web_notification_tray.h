// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_
#define CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_

#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/views/message_center/message_center_widget_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_WIN)
#include "base/threading/thread.h"
#endif

class PrefService;
class StatusIcon;

namespace message_center {
class MessageCenter;
class MessagePopupCollection;
}

namespace views {
class Widget;
}

namespace message_center {

struct PositionInfo;

class DesktopPopupAlignmentDelegate;
class MessageCenterWidgetDelegate;

// A MessageCenterTrayDelegate implementation that exposes the MessageCenterTray
// via a system tray icon.  The notification popups will be displayed in the
// corner of the screen and the message center will be displayed by the system
// tray icon on click.
class WebNotificationTray : public message_center::MessageCenterTrayDelegate,
                            public StatusIconObserver,
                            public base::SupportsWeakPtr<WebNotificationTray>,
                            public StatusIconMenuModel::Delegate {
 public:
  explicit WebNotificationTray(PrefService* local_state);
  ~WebNotificationTray() override;

  message_center::MessageCenter* message_center();

  // MessageCenterTrayDelegate implementation.
  bool ShowPopups() override;
  void HidePopups() override;
  bool ShowMessageCenter() override;
  void HideMessageCenter() override;
  void OnMessageCenterTrayChanged() override;
  bool ShowNotifierSettings() override;
  bool IsContextMenuEnabled() const override;

  // StatusIconObserver implementation.
  void OnStatusIconClicked() override;
#if defined(OS_WIN)
  void OnBalloonClicked() override;

  // This shows a platform-specific balloon informing the user of the existence
  // of the message center in the status tray area.
  void DisplayFirstRunBalloon() override;

  void EnforceStatusIconVisible();
#endif

  // StatusIconMenuModel::Delegate implementation.
  void ExecuteCommand(int command_id, int event_flags) override;

  // Changes the icon and hovertext based on number of unread notifications.
  void UpdateStatusIcon();
  void SendHideMessageCenter();
  void MarkMessageCenterHidden();

  // Gets the point where the status icon was clicked.
  gfx::Point mouse_click_point() { return mouse_click_point_; }
  MessageCenterTray* GetMessageCenterTray() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, WebNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, WebNotificationPopupBubble);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest,
                           ManyMessageCenterNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, ManyPopupNotifications);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, ManuallyCloseMessageCenter);
  FRIEND_TEST_ALL_PREFIXES(WebNotificationTrayTest, StatusIconBehavior);

  PositionInfo GetPositionInfo();

  void CreateStatusIcon(const gfx::ImageSkia& image,
                        const base::string16& tool_tip);
  void DestroyStatusIcon();
  void AddQuietModeMenu(StatusIcon* status_icon);
  MessageCenterWidgetDelegate* GetMessageCenterWidgetDelegateForTest();

#if defined(OS_WIN)
  // This member variable keeps track of whether EnforceStatusIconVisible has
  // been invoked on this machine, so the user still has control after we try
  // promoting it the first time.
  scoped_ptr<BooleanPrefMember> did_force_tray_visible_;
#endif

  MessageCenterWidgetDelegate* message_center_delegate_;
  scoped_ptr<message_center::MessagePopupCollection> popup_collection_;
  scoped_ptr<message_center::DesktopPopupAlignmentDelegate> alignment_delegate_;

  StatusIcon* status_icon_;
  StatusIconMenuModel* status_icon_menu_;
  scoped_ptr<MessageCenterTray> message_center_tray_;
  gfx::Point mouse_click_point_;

  bool should_update_tray_content_;
  bool last_quiet_mode_state_;
  base::string16 title_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationTray);
};

}  // namespace message_center

#endif  // CHROME_BROWSER_UI_VIEWS_MESSAGE_CENTER_WEB_NOTIFICATION_TRAY_H_
