// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/InspectorCacheStorageAgent.h"

#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCache.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>
#include <memory>
#include <utility>

using blink::protocol::Array;
using blink::protocol::CacheStorage::Cache;
using blink::protocol::CacheStorage::DataEntry;

typedef blink::protocol::CacheStorage::Backend::DeleteCacheCallback
    DeleteCacheCallback;
typedef blink::protocol::CacheStorage::Backend::DeleteEntryCallback
    DeleteEntryCallback;
typedef blink::protocol::CacheStorage::Backend::RequestCacheNamesCallback
    RequestCacheNamesCallback;
typedef blink::protocol::CacheStorage::Backend::RequestEntriesCallback
    RequestEntriesCallback;
typedef blink::WebServiceWorkerCache::BatchOperation BatchOperation;

namespace blink {

namespace {

String buildCacheId(const String& securityOrigin, const String& cacheName) {
  String id(securityOrigin);
  id.append('|');
  id.append(cacheName);
  return id;
}

Response parseCacheId(const String& id,
                      String* securityOrigin,
                      String* cacheName) {
  size_t pipe = id.find('|');
  if (pipe == WTF::kNotFound)
    return Response::Error("Invalid cache id.");
  *securityOrigin = id.substring(0, pipe);
  *cacheName = id.substring(pipe + 1);
  return Response::OK();
}

Response assertCacheStorage(
    const String& securityOrigin,
    std::unique_ptr<WebServiceWorkerCacheStorage>& result) {
  RefPtr<SecurityOrigin> secOrigin =
      SecurityOrigin::createFromString(securityOrigin);

  // Cache Storage API is restricted to trustworthy origins.
  if (!secOrigin->isPotentiallyTrustworthy())
    return Response::Error(secOrigin->isPotentiallyTrustworthyErrorMessage());

  std::unique_ptr<WebServiceWorkerCacheStorage> cache = WTF::wrapUnique(
      Platform::current()->cacheStorage(WebSecurityOrigin(secOrigin)));
  if (!cache)
    return Response::Error("Could not find cache storage.");
  result = std::move(cache);
  return Response::OK();
}

Response assertCacheStorageAndNameForId(
    const String& cacheId,
    String* cacheName,
    std::unique_ptr<WebServiceWorkerCacheStorage>& result) {
  String securityOrigin;
  Response response = parseCacheId(cacheId, &securityOrigin, cacheName);
  if (!response.isSuccess())
    return response;
  return assertCacheStorage(securityOrigin, result);
}

CString serviceWorkerCacheErrorString(WebServiceWorkerCacheError error) {
  switch (error) {
    case WebServiceWorkerCacheErrorNotImplemented:
      return CString("not implemented.");
      break;
    case WebServiceWorkerCacheErrorNotFound:
      return CString("not found.");
      break;
    case WebServiceWorkerCacheErrorExists:
      return CString("cache already exists.");
      break;
    case WebServiceWorkerCacheErrorQuotaExceeded:
      return CString("quota exceeded.");
    case WebServiceWorkerCacheErrorCacheNameNotFound:
      return CString("cache not found.");
    case WebServiceWorkerCacheErrorTooLarge:
      return CString("operation too large.");
  }
  NOTREACHED();
  return "";
}

class RequestCacheNames
    : public WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks {
  WTF_MAKE_NONCOPYABLE(RequestCacheNames);

 public:
  RequestCacheNames(const String& securityOrigin,
                    std::unique_ptr<RequestCacheNamesCallback> callback)
      : m_securityOrigin(securityOrigin), m_callback(std::move(callback)) {}

  ~RequestCacheNames() override {}

  void onSuccess(const WebVector<WebString>& caches) override {
    std::unique_ptr<Array<Cache>> array = Array<Cache>::create();
    for (size_t i = 0; i < caches.size(); i++) {
      String name = String(caches[i]);
      std::unique_ptr<Cache> entry =
          Cache::create()
              .setSecurityOrigin(m_securityOrigin)
              .setCacheName(name)
              .setCacheId(buildCacheId(m_securityOrigin, name))
              .build();
      array->addItem(std::move(entry));
    }
    m_callback->sendSuccess(std::move(array));
  }

  void onError(WebServiceWorkerCacheError error) override {
    m_callback->sendFailure(Response::Error(
        String::format("Error requesting cache names: %s",
                       serviceWorkerCacheErrorString(error).data())));
  }

 private:
  String m_securityOrigin;
  std::unique_ptr<RequestCacheNamesCallback> m_callback;
};

struct DataRequestParams {
  String cacheName;
  int skipCount;
  int pageSize;
};

struct RequestResponse {
  RequestResponse() {}
  RequestResponse(const String& request, const String& response)
      : request(request), response(response) {}
  String request;
  String response;
};

class ResponsesAccumulator : public RefCounted<ResponsesAccumulator> {
  WTF_MAKE_NONCOPYABLE(ResponsesAccumulator);

