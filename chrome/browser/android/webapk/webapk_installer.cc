// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_installer.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/android/webapk/webapk.pb.h"
#include "chrome/browser/android/webapk/webapk_icon_hasher.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/manifest_util.h"
#include "jni/WebApkInstaller_jni.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

namespace {

// The default WebAPK server URL.
const char kDefaultServerUrl[] =
    "https://webapk.googleapis.com/v1/webApks/"
    "?alt=proto&key=AIzaSyAoI6v-F31-3t9NunLYEiKcPIqgTJIUZBw";

// The MIME type of the POST data sent to the server.
const char kProtoMimeType[] = "application/x-protobuf";

// The default number of milliseconds to wait for the WebAPK download URL from
// the WebAPK server.
const int kWebApkDownloadUrlTimeoutMs = 60000;

// The default number of milliseconds to wait for the WebAPK download to
// complete.
const int kDownloadTimeoutMs = 60000;

const int kWorldReadableFilePermission = base::FILE_PERMISSION_READ_BY_USER |
                                         base::FILE_PERMISSION_READ_BY_GROUP |
                                         base::FILE_PERMISSION_READ_BY_OTHERS;

// Returns the WebAPK server URL based on the command line.
GURL GetServerUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GURL command_line_url(
      command_line->GetSwitchValueASCII(switches::kWebApkServerUrl));
  return command_line_url.is_valid() ? command_line_url
                                     : GURL(kDefaultServerUrl);
}

// Returns the scope from |info| if it is specified. Otherwise, returns the
// default scope.
GURL GetScope(const ShortcutInfo& info) {
  return (info.scope.is_valid()) ? info.scope
                                 : ShortcutHelper::GetScopeFromURL(info.url);
}

// Converts a color from the format specified in content::Manifest to a CSS
// string.
std::string ColorToString(int64_t color) {
  if (color == content::Manifest::kInvalidOrMissingColor)
    return "";

  SkColor sk_color = reinterpret_cast<uint32_t&>(color);
  int r = SkColorGetR(sk_color);
  int g = SkColorGetG(sk_color);
  int b = SkColorGetB(sk_color);
  double a = SkColorGetA(sk_color) / 255.0;
  return base::StringPrintf("rgba(%d,%d,%d,%.2f)", r, g, b, a);
}

// Get Chrome's current ABI. It depends on whether Chrome is running as a 32 bit
// app or 64 bit, and the device's cpu architecture as well. Note: please keep
// this function stay in sync with |chromium_android_linker::GetCpuAbi()|.
std::string getCurrentAbi() {
#if defined(__arm__) && defined(__ARM_ARCH_7A__)
  return "armeabi-v7a";
#elif defined(__arm__)
  return "armeabi";
#elif defined(__i386__)
  return "x86";
#elif defined(__mips__)
  return "mips";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__aarch64__)
  return "arm64-v8a";
#else
#error "Unsupported target abi"
#endif
}

// Populates webapk::WebApk and returns it.
// Must be called on a worker thread because it encodes an SkBitmap.
std::unique_ptr<webapk::WebApk> BuildWebApkProtoInBackground(
    const ShortcutInfo& shortcut_info,
    const SkBitmap& shortcut_icon,
    const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
    bool is_manifest_stale) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  std::unique_ptr<webapk::WebApk> webapk(new webapk::WebApk);
  webapk->set_manifest_url(shortcut_info.manifest_url.spec());
  webapk->set_requester_application_package(
      base::android::BuildInfo::GetInstance()->package_name());
  webapk->set_requester_application_version(version_info::GetVersionNumber());
  webapk->set_android_abi(getCurrentAbi());
  webapk->set_stale_manifest(is_manifest_stale);

  webapk::WebAppManifest* web_app_manifest = webapk->mutable_manifest();
  web_app_manifest->set_name(base::UTF16ToUTF8(shortcut_info.name));
  web_app_manifest->set_short_name(base::UTF16ToUTF8(shortcut_info.short_name));
  web_app_manifest->set_start_url(shortcut_info.url.spec());
  web_app_manifest->set_orientation(
      content::WebScreenOrientationLockTypeToString(shortcut_info.orientation));
  web_app_manifest->set_display_mode(
      content::WebDisplayModeToString(shortcut_info.display));
  web_app_manifest->set_background_color(
      ColorToString(shortcut_info.background_color));
  web_app_manifest->set_theme_color(ColorToString(shortcut_info.theme_color));

  std::string* scope = web_app_manifest->add_scopes();
  scope->assign(GetScope(shortcut_info).spec());

  webapk::Image* best_image = web_app_manifest->add_icons();
  std::string best_primary_icon_url =
      shortcut_info.best_primary_icon_url.spec();
  best_image->set_src(best_primary_icon_url);
  auto it = icon_url_to_murmur2_hash.find(best_primary_icon_url);
  if (it != icon_url_to_murmur2_hash.end())
    best_image->set_hash(it->second);
  std::vector<unsigned char> png_bytes;
  gfx::PNGCodec::EncodeBGRASkBitmap(shortcut_icon, false, &png_bytes);
  best_image->set_image_data(&png_bytes.front(), png_bytes.size());

  for (const auto& entry : icon_url_to_murmur2_hash) {
    if (entry.first == shortcut_info.best_primary_icon_url.spec())
      continue;
    webapk::Image* image = web_app_manifest->add_icons();
    image->set_src(entry.first);
    image->set_hash(entry.second);
  }

  return webapk;
}

