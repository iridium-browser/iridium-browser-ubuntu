// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/ntp_snippets_bridge.h"

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_metrics.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "jni/SnippetsBridge_jni.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::KnownCategories;
using ntp_snippets::ContentSuggestion;

namespace {

void URLVisitedHistoryRequestCallback(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    bool success,
    const history::URLRow& row,
    const history::VisitVector& visitVector) {
  bool visited = success && row.visit_count() != 0;
  base::android::RunCallbackAndroid(callback, visited);
}

// TODO(treib): Move this into the Time class itself.
base::Time TimeFromJavaTime(jlong timestamp_ms) {
  return base::Time::UnixEpoch() +
         base::TimeDelta::FromMilliseconds(timestamp_ms);
}

} // namespace

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& j_profile) {
  NTPSnippetsBridge* snippets_bridge = new NTPSnippetsBridge(env, j_profile);
  return reinterpret_cast<intptr_t>(snippets_bridge);
}

static void FetchSnippets(JNIEnv* env,
                          const JavaParamRef<jclass>& caller,
                          jboolean j_force_request) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  ntp_snippets::NTPSnippetsService* service =
      ContentSuggestionsServiceFactory::GetForProfile(profile)
          ->ntp_snippets_service();

  // Can be null if the feature has been disabled but the scheduler has not been
  // unregistered yet. The next start should unregister it.
  if (!service)
    return;

  service->FetchSnippets(j_force_request);
}

// Reschedules the fetching of snippets. Used to support different fetching
// intervals for different times of day.
static void RescheduleFetching(JNIEnv* env,
                               const JavaParamRef<jclass>& caller) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  ntp_snippets::NTPSnippetsService* service =
      ContentSuggestionsServiceFactory::GetForProfile(profile)
          ->ntp_snippets_service();

  // Can be null if the feature has been disabled but the scheduler has not been
  // unregistered yet. The next start should unregister it.
  if (!service)
    return;

  service->RescheduleFetching();
}

static void OnSuggestionTargetVisited(JNIEnv* env,
                                      const JavaParamRef<jclass>& caller,
                                      jint category,
                                      jlong visit_time_ms) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  ntp_snippets::ContentSuggestionsService* content_suggestions_service =
      ContentSuggestionsServiceFactory::GetForProfile(profile);
  ntp_snippets::metrics::OnSuggestionTargetVisited(
      content_suggestions_service->category_factory()->FromIDValue(category),
      base::TimeDelta::FromMilliseconds(visit_time_ms));
}

NTPSnippetsBridge::NTPSnippetsBridge(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_profile)
    : content_suggestions_service_observer_(this), weak_ptr_factory_(this) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  content_suggestions_service_ =
      ContentSuggestionsServiceFactory::GetForProfile(profile);
  history_service_ =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  content_suggestions_service_observer_.Add(content_suggestions_service_);
}

void NTPSnippetsBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void NTPSnippetsBridge::SetObserver(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    const JavaParamRef<jobject>& j_observer) {
  observer_.Reset(env, j_observer);
}

ScopedJavaLocalRef<jintArray> NTPSnippetsBridge::GetCategories(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::vector<int> category_ids;
  for (Category category : content_suggestions_service_->GetCategories()) {
    category_ids.push_back(category.id());
  }
  return ToJavaIntArray(env, category_ids);
}

int NTPSnippetsBridge::GetCategoryStatus(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jint category) {
  return static_cast<int>(content_suggestions_service_->GetCategoryStatus(
      CategoryFromIDValue(category)));
}

base::android::ScopedJavaLocalRef<jobject> NTPSnippetsBridge::GetCategoryInfo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint category) {
  base::Optional<CategoryInfo> info =
      content_suggestions_service_->GetCategoryInfo(
          CategoryFromIDValue(category));
  if (!info)
    return base::android::ScopedJavaLocalRef<jobject>(env, nullptr);
  return Java_SnippetsBridge_createSuggestionsCategoryInfo(
      env, ConvertUTF16ToJavaString(env, info->title()),
      static_cast<int>(info->card_layout()), info->has_more_button(),
      info->show_if_empty());
}

ScopedJavaLocalRef<jobject> NTPSnippetsBridge::GetSuggestionsForCategory(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint category) {
  // Get layout for the category.
  base::Optional<CategoryInfo> info =
      content_suggestions_service_->GetCategoryInfo(
          CategoryFromIDValue(category));
  DCHECK(info);

  const std::vector<ContentSuggestion>& suggestions =
      content_suggestions_service_->GetSuggestionsForCategory(
          CategoryFromIDValue(category));
  ScopedJavaLocalRef<jobject> result =
      Java_SnippetsBridge_createSuggestionList(env);
  for (const ContentSuggestion& suggestion : suggestions) {
    Java_SnippetsBridge_addSuggestion(
        env, result, category, ConvertUTF8ToJavaString(env, suggestion.id()),
        ConvertUTF16ToJavaString(env, suggestion.title()),
        ConvertUTF16ToJavaString(env, suggestion.publisher_name()),
        ConvertUTF16ToJavaString(env, suggestion.snippet_text()),
        ConvertUTF8ToJavaString(env, suggestion.url().spec()),
        ConvertUTF8ToJavaString(env, suggestion.amp_url().spec()),
        suggestion.publish_date().ToJavaTime(), suggestion.score(),
        static_cast<int>(info->card_layout()));
  }
  return result;
}