 public:
  ResponsesAccumulator(int numResponses,
                       const DataRequestParams& params,
                       std::unique_ptr<RequestEntriesCallback> callback)
      : m_params(params),
        m_numResponsesLeft(numResponses),
        m_responses(static_cast<size_t>(numResponses)),
        m_callback(std::move(callback)) {}

  void addRequestResponsePair(const WebServiceWorkerRequest& request,
                              const WebServiceWorkerResponse& response) {
    ASSERT(m_numResponsesLeft > 0);
    RequestResponse& requestResponse =
        m_responses.at(m_responses.size() - m_numResponsesLeft);
    requestResponse.request = request.url().string();
    requestResponse.response = response.statusText();

    if (--m_numResponsesLeft != 0)
      return;

    std::sort(m_responses.begin(), m_responses.end(),
              [](const RequestResponse& a, const RequestResponse& b) {
                return WTF::codePointCompareLessThan(a.request, b.request);
              });
    if (m_params.skipCount > 0)
      m_responses.remove(0, m_params.skipCount);
    bool hasMore = false;
    if (static_cast<size_t>(m_params.pageSize) < m_responses.size()) {
      m_responses.remove(m_params.pageSize,
                         m_responses.size() - m_params.pageSize);
      hasMore = true;
    }
    std::unique_ptr<Array<DataEntry>> array = Array<DataEntry>::create();
    for (const auto& requestResponse : m_responses) {
      std::unique_ptr<DataEntry> entry =
          DataEntry::create()
              .setRequest(requestResponse.request)
              .setResponse(requestResponse.response)
              .build();
      array->addItem(std::move(entry));
    }
    m_callback->sendSuccess(std::move(array), hasMore);
  }

  void sendFailure(const Response& error) { m_callback->sendFailure(error); }

