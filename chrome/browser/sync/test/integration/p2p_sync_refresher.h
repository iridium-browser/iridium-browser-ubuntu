// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_P2P_SYNC_REFRESHER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_P2P_SYNC_REFRESHER_H_

#include "base/macros.h"
#include "components/sync/driver/sync_service_observer.h"

class Profile;
class ProfileSyncService;

// This class observes ProfileSyncService events and emits refresh notifications
// to other test profiles for any committed changes it observes.
//
// It register and unregisters in its constructor and destructor.  This is
// intended to make it easy to manage with a scoped_ptr.
class P2PSyncRefresher : public sync_driver::SyncServiceObserver {
 public:
  P2PSyncRefresher(Profile* profile, ProfileSyncService* sync_service);
  ~P2PSyncRefresher() override;

  // Implementation of sync_driver::SyncServiceObserver
  void OnStateChanged() override;
  void OnSyncCycleCompleted() override;

 private:
  Profile* const profile_;            // weak
  ProfileSyncService* sync_service_;  // weak

  DISALLOW_COPY_AND_ASSIGN(P2PSyncRefresher);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_P2P_SYNC_REFRESHER_H_
