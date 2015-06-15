// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_permission_manager.h"

#include <string>

#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::PermissionStatus;
using content::PermissionType;

namespace android_webview {

class LastRequestResultCache {
 public:
  LastRequestResultCache() : weak_factory_(this) {}

  void SetResult(PermissionType permission,
                 const GURL& requesting_origin,
                 const GURL& embedding_origin,
                 PermissionStatus status) {
    DCHECK(status == content::PERMISSION_STATUS_GRANTED ||
           status == content::PERMISSION_STATUS_DENIED);

    // TODO(ddorwin): We should be denying empty origins at a higher level.
    if (requesting_origin.is_empty() || embedding_origin.is_empty()) {
      DLOG(WARNING) << "Not caching result because of empty origin.";
      return;
    }

    if (!requesting_origin.is_valid()) {
      NOTREACHED() << requesting_origin.possibly_invalid_spec();
      return;
    }
    if (!embedding_origin.is_valid()) {
      NOTREACHED() << embedding_origin.possibly_invalid_spec();
      return;
    }

    if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
      // Other permissions are not cached.
      return;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    if (key.empty()) {
      NOTREACHED();
      // Never store an empty key because it could inadvertently be used for
      // another combination.
      return;
    }
    pmi_result_cache_[key] = status;
  }

  PermissionStatus GetResult(PermissionType permission,
                             const GURL& requesting_origin,
                             const GURL& embedding_origin) const {
    // TODO(ddorwin): We should be denying empty origins at a higher level.
    if (requesting_origin.is_empty() || embedding_origin.is_empty()) {
      return content::PERMISSION_STATUS_ASK;
    }

    DCHECK(requesting_origin.is_valid())
        << requesting_origin.possibly_invalid_spec();
    DCHECK(embedding_origin.is_valid())
        << embedding_origin.possibly_invalid_spec();

    if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
      NOTREACHED() << "Results are only cached for PROTECTED_MEDIA_IDENTIFIER";
      return content::PERMISSION_STATUS_ASK;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    StatusMap::const_iterator it = pmi_result_cache_.find(key);
    if (it == pmi_result_cache_.end()) {
      DLOG(WARNING) << "GetResult() called for uncached origins: " << key;
      return content::PERMISSION_STATUS_ASK;
    }

    DCHECK(!key.empty());
    return it->second;
  }

  void ClearResult(PermissionType permission,
                   const GURL& requesting_origin,
                   const GURL& embedding_origin) {
    // TODO(ddorwin): We should be denying empty origins at a higher level.
    if (requesting_origin.is_empty() || embedding_origin.is_empty()) {
      return;
    }

    DCHECK(requesting_origin.is_valid())
        << requesting_origin.possibly_invalid_spec();
    DCHECK(embedding_origin.is_valid())
        << embedding_origin.possibly_invalid_spec();


    if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
      // Other permissions are not cached, so nothing to clear.
      return;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    pmi_result_cache_.erase(key);
  }

  base::WeakPtr<LastRequestResultCache> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Returns a concatenation of the origins to be used as the index.
  // Returns the empty string if either origin is invalid or empty.
  static std::string GetCacheKey(const GURL& requesting_origin,
                                 const GURL& embedding_origin) {
    const std::string& requesting = requesting_origin.spec();
    const std::string& embedding = embedding_origin.spec();
    if (requesting.empty() || embedding.empty())
      return std::string();
    return requesting + "," + embedding;
  }

  using StatusMap = base::hash_map<std::string, PermissionStatus>;
  StatusMap pmi_result_cache_;

  base::WeakPtrFactory<LastRequestResultCache> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LastRequestResultCache);
};

namespace {

void CallbackPermisisonStatusWrapper(
    const base::WeakPtr<LastRequestResultCache>& result_cache,
    const base::Callback<void(content::PermissionStatus)>& callback,
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool allowed) {
  PermissionStatus status = allowed ? content::PERMISSION_STATUS_GRANTED
                                    : content::PERMISSION_STATUS_DENIED;
  if (result_cache.get()) {
    result_cache->SetResult(permission, requesting_origin, embedding_origin,
                            status);
  }

  callback.Run(status);
}

}  // anonymous namespace