 private:
  DataRequestParams m_params;
  int m_numResponsesLeft;
  Vector<RequestResponse> m_responses;
  std::unique_ptr<RequestEntriesCallback> m_callback;
};

class GetCacheResponsesForRequestData
    : public WebServiceWorkerCache::CacheMatchCallbacks {
  WTF_MAKE_NONCOPYABLE(GetCacheResponsesForRequestData);

 public:
  GetCacheResponsesForRequestData(const DataRequestParams& params,
                                  const WebServiceWorkerRequest& request,
                                  PassRefPtr<ResponsesAccumulator> accum)
      : m_params(params), m_request(request), m_accumulator(accum) {}
  ~GetCacheResponsesForRequestData() override {}

  void onSuccess(const WebServiceWorkerResponse& response) override {
    m_accumulator->addRequestResponsePair(m_request, response);
  }

  void onError(WebServiceWorkerCacheError error) override {
    m_accumulator->sendFailure(Response::Error(
        String::format("Error requesting responses for cache  %s: %s",
                       m_params.cacheName.utf8().data(),
                       serviceWorkerCacheErrorString(error).data())));
  }

 private:
  DataRequestParams m_params;
  WebServiceWorkerRequest m_request;
  RefPtr<ResponsesAccumulator> m_accumulator;
};

class GetCacheKeysForRequestData
    : public WebServiceWorkerCache::CacheWithRequestsCallbacks {
  WTF_MAKE_NONCOPYABLE(GetCacheKeysForRequestData);

 public:
  GetCacheKeysForRequestData(const DataRequestParams& params,
                             std::unique_ptr<WebServiceWorkerCache> cache,
                             std::unique_ptr<RequestEntriesCallback> callback)
      : m_params(params),
        m_cache(std::move(cache)),
        m_callback(std::move(callback)) {}
  ~GetCacheKeysForRequestData() override {}

  WebServiceWorkerCache* cache() { return m_cache.get(); }
  void onSuccess(const WebVector<WebServiceWorkerRequest>& requests) override {
    if (requests.isEmpty()) {
      std::unique_ptr<Array<DataEntry>> array = Array<DataEntry>::create();
      m_callback->sendSuccess(std::move(array), false);
      return;
    }
    RefPtr<ResponsesAccumulator> accumulator =
        adoptRef(new ResponsesAccumulator(requests.size(), m_params,
                                          std::move(m_callback)));

    for (size_t i = 0; i < requests.size(); i++) {
      const auto& request = requests[i];
      auto cacheRequest = WTF::makeUnique<GetCacheResponsesForRequestData>(
          m_params, request, accumulator);
      m_cache->dispatchMatch(std::move(cacheRequest), request,
                             WebServiceWorkerCache::QueryParams());
    }
  }

  void onError(WebServiceWorkerCacheError error) override {
    m_callback->sendFailure(Response::Error(
        String::format("Error requesting requests for cache %s: %s",
                       m_params.cacheName.utf8().data(),
                       serviceWorkerCacheErrorString(error).data())));
  }

 private:
  DataRequestParams m_params;
  std::unique_ptr<WebServiceWorkerCache> m_cache;
  std::unique_ptr<RequestEntriesCallback> m_callback;
};

class GetCacheForRequestData
    : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
  WTF_MAKE_NONCOPYABLE(GetCacheForRequestData);

 public:
  GetCacheForRequestData(const DataRequestParams& params,
                         std::unique_ptr<RequestEntriesCallback> callback)
      : m_params(params), m_callback(std::move(callback)) {}
  ~GetCacheForRequestData() override {}

  void onSuccess(std::unique_ptr<WebServiceWorkerCache> cache) override {
    auto cacheRequest = WTF::makeUnique<GetCacheKeysForRequestData>(
        m_params, std::move(cache), std::move(m_callback));
    cacheRequest->cache()->dispatchKeys(std::move(cacheRequest),
                                        WebServiceWorkerRequest(),
                                        WebServiceWorkerCache::QueryParams());
  }

  void onError(WebServiceWorkerCacheError error) override {
    m_callback->sendFailure(Response::Error(String::format(
        "Error requesting cache %s: %s", m_params.cacheName.utf8().data(),
        serviceWorkerCacheErrorString(error).data())));
  }

 private:
  DataRequestParams m_params;
  std::unique_ptr<RequestEntriesCallback> m_callback;
};

class DeleteCache : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
  WTF_MAKE_NONCOPYABLE(DeleteCache);

 public:
  DeleteCache(std::unique_ptr<DeleteCacheCallback> callback)
      : m_callback(std::move(callback)) {}
  ~DeleteCache() override {}

  void onSuccess() override { m_callback->sendSuccess(); }

  void onError(WebServiceWorkerCacheError error) override {
    m_callback->sendFailure(Response::Error(
        String::format("Error requesting cache names: %s",
                       serviceWorkerCacheErrorString(error).data())));
  }

 private:
  std::unique_ptr<DeleteCacheCallback> m_callback;
};

class DeleteCacheEntry : public WebServiceWorkerCache::CacheBatchCallbacks {
  WTF_MAKE_NONCOPYABLE(DeleteCacheEntry);

 public:
  DeleteCacheEntry(std::unique_ptr<DeleteEntryCallback> callback)
      : m_callback(std::move(callback)) {}
  ~DeleteCacheEntry() override {}

  void onSuccess() override { m_callback->sendSuccess(); }

  void onError(WebServiceWorkerCacheError error) override {
    m_callback->sendFailure(Response::Error(
        String::format("Error requesting cache names: %s",
                       serviceWorkerCacheErrorString(error).data())));
  }

 private:
  std::unique_ptr<DeleteEntryCallback> m_callback;
};

