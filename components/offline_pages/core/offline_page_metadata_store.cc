// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_metadata_store.h"

namespace offline_pages {

template class StoreUpdateResult<OfflinePageItem>;

OfflinePageMetadataStore::~OfflinePageMetadataStore() {
}

}  // namespace offline_pages
