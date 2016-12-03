// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATIC_TAB_SCENE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATIC_TAB_SCENE_LAYER_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/android/compositor/scene_layer/scene_layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {
class Layer;
}

namespace chrome {
namespace android {

class ContentLayer;
class ToolbarLayer;

// A SceneLayer to render a static tab.
class StaticTabSceneLayer : public SceneLayer {
 public:
  StaticTabSceneLayer(JNIEnv* env, jobject jobj);
  ~StaticTabSceneLayer() override;

  bool ShouldShowBackground() override;
  SkColor GetBackgroundColor() override;

  // Update StaticTabSceneLayer with the new parameters.
  void UpdateTabLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jfloat content_viewport_x,
      jfloat content_viewport_y,
      jfloat content_viewport_width,
      jfloat content_viewport_height,
      const base::android::JavaParamRef<jobject>& jtab_content_manager,
      jint id,
      jint toolbar_resource_id,
      jboolean can_use_live_layer,
      jint default_background_color,
      jfloat x,
      jfloat y,
      jfloat width,
      jfloat height,
      jfloat content_offset_y,
      jfloat static_to_view_blend,
      jfloat saturation,
      jfloat brightness);

 private:
  scoped_refptr<chrome::android::ContentLayer> content_layer_;
  scoped_refptr<cc::Layer> content_scene_layer_;

  int last_set_tab_id_;
  int background_color_;
  float brightness_;

  DISALLOW_COPY_AND_ASSIGN(StaticTabSceneLayer);
};

bool RegisterStaticTabSceneLayer(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_SCENE_LAYER_STATIC_TAB_SCENE_LAYER_H_