class GetCacheForDeleteEntry
    : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
  WTF_MAKE_NONCOPYABLE(GetCacheForDeleteEntry);

 public:
  GetCacheForDeleteEntry(const String& requestSpec,
                         const String& cacheName,
                         std::unique_ptr<DeleteEntryCallback> callback)
      : m_requestSpec(requestSpec),
        m_cacheName(cacheName),
        m_callback(std::move(callback)) {}
  ~GetCacheForDeleteEntry() override {}

  void onSuccess(std::unique_ptr<WebServiceWorkerCache> cache) override {
    auto deleteRequest =
        WTF::makeUnique<DeleteCacheEntry>(std::move(m_callback));
    BatchOperation deleteOperation;
    deleteOperation.operationType = WebServiceWorkerCache::OperationTypeDelete;
    deleteOperation.request.setURL(KURL(ParsedURLString, m_requestSpec));
    Vector<BatchOperation> operations;
    operations.push_back(deleteOperation);
    cache.release()->dispatchBatch(std::move(deleteRequest),
                                   WebVector<BatchOperation>(operations));
  }

  void onError(WebServiceWorkerCacheError error) override {
    m_callback->sendFailure(Response::Error(String::format(
        "Error requesting cache %s: %s", m_cacheName.utf8().data(),
        serviceWorkerCacheErrorString(error).data())));
  }

 private:
  String m_requestSpec;
  String m_cacheName;
  std::unique_ptr<DeleteEntryCallback> m_callback;
};

}  // namespace

InspectorCacheStorageAgent::InspectorCacheStorageAgent() = default;

InspectorCacheStorageAgent::~InspectorCacheStorageAgent() = default;

DEFINE_TRACE(InspectorCacheStorageAgent) {
  InspectorBaseAgent::trace(visitor);
}

void InspectorCacheStorageAgent::requestCacheNames(
    const String& securityOrigin,
    std::unique_ptr<RequestCacheNamesCallback> callback) {
  RefPtr<SecurityOrigin> secOrigin =
      SecurityOrigin::createFromString(securityOrigin);

  // Cache Storage API is restricted to trustworthy origins.
  if (!secOrigin->isPotentiallyTrustworthy()) {
    // Don't treat this as an error, just don't attempt to open and enumerate
    // the caches.
    callback->sendSuccess(Array<protocol::CacheStorage::Cache>::create());
    return;
  }

  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  Response response = assertCacheStorage(securityOrigin, cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache->dispatchKeys(
      WTF::makeUnique<RequestCacheNames>(securityOrigin, std::move(callback)));
}

void InspectorCacheStorageAgent::requestEntries(
    const String& cacheId,
    int skipCount,
    int pageSize,
    std::unique_ptr<RequestEntriesCallback> callback) {
  String cacheName;
  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  Response response =
      assertCacheStorageAndNameForId(cacheId, &cacheName, cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  DataRequestParams params;
  params.cacheName = cacheName;
  params.pageSize = pageSize;
  params.skipCount = skipCount;
  cache->dispatchOpen(
      WTF::makeUnique<GetCacheForRequestData>(params, std::move(callback)),
      WebString(cacheName));
}

void InspectorCacheStorageAgent::deleteCache(
    const String& cacheId,
    std::unique_ptr<DeleteCacheCallback> callback) {
  String cacheName;
  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  Response response =
      assertCacheStorageAndNameForId(cacheId, &cacheName, cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache->dispatchDelete(WTF::makeUnique<DeleteCache>(std::move(callback)),
                        WebString(cacheName));
}

void InspectorCacheStorageAgent::deleteEntry(
    const String& cacheId,
    const String& request,
    std::unique_ptr<DeleteEntryCallback> callback) {
  String cacheName;
  std::unique_ptr<WebServiceWorkerCacheStorage> cache;
  Response response =
      assertCacheStorageAndNameForId(cacheId, &cacheName, cache);
  if (!response.isSuccess()) {
    callback->sendFailure(response);
    return;
  }
  cache->dispatchOpen(WTF::makeUnique<GetCacheForDeleteEntry>(
                          request, cacheName, std::move(callback)),
                      WebString(cacheName));
}

}  // namespace blink
