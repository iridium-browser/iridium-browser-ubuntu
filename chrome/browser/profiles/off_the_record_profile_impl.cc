// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/json_pref_store.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_distiller/profile_utils.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_url_request_context_getter.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_otr_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ui/zoom/zoom_event_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "storage/browser/database/database_tracker.h"

#if defined(OS_ANDROID)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#endif

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/content_settings/content_settings_supervised_provider.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

using content::BrowserThread;
using content::DownloadManagerDelegate;
using content::HostZoomMap;

#if defined(ENABLE_EXTENSIONS)
namespace {

void NotifyOTRProfileCreatedOnIOThread(void* original_profile,
                                       void* otr_profile) {
  ExtensionWebRequestEventRouter::GetInstance()->OnOTRBrowserContextCreated(
      original_profile, otr_profile);
}

void NotifyOTRProfileDestroyedOnIOThread(void* original_profile,
                                         void* otr_profile) {
  ExtensionWebRequestEventRouter::GetInstance()->OnOTRBrowserContextDestroyed(
      original_profile, otr_profile);
}

}  // namespace
#endif

OffTheRecordProfileImpl::OffTheRecordProfileImpl(Profile* real_profile)
    : profile_(real_profile),
      prefs_(PrefServiceSyncable::IncognitoFromProfile(real_profile)),
      start_time_(Time::Now()) {
  // Register on BrowserContext.
  user_prefs::UserPrefs::Set(this, prefs_);
}

void OffTheRecordProfileImpl::Init() {
  // The construction of OffTheRecordProfileIOData::Handle needs the profile
  // type returned by this->GetProfileType().  Since GetProfileType() is a
  // virtual member function, we cannot call the function defined in the most
  // derived class (e.g. GuestSessionProfile) until a ctor finishes.  Thus,
  // we have to instantiate OffTheRecordProfileIOData::Handle here after a ctor.
  InitIoData();

#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
  // Because UserCloudPolicyManager is in a component, it cannot access
  // GetOriginalProfile. Instead, we have to inject this relation here.
  policy::UserCloudPolicyManagerFactory::RegisterForOffTheRecordBrowserContext(
      this->GetOriginalProfile(), this);
#endif

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  set_is_guest_profile(
      profile_->GetPath() == ProfileManager::GetGuestProfilePath());

  // Guest profiles may always be OTR. Check IncognitoModePrefs otherwise.
  DCHECK(profile_->IsGuestSession() ||
         IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
             IncognitoModePrefs::DISABLED);

  // TODO(oshima): Remove the need to eagerly initialize the request context
  // getter. chromeos::OnlineAttempt is illegally trying to access this
  // Profile member from a thread other than the UI thread, so we need to
  // prevent a race.
#if defined(OS_CHROMEOS)
  GetRequestContext();
#endif  // defined(OS_CHROMEOS)

  TrackZoomLevelsFromParent();

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      PluginPrefs::GetForProfile(this).get(),
      io_data_->GetResourceContextNoInit());
#endif

#if defined(ENABLE_EXTENSIONS)
  // Make the chrome//extension-icon/ resource available.
  extensions::ExtensionIconSource* icon_source =
      new extensions::ExtensionIconSource(profile_);
  content::URLDataSource::Add(this, icon_source);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyOTRProfileCreatedOnIOThread, profile_, this));
#endif

  // The DomDistillerViewerSource is not a normal WebUI so it must be registered
  // as a URLDataSource early.
  RegisterDomDistillerViewerSource(this);
}

OffTheRecordProfileImpl::~OffTheRecordProfileImpl() {
  MaybeSendDestroyedNotification();

#if defined(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
      io_data_->GetResourceContextNoInit());
#endif

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);

#if defined(ENABLE_EXTENSIONS)
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyOTRProfileDestroyedOnIOThread, profile_, this));
#endif

  if (host_content_settings_map_.get())
    host_content_settings_map_->ShutdownOnUIThread();

  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();

  // Clears any data the network stack contains that may be related to the
  // OTR session.
  g_browser_process->io_thread()->ChangedToOnTheRecord();
}

