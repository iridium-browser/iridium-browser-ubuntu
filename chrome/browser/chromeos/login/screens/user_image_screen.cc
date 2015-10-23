// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/user_image_screen.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/camera_presence_notifier.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/user_image_view.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/default_user_images.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "policy/policy_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Time histogram suffix for profile image download.
const char kProfileDownloadReason[] = "OOBE";

// Maximum amount of time to wait for the user image to sync.
// The screen is shown iff sync failed or time limit exceeded.
const int kSyncTimeoutSeconds = 10;

}  // namespace

// static
UserImageScreen* UserImageScreen::Get(ScreenManager* manager) {
  return static_cast<UserImageScreen*>(
      manager->GetScreen(WizardController::kUserImageScreenName));
}

UserImageScreen::UserImageScreen(BaseScreenDelegate* base_screen_delegate,
                                 UserImageView* view)
    : UserImageModel(base_screen_delegate),
      view_(view),
      accept_photo_after_decoding_(false),
      selected_image_(user_manager::User::USER_IMAGE_INVALID),
      is_screen_ready_(false),
      user_has_selected_image_(false) {
  if (view_)
    view_->Bind(*this);
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                              content::NotificationService::AllSources());
  GetContextEditor().SetString(kContextKeyProfilePictureDataURL, std::string());
}

UserImageScreen::~UserImageScreen() {
  CameraPresenceNotifier::GetInstance()->RemoveObserver(this);
  if (view_)
    view_->Unbind();
}

void UserImageScreen::OnScreenReady() {
  is_screen_ready_ = true;
  if (!IsWaitingForSync())
    HideCurtain();
}

void UserImageScreen::OnPhotoTaken(const std::string& raw_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  user_photo_ = gfx::ImageSkia();
  ImageDecoder::Cancel(this);
  ImageDecoder::Start(this, raw_data);
}

void UserImageScreen::OnCameraPresenceCheckDone(bool is_camera_present) {
  GetContextEditor().SetBoolean(kContextKeyIsCameraPresent, is_camera_present);
}

void UserImageScreen::HideCurtain() {
  // Skip user image selection for ephemeral users.
  if (user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          GetUser()->GetUserID())) {
    ExitScreen();
  }
  if (view_)
    view_->HideCurtain();
}

void UserImageScreen::OnImageDecoded(const SkBitmap& decoded_image) {
  user_photo_ = gfx::ImageSkia::CreateFrom1xBitmap(decoded_image);
  if (accept_photo_after_decoding_)
    OnImageAccepted();
}

void UserImageScreen::OnDecodeImageFailed() {
  NOTREACHED() << "Failed to decode PNG image from WebUI";
}

void UserImageScreen::OnInitialSync(bool local_image_updated) {
  DCHECK(sync_timer_);
  ReportSyncResult(SyncResult::SUCCEEDED);
  if (!local_image_updated) {
    sync_timer_.reset();
    GetSyncObserver()->RemoveObserver(this);
    if (is_screen_ready_)
      HideCurtain();
    return;
  }
  ExitScreen();
}

void UserImageScreen::OnSyncTimeout() {
  ReportSyncResult(SyncResult::TIMED_OUT);
  sync_timer_.reset();
  GetSyncObserver()->RemoveObserver(this);
  if (is_screen_ready_)
    HideCurtain();
}

bool UserImageScreen::IsWaitingForSync() const {
  return sync_timer_.get() && sync_timer_->IsRunning();
}

void UserImageScreen::OnUserImagePolicyChanged(const base::Value* previous,
                                               const base::Value* current) {
  if (current) {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, policy_registrar_.release());
    ExitScreen();
  }
}

void UserImageScreen::OnImageSelected(const std::string& image_type,
                                      const std::string& image_url,
                                      bool is_user_selection) {
  if (is_user_selection) {
    user_has_selected_image_ = true;
  }
  if (image_url.empty())
    return;
  int user_image_index = user_manager::User::USER_IMAGE_INVALID;
  if (image_type == "default" &&
      user_manager::IsDefaultImageUrl(image_url, &user_image_index)) {
    selected_image_ = user_image_index;
  } else if (image_type == "camera") {
    selected_image_ = user_manager::User::USER_IMAGE_EXTERNAL;
  } else if (image_type == "profile") {
    selected_image_ = user_manager::User::USER_IMAGE_PROFILE;
  } else {
    NOTREACHED() << "Unexpected image type: " << image_type;
  }
}

void UserImageScreen::OnImageAccepted() {
  UserImageManager* image_manager = GetUserImageManager();
  int uma_index = 0;
  switch (selected_image_) {
    case user_manager::User::USER_IMAGE_EXTERNAL:
      // Photo decoding may not have been finished yet.
      if (user_photo_.isNull()) {
        accept_photo_after_decoding_ = true;
        return;
      }
      image_manager->SaveUserImage(
          user_manager::UserImage::CreateAndEncode(user_photo_));
      uma_index = user_manager::kHistogramImageFromCamera;
      break;
    case user_manager::User::USER_IMAGE_PROFILE:
      image_manager->SaveUserImageFromProfileImage();
      uma_index = user_manager::kHistogramImageFromProfile;
      break;
    default:
      DCHECK(selected_image_ >= 0 &&
             selected_image_ < user_manager::kDefaultImagesCount);
      image_manager->SaveUserDefaultImageIndex(selected_image_);
      uma_index = user_manager::GetDefaultImageHistogramValue(selected_image_);
      break;
  }
  if (user_has_selected_image_) {
    UMA_HISTOGRAM_ENUMERATION("UserImage.FirstTimeChoice",
                              uma_index,
                              user_manager::kHistogramImagesCount);
  }
  ExitScreen();
}


