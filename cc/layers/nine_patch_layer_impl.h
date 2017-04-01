// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_NINE_PATCH_LAYER_IMPL_H_
#define CC_LAYERS_NINE_PATCH_LAYER_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/ui_resource_layer_impl.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class DictionaryValue;
}

namespace cc {

class CC_EXPORT NinePatchLayerImpl : public UIResourceLayerImpl {
 public:
  static std::unique_ptr<NinePatchLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                    int id) {
    return base::WrapUnique(new NinePatchLayerImpl(tree_impl, id));
  }
  ~NinePatchLayerImpl() override;

  // The bitmap stretches out the bounds of the layer.  The following picture
  // illustrates the parameters associated with the dimensions.
  //
  // Layer space layout
  //
  // --------------------------------
  // |         :    :               |
  // |         J    C               |
  // |         :    :               |
  // |      ------------------      |
  // |      |       :        |      |
  // |~~~I~~|  ------------  |      |
  // |      |  |          |  |      |
  // |      |  |          |  |      |
  // |~~~A~~|~~|          |~~|~B~~~~|
  // |      |  |          |  |      |
  // |      L  ------------  |      |
  // |      |       :        |      |
  // |      ---K--------------      |
  // |              D               |
  // |              :               |
  // |              :               |
  // --------------------------------
  //
  // Bitmap space layout
  //
  // ~~~~~~~~~~ W ~~~~~~~~~~
  // :     :                |
  // :     Y                |
  // :     :                |
  // :~~X~~------------     |
  // :     |          :     |
  // :     |          :     |
  // H     |          Q     |
  // :     |          :     |
  // :     ~~~~~P~~~~~      |
  // :                      |
  // :                      |
  // :                      |
  // ------------------------
  //
  // |image_bounds| = (W, H)
  // |image_aperture| = (X, Y, P, Q)
  // |border| = (A, C, A + B, C + D)
  // |occlusion_rectangle| = (I, J, K, L)
  // |fill_center| indicates whether to draw the center quad or not.
  void SetLayout(const gfx::Rect& image_aperture,
                 const gfx::Rect& border,
                 const gfx::Rect& layer_occlusion,
                 bool fill_center,
                 bool nearest_neighbor);

  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;

  void AppendQuads(RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  std::unique_ptr<base::DictionaryValue> LayerTreeAsJson() override;

 protected:
  NinePatchLayerImpl(LayerTreeImpl* tree_impl, int id);

 private:
  class Patch {
   public:
    Patch(const gfx::RectF& image_rect, const gfx::RectF& layer_rect);

    gfx::RectF image_rect;
    gfx::RectF layer_rect;
  };

  const char* LayerTypeAsString() const override;

  void CheckGeometryLimitations();

  std::vector<Patch> ComputeQuadsWithOcclusion() const;
  std::vector<Patch> ComputeQuadsWithoutOcclusion() const;

  // The transparent center region that shows the parent layer's contents in
  // image space.
  gfx::Rect image_aperture_;

  // An inset border that the patches will be mapped to.
  gfx::Rect border_;

  bool fill_center_;

  bool nearest_neighbor_;

  gfx::Rect layer_occlusion_;

  DISALLOW_COPY_AND_ASSIGN(NinePatchLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_NINE_PATCH_LAYER_IMPL_H_
