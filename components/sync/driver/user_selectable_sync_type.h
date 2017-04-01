// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_USER_SELECTABLE_SYNC_TYPE_H_
#define COMPONENTS_SYNC_DRIVER_USER_SELECTABLE_SYNC_TYPE_H_

namespace syncer {
namespace user_selectable_type {

// An enumeration of the sync datatypes that are explicitly exposed to the user
// via checkboxes in the "Advanced Sync Preferences" dialog. Used solely for the
// purposes of UMA histogram logging of the datatypes explicitly selected by
// users when sync is configured on a machine. This is a subset of the sync
// types listed in sync/base/model_type.h.
//
// Note: New sync datatypes must be added to the end of this list. Adding them
// anywhere else will result in incorrect histogram logging.

// THIS ENUM IS MEANT SOLELY FOR THE PURPOSE OF HISTOGRAM LOGGING. IF YOU ARE
// LOOKING TO MODIFY SYNC FUNCTIONALITY AND NEED A LIST OF SYNC TYPES, USE
// syncer::ModelType.

enum UserSelectableSyncType {
  BOOKMARKS = 0,
  PREFERENCES = 1,
  PASSWORDS = 2,
  AUTOFILL = 3,
  THEMES = 4,
  TYPED_URLS = 5,
  EXTENSIONS = 6,
  PROXY_TABS = 7,
  APPS = 8,
  // TODO(petewil): There was talk of removing this from user selectable sync
  // types.  Should we?
  SYNCED_NOTIFICATIONS = 9,
  WIFI_CREDENTIAL = 10,

  READING_LIST = 11,

  // The datatypes below are implicitly synced, and are not exposed via user
  // selectable checkboxes.
  // TODO(lgcheng) ARC_PACKAGE would be selectable.

  // AUTOFILL_PROFILE,
  // NIGORI,
  // DICTIONARY
  // SEARCH_ENGINES,
  // APP_LIST,
  // APP_SETTINGS,
  // EXTENSION_SETTINGS,
  // APP_NOTIFICATIONS,
  // DEVICE_INFO,
  // EXPERIMENTS,
  // PRIORITY_PREFERENCES,
  // SESSIONS,
  // HISTORY_DELETE_DIRECTIVES,
  // FAVICON_IMAGES,
  // FAVICON_TRACKING,
  // ARTICLE,
  // SYNCED_NOTIFICATION_APP_INFO,
  // ARC_PACKAGE,
  // PRINTERS

  // Number of sync datatypes exposed to the user via checboxes in the UI.
  SELECTABLE_DATATYPE_COUNT = 12,
};

}  // namespace user_selectable_type
}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_USER_SELECTABLE_SYNC_TYPE_H_
