// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/chrome_browser_policy_connector.h"

#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"

#if defined(OS_WIN)
#include "components/policy/core/common/policy_loader_win.h"
#elif defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#elif defined(OS_ANDROID)
#include "components/policy/core/browser/android/android_combined_policy_provider.h"
#endif

using content::BrowserThread;

namespace policy {

namespace {

#if defined(OS_MACOSX)
base::FilePath GetManagedPolicyPath() {
  CFBundleRef bundle(CFBundleGetMainBundle());
  if (!bundle)
    return base::FilePath();

  CFStringRef bundle_id = CFBundleGetIdentifier(bundle);
  if (!bundle_id)
    return base::FilePath();

  return policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id);
}
#endif  // defined(OS_MACOSX)

}  // namespace

ChromeBrowserPolicyConnector::ChromeBrowserPolicyConnector()
    : BrowserPolicyConnector(base::Bind(&BuildHandlerList)) {
  ConfigurationPolicyProvider* platform_provider = CreatePlatformProvider();
  if (platform_provider)
    SetPlatformPolicyProvider(make_scoped_ptr(platform_provider));
}

ChromeBrowserPolicyConnector::~ChromeBrowserPolicyConnector() {}

void ChromeBrowserPolicyConnector::Init(
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  // Initialization of some of the providers requires the FILE thread; make
  // sure that threading is ready at this point.
  DCHECK(BrowserThread::IsThreadInitialized(BrowserThread::FILE));

  scoped_ptr<DeviceManagementService::Configuration> configuration(
      new DeviceManagementServiceConfiguration(
          BrowserPolicyConnector::GetDeviceManagementUrl()));
  scoped_ptr<DeviceManagementService> device_management_service(
      new DeviceManagementService(configuration.Pass()));
  device_management_service->ScheduleInitialization(
      kServiceInitializationStartupDelay);

  InitInternal(local_state, device_management_service.Pass());
}

ConfigurationPolicyProvider*
    ChromeBrowserPolicyConnector::CreatePlatformProvider() {
#if defined(OS_WIN)
  scoped_ptr<AsyncPolicyLoader> loader(PolicyLoaderWin::Create(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      kRegistryChromePolicyKey));
  return new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass());
#elif defined(OS_MACOSX)
  scoped_ptr<AsyncPolicyLoader> loader(new PolicyLoaderMac(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      GetManagedPolicyPath(),
      new MacPreferences()));
  return new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass());
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  base::FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    scoped_ptr<AsyncPolicyLoader> loader(new ConfigDirPolicyLoader(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        config_dir_path,
        POLICY_SCOPE_MACHINE));
    return new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass());
  } else {
    return NULL;
  }
#elif defined(OS_ANDROID)
  return new policy::android::AndroidCombinedPolicyProvider(
      GetSchemaRegistry());
#else
  return NULL;
#endif
}

}  // namespace policy
