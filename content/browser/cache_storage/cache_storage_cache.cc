// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_blob_to_disk_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/referrer.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponseType.h"

namespace content {

namespace {

// This class ensures that the cache and the entry have a lifetime as long as
// the blob that is created to contain them.
class CacheStorageCacheDataHandle
    : public storage::BlobDataBuilder::DataHandle {
 public:
  CacheStorageCacheDataHandle(
      std::unique_ptr<CacheStorageCacheHandle> cache_handle,
      disk_cache::ScopedEntryPtr entry)
      : cache_handle_(std::move(cache_handle)), entry_(std::move(entry)) {}

 private:
  ~CacheStorageCacheDataHandle() override {}

  std::unique_ptr<CacheStorageCacheHandle> cache_handle_;
  disk_cache::ScopedEntryPtr entry_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageCacheDataHandle);
};

typedef base::Callback<void(std::unique_ptr<CacheMetadata>)> MetadataCallback;

// The maximum size of each cache. Ultimately, cache size
// is controlled per-origin by the QuotaManager.
const int kMaxCacheBytes = std::numeric_limits<int>::max();

blink::WebServiceWorkerResponseType ProtoResponseTypeToWebResponseType(
    CacheResponse::ResponseType response_type) {
  switch (response_type) {
    case CacheResponse::BASIC_TYPE:
      return blink::WebServiceWorkerResponseTypeBasic;
    case CacheResponse::CORS_TYPE:
      return blink::WebServiceWorkerResponseTypeCORS;
    case CacheResponse::DEFAULT_TYPE:
      return blink::WebServiceWorkerResponseTypeDefault;
    case CacheResponse::ERROR_TYPE:
      return blink::WebServiceWorkerResponseTypeError;
    case CacheResponse::OPAQUE_TYPE:
      return blink::WebServiceWorkerResponseTypeOpaque;
    case CacheResponse::OPAQUE_REDIRECT_TYPE:
      return blink::WebServiceWorkerResponseTypeOpaqueRedirect;
  }
  NOTREACHED();
  return blink::WebServiceWorkerResponseTypeOpaque;
}

CacheResponse::ResponseType WebResponseTypeToProtoResponseType(
    blink::WebServiceWorkerResponseType response_type) {
  switch (response_type) {
    case blink::WebServiceWorkerResponseTypeBasic:
      return CacheResponse::BASIC_TYPE;
    case blink::WebServiceWorkerResponseTypeCORS:
      return CacheResponse::CORS_TYPE;
    case blink::WebServiceWorkerResponseTypeDefault:
      return CacheResponse::DEFAULT_TYPE;
    case blink::WebServiceWorkerResponseTypeError:
      return CacheResponse::ERROR_TYPE;
    case blink::WebServiceWorkerResponseTypeOpaque:
      return CacheResponse::OPAQUE_TYPE;
    case blink::WebServiceWorkerResponseTypeOpaqueRedirect:
      return CacheResponse::OPAQUE_REDIRECT_TYPE;
  }
  NOTREACHED();
  return CacheResponse::OPAQUE_TYPE;
}

// Copy headers out of a cache entry and into a protobuf. The callback is
// guaranteed to be run.
void ReadMetadata(disk_cache::Entry* entry, const MetadataCallback& callback);
void ReadMetadataDidReadMetadata(disk_cache::Entry* entry,
                                 const MetadataCallback& callback,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 int rv);

bool VaryMatches(const ServiceWorkerHeaderMap& request,
                 const ServiceWorkerHeaderMap& cached_request,
                 const ServiceWorkerHeaderMap& response) {
  ServiceWorkerHeaderMap::const_iterator vary_iter = response.find("vary");
  if (vary_iter == response.end())
    return true;

  for (const std::string& trimmed :
       base::SplitString(vary_iter->second, ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (trimmed == "*")
      return false;

    ServiceWorkerHeaderMap::const_iterator request_iter = request.find(trimmed);
    ServiceWorkerHeaderMap::const_iterator cached_request_iter =
        cached_request.find(trimmed);

    // If the header exists in one but not the other, no match.
    if ((request_iter == request.end()) !=
        (cached_request_iter == cached_request.end()))
      return false;

    // If the header exists in one, it exists in both. Verify that the values
    // are equal.
    if (request_iter != request.end() &&
        request_iter->second != cached_request_iter->second)
      return false;
  }

  return true;
}

GURL RemoveQueryParam(const GURL& url) {
  url::Replacements<char> replacements;
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}

void ReadMetadata(disk_cache::Entry* entry, const MetadataCallback& callback) {
  DCHECK(entry);

  scoped_refptr<net::IOBufferWithSize> buffer(new net::IOBufferWithSize(
      entry->GetDataSize(CacheStorageCache::INDEX_HEADERS)));

  net::CompletionCallback read_header_callback =
      base::Bind(ReadMetadataDidReadMetadata, entry, callback, buffer);

  int read_rv =
      entry->ReadData(CacheStorageCache::INDEX_HEADERS, 0, buffer.get(),
                      buffer->size(), read_header_callback);

  if (read_rv != net::ERR_IO_PENDING)
    read_header_callback.Run(read_rv);
}

void ReadMetadataDidReadMetadata(disk_cache::Entry* entry,
                                 const MetadataCallback& callback,
                                 scoped_refptr<net::IOBufferWithSize> buffer,
                                 int rv) {
  if (rv != buffer->size()) {
    callback.Run(std::unique_ptr<CacheMetadata>());
    return;
  }

  std::unique_ptr<CacheMetadata> metadata(new CacheMetadata());

  if (!metadata->ParseFromArray(buffer->data(), buffer->size())) {
    callback.Run(std::unique_ptr<CacheMetadata>());
    return;
  }

  callback.Run(std::move(metadata));
}

}  // namespace

// The state needed to iterate all entries in the cache.
struct CacheStorageCache::OpenAllEntriesContext {
  OpenAllEntriesContext() : enumerated_entry(nullptr) {}
  ~OpenAllEntriesContext() {
    for (size_t i = 0, max = entries.size(); i < max; ++i) {
      if (entries[i])
        entries[i]->Close();
    }
    if (enumerated_entry)
      enumerated_entry->Close();
  }

