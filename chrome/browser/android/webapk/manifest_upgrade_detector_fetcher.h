// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_MANIFEST_UPGRADE_DETECTOR_FETCHER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_MANIFEST_UPGRADE_DETECTOR_FETCHER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
struct Manifest;
class WebContents;
}

class GURL;
class SkBitmap;
struct InstallableData;
struct ShortcutInfo;

// ManifestUpgradeDetectorFetcher is the C++ counterpart of
// org.chromium.chrome.browser's ManifestUpgradeDetectorFetcher in Java. It is
// created via a JNI (Initialize) call and MUST BE DESTROYED via Destroy().
class ManifestUpgradeDetectorFetcher : public content::WebContentsObserver {
 public:
  ManifestUpgradeDetectorFetcher(JNIEnv* env,
                                 jobject obj,
                                 const GURL& scope,
                                 const GURL& web_manifest_url);

  // Replaces the WebContents that is being observed.
  void ReplaceWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& java_web_contents);

  // Called by the Java counterpart to destroy its native half.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called by the Java counterpart to start checking web manifest changes.
  void Start(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             const base::android::JavaParamRef<jobject>& java_web_contents);

  // Registers JNI hooks.
  static bool Register(JNIEnv* env);

 private:
  ~ManifestUpgradeDetectorFetcher() override;

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // Called once the installable data has been fetched.
  void OnDidGetInstallableData(const InstallableData& installable_data);

  void OnDataAvailable(const ShortcutInfo& info, const SkBitmap& icon);

  // Points to the Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // The detector will only fetch the URL within the scope of the WebAPK.
  const GURL scope_;

  // The WebAPK's Web Manifest URL that the detector is looking for.
  const GURL web_manifest_url_;

  base::WeakPtrFactory<ManifestUpgradeDetectorFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManifestUpgradeDetectorFetcher);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_MANIFEST_UPGRADE_DETECTOR_FETCHER_H_
