// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"

#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"

// static
AppListServiceAsh* AppListServiceAsh::GetInstance() {
  return Singleton<AppListServiceAsh,
                   LeakySingletonTraits<AppListServiceAsh> >::get();
}

AppListServiceAsh::AppListServiceAsh()
    : controller_delegate_(new AppListControllerDelegateAsh()) {
}

AppListServiceAsh::~AppListServiceAsh() {
}

void AppListServiceAsh::ShowAndSwitchToState(
    app_list::AppListModel::State state) {
  bool app_list_was_open = true;
  app_list::AppListView* app_list_view =
      ash::Shell::GetInstance()->GetAppListView();
  if (!app_list_view) {
    // TODO(calamity): This may cause the app list to show briefly before the
    // state change. If this becomes an issue, add the ability to ash::Shell to
    // load the app list without showing it.
    ash::Shell::GetInstance()->ShowAppList(NULL);
    app_list_was_open = false;
    app_list_view = ash::Shell::GetInstance()->GetAppListView();
    DCHECK(app_list_view);
  }

  if (state == app_list::AppListModel::INVALID_STATE)
    return;

  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();
  contents_view->SetActiveState(state, app_list_was_open /* animate */);
}

void AppListServiceAsh::Init(Profile* initial_profile) {
  // Ensure the StartPageService is created here. This early initialization is
  // necessary to allow the WebContents to load before the app list is shown.
  app_list::StartPageService* service =
      app_list::StartPageService::Get(initial_profile);
  if (service)
    service->Init();
}

void AppListServiceAsh::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
}

base::FilePath AppListServiceAsh::GetProfilePath(
    const base::FilePath& user_data_dir) {
  return ChromeLauncherController::instance()->profile()->GetPath();
}

void AppListServiceAsh::ShowForProfile(Profile* /*default_profile*/) {
  // This may not work correctly if the profile passed in is different from the
  // one the ash Shell is currently using.
  // TODO(ananta): Handle profile changes correctly when !defined(OS_CHROMEOS).
  ash::Shell::GetInstance()->ShowAppList(NULL);
}

void AppListServiceAsh::ShowForAppInstall(Profile* profile,
                                          const std::string& extension_id,
                                          bool start_discovery_tracking) {
  if (app_list::switches::IsExperimentalAppListEnabled())
    ShowAndSwitchToState(app_list::AppListModel::STATE_APPS);

  AppListServiceImpl::ShowForAppInstall(profile, extension_id,
                                        start_discovery_tracking);
}

void AppListServiceAsh::ShowForCustomLauncherPage(Profile* /*profile*/) {
  ShowAndSwitchToState(app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
}

void AppListServiceAsh::HideCustomLauncherPage() {
  app_list::AppListView* app_list_view =
      ash::Shell::GetInstance()->GetAppListView();
  if (!app_list_view)
    return;

  app_list::ContentsView* contents_view =
      app_list_view->app_list_main_view()->contents_view();
  if (contents_view->IsStateActive(
          app_list::AppListModel::STATE_CUSTOM_LAUNCHER_PAGE)) {
    contents_view->SetActiveState(app_list::AppListModel::STATE_START, true);
  }
}

bool AppListServiceAsh::IsAppListVisible() const {
  return ash::Shell::GetInstance()->GetAppListTargetVisibility();
}

void AppListServiceAsh::DismissAppList() {
  ash::Shell::GetInstance()->DismissAppList();
}

void AppListServiceAsh::EnableAppList(Profile* initial_profile,
                                      AppListEnableSource enable_source) {}

gfx::NativeWindow AppListServiceAsh::GetAppListWindow() {
  if (ash::Shell::HasInstance())
    return ash::Shell::GetInstance()->GetAppListWindow();
  return NULL;
}

Profile* AppListServiceAsh::GetCurrentAppListProfile() {
  return ChromeLauncherController::instance()->profile();
}

AppListControllerDelegate* AppListServiceAsh::GetControllerDelegate() {
  return controller_delegate_.get();
}

void AppListServiceAsh::CreateForProfile(Profile* default_profile) {
}

void AppListServiceAsh::DestroyAppList() {
  // On Ash, the app list is torn down whenever it is dismissed, so just ensure
  // that it is dismissed.
  DismissAppList();
}

// Windows and Linux Ash additionally supports a native UI. See
// app_list_service_{win,linux}.cc.
#if defined(OS_CHROMEOS)

// static
AppListService* AppListService::Get(chrome::HostDesktopType desktop_type) {
  return AppListServiceAsh::GetInstance();
}

// static
void AppListService::InitAll(Profile* initial_profile,
                             const base::FilePath& profile_path) {
  AppListServiceAsh::GetInstance()->Init(initial_profile);
}

#endif  // !defined(OS_WIN)