  // The vector of open entries in the backend.
  Entries entries;

  // Used for enumerating cache entries.
  std::unique_ptr<disk_cache::Backend::Iterator> backend_iterator;
  disk_cache::Entry* enumerated_entry;

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenAllEntriesContext);
};

// The state needed to pass between CacheStorageCache::Put callbacks.
struct CacheStorageCache::PutContext {
  PutContext(std::unique_ptr<ServiceWorkerFetchRequest> request,
             std::unique_ptr<ServiceWorkerResponse> response,
             std::unique_ptr<storage::BlobDataHandle> blob_data_handle,
             const CacheStorageCache::ErrorCallback& callback)
      : request(std::move(request)),
        response(std::move(response)),
        blob_data_handle(std::move(blob_data_handle)),
        callback(callback) {}

  // Input parameters to the Put function.
  std::unique_ptr<ServiceWorkerFetchRequest> request;
  std::unique_ptr<ServiceWorkerResponse> response;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
  CacheStorageCache::ErrorCallback callback;
  disk_cache::ScopedEntryPtr cache_entry;

 private:
  DISALLOW_COPY_AND_ASSIGN(PutContext);
};

struct CacheStorageCache::QueryCacheResults {
  QueryCacheResults(std::unique_ptr<ServiceWorkerFetchRequest> request,
                    const CacheStorageCacheQueryParams& options,
                    const QueryCacheResultsCallback& callback)
      : request(std::move(request)),
        options(options),
        callback(callback),
        out_requests(new Requests),
        out_responses(new Responses),
        out_blob_data_handles(new BlobDataHandles) {}

  std::unique_ptr<ServiceWorkerFetchRequest> request;
  CacheStorageCacheQueryParams options;
  QueryCacheResultsCallback callback;
  QueryCacheType query_type;

  std::unique_ptr<Requests> out_requests;
  std::unique_ptr<Responses> out_responses;
  std::unique_ptr<BlobDataHandles> out_blob_data_handles;
  std::vector<disk_cache::ScopedEntryPtr> out_entries;

  std::unique_ptr<OpenAllEntriesContext> entries_context;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueryCacheResults);
};

// static
std::unique_ptr<CacheStorageCache> CacheStorageCache::CreateMemoryCache(
    const GURL& origin,
    const std::string& cache_name,
    CacheStorage* cache_storage,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  CacheStorageCache* cache =
      new CacheStorageCache(origin, cache_name, base::FilePath(), cache_storage,
                            std::move(request_context_getter),
                            std::move(quota_manager_proxy), blob_context);
  cache->InitBackend();
  return base::WrapUnique(cache);
}

// static
std::unique_ptr<CacheStorageCache> CacheStorageCache::CreatePersistentCache(
    const GURL& origin,
    const std::string& cache_name,
    CacheStorage* cache_storage,
    const base::FilePath& path,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  CacheStorageCache* cache =
      new CacheStorageCache(origin, cache_name, path, cache_storage,
                            std::move(request_context_getter),
                            std::move(quota_manager_proxy), blob_context);
  cache->InitBackend();
  return base::WrapUnique(cache);
}

base::WeakPtr<CacheStorageCache> CacheStorageCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CacheStorageCache::Match(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& match_params,
    const ResponseCallback& callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE,
                 std::unique_ptr<ServiceWorkerResponse>(),
                 std::unique_ptr<storage::BlobDataHandle>());
    return;
  }

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorageCache::MatchImpl, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(request)), match_params,
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorageCache::MatchAll(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& match_params,
    const ResponsesCallback& callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE, std::unique_ptr<Responses>(),
                 std::unique_ptr<BlobDataHandles>());
    return;
  }

  scheduler_->ScheduleOperation(base::Bind(
      &CacheStorageCache::MatchAllImpl, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(std::move(request)), match_params,
      scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorageCache::WriteSideData(const ErrorCallback& callback,
                                      const GURL& url,
                                      base::Time expected_response_time,
                                      scoped_refptr<net::IOBuffer> buffer,
                                      int buf_len) {
  if (backend_state_ == BACKEND_CLOSED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, CACHE_STORAGE_ERROR_STORAGE));
    return;
  }

  // GetUsageAndQuota is called before entering a scheduled operation since it
  // can call Size, another scheduled operation.
  quota_manager_proxy_->GetUsageAndQuota(
      base::ThreadTaskRunnerHandle::Get().get(), origin_,
      storage::kStorageTypeTemporary,
      base::Bind(&CacheStorageCache::WriteSideDataDidGetQuota,
                 weak_ptr_factory_.GetWeakPtr(), callback, url,
                 expected_response_time, buffer, buf_len));
}

void CacheStorageCache::BatchOperation(
    const std::vector<CacheStorageBatchOperation>& operations,
    const ErrorCallback& callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, CACHE_STORAGE_ERROR_STORAGE));
    return;
  }

  // Estimate the required size of the put operations. The size of the deletes
  // is unknown and not considered.
  int64_t space_required = 0;
  for (const auto& operation : operations) {
    if (operation.operation_type == CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT) {
      space_required +=
          operation.request.blob_size + operation.response.blob_size;
    }
  }
  if (space_required > 0) {
    // GetUsageAndQuota is called before entering a scheduled operation since it
    // can call Size, another scheduled operation. This is racy. The decision
    // to commit is made before the scheduled Put operation runs. By the time
    // Put runs, the cache might already be full and the origin will be larger
    // than it's supposed to be.
    quota_manager_proxy_->GetUsageAndQuota(
        base::ThreadTaskRunnerHandle::Get().get(), origin_,
        storage::kStorageTypeTemporary,
        base::Bind(&CacheStorageCache::BatchDidGetUsageAndQuota,
                   weak_ptr_factory_.GetWeakPtr(), operations, callback,
                   space_required));
    return;
  }

  BatchDidGetUsageAndQuota(operations, callback, 0 /* space_required */,
                           storage::kQuotaStatusOk, 0 /* usage */,
                           0 /* quota */);
}