void NTPSnippetsBridge::FetchSuggestionImage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& suggestion_id,
    const JavaParamRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_callback);
  content_suggestions_service_->FetchSuggestionImage(
      ConvertJavaStringToUTF8(env, suggestion_id),
      base::Bind(&NTPSnippetsBridge::OnImageFetched,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void NTPSnippetsBridge::DismissSuggestion(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& suggestion_id) {
  content_suggestions_service_->DismissSuggestion(
      ConvertJavaStringToUTF8(env, suggestion_id));
}

void NTPSnippetsBridge::GetURLVisited(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      const JavaParamRef<jobject>& jcallback,
                                      const JavaParamRef<jstring>& jurl) {
  base::android::ScopedJavaGlobalRef<jobject> callback(jcallback);

  history_service_->QueryURL(
      GURL(ConvertJavaStringToUTF8(env, jurl)), false,
      base::Bind(&URLVisitedHistoryRequestCallback, callback), &tracker_);
}

void NTPSnippetsBridge::OnPageShown(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jintArray>& jcategories,
    const JavaParamRef<jintArray>& jsuggestions_per_category) {
  std::vector<int> categories_int;
  JavaIntArrayToIntVector(env, jcategories, &categories_int);
  std::vector<int> suggestions_per_category_int;
  JavaIntArrayToIntVector(env, jsuggestions_per_category,
                          &suggestions_per_category_int);
  DCHECK_EQ(categories_int.size(), suggestions_per_category_int.size());
  std::vector<std::pair<Category, int>> suggestions_per_category;
  for (size_t i = 0; i < categories_int.size(); i++) {
    suggestions_per_category.push_back(
        std::make_pair(CategoryFromIDValue(categories_int[i]),
                       suggestions_per_category_int[i]));
  }
  ntp_snippets::metrics::OnPageShown(suggestions_per_category);
  content_suggestions_service_->user_classifier()->OnNTPOpened();
}

void NTPSnippetsBridge::OnSuggestionShown(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jint global_position,
                                          jint category,
                                          jint category_position,
                                          jlong publish_timestamp_ms,
                                          jfloat score) {
  ntp_snippets::metrics::OnSuggestionShown(
      global_position, CategoryFromIDValue(category), category_position,
      TimeFromJavaTime(publish_timestamp_ms), score);
  if (global_position == 0)
    content_suggestions_service_->user_classifier()->OnSuggestionsShown();
}

void NTPSnippetsBridge::OnSuggestionOpened(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           jint global_position,
                                           jint category,
                                           jint category_position,
                                           jlong publish_timestamp_ms,
                                           jfloat score,
                                           int windowOpenDisposition) {
  ntp_snippets::metrics::OnSuggestionOpened(
      global_position, CategoryFromIDValue(category), category_position,
      TimeFromJavaTime(publish_timestamp_ms), score,
      static_cast<WindowOpenDisposition>(windowOpenDisposition));
  content_suggestions_service_->user_classifier()->OnSuggestionsUsed();
}

void NTPSnippetsBridge::OnSuggestionMenuOpened(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jint global_position,
                                               jint category,
                                               jint category_position,
                                               jlong publish_timestamp_ms,
                                               jfloat score) {
  ntp_snippets::metrics::OnSuggestionMenuOpened(
      global_position, CategoryFromIDValue(category), category_position,
      TimeFromJavaTime(publish_timestamp_ms), score);
}

void NTPSnippetsBridge::OnMoreButtonShown(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jint category,
                                          jint position) {
  ntp_snippets::metrics::OnMoreButtonShown(CategoryFromIDValue(category),
                                           position);
}

void NTPSnippetsBridge::OnMoreButtonClicked(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jint category,
                                            jint position) {
  ntp_snippets::metrics::OnMoreButtonClicked(CategoryFromIDValue(category),
                                             position);
  content_suggestions_service_->user_classifier()->OnSuggestionsUsed();
}

NTPSnippetsBridge::~NTPSnippetsBridge() {}

void NTPSnippetsBridge::OnNewSuggestions(Category category) {
  if (observer_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onNewSuggestions(env, observer_,
                                       static_cast<int>(category.id()));
}

void NTPSnippetsBridge::OnCategoryStatusChanged(Category category,
                                                CategoryStatus new_status) {
  if (observer_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onCategoryStatusChanged(env, observer_,
                                              static_cast<int>(category.id()),
                                              static_cast<int>(new_status));
}

void NTPSnippetsBridge::OnSuggestionInvalidated(
    Category category,
    const std::string& suggestion_id) {
  if (observer_.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onSuggestionInvalidated(
      env, observer_.obj(), static_cast<int>(category.id()),
      ConvertUTF8ToJavaString(env, suggestion_id).obj());
}

void NTPSnippetsBridge::ContentSuggestionsServiceShutdown() {
  observer_.Reset();
  content_suggestions_service_observer_.Remove(content_suggestions_service_);
}

void NTPSnippetsBridge::OnImageFetched(ScopedJavaGlobalRef<jobject> callback,
                                       const std::string& snippet_id,
                                       const gfx::Image& image) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty())
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());

  base::android::RunCallbackAndroid(callback, j_bitmap);
}

Category NTPSnippetsBridge::CategoryFromIDValue(jint id) {
  return content_suggestions_service_->category_factory()->FromIDValue(id);
}

// static
bool NTPSnippetsBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
