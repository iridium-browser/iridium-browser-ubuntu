// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
#define CHROME_BROWSER_SYNC_GLUE_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/sync_driver/device_info.h"
#include "components/sync_driver/local_device_info_provider.h"

namespace browser_sync {

class LocalDeviceInfoProviderImpl
    : public sync_driver::LocalDeviceInfoProvider {
 public:
  LocalDeviceInfoProviderImpl();
  ~LocalDeviceInfoProviderImpl() override;

  // LocalDeviceInfoProvider implementation.
  const sync_driver::DeviceInfo* GetLocalDeviceInfo() const override;
  std::string GetSyncUserAgent() const override;
  std::string GetLocalSyncCacheGUID() const override;
  void Initialize(const std::string& cache_guid,
                  const std::string& signin_scoped_device_id) override;
  scoped_ptr<Subscription> RegisterOnInitializedCallback(
      const base::Closure& callback) override;

 private:

  void InitializeContinuation(const std::string& guid,
                              const std::string& signin_scoped_device_id,
                              const std::string& session_name);

  std::string cache_guid_;
  scoped_ptr<sync_driver::DeviceInfo> local_device_info_;
  base::CallbackList<void(void)> callback_list_;
  base::WeakPtrFactory<LocalDeviceInfoProviderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalDeviceInfoProviderImpl);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_LOCAL_DEVICE_INFO_PROVIDER_IMPL_H_