void CacheStorageCache::BatchDidGetUsageAndQuota(
    const std::vector<CacheStorageBatchOperation>& operations,
    const ErrorCallback& callback,
    int64_t space_required,
    storage::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  if (status_code != storage::kQuotaStatusOk ||
      space_required > quota - usage) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, CACHE_STORAGE_ERROR_QUOTA_EXCEEDED));
    return;
  }

  std::unique_ptr<ErrorCallback> callback_copy(new ErrorCallback(callback));
  ErrorCallback* callback_ptr = callback_copy.get();
  base::Closure barrier_closure = base::BarrierClosure(
      operations.size(), base::Bind(&CacheStorageCache::BatchDidAllOperations,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    base::Passed(std::move(callback_copy))));
  ErrorCallback completion_callback =
      base::Bind(&CacheStorageCache::BatchDidOneOperation,
                 weak_ptr_factory_.GetWeakPtr(), barrier_closure, callback_ptr);

  for (const auto& operation : operations) {
    switch (operation.operation_type) {
      case CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT:
        Put(operation, completion_callback);
        break;
      case CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE:
        DCHECK_EQ(1u, operations.size());
        Delete(operation, completion_callback);
        break;
      case CACHE_STORAGE_CACHE_OPERATION_TYPE_UNDEFINED:
        NOTREACHED();
        // TODO(nhiroki): This should return "TypeError".
        // http://crbug.com/425505
        completion_callback.Run(CACHE_STORAGE_ERROR_STORAGE);
        break;
    }
  }
}

void CacheStorageCache::BatchDidOneOperation(
    const base::Closure& barrier_closure,
    ErrorCallback* callback,
    CacheStorageError error) {
  if (callback->is_null() || error == CACHE_STORAGE_OK) {
    barrier_closure.Run();
    return;
  }
  callback->Run(error);
  callback->Reset();  // Only call the callback once.

  barrier_closure.Run();
}

void CacheStorageCache::BatchDidAllOperations(
    std::unique_ptr<ErrorCallback> callback) {
  if (callback->is_null())
    return;
  callback->Run(CACHE_STORAGE_OK);
}

void CacheStorageCache::Keys(std::unique_ptr<ServiceWorkerFetchRequest> request,
                             const CacheStorageCacheQueryParams& options,
                             const RequestsCallback& callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE, std::unique_ptr<Requests>());
    return;
  }

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorageCache::KeysImpl, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(request)), options,
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorageCache::Close(const base::Closure& callback) {
  DCHECK_NE(BACKEND_CLOSED, backend_state_)
      << "Was CacheStorageCache::Close() called twice?";

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorageCache::CloseImpl, weak_ptr_factory_.GetWeakPtr(),
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorageCache::Size(const SizeCallback& callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    // TODO(jkarlin): Delete caches that can't be initialized.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, 0));
    return;
  }

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorageCache::SizeImpl, weak_ptr_factory_.GetWeakPtr(),
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorageCache::GetSizeThenClose(const SizeCallback& callback) {
  if (backend_state_ == BACKEND_CLOSED) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, 0));
    return;
  }

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorageCache::SizeImpl, weak_ptr_factory_.GetWeakPtr(),
                 base::Bind(&CacheStorageCache::GetSizeThenCloseDidGetSize,
                            weak_ptr_factory_.GetWeakPtr(),
                            scheduler_->WrapCallbackToRunNext(callback))));
}

CacheStorageCache::~CacheStorageCache() {
  quota_manager_proxy_->NotifyOriginNoLongerInUse(origin_);
}

