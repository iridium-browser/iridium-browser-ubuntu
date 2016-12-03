// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/account_chooser_dialog_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/credential_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "jni/AccountChooserDialog_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/range/range.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

void AddElementsToJavaCredentialArray(
    JNIEnv* env,
    ScopedJavaLocalRef<jobjectArray> java_credentials_array,
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_forms,
    password_manager::CredentialType type,
    int indexStart = 0) {
  int index = indexStart;
  for (const auto& password_form : password_forms) {
    ScopedJavaLocalRef<jobject> java_credential = CreateNativeCredential(
        env, *password_form, index - indexStart, static_cast<int>(type));
    env->SetObjectArrayElement(java_credentials_array.obj(), index,
                               java_credential.obj());
    index++;
  }
}

class AvatarFetcherAndroid : public AccountAvatarFetcher {
 public:
  AvatarFetcherAndroid(
      const GURL& url,
      int index,
      const base::android::ScopedJavaGlobalRef<jobject>& java_dialog);

 private:
  ~AvatarFetcherAndroid() override = default;

  // chrome::BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  int index_;
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  DISALLOW_COPY_AND_ASSIGN(AvatarFetcherAndroid);
};

AvatarFetcherAndroid::AvatarFetcherAndroid(
    const GURL& url,
    int index,
    const base::android::ScopedJavaGlobalRef<jobject>& java_dialog)
    : AccountAvatarFetcher(url, base::WeakPtr<AccountAvatarFetcherDelegate>()),
      index_(index),
      java_dialog_(java_dialog) {}

void AvatarFetcherAndroid::OnFetchComplete(const GURL& url,
                                           const SkBitmap* bitmap) {
  if (bitmap) {
    base::android::ScopedJavaLocalRef<jobject> java_bitmap =
        gfx::ConvertToJavaBitmap(bitmap);
    Java_AccountChooserDialog_imageFetchComplete(
        AttachCurrentThread(), java_dialog_, index_, java_bitmap);
  }
  delete this;
}

void FetchAvatars(
    const base::android::ScopedJavaGlobalRef<jobject>& java_dialog,
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_forms,
    int index,
    net::URLRequestContextGetter* request_context) {
  for (const auto& password_form : password_forms) {
    if (!password_form->icon_url.is_valid())
      continue;
    // Fetcher deletes itself once fetching is finished.
    auto* fetcher =
        new AvatarFetcherAndroid(password_form->icon_url, index, java_dialog);
    fetcher->Start(request_context);
    ++index;
  }
}

};  // namespace

AccountChooserDialogAndroid::AccountChooserDialogAndroid(
    content::WebContents* web_contents,
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials,
    std::vector<std::unique_ptr<autofill::PasswordForm>> federated_credentials,
    const GURL& origin,
    const ManagePasswordsState::CredentialsCallback& callback)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      origin_(origin) {
  passwords_data_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents_));
  passwords_data_.OnRequestCredentials(
      std::move(local_credentials), std::move(federated_credentials), origin);
  passwords_data_.set_credentials_callback(callback);
}

AccountChooserDialogAndroid::~AccountChooserDialogAndroid() {}

