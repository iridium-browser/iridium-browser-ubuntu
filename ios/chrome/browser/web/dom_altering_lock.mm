// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/dom_altering_lock.h"

#include "base/logging.h"
#include "ios/web/public/web_thread.h"

DEFINE_WEB_STATE_USER_DATA_KEY(DOMAlteringLock);

DOMAlteringLock::DOMAlteringLock(web::WebState* web_state) {
}

DOMAlteringLock::~DOMAlteringLock() {
}

void DOMAlteringLock::Acquire(id<DOMAltering> feature,
                              ProceduralBlockWithBool lockAction) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  if (current_dom_altering_feature_.get() == feature) {
    lockAction(YES);
    return;
  }
  if (current_dom_altering_feature_) {
    if (![current_dom_altering_feature_ canReleaseDOMLock]) {
      lockAction(NO);
      return;
    }
    [current_dom_altering_feature_ releaseDOMLockWithCompletionHandler:^() {
      DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
      DCHECK(current_dom_altering_feature_.get() == nil)
          << "The lock must be released before calling the completion handler.";
      current_dom_altering_feature_.reset(feature);
      lockAction(YES);
    }];
    return;
  }
  current_dom_altering_feature_.reset(feature);
  lockAction(YES);
}

// Release the lock on the DOM tree.
void DOMAlteringLock::Release(id<DOMAltering> feature) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  if (current_dom_altering_feature_.get() == feature)
    current_dom_altering_feature_.reset();
}
