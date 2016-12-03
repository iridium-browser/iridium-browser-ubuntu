// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_namespace.h"

#include "content/renderer/dom_storage/local_storage_area.h"
#include "content/renderer/dom_storage/local_storage_cached_areas.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::WebStorageArea;
using blink::WebStorageNamespace;
using blink::WebString;

namespace content {

LocalStorageNamespace::LocalStorageNamespace(
    LocalStorageCachedAreas* local_storage_cached_areas)
    : local_storage_cached_areas_(local_storage_cached_areas) {
}

LocalStorageNamespace::~LocalStorageNamespace() {
}

WebStorageArea* LocalStorageNamespace::createStorageArea(
    const WebString& origin) {
  return new LocalStorageArea(local_storage_cached_areas_->GetCachedArea(
      url::Origin(blink::WebStringToGURL(origin))));
}

bool LocalStorageNamespace::isSameNamespace(
    const WebStorageNamespace& other) const {
  NOTREACHED() << "This method should only be called for session storage.";
  return false;
}

}  // namespace content
