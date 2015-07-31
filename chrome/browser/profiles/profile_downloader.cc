// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_downloader.h"

#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "skia/ext/image_operations.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

// Template for optional authorization header when using an OAuth access token.
const char kAuthorizationHeader[] =
    "Authorization: Bearer %s";

// Path in JSON dictionary to user's photo thumbnail URL.
const char kPhotoThumbnailURLPath[] = "picture";

// Path in JSON dictionary to user's hosted domain.
const char kHostedDomainPath[] = "hd";

// From the user info API, this field corresponds to the full name of the user.
const char kFullNamePath[] = "name";

const char kGivenNamePath[] = "given_name";

// Path in JSON dictionary to user's preferred locale.
const char kLocalePath[] = "locale";

// Path format for specifying thumbnail's size.
const char kThumbnailSizeFormat[] = "s%d-c";
// Default thumbnail size.
const int kDefaultThumbnailSize = 64;

// Separator of URL path components.
const char kURLPathSeparator = '/';

// Photo ID of the Picasa Web Albums profile picture (base64 of 0).
const char kPicasaPhotoId[] = "AAAAAAAAAAA";

// Photo version of the default PWA profile picture (base64 of 1).
const char kDefaultPicasaPhotoVersion[] = "AAAAAAAAAAE";

// The minimum number of path components in profile picture URL.
const size_t kProfileImageURLPathComponentsCount = 6;

// Index of path component with photo ID.
const int kPhotoIdPathComponentIndex = 2;

// Index of path component with photo version.
const int kPhotoVersionPathComponentIndex = 3;

// Given an image URL this function builds a new URL set to |size|.
// For example, if |size| was set to 256 and |old_url| was either:
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg
//   or
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s64-c/photo.jpg
// then return value in |new_url| would be:
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s256-c/photo.jpg
bool GetImageURLWithSize(const GURL& old_url, int size, GURL* new_url) {
  DCHECK(new_url);
  std::vector<std::string> components;
  base::SplitString(old_url.path(), kURLPathSeparator, &components);
  if (components.size() == 0)
    return false;

  const std::string& old_spec = old_url.spec();
  std::string default_size_component(
      base::StringPrintf(kThumbnailSizeFormat, kDefaultThumbnailSize));
  std::string new_size_component(
      base::StringPrintf(kThumbnailSizeFormat, size));

  size_t pos = old_spec.find(default_size_component);
  size_t end = std::string::npos;
  if (pos != std::string::npos) {
    // The default size is already specified in the URL so it needs to be
    // replaced with the new size.
    end = pos + default_size_component.size();
  } else {
    // The default size is not in the URL so try to insert it before the last
    // component.
    const std::string& file_name = old_url.ExtractFileName();
    if (!file_name.empty()) {
      pos = old_spec.find(file_name);
      end = pos - 1;
    }
  }

  if (pos != std::string::npos) {
    std::string new_spec = old_spec.substr(0, pos) + new_size_component +
                           old_spec.substr(end);
    *new_url = GURL(new_spec);
    return new_url->is_valid();
  }

  // We can't set the image size, just use the default size.
  *new_url = old_url;
  return true;
}

}  // namespace

// Parses the entry response and gets the name and profile image URL.
// |data| should be the JSON formatted data return by the response.
// Returns false to indicate a parsing error.
bool ProfileDownloader::ParseProfileJSON(base::DictionaryValue* root_dictionary,
                                         base::string16* full_name,
                                         base::string16* given_name,
                                         std::string* url,
                                         int image_size,
                                         std::string* profile_locale,
                                         base::string16* hosted_domain) {
  DCHECK(full_name);
  DCHECK(given_name);
  DCHECK(url);
  DCHECK(profile_locale);
  DCHECK(hosted_domain);

  *full_name = base::string16();
  *given_name = base::string16();
  *url = std::string();
  *profile_locale = std::string();
  *hosted_domain = base::string16();

  root_dictionary->GetString(kFullNamePath, full_name);
  root_dictionary->GetString(kGivenNamePath, given_name);
  root_dictionary->GetString(kLocalePath, profile_locale);
  root_dictionary->GetString(kHostedDomainPath, hosted_domain);

  std::string url_string;
  if (root_dictionary->GetString(kPhotoThumbnailURLPath, &url_string)) {
    GURL new_url;
    if (!GetImageURLWithSize(GURL(url_string), image_size, &new_url)) {
      LOG(ERROR) << "GetImageURLWithSize failed for url: " << url_string;
      return false;
    }
    *url = new_url.spec();
  }

  // The profile data is considered valid as long as it has a name or a picture.
  return !full_name->empty() || !url->empty();
}

