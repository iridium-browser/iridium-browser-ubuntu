// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_content_layer_impl.h"

namespace cc {

FakeContentLayerImpl::FakeContentLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    scoped_refptr<LayerImpl::SyncedScrollOffset> synced_scroll_offset)
    : TiledLayerImpl(tree_impl, id, synced_scroll_offset),
      lost_output_surface_count_(0) {
}

FakeContentLayerImpl::~FakeContentLayerImpl() {}

scoped_ptr<LayerImpl> FakeContentLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return FakeContentLayerImpl::Create(tree_impl, id(), synced_scroll_offset());
}

bool FakeContentLayerImpl::HaveResourceForTileAt(int i, int j) {
  return HasResourceIdForTileAt(i, j);
}

void FakeContentLayerImpl::ReleaseResources() {
  TiledLayerImpl::ReleaseResources();
  ++lost_output_surface_count_;
}

}  // namespace cc
