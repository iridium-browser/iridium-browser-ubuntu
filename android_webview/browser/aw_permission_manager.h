// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PERMISSION_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PERMISSION_MANAGER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/permission_manager.h"

namespace android_webview {

class LastRequestResultCache;

class AwPermissionManager : public content::PermissionManager {
 public:
  AwPermissionManager();
  ~AwPermissionManager() override;

  // PermissionManager implementation.
  void RequestPermission(
      content::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      int request_id,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(content::PermissionStatus)>& callback) override;
  void CancelPermissionRequest(content::PermissionType permission,
                               content::RenderFrameHost* render_frame_host,
                               int request_id,
                               const GURL& requesting_origin) override;
  void ResetPermission(content::PermissionType permission,
                       const GURL& requesting_origin,
                       const GURL& embedding_origin) override;
  content::PermissionStatus GetPermissionStatus(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  void RegisterPermissionUsage(content::PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) override;
  int SubscribePermissionStatusChange(
      content::PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const base::Callback<void(content::PermissionStatus)>& callback) override;
  void UnsubscribePermissionStatusChange(int subscription_id) override;

 private:
  scoped_ptr<LastRequestResultCache> result_cache_;

  DISALLOW_COPY_AND_ASSIGN(AwPermissionManager);
};

} // namespace android_webview

#endif // ANDROID_WEBVIEW_BROWSER_AW_PERMISSION_MANAGER_H_
