// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHADOW_H_
#define ASH_MUS_SHADOW_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class Layer;
}  // namespace ui

namespace ash {
namespace mus {

// Simple class that draws a drop shadow around content at given bounds.
// http://crbug.com/670840.
class Shadow : public ui::ImplicitAnimationObserver,
               public aura::WindowObserver {
 public:
  enum Style {
    // Active windows have more opaque shadows, shifted down to make the window
    // appear "higher".
    STYLE_ACTIVE,

    // Inactive windows have less opaque shadows.
    STYLE_INACTIVE,

    // Small windows like tooltips and context menus have lighter, smaller
    // shadows.
    STYLE_SMALL,
  };

  Shadow();
  ~Shadow() override;

  void Init(Style style);

  static Shadow* Get(aura::Window* window);

  // Returns the interio inset for the specified style. The interior inset
  // is the amount of padding added to each side of the content bounds that the
  // shadow renders into. In other words the shadow extends from all sides of
  // the layer by this value.
  static int GetInteriorInsetForStyle(Style style);

  // Returns |layer_.get()|. This is exposed so it can be added to the same
  // layer as the content and stacked below it.  SetContentBounds() should be
  // used to adjust the shadow's size and position (rather than applying
  // transformations to this layer).
  ui::Layer* layer() const { return layer_.get(); }

  const gfx::Rect& content_bounds() const { return content_bounds_; }
  Style style() const { return style_; }

  // Moves and resizes the shadow layer to frame |content_bounds|.
  void SetContentBounds(const gfx::Rect& content_bounds);

  // Sets the shadow's style, animating opacity as necessary.
  void SetStyle(Style style);

  // Installs this shadow for |window|.
  void Install(aura::Window* window);

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

 private:
  // Updates the shadow images to the current |style_|.
  void UpdateImagesForStyle();

  // Updates the shadow layer bounds based on the inteior inset and the current
  // |content_bounds_|.
  void UpdateLayerBounds();

  // WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // The current style, set when the transition animation starts.
  Style style_;

  // The parent layer of the shadow layer. It serves as a container accessible
  // from the outside to control the visibility of the shadow.
  std::unique_ptr<ui::Layer> layer_;

  // The actual shadow layer corresponding to a cc::NinePatchLayer.
  std::unique_ptr<ui::Layer> shadow_layer_;

  // Size of the current shadow image.
  gfx::Size image_size_;

  // Bounds of the content that the shadow encloses.
  gfx::Rect content_bounds_;

  // The interior inset of the shadow images. The content bounds of the image
  // grid should be set to |content_bounds_| inset by this amount.
  int interior_inset_;

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(Shadow);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_SHADOW_H_