void UserImageScreen::PrepareToShow() {
  if (view_)
    view_->PrepareToShow();
}

const user_manager::User* UserImageScreen::GetUser() {
  return user_manager::UserManager::Get()->GetLoggedInUser();
}

UserImageManager* UserImageScreen::GetUserImageManager() {
  return ChromeUserManager::Get()->GetUserImageManager(GetUser()->email());
}

UserImageSyncObserver* UserImageScreen::GetSyncObserver() {
  return GetUserImageManager()->GetSyncObserver();
}

void UserImageScreen::Show() {
  if (!view_)
    return;

  DCHECK(!policy_registrar_);
  if (Profile* profile = ProfileHelper::Get()->GetProfileByUser(GetUser())) {
    policy::PolicyService* policy_service =
        policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile)
            ->policy_service();
    if (policy_service->GetPolicies(
            policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                    std::string()))
            .Get(policy::key::kUserAvatarImage)) {
      // If the user image is managed by policy, skip the screen because the
      // user is not allowed to override a policy-set image.
      ExitScreen();
      return;
    }

    // Listen for policy changes. If at any point, the user image becomes
    // managed by policy, the screen will close.
    policy_registrar_.reset(new policy::PolicyChangeRegistrar(
        policy_service,
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string())));
    policy_registrar_->Observe(
        policy::key::kUserAvatarImage,
        base::Bind(&UserImageScreen::OnUserImagePolicyChanged,
                   base::Unretained(this)));
  } else {
    NOTREACHED();
  }

  if (GetUser()->CanSyncImage()) {
    if (UserImageSyncObserver* sync_observer = GetSyncObserver()) {
      sync_waiting_start_time_ = base::Time::Now();
      // We have synced image already.
      if (sync_observer->is_synced()) {
        ReportSyncResult(SyncResult::SUCCEEDED);
        ExitScreen();
        return;
      }
      sync_observer->AddObserver(this);
      sync_timer_.reset(new base::Timer(
            FROM_HERE,
            base::TimeDelta::FromSeconds(kSyncTimeoutSeconds),
            base::Bind(&UserImageScreen::OnSyncTimeout, base::Unretained(this)),
            false));
      sync_timer_->Reset();
    }
  }
  CameraPresenceNotifier::GetInstance()->AddObserver(this);
  view_->Show();

  selected_image_ = GetUser()->image_index();
  GetContextEditor().SetString(
      kContextKeySelectedImageURL,
      user_manager::GetDefaultImageUrl(selected_image_));

  // Start fetching the profile image.
  GetUserImageManager()->DownloadProfileImage(kProfileDownloadReason);
}

void UserImageScreen::Hide() {
  CameraPresenceNotifier::GetInstance()->RemoveObserver(this);
  notification_registrar_.RemoveAll();
  policy_registrar_.reset();
  sync_timer_.reset();
  if (UserImageSyncObserver* sync_observer = GetSyncObserver())
    sync_observer->RemoveObserver(this);
  if (view_)
    view_->Hide();
}

void UserImageScreen::OnViewDestroyed(UserImageView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void UserImageScreen::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_IMAGE_UPDATED: {
      // We've got a new profile image.
      GetContextEditor().SetString(
          kContextKeyProfilePictureDataURL,
          webui::GetBitmapDataUrl(
              *content::Details<const gfx::ImageSkia>(details)
                   .ptr()
                   ->bitmap()));
      break;
    }
    case chrome::NOTIFICATION_PROFILE_IMAGE_UPDATE_FAILED: {
      // User has a default profile image or fetching profile image has failed.
      GetContextEditor().SetString(kContextKeyProfilePictureDataURL,
                                   std::string());
      break;
    }
    case chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED: {
      GetContextEditor().SetString(
          kContextKeySelectedImageURL,
          user_manager::GetDefaultImageUrl(GetUser()->image_index()));
      break;
    }
    default:
      NOTREACHED();
  }
}

void UserImageScreen::ExitScreen() {
  policy_registrar_.reset();
  sync_timer_.reset();
  if (UserImageSyncObserver* sync_observer = GetSyncObserver())
    sync_observer->RemoveObserver(this);
  Finish(BaseScreenDelegate::USER_IMAGE_SELECTED);
}

void UserImageScreen::ReportSyncResult(SyncResult timed_out) const {
  base::TimeDelta duration = base::Time::Now() - sync_waiting_start_time_;
  UMA_HISTOGRAM_TIMES("Login.NewUserPriorityPrefsSyncTime", duration);
  UMA_HISTOGRAM_ENUMERATION("Login.NewUserPriorityPrefsSyncResult",
                            static_cast<int>(timed_out),
                            static_cast<int>(SyncResult::COUNT));
}

}  // namespace chromeos
