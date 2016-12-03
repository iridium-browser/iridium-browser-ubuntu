// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shell_delegate_mus.h"

#include <utility>

#include "ash/common/gpu_support_stub.h"
#include "ash/common/media_delegate.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/system/tray/default_system_tray_delegate.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/mus/accessibility_delegate_mus.h"
#include "ash/mus/context_menu_mus.h"
#include "ash/mus/new_window_delegate_mus.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "components/user_manager/user_info_impl.h"
#include "ui/app_list/presenter/app_list_presenter.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace {

class SessionStateDelegateStub : public SessionStateDelegate {
 public:
  SessionStateDelegateStub()
      : screen_locked_(false), user_info_(new user_manager::UserInfoImpl()) {}

  ~SessionStateDelegateStub() override {}

  // SessionStateDelegate:
  int GetMaximumNumberOfLoggedInUsers() const override { return 3; }
  int NumberOfLoggedInUsers() const override {
    // ash_shell has 2 users.
    return 2;
  }
  bool IsActiveUserSessionStarted() const override { return true; }
  bool CanLockScreen() const override { return true; }
  bool IsScreenLocked() const override { return screen_locked_; }
  bool ShouldLockScreenBeforeSuspending() const override { return false; }
  void LockScreen() override {
    screen_locked_ = true;
    NOTIMPLEMENTED();
  }
  void UnlockScreen() override {
    NOTIMPLEMENTED();
    screen_locked_ = false;
  }
  bool IsUserSessionBlocked() const override { return false; }
  SessionState GetSessionState() const override { return SESSION_STATE_ACTIVE; }
  const user_manager::UserInfo* GetUserInfo(UserIndex index) const override {
    return user_info_.get();
  }
  bool ShouldShowAvatar(WmWindow* window) const override {
    NOTIMPLEMENTED();
    return !user_info_->GetImage().isNull();
  }
  gfx::ImageSkia GetAvatarImageForWindow(WmWindow* window) const override {
    NOTIMPLEMENTED();
    return gfx::ImageSkia();
  }
  void SwitchActiveUser(const AccountId& account_id) override {}
  void CycleActiveUser(CycleUser cycle_user) override {}
  bool IsMultiProfileAllowedByPrimaryUserPolicy() const override {
    return true;
  }
  void AddSessionStateObserver(ash::SessionStateObserver* observer) override {}
  void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) override {}

 private:
  bool screen_locked_;

  // A pseudo user info.
  std::unique_ptr<user_manager::UserInfo> user_info_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateStub);
};

class MediaDelegateStub : public MediaDelegate {
 public:
  MediaDelegateStub() {}
  ~MediaDelegateStub() override {}

  // MediaDelegate:
  void HandleMediaNextTrack() override { NOTIMPLEMENTED(); }
  void HandleMediaPlayPause() override { NOTIMPLEMENTED(); }
  void HandleMediaPrevTrack() override { NOTIMPLEMENTED(); }
  MediaCaptureState GetMediaCaptureState(UserIndex index) override {
    NOTIMPLEMENTED();
    return MEDIA_CAPTURE_NONE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaDelegateStub);
};

class ShelfDelegateStub : public ShelfDelegate {
 public:
  ShelfDelegateStub() {}
  ~ShelfDelegateStub() override {}

  // ShelfDelegate overrides:
  void OnShelfCreated(WmShelf* shelf) override {}
  void OnShelfDestroyed(WmShelf* shelf) override {}
  void OnShelfAlignmentChanged(WmShelf* shelf) override {}
  void OnShelfAutoHideBehaviorChanged(WmShelf* shelf) override {}
  void OnShelfAutoHideStateChanged(WmShelf* shelf) override {}
  void OnShelfVisibilityStateChanged(WmShelf* shelf) override {}
  ShelfID GetShelfIDForAppID(const std::string& app_id) override { return 0; }
  bool HasShelfIDToAppIDMapping(ShelfID id) const override { return false; }
  const std::string& GetAppIDForShelfID(ShelfID id) override {
    return base::EmptyString();
  }
  void PinAppWithID(const std::string& app_id) override {}
  bool IsAppPinned(const std::string& app_id) override { return false; }
  void UnpinAppWithID(const std::string& app_id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateStub);
};

}  // namespace

ShellDelegateMus::ShellDelegateMus(
    std::unique_ptr<app_list::AppListPresenter> app_list_presenter,
    shell::Connector* connector)
    : app_list_presenter_(std::move(app_list_presenter)),
      connector_(connector) {
  // |connector_| may be null in tests.
}

ShellDelegateMus::~ShellDelegateMus() {}

bool ShellDelegateMus::IsFirstRunAfterBoot() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::IsIncognitoAllowed() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::IsMultiProfilesEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::IsRunningInForcedAppMode() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::CanShowWindowForUser(WmWindow* window) const {
  NOTIMPLEMENTED();
  return true;
}

bool ShellDelegateMus::IsForceMaximizeOnFirstRun() const {
  NOTIMPLEMENTED();
  return false;
}

void ShellDelegateMus::PreInit() {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::PreShutdown() {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::Exit() {
  NOTIMPLEMENTED();
}

keyboard::KeyboardUI* ShellDelegateMus::CreateKeyboardUI() {
  NOTIMPLEMENTED();
  return nullptr;
}

void ShellDelegateMus::OpenUrlFromArc(const GURL& url) {
  NOTIMPLEMENTED();
}

app_list::AppListPresenter* ShellDelegateMus::GetAppListPresenter() {
  return app_list_presenter_.get();
}

ShelfDelegate* ShellDelegateMus::CreateShelfDelegate(ShelfModel* model) {
  // TODO(mash): Implement a real shelf delegate; maybe port ShelfDelegateMus?
  return new ShelfDelegateStub;
}

SystemTrayDelegate* ShellDelegateMus::CreateSystemTrayDelegate() {
  NOTIMPLEMENTED() << " Using the default SystemTrayDelegate implementation";
  return new DefaultSystemTrayDelegate;
}

std::unique_ptr<WallpaperDelegate> ShellDelegateMus::CreateWallpaperDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

SessionStateDelegate* ShellDelegateMus::CreateSessionStateDelegate() {
  NOTIMPLEMENTED() << " Using a stub SessionStateDeleagte implementation";
  return new SessionStateDelegateStub;
}

AccessibilityDelegate* ShellDelegateMus::CreateAccessibilityDelegate() {
  return new AccessibilityDelegateMus(connector_);
}

NewWindowDelegate* ShellDelegateMus::CreateNewWindowDelegate() {
  return new mus::NewWindowDelegateMus;
}

MediaDelegate* ShellDelegateMus::CreateMediaDelegate() {
  NOTIMPLEMENTED() << " Using a stub MediaDelegate implementation";
  return new MediaDelegateStub;
}

std::unique_ptr<PaletteDelegate> ShellDelegateMus::CreatePaletteDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

ui::MenuModel* ShellDelegateMus::CreateContextMenu(WmShelf* wm_shelf,
                                                   const ShelfItem* item) {
  return new ContextMenuMus(wm_shelf);
}

GPUSupport* ShellDelegateMus::CreateGPUSupport() {
  NOTIMPLEMENTED() << " Using a stub GPUSupport implementation";
  return new GPUSupportStub();
}

base::string16 ShellDelegateMus::GetProductName() const {
  NOTIMPLEMENTED();
  return base::string16();
}

gfx::Image ShellDelegateMus::GetDeprecatedAcceleratorImage() const {
  NOTIMPLEMENTED();
  return gfx::Image();
}

}  // namespace ash
