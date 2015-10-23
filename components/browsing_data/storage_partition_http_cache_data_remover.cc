// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/storage_partition_http_cache_data_remover.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/sdch_manager.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace browsing_data {

StoragePartitionHttpCacheDataRemover::StoragePartitionHttpCacheDataRemover(
    base::Time delete_begin,
    base::Time delete_end,
    net::URLRequestContextGetter* main_context_getter,
    net::URLRequestContextGetter* media_context_getter)
    : delete_begin_(delete_begin),
      delete_end_(delete_end),
      main_context_getter_(main_context_getter),
      media_context_getter_(media_context_getter),
      next_cache_state_(STATE_NONE),
      cache_(nullptr) {
}

StoragePartitionHttpCacheDataRemover::~StoragePartitionHttpCacheDataRemover() {
}

// static.
StoragePartitionHttpCacheDataRemover*
StoragePartitionHttpCacheDataRemover::CreateForRange(
    content::StoragePartition* storage_partition,
    base::Time delete_begin,
    base::Time delete_end) {
  return new StoragePartitionHttpCacheDataRemover(
      delete_begin, delete_end, storage_partition->GetURLRequestContext(),
      storage_partition->GetMediaURLRequestContext());
}

void StoragePartitionHttpCacheDataRemover::Remove(
    const base::Closure& done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!done_callback.is_null());
  done_callback_ = done_callback;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &StoragePartitionHttpCacheDataRemover::ClearHttpCacheOnIOThread,
          base::Unretained(this)));
}

void StoragePartitionHttpCacheDataRemover::ClearHttpCacheOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  next_cache_state_ = STATE_NONE;
  DCHECK_EQ(STATE_NONE, next_cache_state_);
  DCHECK(main_context_getter_.get());
  DCHECK(media_context_getter_.get());

  next_cache_state_ = STATE_CREATE_MAIN;
  DoClearCache(net::OK);
}

void StoragePartitionHttpCacheDataRemover::ClearedHttpCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  done_callback_.Run();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

// The expected state sequence is STATE_NONE --> STATE_CREATE_MAIN -->
// STATE_DELETE_MAIN --> STATE_CREATE_MEDIA --> STATE_DELETE_MEDIA -->
// STATE_DONE, and any errors are ignored.
void StoragePartitionHttpCacheDataRemover::DoClearCache(int rv) {
  DCHECK_NE(STATE_NONE, next_cache_state_);

  while (rv != net::ERR_IO_PENDING && next_cache_state_ != STATE_NONE) {
    switch (next_cache_state_) {
      case STATE_CREATE_MAIN:
      case STATE_CREATE_MEDIA: {
        // Get a pointer to the cache.
        net::URLRequestContextGetter* getter =
            (next_cache_state_ == STATE_CREATE_MAIN)
                ? main_context_getter_.get()
                : media_context_getter_.get();
        net::HttpCache* http_cache = getter->GetURLRequestContext()
                                         ->http_transaction_factory()
                                         ->GetCache();

        next_cache_state_ = (next_cache_state_ == STATE_CREATE_MAIN)
                                ? STATE_DELETE_MAIN
                                : STATE_DELETE_MEDIA;

        // Clear QUIC server information from memory and the disk cache.
        http_cache->GetSession()
            ->quic_stream_factory()
            ->ClearCachedStatesInCryptoConfig();

        // Clear SDCH dictionary state.
        net::SdchManager* sdch_manager =
            getter->GetURLRequestContext()->sdch_manager();
        // The test is probably overkill, since chrome should always have an
        // SdchManager.  But in general the URLRequestContext  is *not*
        // guaranteed to have an SdchManager, so checking is wise.
        if (sdch_manager)
          sdch_manager->ClearData();

        rv = http_cache->GetBackend(
            &cache_,
            base::Bind(&StoragePartitionHttpCacheDataRemover::DoClearCache,
                       base::Unretained(this)));
        break;
      }
      case STATE_DELETE_MAIN:
      case STATE_DELETE_MEDIA: {
        next_cache_state_ = (next_cache_state_ == STATE_DELETE_MAIN)
                                ? STATE_CREATE_MEDIA
                                : STATE_DONE;

        // |cache_| can be null if it cannot be initialized.
        if (cache_) {
          if (delete_begin_.is_null()) {
            rv = cache_->DoomAllEntries(
                base::Bind(&StoragePartitionHttpCacheDataRemover::DoClearCache,
                           base::Unretained(this)));
          } else {
            rv = cache_->DoomEntriesBetween(
                delete_begin_, delete_end_,
                base::Bind(&StoragePartitionHttpCacheDataRemover::DoClearCache,
                           base::Unretained(this)));
          }
          cache_ = NULL;
        }
        break;
      }
      case STATE_DONE: {
        cache_ = NULL;
        next_cache_state_ = STATE_NONE;

        // Notify the UI thread that we are done.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&StoragePartitionHttpCacheDataRemover::ClearedHttpCache,
                       base::Unretained(this)));
        return;
      }
      default: {
        NOTREACHED() << "bad state";
        next_cache_state_ = STATE_NONE;  // Stop looping.
        return;
      }
    }
  }
}

}  // namespace browsing_data
