// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/web_ui_message_handler.h"

class PrefRegistrySimple;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Handler for general New Tab Page functionality that does not belong in a
// more specialized handler.
class NewTabPageHandler : public content::WebUIMessageHandler,
                          public base::SupportsWeakPtr<NewTabPageHandler> {
 public:
  NewTabPageHandler();

  // Register NTP per-profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Registers values (strings etc.) for the page.
  static void GetLocalizedValues(Profile* profile,
      base::DictionaryValue* values);

 private:
  ~NewTabPageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for "notificationPromoClosed". No arguments.
  void HandleNotificationPromoClosed(const base::ListValue* args);

  // Callback for "notificationPromoViewed". No arguments.
  void HandleNotificationPromoViewed(const base::ListValue* args);

  // Callback for "notificationPromoLinkClicked". No arguments.
  void HandleNotificationPromoLinkClicked(const base::ListValue* args);

  // Callback for "bubblePromoClosed". No arguments.
  void HandleBubblePromoClosed(const base::ListValue* args);

  // Callback for "bubblePromoViewed". No arguments.
  void HandleBubblePromoViewed(const base::ListValue* args);

  // Callback for "bubblePromoLinkClicked". No arguments.
  void HandleBubblePromoLinkClicked(const base::ListValue* args);

  // Callback for "pageSelected".
  void HandlePageSelected(const base::ListValue* args);

  // Tracks the number of times the user has switches pages (for UMA).
  size_t page_switch_count_;

  // The purpose of this enum is to track which page on the NTP is showing.
  // The lower 10 bits of kNtpShownPage are used for the index within the page
  // group, and the rest of the bits are used for the page group ID (defined
  // here).
  static const int kPageIdOffset = 10;
  enum {
    INDEX_MASK = (1 << kPageIdOffset) - 1,
    APPS_PAGE_ID = 2 << kPageIdOffset,
    LAST_PAGE_ID = APPS_PAGE_ID,
  };
  static const int kHistogramEnumerationMax =
      (LAST_PAGE_ID >> kPageIdOffset) + 1;

  // Helper to send out promo resource change notification.
  void Notify(chrome::NotificationType notification_type);

  DISALLOW_COPY_AND_ASSIGN(NewTabPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_NEW_TAB_PAGE_HANDLER_H_
