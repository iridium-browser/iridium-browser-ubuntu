// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_util.h"

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/wm_shell.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/ash_init.h"
#include "services/shell/runner/common/client_util.h"
#include "ui/aura/window_event_dispatcher.h"

namespace chrome {

bool ShouldOpenAshOnStartup() {
  return !IsRunningInMash();
}

bool IsRunningInMash() {
  return shell::ShellIsRemote();
}

bool IsAcceleratorDeprecated(const ui::Accelerator& accelerator) {
  // When running in mash the browser doesn't handle ash accelerators.
  if (chrome::IsRunningInMash())
    return false;

  return ash::WmShell::Get()->accelerator_controller()->IsDeprecated(
      accelerator);
}

}  // namespace chrome
