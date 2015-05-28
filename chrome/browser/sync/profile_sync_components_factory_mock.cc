// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_components_factory_mock.h"

#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/local_device_info_provider_mock.h"
#include "components/sync_driver/model_associator.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/attachments/attachment_store.h"
#include "sync/internal_api/public/attachments/attachment_service_impl.h"

using sync_driver::AssociatorInterface;
using sync_driver::ChangeProcessor;
using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;

ProfileSyncComponentsFactoryMock::ProfileSyncComponentsFactoryMock()
    : local_device_(new sync_driver::LocalDeviceInfoProviderMock()) {
}

ProfileSyncComponentsFactoryMock::ProfileSyncComponentsFactoryMock(
    AssociatorInterface* model_associator, ChangeProcessor* change_processor)
    : model_associator_(model_associator),
      change_processor_(change_processor),
      local_device_(new sync_driver::LocalDeviceInfoProviderMock()) {
  ON_CALL(*this, CreateBookmarkSyncComponents(_, _)).
      WillByDefault(
          InvokeWithoutArgs(
              this,
              &ProfileSyncComponentsFactoryMock::MakeSyncComponents));
}

ProfileSyncComponentsFactoryMock::~ProfileSyncComponentsFactoryMock() {}

scoped_ptr<syncer::AttachmentService>
ProfileSyncComponentsFactoryMock::CreateAttachmentService(
    scoped_ptr<syncer::AttachmentStoreForSync> attachment_store,
    const syncer::UserShare& user_share,
    const std::string& store_birthday,
    syncer::ModelType model_type,
    syncer::AttachmentService::Delegate* delegate) {
  return syncer::AttachmentServiceImpl::CreateForTest();
}

ProfileSyncComponentsFactory::SyncComponents
ProfileSyncComponentsFactoryMock::MakeSyncComponents() {
  return SyncComponents(model_associator_.release(),
                        change_processor_.release());
}

scoped_ptr<sync_driver::LocalDeviceInfoProvider>
ProfileSyncComponentsFactoryMock::CreateLocalDeviceInfoProvider() {
  return local_device_.Pass();
}

void ProfileSyncComponentsFactoryMock::SetLocalDeviceInfoProvider(
    scoped_ptr<sync_driver::LocalDeviceInfoProvider> local_device) {
  local_device_ = local_device.Pass();
}