// Calls the callback when the |webapk| request is created.
void OnWebApkProtoBuilt(
    const base::Callback<void(std::unique_ptr<webapk::WebApk>)>& callback,
    std::unique_ptr<webapk::WebApk> webapk) {
  callback.Run(std::move(webapk));
}

// Returns task runner for running background tasks.
scoped_refptr<base::TaskRunner> GetBackgroundTaskRunner() {
  return content::BrowserThread::GetBlockingPool()
      ->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

// Creates a directory depending on the type of the task, and set permissions.
// It also creates any parent directory along the path if doesn't exist,
// and sets permissions as well.
// The previously downloaded APKs are deleted in order to clean up unused cached
// data.
base::FilePath CreateSubDirAndSetPermissionsInBackground(
    const base::StringPiece& output_dir_name,
    const std::string& package_name) {
  base::FilePath output_root_dir;
  base::android::GetCacheDirectory(&output_root_dir);
  base::FilePath webapk_dir = output_root_dir.AppendASCII("webapks");
  // Creating different downloaded directory for install/update cases is
  // to prevent deleting the APK which is still in use when an install and an
  // update happen at the same time. However, it doesn't help the cases of when
  // mutiple installs (or multiple updates) happen at the same time.
  base::FilePath output_dir = webapk_dir.AppendASCII(output_dir_name);
  int posix_permissions = kWorldReadableFilePermission |
                          base::FILE_PERMISSION_WRITE_BY_USER |
                          base::FILE_PERMISSION_EXECUTE_BY_USER |
                          base::FILE_PERMISSION_EXECUTE_BY_OTHERS;
  if (base::PathExists(output_dir))
    base::DeleteFile(output_dir, true);

  // Creates the directory to download and sets permissions.
  if (!base::CreateDirectory(output_dir) ||
      !base::SetPosixFilePermissions(webapk_dir, posix_permissions) ||
      !base::SetPosixFilePermissions(output_dir, posix_permissions))
    return base::FilePath();

  return output_dir;
}

}  // anonymous namespace

WebApkInstaller::~WebApkInstaller() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkInstaller_destroy(env, java_ref_);
  java_ref_.Reset();
}

// static
void WebApkInstaller::InstallAsync(content::BrowserContext* context,
                                   const ShortcutInfo& shortcut_info,
                                   const SkBitmap& shortcut_icon,
                                   const FinishCallback& finish_callback) {
  // The installer will delete itself when it is done.
  WebApkInstaller* installer =
      new WebApkInstaller(context, shortcut_info, shortcut_icon);
  installer->InstallAsync(finish_callback);
}

// static
void WebApkInstaller::UpdateAsync(
    content::BrowserContext* context,
    const ShortcutInfo& shortcut_info,
    const SkBitmap& shortcut_icon,
    const std::string& webapk_package,
    int webapk_version,
    const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    const FinishCallback& finish_callback) {
  // The installer will delete itself when it is done.
  WebApkInstaller* installer =
      new WebApkInstaller(context, shortcut_info, shortcut_icon);
  installer->UpdateAsync(webapk_package, webapk_version,
                         icon_url_to_murmur2_hash, is_manifest_stale,
                         finish_callback);
}

// staic
void WebApkInstaller::InstallAsyncForTesting(
    WebApkInstaller* installer,
    const FinishCallback& finish_callback) {
  installer->InstallAsync(finish_callback);
}

