// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_MOCK_WIFI_MANAGER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_MOCK_WIFI_MANAGER_H_

#include "base/observer_list.h"
#include "chrome/browser/local_discovery/wifi/wifi_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace local_discovery {

namespace wifi {

class MockWifiManager : public WifiManager {
 public:
  MockWifiManager();
  ~MockWifiManager() override;

  MOCK_METHOD0(Start, void());

  void GetSSIDList(const SSIDListCallback& callback) override;

  MOCK_METHOD0(GetSSIDListInternal, void());

  void CallSSIDListCallback(const std::vector<NetworkProperties>& networks);

  MOCK_METHOD0(RequestScan, void());

  void ConfigureAndConnectNetwork(
      const std::string& ssid,
      const WifiCredentials& credentials,
      const SuccessCallback& callback) override;

  MOCK_METHOD2(ConfigureAndConnectNetworkInternal,
               void(const std::string& ssid, const std::string& password));

  void CallConfigureAndConnectNetworkCallback(bool success);

  void ConnectToNetworkByID(const std::string& internal_id,
                            const SuccessCallback& callback) override;

  MOCK_METHOD1(ConnectToNetworkByIDInternal,
               void(const std::string& internal_id));

  void CallConnectToNetworkByIDCallback(bool success);

  void RequestNetworkCredentials(
      const std::string& internal_id,
      const CredentialsCallback& callback) override;

  MOCK_METHOD1(RequestNetworkCredentialsInternal,
               void(const std::string& internal_id));

  void CallRequestNetworkCredentialsCallback(bool success,
                                             const std::string& ssid,
                                             const std::string& password);

  void AddNetworkListObserver(NetworkListObserver* observer);

  void RemoveNetworkListObserver(NetworkListObserver* observer);

  void CallNetworkListObservers(const std::vector<NetworkProperties>& ssids);

 private:
  SSIDListCallback ssid_list_callback_;
  SuccessCallback configure_and_connect_network_callback_;
  SuccessCallback connect_by_id_callback_;
  CredentialsCallback credentials_callback_;
  base::ObserverList<NetworkListObserver> network_observers_;

  base::WeakPtrFactory<MockWifiManager> weak_factory_;
};

class MockWifiManagerFactory : public WifiManagerFactory {
 public:
  MockWifiManagerFactory();
  ~MockWifiManagerFactory() override;

  scoped_ptr<WifiManager> CreateWifiManager() override;

  MockWifiManager* GetLastCreatedWifiManager();

  MOCK_METHOD0(WifiManagerCreated, void());

 private:
  MockWifiManager* last_created_manager_;
};

}  // namespace wifi

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_MOCK_WIFI_MANAGER_H_
