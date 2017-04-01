// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_SYSTEM_TRAY_CONTROLLER_H_
#define ASH_COMMON_SYSTEM_TRAY_SYSTEM_TRAY_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/system_tray.mojom.h"
#include "base/compiler_specific.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

// Both implements mojom::SystemTray and wraps the mojom::SystemTrayClient
// interface. Implements both because it caches state pushed down from the
// browser process via SystemTray so it can be synchronously queried inside ash.
//
// Conceptually similar to historical ash-to-chrome interfaces like
// SystemTrayDelegate. Lives on the main thread.
//
// TODO: Consider renaming this to SystemTrayClient or renaming the current
// SystemTray to SystemTrayView and making this class SystemTray.
class ASH_EXPORT SystemTrayController
    : NON_EXPORTED_BASE(public mojom::SystemTray) {
 public:
  SystemTrayController();
  ~SystemTrayController() override;

  base::HourClockType hour_clock_type() const { return hour_clock_type_; }

  // Wrappers around the mojom::SystemTrayClient interface.
  void ShowSettings();
  void ShowDateSettings();
  void ShowSetTimeDialog();
  void ShowDisplaySettings();
  void ShowPowerSettings();
  void ShowChromeSlow();
  void ShowIMESettings();
  void ShowHelp();
  void ShowAccessibilityHelp();
  void ShowAccessibilitySettings();
  void ShowPaletteHelp();
  void ShowPaletteSettings();
  void ShowPublicAccountInfo();
  void ShowNetworkConfigure(const std::string& network_id);
  void ShowNetworkCreate(const std::string& type);
  void ShowThirdPartyVpnCreate(const std::string& extension_id);
  void ShowNetworkSettings(const std::string& network_id);
  void ShowProxySettings();
  void SignOut();
  void RequestRestartForUpdate();

  // Binds the mojom::SystemTray interface to this object.
  void BindRequest(mojom::SystemTrayRequest request);

  // mojom::SystemTray overrides. Public for testing.
  void SetClient(mojom::SystemTrayClientPtr client) override;
  void SetPrimaryTrayEnabled(bool enabled) override;
  void SetPrimaryTrayVisible(bool visible) override;
  void SetUse24HourClock(bool use_24_hour) override;
  void ShowUpdateIcon(mojom::UpdateSeverity severity,
                      bool factory_reset_required) override;

 private:
  // Client interface in chrome browser. Only bound on Chrome OS.
  mojom::SystemTrayClientPtr system_tray_client_;

  // Bindings for the SystemTray interface.
  mojo::BindingSet<mojom::SystemTray> bindings_;

  // The type of clock hour display: 12 or 24 hour.
  base::HourClockType hour_clock_type_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayController);
};

}  // namspace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_SYSTEM_TRAY_CONTROLLER_H_
