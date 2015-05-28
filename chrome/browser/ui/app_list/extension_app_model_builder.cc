// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_model_builder.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using extensions::Extension;

ExtensionAppModelBuilder::ExtensionAppModelBuilder(
    AppListControllerDelegate* controller)
    : service_(NULL),
      profile_(NULL),
      controller_(controller),
      model_(NULL),
      tracker_(NULL),
      extension_registry_(NULL) {
}

ExtensionAppModelBuilder::~ExtensionAppModelBuilder() {
  OnShutdown();
  OnShutdown(extension_registry_);
  if (!service_)
    model_->top_level_item_list()->RemoveObserver(this);
}

void ExtensionAppModelBuilder::InitializeWithService(
    app_list::AppListSyncableService* service,
    app_list::AppListModel* model) {
  DCHECK(!service_ && !profile_);
  model_ = model;
  service_ = service;
  profile_ = service->profile();
  InitializePrefChangeRegistrars();

  BuildModel();
}

void ExtensionAppModelBuilder::InitializeWithProfile(
    Profile* profile,
    app_list::AppListModel* model) {
  DCHECK(!service_ && !profile_);
  model_ = model;
  model_->top_level_item_list()->AddObserver(this);
  profile_ = profile;
  InitializePrefChangeRegistrars();

  BuildModel();
}

void ExtensionAppModelBuilder::InitializePrefChangeRegistrars() {
  profile_pref_change_registrar_.Init(profile_->GetPrefs());
  profile_pref_change_registrar_.Add(
      prefs::kHideWebStoreIcon,
      base::Bind(&ExtensionAppModelBuilder::OnProfilePreferenceChanged,
                 base::Unretained(this)));

  if (!extensions::util::IsNewBookmarkAppsEnabled())
    return;

  // TODO(calamity): analyze the performance impact of doing this every
  // extension pref change.
  extensions::ExtensionsBrowserClient* client =
      extensions::ExtensionsBrowserClient::Get();
  extension_pref_change_registrar_.Init(
      client->GetPrefServiceForContext(profile_));
  extension_pref_change_registrar_.Add(
    extensions::pref_names::kExtensions,
    base::Bind(&ExtensionAppModelBuilder::OnExtensionPreferenceChanged,
               base::Unretained(this)));
}

void ExtensionAppModelBuilder::OnProfilePreferenceChanged() {
  extensions::ExtensionSet extensions;
  controller_->GetApps(profile_, &extensions);

  for (extensions::ExtensionSet::const_iterator app = extensions.begin();
       app != extensions.end(); ++app) {
    bool should_display =
        extensions::ui_util::ShouldDisplayInAppLauncher(app->get(), profile_);
    bool does_display = GetExtensionAppItem((*app)->id()) != NULL;

    if (should_display == does_display)
      continue;

    if (should_display) {
      InsertApp(CreateAppItem((*app)->id(),
                              "",
                              gfx::ImageSkia(),
                              (*app)->is_platform_app()));
    } else {
      if (service_)
        service_->RemoveItem((*app)->id());
      else
        model_->DeleteItem((*app)->id());
    }
  }
}

void ExtensionAppModelBuilder::OnExtensionPreferenceChanged() {
  model_->NotifyExtensionPreferenceChanged();
}

void ExtensionAppModelBuilder::OnBeginExtensionInstall(
    const ExtensionInstallParams& params) {
  if (!params.is_app || params.is_ephemeral)
    return;

  DVLOG(2) << service_ << ": OnBeginExtensionInstall: "
           << params.extension_id.substr(0, 8);
  ExtensionAppItem* existing_item = GetExtensionAppItem(params.extension_id);
  if (existing_item) {
    existing_item->SetIsInstalling(true);
    return;
  }

  // Icons from the webstore can be unusual sizes. Once installed,
  // ExtensionAppItem uses extension_misc::EXTENSION_ICON_MEDIUM (48) to load
  // it, so be consistent with that.
  gfx::Size icon_size(extension_misc::EXTENSION_ICON_MEDIUM,
                      extension_misc::EXTENSION_ICON_MEDIUM);
  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      params.installing_icon, skia::ImageOperations::RESIZE_BEST, icon_size));

  InsertApp(CreateAppItem(params.extension_id,
                          params.extension_name,
                          resized,
                          params.is_platform_app));
}

void ExtensionAppModelBuilder::OnDownloadProgress(
    const std::string& extension_id,
    int percent_downloaded) {
  ExtensionAppItem* item = GetExtensionAppItem(extension_id);
  if (!item)
    return;
  item->SetPercentDownloaded(percent_downloaded);
}

void ExtensionAppModelBuilder::OnInstallFailure(
    const std::string& extension_id) {
  model_->DeleteItem(extension_id);
}

