// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_
#define COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/link_handler_model_factory.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class LinkHandlerModel;

}  // namespace ash

namespace arc {

class ActivityIconLoader;
class ArcBridgeService;
class IntentFilter;
class LocalActivityResolver;

// Receives intents from ARC.
class ArcIntentHelperBridge
    : public ArcService,
      public InstanceHolder<mojom::IntentHelperInstance>::Observer,
      public mojom::IntentHelperHost,
      public ash::LinkHandlerModelFactory {
 public:
  enum class GetResult {
    // Failed. The intent_helper instance is not yet ready. This is a temporary
    // error.
    FAILED_ARC_NOT_READY,
    // Failed. Either ARC is not supported at all or intent_helper instance
    // version is too old.
    FAILED_ARC_NOT_SUPPORTED,
  };

  ArcIntentHelperBridge(
      ArcBridgeService* bridge_service,
      const scoped_refptr<ActivityIconLoader>& icon_loader,
      const scoped_refptr<LocalActivityResolver>& activity_resolver);
  ~ArcIntentHelperBridge() override;

  void AddObserver(ArcIntentHelperObserver* observer);
  void RemoveObserver(ArcIntentHelperObserver* observer);

  // InstanceHolder<mojom::IntentHelperInstance>::Observer
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // mojom::IntentHelperHost
  void OnIconInvalidated(const std::string& package_name) override;
  void OnIntentFiltersUpdated(
      std::vector<IntentFilter> intent_filters) override;
  void OnOpenDownloads() override;
  void OnOpenUrl(const std::string& url) override;
  void OpenWallpaperPicker() override;
  void SetWallpaperDeprecated(const std::vector<uint8_t>& jpeg_data) override;

  // ash::LinkHandlerModelFactory
  std::unique_ptr<ash::LinkHandlerModel> CreateModel(const GURL& url) override;

  // Returns false if |package_name| is for the intent_helper apk.
  static bool IsIntentHelperPackage(const std::string& package_name);

  // Filters out handlers that belong to the intent_helper apk and returns
  // a new array.
  static std::vector<mojom::IntentHandlerInfoPtr> FilterOutIntentHelper(
      std::vector<mojom::IntentHandlerInfoPtr> handlers);

  // Checks if the intent helper interface is available. When it is not, returns
  // false and updates |out_error_code| if it's not nullptr.
  static bool IsIntentHelperAvailable(GetResult* out_error_code);

  // For supporting ArcServiceManager::GetService<T>().
  static const char kArcServiceName[];

  static const char kArcIntentHelperPackageName[];

 private:
  mojo::Binding<mojom::IntentHelperHost> binding_;
  scoped_refptr<ActivityIconLoader> icon_loader_;
  scoped_refptr<LocalActivityResolver> activity_resolver_;

  base::ThreadChecker thread_checker_;

  base::ObserverList<ArcIntentHelperObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ArcIntentHelperBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_
