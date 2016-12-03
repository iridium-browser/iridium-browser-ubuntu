// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTENT_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTENT_LAYER_H_

#include <memory>

#include "base/macros.h"
#include "cc/output/filter_operations.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class Layer;
}

namespace chrome {
namespace android {

class TabContentManager;
class ThumbnailLayer;

// Sub layer tree representation of the contents of a tab.
// Contains logic to temporarily display a static thumbnail
// when the content layer is not available.
// To specialize call SetProperties.
class ContentLayer : public Layer {
 public:
  static scoped_refptr<ContentLayer> Create(
      TabContentManager* tab_content_manager);
  void SetProperties(int id,
                     bool can_use_live_layer,
                     float static_to_view_blend,
                     bool should_override_content_alpha,
                     float content_alpha_override,
                     float saturation,
                     const gfx::Rect& desired_bounds,
                     const gfx::Size& content_size);
  bool ShowingLiveLayer() { return !static_attached_ && content_attached_; }
  gfx::Size GetContentSize();

  scoped_refptr<cc::Layer> layer() override;

 protected:
  explicit ContentLayer(TabContentManager* tab_content_manager);
  ~ContentLayer() override;

 private:
  void SetContentLayer(scoped_refptr<cc::Layer> layer);
  void SetStaticLayer(scoped_refptr<ThumbnailLayer> layer);
  void ClipContentLayer(scoped_refptr<cc::Layer> content_layer,
                        gfx::Rect clipping);
  void ClipStaticLayer(scoped_refptr<ThumbnailLayer> static_layer,
                       gfx::Rect clipping);

  scoped_refptr<cc::Layer> layer_;
  scoped_refptr<ThumbnailLayer> static_layer_;
  bool content_attached_;
  bool static_attached_;

  cc::FilterOperations static_filter_operations_;
  cc::FilterOperations content_filter_operations_;

  TabContentManager* tab_content_manager_;

  DISALLOW_COPY_AND_ASSIGN(ContentLayer);
};

}  //  namespace android
}  //  namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTENT_LAYER_H_
