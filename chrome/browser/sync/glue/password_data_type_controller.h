// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__

#include <string>

#include "base/memory/ref_counted.h"
#include "components/sync_driver/non_ui_data_type_controller.h"
#include "components/sync_driver/sync_service_observer.h"

class Profile;
class ProfileSyncComponentsFactory;

namespace password_manager {
class PasswordStore;
}

namespace browser_sync {

// A class that manages the startup and shutdown of password sync.
class PasswordDataTypeController : public sync_driver::NonUIDataTypeController,
                                   public sync_driver::SyncServiceObserver {
 public:
  PasswordDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile);

  // NonFrontendDataTypeController implementation
  syncer::ModelType type() const override;
  syncer::ModelSafeGroup model_safe_group() const override;

 protected:
  ~PasswordDataTypeController() override;

  // NonUIDataTypeController interface.
  bool PostTaskOnBackendThread(const tracked_objects::Location& from_here,
                               const base::Closure& task) override;
  bool StartModels() override;
  void StopModels() override;

  // sync_driver::SyncServiceObserver:
  void OnStateChanged() override;

 private:
  Profile* const profile_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordDataTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_DATA_TYPE_CONTROLLER_H__
