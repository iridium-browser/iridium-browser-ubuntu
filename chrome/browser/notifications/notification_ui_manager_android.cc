// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_android.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/common/platform_notification_data.h"
#include "jni/NotificationUIManager_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

// Called by the Java side when a notification event has been received, but the
// NotificationUIManager has not been initialized yet. Enforce initialization of
// the class.
static void InitializeNotificationUIManager(JNIEnv* env, jclass clazz) {
  g_browser_process->notification_ui_manager();
}

// static
NotificationUIManager* NotificationUIManager::Create(PrefService* local_state) {
  return new NotificationUIManagerAndroid();
}

NotificationUIManagerAndroid::NotificationUIManagerAndroid() {
  java_object_.Reset(
      Java_NotificationUIManager_create(
          AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this),
          base::android::GetApplicationContext()));
}

NotificationUIManagerAndroid::~NotificationUIManagerAndroid() {
  Java_NotificationUIManager_destroy(AttachCurrentThread(),
                                     java_object_.obj());
}

bool NotificationUIManagerAndroid::OnNotificationClicked(
    JNIEnv* env,
    jobject java_object,
    jlong persistent_notification_id,
    jstring java_origin,
    jstring java_tag) {
  GURL origin(ConvertJavaStringToUTF8(env, java_origin));
  std::string tag = ConvertJavaStringToUTF8(env, java_tag);

  regenerated_notification_infos_[persistent_notification_id] =
      std::make_pair(origin.spec(), tag);

  // TODO(peter): Rather than assuming that the last used profile is the
  // appropriate one for this notification, the used profile should be
  // stored as part of the notification's data. See https://crbug.com/437574.
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      ProfileManager::GetLastUsedProfile(),
      persistent_notification_id,
      origin,
      -1 /* action_index */);

  return true;
}

bool NotificationUIManagerAndroid::OnNotificationClosed(
    JNIEnv* env,
    jobject java_object,
    jlong persistent_notification_id,
    jstring java_origin,
    jstring java_tag) {
  GURL origin(ConvertJavaStringToUTF8(env, java_origin));
  std::string tag = ConvertJavaStringToUTF8(env, java_tag);

  // The notification was closed by the platform, so clear all local state.
  regenerated_notification_infos_.erase(persistent_notification_id);

  // TODO(peter): Rather than assuming that the last used profile is the
  // appropriate one for this notification, the used profile should be
  // stored as part of the notification's data. See https://crbug.com/437574.
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClose(
      ProfileManager::GetLastUsedProfile(),
      persistent_notification_id,
      origin);

  return true;
}

void NotificationUIManagerAndroid::Add(const Notification& notification,
                                       Profile* profile) {
  JNIEnv* env = AttachCurrentThread();

  // The Android notification UI manager only supports Web Notifications, which
  // have a PersistentNotificationDelegate. The persistent id of the
  // notification is exposed through it's interface.
  //
  // TODO(peter): When content/ passes a message_center::Notification to the
  // chrome/ layer, the persistent notification id should be captured as a
  // property on that object instead, making this cast unnecessary.
  PersistentNotificationDelegate* delegate =
      static_cast<PersistentNotificationDelegate*>(notification.delegate());
  DCHECK(delegate);

  int64_t persistent_notification_id = delegate->persistent_notification_id();
  GURL origin_url(notification.origin_url().GetOrigin());

  ScopedJavaLocalRef<jstring> origin = ConvertUTF8ToJavaString(
      env, origin_url.spec());
  ScopedJavaLocalRef<jstring> tag =
      ConvertUTF8ToJavaString(env, notification.tag());
  ScopedJavaLocalRef<jstring> title = ConvertUTF16ToJavaString(
      env, notification.title());
  ScopedJavaLocalRef<jstring> body = ConvertUTF16ToJavaString(
      env, notification.message());

  ScopedJavaLocalRef<jobject> icon;

  SkBitmap icon_bitmap = notification.icon().AsBitmap();
  if (!icon_bitmap.isNull())
    icon = gfx::ConvertToJavaBitmap(&icon_bitmap);

  ScopedJavaLocalRef<jintArray> vibration_pattern =
      base::android::ToJavaIntArray(env, notification.vibration_pattern());

  Java_NotificationUIManager_displayNotification(
      env,
      java_object_.obj(),
      persistent_notification_id,
      origin.obj(),
      tag.obj(),
      title.obj(),
      body.obj(),
      icon.obj(),
      vibration_pattern.obj(),
      notification.silent());

  regenerated_notification_infos_[persistent_notification_id] =
      std::make_pair(origin_url.spec(), notification.tag());

  notification.delegate()->Display();
}

bool NotificationUIManagerAndroid::Update(const Notification& notification,
                                          Profile* profile) {
  NOTREACHED();
  return false;
}

const Notification* NotificationUIManagerAndroid::FindById(
    const std::string& delegate_id,
    ProfileID profile_id) const {
  NOTREACHED();
  return nullptr;
}

bool NotificationUIManagerAndroid::CancelById(const std::string& delegate_id,
                                              ProfileID profile_id) {
  int64_t persistent_notification_id = 0;

  // TODO(peter): Use the |delegate_id| directly when notification ids are being
  // generated by content/ instead of us.
  if (!base::StringToInt64(delegate_id, &persistent_notification_id))
    return false;

  const auto iterator =
      regenerated_notification_infos_.find(persistent_notification_id);
  if (iterator == regenerated_notification_infos_.end())
    return false;

  const RegeneratedNotificationInfo& notification_info = iterator->second;

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> origin =
      ConvertUTF8ToJavaString(env, notification_info.first);
  ScopedJavaLocalRef<jstring> tag =
      ConvertUTF8ToJavaString(env, notification_info.second);

  regenerated_notification_infos_.erase(iterator);

  Java_NotificationUIManager_closeNotification(env,
                                               java_object_.obj(),
                                               persistent_notification_id,
                                               origin.obj(),
                                               tag.obj());
  return true;
}

std::set<std::string>
NotificationUIManagerAndroid::GetAllIdsByProfileAndSourceOrigin(
    ProfileID profile_id,
    const GURL& source) {
  NOTREACHED();
  return std::set<std::string>();
}

std::set<std::string> NotificationUIManagerAndroid::GetAllIdsByProfile(
    ProfileID profile_id) {
  NOTREACHED();
  return std::set<std::string>();
}

bool NotificationUIManagerAndroid::CancelAllBySourceOrigin(
    const GURL& source_origin) {
  NOTREACHED();
  return false;
}

bool NotificationUIManagerAndroid::CancelAllByProfile(ProfileID profile_id) {
  NOTREACHED();
  return false;
}

void NotificationUIManagerAndroid::CancelAll() {
  NOTREACHED();
}

bool NotificationUIManagerAndroid::RegisterNotificationUIManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