AwPermissionManager::AwPermissionManager()
    : content::PermissionManager(), result_cache_(new LastRequestResultCache) {
}

AwPermissionManager::~AwPermissionManager() {
}

void AwPermissionManager::RequestPermission(
    content::PermissionType permission,
    content::WebContents* web_contents,
    int request_id,
    const GURL& origin,
    bool user_gesture,
    const base::Callback<void(content::PermissionStatus)>& callback) {
  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                 render_view_id);
  if (!delegate) {
    DVLOG(0) << "Dropping permission request for "
             << static_cast<int>(permission);
    callback.Run(content::PERMISSION_STATUS_DENIED);
    return;
  }

  const GURL& embedding_origin =
      web_contents->GetLastCommittedURL().GetOrigin();

  switch (permission) {
    case content::PermissionType::GEOLOCATION:
      delegate->RequestGeolocationPermission(
          origin, base::Bind(&CallbackPermisisonStatusWrapper,
                             result_cache_->GetWeakPtr(), callback, permission,
                             origin, embedding_origin));
      break;
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      delegate->RequestProtectedMediaIdentifierPermission(
          origin, base::Bind(&CallbackPermisisonStatusWrapper,
                             result_cache_->GetWeakPtr(), callback, permission,
                             origin, embedding_origin));
      break;
    case content::PermissionType::MIDI_SYSEX:
    case content::PermissionType::NOTIFICATIONS:
    case content::PermissionType::PUSH_MESSAGING:
      NOTIMPLEMENTED() << "RequestPermission is not implemented for "
                       << static_cast<int>(permission);
      callback.Run(content::PERMISSION_STATUS_DENIED);
      break;
    case content::PermissionType::NUM:
      NOTREACHED() << "PermissionType::NUM was not expected here.";
      callback.Run(content::PERMISSION_STATUS_DENIED);
      break;
  }
}

void AwPermissionManager::CancelPermissionRequest(
    content::PermissionType permission,
    content::WebContents* web_contents,
    int request_id,
    const GURL& origin) {
  // The caller is canceling (presumably) the most recent request. Assuming the
  // request did not complete, the user did not respond to the requset.
  // Thus, assume we do not know the result.
  const GURL& embedding_origin =
      web_contents->GetLastCommittedURL().GetOrigin();
  result_cache_->ClearResult(permission, origin, embedding_origin);

  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();
  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                 render_view_id);
  if (!delegate)
    return;

  switch (permission) {
    case content::PermissionType::GEOLOCATION:
      delegate->CancelGeolocationPermissionRequests(origin);
      break;
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      delegate->CancelProtectedMediaIdentifierPermissionRequests(origin);
      break;
    case content::PermissionType::MIDI_SYSEX:
    case content::PermissionType::NOTIFICATIONS:
    case content::PermissionType::PUSH_MESSAGING:
      NOTIMPLEMENTED() << "CancelPermission not implemented for "
                       << static_cast<int>(permission);
      break;
    case content::PermissionType::NUM:
      NOTREACHED() << "PermissionType::NUM was not expected here.";
      break;
  }
}

void AwPermissionManager::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  result_cache_->ClearResult(permission, requesting_origin, embedding_origin);
}

content::PermissionStatus AwPermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // Method is called outside the Permissions API only for this permission.
  if (permission == PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
    return result_cache_->GetResult(permission, requesting_origin,
                                    embedding_origin);
  }

  return content::PERMISSION_STATUS_DENIED;
}

void AwPermissionManager::RegisterPermissionUsage(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

int AwPermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(content::PermissionStatus)>& callback) {
  return -1;
}

void AwPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
}

}  // namespace android_webview
