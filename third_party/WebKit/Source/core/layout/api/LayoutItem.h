// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutItem_h
#define LayoutItem_h

#include "core/inspector/InspectorTraceEvents.h"
#include "core/layout/LayoutObject.h"

#include "wtf/Allocator.h"

namespace blink {

class FrameView;
class LayoutAPIShim;
class LocalFrame;
class LayoutViewItem;
class Node;
class ObjectPaintProperties;

class LayoutItem {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  explicit LayoutItem(LayoutObject* layoutObject)
      : m_layoutObject(layoutObject) {}

  LayoutItem(std::nullptr_t) : m_layoutObject(0) {}

  LayoutItem() : m_layoutObject(0) {}

  // TODO(leviw): This should be "explicit operator bool", but
  // using this operator allows the API to be landed in pieces.
  // https://crbug.com/499321
  operator LayoutObject*() const { return m_layoutObject; }

  // TODO(pilgrim): Remove this when we replace the operator above with
  // operator bool.
  bool isNull() const { return !m_layoutObject; }

  String debugName() const { return m_layoutObject->debugName(); }

  bool isDescendantOf(LayoutItem item) const {
    return m_layoutObject->isDescendantOf(item.layoutObject());
  }

  bool isBoxModelObject() const { return m_layoutObject->isBoxModelObject(); }

  bool isBox() const { return m_layoutObject->isBox(); }

  bool isBR() const { return m_layoutObject->isBR(); }

  bool isLayoutBlock() const { return m_layoutObject->isLayoutBlock(); }

  bool isText() const { return m_layoutObject->isText(); }

  bool isTextControl() const { return m_layoutObject->isTextControl(); }

  bool isLayoutPart() const { return m_layoutObject->isLayoutPart(); }

  bool isEmbeddedObject() const { return m_layoutObject->isEmbeddedObject(); }

  bool isImage() const { return m_layoutObject->isImage(); }

  bool isLayoutFullScreen() const {
    return m_layoutObject->isLayoutFullScreen();
  }

  bool isListItem() const { return m_layoutObject->isListItem(); }

  bool isMedia() const { return m_layoutObject->isMedia(); }

  bool isMenuList() const { return m_layoutObject->isMenuList(); }

  bool isProgress() const { return m_layoutObject->isProgress(); }

  bool isSlider() const { return m_layoutObject->isSlider(); }

  bool isLayoutView() const { return m_layoutObject->isLayoutView(); }

  bool needsLayout() { return m_layoutObject->needsLayout(); }

  void layout() { m_layoutObject->layout(); }

  LayoutItem container() const {
    return LayoutItem(m_layoutObject->container());
  }

  Node* node() const { return m_layoutObject->node(); }

  Document& document() const { return m_layoutObject->document(); }

  LocalFrame* frame() const { return m_layoutObject->frame(); }

  LayoutItem nextInPreOrder() const {
    return LayoutItem(m_layoutObject->nextInPreOrder());
  }

  void updateStyleAndLayout() {
    return m_layoutObject->document().updateStyleAndLayout();
  }

  const ComputedStyle& styleRef() const { return m_layoutObject->styleRef(); }

  ComputedStyle* mutableStyle() const { return m_layoutObject->mutableStyle(); }

  ComputedStyle& mutableStyleRef() const {
    return m_layoutObject->mutableStyleRef();
  }

  void setStyle(PassRefPtr<ComputedStyle> style) {
    m_layoutObject->setStyle(std::move(style));
  }

  LayoutSize offsetFromContainer(const LayoutItem& item) const {
    return m_layoutObject->offsetFromContainer(item.layoutObject());
  }

  LayoutViewItem view() const;

  FrameView* frameView() const { return m_layoutObject->document().view(); }

  const ComputedStyle* style() const { return m_layoutObject->style(); }

  PaintLayer* enclosingLayer() const {
    return m_layoutObject->enclosingLayer();
  }

  bool hasLayer() const { return m_layoutObject->hasLayer(); }

