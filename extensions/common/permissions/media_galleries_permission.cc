// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/media_galleries_permission.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/permissions/permissions_info.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// copyTo permission requires delete permission as a prerequisite.
// delete permission requires read permission as a prerequisite.
bool IsValidPermissionSet(bool has_read, bool has_copy_to, bool has_delete,
                          std::string* error) {
  if (has_copy_to) {
    if (has_read && has_delete)
      return true;
    if (error)
      *error = "copyTo permission requires read and delete permissions";
    return false;
  }
  if (has_delete) {
    if (has_read)
      return true;
    if (error)
      *error = "delete permission requires read permission";
    return false;
  }
  return true;
}

// Adds the permissions from the |data_set| to |ids|.
void AddPermissionsToLists(
    const std::set<MediaGalleriesPermissionData>& data_set,
    PermissionIDSet* ids) {
  // TODO(sashab): Once GetMessages() is deprecated, move this logic back into
  // GetPermissions().
  bool has_all_auto_detected = false;
  bool has_read = false;
  bool has_copy_to = false;
  bool has_delete = false;

  for (std::set<MediaGalleriesPermissionData>::const_iterator it =
           data_set.begin();
       it != data_set.end(); ++it) {
    if (it->permission() ==
        MediaGalleriesPermission::kAllAutoDetectedPermission)
      has_all_auto_detected = true;
    else if (it->permission() == MediaGalleriesPermission::kReadPermission)
      has_read = true;
    else if (it->permission() == MediaGalleriesPermission::kCopyToPermission)
      has_copy_to = true;
    else if (it->permission() == MediaGalleriesPermission::kDeletePermission)
      has_delete = true;
  }

  if (!IsValidPermissionSet(has_read, has_copy_to, has_delete, NULL)) {
    NOTREACHED();
    return;
  }

  // If |has_all_auto_detected| is false, then Chrome will prompt the user at
  // runtime when the extension call the getMediaGalleries API.
  if (!has_all_auto_detected)
    return;
  // No access permission case.
  if (!has_read)
    return;

  // Separate PermissionMessage IDs for read, copyTo, and delete. Otherwise an
  // extension can silently gain new access capabilities.
  ids->insert(APIPermission::kMediaGalleriesAllGalleriesRead);

  // For copyTo and delete, the proper combined permission message will be
  // derived in ChromePermissionMessageProvider::GetWarningMessages(), such
  // that the user get 1 entry for all media galleries access permissions,
  // rather than several separate entries.
  if (has_copy_to)
    ids->insert(APIPermission::kMediaGalleriesAllGalleriesCopyTo);
  if (has_delete)
    ids->insert(APIPermission::kMediaGalleriesAllGalleriesDelete);
}

}  // namespace

const char MediaGalleriesPermission::kAllAutoDetectedPermission[] =
    "allAutoDetected";
const char MediaGalleriesPermission::kScanPermission[] = "scan";
const char MediaGalleriesPermission::kReadPermission[] = "read";
const char MediaGalleriesPermission::kCopyToPermission[] = "copyTo";
const char MediaGalleriesPermission::kDeletePermission[] = "delete";

MediaGalleriesPermission::MediaGalleriesPermission(
    const APIPermissionInfo* info)
  : SetDisjunctionPermission<MediaGalleriesPermissionData,
                             MediaGalleriesPermission>(info) {
}

MediaGalleriesPermission::~MediaGalleriesPermission() {
}

bool MediaGalleriesPermission::FromValue(
    const base::Value* value,
    std::string* error,
    std::vector<std::string>* unhandled_permissions) {
  size_t unhandled_permissions_count = 0;
  if (unhandled_permissions)
    unhandled_permissions_count = unhandled_permissions->size();
  bool parsed_ok =
      SetDisjunctionPermission<MediaGalleriesPermissionData,
                               MediaGalleriesPermission>::FromValue(
                                   value, error, unhandled_permissions);
  if (unhandled_permissions) {
    for (size_t i = unhandled_permissions_count;
         i < unhandled_permissions->size();
         i++) {
      (*unhandled_permissions)[i] =
          "{\"mediaGalleries\": [" + (*unhandled_permissions)[i] + "]}";
    }
  }
  if (!parsed_ok)
    return false;

  bool has_read = false;
  bool has_copy_to = false;
  bool has_delete = false;
  for (std::set<MediaGalleriesPermissionData>::const_iterator it =
      data_set_.begin(); it != data_set_.end(); ++it) {
    if (it->permission() == kAllAutoDetectedPermission ||
        it->permission() == kScanPermission) {
      continue;
    }
    if (it->permission() == kReadPermission) {
      has_read = true;
      continue;
    }
    if (it->permission() == kCopyToPermission) {
      has_copy_to = true;
      continue;
    }
    if (it->permission() == kDeletePermission) {
      has_delete = true;
      continue;
    }

    // No other permissions, so reaching this means
    // MediaGalleriesPermissionData is probably out of sync in some way.
    // Fail so developers notice this.
    NOTREACHED();
    return false;
  }

  return IsValidPermissionSet(has_read, has_copy_to, has_delete, error);
}

PermissionIDSet MediaGalleriesPermission::GetPermissions() const {
  PermissionIDSet result;
  AddPermissionsToLists(data_set_, &result);
  return result;
}

}  // namespace extensions