CacheStorageCache::CacheStorageCache(
    const GURL& origin,
    const std::string& cache_name,
    const base::FilePath& path,
    CacheStorage* cache_storage,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : origin_(origin),
      cache_name_(cache_name),
      path_(path),
      cache_storage_(cache_storage),
      request_context_getter_(std::move(request_context_getter)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      blob_storage_context_(blob_context),
      scheduler_(
          new CacheStorageScheduler(CacheStorageSchedulerClient::CLIENT_CACHE)),
      memory_only_(path.empty()),
      weak_ptr_factory_(this) {
  DCHECK(!origin_.is_empty());
  DCHECK(quota_manager_proxy_.get());

  quota_manager_proxy_->NotifyOriginInUse(origin_);
}

void CacheStorageCache::OpenAllEntries(const OpenAllEntriesCallback& callback) {
  std::unique_ptr<OpenAllEntriesContext> entries_context(
      new OpenAllEntriesContext);
  entries_context->backend_iterator = backend_->CreateIterator();
  disk_cache::Backend::Iterator& iterator = *entries_context->backend_iterator;
  disk_cache::Entry** enumerated_entry = &entries_context->enumerated_entry;

  net::CompletionCallback open_entry_callback = base::Bind(
      &CacheStorageCache::DidOpenNextEntry, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(std::move(entries_context)), callback);

  int rv = iterator.OpenNextEntry(enumerated_entry, open_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void CacheStorageCache::DidOpenNextEntry(
    std::unique_ptr<OpenAllEntriesContext> entries_context,
    const OpenAllEntriesCallback& callback,
    int rv) {
  if (rv == net::ERR_FAILED) {
    DCHECK(!entries_context->enumerated_entry);
    // Enumeration is complete, extract the requests from the entries.
    callback.Run(std::move(entries_context), CACHE_STORAGE_OK);
    return;
  }

  if (rv < 0) {
    callback.Run(std::move(entries_context), CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  if (backend_state_ != BACKEND_OPEN) {
    callback.Run(std::move(entries_context), CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  // Store the entry.
  entries_context->entries.push_back(entries_context->enumerated_entry);
  entries_context->enumerated_entry = nullptr;

  // Enumerate the next entry.
  disk_cache::Backend::Iterator& iterator = *entries_context->backend_iterator;
  disk_cache::Entry** enumerated_entry = &entries_context->enumerated_entry;
  net::CompletionCallback open_entry_callback = base::Bind(
      &CacheStorageCache::DidOpenNextEntry, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(std::move(entries_context)), callback);

  rv = iterator.OpenNextEntry(enumerated_entry, open_entry_callback);

  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void CacheStorageCache::QueryCache(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& options,
    QueryCacheType query_type,
    const QueryCacheResultsCallback& callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE,
                 std::unique_ptr<QueryCacheResults>());
    return;
  }

  if (!options.ignore_method && request && !request->method.empty() &&
      request->method != "GET") {
    callback.Run(CACHE_STORAGE_OK, base::MakeUnique<QueryCacheResults>(
                                       std::move(request), options, callback));
    return;
  }

  ServiceWorkerFetchRequest* request_ptr = request.get();

  std::unique_ptr<QueryCacheResults> query_cache_results(
      new QueryCacheResults(std::move(request), options, callback));
  query_cache_results->query_type = query_type;

  if (query_cache_results->request &&
      !query_cache_results->request->url.is_empty() && !options.ignore_search) {
    // There is no need to scan the entire backend, just search for the exact
    // URL.
    std::unique_ptr<disk_cache::Entry*> entry =
        base::MakeUnique<disk_cache::Entry*>();
    disk_cache::Entry** entry_ptr = entry.get();

    net::CompletionCallback open_entry_callback =
        base::Bind(&CacheStorageCache::QueryCacheDidOpenEntry,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(std::move(query_cache_results)),
                   base::Passed(std::move(entry)));
    int rv = backend_->OpenEntry(request_ptr->url.spec(), entry_ptr,
                                 open_entry_callback);
    if (rv != net::ERR_IO_PENDING)
      open_entry_callback.Run(rv);
    return;
  }

  OpenAllEntries(base::Bind(&CacheStorageCache::QueryCacheDidOpenAllEntries,
                            weak_ptr_factory_.GetWeakPtr(),
                            base::Passed(std::move(query_cache_results))));
}

void CacheStorageCache::QueryCacheDidOpenEntry(
    std::unique_ptr<QueryCacheResults> query_cache_results,
    std::unique_ptr<disk_cache::Entry*> entry,
    int rv) {
  if (rv != net::OK) {
    QueryCacheResults* results = query_cache_results.get();
    results->callback.Run(CACHE_STORAGE_OK, std::move(query_cache_results));
    return;
  }

  std::unique_ptr<OpenAllEntriesContext> entries_context =
      base::MakeUnique<OpenAllEntriesContext>();
  entries_context->entries.push_back(*entry.get());
  QueryCacheDidOpenAllEntries(std::move(query_cache_results),
                              std::move(entries_context), CACHE_STORAGE_OK);
}

void CacheStorageCache::QueryCacheDidOpenAllEntries(
    std::unique_ptr<QueryCacheResults> query_cache_results,
    std::unique_ptr<OpenAllEntriesContext> entries_context,
    CacheStorageError error) {
  if (error != CACHE_STORAGE_OK) {
    query_cache_results->callback.Run(error,
                                      std::unique_ptr<QueryCacheResults>());
    return;
  }

  query_cache_results->entries_context.swap(entries_context);
  Entries::iterator iter =
      query_cache_results->entries_context->entries.begin();
  QueryCacheProcessNextEntry(std::move(query_cache_results), iter);
}

void CacheStorageCache::QueryCacheProcessNextEntry(
    std::unique_ptr<QueryCacheResults> query_cache_results,
    const Entries::iterator& iter) {
  if (iter == query_cache_results->entries_context->entries.end()) {
    QueryCacheResultsCallback success_callback = query_cache_results->callback;
    success_callback.Run(CACHE_STORAGE_OK, std::move(query_cache_results));
    return;
  }

  if (query_cache_results->request &&
      !query_cache_results->request->url.is_empty()) {
    disk_cache::Entry* entry(*iter);
    GURL requestURL = query_cache_results->request->url;
    GURL cachedURL = GURL(entry->GetKey());

    if (query_cache_results->options.ignore_search) {
      requestURL = RemoveQueryParam(requestURL);
      cachedURL = RemoveQueryParam(cachedURL);
    }

    if (cachedURL != requestURL) {
      QueryCacheProcessNextEntry(std::move(query_cache_results), iter + 1);
      return;
    }
  }

  ReadMetadata(*iter,
               base::Bind(&CacheStorageCache::QueryCacheDidReadMetadata,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::Passed(std::move(query_cache_results)), iter));
}

void CacheStorageCache::QueryCacheDidReadMetadata(
    std::unique_ptr<QueryCacheResults> query_cache_results,
    const Entries::iterator& iter,
    std::unique_ptr<CacheMetadata> metadata) {
  disk_cache::ScopedEntryPtr entry(*iter);
  *iter = nullptr;

  if (!metadata) {
    entry->Doom();
    QueryCacheProcessNextEntry(std::move(query_cache_results), iter + 1);
    return;
  }

  ServiceWorkerFetchRequest request;
  PopulateRequestFromMetadata(*metadata, GURL(entry->GetKey()), &request);

  ServiceWorkerResponse response;
  PopulateResponseMetadata(*metadata, &response);

  if (query_cache_results->request &&
      !query_cache_results->options.ignore_vary &&
      !VaryMatches(query_cache_results->request->headers, request.headers,
                   response.headers)) {
    QueryCacheProcessNextEntry(std::move(query_cache_results), iter + 1);
    return;
  }

  if (query_cache_results->query_type == QueryCacheType::CACHE_ENTRIES) {
    query_cache_results->out_entries.push_back(std::move(entry));
    QueryCacheProcessNextEntry(std::move(query_cache_results), iter + 1);
    return;
  }
  DCHECK_EQ(QueryCacheType::REQUESTS_AND_RESPONSES,
            query_cache_results->query_type);

  query_cache_results->out_requests->push_back(request);

  if (entry->GetDataSize(INDEX_RESPONSE_BODY) == 0) {
    query_cache_results->out_responses->push_back(response);
    QueryCacheProcessNextEntry(std::move(query_cache_results), iter + 1);
    return;
  }

  if (!blob_storage_context_) {
    query_cache_results->callback.Run(CACHE_STORAGE_ERROR_STORAGE,
                                      std::unique_ptr<QueryCacheResults>());
    return;
  }

  std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
      PopulateResponseBody(std::move(entry), &response);

  query_cache_results->out_responses->push_back(response);
  query_cache_results->out_blob_data_handles->push_back(*blob_data_handle);
  QueryCacheProcessNextEntry(std::move(query_cache_results), iter + 1);
}

void CacheStorageCache::MatchImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& match_params,
    const ResponseCallback& callback) {
  MatchAllImpl(std::move(request), match_params,
               base::Bind(&CacheStorageCache::MatchDidMatchAll,
                          weak_ptr_factory_.GetWeakPtr(), callback));
}

void CacheStorageCache::MatchDidMatchAll(
    const ResponseCallback& callback,
    CacheStorageError match_all_error,
    std::unique_ptr<Responses> match_all_responses,
    std::unique_ptr<BlobDataHandles> match_all_handles) {
  if (match_all_error != CACHE_STORAGE_OK) {
    callback.Run(match_all_error, std::unique_ptr<ServiceWorkerResponse>(),
                 std::unique_ptr<storage::BlobDataHandle>());
    return;
  }

  if (match_all_responses->empty()) {
    callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND,
                 std::unique_ptr<ServiceWorkerResponse>(),
                 std::unique_ptr<storage::BlobDataHandle>());
    return;
  }

  std::unique_ptr<ServiceWorkerResponse> response =
      base::MakeUnique<ServiceWorkerResponse>(match_all_responses->at(0));

  std::unique_ptr<storage::BlobDataHandle> data_handle;
  if (response->blob_size > 0) {
    // NOTE: This assumes that MatchAll returns the handles in the same order
    // as the responses.
    data_handle =
        base::MakeUnique<storage::BlobDataHandle>(match_all_handles->at(0));
  }

  callback.Run(CACHE_STORAGE_OK, std::move(response), std::move(data_handle));
}

void CacheStorageCache::MatchAllImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& options,
    const ResponsesCallback& callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE, std::unique_ptr<Responses>(),
                 std::unique_ptr<BlobDataHandles>());
    return;
  }

  QueryCache(std::move(request), options,
             QueryCacheType::REQUESTS_AND_RESPONSES,
             base::Bind(&CacheStorageCache::MatchAllDidQueryCache,
                        weak_ptr_factory_.GetWeakPtr(), callback));
}

void CacheStorageCache::MatchAllDidQueryCache(
    const ResponsesCallback& callback,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  if (error != CACHE_STORAGE_OK) {
    callback.Run(error, std::unique_ptr<Responses>(),
                 std::unique_ptr<BlobDataHandles>());
    return;
  }

  callback.Run(CACHE_STORAGE_OK, std::move(query_cache_results->out_responses),
               std::move(query_cache_results->out_blob_data_handles));
}

void CacheStorageCache::WriteSideDataDidGetQuota(
    const ErrorCallback& callback,
    const GURL& url,
    base::Time expected_response_time,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    storage::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  if (status_code != storage::kQuotaStatusOk || (buf_len > quota - usage)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, CACHE_STORAGE_ERROR_QUOTA_EXCEEDED));
    return;
  }

  scheduler_->ScheduleOperation(base::Bind(
      &CacheStorageCache::WriteSideDataImpl, weak_ptr_factory_.GetWeakPtr(),
      scheduler_->WrapCallbackToRunNext(callback), url, expected_response_time,
      buffer, buf_len));
}

