// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_GOOGLE_NOW_NOTIFICATION_STATS_COLLECTOR_H_
#define CHROME_BROWSER_NOTIFICATIONS_GOOGLE_NOW_NOTIFICATION_STATS_COLLECTOR_H_

#include <string>

#include "base/macros.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"

// The Google Now Notification Stats Collector listens for message center
// events and records stats about Google Now specific notifications.
class GoogleNowNotificationStatsCollector
    : public message_center::MessageCenterObserver {
 public:
  explicit GoogleNowNotificationStatsCollector(
      message_center::MessageCenter* message_center);
  ~GoogleNowNotificationStatsCollector() override;

 private:
  // MessageCenterObserver
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;
  void OnCenterVisibilityChanged(
      message_center::Visibility visibility) override;

  // Counts the number of Google Now Notifications in the message center.
  int CountVisibleGoogleNowNotifications();

  // Returns true if the ID of a visible notification is for Google Now.
  bool IsVisibleNotificationIdForGoogleNow(const std::string& notification_id);

  // Weak, global.
  message_center::MessageCenter* message_center_;

  DISALLOW_COPY_AND_ASSIGN(GoogleNowNotificationStatsCollector);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_GOOGLE_NOW_NOTIFICATION_STATS_COLLECTOR_H_
