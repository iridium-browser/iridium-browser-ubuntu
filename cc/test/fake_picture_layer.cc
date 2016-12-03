// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer.h"

#include "cc/test/fake_picture_layer_impl.h"

namespace cc {

FakePictureLayer::FakePictureLayer(ContentLayerClient* client)
    : PictureLayer(client),
      update_count_(0),
      push_properties_count_(0),
      always_update_resources_(false),
      force_unsuitable_for_gpu_rasterization_(false) {
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakePictureLayer::FakePictureLayer(ContentLayerClient* client,
                                   std::unique_ptr<RecordingSource> source)
    : PictureLayer(client, std::move(source)),
      update_count_(0),
      push_properties_count_(0),
      always_update_resources_(false),
      force_unsuitable_for_gpu_rasterization_(false) {
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakePictureLayer::~FakePictureLayer() {}

std::unique_ptr<LayerImpl> FakePictureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  if (is_mask())
    return FakePictureLayerImpl::CreateMask(tree_impl, id());
  return FakePictureLayerImpl::Create(tree_impl, id());
}

bool FakePictureLayer::Update() {
  bool updated = PictureLayer::Update();
  update_count_++;
  return updated || always_update_resources_;
}

void FakePictureLayer::PushPropertiesTo(LayerImpl* layer) {
  PictureLayer::PushPropertiesTo(layer);
  push_properties_count_++;
}

bool FakePictureLayer::IsSuitableForGpuRasterization() const {
  if (force_unsuitable_for_gpu_rasterization_)
    return false;
  return PictureLayer::IsSuitableForGpuRasterization();
}

}  // namespace cc