void CacheStorageCache::WriteSideDataImpl(const ErrorCallback& callback,
                                          const GURL& url,
                                          base::Time expected_response_time,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          int buf_len) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  std::unique_ptr<disk_cache::Entry*> scoped_entry_ptr(
      new disk_cache::Entry*());
  disk_cache::Entry** entry_ptr = scoped_entry_ptr.get();
  net::CompletionCallback open_entry_callback = base::Bind(
      &CacheStorageCache::WriteSideDataDidOpenEntry,
      weak_ptr_factory_.GetWeakPtr(), callback, expected_response_time, buffer,
      buf_len, base::Passed(std::move(scoped_entry_ptr)));

  int rv = backend_->OpenEntry(url.spec(), entry_ptr, open_entry_callback);
  if (rv != net::ERR_IO_PENDING)
    open_entry_callback.Run(rv);
}

void CacheStorageCache::WriteSideDataDidOpenEntry(
    const ErrorCallback& callback,
    base::Time expected_response_time,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    std::unique_ptr<disk_cache::Entry*> entry_ptr,
    int rv) {
  if (rv != net::OK) {
    callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }
  disk_cache::ScopedEntryPtr entry(*entry_ptr);

  ReadMetadata(*entry_ptr,
               base::Bind(&CacheStorageCache::WriteSideDataDidReadMetaData,
                          weak_ptr_factory_.GetWeakPtr(), callback,
                          expected_response_time, buffer, buf_len,
                          base::Passed(std::move(entry))));
}

