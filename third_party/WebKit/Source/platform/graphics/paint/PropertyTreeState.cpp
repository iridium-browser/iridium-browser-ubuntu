// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PropertyTreeState.h"

#include "platform/graphics/paint/GeometryMapper.h"

namespace blink {

bool PropertyTreeState::hasDirectCompositingReasons() const {
  switch (innermostNode()) {
    case Transform:
      return transform()->hasDirectCompositingReasons();
    case Clip:
      return clip()->hasDirectCompositingReasons();
    case Effect:
      return effect()->hasDirectCompositingReasons();
    default:
      return false;
  }
}

template <typename PropertyNode>
bool isAncestorOf(const PropertyNode* ancestor, const PropertyNode* child) {
  while (child && child != ancestor) {
    child = child->parent();
  }
  return child == ancestor;
}

PropertyTreeState::InnermostNode PropertyTreeState::innermostNode() const {
  // TODO(chrishtr): this is very inefficient when innermostNode() is called
  // repeatedly.
  bool clipTransformStrictAncestorOfTransform =
      m_clip->localTransformSpace() != m_transform.get() &&
      isAncestorOf<TransformPaintPropertyNode>(m_clip->localTransformSpace(),
                                               m_transform.get());
  bool effectTransformStrictAncestorOfTransform =
      m_effect->localTransformSpace() != m_transform.get() &&
      isAncestorOf<TransformPaintPropertyNode>(m_effect->localTransformSpace(),
                                               m_transform.get());

  if (!m_transform->isRoot() && clipTransformStrictAncestorOfTransform &&
      effectTransformStrictAncestorOfTransform)
    return Transform;

  bool clipAncestorOfEffect =
      isAncestorOf<ClipPaintPropertyNode>(m_clip.get(), m_effect->outputClip());

  if (!m_effect->isRoot() && clipAncestorOfEffect) {
    return Effect;
  }
  if (!m_clip->isRoot())
    return Clip;
  return None;
}

const PropertyTreeState* PropertyTreeStateIterator::next() {
  switch (m_properties.innermostNode()) {
    case PropertyTreeState::Transform:
      m_properties.setTransform(m_properties.transform()->parent());
      return &m_properties;
    case PropertyTreeState::Clip:
      m_properties.setClip(m_properties.clip()->parent());
      return &m_properties;
    case PropertyTreeState::Effect:
      m_properties.setEffect(m_properties.effect()->parent());
      return &m_properties;
    case PropertyTreeState::None:
      return nullptr;
  }
  return nullptr;
}

}  // namespace blink
