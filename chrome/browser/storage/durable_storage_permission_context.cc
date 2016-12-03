// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_context.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/origin_util.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;

DurableStoragePermissionContext::DurableStoragePermissionContext(
    Profile* profile)
    : PermissionContextBase(profile,
                            content::PermissionType::DURABLE_STORAGE,
                            CONTENT_SETTINGS_TYPE_DURABLE_STORAGE) {}

void DurableStoragePermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // TODO(dgrogan): Remove bookmarks check in favor of site engagement. In the
  // meantime maybe grant permission to A2HS origins as well.
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContextIfExists(profile());
  if (model) {
    std::vector<bookmarks::BookmarkModel::URLAndTitle> bookmarks;
    model->GetBookmarks(&bookmarks);
    if (IsOriginBookmarked(bookmarks, requesting_origin)) {
      NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                          true /* persist */, CONTENT_SETTING_ALLOW);
      return;
    }
  }

  NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                      false /* persist */, CONTENT_SETTING_DEFAULT);
}

void DurableStoragePermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin_ignored,
    ContentSetting content_setting) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin_ignored, embedding_origin_ignored.GetOrigin());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(requesting_origin, GURL(),
                                      CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
                                      std::string(), content_setting);
}

bool DurableStoragePermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

bool DurableStoragePermissionContext::IsOriginBookmarked(
    const std::vector<bookmarks::BookmarkModel::URLAndTitle>& bookmarks,
    const GURL& origin) {
  BookmarkModel::URLAndTitle looking_for;
  looking_for.url = origin;
  return std::binary_search(bookmarks.begin(), bookmarks.end(), looking_for,
                            [](const BookmarkModel::URLAndTitle& a,
                               const BookmarkModel::URLAndTitle& b) {
                              return a.url.GetOrigin() < b.url.GetOrigin();
                            });
}