void CacheStorageCache::WriteSideDataDidReadMetaData(
    const ErrorCallback& callback,
    base::Time expected_response_time,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len,
    disk_cache::ScopedEntryPtr entry,
    std::unique_ptr<CacheMetadata> headers) {
  if (!headers ||
      headers->response().response_time() !=
          expected_response_time.ToInternalValue()) {
    callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }
  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* temp_entry_ptr = entry.get();

  net::CompletionCallback write_side_data_callback = base::Bind(
      &CacheStorageCache::WriteSideDataDidWrite, weak_ptr_factory_.GetWeakPtr(),
      callback, base::Passed(std::move(entry)), buf_len);

  int rv = temp_entry_ptr->WriteData(
      INDEX_SIDE_DATA, 0 /* offset */, buffer.get(), buf_len,
      write_side_data_callback, true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    write_side_data_callback.Run(rv);
}

void CacheStorageCache::WriteSideDataDidWrite(const ErrorCallback& callback,
                                              disk_cache::ScopedEntryPtr entry,
                                              int expected_bytes,
                                              int rv) {
  if (rv != expected_bytes) {
    entry->Doom();
    UpdateCacheSize();
    callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  UpdateCacheSize();
  callback.Run(CACHE_STORAGE_OK);
}

void CacheStorageCache::Put(const CacheStorageBatchOperation& operation,
                            const ErrorCallback& callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT, operation.operation_type);

  std::unique_ptr<ServiceWorkerFetchRequest> request(
      new ServiceWorkerFetchRequest(
          operation.request.url, operation.request.method,
          operation.request.headers, operation.request.referrer,
          operation.request.is_reload));

  // We don't support streaming for cache.
  DCHECK(operation.response.stream_url.is_empty());
  // We don't support the body of redirect response.
  DCHECK(!(operation.response.response_type ==
               blink::WebServiceWorkerResponseTypeOpaqueRedirect &&
           operation.response.blob_size));
  std::unique_ptr<ServiceWorkerResponse> response(new ServiceWorkerResponse(
      operation.response.url, operation.response.status_code,
      operation.response.status_text, operation.response.response_type,
      operation.response.headers, operation.response.blob_uuid,
      operation.response.blob_size, operation.response.stream_url,
      operation.response.error, operation.response.response_time,
      false /* is_in_cache_storage */,
      std::string() /* cache_storage_cache_name */,
      operation.response.cors_exposed_header_names));

  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;

  if (!response->blob_uuid.empty()) {
    if (!blob_storage_context_) {
      callback.Run(CACHE_STORAGE_ERROR_STORAGE);
      return;
    }
    blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response->blob_uuid);
    if (!blob_data_handle) {
      callback.Run(CACHE_STORAGE_ERROR_STORAGE);
      return;
    }
  }

  UMA_HISTOGRAM_ENUMERATION(
      "ServiceWorkerCache.Cache.AllWritesResponseType",
      operation.response.response_type,
      blink::WebServiceWorkerResponseType::WebServiceWorkerResponseTypeLast +
          1);

  std::unique_ptr<PutContext> put_context(new PutContext(
      std::move(request), std::move(response), std::move(blob_data_handle),
      scheduler_->WrapCallbackToRunNext(callback)));

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorageCache::PutImpl, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(put_context))));
}

void CacheStorageCache::PutImpl(std::unique_ptr<PutContext> put_context) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  std::string key = put_context->request->url.spec();

  net::CompletionCallback callback = base::Bind(
      &CacheStorageCache::PutDidDoomEntry, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(std::move(put_context)));

  int rv = backend_->DoomEntry(key, callback);
  if (rv != net::ERR_IO_PENDING)
    callback.Run(rv);
}

