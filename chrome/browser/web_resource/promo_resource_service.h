// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_
#define CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_resource/chrome_web_resource_service.h"

class NotificationPromo;
class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A PromoResourceService fetches data from a web resource server to be used to
// dynamically change the appearance of the New Tab Page. For example, it has
// been used to fetch "tips" to be displayed on the NTP, or to display
// promotional messages to certain groups of Chrome users.
class PromoResourceService : public ChromeWebResourceService {
 public:
  using StateChangedCallbackList = base::CallbackList<void()>;
  using StateChangedSubscription = StateChangedCallbackList::Subscription;

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static void MigrateUserPrefs(PrefService* user_prefs);

  PromoResourceService();
  ~PromoResourceService() override;

  // Registers a callback called when the state of a web resource has been
  // changed. A resource may have been added, removed, or altered.
  scoped_ptr<StateChangedSubscription> RegisterStateChangedCallback(
      const base::Closure& closure);

 private:
  // Schedule a notification that a web resource is either going to become
  // available or be no longer valid.
  void ScheduleNotification(const NotificationPromo& notification_promo);

  // Schedules the initial notification for when the web resource is going
  // to become available or no longer valid. This performs a few additional
  // checks than ScheduleNotification, namely it schedules updates immediately
  // if the promo service or Chrome locale has changed.
  void ScheduleNotificationOnInit();

  // If delay_ms is positive, schedule notification with the delay.
  // If delay_ms is 0, notify immediately by calling WebResourceStateChange().
  // If delay_ms is negative, do nothing.
  void PostNotification(int64 delay_ms);

  // Notify listeners that the state of a web resource has changed.
  void PromoResourceStateChange();

  // WebResourceService override to process the parsed information.
  void Unpack(const base::DictionaryValue& parsed_json) override;

  // List of callbacks called when the state of a web resource has changed.
  StateChangedCallbackList callback_list_;

  // Allows the creation of tasks to send a notification.
  // This allows the PromoResourceService to notify the New Tab Page immediately
  // when a new web resource should be shown or removed.
  base::WeakPtrFactory<PromoResourceService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PromoResourceService);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_PROMO_RESOURCE_SERVICE_H_
