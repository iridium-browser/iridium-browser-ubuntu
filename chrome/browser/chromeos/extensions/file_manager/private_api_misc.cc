// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_misc.h"

#include <set>
#include <vector>

#include "ash/frame/frame_util.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/file_manager/zip_file_creator.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/drive/event_logger.h"
#include "chrome/browser/extensions/api/file_handlers/mime_util.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "google_apis/drive/auth_service.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace extensions {

namespace {
const char kCWSScope[] = "https://www.googleapis.com/auth/chromewebstore";

// Obtains the current app window.
AppWindow* GetCurrentAppWindow(ChromeSyncExtensionFunction* function) {
  AppWindowRegistry* const app_window_registry =
      AppWindowRegistry::Get(function->GetProfile());
  content::WebContents* const contents = function->GetAssociatedWebContents();
  content::RenderViewHost* const render_view_host =
      contents ? contents->GetRenderViewHost() : NULL;
  return render_view_host ? app_window_registry->GetAppWindowForRenderViewHost(
                                render_view_host)
                          : NULL;
}

std::vector<linked_ptr<api::file_manager_private::ProfileInfo> >
GetLoggedInProfileInfoList() {
  DCHECK(user_manager::UserManager::IsInitialized());
  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::set<Profile*> original_profiles;
  std::vector<linked_ptr<api::file_manager_private::ProfileInfo> >
      result_profiles;

  for (size_t i = 0; i < profiles.size(); ++i) {
    // Filter the profile.
    Profile* const profile = profiles[i]->GetOriginalProfile();
    if (original_profiles.count(profile))
      continue;
    original_profiles.insert(profile);
    const user_manager::User* const user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    if (!user || !user->is_logged_in())
      continue;

    // Make a ProfileInfo.
    linked_ptr<api::file_manager_private::ProfileInfo> profile_info(
        new api::file_manager_private::ProfileInfo());
    profile_info->profile_id = multi_user_util::GetUserIDFromProfile(profile);
    profile_info->display_name = UTF16ToUTF8(user->GetDisplayName());
    // TODO(hirono): Remove the property from the profile_info.
    profile_info->is_current_profile = true;

    result_profiles.push_back(profile_info);
  }

  return result_profiles;
}
}  // namespace

bool FileManagerPrivateLogoutUserForReauthenticationFunction::RunSync() {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(GetProfile());
  if (user) {
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        user->email(), user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
  }

  chrome::AttemptUserExit();
  return true;
}

bool FileManagerPrivateGetPreferencesFunction::RunSync() {
  api::file_manager_private::Preferences result;
  const PrefService* const service = GetProfile()->GetPrefs();

  result.drive_enabled = drive::util::IsDriveEnabledForProfile(GetProfile());
  result.cellular_disabled =
      service->GetBoolean(prefs::kDisableDriveOverCellular);
  result.hosted_files_disabled =
      service->GetBoolean(prefs::kDisableDriveHostedFiles);
  result.use24hour_clock = service->GetBoolean(prefs::kUse24HourClock);
  result.allow_redeem_offers = true;
  if (!chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kAllowRedeemChromeOsRegistrationOffers,
          &result.allow_redeem_offers)) {
    result.allow_redeem_offers = true;
  }

  SetResult(result.ToValue().release());

  drive::EventLogger* logger = file_manager::util::GetLogger(GetProfile());
  if (logger)
    logger->Log(logging::LOG_INFO, "%s succeeded.", name());
  return true;
}

bool FileManagerPrivateSetPreferencesFunction::RunSync() {
  using extensions::api::file_manager_private::SetPreferences::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  PrefService* const service = GetProfile()->GetPrefs();

  if (params->change_info.cellular_disabled)
    service->SetBoolean(prefs::kDisableDriveOverCellular,
                        *params->change_info.cellular_disabled);

  if (params->change_info.hosted_files_disabled)
    service->SetBoolean(prefs::kDisableDriveHostedFiles,
                        *params->change_info.hosted_files_disabled);

  drive::EventLogger* logger = file_manager::util::GetLogger(GetProfile());
  if (logger)
    logger->Log(logging::LOG_INFO, "%s succeeded.", name());
  return true;
}