// static
void WebApkInstaller::UpdateAsyncForTesting(
    WebApkInstaller* installer,
    const std::string& webapk_package,
    int webapk_version,
    const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    const FinishCallback& finish_callback) {
  installer->UpdateAsync(webapk_package, webapk_version,
                         icon_url_to_murmur2_hash, is_manifest_stale,
                         finish_callback);
}

void WebApkInstaller::SetTimeoutMs(int timeout_ms) {
  webapk_download_url_timeout_ms_ = timeout_ms;
  download_timeout_ms_ = timeout_ms;
}

void WebApkInstaller::OnInstallFinished(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint result) {
  OnResult(static_cast<WebApkInstallResult>(result));
}

void WebApkInstaller::BuildWebApkProtoInBackgroundForTesting(
    const base::Callback<void(std::unique_ptr<webapk::WebApk>)>& callback,
    const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
    bool is_manifest_stale) {
  base::PostTaskAndReplyWithResult(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&BuildWebApkProtoInBackground, shortcut_info_, shortcut_icon_,
                 icon_url_to_murmur2_hash, is_manifest_stale),
      base::Bind(&OnWebApkProtoBuilt, callback));
}

// static
bool WebApkInstaller::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool WebApkInstaller::StartInstallingDownloadedWebApk(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jstring>& java_file_path,
    const base::android::ScopedJavaLocalRef<jstring>& java_package_name) {
  return Java_WebApkInstaller_installAsyncAndMonitorInstallationFromNative(
      env, java_ref_, java_file_path, java_package_name);
}

bool WebApkInstaller::StartUpdateUsingDownloadedWebApk(
    JNIEnv* env,
    const base::android::ScopedJavaLocalRef<jstring>& java_file_path) {
  return Java_WebApkInstaller_updateAsyncFromNative(env, java_ref_,
                                                    java_file_path);
}

bool WebApkInstaller::CanUseGooglePlayInstallService() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebApkInstaller_canUseGooglePlayInstallService(env, java_ref_);
}

void WebApkInstaller::InstallOrUpdateWebApkFromGooglePlay(
    const std::string& package_name,
    int version,
    const std::string& token) {
  webapk_package_ = package_name;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_webapk_package =
      base::android::ConvertUTF8ToJavaString(env, webapk_package_);
  base::android::ScopedJavaLocalRef<jstring> java_title =
      base::android::ConvertUTF16ToJavaString(env, shortcut_info_.user_title);
  base::android::ScopedJavaLocalRef<jstring> java_token =
      base::android::ConvertUTF8ToJavaString(env, token);
  base::android::ScopedJavaLocalRef<jstring> java_url =
      base::android::ConvertUTF8ToJavaString(env, shortcut_info_.url.spec());

  if (task_type_ == WebApkInstaller::INSTALL) {
    Java_WebApkInstaller_installWebApkFromGooglePlayAsync(
        env, java_ref_, java_webapk_package, version, java_title, java_token,
        java_url);
  } else {
    Java_WebApkInstaller_updateAsyncFromGooglePlay(
        env, java_ref_, java_webapk_package, version, java_title, java_token,
        java_url);
  }
}

void WebApkInstaller::OnResult(WebApkInstallResult result) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  finish_callback_.Run(result, webapk_package_);
  delete this;
}

WebApkInstaller::WebApkInstaller(content::BrowserContext* browser_context,
                                 const ShortcutInfo& shortcut_info,
                                 const SkBitmap& shortcut_icon)
    : request_context_getter_(
          Profile::FromBrowserContext(browser_context)->GetRequestContext()),
      shortcut_info_(shortcut_info),
      shortcut_icon_(shortcut_icon),
      server_url_(GetServerUrl()),
      webapk_download_url_timeout_ms_(kWebApkDownloadUrlTimeoutMs),
      download_timeout_ms_(kDownloadTimeoutMs),
      task_type_(UNDEFINED),
      weak_ptr_factory_(this) {
  CreateJavaRef();
}

void WebApkInstaller::CreateJavaRef() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(
      Java_WebApkInstaller_create(env, reinterpret_cast<intptr_t>(this)));
}

void WebApkInstaller::InstallAsync(const FinishCallback& finish_callback) {
  finish_callback_ = finish_callback;
  task_type_ = INSTALL;

  // We need to take the hash of the bitmap at the icon URL prior to any
  // transformations being applied to the bitmap (such as encoding/decoding
  // the bitmap). The icon hash is used to determine whether the icon that
  // the user sees matches the icon of a WebAPK that the WebAPK server
  // generated for another user. (The icon can be dynamically generated.)
  //
  // We redownload the icon in order to take the Murmur2 hash. The redownload
  // should be fast because the icon should be in the HTTP cache.
  DownloadAppIconAndComputeMurmur2Hash();
}

