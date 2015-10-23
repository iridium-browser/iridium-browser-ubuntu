// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/google/core/browser/google_util.h"
#include "components/url_formatter/url_fixer.h"
#include "jni/UrlUtilities_jni.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF8;

namespace {

GURL ConvertJavaStringToGURL(JNIEnv*env, jstring url) {
  return url ? GURL(ConvertJavaStringToUTF8(env, url)) : GURL();
}

net::registry_controlled_domains::PrivateRegistryFilter GetRegistryFilter(
    jboolean include_private) {
  return include_private
      ? net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES
      : net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
}

}  // namespace

static jboolean SameDomainOrHost(JNIEnv* env,
                                 jclass clazz,
                                 jstring url_1_str,
                                 jstring url_2_str,
                                 jboolean include_private) {
  GURL url_1 = ConvertJavaStringToGURL(env, url_1_str);
  GURL url_2 = ConvertJavaStringToGURL(env, url_2_str);

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return net::registry_controlled_domains::SameDomainOrHost(url_1,
                                                            url_2,
                                                            filter);
}

static jstring GetDomainAndRegistry(JNIEnv* env,
                                    jclass clazz,
                                    jstring url,
                                    jboolean include_private) {
  DCHECK(url);
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return nullptr;

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  // OK to release, JNI binding.
  return base::android::ConvertUTF8ToJavaString(
      env, net::registry_controlled_domains::GetDomainAndRegistry(
          gurl, filter)).Release();
}

static jboolean IsGoogleSearchUrl(JNIEnv* env, jclass clazz, jstring url) {
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleSearchUrl(gurl);
}

static jboolean IsGoogleHomePageUrl(JNIEnv* env, jclass clazz, jstring url) {
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleHomePageUrl(gurl);
}

static jstring FixupUrl(JNIEnv* env,
                        jclass clazz,
                        jstring url,
                        jstring optional_desired_tld) {
  DCHECK(url);
  GURL fixed_url = url_formatter::FixupURL(
      base::android::ConvertJavaStringToUTF8(env, url),
      optional_desired_tld
          ? base::android::ConvertJavaStringToUTF8(env, optional_desired_tld)
          : std::string());

  return fixed_url.is_valid() ?
      base::android::ConvertUTF8ToJavaString(env, fixed_url.spec()).Release() :
      nullptr;
}

// Register native methods
bool RegisterUrlUtilities(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
