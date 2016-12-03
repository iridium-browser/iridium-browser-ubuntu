// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SYNC_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SYNC_DATA_TYPE_CONTROLLER_H_

#include "base/macros.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/ui_data_type_controller.h"

namespace sync_driver {
class SyncClient;
}

class Profile;

// A UIDataTypeController for supervised user sync datatypes, which enables or
// disables these types based on the profile's IsSupervised state.
class SupervisedUserSyncDataTypeController
    : public sync_driver::UIDataTypeController {
 public:
  SupervisedUserSyncDataTypeController(syncer::ModelType type,
                                       const base::Closure& error_callback,
                                       sync_driver::SyncClient* sync_client,
                                       Profile* profile);

  bool ReadyForStart() const override;

 private:
  // DataTypeController is RefCounted.
  ~SupervisedUserSyncDataTypeController() override;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserSyncDataTypeController);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SYNC_DATA_TYPE_CONTROLLER_H_
