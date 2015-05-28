// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/cookie_notification_bridge.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/location.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/web/public/web_thread.h"

namespace web {

CookieNotificationBridge::CookieNotificationBridge() {
  observer_.reset([[NSNotificationCenter defaultCenter]
      addObserverForName:NSHTTPCookieManagerCookiesChangedNotification
                  object:[NSHTTPCookieStorage sharedHTTPCookieStorage]
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                  OnNotificationReceived(notification);
              }]);
}

CookieNotificationBridge::~CookieNotificationBridge() {
  [[NSNotificationCenter defaultCenter] removeObserver:observer_];
}

void CookieNotificationBridge::OnNotificationReceived(
    NSNotification* notification) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK([[notification name]
      isEqualToString:NSHTTPCookieManagerCookiesChangedNotification]);
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&net::CookieStoreIOS::NotifySystemCookiesChanged));
}

}  // namespace web
