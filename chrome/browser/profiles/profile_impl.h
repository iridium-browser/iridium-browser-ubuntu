// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl_io_data.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/host_zoom_map.h"

class NetPrefObserver;
class PrefService;
class PrefServiceSyncable;
class ShortcutsBackend;
class SSLConfigServiceManager;
class TrackedPreferenceValidationDelegate;

#if defined(OS_CHROMEOS)
namespace chromeos {
class KioskTest;
class LocaleChangeGuard;
class Preferences;
class SupervisedUserTestBase;
}
#endif

namespace base {
class SequencedTaskRunner;
}

namespace domain_reliability {
class DomainReliabilityMonitor;
}

namespace extensions {
class ExtensionSystem;
}

namespace policy {
class CloudPolicyManager;
class ProfilePolicyConnector;
class SchemaRegistryService;
}

namespace user_prefs {
class refRegistrySyncable;
}

// The default profile implementation.
class ProfileImpl : public Profile {
 public:
  // Value written to prefs when the exit type is EXIT_NORMAL. Public for tests.
  static const char* const kPrefExitTypeNormal;

  ~ProfileImpl() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // content::BrowserContext implementation:
  scoped_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  base::FilePath GetPath() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  net::URLRequestContextGetter* GetRequestContext() override;
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

  // Profile implementation:
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  // Note that this implementation returns the Google-services username, if any,
  // not the Chrome user's display name.
  std::string GetProfileUserName() const override;
  ProfileType GetProfileType() const override;
  bool IsOffTheRecord() const override;
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
  chrome::ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;
  PrefService* GetOffTheRecordPrefs() override;
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
  chrome_browser_net::Predictor* GetNetworkPredictor() override;
  DevToolsNetworkController* GetDevToolsNetworkController() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   const base::Closure& completion) override;
  GURL GetHomePage() override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  void SetExitType(ExitType exit_type) override;
  ExitType GetLastSessionExitType() override;

#if defined(OS_CHROMEOS)
  void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;
#endif  // defined(OS_CHROMEOS)

  PrefProxyConfigTracker* GetProxyConfigTracker() override;

 private:
#if defined(OS_CHROMEOS)
  friend class chromeos::KioskTest;
  friend class chromeos::SupervisedUserTestBase;
#endif
  friend class Profile;
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ProfilesLaunchedAfterCrash);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest, DISABLED_ProfileReadmeCreated);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest,
                           ProfileDeletedBeforeReadmeCreated);

  // Delay, in milliseconds, before README file is created for a new profile.
  // This is non-const for testing purposes.
  static int create_readme_delay_ms;

  ProfileImpl(const base::FilePath& path,
              Delegate* delegate,
              CreateMode create_mode,
              base::SequencedTaskRunner* sequenced_task_runner);

  // Does final initialization. Should be called after prefs were loaded.
  void DoFinalInit();

  // Switch locale (when possible) and proceed to OnLocaleReady().
  void OnPrefsLoaded(CreateMode create_mode, bool success);

  // Does final prefs initialization and calls Init().
  void OnLocaleReady();

#if defined(ENABLE_SESSION_SERVICE)
  void StopCreateSessionServiceTimer();

  void EnsureSessionServiceCreated();
#endif


  void EnsureRequestContextCreated() {
    GetRequestContext();
  }

  // Updates the ProfileInfoCache with data from this profile.
  void UpdateProfileSupervisedUserIdCache();
  void UpdateProfileNameCache();
  void UpdateProfileAvatarCache();
  void UpdateProfileIsEphemeralCache();

  void GetCacheParameters(bool is_media_context,
                          base::FilePath* cache_path,
                          int* max_size);

  PrefProxyConfigTracker* CreateProxyConfigTracker();

  scoped_ptr<domain_reliability::DomainReliabilityMonitor>
      CreateDomainReliabilityMonitor(PrefService* local_state);

  PrefChangeRegistrar pref_change_registrar_;

  base::FilePath path_;
  base::FilePath base_cache_path_;

  // !!! BIG HONKING WARNING !!!
  //  The order of the members below is important. Do not change it unless
  //  you know what you're doing. Also, if adding a new member here make sure
  //  that the declaration occurs AFTER things it depends on as destruction
  //  happens in reverse order of declaration.

  // TODO(mnissler, joaodasilva): The |profile_policy_connector_| provides the
  // PolicyService that the |prefs_| depend on, and must outlive |prefs_|.
// This can be removed once |prefs_| becomes a KeyedService too.
// |profile_policy_connector_| in turn depends on |cloud_policy_manager_|,
// which depends on |schema_registry_service_|.
#if defined(ENABLE_CONFIGURATION_POLICY)
  scoped_ptr<policy::SchemaRegistryService> schema_registry_service_;
  scoped_ptr<policy::CloudPolicyManager> cloud_policy_manager_;
#endif
  scoped_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  // Keep |pref_validation_delegate_| above |prefs_| so that the former outlives
  // the latter.
  scoped_ptr<TrackedPreferenceValidationDelegate> pref_validation_delegate_;

  // Keep |prefs_| on top for destruction order because |extension_prefs_|,
  // |net_pref_observer_|, |io_data_| and others store pointers to |prefs_| and
  // shall be destructed first.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  scoped_ptr<PrefServiceSyncable> prefs_;
  scoped_ptr<PrefServiceSyncable> otr_prefs_;
  ProfileImplIOData::Handle io_data_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;
#endif
  scoped_ptr<NetPrefObserver> net_pref_observer_;
  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<ShortcutsBackend> shortcuts_backend_;

  // Exit type the last time the profile was opened. This is set only once from
  // prefs.
  ExitType last_session_exit_type_;

#if defined(ENABLE_SESSION_SERVICE)
  base::OneShotTimer<ProfileImpl> create_session_service_timer_;
#endif

  scoped_ptr<Profile> off_the_record_profile_;

  // See GetStartTime for details.
  base::Time start_time_;

#if defined(OS_CHROMEOS)
  scoped_ptr<chromeos::Preferences> chromeos_preferences_;

  scoped_ptr<chromeos::LocaleChangeGuard> locale_change_guard_;
#endif

  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  // STOP!!!! DO NOT ADD ANY MORE ITEMS HERE!!!!
  //
  // Instead, make your Service/Manager/whatever object you're hanging off the
  // Profile use our new BrowserContextKeyedServiceFactory system instead.
  // You can find the design document here:
  //
  //   https://sites.google.com/a/chromium.org/dev/developers/design-documents/profile-architecture
  //
  // and you can read the raw headers here:
  //
  // components/keyed_service/content/browser_context_dependency_manager.*
  // components/keyed_service/core/keyed_service.h
  // components/keyed_service/content/browser_context_keyed_service_factory.*

  Profile::Delegate* delegate_;

  chrome_browser_net::Predictor* predictor_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
