// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_DEBUG_DAEMON_CLIENT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/debug_daemon_client.h"

namespace chromeos {

// The DebugDaemonClient implementation used on Linux desktop,
// which does nothing.
class CHROMEOS_EXPORT FakeDebugDaemonClient : public DebugDaemonClient {
 public:
  FakeDebugDaemonClient();
  ~FakeDebugDaemonClient() override;

  void Init(dbus::Bus* bus) override;
  void DumpDebugLogs(bool is_compressed,
                     base::File file,
                     scoped_refptr<base::TaskRunner> task_runner,
                     const GetDebugLogsCallback& callback) override;
  void SetDebugMode(const std::string& subsystem,
                    const SetDebugModeCallback& callback) override;
  void StartSystemTracing() override;
  bool RequestStopSystemTracing(
      scoped_refptr<base::TaskRunner> task_runner,
      const StopSystemTracingCallback& callback) override;
  void GetRoutes(bool numeric,
                 bool ipv6,
                 const GetRoutesCallback& callback) override;
  void GetNetworkStatus(const GetNetworkStatusCallback& callback) override;
  void GetModemStatus(const GetModemStatusCallback& callback) override;
  void GetWiMaxStatus(const GetWiMaxStatusCallback& callback) override;
  void GetNetworkInterfaces(
      const GetNetworkInterfacesCallback& callback) override;
  void GetPerfData(uint32_t duration,
                   const GetPerfDataCallback& callback) override;
  void GetPerfOutput(uint32_t duration,
                     const GetPerfOutputCallback& callback) override;
  void GetScrubbedLogs(const GetLogsCallback& callback) override;
  void GetAllLogs(const GetLogsCallback& callback) override;
  void GetUserLogFiles(const GetLogsCallback& callback) override;
  void TestICMP(const std::string& ip_address,
                const TestICMPCallback& callback) override;
  void TestICMPWithOptions(const std::string& ip_address,
                           const std::map<std::string, std::string>& options,
                           const TestICMPCallback& callback) override;
  void UploadCrashes() override;
  void EnableDebuggingFeatures(
      const std::string& password,
      const EnableDebuggingCallback& callback) override;
  void QueryDebuggingFeatures(
      const QueryDevFeaturesCallback& callback) override;
  void RemoveRootfsVerification(
      const EnableDebuggingCallback& callback) override;
  void WaitForServiceToBeAvailable(
      const WaitForServiceToBeAvailableCallback& callback) override;

  // Sets debugging features mask for testing.
  virtual void SetDebuggingFeaturesStatus(int featues_mask);

  // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
  // pending callbacks if is_available is true.
  void SetServiceIsAvailable(bool is_available);

 private:
  int featues_mask_;

  bool service_is_available_;
  std::vector<WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FakeDebugDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_DEBUG_DAEMON_CLIENT_H_
