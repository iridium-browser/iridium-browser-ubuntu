// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_EDGE_EFFECT_H_
#define CONTENT_BROWSER_ANDROID_EDGE_EFFECT_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/android/edge_effect_base.h"

namespace cc {
class Layer;
}

namespace ui {
class SystemUIResourceManager;
}

namespace content {

// |EdgeEffect| mirrors its Android counterpart, EdgeEffect.java.
// Conscious tradeoffs were made to align this as closely as possible with the
// the original Android java version.
// All coordinates and dimensions are in device pixels.
class EdgeEffect : public EdgeEffectBase {
 public:
  explicit EdgeEffect(ui::SystemUIResourceManager* resource_manager,
                      float device_scale_factor);
  virtual ~EdgeEffect();

  virtual void Pull(base::TimeTicks current_time,
                    float delta_distance,
                    float displacement) OVERRIDE;
  virtual void Absorb(base::TimeTicks current_time, float velocity) OVERRIDE;
  virtual bool Update(base::TimeTicks current_time) OVERRIDE;
  virtual void Release(base::TimeTicks current_time) OVERRIDE;

  virtual void Finish() OVERRIDE;
  virtual bool IsFinished() const OVERRIDE;

  virtual void ApplyToLayers(const gfx::SizeF& size,
                             const gfx::Transform& transform) OVERRIDE;
  virtual void SetParent(cc::Layer* parent) OVERRIDE;

  // Thread-safe trigger to load resources.
  static void PreloadResources(ui::SystemUIResourceManager* resource_manager);

 private:
  class EffectLayer;
  scoped_ptr<EffectLayer> edge_;
  scoped_ptr<EffectLayer> glow_;

  float base_edge_height_;
  float base_glow_height_;

  float edge_alpha_;
  float edge_scale_y_;
  float glow_alpha_;
  float glow_scale_y_;

  float edge_alpha_start_;
  float edge_alpha_finish_;
  float edge_scale_y_start_;
  float edge_scale_y_finish_;
  float glow_alpha_start_;
  float glow_alpha_finish_;
  float glow_scale_y_start_;
  float glow_scale_y_finish_;

  base::TimeTicks start_time_;
  base::TimeDelta duration_;

  State state_;

  float pull_distance_;

  DISALLOW_COPY_AND_ASSIGN(EdgeEffect);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_EDGE_EFFECT_H_
