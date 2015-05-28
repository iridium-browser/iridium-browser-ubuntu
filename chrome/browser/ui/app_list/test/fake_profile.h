// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "components/domain_reliability/clear_mode.h"
#include "content/public/browser/browser_context.h"

class ResourceContext;

namespace net {
class URLRequestContextGetter;
}

namespace content {
class DownloadManagerDelegate;
class ResourceContext;
class SSLHostStateDelegate;
class ZoomLevelDelegate;
}

class FakeProfile : public Profile {
 public:
  explicit FakeProfile(const std::string& name);
  FakeProfile(const std::string& name, const base::FilePath& path);

  // Profile overrides.
  std::string GetProfileUserName() const override;
  ProfileType GetProfileType() const override;
  base::FilePath GetPath() const override;
  scoped_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) override;
  net::URLRequestContextGetter* GetMediaRequestContext() override;
  net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) override;
  net::URLRequestContextGetter* GetMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;
  content::ResourceContext* GetResourceContext() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  Profile* GetOffTheRecordProfile() override;
  void DestroyOffTheRecordProfile() override;
  bool HasOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  bool IsSupervised() override;
  bool IsChild() override;
  bool IsLegacySupervised() override;
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  PrefService* GetOffTheRecordPrefs() override;
  net::URLRequestContextGetter* GetRequestContext() override;
  net::URLRequestContextGetter* GetRequestContextForExtensions() override;
  net::SSLConfigService* GetSSLConfigService() override;
  HostContentSettingsMap* GetHostContentSettingsMap() override;
  bool IsSameProfile(Profile* profile) override;
  base::Time GetStartTime() const override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;

#if defined(OS_CHROMEOS)
  void ChangeAppLocale(const std::string& locale,
                       AppLocaleChangedVia via) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;
#endif  // defined(OS_CHROMEOS)

  PrefProxyConfigTracker* GetProxyConfigTracker() override;
  chrome_browser_net::Predictor* GetNetworkPredictor() override;
  DevToolsNetworkController* GetDevToolsNetworkController() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   const base::Closure& completion) override;
  GURL GetHomePage() override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  void SetExitType(ExitType exit_type) override;
  ExitType GetLastSessionExitType() override;

 private:
  std::string name_;
  base::FilePath path_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_PROFILE_H_
