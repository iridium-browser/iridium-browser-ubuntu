// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/CacheStorage.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/cachestorage/CacheStorageError.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheStorage.h"
#include "wtf/PtrUtil.h"
#include <memory>
#include <utility>

namespace blink {

namespace {

DOMException* createNoImplementationException() {
  return DOMException::create(NotSupportedError,
                              "No CacheStorage implementation provided.");
}

bool commonChecks(ScriptState* scriptState, ExceptionState& exceptionState) {
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!executionContext)
    return false;

  String errorMessage;
  if (!executionContext->isSecureContext(errorMessage)) {
    exceptionState.throwSecurityError(errorMessage);
    return false;
  }
  return true;
}

}  // namespace

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::Callbacks final
    : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
  WTF_MAKE_NONCOPYABLE(Callbacks);

 public:
  explicit Callbacks(ScriptPromiseResolver* resolver) : m_resolver(resolver) {}
  ~Callbacks() override {}

  void onSuccess() override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    m_resolver->resolve(true);
    m_resolver.clear();
  }

  void onError(WebServiceWorkerCacheError reason) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    if (reason == WebServiceWorkerCacheErrorNotFound)
      m_resolver->resolve(false);
    else
      m_resolver->reject(CacheStorageError::createException(reason));
    m_resolver.clear();
  }

 private:
  Persistent<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::WithCacheCallbacks final
    : public WebServiceWorkerCacheStorage::CacheStorageWithCacheCallbacks {
  WTF_MAKE_NONCOPYABLE(WithCacheCallbacks);

 public:
  WithCacheCallbacks(const String& cacheName,
                     CacheStorage* cacheStorage,
                     ScriptPromiseResolver* resolver)
      : m_cacheName(cacheName),
        m_cacheStorage(cacheStorage),
        m_resolver(resolver) {}
  ~WithCacheCallbacks() override {}

  void onSuccess(std::unique_ptr<WebServiceWorkerCache> webCache) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    Cache* cache = Cache::create(m_cacheStorage->m_scopedFetcher,
                                 WTF::wrapUnique(webCache.release()));
    m_resolver->resolve(cache);
    m_resolver.clear();
  }

  void onError(WebServiceWorkerCacheError reason) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    if (reason == WebServiceWorkerCacheErrorNotFound)
      m_resolver->resolve();
    else
      m_resolver->reject(CacheStorageError::createException(reason));
    m_resolver.clear();
  }

 private:
  String m_cacheName;
  Persistent<CacheStorage> m_cacheStorage;
  Persistent<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::MatchCallbacks
    : public WebServiceWorkerCacheStorage::CacheStorageMatchCallbacks {
  WTF_MAKE_NONCOPYABLE(MatchCallbacks);

 public:
  explicit MatchCallbacks(ScriptPromiseResolver* resolver)
      : m_resolver(resolver) {}

  void onSuccess(const WebServiceWorkerResponse& webResponse) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    ScriptState::Scope scope(m_resolver->getScriptState());
    m_resolver->resolve(
        Response::create(m_resolver->getScriptState(), webResponse));
    m_resolver.clear();
  }

  void onError(WebServiceWorkerCacheError reason) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    if (reason == WebServiceWorkerCacheErrorNotFound ||
        reason == WebServiceWorkerCacheErrorCacheNameNotFound)
      m_resolver->resolve();
    else
      m_resolver->reject(CacheStorageError::createException(reason));
    m_resolver.clear();
  }

 private:
  Persistent<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::DeleteCallbacks final
    : public WebServiceWorkerCacheStorage::CacheStorageCallbacks {
  WTF_MAKE_NONCOPYABLE(DeleteCallbacks);

 public:
  DeleteCallbacks(const String& cacheName,
                  CacheStorage* cacheStorage,
                  ScriptPromiseResolver* resolver)
      : m_cacheName(cacheName),
        m_cacheStorage(cacheStorage),
        m_resolver(resolver) {}
  ~DeleteCallbacks() override {}

  void onSuccess() override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    m_resolver->resolve(true);
    m_resolver.clear();
  }

  void onError(WebServiceWorkerCacheError reason) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    if (reason == WebServiceWorkerCacheErrorNotFound)
      m_resolver->resolve(false);
    else
      m_resolver->reject(CacheStorageError::createException(reason));
    m_resolver.clear();
  }

 private:
  String m_cacheName;
  Persistent<CacheStorage> m_cacheStorage;
  Persistent<ScriptPromiseResolver> m_resolver;
};