FileManagerPrivateZipSelectionFunction::
    FileManagerPrivateZipSelectionFunction() {}

FileManagerPrivateZipSelectionFunction::
    ~FileManagerPrivateZipSelectionFunction() {}

bool FileManagerPrivateZipSelectionFunction::RunAsync() {
  using extensions::api::file_manager_private::ZipSelection::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // First param is the source directory URL.
  if (params->dir_url.empty())
    return false;

  base::FilePath src_dir = file_manager::util::GetLocalPathFromURL(
      render_view_host(), GetProfile(), GURL(params->dir_url));
  if (src_dir.empty())
    return false;

  // Second param is the list of selected file URLs.
  if (params->selection_urls.empty())
    return false;

  std::vector<base::FilePath> files;
  for (size_t i = 0; i < params->selection_urls.size(); ++i) {
    base::FilePath path = file_manager::util::GetLocalPathFromURL(
        render_view_host(), GetProfile(), GURL(params->selection_urls[i]));
    if (path.empty())
      return false;
    files.push_back(path);
  }

  // Third param is the name of the output zip file.
  if (params->dest_name.empty())
    return false;

  // Check if the dir path is under Drive mount point.
  // TODO(hshi): support create zip file on Drive (crbug.com/158690).
  if (drive::util::IsUnderDriveMountPoint(src_dir))
    return false;

  base::FilePath dest_file = src_dir.Append(params->dest_name);
  std::vector<base::FilePath> src_relative_paths;
  for (size_t i = 0; i != files.size(); ++i) {
    const base::FilePath& file_path = files[i];

    // Obtain the relative path of |file_path| under |src_dir|.
    base::FilePath relative_path;
    if (!src_dir.AppendRelativePath(file_path, &relative_path))
      return false;
    src_relative_paths.push_back(relative_path);
  }

  (new file_manager::ZipFileCreator(
       base::Bind(&FileManagerPrivateZipSelectionFunction::OnZipDone, this),
       src_dir,
       src_relative_paths,
       dest_file))->Start();
  return true;
}

void FileManagerPrivateZipSelectionFunction::OnZipDone(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
}

bool FileManagerPrivateZoomFunction::RunSync() {
  using extensions::api::file_manager_private::Zoom::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  content::PageZoom zoom_type;
  switch (params->operation) {
    case api::file_manager_private::ZOOM_OPERATION_TYPE_IN:
      zoom_type = content::PAGE_ZOOM_IN;
      break;
    case api::file_manager_private::ZOOM_OPERATION_TYPE_OUT:
      zoom_type = content::PAGE_ZOOM_OUT;
      break;
    case api::file_manager_private::ZOOM_OPERATION_TYPE_RESET:
      zoom_type = content::PAGE_ZOOM_RESET;
      break;
    default:
      NOTREACHED();
      return false;
  }
  render_view_host()->Zoom(zoom_type);
  return true;
}

bool FileManagerPrivateInstallWebstoreItemFunction::RunAsync() {
  using extensions::api::file_manager_private::InstallWebstoreItem::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  SetError("Deleted, use chrome.webstoreWidgetPrivate API instead.");
  return false;
}

FileManagerPrivateRequestWebStoreAccessTokenFunction::
    FileManagerPrivateRequestWebStoreAccessTokenFunction() {
}

FileManagerPrivateRequestWebStoreAccessTokenFunction::
    ~FileManagerPrivateRequestWebStoreAccessTokenFunction() {
}

bool FileManagerPrivateRequestWebStoreAccessTokenFunction::RunAsync() {
  std::vector<std::string> scopes;
  scopes.push_back(kCWSScope);

  ProfileOAuth2TokenService* oauth_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(GetProfile());
  net::URLRequestContextGetter* url_request_context_getter =
      g_browser_process->system_request_context();

  if (!oauth_service) {
    drive::EventLogger* logger = file_manager::util::GetLogger(GetProfile());
    if (logger) {
      logger->Log(logging::LOG_ERROR,
                  "CWS OAuth token fetch failed. OAuth2TokenService can't "
                  "be retrieved.");
    }
    SetResult(base::Value::CreateNullValue());
    return false;
  }

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(GetProfile());
  auth_service_.reset(new google_apis::AuthService(
      oauth_service,
      signin_manager->GetAuthenticatedAccountId(),
      url_request_context_getter,
      scopes));
  auth_service_->StartAuthentication(base::Bind(
      &FileManagerPrivateRequestWebStoreAccessTokenFunction::
          OnAccessTokenFetched,
      this));

  return true;
}

