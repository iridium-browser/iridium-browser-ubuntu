// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_

#include <jni.h>
#include <map>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/net/file_downloader.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace webapk {
class WebApk;
}

class WebApkIconHasher;

// Talks to Chrome WebAPK server and Google Play to generate a WebAPK on the
// server, download it, and install it. The native WebApkInstaller owns the
// Java WebApkInstaller counterpart.
class WebApkInstaller : public net::URLFetcherDelegate {
 public:
  using FinishCallback = WebApkInstallService::FinishCallback;

  ~WebApkInstaller() override;

  // Creates a self-owned WebApkInstaller instance and talks to the Chrome
  // WebAPK server to generate a WebAPK on the server and to Google Play to
  // install the downloaded WebAPK. Calls |callback| once the install completed
  // or failed.
  static void InstallAsync(content::BrowserContext* context,
                           const ShortcutInfo& shortcut_info,
                           const SkBitmap& shortcut_icon,
                           const FinishCallback& finish_callback);

  // Creates a self-owned WebApkInstaller instance and talks to the Chrome
  // WebAPK server to update a WebAPK on the server and to the Google Play
  // server to install the downloaded WebAPK. Calls |callback| after the request
  // to install the WebAPK is sent to the Google Play server.
  static void UpdateAsync(
      content::BrowserContext* context,
      const ShortcutInfo& shortcut_info,
      const SkBitmap& shortcut_icon,
      const std::string& webapk_package,
      int webapk_version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      const FinishCallback& callback);

  // Calls the private function |InstallAsync| for testing.
  // Should be used only for testing.
  static void InstallAsyncForTesting(WebApkInstaller* installer,
                                     const FinishCallback& finish_callback);

  // Calls the private function |UpdateAsync| for testing.
  // Should be used only for testing.
  static void UpdateAsyncForTesting(
      WebApkInstaller* installer,
      const std::string& webapk_package,
      int webapk_version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      const FinishCallback& callback);

  // Sets the timeout for the server requests.
  void SetTimeoutMs(int timeout_ms);

  // Called once the installation is complete or failed.
  void OnInstallFinished(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint result);

  // Creates a WebApk install or update request.
  // Should be used only for testing.
  void BuildWebApkProtoInBackgroundForTesting(
      const base::Callback<void(std::unique_ptr<webapk::WebApk>)>& callback,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale);

  // Registers JNI hooks.
  static bool Register(JNIEnv* env);

 protected:
  WebApkInstaller(content::BrowserContext* browser_context,
                  const ShortcutInfo& shortcut_info,
                  const SkBitmap& shortcut_icon);

  // Starts installation of the downloaded WebAPK. Returns whether the install
  // could be started. The installation may still fail if true is returned.
  // |file_path| is the file path that the WebAPK was downloaded to.
  // |package_name| is the package name that the WebAPK should be installed at.
  virtual bool StartInstallingDownloadedWebApk(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jstring>& java_file_path,
      const base::android::ScopedJavaLocalRef<jstring>& java_package_name);

  // Starts update using the downloaded WebAPK. Returns whether the updating
  // could be started. The updating may still fail if true is returned.
  // |file_path| is the file path that the WebAPK was downloaded to.
  virtual bool StartUpdateUsingDownloadedWebApk(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jstring>& java_file_path);

  // Returns whether Google Play Services can be used and the install delegate
  // is available.
  // Note: it is possible that this delegate is null even when installing
  // WebAPKs using Google Play is enabled.
  virtual bool CanUseGooglePlayInstallService();

  // Called when the package name of the WebAPK is available and the install
  // or update request is handled by Google Play.
  virtual void InstallOrUpdateWebApkFromGooglePlay(
      const std::string& package_name,
      int version,
      const std::string& token);

  // Called when the install or update process has completed or failed.
  void OnResult(WebApkInstallResult result);

 private:
  enum TaskType {
    UNDEFINED,
    INSTALL,
    UPDATE,
  };

  // Create the Java object.
  void CreateJavaRef();

  // Talks to the Chrome WebAPK server to generate a WebAPK on the server and to
  // Google Play to install the downloaded WebAPK. Calls |callback| once the
  // install completed or failed.
  void InstallAsync(const FinishCallback& finish_callback);