void ExtensionAppModelBuilder::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!extensions::ui_util::ShouldDisplayInAppLauncher(extension, profile_))
    return;

  DVLOG(2) << service_ << ": OnExtensionLoaded: "
           << extension->id().substr(0, 8);
  ExtensionAppItem* existing_item = GetExtensionAppItem(extension->id());
  if (existing_item) {
    existing_item->Reload();
    if (service_)
      service_->UpdateItem(existing_item);
    return;
  }

  InsertApp(CreateAppItem(extension->id(),
                          "",
                          gfx::ImageSkia(),
                          extension->is_platform_app()));
}

void ExtensionAppModelBuilder::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  ExtensionAppItem* item = GetExtensionAppItem(extension->id());
  if (!item)
    return;
  item->UpdateIcon();
}

void ExtensionAppModelBuilder::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (service_) {
    DVLOG(2) << service_ << ": OnExtensionUninstalled: "
             << extension->id().substr(0, 8);
    service_->RemoveUninstalledItem(extension->id());
    return;
  }
  model_->DeleteUninstalledItem(extension->id());
}

void ExtensionAppModelBuilder::OnDisabledExtensionUpdated(
    const Extension* extension) {
  if (!extensions::ui_util::ShouldDisplayInAppLauncher(extension, profile_))
    return;

  ExtensionAppItem* existing_item = GetExtensionAppItem(extension->id());
  if (existing_item)
    existing_item->Reload();
}

void ExtensionAppModelBuilder::OnShutdown() {
  if (tracker_) {
    tracker_->RemoveObserver(this);
    tracker_ = NULL;
  }
}

void ExtensionAppModelBuilder::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  if (!extension_registry_)
    return;

  DCHECK_EQ(extension_registry_, registry);
  extension_registry_->RemoveObserver(this);
  extension_registry_ = NULL;
}

scoped_ptr<ExtensionAppItem> ExtensionAppModelBuilder::CreateAppItem(
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon,
    bool is_platform_app) {
  const app_list::AppListSyncableService::SyncItem* sync_item =
      service_ ? service_->GetSyncItem(extension_id) : NULL;
  return make_scoped_ptr(new ExtensionAppItem(profile_,
                                              sync_item,
                                              extension_id,
                                              extension_name,
                                              installing_icon,
                                              is_platform_app));
}

void ExtensionAppModelBuilder::BuildModel() {
  DCHECK(!tracker_);
  tracker_ = controller_->GetInstallTrackerFor(profile_);
  extension_registry_ = extensions::ExtensionRegistry::Get(profile_);

  PopulateApps();

  // Start observing after model is built.
  if (tracker_)
    tracker_->AddObserver(this);

  if (extension_registry_)
    extension_registry_->AddObserver(this);
}

void ExtensionAppModelBuilder::PopulateApps() {
  extensions::ExtensionSet extensions;
  controller_->GetApps(profile_, &extensions);

  for (extensions::ExtensionSet::const_iterator app = extensions.begin();
       app != extensions.end(); ++app) {
    if (!extensions::ui_util::ShouldDisplayInAppLauncher(app->get(), profile_))
      continue;
    InsertApp(CreateAppItem((*app)->id(),
                            "",
                            gfx::ImageSkia(),
                            (*app)->is_platform_app()));
  }
}

void ExtensionAppModelBuilder::InsertApp(scoped_ptr<ExtensionAppItem> app) {
  if (service_) {
    service_->AddItem(app.Pass());
    return;
  }
  model_->AddItem(app.Pass());
}

ExtensionAppItem* ExtensionAppModelBuilder::GetExtensionAppItem(
    const std::string& extension_id) {
  app_list::AppListItem* item = model_->FindItem(extension_id);
  LOG_IF(ERROR, item &&
         item->GetItemType() != ExtensionAppItem::kItemType)
      << "App Item matching id: " << extension_id
      << " has incorrect type: '" << item->GetItemType() << "'";
  return static_cast<ExtensionAppItem*>(item);
}

void ExtensionAppModelBuilder::OnListItemMoved(size_t from_index,
                                               size_t to_index,
                                               app_list::AppListItem* item) {
  DCHECK(!service_);

  // This will get called from AppListItemList::ListItemMoved after
  // set_position is called for the item.
  if (item->GetItemType() != ExtensionAppItem::kItemType)
    return;

  app_list::AppListItemList* item_list = model_->top_level_item_list();
  ExtensionAppItem* prev = NULL;
  for (size_t idx = to_index; idx > 0; --idx) {
    app_list::AppListItem* item = item_list->item_at(idx - 1);
    if (item->GetItemType() == ExtensionAppItem::kItemType) {
      prev = static_cast<ExtensionAppItem*>(item);
      break;
    }
  }
  ExtensionAppItem* next = NULL;
  for (size_t idx = to_index; idx < item_list->item_count() - 1; ++idx) {
    app_list::AppListItem* item = item_list->item_at(idx + 1);
    if (item->GetItemType() == ExtensionAppItem::kItemType) {
      next = static_cast<ExtensionAppItem*>(item);
      break;
    }
  }
  // item->Move will call set_position, overriding the item's position.
  if (prev || next)
    static_cast<ExtensionAppItem*>(item)->Move(prev, next);
}
