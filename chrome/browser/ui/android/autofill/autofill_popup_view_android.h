// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_POPUP_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_POPUP_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"

namespace gfx {
class Rect;
}

namespace autofill {

class AutofillPopupController;

class AutofillPopupViewAndroid : public AutofillPopupView {
 public:
  explicit AutofillPopupViewAndroid(AutofillPopupController* controller);

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
  // Called when an autofill item was selected.
  void SuggestionSelected(JNIEnv* env, jobject obj, jint list_index);

  void DeletionRequested(JNIEnv* env, jobject obj, jint list_index);

  void DeletionConfirmed(JNIEnv* env, jobject obj);

  void PopupDismissed(JNIEnv* env, jobject obj);

  static bool RegisterAutofillPopupViewAndroid(JNIEnv* env);

 protected:
  // AutofillPopupView implementation.
  void Show() override;
  void Hide() override;
  void InvalidateRow(size_t row) override;
  void UpdateBoundsAndRedrawPopup() override;

 private:
  ~AutofillPopupViewAndroid() override;

  AutofillPopupController* controller_;  // weak.

  // The index of the last item the user long-pressed (they will be shown a
  // confirmation dialog).
  int deleting_index_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_POPUP_VIEW_ANDROID_H_
