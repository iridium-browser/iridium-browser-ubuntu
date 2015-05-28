// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_disk_cache.h"

namespace content {

ServiceWorkerResponseReader::ServiceWorkerResponseReader(
    int64 response_id, ServiceWorkerDiskCache* disk_cache)
    : AppCacheResponseReader(response_id, 0, disk_cache) {
}

ServiceWorkerResponseWriter::ServiceWorkerResponseWriter(
    int64 response_id, ServiceWorkerDiskCache* disk_cache)
    : AppCacheResponseWriter(response_id, 0, disk_cache) {
}

ServiceWorkerResponseMetadataWriter::ServiceWorkerResponseMetadataWriter(
    int64 response_id,
    ServiceWorkerDiskCache* disk_cache)
    : AppCacheResponseMetadataWriter(response_id, 0, disk_cache) {
}

}  // namespace content
