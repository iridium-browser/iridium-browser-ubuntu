// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <utility>
#include <vector>

#include "ash/common/new_window_delegate.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
#include "components/arc/intent_helper/link_handler_model_impl.h"
#include "components/arc/intent_helper/local_activity_resolver.h"
#include "components/arc/set_wallpaper_delegate.h"
#include "ui/base/layout.h"
#include "url/gurl.h"

namespace arc {

namespace {

constexpr char kArcIntentHelperPackageName[] = "org.chromium.arc.intent_helper";

}  // namespace

ArcIntentHelperBridge::ArcIntentHelperBridge(
    ArcBridgeService* bridge_service,
    const scoped_refptr<ActivityIconLoader>& icon_loader,
    std::unique_ptr<SetWallpaperDelegate> set_wallpaper_delegate,
    const scoped_refptr<LocalActivityResolver>& activity_resolver)
    : ArcService(bridge_service),
      binding_(this),
      icon_loader_(icon_loader),
      set_wallpaper_delegate_(std::move(set_wallpaper_delegate)),
      activity_resolver_(activity_resolver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->intent_helper()->AddObserver(this);
}

ArcIntentHelperBridge::~ArcIntentHelperBridge() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->intent_helper()->RemoveObserver(this);
}

void ArcIntentHelperBridge::OnInstanceReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::Shell::GetInstance()->set_link_handler_model_factory(this);
  arc_bridge_service()->intent_helper()->instance()->Init(
      binding_.CreateInterfacePtrAndBind());
}

void ArcIntentHelperBridge::OnInstanceClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::Shell::GetInstance()->set_link_handler_model_factory(nullptr);
}

void ArcIntentHelperBridge::OnIconInvalidated(
    const mojo::String& package_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  icon_loader_->InvalidateIcons(package_name);
}

void ArcIntentHelperBridge::OnOpenDownloads() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(607411): If the FileManager is not yet open this will open to
  // downloads by default, which is what we want.  However if it is open it will
  // simply be brought to the forgeground without forcibly being navigated to
  // downloads, which is probably not ideal.
  ash::WmShell::Get()->new_window_delegate()->OpenFileManager();
}

void ArcIntentHelperBridge::OnOpenUrl(const mojo::String& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GURL gurl(url.get());
  ash::WmShell::Get()->delegate()->OpenUrlFromArc(gurl);
}

void ArcIntentHelperBridge::OpenWallpaperPicker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::WmShell::Get()->wallpaper_delegate()->OpenSetWallpaperPage();
}

void ArcIntentHelperBridge::SetWallpaper(mojo::Array<uint8_t> jpeg_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  set_wallpaper_delegate_->SetWallpaper(jpeg_data.PassStorage());
}

std::unique_ptr<ash::LinkHandlerModel> ArcIntentHelperBridge::CreateModel(
    const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<LinkHandlerModelImpl> impl(
      new LinkHandlerModelImpl(icon_loader_));
  if (!impl->Init(url))
    return nullptr;
  return std::move(impl);
}

// static
bool ArcIntentHelperBridge::IsIntentHelperPackage(
    const std::string& package_name) {
  return package_name == kArcIntentHelperPackageName;
}

// static
mojo::Array<mojom::UrlHandlerInfoPtr>
ArcIntentHelperBridge::FilterOutIntentHelper(
    mojo::Array<mojom::UrlHandlerInfoPtr> handlers) {
  mojo::Array<mojom::UrlHandlerInfoPtr> handlers_filtered;
  for (auto& handler : handlers) {
    if (IsIntentHelperPackage(handler->package_name.get()))
      continue;
    handlers_filtered.push_back(std::move(handler));
  }
  return handlers_filtered;
}

// static
mojom::IntentHelperInstance*
ArcIntentHelperBridge::GetIntentHelperInstanceWithErrorCode(
    int min_instance_version,
    GetResult* out_error_code) {
  ArcBridgeService* bridge_service = ArcBridgeService::Get();
  if (!bridge_service) {
    if (!ArcBridgeService::GetEnabled(base::CommandLine::ForCurrentProcess())) {
      VLOG(2) << "ARC bridge is not supported.";
      if (out_error_code)
        *out_error_code = GetResult::FAILED_ARC_NOT_SUPPORTED;
    } else {
      VLOG(2) << "ARC bridge is not ready.";
      if (out_error_code)
        *out_error_code = GetResult::FAILED_ARC_NOT_READY;
    }
    return nullptr;
  }
  mojom::IntentHelperInstance* intent_helper_instance =
      bridge_service->intent_helper()->instance();
  if (!intent_helper_instance) {
    VLOG(2) << "ARC intent helper instance is not ready.";
    if (out_error_code)
      *out_error_code = GetResult::FAILED_ARC_NOT_READY;
    return nullptr;
  }
  const int version = bridge_service->intent_helper()->version();
  if (version < min_instance_version) {
    VLOG(1) << "ARC intent helper instance is too old. required: "
            << min_instance_version << ", actual: " << version;
    if (out_error_code)
      *out_error_code = GetResult::FAILED_ARC_NOT_SUPPORTED;
    return nullptr;
  }
  return intent_helper_instance;
}

// static
mojom::IntentHelperInstance* ArcIntentHelperBridge::GetIntentHelperInstance(
    int min_instance_version) {
  return GetIntentHelperInstanceWithErrorCode(min_instance_version, nullptr);
}

void ArcIntentHelperBridge::OnIntentFiltersUpdated(
    mojo::Array<mojom::IntentFilterPtr> filters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  activity_resolver_->UpdateIntentFilters(std::move(filters));
}

}  // namespace arc
