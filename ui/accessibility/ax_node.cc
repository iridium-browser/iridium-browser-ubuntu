// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"
#include "ui/gfx/transform.h"

namespace ui {

AXNode::AXNode(AXNode* parent, int32_t id, int32_t index_in_parent)
    : index_in_parent_(index_in_parent), parent_(parent) {
  data_.id = id;
}

AXNode::~AXNode() {
}

void AXNode::SetData(const AXNodeData& src) {
  data_ = src;
}

void AXNode::SetLocation(int offset_container_id,
                         const gfx::RectF& location,
                         gfx::Transform* transform) {
  data_.offset_container_id = offset_container_id;
  data_.location = location;
  if (transform)
    data_.transform.reset(new gfx::Transform(*transform));
  else
    data_.transform.reset(nullptr);
}

void AXNode::SetIndexInParent(int index_in_parent) {
  index_in_parent_ = index_in_parent;
}

void AXNode::SwapChildren(std::vector<AXNode*>& children) {
  children.swap(children_);
}

void AXNode::Destroy() {
  delete this;
}

bool AXNode::IsDescendantOf(AXNode* ancestor) {
  if (this == ancestor)
    return true;
  else if (parent())
    return parent()->IsDescendantOf(ancestor);

  return false;
}

}  // namespace ui