void WebApkInstaller::UpdateAsync(
    const std::string& webapk_package,
    int webapk_version,
    const std::map<std::string, std::string>& icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    const FinishCallback& finish_callback) {
  webapk_package_ = webapk_package;
  webapk_version_ = webapk_version;
  finish_callback_ = finish_callback;
  task_type_ = UPDATE;

  base::PostTaskAndReplyWithResult(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&BuildWebApkProtoInBackground, shortcut_info_, shortcut_icon_,
                 icon_url_to_murmur2_hash, is_manifest_stale),
      base::Bind(&WebApkInstaller::SendUpdateWebApkRequest,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::OnURLFetchComplete(const net::URLFetcher* source) {
  timer_.Stop();

  if (!source->GetStatus().is_success() ||
      source->GetResponseCode() != net::HTTP_OK) {
    LOG(WARNING) << base::StringPrintf(
        "WebAPK server returned response code %d.", source->GetResponseCode());
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  std::string response_string;
  source->GetResponseAsString(&response_string);

  std::unique_ptr<webapk::WebApkResponse> response(new webapk::WebApkResponse);
  if (!response->ParseFromString(response_string)) {
    LOG(WARNING) << "WebAPK server did not return proto.";
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  GURL signed_download_url(response->signed_download_url());
  // https://crbug.com/680131. The server sends an empty URL if the server does
  // not have a newer WebAPK to update to.
  if (task_type_ == UPDATE && signed_download_url.is_empty()) {
    OnResult(WebApkInstallResult::SUCCESS);
    return;
  }

  if (!signed_download_url.is_valid() || response->package_name().empty()) {
    LOG(WARNING) << "WebAPK server returned incomplete proto.";
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  if (CanUseGooglePlayInstallService()) {
    int version = 1;
    base::StringToInt(response->version(), &version);
    InstallOrUpdateWebApkFromGooglePlay(response->package_name(), version,
                                        response->token());
    return;
  }

  OnGotWebApkDownloadUrl(signed_download_url, response->package_name());
}

void WebApkInstaller::DownloadAppIconAndComputeMurmur2Hash() {
  // Safeguard. WebApkIconHasher crashes if asked to fetch an invalid URL.
  if (!shortcut_info_.best_primary_icon_url.is_valid()) {
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(download_timeout_ms_),
      base::Bind(&WebApkInstaller::OnResult, weak_ptr_factory_.GetWeakPtr(),
                 WebApkInstallResult::FAILURE));

  icon_hasher_.reset(new WebApkIconHasher());
  icon_hasher_->DownloadAndComputeMurmur2Hash(
      request_context_getter_, shortcut_info_.best_primary_icon_url,
      base::Bind(&WebApkInstaller::OnGotIconMurmur2Hash,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::OnGotIconMurmur2Hash(
    const std::string& icon_murmur2_hash) {
  timer_.Stop();
  icon_hasher_.reset();

  // An empty hash indicates that |icon_hasher_| encountered an error.
  if (icon_murmur2_hash.empty()) {
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  std::map<std::string, std::string> icon_url_to_murmur2_hash;
  for (const std::string& icon_url : shortcut_info_.icon_urls) {
    if (icon_url != shortcut_info_.best_primary_icon_url.spec())
      icon_url_to_murmur2_hash[icon_url] = "";
    else
      icon_url_to_murmur2_hash[icon_url] = icon_murmur2_hash;
  }

  base::PostTaskAndReplyWithResult(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&BuildWebApkProtoInBackground, shortcut_info_, shortcut_icon_,
                 icon_url_to_murmur2_hash, false /* is_manifest_stale */),
      base::Bind(&WebApkInstaller::SendCreateWebApkRequest,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::SendCreateWebApkRequest(
    std::unique_ptr<webapk::WebApk> webapk) {
  SendRequest(std::move(webapk), server_url_);
}

void WebApkInstaller::SendUpdateWebApkRequest(
    std::unique_ptr<webapk::WebApk> webapk) {
  webapk->set_package_name(webapk_package_);
  webapk->set_version(std::to_string(webapk_version_));

  SendRequest(std::move(webapk), server_url_);
}

void WebApkInstaller::SendRequest(std::unique_ptr<webapk::WebApk> request_proto,
                                  const GURL& server_url) {
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(webapk_download_url_timeout_ms_),
      base::Bind(&WebApkInstaller::OnResult, weak_ptr_factory_.GetWeakPtr(),
                 WebApkInstallResult::FAILURE));

  url_fetcher_ =
      net::URLFetcher::Create(server_url, net::URLFetcher::POST, this);
  url_fetcher_->SetRequestContext(request_context_getter_);
  std::string serialized_request;
  request_proto->SerializeToString(&serialized_request);
  url_fetcher_->SetUploadData(kProtoMimeType, serialized_request);
  url_fetcher_->SetLoadFlags(
      net::LOAD_DISABLE_CACHE | net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->Start();
}

void WebApkInstaller::OnGotWebApkDownloadUrl(const GURL& download_url,
                                             const std::string& package_name) {
  webapk_package_ = package_name;

  base::PostTaskAndReplyWithResult(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&CreateSubDirAndSetPermissionsInBackground,
                 task_type_ == WebApkInstaller::INSTALL ? "install" : "update",
                 package_name),
      base::Bind(&WebApkInstaller::OnCreatedSubDirAndSetPermissions,
                 weak_ptr_factory_.GetWeakPtr(), download_url));
}

void WebApkInstaller::OnCreatedSubDirAndSetPermissions(
    const GURL& download_url,
    const base::FilePath& output_dir) {
  if (output_dir.empty()) {
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  DownloadWebApk(output_dir.AppendASCII(webapk_package_ + ".apk"), download_url,
                 true);
}

void WebApkInstaller::DownloadWebApk(const base::FilePath& output_path,
                                     const GURL& download_url,
                                     bool retry_if_fails) {
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(download_timeout_ms_),
      base::Bind(&WebApkInstaller::OnResult, weak_ptr_factory_.GetWeakPtr(),
                 WebApkInstallResult::FAILURE));

  downloader_.reset(new FileDownloader(
      download_url, output_path, true, request_context_getter_,
      base::Bind(&WebApkInstaller::OnWebApkDownloaded,
                 weak_ptr_factory_.GetWeakPtr(), output_path, download_url,
                 retry_if_fails),
      NO_TRAFFIC_ANNOTATION_YET));
}

void WebApkInstaller::OnWebApkDownloaded(const base::FilePath& file_path,
                                         const GURL& download_url,
                                         bool retry_if_fails,
                                         FileDownloader::Result result) {
  timer_.Stop();

  if (result != FileDownloader::DOWNLOADED) {
    if (!retry_if_fails) {
      OnResult(WebApkInstallResult::FAILURE);
      return;
    }

    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&WebApkInstaller::DownloadWebApk,
                   weak_ptr_factory_.GetWeakPtr(), file_path, download_url,
                   false),
        base::TimeDelta::FromSeconds(2));
    return;
  }

  int posix_permissions = kWorldReadableFilePermission |
                          base::FILE_PERMISSION_WRITE_BY_USER |
                          base::FILE_PERMISSION_EXECUTE_BY_USER;
  base::PostTaskAndReplyWithResult(
      GetBackgroundTaskRunner().get(), FROM_HERE,
      base::Bind(&base::SetPosixFilePermissions, file_path, posix_permissions),
      base::Bind(&WebApkInstaller::OnWebApkMadeWorldReadable,
                 weak_ptr_factory_.GetWeakPtr(), file_path));
}

void WebApkInstaller::OnWebApkMadeWorldReadable(
    const base::FilePath& file_path,
    bool change_permission_success) {
  if (!change_permission_success) {
    OnResult(WebApkInstallResult::FAILURE);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_file_path =
      base::android::ConvertUTF8ToJavaString(env, file_path.value());
  base::android::ScopedJavaLocalRef<jstring> java_package_name =
      base::android::ConvertUTF8ToJavaString(env, webapk_package_);
  bool success = false;
  if (task_type_ == INSTALL) {
    success =
        StartInstallingDownloadedWebApk(env, java_file_path, java_package_name);
  } else if (task_type_ == UPDATE) {
    success = StartUpdateUsingDownloadedWebApk(env, java_file_path);
    if (success) {
      // Since WebApkInstaller doesn't listen to WebAPKs' update events
      // we call OnSuccess() as long as the update started successfully.
      OnResult(WebApkInstallResult::SUCCESS);
      return;
    }
  }
  if (!success)
    OnResult(WebApkInstallResult::FAILURE);
}
