// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_HISTORY_DELETE_DIRECTIVES_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_HISTORY_DELETE_DIRECTIVES_DATA_TYPE_CONTROLLER_H_

#include "components/sync_driver/local_device_info_provider.h"
#include "components/sync_driver/sync_service_observer.h"
#include "components/sync_driver/ui_data_type_controller.h"

class Profile;
class ProfileSyncService;

namespace browser_sync {

// A controller for delete directives, which cannot sync when full encryption
// is enabled.
class HistoryDeleteDirectivesDataTypeController
    : public sync_driver::UIDataTypeController,
      public sync_driver::SyncServiceObserver {
 public:
  HistoryDeleteDirectivesDataTypeController(
      sync_driver::SyncApiComponentFactory* factory,
      ProfileSyncService* sync_service);

  // UIDataTypeController override.
  bool ReadyForStart() const override;
  bool StartModels() override;
  void StopModels() override;

  // sync_driver::SyncServiceObserver implementation.
  void OnStateChanged() override;

 private:
  // Refcounted.
  ~HistoryDeleteDirectivesDataTypeController() override;

  // Triggers a SingleDataTypeUnrecoverable error and returns true if the
  // type is no longer ready, else does nothing and returns false.
  bool DisableTypeIfNecessary();

  ProfileSyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDeleteDirectivesDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_HISTORY_DELETE_DIRECTIVES_DATA_TYPE_CONTROLLER_H_