void FileManagerPrivateRequestWebStoreAccessTokenFunction::OnAccessTokenFetched(
    google_apis::DriveApiErrorCode code,
    const std::string& access_token) {
  drive::EventLogger* logger = file_manager::util::GetLogger(GetProfile());

  if (code == google_apis::HTTP_SUCCESS) {
    DCHECK(auth_service_->HasAccessToken());
    DCHECK(access_token == auth_service_->access_token());
    if (logger)
      logger->Log(logging::LOG_INFO, "CWS OAuth token fetch succeeded.");
    SetResult(new base::StringValue(access_token));
    SendResponse(true);
  } else {
    if (logger) {
      logger->Log(logging::LOG_ERROR,
                  "CWS OAuth token fetch failed. (DriveApiErrorCode: %s)",
                  google_apis::DriveApiErrorCodeToString(code).c_str());
    }
    SetResult(base::Value::CreateNullValue());
    SendResponse(false);
  }
}

bool FileManagerPrivateGetProfilesFunction::RunSync() {
  const std::vector<linked_ptr<api::file_manager_private::ProfileInfo> >&
      profiles = GetLoggedInProfileInfoList();

  // Obtains the display profile ID.
  AppWindow* const app_window = GetCurrentAppWindow(this);
  chrome::MultiUserWindowManager* const window_manager =
      chrome::MultiUserWindowManager::GetInstance();
  const std::string current_profile_id =
      multi_user_util::GetUserIDFromProfile(GetProfile());
  const std::string display_profile_id =
      window_manager && app_window ? window_manager->GetUserPresentingWindow(
                                         app_window->GetNativeWindow())
                                   : "";

  results_ = api::file_manager_private::GetProfiles::Results::Create(
      profiles,
      current_profile_id,
      display_profile_id.empty() ? current_profile_id : display_profile_id);
  return true;
}

bool FileManagerPrivateOpenInspectorFunction::RunSync() {
  using extensions::api::file_manager_private::OpenInspector::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  switch (params->type) {
    case extensions::api::file_manager_private::INSPECTION_TYPE_NORMAL:
      // Open inspector for foreground page.
      DevToolsWindow::OpenDevToolsWindow(
          content::WebContents::FromRenderViewHost(render_view_host()));
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_CONSOLE:
      // Open inspector for foreground page and bring focus to the console.
      DevToolsWindow::OpenDevToolsWindow(
          content::WebContents::FromRenderViewHost(render_view_host()),
          DevToolsToggleAction::ShowConsole());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_ELEMENT:
      // Open inspector for foreground page in inspect element mode.
      DevToolsWindow::OpenDevToolsWindow(
          content::WebContents::FromRenderViewHost(render_view_host()),
          DevToolsToggleAction::Inspect());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_BACKGROUND:
      // Open inspector for background page.
      extensions::devtools_util::InspectBackgroundPage(extension(),
                                                       GetProfile());
      break;
    default:
      NOTREACHED();
      SetError(
          base::StringPrintf("Unexpected inspection type(%d) is specified.",
                             static_cast<int>(params->type)));
      return false;
  }
  return true;
}

FileManagerPrivateGetMimeTypeFunction::FileManagerPrivateGetMimeTypeFunction() {
}

FileManagerPrivateGetMimeTypeFunction::
    ~FileManagerPrivateGetMimeTypeFunction() {
}

bool FileManagerPrivateGetMimeTypeFunction::RunAsync() {
  using extensions::api::file_manager_private::GetMimeType::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convert file url to local path.
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  const GURL file_url(params->file_url);
  storage::FileSystemURL file_system_url(
      file_system_context->CrackURL(file_url));

  app_file_handler_util::GetMimeTypeForLocalPath(
      GetProfile(), file_system_url.path(),
      base::Bind(&FileManagerPrivateGetMimeTypeFunction::OnGetMimeType, this));

  return true;
}

