// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/suggestion.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/AutofillPopupBridge_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

AutofillPopupViewAndroid::AutofillPopupViewAndroid(
    AutofillPopupController* controller)
    : controller_(controller) {}

AutofillPopupViewAndroid::~AutofillPopupViewAndroid() {}

void AutofillPopupViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = controller_->container_view();

  DCHECK(view_android);

  java_object_.Reset(Java_AutofillPopupBridge_create(
      env,
      reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject().obj(),
      view_android->GetJavaObject().obj()));

  UpdateBoundsAndRedrawPopup();
}

void AutofillPopupViewAndroid::Hide() {
  controller_ = NULL;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPopupBridge_dismiss(env, java_object_.obj());
}

void AutofillPopupViewAndroid::UpdateBoundsAndRedrawPopup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPopupBridge_setAnchorRect(
      env,
      java_object_.obj(),
      controller_->element_bounds().x(),
      controller_->element_bounds().y(),
      controller_->element_bounds().width(),
      controller_->element_bounds().height());

  size_t count = controller_->GetLineCount();
  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AutofillPopupBridge_createAutofillSuggestionArray(env, count);

  for (size_t i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jstring> value = base::android::ConvertUTF16ToJavaString(
        env, controller_->GetElidedValueAt(i));
    ScopedJavaLocalRef<jstring> label =
        base::android::ConvertUTF16ToJavaString(
            env, controller_->GetElidedLabelAt(i));
    int android_icon_id = 0;

    const autofill::Suggestion& suggestion = controller_->GetSuggestionAt(i);
    if (!suggestion.icon.empty()) {
      android_icon_id = ResourceMapper::MapFromChromiumId(
          controller_->GetIconResourceID(suggestion.icon));
    }

    Java_AutofillPopupBridge_addToAutofillSuggestionArray(
        env,
        data_array.obj(),
        i,
        value.obj(),
        label.obj(),
        android_icon_id,
        suggestion.frontend_id);
  }

  Java_AutofillPopupBridge_show(
      env, java_object_.obj(), data_array.obj(), controller_->IsRTL());
}

void AutofillPopupViewAndroid::SuggestionSelected(JNIEnv* env,
                                                  jobject obj,
                                                  jint list_index) {
  // Race: Hide() may have already run.
  if (controller_)
    controller_->AcceptSuggestion(list_index);
}

void AutofillPopupViewAndroid::PopupDismissed(JNIEnv* env, jobject obj) {
  if (controller_)
    controller_->ViewDestroyed();

  delete this;
}

void AutofillPopupViewAndroid::InvalidateRow(size_t) {}

// static
bool AutofillPopupViewAndroid::RegisterAutofillPopupViewAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
AutofillPopupView* AutofillPopupView::Create(
    AutofillPopupController* controller) {
  return new AutofillPopupViewAndroid(controller);
}

}  // namespace autofill
