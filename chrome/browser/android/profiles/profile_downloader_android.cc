// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/profiles/profile_downloader_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "jni/ProfileDownloader_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"

namespace {

// An account fetcher callback.
class AccountInfoRetriever : public ProfileDownloaderDelegate {
 public:
  AccountInfoRetriever(Profile* profile,
                       const std::string& account_id,
                       const std::string& email,
                       const int desired_image_side_pixels,
                       bool is_pre_signin)
      : profile_(profile),
        account_id_(account_id),
        email_(email),
        desired_image_side_pixels_(desired_image_side_pixels),
        is_pre_signin_(is_pre_signin) {}

  void Start() {
    profile_image_downloader_.reset(new ProfileDownloader(this));
    profile_image_downloader_->StartForAccount(account_id_);
  }

 private:
  void Shutdown() {
    profile_image_downloader_.reset();
    delete this;
  }

  // ProfileDownloaderDelegate implementation:
  bool NeedsProfilePicture() const override {
    return desired_image_side_pixels_ > 0;
  }

  int GetDesiredImageSideLength() const override {
    return desired_image_side_pixels_;
  }

  Profile* GetBrowserProfile() override {
    return profile_;
  }

  std::string GetCachedPictureURL() const override {
    return std::string();
  }

  bool IsPreSignin() const override {
    return is_pre_signin_;
  }

  void OnProfileDownloadSuccess(
      ProfileDownloader* downloader) override {

    base::string16 full_name = downloader->GetProfileFullName();
    base::string16 given_name = downloader->GetProfileGivenName();
    SkBitmap bitmap = downloader->GetProfilePicture();
    ScopedJavaLocalRef<jobject> jbitmap;
    if (!bitmap.isNull() && bitmap.bytesPerPixel() != 0)
      jbitmap = gfx::ConvertToJavaBitmap(&bitmap);

    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ProfileDownloader_onProfileDownloadSuccess(
        env,
        base::android::ConvertUTF8ToJavaString(env, email_).obj(),
        base::android::ConvertUTF16ToJavaString(env, full_name).obj(),
        base::android::ConvertUTF16ToJavaString(env, given_name).obj(),
        jbitmap.obj());
    Shutdown();
  }

  void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) override {
    LOG(ERROR) << "Failed to download the profile information: " << reason;
    Shutdown();
  }

  // The profile image downloader instance.
  scoped_ptr<ProfileDownloader> profile_image_downloader_;

  // The browser profile associated with this download request.
  Profile* profile_;

  // The account ID and email address of account to be loaded.
  const std::string account_id_;
  const std::string email_;

  // Desired side length of the profile image (in pixels).
  const int desired_image_side_pixels_;

  // True when the profile download is happening before the user has signed in,
  // such as during first run when we can still get tokens and want to fetch
  // the profile name and picture to display.
  bool is_pre_signin_;

  DISALLOW_COPY_AND_ASSIGN(AccountInfoRetriever);
};

}  // namespace

// static
jstring GetCachedFullNameForPrimaryAccount(JNIEnv* env,
                                           jclass clazz,
                                           jobject jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  ProfileInfoInterface& info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t index = info.GetIndexOfProfileWithPath(profile->GetPath());

  base::string16 name;
  if (index != std::string::npos)
    name = info.GetGAIANameOfProfileAtIndex(index);

  return base::android::ConvertUTF16ToJavaString(env, name).Release();
}

// static
jstring GetCachedGivenNameForPrimaryAccount(JNIEnv* env,
                                            jclass clazz,
                                            jobject jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  ProfileInfoInterface& info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t index = info.GetIndexOfProfileWithPath(profile->GetPath());

  base::string16 name;
  if (index != std::string::npos)
    name = info.GetGAIAGivenNameOfProfileAtIndex(index);

  return base::android::ConvertUTF16ToJavaString(env, name).Release();
}

// static
jobject GetCachedAvatarForPrimaryAccount(JNIEnv* env,
                                         jclass clazz,
                                         jobject jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  ProfileInfoInterface& info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const size_t index = info.GetIndexOfProfileWithPath(profile->GetPath());

  ScopedJavaLocalRef<jobject> jbitmap;
  if (index != std::string::npos) {
    gfx::Image avatar_image = info.GetAvatarIconOfProfileAtIndex(index);
    if (!avatar_image.IsEmpty() &&
        avatar_image.Width() > profiles::kAvatarIconWidth &&
        avatar_image.Height() > profiles::kAvatarIconHeight &&
        avatar_image.AsImageSkia().bitmap()) {
      jbitmap = gfx::ConvertToJavaBitmap(avatar_image.AsImageSkia().bitmap());
    }
  }

  return jbitmap.Release();
}

// static
void StartFetchingAccountInfoFor(
    JNIEnv* env,
    jclass clazz,
    jobject jprofile,
    jstring jemail,
    jint image_side_pixels,
    jboolean is_pre_signin) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  const std::string email =
      base::android::ConvertJavaStringToUTF8(env, jemail);
  // TODO(rogerta): the java code will need to pass in the gaia-id
  // of the account instead of the email when chrome uses gaia-id as key.
  DCHECK_EQ(AccountTrackerService::MIGRATION_NOT_STARTED,
            AccountTrackerServiceFactory::GetForProfile(profile)->
                GetMigrationState());
  AccountInfoRetriever* retriever =
      new AccountInfoRetriever(
          profile, gaia::CanonicalizeEmail(gaia::SanitizeEmail(email)), email,
          image_side_pixels, is_pre_signin);
  retriever->Start();
}

// static
bool RegisterProfileDownloader(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