  void setNeedsLayout(LayoutInvalidationReasonForTracing reason,
                      MarkingBehavior marking = MarkContainerChain,
                      SubtreeLayoutScope* scope = nullptr) {
    m_layoutObject->setNeedsLayout(reason, marking, scope);
  }

  void setNeedsLayoutAndFullPaintInvalidation(
      LayoutInvalidationReasonForTracing reason,
      MarkingBehavior behavior = MarkContainerChain,
      SubtreeLayoutScope* scope = nullptr) {
    m_layoutObject->setNeedsLayoutAndFullPaintInvalidation(reason, behavior,
                                                           scope);
  }

  void setNeedsLayoutAndPrefWidthsRecalc(
      LayoutInvalidationReasonForTracing reason) {
    m_layoutObject->setNeedsLayoutAndPrefWidthsRecalc(reason);
  }

  void setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReasonForTracing reason) {
    m_layoutObject->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        reason);
  }

  void setMayNeedPaintInvalidation() {
    m_layoutObject->setMayNeedPaintInvalidation();
  }

  void setShouldDoFullPaintInvalidation(
      PaintInvalidationReason reason = PaintInvalidationFull) {
    m_layoutObject->setShouldDoFullPaintInvalidation(reason);
  }

  void setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants() {
    m_layoutObject
        ->setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
  }

  void computeLayerHitTestRects(LayerHitTestRects& layerRects) const {
    m_layoutObject->computeLayerHitTestRects(layerRects);
  }

  FloatPoint localToAbsolute(const FloatPoint& localPoint = FloatPoint(),
                             MapCoordinatesFlags mode = 0) const {
    return m_layoutObject->localToAbsolute(localPoint, mode);
  }

  FloatQuad localToAbsoluteQuad(const FloatQuad& quad,
                                MapCoordinatesFlags mode = 0) const {
    return m_layoutObject->localToAbsoluteQuad(quad, mode);
  }

  FloatPoint absoluteToLocal(const FloatPoint& point,
                             MapCoordinatesFlags mode = 0) const {
    return m_layoutObject->absoluteToLocal(point, mode);
  }

  bool wasNotifiedOfSubtreeChange() const {
    return m_layoutObject->wasNotifiedOfSubtreeChange();
  }

  void handleSubtreeModifications() {
    m_layoutObject->handleSubtreeModifications();
  }

  bool needsOverflowRecalcAfterStyleChange() const {
    return m_layoutObject->needsOverflowRecalcAfterStyleChange();
  }

  void invalidateTreeIfNeeded(const PaintInvalidationState& state) {
    m_layoutObject->invalidateTreeIfNeeded(state);
  }

  CompositingState compositingState() const {
    return m_layoutObject->compositingState();
  }

  bool mapToVisualRectInAncestorSpace(
      const LayoutBoxModelObject* ancestor,
      LayoutRect& layoutRect,
      VisualRectFlags flags = DefaultVisualRectFlags) const {
    return m_layoutObject->mapToVisualRectInAncestorSpace(ancestor, layoutRect,
                                                          flags);
  }

  Color resolveColor(int colorProperty) const {
    return m_layoutObject->resolveColor(colorProperty);
  }

  const ObjectPaintProperties* paintProperties() const {
    return m_layoutObject->paintProperties();
  }

  void invalidatePaintRectangle(const LayoutRect& dirtyRect) const {
    m_layoutObject->invalidatePaintRectangle(dirtyRect);
  }

  PassRefPtr<ComputedStyle> getUncachedPseudoStyle(
      const PseudoStyleRequest& pseudoStyleRequest,
      const ComputedStyle* parentStyle = nullptr,
      const ComputedStyle* ownStyle = nullptr) const {
    return m_layoutObject->getUncachedPseudoStyle(pseudoStyleRequest,
                                                  parentStyle, ownStyle);
  }

 protected:
  LayoutObject* layoutObject() { return m_layoutObject; }
  const LayoutObject* layoutObject() const { return m_layoutObject; }

 private:
  LayoutObject* m_layoutObject;

  friend class LayoutAPIShim;
};

}  // namespace blink

#endif  // LayoutItem_h