// FIXME: Consider using CallbackPromiseAdapter.
class CacheStorage::KeysCallbacks final
    : public WebServiceWorkerCacheStorage::CacheStorageKeysCallbacks {
  WTF_MAKE_NONCOPYABLE(KeysCallbacks);

 public:
  explicit KeysCallbacks(ScriptPromiseResolver* resolver)
      : m_resolver(resolver) {}
  ~KeysCallbacks() override {}

  void onSuccess(const WebVector<WebString>& keys) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    Vector<String> wtfKeys;
    for (size_t i = 0; i < keys.size(); ++i)
      wtfKeys.push_back(keys[i]);
    m_resolver->resolve(wtfKeys);
    m_resolver.clear();
  }

  void onError(WebServiceWorkerCacheError reason) override {
    if (!m_resolver->getExecutionContext() ||
        m_resolver->getExecutionContext()->isContextDestroyed())
      return;
    m_resolver->reject(CacheStorageError::createException(reason));
    m_resolver.clear();
  }

 private:
  Persistent<ScriptPromiseResolver> m_resolver;
};

CacheStorage* CacheStorage::create(
    GlobalFetch::ScopedFetcher* fetcher,
    WebServiceWorkerCacheStorage* webCacheStorage) {
  return new CacheStorage(fetcher, WTF::wrapUnique(webCacheStorage));
}

ScriptPromise CacheStorage::open(ScriptState* scriptState,
                                 const String& cacheName,
                                 ExceptionState& exceptionState) {
  if (!commonChecks(scriptState, exceptionState))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  const ScriptPromise promise = resolver->promise();

  if (m_webCacheStorage) {
    m_webCacheStorage->dispatchOpen(
        WTF::makeUnique<WithCacheCallbacks>(cacheName, this, resolver),
        cacheName);
  } else {
    resolver->reject(createNoImplementationException());
  }

  return promise;
}

ScriptPromise CacheStorage::has(ScriptState* scriptState,
                                const String& cacheName,
                                ExceptionState& exceptionState) {
  if (!commonChecks(scriptState, exceptionState))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  const ScriptPromise promise = resolver->promise();

  if (m_webCacheStorage) {
    m_webCacheStorage->dispatchHas(WTF::makeUnique<Callbacks>(resolver),
                                   cacheName);
  } else {
    resolver->reject(createNoImplementationException());
  }

  return promise;
}

ScriptPromise CacheStorage::deleteFunction(ScriptState* scriptState,
                                           const String& cacheName,
                                           ExceptionState& exceptionState) {
  if (!commonChecks(scriptState, exceptionState))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  const ScriptPromise promise = resolver->promise();

  if (m_webCacheStorage) {
    m_webCacheStorage->dispatchDelete(
        WTF::makeUnique<DeleteCallbacks>(cacheName, this, resolver), cacheName);
  } else {
    resolver->reject(createNoImplementationException());
  }

  return promise;
}

ScriptPromise CacheStorage::keys(ScriptState* scriptState,
                                 ExceptionState& exceptionState) {
  if (!commonChecks(scriptState, exceptionState))
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  const ScriptPromise promise = resolver->promise();

  if (m_webCacheStorage)
    m_webCacheStorage->dispatchKeys(WTF::makeUnique<KeysCallbacks>(resolver));
  else
    resolver->reject(createNoImplementationException());

  return promise;
}

ScriptPromise CacheStorage::match(ScriptState* scriptState,
                                  const RequestInfo& request,
                                  const CacheQueryOptions& options,
                                  ExceptionState& exceptionState) {
  ASSERT(!request.isNull());
  if (!commonChecks(scriptState, exceptionState))
    return ScriptPromise();

  if (request.isRequest())
    return matchImpl(scriptState, request.getAsRequest(), options);
  Request* newRequest =
      Request::create(scriptState, request.getAsUSVString(), exceptionState);
  if (exceptionState.hadException())
    return ScriptPromise();
  return matchImpl(scriptState, newRequest, options);
}

ScriptPromise CacheStorage::matchImpl(ScriptState* scriptState,
                                      const Request* request,
                                      const CacheQueryOptions& options) {
  WebServiceWorkerRequest webRequest;
  request->populateWebServiceWorkerRequest(webRequest);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  const ScriptPromise promise = resolver->promise();

  if (request->method() != HTTPNames::GET && !options.ignoreMethod()) {
    resolver->resolve();
    return promise;
  }

  if (m_webCacheStorage)
    m_webCacheStorage->dispatchMatch(WTF::makeUnique<MatchCallbacks>(resolver),
                                     webRequest,
                                     Cache::toWebQueryParams(options));
  else
    resolver->reject(createNoImplementationException());

  return promise;
}

CacheStorage::CacheStorage(
    GlobalFetch::ScopedFetcher* fetcher,
    std::unique_ptr<WebServiceWorkerCacheStorage> webCacheStorage)
    : m_scopedFetcher(fetcher), m_webCacheStorage(std::move(webCacheStorage)) {}

CacheStorage::~CacheStorage() {}

void CacheStorage::dispose() {
  m_webCacheStorage.reset();
}

DEFINE_TRACE(CacheStorage) {
  visitor->trace(m_scopedFetcher);
}

}  // namespace blink
