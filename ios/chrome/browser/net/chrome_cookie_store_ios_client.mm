// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/chrome_cookie_store_ios_client.h"

#include "base/logging.h"
#import "ios/chrome/browser/browsing_data_change_listening.h"
#include "ios/web/public/web_thread.h"

ChromeCookieStoreIOSClient::ChromeCookieStoreIOSClient(
    id<BrowsingDataChangeListening> browsing_data_change_listener)
    : browsing_data_change_listener_(browsing_data_change_listener) {
  DCHECK(browsing_data_change_listener);
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
}

void ChromeCookieStoreIOSClient::DidChangeCookieStorage() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  [browsing_data_change_listener_ didChangeCookieStorage];
}

scoped_refptr<base::SequencedTaskRunner>
ChromeCookieStoreIOSClient::GetTaskRunner() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::SequencedWorkerPool* pool = web::WebThread::GetBlockingPool();
  return pool->GetSequencedTaskRunner(pool->GetSequenceToken()).get();
}
