// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "ui/gfx/native_widget_types.h"

namespace sync_driver {
class SyncService;
}

namespace password_manager_util {

enum OsPasswordStatus {
  PASSWORD_STATUS_UNKNOWN = 0,
  PASSWORD_STATUS_UNSUPPORTED,
  PASSWORD_STATUS_BLANK,
  PASSWORD_STATUS_NONBLANK,
  PASSWORD_STATUS_WIN_DOMAIN,
  // NOTE: Add new status types only immediately above this line. Also,
  // make sure the enum list in tools/histogram/histograms.xml is
  // updated with any change in here.
  MAX_PASSWORD_STATUS
};

// Attempts to (re-)authenticate the user of the OS account. Returns true if
// the user was successfully authenticated, or if authentication was not
// possible. On platforms where reauthentication is not possible or does not
// make sense, the default implementation always returns true.
bool AuthenticateUser(gfx::NativeWindow window);

// Query the system to determine whether the current logged on user has a
// password set on their OS account.  It should be called on UI thread. |reply|
// is invoked on UI thread with result.
void GetOsPasswordStatus(const base::Callback<void(OsPasswordStatus)>& reply);

// Reports whether and how passwords are currently synced. In particular, for a
// null |sync_service| returns NOT_SYNCING_PASSWORDS.
password_manager::PasswordSyncState GetPasswordSyncState(
    const sync_driver::SyncService* sync_service);

}  // namespace password_manager_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