void CacheStorageCache::PutDidDoomEntry(std::unique_ptr<PutContext> put_context,
                                        int rv) {
  if (backend_state_ != BACKEND_OPEN) {
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  // |rv| is ignored as doom entry can fail if the entry doesn't exist.

  std::unique_ptr<disk_cache::Entry*> scoped_entry_ptr(
      new disk_cache::Entry*());
  disk_cache::Entry** entry_ptr = scoped_entry_ptr.get();
  ServiceWorkerFetchRequest* request_ptr = put_context->request.get();
  disk_cache::Backend* backend_ptr = backend_.get();

  net::CompletionCallback create_entry_callback = base::Bind(
      &CacheStorageCache::PutDidCreateEntry, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(std::move(scoped_entry_ptr)),
      base::Passed(std::move(put_context)));

  int create_rv = backend_ptr->CreateEntry(request_ptr->url.spec(), entry_ptr,
                                           create_entry_callback);

  if (create_rv != net::ERR_IO_PENDING)
    create_entry_callback.Run(create_rv);
}

void CacheStorageCache::PutDidCreateEntry(
    std::unique_ptr<disk_cache::Entry*> entry_ptr,
    std::unique_ptr<PutContext> put_context,
    int rv) {
  put_context->cache_entry.reset(*entry_ptr);

  if (rv != net::OK) {
    put_context->callback.Run(CACHE_STORAGE_ERROR_EXISTS);
    return;
  }

  CacheMetadata metadata;
  CacheRequest* request_metadata = metadata.mutable_request();
  request_metadata->set_method(put_context->request->method);
  for (ServiceWorkerHeaderMap::const_iterator it =
           put_context->request->headers.begin();
       it != put_context->request->headers.end(); ++it) {
    DCHECK_EQ(std::string::npos, it->first.find('\0'));
    DCHECK_EQ(std::string::npos, it->second.find('\0'));
    CacheHeaderMap* header_map = request_metadata->add_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }

  CacheResponse* response_metadata = metadata.mutable_response();
  response_metadata->set_status_code(put_context->response->status_code);
  response_metadata->set_status_text(put_context->response->status_text);
  response_metadata->set_response_type(
      WebResponseTypeToProtoResponseType(put_context->response->response_type));
  response_metadata->set_url(put_context->response->url.spec());
  response_metadata->set_response_time(
      put_context->response->response_time.ToInternalValue());
  for (ServiceWorkerHeaderMap::const_iterator it =
           put_context->response->headers.begin();
       it != put_context->response->headers.end(); ++it) {
    DCHECK_EQ(std::string::npos, it->first.find('\0'));
    DCHECK_EQ(std::string::npos, it->second.find('\0'));
    CacheHeaderMap* header_map = response_metadata->add_headers();
    header_map->set_name(it->first);
    header_map->set_value(it->second);
  }
  for (const auto& header : put_context->response->cors_exposed_header_names)
    response_metadata->add_cors_exposed_header_names(header);

  std::unique_ptr<std::string> serialized(new std::string());
  if (!metadata.SerializeToString(serialized.get())) {
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  scoped_refptr<net::StringIOBuffer> buffer(
      new net::StringIOBuffer(std::move(serialized)));

  // Get a temporary copy of the entry pointer before passing it in base::Bind.
  disk_cache::Entry* temp_entry_ptr = put_context->cache_entry.get();

  net::CompletionCallback write_headers_callback = base::Bind(
      &CacheStorageCache::PutDidWriteHeaders, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(std::move(put_context)), buffer->size());

  rv = temp_entry_ptr->WriteData(INDEX_HEADERS, 0 /* offset */, buffer.get(),
                                 buffer->size(), write_headers_callback,
                                 true /* truncate */);

  if (rv != net::ERR_IO_PENDING)
    write_headers_callback.Run(rv);
}

void CacheStorageCache::PutDidWriteHeaders(
    std::unique_ptr<PutContext> put_context,
    int expected_bytes,
    int rv) {
  if (rv != expected_bytes) {
    put_context->cache_entry->Doom();
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  // The metadata is written, now for the response content. The data is streamed
  // from the blob into the cache entry.

  if (put_context->response->blob_uuid.empty()) {
    UpdateCacheSize();
    put_context->callback.Run(CACHE_STORAGE_OK);
    return;
  }

  DCHECK(put_context->blob_data_handle);

  disk_cache::ScopedEntryPtr entry(std::move(put_context->cache_entry));
  put_context->cache_entry = NULL;

  CacheStorageBlobToDiskCache* blob_to_cache =
      new CacheStorageBlobToDiskCache();
  BlobToDiskCacheIDMap::KeyType blob_to_cache_key =
      active_blob_to_disk_cache_writers_.Add(blob_to_cache);

  std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
      std::move(put_context->blob_data_handle);

  blob_to_cache->StreamBlobToCache(
      std::move(entry), INDEX_RESPONSE_BODY, request_context_getter_.get(),
      std::move(blob_data_handle),
      base::Bind(&CacheStorageCache::PutDidWriteBlobToCache,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(put_context)), blob_to_cache_key));
}

void CacheStorageCache::PutDidWriteBlobToCache(
    std::unique_ptr<PutContext> put_context,
    BlobToDiskCacheIDMap::KeyType blob_to_cache_key,
    disk_cache::ScopedEntryPtr entry,
    bool success) {
  DCHECK(entry);
  put_context->cache_entry = std::move(entry);

  active_blob_to_disk_cache_writers_.Remove(blob_to_cache_key);

  if (!success) {
    put_context->cache_entry->Doom();
    put_context->callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  UpdateCacheSize();
  put_context->callback.Run(CACHE_STORAGE_OK);
}

void CacheStorageCache::UpdateCacheSize() {
  if (backend_state_ != BACKEND_OPEN)
    return;

  // Note that the callback holds a cache handle to keep the cache alive during
  // the operation since this UpdateCacheSize is often run after an operation
  // completes and runs its callback.
  int rv = backend_->CalculateSizeOfAllEntries(base::Bind(
      &CacheStorageCache::UpdateCacheSizeGotSize,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(CreateCacheHandle())));

  if (rv != net::ERR_IO_PENDING)
    UpdateCacheSizeGotSize(CreateCacheHandle(), rv);
}

void CacheStorageCache::UpdateCacheSizeGotSize(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    int current_cache_size) {
  int64_t old_cache_size = cache_size_;
  cache_size_ = current_cache_size;

  quota_manager_proxy_->NotifyStorageModified(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary, current_cache_size - old_cache_size);
}

void CacheStorageCache::Delete(const CacheStorageBatchOperation& operation,
                               const ErrorCallback& callback) {
  DCHECK(BACKEND_OPEN == backend_state_ || initializing_);
  DCHECK_EQ(CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE,
            operation.operation_type);

  std::unique_ptr<ServiceWorkerFetchRequest> request(
      new ServiceWorkerFetchRequest(
          operation.request.url, operation.request.method,
          operation.request.headers, operation.request.referrer,
          operation.request.is_reload));

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorageCache::DeleteImpl, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(request)), operation.match_params,
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorageCache::DeleteImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& match_params,
    const ErrorCallback& callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  QueryCache(std::move(request), match_params, QueryCacheType::CACHE_ENTRIES,
             base::Bind(&CacheStorageCache::DeleteDidQueryCache,
                        weak_ptr_factory_.GetWeakPtr(), callback));
}

void CacheStorageCache::DeleteDidQueryCache(
    const ErrorCallback& callback,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  if (error != CACHE_STORAGE_OK) {
    callback.Run(error);
    return;
  }

  if (query_cache_results->out_entries.empty()) {
    callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  for (auto& entry : query_cache_results->out_entries) {
    entry->Doom();
    entry.reset();
  }

  UpdateCacheSize();
  callback.Run(CACHE_STORAGE_OK);
}

void CacheStorageCache::KeysImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& options,
    const RequestsCallback& callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);
  if (backend_state_ != BACKEND_OPEN) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE, std::unique_ptr<Requests>());
    return;
  }

  QueryCache(std::move(request), options,
             QueryCacheType::REQUESTS_AND_RESPONSES,
             base::Bind(&CacheStorageCache::KeysDidQueryCache,
                        weak_ptr_factory_.GetWeakPtr(), callback));
}

void CacheStorageCache::KeysDidQueryCache(
    const RequestsCallback& callback,
    CacheStorageError error,
    std::unique_ptr<QueryCacheResults> query_cache_results) {
  if (error != CACHE_STORAGE_OK) {
    callback.Run(error, std::unique_ptr<Requests>());
    return;
  }

  callback.Run(CACHE_STORAGE_OK, std::move(query_cache_results->out_requests));
}

void CacheStorageCache::CloseImpl(const base::Closure& callback) {
  DCHECK_NE(BACKEND_CLOSED, backend_state_);

  backend_state_ = BACKEND_CLOSED;
  backend_.reset();
  callback.Run();
}

void CacheStorageCache::SizeImpl(const SizeCallback& callback) {
  DCHECK_NE(BACKEND_UNINITIALIZED, backend_state_);

  int64_t size = backend_state_ == BACKEND_OPEN ? cache_size_ : 0;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, size));
}

void CacheStorageCache::GetSizeThenCloseDidGetSize(const SizeCallback& callback,
                                                   int64_t cache_size) {
  CloseImpl(base::Bind(callback, cache_size));
}

