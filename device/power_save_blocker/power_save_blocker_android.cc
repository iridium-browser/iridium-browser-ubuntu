// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/power_save_blocker/power_save_blocker.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "jni/PowerSaveBlocker_jni.h"
#include "ui/android/view_android.h"

namespace device {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  Delegate(base::WeakPtr<ui::ViewAndroid> view_android,
           scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  // Does the actual work to apply or remove the desired power save block.
  void ApplyBlock();
  void RemoveBlock();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate();

  base::WeakPtr<ui::ViewAndroid> view_android_;

  base::android::ScopedJavaGlobalRef<jobject> java_power_save_blocker_;

  ui::ViewAndroid::ScopedAnchorView anchor_view_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlocker::Delegate::Delegate(
    base::WeakPtr<ui::ViewAndroid> view_android,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : view_android_(view_android), ui_task_runner_(ui_task_runner) {
  JNIEnv* env = AttachCurrentThread();
  java_power_save_blocker_.Reset(Java_PowerSaveBlocker_create(env));
}

PowerSaveBlocker::Delegate::~Delegate() {}

void PowerSaveBlocker::Delegate::ApplyBlock() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  if (!view_android_)
    return;

  JNIEnv* env = AttachCurrentThread();
  anchor_view_ = view_android_->AcquireAnchorView();
  const ScopedJavaLocalRef<jobject> popup_view = anchor_view_.view();
  if (popup_view.is_null())
    return;
  view_android_->SetAnchorRect(popup_view, gfx::RectF());
  ScopedJavaLocalRef<jobject> obj(java_power_save_blocker_);
  Java_PowerSaveBlocker_applyBlock(env, obj, popup_view);
}

void PowerSaveBlocker::Delegate::RemoveBlock() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  ScopedJavaLocalRef<jobject> obj(java_power_save_blocker_);
  Java_PowerSaveBlocker_removeBlock(AttachCurrentThread(), obj);
  anchor_view_.Reset();
}

PowerSaveBlocker::PowerSaveBlocker(
    PowerSaveBlockerType type,
    Reason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {
  // Don't support kPowerSaveBlockPreventAppSuspension
}

PowerSaveBlocker::~PowerSaveBlocker() {
  if (delegate_.get()) {
    ui_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&Delegate::RemoveBlock, delegate_));
  }
}

void PowerSaveBlocker::InitDisplaySleepBlocker(
    const base::WeakPtr<ui::ViewAndroid>& view_android) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  if (!view_android)
    return;

  delegate_ = new Delegate(view_android, ui_task_runner_);
  delegate_->ApplyBlock();
}

}  // namespace device
