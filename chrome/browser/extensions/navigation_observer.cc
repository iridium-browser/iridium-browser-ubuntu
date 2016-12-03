// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/navigation_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"

using content::NavigationController;
using content::NavigationEntry;

namespace extensions {

NavigationObserver::NavigationObserver(Profile* profile)
    : profile_(profile),
      extension_registry_observer_(this),
      weak_factory_(this) {
  RegisterForNotifications();
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
}

NavigationObserver::~NavigationObserver() {}

void NavigationObserver::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_NAV_ENTRY_COMMITTED, type);

  NavigationController* controller =
      content::Source<NavigationController>(source).ptr();
  if (!profile_->IsSameProfile(
          Profile::FromBrowserContext(controller->GetBrowserContext())))
    return;

  PromptToEnableExtensionIfNecessary(controller);
}

void NavigationObserver::RegisterForNotifications() {
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
}

void NavigationObserver::PromptToEnableExtensionIfNecessary(
    NavigationController* nav_controller) {
  // Bail out if we're already running a prompt.
  if (!in_progress_prompt_extension_id_.empty())
    return;

  NavigationEntry* nav_entry = nav_controller->GetVisibleEntry();
  if (!nav_entry)
    return;

  ExtensionRegistry* registry = extensions::ExtensionRegistry::Get(profile_);
  const Extension* extension =
      registry->disabled_extensions().GetExtensionOrAppByURL(
          nav_entry->GetURL());
  if (!extension)
    return;

  // Try not to repeatedly prompt the user about the same extension.
  if (prompted_extensions_.find(extension->id()) != prompted_extensions_.end())
    return;
  prompted_extensions_.insert(extension->id());

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile_);
  if (extension_prefs->DidExtensionEscalatePermissions(extension->id())) {
    // Keep track of the extension id and nav controller we're prompting for.
    // These must be reset in OnInstallPromptDone.
    in_progress_prompt_extension_id_ = extension->id();
    in_progress_prompt_navigation_controller_ = nav_controller;

    extension_install_prompt_.reset(
        new ExtensionInstallPrompt(nav_controller->GetWebContents()));
    ExtensionInstallPrompt::PromptType type =
        ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(profile_,
                                                                  extension);
    extension_install_prompt_->ShowDialog(
        base::Bind(&NavigationObserver::OnInstallPromptDone,
                   weak_factory_.GetWeakPtr()),
        extension, nullptr,
        base::WrapUnique(new ExtensionInstallPrompt::Prompt(type)),
        ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  }
}

void NavigationObserver::OnInstallPromptDone(
    ExtensionInstallPrompt::Result result) {
  // The extension was already uninstalled.
  if (in_progress_prompt_extension_id_.empty())
    return;

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension = extension_service->GetExtensionById(
      in_progress_prompt_extension_id_, true);
  CHECK(extension);

  if (result == ExtensionInstallPrompt::Result::ACCEPTED) {
    NavigationController* nav_controller =
        in_progress_prompt_navigation_controller_;
    CHECK(nav_controller);

    // Grant permissions, re-enable the extension, and then reload the tab.
    extension_service->GrantPermissionsAndEnableExtension(extension);
    nav_controller->Reload(true);
  } else {
    std::string histogram_name =
       result == ExtensionInstallPrompt::Result::USER_CANCELED
            ? "ReEnableCancel"
            : "ReEnableAbort";
    ExtensionService::RecordPermissionMessagesHistogram(extension,
                                                        histogram_name.c_str());
  }

  in_progress_prompt_extension_id_ = std::string();
  in_progress_prompt_navigation_controller_ = nullptr;
  extension_install_prompt_.reset();
}

void NavigationObserver::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  if (in_progress_prompt_extension_id_.empty() ||
      in_progress_prompt_extension_id_ != extension->id()) {
    return;
  }

  in_progress_prompt_extension_id_ = std::string();
  in_progress_prompt_navigation_controller_ = nullptr;
  extension_install_prompt_.reset();
}

}  // namespace extensions
