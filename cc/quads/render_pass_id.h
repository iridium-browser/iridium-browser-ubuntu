// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_RENDER_PASS_ID_H_
#define CC_QUADS_RENDER_PASS_ID_H_

#include <stddef.h>
#include <stdint.h>

#include <tuple>

#include "base/hash.h"
#include "cc/base/cc_export.h"

namespace cc {

class CC_EXPORT RenderPassId {
 public:
  int layer_id;
  uint32_t index;

  RenderPassId() : layer_id(-1), index(0) {}
  RenderPassId(int layer_id, uint32_t index)
      : layer_id(layer_id), index(index) {}
  void* AsTracingId() const;

  bool IsValid() const { return layer_id >= 0; }

  bool operator==(const RenderPassId& other) const {
    return layer_id == other.layer_id && index == other.index;
  }
  bool operator!=(const RenderPassId& other) const { return !(*this == other); }
  bool operator<(const RenderPassId& other) const {
    return std::tie(layer_id, index) < std::tie(other.layer_id, other.index);
  }
};

struct RenderPassIdHash {
  size_t operator()(RenderPassId key) const {
    return base::HashInts(key.layer_id, static_cast<int>(key.index));
  }
};

}  // namespace cc

#endif  // CC_QUADS_RENDER_PASS_ID_H_
