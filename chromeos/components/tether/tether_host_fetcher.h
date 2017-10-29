// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/cryptauth/remote_device.h"

namespace cryptauth {
class CryptAuthDeviceManager;
class CryptAuthService;
class RemoteDeviceLoader;
}  // namespace cryptauth

namespace chromeos {

namespace tether {

// Fetches RemoteDevice objects corresponding to tether hosts which have been
// synced via CryptAuth.
class TetherHostFetcher {
 public:
  explicit TetherHostFetcher(cryptauth::CryptAuthService* cryptauth_service);
  virtual ~TetherHostFetcher();

  // Fetches all tether hosts.
  using TetherHostListCallback =
      base::Callback<void(const cryptauth::RemoteDeviceList&)>;
  virtual void FetchAllTetherHosts(const TetherHostListCallback& callback);

  // Fetches the tether host with the ID |device_id|.
  using TetherHostCallback =
      base::Callback<void(std::unique_ptr<cryptauth::RemoteDevice>)>;
  virtual void FetchTetherHost(const std::string& device_id,
                               const TetherHostCallback& callback);

 protected:
  struct TetherHostFetchRequest {
    explicit TetherHostFetchRequest(
        const TetherHostListCallback& list_callback);
    TetherHostFetchRequest(const std::string& device_id,
                           const TetherHostCallback& single_callback);
    TetherHostFetchRequest(const TetherHostFetchRequest& other);
    ~TetherHostFetchRequest();

    std::string device_id;

    // Only one of the two callbacks is actually used depending on which
    // constructor is used. If a device ID is provided, then the request is for
    // a single device, and |single_callback| is used; otherwise, the request is
    // for all devices, so |list_callback| is used.
    TetherHostListCallback list_callback;
    TetherHostCallback single_callback;
  };

  TetherHostFetcher(const std::string& user_id,
                    const std::string& user_private_key,
                    cryptauth::CryptAuthService* cryptauth_service,
                    cryptauth::CryptAuthDeviceManager* device_manager);

  void OnRemoteDevicesLoaded(const cryptauth::RemoteDeviceList& remote_devices);

  std::vector<TetherHostFetchRequest> requests_;

 private:
  void StartLoadingDevicesIfNeeded();

  cryptauth::CryptAuthService* cryptauth_service_;

  std::unique_ptr<cryptauth::RemoteDeviceLoader> remote_device_loader_;

  base::WeakPtrFactory<TetherHostFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetherHostFetcher);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_