  // Talks to the Chrome WebAPK server to update a WebAPK on the server and to
  // the Google Play server to install the downloaded WebAPK. Calls |callback|
  // after the request to install the WebAPK is sent to the Google Play server.
  void UpdateAsync(
      const std::string& webapk_package,
      int webapk_version,
      const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
      bool is_manifest_stale,
      const FinishCallback& callback);

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Downloads app icon in order to compute Murmur2 hash.
  void DownloadAppIconAndComputeMurmur2Hash();

  // Called with the computed Murmur2 hash for the app icon.
  void OnGotIconMurmur2Hash(const std::string& icon_murmur2_hash);

  // Sends request to WebAPK server to create WebAPK. During a successful
  // request the WebAPK server responds with the URL of the generated WebAPK.
  // |webapk| is the proto to send to the WebAPK server.
  void SendCreateWebApkRequest(std::unique_ptr<webapk::WebApk> webapk_proto);

  // Sends request to WebAPK server to update a WebAPK. During a successful
  // request the WebAPK server responds with the URL of the generated WebAPK.
  // |webapk| is the proto to send to the WebAPK server.
  void SendUpdateWebApkRequest(std::unique_ptr<webapk::WebApk> webapk_proto);

  // Sends a request to WebAPK server to create/update WebAPK. During a
  // successful request the WebAPK server responds with the URL of the generated
  // WebAPK.
  void SendRequest(std::unique_ptr<webapk::WebApk> request_proto,
                   const GURL& server_url);

  // Called with the URL of generated WebAPK and the package name that the
  // WebAPK should be installed at.
  void OnGotWebApkDownloadUrl(const GURL& download_url,
                              const std::string& package_name);

  // Downloads the WebAPK from the given |download_url|.
  void DownloadWebApk(const base::FilePath& output_path,
                      const GURL& download_url,
                      bool retry_if_fails);

  // Called once the sub directory to store the downloaded WebAPK was
  // created with permissions set properly or if creation failed.
  void OnCreatedSubDirAndSetPermissions(const GURL& download_url,
                                        const base::FilePath& file_path);

  // Called once the WebAPK has been downloaded. Makes the downloaded WebAPK
  // world readable and installs the WebAPK if the download was successful.
  // |file_path| is the file path that the WebAPK was downloaded to.
  // If |retry_if_fails| is true, will post a delayed task and retry the
  // download after 2 seconds.
  void OnWebApkDownloaded(const base::FilePath& file_path,
                          const GURL& download_url,
                          bool retry_if_fails,
                          FileDownloader::Result result);

  // Called once the downloaded WebAPK has been made world readable. Installs
  // the WebAPK.
  // |file_path| is the file path that the WebAPK was downloaded to.
  // |change_permission_success| is whether the WebAPK could be made world
  // readable.
  void OnWebApkMadeWorldReadable(const base::FilePath& file_path,
                                 bool change_permission_success);

  net::URLRequestContextGetter* request_context_getter_;

  // Sends HTTP request to WebAPK server.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Downloads app icon and computes Murmur2 hash.
  std::unique_ptr<WebApkIconHasher> icon_hasher_;

  // Downloads WebAPK.
  std::unique_ptr<FileDownloader> downloader_;

  // Fails WebApkInstaller if WebAPK server takes too long to respond or if the
  // download takes too long.
  base::OneShotTimer timer_;

  // Callback to call once WebApkInstaller succeeds or fails.
  FinishCallback finish_callback_;

  // Web Manifest info.
  const ShortcutInfo shortcut_info_;

  // WebAPK app icon.
  const SkBitmap shortcut_icon_;

  // WebAPK server URL.
  GURL server_url_;

  // The number of milliseconds to wait for the WebAPK download URL from the
  // WebAPK server.
  int webapk_download_url_timeout_ms_;

  // The number of milliseconds to wait for the WebAPK download to complete.
  int download_timeout_ms_;

  // WebAPK package name.
  std::string webapk_package_;

  // WebAPK version code.
  int webapk_version_;

  // Indicates whether the installer is for installing or updating a WebAPK.
  TaskType task_type_;

  // Points to the Java Object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Used to get |weak_ptr_|.
  base::WeakPtrFactory<WebApkInstaller> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebApkInstaller);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALLER_H_
