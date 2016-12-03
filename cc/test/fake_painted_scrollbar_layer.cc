// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_painted_scrollbar_layer.h"

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "cc/test/fake_scrollbar.h"

namespace cc {

scoped_refptr<FakePaintedScrollbarLayer> FakePaintedScrollbarLayer::Create(
    bool paint_during_update,
    bool has_thumb,
    int scrolling_layer_id) {
  FakeScrollbar* fake_scrollbar = new FakeScrollbar(
      paint_during_update, has_thumb, false);
  return make_scoped_refptr(
      new FakePaintedScrollbarLayer(fake_scrollbar, scrolling_layer_id));
}

FakePaintedScrollbarLayer::FakePaintedScrollbarLayer(
    FakeScrollbar* fake_scrollbar,
    int scrolling_layer_id)
    : PaintedScrollbarLayer(std::unique_ptr<Scrollbar>(fake_scrollbar),
                            scrolling_layer_id),
      update_count_(0),
      push_properties_count_(0),
      fake_scrollbar_(fake_scrollbar) {
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakePaintedScrollbarLayer::~FakePaintedScrollbarLayer() {}

bool FakePaintedScrollbarLayer::Update() {
  bool updated = PaintedScrollbarLayer::Update();
  ++update_count_;
  return updated;
}

void FakePaintedScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  PaintedScrollbarLayer::PushPropertiesTo(layer);
  ++push_properties_count_;
}

std::unique_ptr<base::AutoReset<bool>>
FakePaintedScrollbarLayer::IgnoreSetNeedsCommit() {
  return base::MakeUnique<base::AutoReset<bool>>(&ignore_set_needs_commit_,
                                                 true);
}

}  // namespace cc
