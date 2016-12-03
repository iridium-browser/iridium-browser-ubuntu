// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_driver_switches.h"

namespace switches {

// Allows overriding the deferred init fallback timeout.
const char kSyncDeferredStartupTimeoutSeconds[] =
    "sync-deferred-startup-timeout-seconds";

// Enables deferring sync backend initialization until user initiated changes
// occur.
const char kSyncDisableDeferredStartup[] = "sync-disable-deferred-startup";

// Enables feature to avoid unnecessary GetUpdate requests.
const char kSyncEnableGetUpdateAvoidance[] = "sync-enable-get-update-avoidance";

// Enables USS implementation of DeviceInfo datatype. This flag controls whether
// SyncableService based or ModelTypeService based implementation is used for
// DeviceInfo type.
const char kSyncEnableUSSDeviceInfo[] = "sync-enable-uss-device-info";

// Overrides the default server used for profile sync.
const char kSyncServiceURL[] = "sync-url";

// This flag causes sync to retry very quickly (see polling_constants.h) the
// when it encounters an error, as the first step towards exponential backoff.
const char kSyncShortInitialRetryOverride[] =
    "sync-short-initial-retry-override";

// Enables clearing of sync data when a user enables passphrase encryption.
const base::Feature kSyncClearDataOnPassphraseEncryption{
    "ClearSyncDataOnPassphraseEncryption", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