void CacheStorageCache::CreateBackend(const ErrorCallback& callback) {
  DCHECK(!backend_);

  // Use APP_CACHE as opposed to DISK_CACHE to prevent cache eviction.
  net::CacheType cache_type = memory_only_ ? net::MEMORY_CACHE : net::APP_CACHE;

  std::unique_ptr<ScopedBackendPtr> backend_ptr(new ScopedBackendPtr());

  // Temporary pointer so that backend_ptr can be Pass()'d in Bind below.
  ScopedBackendPtr* backend = backend_ptr.get();

  net::CompletionCallback create_cache_callback =
      base::Bind(&CacheStorageCache::CreateBackendDidCreate,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 base::Passed(std::move(backend_ptr)));

  // TODO(jkarlin): Use the cache task runner that ServiceWorkerCacheCore
  // has for disk caches.
  int rv = disk_cache::CreateCacheBackend(
      cache_type, net::CACHE_BACKEND_SIMPLE, path_, kMaxCacheBytes,
      false, /* force */
      BrowserThread::GetTaskRunnerForThread(BrowserThread::CACHE).get(), NULL,
      backend, create_cache_callback);
  if (rv != net::ERR_IO_PENDING)
    create_cache_callback.Run(rv);
}

void CacheStorageCache::CreateBackendDidCreate(
    const CacheStorageCache::ErrorCallback& callback,
    std::unique_ptr<ScopedBackendPtr> backend_ptr,
    int rv) {
  if (rv != net::OK) {
    callback.Run(CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  backend_ = std::move(*backend_ptr);
  callback.Run(CACHE_STORAGE_OK);
}

void CacheStorageCache::InitBackend() {
  DCHECK_EQ(BACKEND_UNINITIALIZED, backend_state_);
  DCHECK(!initializing_);
  DCHECK(!scheduler_->ScheduledOperations());
  initializing_ = true;

  scheduler_->ScheduleOperation(base::Bind(
      &CacheStorageCache::CreateBackend, weak_ptr_factory_.GetWeakPtr(),
      base::Bind(
          &CacheStorageCache::InitDidCreateBackend,
          weak_ptr_factory_.GetWeakPtr(),
          scheduler_->WrapCallbackToRunNext(base::Bind(&base::DoNothing)))));
}

void CacheStorageCache::InitDidCreateBackend(
    const base::Closure& callback,
    CacheStorageError cache_create_error) {
  if (cache_create_error != CACHE_STORAGE_OK) {
    InitGotCacheSize(callback, cache_create_error, 0);
    return;
  }

  int rv = backend_->CalculateSizeOfAllEntries(
      base::Bind(&CacheStorageCache::InitGotCacheSize,
                 weak_ptr_factory_.GetWeakPtr(), callback, cache_create_error));

  if (rv != net::ERR_IO_PENDING)
    InitGotCacheSize(callback, cache_create_error, rv);
}

void CacheStorageCache::InitGotCacheSize(const base::Closure& callback,
                                         CacheStorageError cache_create_error,
                                         int cache_size) {
  cache_size_ = cache_size;
  initializing_ = false;
  backend_state_ = (cache_create_error == CACHE_STORAGE_OK && backend_ &&
                    backend_state_ == BACKEND_UNINITIALIZED)
                       ? BACKEND_OPEN
                       : BACKEND_CLOSED;

  UMA_HISTOGRAM_ENUMERATION("ServiceWorkerCache.InitBackendResult",
                            cache_create_error, CACHE_STORAGE_ERROR_LAST + 1);

  callback.Run();
}

void CacheStorageCache::PopulateRequestFromMetadata(
    const CacheMetadata& metadata,
    const GURL& request_url,
    ServiceWorkerFetchRequest* request) {
  *request =
      ServiceWorkerFetchRequest(request_url, metadata.request().method(),
                                ServiceWorkerHeaderMap(), Referrer(), false);

  for (int i = 0; i < metadata.request().headers_size(); ++i) {
    const CacheHeaderMap header = metadata.request().headers(i);
    DCHECK_EQ(std::string::npos, header.name().find('\0'));
    DCHECK_EQ(std::string::npos, header.value().find('\0'));
    request->headers.insert(std::make_pair(header.name(), header.value()));
  }
}

void CacheStorageCache::PopulateResponseMetadata(
    const CacheMetadata& metadata,
    ServiceWorkerResponse* response) {
  *response = ServiceWorkerResponse(
      GURL(metadata.response().url()), metadata.response().status_code(),
      metadata.response().status_text(),
      ProtoResponseTypeToWebResponseType(metadata.response().response_type()),
      ServiceWorkerHeaderMap(), "", 0, GURL(),
      blink::WebServiceWorkerResponseErrorUnknown,
      base::Time::FromInternalValue(metadata.response().response_time()),
      true /* is_in_cache_storage */, cache_name_,
      ServiceWorkerHeaderList(
          metadata.response().cors_exposed_header_names().begin(),
          metadata.response().cors_exposed_header_names().end()));

  for (int i = 0; i < metadata.response().headers_size(); ++i) {
    const CacheHeaderMap header = metadata.response().headers(i);
    DCHECK_EQ(std::string::npos, header.name().find('\0'));
    DCHECK_EQ(std::string::npos, header.value().find('\0'));
    response->headers.insert(std::make_pair(header.name(), header.value()));
  }
}

std::unique_ptr<storage::BlobDataHandle>
CacheStorageCache::PopulateResponseBody(disk_cache::ScopedEntryPtr entry,
                                        ServiceWorkerResponse* response) {
  DCHECK(blob_storage_context_);

  // Create a blob with the response body data.
  response->blob_size = entry->GetDataSize(INDEX_RESPONSE_BODY);
  response->blob_uuid = base::GenerateGUID();
  storage::BlobDataBuilder blob_data(response->blob_uuid);

  disk_cache::Entry* temp_entry = entry.get();
  blob_data.AppendDiskCacheEntryWithSideData(
      new CacheStorageCacheDataHandle(CreateCacheHandle(), std::move(entry)),
      temp_entry, INDEX_RESPONSE_BODY, INDEX_SIDE_DATA);
  return blob_storage_context_->AddFinishedBlob(&blob_data);
}

std::unique_ptr<CacheStorageCacheHandle>
CacheStorageCache::CreateCacheHandle() {
  return cache_storage_->CreateCacheHandle(this);
}

}  // namespace content
