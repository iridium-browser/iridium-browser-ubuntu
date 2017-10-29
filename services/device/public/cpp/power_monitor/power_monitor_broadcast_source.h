// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_

#include "base/macros.h"
#include "base/power_monitor/power_monitor_source.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/interfaces/power_monitor.mojom.h"

namespace service_manager {
class Connector;
}

namespace device {

// Receives state changes from Power Monitor through mojo, and relays them to
// the PowerMonitor of the current process.
class PowerMonitorBroadcastSource : public base::PowerMonitorSource,
                                    public device::mojom::PowerMonitorClient {
 public:
  explicit PowerMonitorBroadcastSource(service_manager::Connector* connector);
  ~PowerMonitorBroadcastSource() override;

  void PowerStateChange(bool on_battery_power) override;
  void Suspend() override;
  void Resume() override;

 private:
  bool IsOnBatteryPowerImpl() override;
  bool last_reported_battery_power_state_;
  mojo::Binding<device::mojom::PowerMonitorClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorBroadcastSource);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_
