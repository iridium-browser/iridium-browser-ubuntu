// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_tiling_client.h"

#include <limits>

#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/fake_tile_manager.h"

namespace cc {

FakePictureLayerTilingClient::FakePictureLayerTilingClient()
    : tile_manager_(new FakeTileManager(&tile_manager_client_)),
      pile_(FakePicturePileImpl::CreateInfiniteFilledPile()),
      twin_set_(nullptr),
      twin_tiling_(nullptr),
      has_valid_tile_priorities_(true) {
}

FakePictureLayerTilingClient::FakePictureLayerTilingClient(
    ResourceProvider* resource_provider)
    : resource_pool_(ResourcePool::Create(resource_provider, GL_TEXTURE_2D)),
      tile_manager_(
          new FakeTileManager(&tile_manager_client_, resource_pool_.get())),
      pile_(FakePicturePileImpl::CreateInfiniteFilledPile()),
      twin_set_(nullptr),
      twin_tiling_(nullptr),
      has_valid_tile_priorities_(true) {
}

FakePictureLayerTilingClient::~FakePictureLayerTilingClient() {
}

ScopedTilePtr FakePictureLayerTilingClient::CreateTile(float content_scale,
                                                       const gfx::Rect& rect) {
  return tile_manager_->CreateTile(tile_size_, rect, 1, 0, 0, 0);
}

void FakePictureLayerTilingClient::SetTileSize(const gfx::Size& tile_size) {
  tile_size_ = tile_size;
}

gfx::Size FakePictureLayerTilingClient::CalculateTileSize(
    const gfx::Size& /* content_bounds */) const {
  return tile_size_;
}

bool FakePictureLayerTilingClient::HasValidTilePriorities() const {
  return has_valid_tile_priorities_;
}

const Region* FakePictureLayerTilingClient::GetPendingInvalidation() {
  return &invalidation_;
}

const PictureLayerTiling*
FakePictureLayerTilingClient::GetPendingOrActiveTwinTiling(
    const PictureLayerTiling* tiling) const {
  if (!twin_set_)
    return twin_tiling_;
  for (size_t i = 0; i < twin_set_->num_tilings(); ++i) {
    if (twin_set_->tiling_at(i)->contents_scale() == tiling->contents_scale())
      return twin_set_->tiling_at(i);
  }
  return nullptr;
}

bool FakePictureLayerTilingClient::RequiresHighResToDraw() const {
  return false;
}

}  // namespace cc