void FileManagerPrivateGetMimeTypeFunction::OnGetMimeType(
    const std::string& mimeType) {
  SetResult(new base::StringValue(mimeType));
  SendResponse(true);
}

ExtensionFunction::ResponseAction
FileManagerPrivateIsPiexLoaderEnabledFunction::Run() {
#if defined(OFFICIAL_BUILD)
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
#else
  return RespondNow(OneArgument(new base::FundamentalValue(false)));
#endif
}

FileManagerPrivateGetProvidingExtensionsFunction::
    FileManagerPrivateGetProvidingExtensionsFunction()
    : chrome_details_(this) {
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetProvidingExtensionsFunction::Run() {
  using chromeos::file_system_provider::Service;
  using chromeos::file_system_provider::ProvidingExtensionInfo;
  const Service* const service = Service::Get(chrome_details_.GetProfile());
  const std::vector<ProvidingExtensionInfo> info_list =
      service->GetProvidingExtensionInfoList();

  using api::file_manager_private::ProvidingExtension;
  std::vector<linked_ptr<ProvidingExtension>> providing_extensions;
  for (const auto& info : info_list) {
    const linked_ptr<ProvidingExtension> providing_extension(
        new ProvidingExtension);
    providing_extension->extension_id = info.extension_id;
    providing_extension->name = info.name;
    providing_extension->configurable = info.capabilities.configurable();
    providing_extension->multiple_mounts = info.capabilities.multiple_mounts();
    switch (info.capabilities.source()) {
      case SOURCE_FILE:
        providing_extension->source =
            api::manifest_types::FILE_SYSTEM_PROVIDER_SOURCE_FILE;
        break;
      case SOURCE_DEVICE:
        providing_extension->source =
            api::manifest_types::FILE_SYSTEM_PROVIDER_SOURCE_DEVICE;
        break;
      case SOURCE_NETWORK:
        providing_extension->source =
            api::manifest_types::FILE_SYSTEM_PROVIDER_SOURCE_NETWORK;
        break;
    }
    providing_extensions.push_back(providing_extension);
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetProvidingExtensions::Results::Create(
          providing_extensions).Pass()));
}

FileManagerPrivateAddProvidedFileSystemFunction::
    FileManagerPrivateAddProvidedFileSystemFunction()
    : chrome_details_(this) {
}

ExtensionFunction::ResponseAction
FileManagerPrivateAddProvidedFileSystemFunction::Run() {
  using extensions::api::file_manager_private::AddProvidedFileSystem::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using chromeos::file_system_provider::Service;
  using chromeos::file_system_provider::ProvidingExtensionInfo;
  Service* const service = Service::Get(chrome_details_.GetProfile());

  if (!service->RequestMount(params->extension_id))
    return RespondNow(Error("Failed to request a new mount."));

  return RespondNow(NoArguments());
}

FileManagerPrivateConfigureVolumeFunction::
    FileManagerPrivateConfigureVolumeFunction()
    : chrome_details_(this) {
}

ExtensionFunction::ResponseAction
FileManagerPrivateConfigureVolumeFunction::Run() {
  using extensions::api::file_manager_private::ConfigureVolume::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::VolumeManager;
  using file_manager::Volume;
  VolumeManager* const volume_manager =
      VolumeManager::Get(chrome_details_.GetProfile());
  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume.get())
    return RespondNow(Error("Volume not found."));
  if (!volume->configurable())
    return RespondNow(Error("Volume not configurable."));

  switch (volume->type()) {
    case file_manager::VOLUME_TYPE_PROVIDED: {
      using chromeos::file_system_provider::Service;
      Service* const service = Service::Get(chrome_details_.GetProfile());
      DCHECK(service);

      using chromeos::file_system_provider::ProvidedFileSystemInterface;
      ProvidedFileSystemInterface* const file_system =
          service->GetProvidedFileSystem(volume->extension_id(),
                                         volume->file_system_id());
      if (file_system)
        file_system->Configure(base::Bind(
            &FileManagerPrivateConfigureVolumeFunction::OnCompleted, this));
      break;
    }
    default:
      NOTIMPLEMENTED();
  }

  return RespondLater();
}

void FileManagerPrivateConfigureVolumeFunction::OnCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to complete configuration."));
    return;
  }

  Respond(NoArguments());
}

}  // namespace extensions