void AccountChooserDialogAndroid::ShowDialog() {
  JNIEnv* env = AttachCurrentThread();
  bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockUser(
          ProfileSyncServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents_->GetBrowserContext())));
  base::string16 title;
  gfx::Range title_link_range = gfx::Range();
  GetAccountChooserDialogTitleTextAndLinkRange(
      is_smartlock_branding_enabled, local_credentials_forms().size() > 1,
      &title, &title_link_range);
  gfx::NativeWindow native_window = web_contents_->GetTopLevelNativeWindow();
  size_t credential_array_size =
      local_credentials_forms().size() + federation_providers_forms().size();
  ScopedJavaLocalRef<jobjectArray> java_credentials_array =
      CreateNativeCredentialArray(env, credential_array_size);
  AddElementsToJavaCredentialArray(
      env, java_credentials_array, local_credentials_forms(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  AddElementsToJavaCredentialArray(
      env, java_credentials_array, federation_providers_forms(),
      password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED,
      local_credentials_forms().size());
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_global;
  const std::string origin = password_manager::GetShownOrigin(origin_);
  base::string16 signin_button;
  if (local_credentials_forms().size() == 1) {
    signin_button =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN);
  }
  dialog_jobject_.Reset(Java_AccountChooserDialog_createAndShowAccountChooser(
      env, native_window->GetJavaObject(), reinterpret_cast<intptr_t>(this),
      java_credentials_array,
      base::android::ConvertUTF16ToJavaString(env, title),
      title_link_range.start(), title_link_range.end(),
      base::android::ConvertUTF8ToJavaString(env, origin),
      base::android::ConvertUTF16ToJavaString(env, signin_button)));
  net::URLRequestContextGetter* request_context =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetRequestContext();
  FetchAvatars(dialog_jobject_, local_credentials_forms(), 0, request_context);
  FetchAvatars(dialog_jobject_, federation_providers_forms(),
               local_credentials_forms().size(), request_context);
}

void AccountChooserDialogAndroid::OnCredentialClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint credential_item,
    jint credential_type,
    jboolean signin_button_clicked) {
  ChooseCredential(
      credential_item,
      static_cast<password_manager::CredentialType>(credential_type),
      signin_button_clicked);
}

void AccountChooserDialogAndroid::Destroy(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  delete this;
}

void AccountChooserDialogAndroid::CancelDialog(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  OnDialogCancel();
}

void AccountChooserDialogAndroid::OnLinkClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(password_manager::kPasswordManagerHelpCenterSmartLock),
      content::Referrer(), NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_initiated */));
}

void AccountChooserDialogAndroid::WebContentsDestroyed() {
  JNIEnv* env = AttachCurrentThread();
  Java_AccountChooserDialog_dismissDialog(env, dialog_jobject_);
}

void AccountChooserDialogAndroid::WasHidden() {
  // TODO(https://crbug.com/610700): once bug is fixed, this code should be
  // gone.
  OnDialogCancel();
  JNIEnv* env = AttachCurrentThread();
  Java_AccountChooserDialog_dismissDialog(env, dialog_jobject_);
}

void AccountChooserDialogAndroid::OnDialogCancel() {
  ChooseCredential(-1, password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY,
                   false /* signin_button_clicked */);
}

const std::vector<std::unique_ptr<autofill::PasswordForm>>&
AccountChooserDialogAndroid::local_credentials_forms() const {
  return passwords_data_.GetCurrentForms();
}

const std::vector<std::unique_ptr<autofill::PasswordForm>>&
AccountChooserDialogAndroid::federation_providers_forms() const {
  return passwords_data_.federation_providers_forms();
}

void AccountChooserDialogAndroid::ChooseCredential(
    size_t index,
    password_manager::CredentialType type,
    bool signin_button_clicked) {
  using namespace password_manager;
  password_manager::metrics_util::AccountChooserUserAction action;
  if (type == CredentialType::CREDENTIAL_TYPE_EMPTY) {
    passwords_data_.ChooseCredential(nullptr);
    action = metrics_util::ACCOUNT_CHOOSER_DISMISSED;
  } else {
    action = signin_button_clicked
                 ? metrics_util::ACCOUNT_CHOOSER_SIGN_IN
                 : metrics_util::ACCOUNT_CHOOSER_CREDENTIAL_CHOSEN;
    const auto& credentials_forms =
        (type == CredentialType::CREDENTIAL_TYPE_PASSWORD)
            ? local_credentials_forms()
            : federation_providers_forms();
    if (index < credentials_forms.size()) {
      passwords_data_.ChooseCredential(credentials_forms[index].get());
    }
  }

  if (local_credentials_forms().size() == 1) {
    password_manager::metrics_util::LogAccountChooserUserActionOneAccount(
        action);
  } else {
    password_manager::metrics_util::LogAccountChooserUserActionManyAccounts(
        action);
  }
}

bool RegisterAccountChooserDialogAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
