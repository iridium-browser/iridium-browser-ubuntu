// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality_store.h"

#include "net/base/network_change_notifier.h"

namespace net {

namespace nqe {

namespace internal {

NetworkQualityStore::NetworkQualityStore() {
  static_assert(kMaximumNetworkQualityCacheSize > 0,
                "Size of the network quality cache must be > 0");
  // This limit should not be increased unless the logic for removing the
  // oldest cache entry is rewritten to use a doubly-linked-list LRU queue.
  static_assert(kMaximumNetworkQualityCacheSize <= 10,
                "Size of the network quality cache must <= 10");
}

NetworkQualityStore::~NetworkQualityStore() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void NetworkQualityStore::Add(
    const nqe::internal::NetworkID& network_id,
    const nqe::internal::CachedNetworkQuality& cached_network_quality) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(cached_network_qualities_.size(),
            static_cast<size_t>(kMaximumNetworkQualityCacheSize));

  // If the network name is unavailable, caching should not be performed.
  if (network_id.type != net::NetworkChangeNotifier::CONNECTION_ETHERNET &&
      network_id.id.empty()) {
    return;
  }

  // Remove the entry from the map, if it is already present.
  cached_network_qualities_.erase(network_id);

  if (cached_network_qualities_.size() == kMaximumNetworkQualityCacheSize) {
    // Remove the oldest entry.
    CachedNetworkQualities::iterator oldest_entry_iterator =
        cached_network_qualities_.begin();

    for (CachedNetworkQualities::iterator it =
             cached_network_qualities_.begin();
         it != cached_network_qualities_.end(); ++it) {
      if ((it->second).OlderThan(oldest_entry_iterator->second))
        oldest_entry_iterator = it;
    }
    cached_network_qualities_.erase(oldest_entry_iterator);
  }

  cached_network_qualities_.insert(
      std::make_pair(network_id, cached_network_quality));
  DCHECK_LE(cached_network_qualities_.size(),
            static_cast<size_t>(kMaximumNetworkQualityCacheSize));
}

bool NetworkQualityStore::GetById(
    const nqe::internal::NetworkID& network_id,
    nqe::internal::CachedNetworkQuality* cached_network_quality) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CachedNetworkQualities::const_iterator it =
      cached_network_qualities_.find(network_id);

  if (it == cached_network_qualities_.end())
    return false;

  *cached_network_quality = it->second;
  return true;
}

}  // namespace internal

}  // namespace nqe

}  // namespace net
