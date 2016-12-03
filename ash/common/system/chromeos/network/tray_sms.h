// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_TRAY_SMS_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_TRAY_SMS_H_

#include <stddef.h>

#include <string>

#include "ash/common/system/tray/system_tray_item.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/network/network_sms_handler.h"

namespace ash {

class TraySms : public SystemTrayItem,
                public chromeos::NetworkSmsHandler::Observer {
 public:
  explicit TraySms(SystemTray* system_tray);
  ~TraySms() override;

  // Overridden from SystemTrayItem.
  views::View* CreateDefaultView(LoginStatus status) override;
  views::View* CreateDetailedView(LoginStatus status) override;
  views::View* CreateNotificationView(LoginStatus status) override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void DestroyNotificationView() override;

  // Overridden from chromeos::NetworkSmsHandler::Observer.
  void MessageReceived(const base::DictionaryValue& message) override;

 protected:
  class SmsDefaultView;
  class SmsDetailedView;
  class SmsMessageView;
  class SmsNotificationView;

  // Gets the most recent message. Returns false if no messages or unable to
  // retrieve the numebr and text from the message.
  bool GetLatestMessage(size_t* index, std::string* number, std::string* text);

  // Removes message at |index| from message list. Returns true if |index| was
  // valid and a message was removed.
  bool RemoveMessage(size_t index);

  // Called when sms messages have changed (through
  // chromeos::NetworkSmsHandler::Observer).
  void Update(bool notify);

  base::ListValue& messages() { return messages_; }

 private:
  SmsDefaultView* default_;
  SmsDetailedView* detailed_;
  SmsNotificationView* notification_;
  base::ListValue messages_;

  DISALLOW_COPY_AND_ASSIGN(TraySms);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_TRAY_SMS_H_