void OffTheRecordProfileImpl::InitIoData() {
  io_data_.reset(new OffTheRecordProfileIOData::Handle(this));
}

void OffTheRecordProfileImpl::TrackZoomLevelsFromParent() {
  DCHECK_NE(INCOGNITO_PROFILE, profile_->GetProfileType());

  // Here we only want to use zoom levels stored in the main-context's default
  // storage partition. We're not interested in zoom levels in special
  // partitions, e.g. those used by WebViewGuests.
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  HostZoomMap* parent_host_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(profile_);
  host_zoom_map->CopyFrom(parent_host_zoom_map);
  // Observe parent profile's HostZoomMap changes so they can also be applied
  // to this profile's HostZoomMap.
  track_zoom_subscription_ = parent_host_zoom_map->AddZoomLevelChangedCallback(
      base::Bind(&OffTheRecordProfileImpl::OnParentZoomLevelChanged,
                 base::Unretained(this)));
  if (!profile_->GetZoomLevelPrefs())
    return;

  // Also track changes to the parent profile's default zoom level.
  parent_default_zoom_level_subscription_ =
      profile_->GetZoomLevelPrefs()->RegisterDefaultZoomLevelCallback(
          base::Bind(&OffTheRecordProfileImpl::UpdateDefaultZoomLevel,
                     base::Unretained(this)));
}

std::string OffTheRecordProfileImpl::GetProfileUserName() const {
  // Incognito profile should not return the username.
  return std::string();
}

Profile::ProfileType OffTheRecordProfileImpl::GetProfileType() const {
#if !defined(OS_CHROMEOS)
  return profile_->IsGuestSession() ? GUEST_PROFILE : INCOGNITO_PROFILE;
#else
  return INCOGNITO_PROFILE;
#endif
}

base::FilePath OffTheRecordProfileImpl::GetPath() const {
  return profile_->GetPath();
}

scoped_ptr<content::ZoomLevelDelegate>
OffTheRecordProfileImpl::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return make_scoped_ptr(new chrome::ChromeZoomLevelOTRDelegate(
      ui_zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr()));
}

scoped_refptr<base::SequencedTaskRunner>
OffTheRecordProfileImpl::GetIOTaskRunner() {
  return profile_->GetIOTaskRunner();
}

bool OffTheRecordProfileImpl::IsOffTheRecord() const {
  return true;
}

Profile* OffTheRecordProfileImpl::GetOffTheRecordProfile() {
  return this;
}

void OffTheRecordProfileImpl::DestroyOffTheRecordProfile() {
  // Suicide is bad!
  NOTREACHED();
}

bool OffTheRecordProfileImpl::HasOffTheRecordProfile() {
  return true;
}

Profile* OffTheRecordProfileImpl::GetOriginalProfile() {
  return profile_;
}

ExtensionSpecialStoragePolicy*
    OffTheRecordProfileImpl::GetExtensionSpecialStoragePolicy() {
  return GetOriginalProfile()->GetExtensionSpecialStoragePolicy();
}

bool OffTheRecordProfileImpl::IsSupervised() {
  return GetOriginalProfile()->IsSupervised();
}

bool OffTheRecordProfileImpl::IsChild() {
  // TODO(treib): If we ever allow incognito for child accounts, evaluate
  // whether we want to just return false here.
  return GetOriginalProfile()->IsChild();
}

bool OffTheRecordProfileImpl::IsLegacySupervised() {
  return GetOriginalProfile()->IsLegacySupervised();
}

PrefService* OffTheRecordProfileImpl::GetPrefs() {
  return prefs_;
}

const PrefService* OffTheRecordProfileImpl::GetPrefs() const {
  return prefs_;
}

PrefService* OffTheRecordProfileImpl::GetOffTheRecordPrefs() {
  return prefs_;
}

DownloadManagerDelegate* OffTheRecordProfileImpl::GetDownloadManagerDelegate() {
  return DownloadServiceFactory::GetForBrowserContext(this)->
      GetDownloadManagerDelegate();
}

