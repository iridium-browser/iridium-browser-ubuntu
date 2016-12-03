// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/background_sync_network_observer_android.h"
#include "content/browser/android/browser_startup_controller.h"
#include "content/browser/android/child_process_launcher_android.h"
#include "content/browser/android/content_video_view.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/content_view_render_view.h"
#include "content/browser/android/content_view_statics.h"
#include "content/browser/android/date_time_chooser_android.h"
#include "content/browser/android/interface_provider_android_impl.h"
#include "content/browser/android/interface_registry_android_impl.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/android/load_url_params.h"
#include "content/browser/android/tracing_controller_android.h"
#include "content/browser/android/web_contents_observer_proxy.h"
#include "content/browser/device_sensors/sensor_manager_android.h"
#include "content/browser/frame_host/navigation_controller_android.h"
#include "content/browser/media/session/media_session_delegate_android.h"
#include "content/browser/renderer_host/ime_adapter_android.h"
#include "content/browser/speech/speech_recognizer_impl_android.h"
#include "content/browser/time_zone_monitor_android.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "mojo/android/system/core_impl.h"
#include "mojo/android/system/watcher_impl.h"

namespace {
base::android::RegistrationMethod kContentRegisteredMethods[] = {
    {"BrowserStartupController", content::RegisterBrowserStartupController},
    {"ChildProcessLauncher", content::RegisterChildProcessLauncher},
    {"ContentVideoView", content::ContentVideoView::RegisterContentVideoView},
    {"CoreImpl", mojo::android::RegisterCoreImpl},
    {"MediaSessionDelegate", content::MediaSessionDelegateAndroid::Register},
    {"BackgroundSyncNetworkObserverAndroid",
     content::BackgroundSyncNetworkObserverAndroid::Observer::
         RegisterNetworkObserver},
    {"BrowserAccessibilityManager",
     content::RegisterBrowserAccessibilityManager},
    {"ContentViewCore", content::RegisterContentViewCore},
    {"ContentViewRenderView",
     content::ContentViewRenderView::RegisterContentViewRenderView},
    {"DateTimePickerAndroid", content::RegisterDateTimeChooserAndroid},
    {"InterfaceRegistryAndroid",
     content::InterfaceRegistryAndroidImpl::Register},
    {"InterfaceProviderAndroid",
     content::InterfaceProviderAndroidImpl::Register},
    {"InterstitialPageDelegateAndroid",
     content::InterstitialPageDelegateAndroid::
         RegisterInterstitialPageDelegateAndroid},
    {"LoadUrlParams", content::RegisterLoadUrlParams},
    {"NavigationControllerAndroid",
     content::NavigationControllerAndroid::Register},
    {"RegisterImeAdapter", content::RegisterImeAdapter},
    {"SensorManagerAndroid", content::SensorManagerAndroid::Register},
    {"SpeechRecognizerImplAndroid",
     content::SpeechRecognizerImplAndroid::RegisterSpeechRecognizer},
    {"TimeZoneMonitorAndroid", content::TimeZoneMonitorAndroid::Register},
    {"TracingControllerAndroid", content::RegisterTracingControllerAndroid},
    {"WatcherImpl", mojo::android::RegisterWatcherImpl},
    {"WebContentsAndroid", content::WebContentsAndroid::Register},
    {"WebContentsObserver", content::RegisterWebContentsObserverProxy},
    {"WebViewStatics", content::RegisterWebViewStatics},
};

}  // namespace

namespace content {
namespace android {

bool RegisterBrowserJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kContentRegisteredMethods,
                               arraysize(kContentRegisteredMethods));
}

}  // namespace android
}  // namespace content
