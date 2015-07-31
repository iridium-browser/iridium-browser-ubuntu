// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/android/infobars/infobar_android.h"

namespace translate {
class TranslateInfoBarDelegate;
}

class TranslateInfoBar : public InfoBarAndroid {
 public:
  explicit TranslateInfoBar(
      scoped_ptr<translate::TranslateInfoBarDelegate> delegate);
  ~TranslateInfoBar() override;

  // JNI methods specific to translate.
  void ApplyTranslateOptions(JNIEnv* env,
                             jobject obj,
                             int source_language_index,
                             int target_language_index,
                             bool always_translate,
                             bool never_translate_language,
                             bool never_translate_site);

 private:
  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void ProcessButton(int action, const std::string& action_value) override;
  void PassJavaInfoBar(InfoBarAndroid* source) override;
  void SetJavaInfoBar(
      const base::android::JavaRef<jobject>& java_info_bar) override;

  void TransferOwnership(TranslateInfoBar* destination,
                         translate::TranslateStep new_type);
  bool ShouldDisplayNeverTranslateInfoBarOnCancel();

  translate::TranslateInfoBarDelegate* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBar);
};

// Registers the native methods through JNI.
bool RegisterTranslateInfoBarDelegate(JNIEnv* env);

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_TRANSLATE_INFOBAR_H_