net::URLRequestContextGetter* OffTheRecordProfileImpl::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter* OffTheRecordProfileImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_->CreateMainRequestContextGetter(
      protocol_handlers, request_interceptors.Pass()).get();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForRenderProcess(
        int renderer_child_id) {
  content::RenderProcessHost* rph = content::RenderProcessHost::FromID(
      renderer_child_id);
  return rph->GetStoragePartition()->GetURLRequestContext();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetMediaRequestContext() {
  // In OTR mode, media request context is the same as the original one.
  return GetRequestContext();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetMediaRequestContextForRenderProcess(
        int renderer_child_id) {
  // In OTR mode, media request context is the same as the original one.
  return GetRequestContextForRenderProcess(renderer_child_id);
}

net::URLRequestContextGetter*
OffTheRecordProfileImpl::GetMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return io_data_->GetIsolatedAppRequestContextGetter(partition_path, in_memory)
      .get();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForExtensions() {
  return io_data_->GetExtensionsRequestContextGetter().get();
}

net::URLRequestContextGetter*
OffTheRecordProfileImpl::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return io_data_->CreateIsolatedAppRequestContextGetter(
      partition_path,
      in_memory,
      protocol_handlers,
      request_interceptors.Pass()).get();
}

content::ResourceContext* OffTheRecordProfileImpl::GetResourceContext() {
  return io_data_->GetResourceContext();
}

net::SSLConfigService* OffTheRecordProfileImpl::GetSSLConfigService() {
  return profile_->GetSSLConfigService();
}

HostContentSettingsMap* OffTheRecordProfileImpl::GetHostContentSettingsMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Retrieve the host content settings map of the parent profile in order to
  // ensure the preferences have been migrated.
  profile_->GetHostContentSettingsMap();
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(GetPrefs(), true);
#if defined(ENABLE_EXTENSIONS)
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(this)->extension_service();
    if (extension_service) {
      extension_service->RegisterContentSettings(
          host_content_settings_map_.get());
    }
#endif
#if defined(ENABLE_SUPERVISED_USERS)
    SupervisedUserSettingsService* supervised_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(this);
    scoped_ptr<content_settings::SupervisedProvider> supervised_provider(
        new content_settings::SupervisedProvider(supervised_service));
    host_content_settings_map_->RegisterProvider(
        HostContentSettingsMap::SUPERVISED_PROVIDER,
        supervised_provider.Pass());
#endif
  }
  return host_content_settings_map_.get();
}

content::BrowserPluginGuestManager* OffTheRecordProfileImpl::GetGuestManager() {
#if defined(ENABLE_EXTENSIONS)
  return guest_view::GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

storage::SpecialStoragePolicy*
OffTheRecordProfileImpl::GetSpecialStoragePolicy() {
#if defined(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

content::PushMessagingService*
OffTheRecordProfileImpl::GetPushMessagingService() {
  // TODO(johnme): Support push messaging in incognito if possible.
  return NULL;
}

content::SSLHostStateDelegate*
OffTheRecordProfileImpl::GetSSLHostStateDelegate() {
  return ChromeSSLHostStateDelegateFactory::GetForProfile(this);
}

// TODO(mlamouri): we should all these BrowserContext implementation to Profile
// instead of repeating them inside all Profile implementations.
content::PermissionManager* OffTheRecordProfileImpl::GetPermissionManager() {
  return PermissionManagerFactory::GetForProfile(this);
}

bool OffTheRecordProfileImpl::IsSameProfile(Profile* profile) {
  return (profile == this) || (profile == profile_);
}

Time OffTheRecordProfileImpl::GetStartTime() const {
  return start_time_;
}

void OffTheRecordProfileImpl::SetExitType(ExitType exit_type) {
}

base::FilePath OffTheRecordProfileImpl::last_selected_directory() {
  const base::FilePath& directory = last_selected_directory_;
  if (directory.empty()) {
    return profile_->last_selected_directory();
  }
  return directory;
}

void OffTheRecordProfileImpl::set_last_selected_directory(
    const base::FilePath& path) {
  last_selected_directory_ = path;
}

bool OffTheRecordProfileImpl::WasCreatedByVersionOrLater(
    const std::string& version) {
  return profile_->WasCreatedByVersionOrLater(version);
}

Profile::ExitType OffTheRecordProfileImpl::GetLastSessionExitType() {
  return profile_->GetLastSessionExitType();
}

#if defined(OS_CHROMEOS)
void OffTheRecordProfileImpl::ChangeAppLocale(const std::string& locale,
                                              AppLocaleChangedVia) {
}

void OffTheRecordProfileImpl::OnLogin() {
}

void OffTheRecordProfileImpl::InitChromeOSPreferences() {
  // The incognito profile shouldn't have Chrome OS's preferences.
  // The preferences are associated with the regular user profile.
}
#endif  // defined(OS_CHROMEOS)

PrefProxyConfigTracker* OffTheRecordProfileImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_)
    pref_proxy_config_tracker_.reset(CreateProxyConfigTracker());
  return pref_proxy_config_tracker_.get();
}

