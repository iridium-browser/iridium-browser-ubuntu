// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerCache_h
#define WebServiceWorkerCache_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include <memory>
#include <utility>

namespace blink {

// The Service Worker Cache API. The embedder provides the implementation of the
// Cache to Blink. Blink uses the interface to operate on entries. This object
// is owned by Blink, and should be destroyed as each Cache instance is no
// longer in use.
class WebServiceWorkerCache {
 public:
  using CacheMatchCallbacks =
      WebCallbacks<const WebServiceWorkerResponse&, WebServiceWorkerCacheError>;
  using CacheWithResponsesCallbacks =
      WebCallbacks<const WebVector<WebServiceWorkerResponse>&,
                   WebServiceWorkerCacheError>;
  using CacheWithRequestsCallbacks =
      WebCallbacks<const WebVector<WebServiceWorkerRequest>&,
                   WebServiceWorkerCacheError>;
  using CacheBatchCallbacks = WebCallbacks<void, WebServiceWorkerCacheError>;

  virtual ~WebServiceWorkerCache() {}

  // Options that affect the scope of searches.
  struct QueryParams {
    QueryParams()
        : ignoreSearch(false), ignoreMethod(false), ignoreVary(false) {}

    bool ignoreSearch;
    bool ignoreMethod;
    bool ignoreVary;
    WebString cacheName;
  };

  enum OperationType {
    OperationTypeUndefined,
    OperationTypePut,
    OperationTypeDelete,
    OperationTypeLast = OperationTypeDelete
  };

  struct BatchOperation {
    BatchOperation() : operationType(OperationTypeUndefined) {}

    OperationType operationType;
    WebServiceWorkerRequest request;
    WebServiceWorkerResponse response;
    QueryParams matchParams;
  };

  WebServiceWorkerCache() {}

  // Ownership of the Cache*Callbacks methods passes to the
  // WebServiceWorkerCache instance, which will delete it after calling
  // onSuccess or onFailure.
  virtual void dispatchMatch(std::unique_ptr<CacheMatchCallbacks>,
                             const WebServiceWorkerRequest&,
                             const QueryParams&) = 0;
  virtual void dispatchMatchAll(std::unique_ptr<CacheWithResponsesCallbacks>,
                                const WebServiceWorkerRequest&,
                                const QueryParams&) = 0;
  virtual void dispatchKeys(std::unique_ptr<CacheWithRequestsCallbacks>,
                            const WebServiceWorkerRequest&,
                            const QueryParams&) = 0;
  virtual void dispatchBatch(std::unique_ptr<CacheBatchCallbacks>,
                             const WebVector<BatchOperation>&) = 0;
};

}  // namespace blink

#endif  // WebServiceWorkerCache_h