// static
bool ProfileDownloader::IsDefaultProfileImageURL(const std::string& url) {
  if (url.empty())
    return true;

  GURL image_url_object(url);
  DCHECK(image_url_object.is_valid());
  VLOG(1) << "URL to check for default image: " << image_url_object.spec();
  std::vector<std::string> path_components;
  base::SplitString(image_url_object.path(),
                    kURLPathSeparator,
                    &path_components);

  if (path_components.size() < kProfileImageURLPathComponentsCount)
    return false;

  const std::string& photo_id = path_components[kPhotoIdPathComponentIndex];
  const std::string& photo_version =
      path_components[kPhotoVersionPathComponentIndex];

  // Check that the ID and version match the default Picasa profile photo.
  return photo_id == kPicasaPhotoId &&
         photo_version == kDefaultPicasaPhotoVersion;
}

ProfileDownloader::ProfileDownloader(ProfileDownloaderDelegate* delegate)
    : OAuth2TokenService::Consumer("profile_downloader"),
      delegate_(delegate),
      picture_status_(PICTURE_FAILED) {
  DCHECK(delegate_);
}

void ProfileDownloader::Start() {
  StartForAccount(std::string());
}

void ProfileDownloader::StartForAccount(const std::string& account_id) {
  VLOG(1) << "Starting profile downloader...";
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(
          delegate_->GetBrowserProfile());
  if (!service) {
    // This can happen in some test paths.
    LOG(WARNING) << "User has no token service";
    delegate_->OnProfileDownloadFailure(
        this, ProfileDownloaderDelegate::TOKEN_ERROR);
    return;
  }

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(delegate_->GetBrowserProfile());
  account_id_ =
      account_id.empty() ?
          signin_manager->GetAuthenticatedAccountId() : account_id;
  if (service->RefreshTokenIsAvailable(account_id_)) {
    StartFetchingOAuth2AccessToken();
  } else {
    service->AddObserver(this);
  }
}

base::string16 ProfileDownloader::GetProfileHostedDomain() const {
  return profile_hosted_domain_;
}

base::string16 ProfileDownloader::GetProfileFullName() const {
  return profile_full_name_;
}

base::string16 ProfileDownloader::GetProfileGivenName() const {
  return profile_given_name_;
}

std::string ProfileDownloader::GetProfileLocale() const {
  return profile_locale_;
}

SkBitmap ProfileDownloader::GetProfilePicture() const {
  return profile_picture_;
}

ProfileDownloader::PictureStatus ProfileDownloader::GetProfilePictureStatus()
    const {
  return picture_status_;
}

std::string ProfileDownloader::GetProfilePictureURL() const {
  return picture_url_;
}

void ProfileDownloader::StartFetchingImage() {
  VLOG(1) << "Fetching user entry with token: " << auth_token_;
  gaia_client_.reset(new gaia::GaiaOAuthClient(
      delegate_->GetBrowserProfile()->GetRequestContext()));
  gaia_client_->GetUserInfo(auth_token_, 0, this);
}

void ProfileDownloader::StartFetchingOAuth2AccessToken() {
  Profile* profile = delegate_->GetBrowserProfile();
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleUserInfoProfile);
  // Increase scope to get hd attribute to determine if lock should be enabled.
  if (switches::IsNewProfileManagement())
    scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  oauth2_access_token_request_ = token_service->StartRequest(
      account_id_, scopes, this);
}

ProfileDownloader::~ProfileDownloader() {
  // Ensures PO2TS observation is cleared when ProfileDownloader is destructed
  // before refresh token is available.
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(
          delegate_->GetBrowserProfile());
  if (service)
    service->RemoveObserver(this);
}

