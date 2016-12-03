// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents_user_data.h"

namespace banners {

// Extends the AppBannerManager to support native Android apps.
// This class owns a Java-side AppBannerManager which is used to interface with
// the Java runtime for fetching native app data and installing them when
// requested.
// A site requests a native app banner by setting "prefer_related_applications"
// to true in its manifest, and providing at least one related application for
// the "play" platform with a Play Store ID.
// This class uses that information to request the app's metadata, including an
// icon. If successful, the icon is downloaded and the native app banner shown.
// Otherwise, if no related applications were detected, or their manifest
// entries were invalid, this class falls back to trying to verify if a web app
// banner is suitable.
class AppBannerManagerAndroid
    : public AppBannerManager,
      public content::WebContentsUserData<AppBannerManagerAndroid> {
 public:
  explicit AppBannerManagerAndroid(content::WebContents* web_contents);
  ~AppBannerManagerAndroid() override;

  // Returns a reference to the Java-side AppBannerManager owned by this object.
  const base::android::ScopedJavaGlobalRef<jobject>& GetJavaBannerManager()
      const;

  // Returns true if this object is currently active.
  bool IsActiveForTesting(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj);

  // Called when the Java-side has retrieved information for the app.
  // Returns |false| if an icon fetch couldn't be kicked off.
  bool OnAppDetailsRetrieved(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& japp_data,
      const base::android::JavaParamRef<jstring>& japp_title,
      const base::android::JavaParamRef<jstring>& japp_package,
      const base::android::JavaParamRef<jstring>& jicon_url);

  // AppBannerManager overrides.
  void RequestAppBanner(const GURL& validated_url, bool is_debug_mode) override;

  // Returns a callback which fetches the splash screen image and stores it in
  // a WebappDataStorage.
  base::Closure FetchWebappSplashScreenImageCallback(
      const std::string& webapp_id) override;

  // Registers native methods.
  static bool Register(JNIEnv* env);

 protected:
  // AppBannerManager overrides.
  std::string GetAppIdentifier() override;
  std::string GetBannerType() override;
  int GetIdealIconSizeInDp() override;
  int GetMinimumIconSizeInDp() override;
  bool IsWebAppInstalled(content::BrowserContext* browser_context,
                         const GURL& start_url) override;

  void PerformInstallableCheck() override;
  void OnAppIconFetched(const SkBitmap& bitmap) override;
  void ShowBanner() override;

 private:
  friend class content::WebContentsUserData<AppBannerManagerAndroid>;

  // Creates the Java-side AppBannerManager.
  void CreateJavaBannerManager();

  // Returns true if |platform| and |id| are valid for querying the Play Store.
  bool CheckPlatformAndId(const std::string& platform,
                          const std::string& id);

  // Returns the query value for |name| in |url|, e.g. example.com?name=value.
  std::string ExtractQueryValueForName(const GURL& url,
                                       const std::string& name);

  // Returns true if |platform|, |url|, and |id| are consistent and can be used
  // to query the Play Store for a native app. The query may not necessarily
  // succeed (e.g. |id| doesn't map to anything), but if this method returns
  // true, only a native app banner may be shown, and the web app banner flow
  // will not be run.
  bool CanHandleNonWebApp(const std::string& platform,
                          const GURL& url,
                          const std::string& id);

  // The Java-side AppBannerManager.
  base::android::ScopedJavaGlobalRef<jobject> java_banner_manager_;

  // Java-side object containing data about a native app.
  base::android::ScopedJavaGlobalRef<jobject> native_app_data_;

  // App package name for a native app banner.
  std::string native_app_package_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerAndroid);
};

}  // namespace banners

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_MANAGER_ANDROID_H_
