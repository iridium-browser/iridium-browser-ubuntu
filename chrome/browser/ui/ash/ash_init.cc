// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_init.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/ash_switches.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "base/command_line.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/ui/ash/ime_controller_chromeos.h"
#include "chrome/browser/ui/ash/volume_controller_chromeos.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "ui/base/x/x11_util.h"
#endif

namespace chrome {

void OpenAsh(gfx::AcceleratedWidget remote_window) {
#if defined(OS_CHROMEOS)
#if defined(USE_X11)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // Hides the cursor outside of the Aura root window. The cursor will be
    // drawn within the Aura root window, and it'll remain hidden after the
    // Aura window is closed.
    ui::HideHostCursor();
  }
#endif

  // Hide the mouse cursor completely at boot.
  if (!chromeos::LoginState::Get()->IsUserLoggedIn())
    ash::Shell::set_initially_hide_cursor(true);
#endif

  ash::ShellInitParams shell_init_params;
  // Shell takes ownership of ChromeShellDelegate.
  shell_init_params.delegate = new ChromeShellDelegate;
  shell_init_params.context_factory = content::GetContextFactory();
#if defined(OS_WIN)
  shell_init_params.remote_hwnd = remote_window;
#endif

  ash::Shell* shell = ash::Shell::CreateInstance(shell_init_params);
  shell->accelerator_controller()->SetScreenshotDelegate(
      scoped_ptr<ash::ScreenshotDelegate>(new ChromeScreenshotGrabber).Pass());
#if defined(OS_CHROMEOS)
  // TODO(flackr): Investigate exposing a blocking pool task runner to chromeos.
  chromeos::AccelerometerReader::GetInstance()->Initialize(
      content::BrowserThread::GetBlockingPool()
          ->GetSequencedTaskRunnerWithShutdownBehavior(
              content::BrowserThread::GetBlockingPool()->GetSequenceToken(),
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  shell->accelerator_controller()->SetImeControlDelegate(
      scoped_ptr<ash::ImeControlDelegate>(new ImeController).Pass());
  shell->high_contrast_controller()->SetEnabled(
      chromeos::AccessibilityManager::Get()->IsHighContrastEnabled());

  DCHECK(chromeos::MagnificationManager::Get());
  bool magnifier_enabled =
      chromeos::MagnificationManager::Get()->IsMagnifierEnabled();
  ui::MagnifierType magnifier_type =
      chromeos::MagnificationManager::Get()->GetMagnifierType();
  shell->magnification_controller()->
      SetEnabled(magnifier_enabled && magnifier_type == ui::MAGNIFIER_FULL);
  shell->partial_magnification_controller()->
      SetEnabled(magnifier_enabled && magnifier_type == ui::MAGNIFIER_PARTIAL);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableZeroBrowsersOpenForTests)) {
    chrome::IncrementKeepAliveCount();
  }
#endif
  ash::Shell::GetPrimaryRootWindow()->GetHost()->Show();
}

void CloseAsh() {
  if (ash::Shell::HasInstance())
    ash::Shell::DeleteInstance();
}

}  // namespace chrome