void ProfileDownloader::OnGetUserInfoResponse(
    scoped_ptr<base::DictionaryValue> user_info) {
  std::string image_url;
  if (!ParseProfileJSON(user_info.get(),
                        &profile_full_name_,
                        &profile_given_name_,
                        &image_url,
                        delegate_->GetDesiredImageSideLength(),
                        &profile_locale_,
                        &profile_hosted_domain_)) {
    delegate_->OnProfileDownloadFailure(
        this, ProfileDownloaderDelegate::SERVICE_ERROR);
    return;
  }
  if (!delegate_->NeedsProfilePicture()) {
    VLOG(1) << "Skipping profile picture download";
    delegate_->OnProfileDownloadSuccess(this);
    return;
  }
  if (IsDefaultProfileImageURL(image_url)) {
    VLOG(1) << "User has default profile picture";
    picture_status_ = PICTURE_DEFAULT;
    delegate_->OnProfileDownloadSuccess(this);
    return;
  }
  if (!image_url.empty() && image_url == delegate_->GetCachedPictureURL()) {
    VLOG(1) << "Picture URL matches cached picture URL";
    picture_status_ = PICTURE_CACHED;
    delegate_->OnProfileDownloadSuccess(this);
    return;
  }
  VLOG(1) << "Fetching profile image from " << image_url;
  picture_url_ = image_url;
  profile_image_fetcher_ =
      net::URLFetcher::Create(GURL(image_url), net::URLFetcher::GET, this);
  profile_image_fetcher_->SetRequestContext(
      delegate_->GetBrowserProfile()->GetRequestContext());
  profile_image_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                       net::LOAD_DO_NOT_SAVE_COOKIES);
  if (!auth_token_.empty()) {
    profile_image_fetcher_->SetExtraRequestHeaders(
        base::StringPrintf(kAuthorizationHeader, auth_token_.c_str()));
  }
  profile_image_fetcher_->Start();
}

void ProfileDownloader::OnOAuthError() {
  LOG(WARNING) << "OnOAuthError: Fetching profile data failed";
  delegate_->OnProfileDownloadFailure(
      this, ProfileDownloaderDelegate::SERVICE_ERROR);
}

void ProfileDownloader::OnNetworkError(int response_code) {
  LOG(WARNING) << "OnNetworkError: Fetching profile data failed";
  DVLOG(1) << "  Response code: " << response_code;
  delegate_->OnProfileDownloadFailure(
      this, ProfileDownloaderDelegate::NETWORK_ERROR);
}

void ProfileDownloader::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string data;
  source->GetResponseAsString(&data);
  bool network_error =
      source->GetStatus().status() != net::URLRequestStatus::SUCCESS;
  if (network_error || source->GetResponseCode() != 200) {
    LOG(WARNING) << "Fetching profile data failed";
    DVLOG(1) << "  Status: " << source->GetStatus().status();
    DVLOG(1) << "  Error: " << source->GetStatus().error();
    DVLOG(1) << "  Response code: " << source->GetResponseCode();
    DVLOG(1) << "  Url: " << source->GetURL().spec();
    delegate_->OnProfileDownloadFailure(this, network_error ?
        ProfileDownloaderDelegate::NETWORK_ERROR :
        ProfileDownloaderDelegate::SERVICE_ERROR);
    return;
  }

  VLOG(1) << "Decoding the image...";
  ImageDecoder::Start(this, data);
}

void ProfileDownloader::OnImageDecoded(const SkBitmap& decoded_image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int image_size = delegate_->GetDesiredImageSideLength();
  profile_picture_ = skia::ImageOperations::Resize(
      decoded_image,
      skia::ImageOperations::RESIZE_BEST,
      image_size,
      image_size);
  picture_status_ = PICTURE_SUCCESS;
  delegate_->OnProfileDownloadSuccess(this);
}

void ProfileDownloader::OnDecodeImageFailed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnProfileDownloadFailure(
      this, ProfileDownloaderDelegate::IMAGE_DECODE_FAILED);
}

void ProfileDownloader::OnRefreshTokenAvailable(const std::string& account_id) {
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(
          delegate_->GetBrowserProfile());
  if (account_id != account_id_)
    return;

  service->RemoveObserver(this);
  StartFetchingOAuth2AccessToken();
}

// Callback for OAuth2TokenService::Request on success. |access_token| is the
// token used to start fetching user data.
void ProfileDownloader::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(request, oauth2_access_token_request_.get());
  oauth2_access_token_request_.reset();
  auth_token_ = access_token;
  StartFetchingImage();
}

// Callback for OAuth2TokenService::Request on failure.
void ProfileDownloader::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(request, oauth2_access_token_request_.get());
  oauth2_access_token_request_.reset();
  LOG(WARNING) << "ProfileDownloader: token request using refresh token failed:"
               << error.ToString();
  delegate_->OnProfileDownloadFailure(
      this, ProfileDownloaderDelegate::TOKEN_ERROR);
}
