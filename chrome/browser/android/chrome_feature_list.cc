// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_feature_list.h"

#include <stddef.h>

#include <string>

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/ntp_snippets/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/common/content_features.h"
#include "jni/ChromeFeatureList_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace chrome {
namespace android {

namespace {

// Array of features exposed through the Java ChromeFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. chrome/, components/, etc).
const base::Feature* kFeaturesExposedToJava[] = {
    &autofill::kAutofillScanCardholderName,
    &features::kConsistentOmniboxGeolocation,
    &features::kCredentialManagementAPI,
    &features::kServiceWorkerPaymentApps,
    &features::kSimplifiedFullscreenUI,
    &features::kVrShell,
    &features::kWebPayments,
    &kAndroidPayIntegrationV1,
    &kAndroidPayIntegrationV2,
    &kAndroidPaymentApps,
    &kCCTExternalLinkHandling,
    &kCCTPostMessageAPI,
    &kChromeHomeFeature,
    &kContextualSearchSingleActions,
    &kImportantSitesInCBD,
    &kImprovedA2HS,
    &kNativeAndroidHistoryManager,
    &kNoCreditCardAbort,
    &kNTPFakeOmniboxTextFeature,
    &kNTPOfflinePagesFeature,
    &kNTPSuggestionsStandaloneUIFeature,
    &kPhysicalWebFeature,
    &kPhysicalWebIgnoreOtherClientsFeature,
    &kSpecialLocaleFeature,
    &kSpecialLocaleWrapper,
    &kTabReparenting,
    &kWebPaymentsModifiers,
    &ntp_snippets::kContentSuggestionsFeature,
    &kWebVRCardboardSupport,
    &ntp_snippets::kIncreasedVisibility,
    &ntp_snippets::kForeignSessionsSuggestionsFeature,
    &ntp_snippets::kOfflineBadgeFeature,
    &ntp_snippets::kSaveToOfflineFeature,
    &ntp_snippets::kSectionDismissalFeature,
    &offline_pages::kBackgroundLoaderForDownloadsFeature,
    &offline_pages::kOfflinePagesCTFeature,  // See crbug.com/620421.
    &offline_pages::kOfflinePagesSharingFeature,
    &password_manager::features::kViewPasswords,
};

}  // namespace

// Alphabetical:
const base::Feature kAndroidPayIntegrationV1{"AndroidPayIntegrationV1",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAndroidPayIntegrationV2{"AndroidPayIntegrationV2",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAndroidPaymentApps{"AndroidPaymentApps",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTExternalLinkHandling{"CCTExternalLinkHandling",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTPostMessageAPI{"CCTPostMessageAPI",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kChromeHomeFeature{"ChromeHome",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchSingleActions{
    "ContextualSearchSingleActions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDownloadAutoResumptionThrottling{
    "DownloadAutoResumptionThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kImportantSitesInCBD{"ImportantSitesInCBD",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Makes "Add to Home screen" in the app menu generate an APK for the shortcut
// URL which opens Chrome in fullscreen.
const base::Feature kImprovedA2HS{"ImprovedA2HS",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNativeAndroidHistoryManager{
  "AndroidHistoryManager", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNoCreditCardAbort{"NoCreditCardAbort",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNTPFakeOmniboxTextFeature{
    "NTPFakeOmniboxText", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNTPOfflinePagesFeature{"NTPOfflinePages",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNTPSuggestionsStandaloneUIFeature{
    "NTPSuggestionsStandaloneUI", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPhysicalWebFeature{"PhysicalWeb",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPhysicalWebIgnoreOtherClientsFeature{
    "PhysicalWebIgnoreOtherClients", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSpecialLocaleFeature{"SpecialLocale",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSpecialLocaleWrapper{"SpecialLocaleWrapper",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTabReparenting{"TabReparenting",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUserMediaScreenCapturing{
    "UserMediaScreenCapturing", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebPaymentsModifiers{"WebPaymentsModifiers",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebVRCardboardSupport{
    "WebVRCardboardSupport", base::FEATURE_ENABLED_BY_DEFAULT};

static jboolean IsEnabled(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          const JavaParamRef<jstring>& jfeature_name) {
  const std::string feature_name = ConvertJavaStringToUTF8(env, jfeature_name);
  for (size_t i = 0; i < arraysize(kFeaturesExposedToJava); ++i) {
    if (kFeaturesExposedToJava[i]->name == feature_name)
      return base::FeatureList::IsEnabled(*kFeaturesExposedToJava[i]);
  }
  // Features queried via this API must be present in |kFeaturesExposedToJava|.
  NOTREACHED();
  return false;
}

static jint GetFieldTrialParamByFeatureAsInt(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jint jdefault_value) {
  const std::string feature_name = ConvertJavaStringToUTF8(env, jfeature_name);
  const std::string param_name = ConvertJavaStringToUTF8(env, jparam_name);
  int default_value = static_cast<int>(jdefault_value);

  for (size_t i = 0; i < arraysize(kFeaturesExposedToJava); ++i) {
    if (kFeaturesExposedToJava[i]->name == feature_name)
      return variations::GetVariationParamByFeatureAsInt(
          *kFeaturesExposedToJava[i], param_name, default_value);
  }
  // Features queried via this API must be present in |kFeaturesExposedToJava|.
  NOTREACHED();
  return jdefault_value;
}

bool RegisterChromeFeatureListJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