chrome_browser_net::Predictor* OffTheRecordProfileImpl::GetNetworkPredictor() {
  // We do not store information about websites visited in OTR profiles which
  // is necessary for a Predictor, so we do not have a Predictor at all.
  return NULL;
}

DevToolsNetworkController*
OffTheRecordProfileImpl::GetDevToolsNetworkController() {
  return io_data_->GetDevToolsNetworkController();
}

void OffTheRecordProfileImpl::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  // Nothing to do here, our transport security state is read-only.
  // Still, fire the callback to indicate we have finished, otherwise the
  // BrowsingDataRemover will never be destroyed and the dialog will never be
  // closed. We must do this asynchronously in order to avoid reentrancy issues.
  if (!completion.is_null()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, completion);
  }
}

GURL OffTheRecordProfileImpl::GetHomePage() {
  return profile_->GetHomePage();
}

#if defined(OS_CHROMEOS)
// Special case of the OffTheRecordProfileImpl which is used while Guest
// session in CrOS.
class GuestSessionProfile : public OffTheRecordProfileImpl {
 public:
  explicit GuestSessionProfile(Profile* real_profile)
      : OffTheRecordProfileImpl(real_profile) {
    set_is_guest_profile(true);
  }

  ProfileType GetProfileType() const override { return GUEST_PROFILE; }

  void InitChromeOSPreferences() override {
    chromeos_preferences_.reset(new chromeos::Preferences());
    chromeos_preferences_->Init(
        this, user_manager::UserManager::Get()->GetActiveUser());
  }

 private:
  // The guest user should be able to customize Chrome OS preferences.
  scoped_ptr<chromeos::Preferences> chromeos_preferences_;
};
#endif

Profile* Profile::CreateOffTheRecordProfile() {
  OffTheRecordProfileImpl* profile = NULL;
#if defined(OS_CHROMEOS)
  if (IsGuestSession())
    profile = new GuestSessionProfile(this);
#endif
  if (!profile)
    profile = new OffTheRecordProfileImpl(this);
  profile->Init();
  return profile;
}

void OffTheRecordProfileImpl::OnParentZoomLevelChanged(
    const HostZoomMap::ZoomLevelChange& change) {
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  switch (change.mode) {
    case HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
      return;
    case HostZoomMap::ZOOM_CHANGED_FOR_HOST:
      host_zoom_map->SetZoomLevelForHost(change.host, change.zoom_level);
      return;
    case HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
      host_zoom_map->SetZoomLevelForHostAndScheme(change.scheme,
          change.host,
          change.zoom_level);
      return;
    case HostZoomMap::PAGE_SCALE_IS_ONE_CHANGED:
      return;
  }
}

void OffTheRecordProfileImpl::UpdateDefaultZoomLevel() {
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  double default_zoom_level =
      profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref();
  host_zoom_map->SetDefaultZoomLevel(default_zoom_level);
}

PrefProxyConfigTracker* OffTheRecordProfileImpl::CreateProxyConfigTracker() {
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(this)) {
    return ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
        g_browser_process->local_state());
  }
#endif  // defined(OS_CHROMEOS)
  return ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
      GetPrefs(), g_browser_process->local_state());
}
