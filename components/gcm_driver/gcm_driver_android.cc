// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "jni/GCMDriver_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ToJavaArrayOfStrings;

namespace gcm {

 GCMDriverAndroid::GCMDriverAndroid() {
  JNIEnv* env = AttachCurrentThread();
  java_ref_.Reset(
      Java_GCMDriver_create(env,
                            reinterpret_cast<intptr_t>(this),
                            base::android::GetApplicationContext()));
}

GCMDriverAndroid::~GCMDriverAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_GCMDriver_destroy(env, java_ref_.obj());
}

void GCMDriverAndroid::OnRegisterFinished(JNIEnv* env,
                                          jobject obj,
                                          jstring j_app_id,
                                          jstring j_registration_id,
                                          jboolean success) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);
  std::string registration_id = ConvertJavaStringToUTF8(env, j_registration_id);
  GCMClient::Result result = success ? GCMClient::SUCCESS
                                     : GCMClient::UNKNOWN_ERROR;

  RegisterFinished(app_id, registration_id, result);
}

void GCMDriverAndroid::OnUnregisterFinished(JNIEnv* env,
                                            jobject obj,
                                            jstring j_app_id,
                                            jboolean success) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);
  GCMClient::Result result = success ? GCMClient::SUCCESS
                                     : GCMClient::UNKNOWN_ERROR;

  UnregisterFinished(app_id, result);
}

void GCMDriverAndroid::OnMessageReceived(JNIEnv* env,
                                         jobject obj,
                                         jstring j_app_id,
                                         jstring j_sender_id,
                                         jstring j_collapse_key,
                                         jobjectArray j_data_keys_and_values) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);

  GCMClient::IncomingMessage message;
  message.sender_id = ConvertJavaStringToUTF8(env, j_sender_id);
  message.collapse_key = ConvertJavaStringToUTF8(env, j_collapse_key);
  // Expand j_data_keys_and_values from array to map.
  std::vector<std::string> data_keys_and_values;
  AppendJavaStringArrayToStringVector(env,
                                      j_data_keys_and_values,
                                      &data_keys_and_values);
  for (size_t i = 0; i + 1 < data_keys_and_values.size(); i += 2) {
    message.data[data_keys_and_values[i]] = data_keys_and_values[i+1];
  }

  GetAppHandler(app_id)->OnMessage(app_id, message);
}

void GCMDriverAndroid::OnMessagesDeleted(JNIEnv* env,
                                         jobject obj,
                                         jstring j_app_id) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);

  GetAppHandler(app_id)->OnMessagesDeleted(app_id);
}

// static
bool GCMDriverAndroid::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void GCMDriverAndroid::OnSignedIn() {
}

void GCMDriverAndroid::OnSignedOut() {
}

void GCMDriverAndroid::AddConnectionObserver(GCMConnectionObserver* observer) {
}

void GCMDriverAndroid::RemoveConnectionObserver(
    GCMConnectionObserver* observer) {
}

void GCMDriverAndroid::Enable() {
}

void GCMDriverAndroid::Disable() {
}

GCMClient* GCMDriverAndroid::GetGCMClientForTesting() const {
  NOTIMPLEMENTED();
  return NULL;
}

bool GCMDriverAndroid::IsStarted() const {
  return true;
}

bool GCMDriverAndroid::IsConnected() const {
  // TODO(gcm): hook up to GCM connected status
  return true;
}

void GCMDriverAndroid::GetGCMStatistics(
    const GetGCMStatisticsCallback& callback,
    bool clear_logs) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::SetGCMRecording(const GetGCMStatisticsCallback& callback,
                                       bool recording) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::SetAccountTokens(
    const std::vector<GCMClient::AccountTokenInfo>& account_tokens) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::RemoveAccountMapping(const std::string& account_id) {
  NOTIMPLEMENTED();
}

base::Time GCMDriverAndroid::GetLastTokenFetchTime() {
  NOTIMPLEMENTED();
  return base::Time();
}

void GCMDriverAndroid::SetLastTokenFetchTime(const base::Time& time) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::WakeFromSuspendForHeartbeat(bool wake) {
}

InstanceIDStore* GCMDriverAndroid::GetInstanceIDStore() {
  // Not supported for Android.
  return NULL;
}

void GCMDriverAndroid::AddHeartbeatInterval(const std::string& scope,
                                            int interval_ms) {
}

void GCMDriverAndroid::RemoveHeartbeatInterval(const std::string& scope) {
}

GCMClient::Result GCMDriverAndroid::EnsureStarted(
    GCMClient::StartMode start_mode) {
  // TODO(johnme): Maybe we should check if GMS is available?
  return GCMClient::SUCCESS;
}

void GCMDriverAndroid::RegisterImpl(
    const std::string& app_id, const std::vector<std::string>& sender_ids) {
  DCHECK_EQ(1u, sender_ids.size());
  JNIEnv* env = AttachCurrentThread();
  Java_GCMDriver_register(
      env, java_ref_.obj(),
      ConvertUTF8ToJavaString(env, app_id).Release(),
      ConvertUTF8ToJavaString(env, sender_ids[0]).Release());
}

void GCMDriverAndroid::UnregisterImpl(const std::string& app_id) {
  NOTREACHED();
}

void GCMDriverAndroid::UnregisterWithSenderIdImpl(
    const std::string& app_id, const std::string& sender_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_GCMDriver_unregister(
      env, java_ref_.obj(),
      ConvertUTF8ToJavaString(env, app_id).Release(),
      ConvertUTF8ToJavaString(env, sender_id).Release());
}

void GCMDriverAndroid::SendImpl(const std::string& app_id,
                                const std::string& receiver_id,
                                const GCMClient::OutgoingMessage& message) {
  NOTIMPLEMENTED();
}

}  // namespace gcm
