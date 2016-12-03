// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/common/shell_delegate.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/metrics/chrome_user_metrics_recorder.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {
class DisplayConfigurationObserver;
}

namespace keyboard {
class KeyboardUI;
}

class ChromeLauncherControllerImpl;

class ChromeShellDelegate : public ash::ShellDelegate,
                            public content::NotificationObserver {
 public:
  ChromeShellDelegate();
  ~ChromeShellDelegate() override;

  // ash::ShellDelegate overrides;
  bool IsFirstRunAfterBoot() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsIncognitoAllowed() const override;
  bool IsRunningInForcedAppMode() const override;
  bool CanShowWindowForUser(ash::WmWindow* window) const override;
  bool IsForceMaximizeOnFirstRun() const override;
  void PreInit() override;
  void PreShutdown() override;
  void Exit() override;
  keyboard::KeyboardUI* CreateKeyboardUI() override;
  void OpenUrlFromArc(const GURL& url) override;
  app_list::AppListPresenter* GetAppListPresenter() override;
  ash::ShelfDelegate* CreateShelfDelegate(ash::ShelfModel* model) override;
  ash::SystemTrayDelegate* CreateSystemTrayDelegate() override;
  std::unique_ptr<ash::WallpaperDelegate> CreateWallpaperDelegate() override;
  ash::SessionStateDelegate* CreateSessionStateDelegate() override;
  ash::AccessibilityDelegate* CreateAccessibilityDelegate() override;
  ash::NewWindowDelegate* CreateNewWindowDelegate() override;
  ash::MediaDelegate* CreateMediaDelegate() override;
  std::unique_ptr<ash::PaletteDelegate> CreatePaletteDelegate() override;
  ui::MenuModel* CreateContextMenu(ash::WmShelf* wm_shelf,
                                   const ash::ShelfItem* item) override;
  ash::GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;
  void OpenKeyboardShortcutHelpPage() const override;
  gfx::Image GetDeprecatedAcceleratorImage() const override;
  void ToggleTouchpad() override;
  void ToggleTouchscreen() override;

  // content::NotificationObserver override:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  void PlatformInit();

  content::NotificationRegistrar registrar_;

  ChromeLauncherControllerImpl* shelf_delegate_;

  // Proxies events from chrome/browser to ash::UserMetricsRecorder.
  std::unique_ptr<ChromeUserMetricsRecorder> chrome_user_metrics_recorder_;

  std::unique_ptr<chromeos::DisplayConfigurationObserver>
      display_configuration_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeShellDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
