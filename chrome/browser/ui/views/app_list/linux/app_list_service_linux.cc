// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/linux/app_list_service_linux.h"

#include "base/memory/singleton.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate_views.h"
#include "chrome/browser/ui/app_list/app_list_shower_views.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/views/app_list/linux/app_list_linux.h"
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#endif

namespace {

void CreateShortcuts() {
  std::string app_list_title =
      l10n_util::GetStringUTF8(IDS_APP_LIST_SHORTCUT_NAME);

  if (!shell_integration_linux::CreateAppListDesktopShortcut(
           app_list::kAppListWMClass,
           app_list_title)) {
    LOG(WARNING) << "Unable to create App Launcher shortcut.";
  }
}

}  // namespace

AppListServiceLinux::~AppListServiceLinux() {}

// static
AppListServiceLinux* AppListServiceLinux::GetInstance() {
  return base::Singleton<AppListServiceLinux, base::LeakySingletonTraits<
                                                  AppListServiceLinux>>::get();
}

void AppListServiceLinux::CreateShortcut() {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE, base::Bind(&CreateShortcuts));
}

void AppListServiceLinux::OnActivationChanged(views::Widget* /*widget*/,
                                              bool active) {
  if (active)
    return;

  if (app_list::switches::ShouldNotDismissOnBlur())
    return;

  // Dismiss the app list asynchronously. This must be done asynchronously
  // or our caller will crash, as it expects the app list to remain alive.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&AppListServiceLinux::DismissAppList, base::Unretained(this)));
}

AppListServiceLinux::AppListServiceLinux()
    : AppListServiceViews(std::unique_ptr<AppListControllerDelegate>(
          new AppListControllerDelegateViews(this))) {}

void AppListServiceLinux::OnViewCreated() {
  shower().app_list()->AddObserver(this);
}

void AppListServiceLinux::OnViewBeingDestroyed() {
  shower().app_list()->RemoveObserver(this);
  AppListServiceViews::OnViewBeingDestroyed();
}

void AppListServiceLinux::OnViewDismissed() {
}

void AppListServiceLinux::MoveNearCursor(app_list::AppListView* view) {
  AppListLinux::MoveNearCursor(view);
}

// static
AppListService* AppListService::Get() {
#if defined(USE_ASH)
  return AppListServiceAsh::GetInstance();
#else
  return AppListServiceLinux::GetInstance();
#endif
}

// static
void AppListService::InitAll(Profile* initial_profile,
                             const base::FilePath& profile_path) {
#if defined(USE_ASH)
  AppListServiceAsh::GetInstance()->Init(initial_profile);
#else
  AppListServiceLinux::GetInstance()->Init(initial_profile);
#endif
}
