/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutObject_h
#define LayoutObject_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentLifecycle.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/layout/LayoutObjectChildList.h"
#include "core/layout/MapCoordinatesFlags.h"
#include "core/layout/PaintInvalidationState.h"
#include "core/layout/ScrollAlignment.h"
#include "core/layout/SubtreeLayoutScope.h"
#include "core/layout/api/HitTestAction.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/compositing/CompositingState.h"
#include "core/loader/resource/ImageResourceObserver.h"
#include "core/paint/LayerHitTestRects.h"
#include "core/paint/PaintPhase.h"
#include "core/style/ComputedStyle.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/AutoReset.h"

namespace blink {

class AffineTransform;
class Cursor;
class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class InlineBox;
class LayoutBoxModelObject;
class LayoutBlock;
class LayoutFlowThread;
class LayoutGeometryMap;
class LayoutMultiColumnSpannerPlaceholder;
class LayoutView;
class ObjectPaintProperties;
class PaintLayer;
class PseudoStyleRequest;
class TransformState;

struct PaintInfo;
struct PaintInvalidatorContext;

enum CursorDirective { SetCursorBasedOnStyle, SetCursor, DoNotSetCursor };

enum HitTestFilter { HitTestAll, HitTestSelf, HitTestDescendants };

enum MarkingBehavior {
  MarkOnlyThis,
  MarkContainerChain,
};

enum ScheduleRelayoutBehavior { ScheduleRelayout, DontScheduleRelayout };

const LayoutUnit& caretWidth();

struct AnnotatedRegionValue {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  bool operator==(const AnnotatedRegionValue& o) const {
    return draggable == o.draggable && bounds == o.bounds;
  }

  LayoutRect bounds;
  bool draggable;
};

#ifndef NDEBUG
const int showTreeCharacterOffset = 39;
#endif

// LayoutObject is the base class for all layout tree objects.
//
// LayoutObjects form a tree structure that is a close mapping of the DOM tree.
// The root of the LayoutObject tree is the LayoutView, which is the
// LayoutObject associated with the Document.
//
// Some LayoutObjects don't have an associated Node and are called "anonymous"
// (see the constructor below). Anonymous LayoutObjects exist for several
// purposes but are usually required by CSS. A good example is anonymous table
// parts (see LayoutTable for the expected structure). Anonymous LayoutObjects
// are generated when a new child is added to the tree in addChild(). See the
// function for some important information on this.
//
// Also some Node don't have an associated LayoutObjects e.g. if display: none
// or display: contents is set. For more detail, see LayoutObject::createObject
// that creates the right LayoutObject based on the style.
//
// Because the SVG and CSS classes both inherit from this object, functions can
// belong to either realm and sometimes to both.
//
// The purpose of the layout tree is to do layout (aka reflow) and store its
// results for painting and hit-testing. Layout is the process of sizing and
// positioning Nodes on the page. In Blink, layouts always start from a relayout
// boundary (see objectIsRelayoutBoundary in LayoutObject.cpp). As such, we
// need to mark the ancestors all the way to the enclosing relayout boundary in
// order to do a correct layout.
//
// Due to the high cost of layout, a lot of effort is done to avoid doing full
// layouts of nodes. This is why there are several types of layout available to
// bypass the complex operations. See the comments on the layout booleans in
// LayoutObjectBitfields below about the different layouts.
//
// To save memory, especially for the common child class LayoutText,
// LayoutObject doesn't provide storage for children. Descendant classes that do
// allow children have to have a LayoutObjectChildList member that stores the
// actual children and override virtualChildren().
//
// LayoutObject is an ImageResourceObserver, which means that it gets notified
// when associated images are changed. This is used for 2 main use cases:
// - reply to 'background-image' as we need to invalidate the background in this
//   case.
//   (See https://drafts.csswg.org/css-backgrounds-3/#the-background-image)
// - image (LayoutImage, LayoutSVGImage) or video (LayoutVideo) objects that are
//   placeholders for displaying them.
//
//
// ***** LIFETIME *****
//
// LayoutObjects are fully owned by their associated DOM node. In other words,
// it's the DOM node's responsibility to free its LayoutObject, this is why
// LayoutObjects are not and SHOULD NOT be RefCounted.
//
// LayoutObjects are created during the DOM attachment. This phase computes
// the style and create the LayoutObject associated with the Node (see
// Node::attachLayoutTree). LayoutObjects are destructed during detachment (see
// Node::detachLayoutTree), which can happen when the DOM node is removed from
// the
// DOM tree, during page tear down or when the style is changed to contain
// 'display: none'.
//
// Anonymous LayoutObjects are owned by their enclosing DOM node. This means
// that if the DOM node is detached, it has to destroy any anonymous
// descendants. This is done in LayoutObject::destroy().
//
// Note that for correctness, destroy() is expected to clean any anonymous
// wrappers as sequences of insertion / removal could make them visible to
// the page. This is done by LayoutObject::destroyAndCleanupAnonymousWrappers()
// which is the preferred way to destroy an object.
//
//
// ***** INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS *****
// The preferred logical widths are the intrinsic sizes of this element
// (https://drafts.csswg.org/css-sizing-3/#intrinsic). Intrinsic sizes depend
// mostly on the content and a limited set of style properties (e.g. any
// font-related property for text, 'min-width'/'max-width',
// 'min-height'/'max-height').
//
// Those widths are used to determine the final layout logical width, which
// depends on the layout algorithm used and the available logical width.
//
// LayoutObject only has getters for the widths (minPreferredLogicalWidth and
// maxPreferredLogicalWidth). However the storage for them is in LayoutBox
// (see m_minPreferredLogicalWidth and m_maxPreferredLogicalWidth). This is
// because only boxes implementing the full box model have a need for them.
// Because LayoutBlockFlow's intrinsic widths rely on the underlying text
// content, LayoutBlockFlow may call LayoutText::computePreferredLogicalWidths.
//
// The 2 widths are computed lazily during layout when the getters are called.
// The computation is done by calling computePreferredLogicalWidths() behind the
// scene. The boolean used to control the lazy recomputation is
// preferredLogicalWidthsDirty.
//
// See the individual getters below for more details about what each width is.
class CORE_EXPORT LayoutObject : public ImageResourceObserver,
                                 public DisplayItemClient {
  friend class LayoutObjectChildList;
  FRIEND_TEST_ALL_PREFIXES(LayoutObjectTest, MutableForPaintingClearPaintFlags);
  friend class VisualRectMappingTest;
  WTF_MAKE_NONCOPYABLE(LayoutObject);

 public:
  // Anonymous objects should pass the document as their node, and they will
  // then automatically be marked as anonymous in the constructor.
  explicit LayoutObject(Node*);
  ~LayoutObject() override;

  // Returns the name of the layout object.
  virtual const char* name() const = 0;

  // Returns the decorated name used by run-layout-tests. The name contains the
  // name of the object along with extra information about the layout object
  // state (e.g. positioning).
  String decoratedName() const;

  // DisplayItemClient methods.
  LayoutRect visualRect() const override;
  String debugName() const final;

  LayoutObject* parent() const { return m_parent; }
  bool isDescendantOf(const LayoutObject*) const;

  LayoutObject* previousSibling() const { return m_previous; }
  LayoutObject* nextSibling() const { return m_next; }

  DISABLE_CFI_PERF
  LayoutObject* slowFirstChild() const {
    if (const LayoutObjectChildList* children = virtualChildren())
      return children->firstChild();
    return nullptr;
  }
  LayoutObject* slowLastChild() const {
    if (const LayoutObjectChildList* children = virtualChildren())
      return children->lastChild();
    return nullptr;
  }

  // See comment in the class description as to why there is no child.
  virtual LayoutObjectChildList* virtualChildren() { return nullptr; }
  virtual const LayoutObjectChildList* virtualChildren() const {
    return nullptr;
  }

  LayoutObject* nextInPreOrder() const;
  LayoutObject* nextInPreOrder(const LayoutObject* stayWithin) const;
  LayoutObject* nextInPreOrderAfterChildren() const;
  LayoutObject* nextInPreOrderAfterChildren(
      const LayoutObject* stayWithin) const;
  LayoutObject* previousInPreOrder() const;
  LayoutObject* previousInPreOrder(const LayoutObject* stayWithin) const;
  LayoutObject* childAt(unsigned) const;

  LayoutObject* lastLeafChild() const;

  // The following functions are used when the layout tree hierarchy changes to
  // make sure layers get properly added and removed. Since containership can be
  // implemented by any subclass, and since a hierarchy can contain a mixture of
  // boxes and other object types, these functions need to be in the base class.
  PaintLayer* enclosingLayer() const;
  void addLayers(PaintLayer* parentLayer);
  void removeLayers(PaintLayer* parentLayer);
  void moveLayers(PaintLayer* oldParent, PaintLayer* newParent);
  PaintLayer* findNextLayer(PaintLayer* parentLayer,
                            LayoutObject* startPoint,
                            bool checkParent = true);

  // Returns the layer that will paint this object. If possible, use the faster
  // PaintInvalidationState::paintingLayer() instead.
  PaintLayer* paintingLayer() const;

  // Scrolling is a LayoutBox concept, however some code just cares about
  // recursively scrolling our enclosing ScrollableArea(s).
  bool scrollRectToVisible(
      const LayoutRect&,
      const ScrollAlignment& alignX = ScrollAlignment::alignCenterIfNeeded,
      const ScrollAlignment& alignY = ScrollAlignment::alignCenterIfNeeded,
      ScrollType = ProgrammaticScroll,
      bool makeVisibleInVisualViewport = true);

  // Convenience function for getting to the nearest enclosing box of a
  // LayoutObject.
  LayoutBox* enclosingBox() const;
  LayoutBoxModelObject* enclosingBoxModelObject() const;

  LayoutBox* enclosingScrollableBox() const;

  // Function to return our enclosing flow thread if we are contained inside
  // one. This function follows the containing block chain.
  LayoutFlowThread* flowThreadContainingBlock() const {
    if (!isInsideFlowThread())
      return nullptr;
    return locateFlowThreadContainingBlock();
  }

#if DCHECK_IS_ON()
  void setHasAXObject(bool flag) { m_hasAXObject = flag; }
  bool hasAXObject() const { return m_hasAXObject; }

  // Helper class forbidding calls to setNeedsLayout() during its lifetime.
  class SetLayoutNeededForbiddenScope {
   public:
    explicit SetLayoutNeededForbiddenScope(LayoutObject&);
    ~SetLayoutNeededForbiddenScope();

   private:
    LayoutObject& m_layoutObject;
    bool m_preexistingForbidden;
  };

  void assertLaidOut() const {
#ifndef NDEBUG
    if (needsLayout())
      showLayoutTreeForThis();
#endif
    SECURITY_DCHECK(!needsLayout());
  }

  void assertSubtreeIsLaidOut() const {
    for (const LayoutObject* layoutObject = this; layoutObject;
         layoutObject = layoutObject->nextInPreOrder())
      layoutObject->assertLaidOut();
  }

  void assertClearedPaintInvalidationFlags() const {
#ifndef NDEBUG
    if (paintInvalidationStateIsDirty()) {
      showLayoutTreeForThis();
      ASSERT_NOT_REACHED();
    }
#endif
  }

  void assertSubtreeClearedPaintInvalidationFlags() const {
    for (const LayoutObject* layoutObject = this; layoutObject;
         layoutObject = layoutObject->nextInPreOrder())
      layoutObject->assertClearedPaintInvalidationFlags();
  }

#endif

  // LayoutObject tree manipulation
  //////////////////////////////////////////
  DISABLE_CFI_PERF virtual bool canHaveChildren() const {
    return virtualChildren();
  }
  virtual bool isChildAllowed(LayoutObject*, const ComputedStyle&) const {
    return true;
  }

  // This function is called whenever a child is inserted under |this|.
  //
  // The main purpose of this function is to generate a consistent layout
  // tree, which means generating the missing anonymous objects. Most of the
  // time there'll be no anonymous objects to generate.
  //
  // The following invariants are true on the input:
  // - |newChild->node()| is a child of |this->node()|, if |this| is not
  //   anonymous. If |this| is anonymous, the invariant holds with the
  //   enclosing non-anonymous LayoutObject.
  // - |beforeChild->node()| (if |beforeChild| is provided and not anonymous)
  //   is a sibling of |newChild->node()| (if |newChild| is not anonymous).
  //
  // The reason for these invariants is that insertions are performed on the
  // DOM tree. Because the layout tree may insert extra anonymous renderers,
  // the previous invariants are only guaranteed for the DOM tree. In
  // particular, |beforeChild| may not be a direct child when it's wrapped in
  // anonymous wrappers.
  //
  // Classes inserting anonymous LayoutObjects in the tree are expected to
  // check for the anonymous wrapper case with:
  //                    beforeChild->parent() != this
  //
  // The usage of |child/parent/sibling| in this comment actually means
  // |child/parent/sibling| in a flat tree because a layout tree is generated
  // from a structure of a flat tree if Shadow DOM is used.
  // See LayoutTreeBuilderTraversal and FlatTreeTraversal.
  //
  // See LayoutTable::addChild and LayoutBlock::addChild.
  // TODO(jchaffraix): |newChild| cannot be nullptr and should be a reference.
  virtual void addChild(LayoutObject* newChild,
                        LayoutObject* beforeChild = nullptr);
  virtual void addChildIgnoringContinuation(
      LayoutObject* newChild,
      LayoutObject* beforeChild = nullptr) {
    return addChild(newChild, beforeChild);
  }
  virtual void removeChild(LayoutObject*);
  virtual bool createsAnonymousWrapper() const { return false; }
  //////////////////////////////////////////

  // Sets the parent of this object but doesn't add it as a child of the parent.
  void setDangerousOneWayParent(LayoutObject*);

  // For SlimmingPaintInvalidation/SPv2 only.
  // The ObjectPaintProperties structure holds references to the property tree
  // nodes that are created by the layout object for painting. The property
  // nodes are only updated during InPrePaint phase of the document lifecycle
  // and shall remain immutable during other phases.
  const ObjectPaintProperties* paintProperties() const;

 private:
  ObjectPaintProperties& ensurePaintProperties();

 private:
  //////////////////////////////////////////
  // Helper functions. Dangerous to use!
  void setPreviousSibling(LayoutObject* previous) { m_previous = previous; }
  void setNextSibling(LayoutObject* next) { m_next = next; }
  void setParent(LayoutObject* parent) {
    m_parent = parent;

    // Only update if our flow thread state is different from our new parent and
    // if we're not a LayoutFlowThread.
    // A LayoutFlowThread is always considered to be inside itself, so it never
    // has to change its state in response to parent changes.
    bool insideFlowThread = parent && parent->isInsideFlowThread();
    if (insideFlowThread != isInsideFlowThread() && !isLayoutFlowThread())
      setIsInsideFlowThreadIncludingDescendants(insideFlowThread);
  }

  //////////////////////////////////////////
 private:
#if DCHECK_IS_ON()
  bool isSetNeedsLayoutForbidden() const { return m_setNeedsLayoutForbidden; }
  void setNeedsLayoutIsForbidden(bool flag) {
    m_setNeedsLayoutForbidden = flag;
  }
#endif

  void addAbsoluteRectForLayer(IntRect& result);
  bool requiresAnonymousTableWrappers(const LayoutObject*) const;

  // Gets pseudoStyle from Shadow host(in case of input elements)
  // or from Parent element.
  PassRefPtr<ComputedStyle> getUncachedPseudoStyleFromParentOrShadowHost()
      const;

 public:
#ifndef NDEBUG
  void showTreeForThis() const;
  void showLayoutTreeForThis() const;
  void showLineTreeForThis() const;

  void showLayoutObject() const;
  // We don't make stringBuilder an optional parameter so that
  // showLayoutObject can be called from gdb easily.
  void showLayoutObject(StringBuilder&) const;
  void showLayoutTreeAndMark(const LayoutObject* markedObject1 = nullptr,
                             const char* markedLabel1 = nullptr,
                             const LayoutObject* markedObject2 = nullptr,
                             const char* markedLabel2 = nullptr,
                             unsigned depth = 0) const;
#endif

  // This function is used to create the appropriate LayoutObject based
  // on the style, in particular 'display' and 'content'.
  // "display: none" or "display: contents" are the only times this function
  // will return nullptr.
  //
  // For renderer creation, the inline-* values create the same renderer
  // as the non-inline version. The difference is that inline-* sets
  // m_isInline during initialization. This means that
  // "display: inline-table" creates a LayoutTable, like "display: table".
  //
  // Ideally every Element::createLayoutObject would call this function to
  // respond to 'display' but there are deep rooted assumptions about
  // which LayoutObject is created on a fair number of Elements. This
  // function also doesn't handle the default association between a tag
  // and its renderer (e.g. <iframe> creates a LayoutIFrame even if the
  // initial 'display' value is inline).
  static LayoutObject* createObject(Element*, const ComputedStyle&);

  // LayoutObjects are allocated out of the rendering partition.
  void* operator new(size_t);
  void operator delete(void*);

  bool isPseudoElement() const { return node() && node()->isPseudoElement(); }

  virtual bool isBoxModelObject() const { return false; }
  bool isBR() const { return isOfType(LayoutObjectBr); }
  bool isCanvas() const { return isOfType(LayoutObjectCanvas); }
  bool isCounter() const { return isOfType(LayoutObjectCounter); }
  bool isDetailsMarker() const { return isOfType(LayoutObjectDetailsMarker); }
  bool isEmbeddedObject() const { return isOfType(LayoutObjectEmbeddedObject); }
  bool isFieldset() const { return isOfType(LayoutObjectFieldset); }
  bool isFileUploadControl() const {
    return isOfType(LayoutObjectFileUploadControl);
  }
  bool isFrame() const { return isOfType(LayoutObjectFrame); }
  bool isFrameSet() const { return isOfType(LayoutObjectFrameSet); }
  bool isLayoutNGBlockFlow() const { return isOfType(LayoutObjectNGBlockFlow); }
  bool isLayoutTableCol() const { return isOfType(LayoutObjectLayoutTableCol); }
  bool isListBox() const { return isOfType(LayoutObjectListBox); }
  bool isListItem() const { return isOfType(LayoutObjectListItem); }
  bool isListMarker() const { return isOfType(LayoutObjectListMarker); }
  bool isMedia() const { return isOfType(LayoutObjectMedia); }
  bool isMenuList() const { return isOfType(LayoutObjectMenuList); }
  bool isProgress() const { return isOfType(LayoutObjectProgress); }
  bool isQuote() const { return isOfType(LayoutObjectQuote); }
  bool isLayoutButton() const { return isOfType(LayoutObjectLayoutButton); }
  bool isLayoutFullScreen() const {
    return isOfType(LayoutObjectLayoutFullScreen);
  }
  bool isLayoutFullScreenPlaceholder() const {
    return isOfType(LayoutObjectLayoutFullScreenPlaceholder);
  }
  bool isLayoutGrid() const { return isOfType(LayoutObjectLayoutGrid); }
  bool isLayoutIFrame() const { return isOfType(LayoutObjectLayoutIFrame); }
  bool isLayoutImage() const { return isOfType(LayoutObjectLayoutImage); }
  bool isLayoutMultiColumnSet() const {
    return isOfType(LayoutObjectLayoutMultiColumnSet);
  }
  bool isLayoutMultiColumnSpannerPlaceholder() const {
    return isOfType(LayoutObjectLayoutMultiColumnSpannerPlaceholder);
  }
  bool isLayoutScrollbarPart() const {
    return isOfType(LayoutObjectLayoutScrollbarPart);
  }
  bool isLayoutView() const { return isOfType(LayoutObjectLayoutView); }
  bool isRuby() const { return isOfType(LayoutObjectRuby); }
  bool isRubyBase() const { return isOfType(LayoutObjectRubyBase); }
  bool isRubyRun() const { return isOfType(LayoutObjectRubyRun); }
  bool isRubyText() const { return isOfType(LayoutObjectRubyText); }
  bool isSlider() const { return isOfType(LayoutObjectSlider); }
  bool isSliderThumb() const { return isOfType(LayoutObjectSliderThumb); }
  bool isTable() const { return isOfType(LayoutObjectTable); }
  bool isTableCaption() const { return isOfType(LayoutObjectTableCaption); }
  bool isTableCell() const { return isOfType(LayoutObjectTableCell); }
  bool isTableRow() const { return isOfType(LayoutObjectTableRow); }
  bool isTableSection() const { return isOfType(LayoutObjectTableSection); }
  bool isTextArea() const { return isOfType(LayoutObjectTextArea); }
  bool isTextControl() const { return isOfType(LayoutObjectTextControl); }
  bool isTextField() const { return isOfType(LayoutObjectTextField); }
  bool isVideo() const { return isOfType(LayoutObjectVideo); }
  bool isWidget() const { return isOfType(LayoutObjectWidget); }

  virtual bool isImage() const { return false; }

  virtual bool isInlineBlockOrInlineTable() const { return false; }
  virtual bool isLayoutBlock() const { return false; }
  virtual bool isLayoutBlockFlow() const { return false; }
  virtual bool isLayoutFlowThread() const { return false; }
  virtual bool isLayoutInline() const { return false; }
  virtual bool isLayoutPart() const { return false; }

  bool isDocumentElement() const {
    return document().documentElement() == m_node;
  }
  // isBody is called from LayoutBox::styleWillChange and is thus quite hot.
  bool isBody() const {
    return node() && node()->hasTagName(HTMLNames::bodyTag);
  }
  bool isHR() const;
  bool isLegend() const;

  bool isTablePart() const {
    return isTableCell() || isLayoutTableCol() || isTableCaption() ||
           isTableRow() || isTableSection();
  }

  inline bool isBeforeContent() const;
  inline bool isAfterContent() const;
  inline bool isBeforeOrAfterContent() const;
  static inline bool isAfterContent(const LayoutObject* obj) {
    return obj && obj->isAfterContent();
  }

  bool hasCounterNodeMap() const { return m_bitfields.hasCounterNodeMap(); }
  void setHasCounterNodeMap(bool hasCounterNodeMap) {
    m_bitfields.setHasCounterNodeMap(hasCounterNodeMap);
  }

  bool everHadLayout() const { return m_bitfields.everHadLayout(); }

  bool childrenInline() const { return m_bitfields.childrenInline(); }
  void setChildrenInline(bool b) { m_bitfields.setChildrenInline(b); }

  bool alwaysCreateLineBoxesForLayoutInline() const {
    ASSERT(isLayoutInline());
    return m_bitfields.alwaysCreateLineBoxesForLayoutInline();
  }
  void setAlwaysCreateLineBoxesForLayoutInline(bool alwaysCreateLineBoxes) {
    ASSERT(isLayoutInline());
    m_bitfields.setAlwaysCreateLineBoxesForLayoutInline(alwaysCreateLineBoxes);
  }

  bool ancestorLineBoxDirty() const {
    return m_bitfields.ancestorLineBoxDirty();
  }
  void setAncestorLineBoxDirty(bool value = true) {
    m_bitfields.setAncestorLineBoxDirty(value);
    if (value)
      setNeedsLayoutAndFullPaintInvalidation(
          LayoutInvalidationReason::LineBoxesChanged);
  }

  void setIsInsideFlowThreadIncludingDescendants(bool);

  bool isInsideFlowThread() const { return m_bitfields.isInsideFlowThread(); }
  void setIsInsideFlowThread(bool insideFlowThread) {
    m_bitfields.setIsInsideFlowThread(insideFlowThread);
  }

  // FIXME: Until all SVG layoutObjects can be subclasses of
  // LayoutSVGModelObject we have to add SVG layoutObject methods to
  // LayoutObject with an ASSERT_NOT_REACHED() default implementation.
  bool isSVG() const { return isOfType(LayoutObjectSVG); }
  bool isSVGRoot() const { return isOfType(LayoutObjectSVGRoot); }
  bool isSVGChild() const { return isSVG() && !isSVGRoot(); }
  bool isSVGContainer() const { return isOfType(LayoutObjectSVGContainer); }
  bool isSVGTransformableContainer() const {
    return isOfType(LayoutObjectSVGTransformableContainer);
  }
  bool isSVGViewportContainer() const {
    return isOfType(LayoutObjectSVGViewportContainer);
  }
  bool isSVGGradientStop() const {
    return isOfType(LayoutObjectSVGGradientStop);
  }
  bool isSVGHiddenContainer() const {
    return isOfType(LayoutObjectSVGHiddenContainer);
  }
  bool isSVGShape() const { return isOfType(LayoutObjectSVGShape); }
  bool isSVGText() const { return isOfType(LayoutObjectSVGText); }
  bool isSVGTextPath() const { return isOfType(LayoutObjectSVGTextPath); }
  bool isSVGInline() const { return isOfType(LayoutObjectSVGInline); }
  bool isSVGInlineText() const { return isOfType(LayoutObjectSVGInlineText); }
  bool isSVGImage() const { return isOfType(LayoutObjectSVGImage); }
  bool isSVGForeignObject() const {
    return isOfType(LayoutObjectSVGForeignObject);
  }
  bool isSVGResourceContainer() const {
    return isOfType(LayoutObjectSVGResourceContainer);
  }
  bool isSVGResourceFilter() const {
    return isOfType(LayoutObjectSVGResourceFilter);
  }
  bool isSVGResourceFilterPrimitive() const {
    return isOfType(LayoutObjectSVGResourceFilterPrimitive);
  }

  // FIXME: Those belong into a SVG specific base-class for all layoutObjects
  // (see above). Unfortunately we don't have such a class yet, because it's not
  // possible for all layoutObjects to inherit from LayoutSVGObject ->
  // LayoutObject (some need LayoutBlock inheritance for instance)
  virtual void setNeedsTransformUpdate() {}
  virtual void setNeedsBoundariesUpdate();

  bool isBlendingAllowed() const {
    return !isSVG() || (isSVGContainer() && !isSVGHiddenContainer()) ||
           isSVGShape() || isSVGImage() || isSVGText();
  }
  virtual bool hasNonIsolatedBlendingDescendants() const {
    // This is only implemented for layout objects that containt SVG flow.
    // For HTML/CSS layout objects, use the PaintLayer version instead.
    DCHECK(isSVG());
    return false;
  }
  enum DescendantIsolationState {
    DescendantIsolationRequired,
    DescendantIsolationNeedsUpdate,
  };
  virtual void descendantIsolationRequirementsChanged(
      DescendantIsolationState) {}

  // Per SVG 1.1 objectBoundingBox ignores clipping, masking, filter effects,
  // opacity and stroke-width.
  // This is used for all computation of objectBoundingBox relative units and by
  // SVGGraphicsElement::getBBox().
  // NOTE: Markers are not specifically ignored here by SVG 1.1 spec, but we
  // ignore them since stroke-width is ignored (and marker size can depend on
  // stroke-width). objectBoundingBox is returned local coordinates.
  // The name objectBoundingBox is taken from the SVG 1.1 spec.
  virtual FloatRect objectBoundingBox() const;
  virtual FloatRect strokeBoundingBox() const;

  // Returns the smallest rectangle enclosing all of the painted content
  // respecting clipping, masking, filters, opacity, stroke-width and markers.
  // The local SVG coordinate space is the space where localSVGTransform
  // applies. For SVG objects defining viewports (e.g.
  // LayoutSVGViewportContainer and  LayoutSVGResourceMarker), the local SVG
  // coordinate space is the viewport space.
  virtual FloatRect visualRectInLocalSVGCoordinates() const;

  // This returns the transform applying to the local SVG coordinate space,
  // which combines the CSS transform properties and animation motion transform.
  // See SVGElement::calculateTransform().
  // Most callsites want localToSVGParentTransform() instead.
  virtual AffineTransform localSVGTransform() const;

  // Returns the full transform mapping from local coordinates to parent's local
  // coordinates. For most SVG objects, this is the same as localSVGTransform.
  // For SVG objects defining viewports (see visualRectInLocalSVGCoordinates),
  // this includes any viewport transforms and x/y offsets as well as
  // localSVGTransform.
  virtual AffineTransform localToSVGParentTransform() const {
    return localSVGTransform();
  }

  // SVG uses FloatPoint precise hit testing, and passes the point in parent
  // coordinates instead of in paint invalidation container coordinates.
  // Eventually the rest of the layout tree will move to a similar model.
  virtual bool nodeAtFloatPoint(HitTestResult&,
                                const FloatPoint& pointInParent,
                                HitTestAction);

  // End of SVG-specific methods.

  bool isAnonymous() const { return m_bitfields.isAnonymous(); }
  bool isAnonymousBlock() const {
    // This function is kept in sync with anonymous block creation conditions in
    // LayoutBlock::createAnonymousBlock(). This includes creating an anonymous
    // LayoutBlock having a BLOCK or BOX display. Other classes such as
    // LayoutTextFragment are not LayoutBlocks and will return false.
    // See https://bugs.webkit.org/show_bug.cgi?id=56709.
    return isAnonymous() && (style()->display() == EDisplay::Block ||
                             style()->display() == EDisplay::WebkitBox) &&
           style()->styleType() == PseudoIdNone && isLayoutBlock() &&
           !isListMarker() && !isLayoutFlowThread() &&
           !isLayoutMultiColumnSet() && !isLayoutFullScreen() &&
           !isLayoutFullScreenPlaceholder();
  }
  bool isElementContinuation() const {
    return node() && node()->layoutObject() != this;
  }
  bool isInlineElementContinuation() const {
    return isElementContinuation() && isInline();
  }
  virtual LayoutBoxModelObject* virtualContinuation() const { return nullptr; }

  bool isFloating() const { return m_bitfields.floating(); }

  bool isOutOfFlowPositioned() const {
    return m_bitfields.isOutOfFlowPositioned();
  }  // absolute or fixed positioning
  bool isInFlowPositioned() const {
    return m_bitfields.isInFlowPositioned();
  }  // relative or sticky positioning
  bool isRelPositioned() const {
    return m_bitfields.isRelPositioned();
  }  // relative positioning
  bool isStickyPositioned() const {
    return m_bitfields.isStickyPositioned();
  }  // sticky positioning
  bool isFixedPositioned() const {
    return isOutOfFlowPositioned() && style()->position() == FixedPosition;
  }  // fixed positioning
  bool isPositioned() const { return m_bitfields.isPositioned(); }

  bool isText() const { return m_bitfields.isText(); }
  bool isBox() const { return m_bitfields.isBox(); }
  bool isInline() const { return m_bitfields.isInline(); }  // inline object
  bool isAtomicInlineLevel() const { return m_bitfields.isAtomicInlineLevel(); }
  bool isHorizontalWritingMode() const {
    return m_bitfields.horizontalWritingMode();
  }
  bool hasFlippedBlocksWritingMode() const {
    return style()->isFlippedBlocksWritingMode();
  }

  bool hasLayer() const { return m_bitfields.hasLayer(); }

  // This may be different from styleRef().hasBoxDecorationBackground() because
  // some objects may have box decoration background other than from their own
  // style.
  bool hasBoxDecorationBackground() const {
    return m_bitfields.hasBoxDecorationBackground();
  }

  bool backgroundIsKnownToBeObscured() const;

  bool needsLayout() const {
    return m_bitfields.selfNeedsLayout() ||
           m_bitfields.normalChildNeedsLayout() ||
           m_bitfields.posChildNeedsLayout() ||
           m_bitfields.needsSimplifiedNormalFlowLayout() ||
           m_bitfields.needsPositionedMovementLayout();
  }

  bool selfNeedsLayout() const { return m_bitfields.selfNeedsLayout(); }
  bool needsPositionedMovementLayout() const {
    return m_bitfields.needsPositionedMovementLayout();
  }

  bool posChildNeedsLayout() const { return m_bitfields.posChildNeedsLayout(); }
  bool needsSimplifiedNormalFlowLayout() const {
    return m_bitfields.needsSimplifiedNormalFlowLayout();
  }
  bool normalChildNeedsLayout() const {
    return m_bitfields.normalChildNeedsLayout();
  }

  bool preferredLogicalWidthsDirty() const {
    return m_bitfields.preferredLogicalWidthsDirty();
  }

  bool needsOverflowRecalcAfterStyleChange() const {
    return m_bitfields.selfNeedsOverflowRecalcAfterStyleChange() ||
           m_bitfields.childNeedsOverflowRecalcAfterStyleChange();
  }
  bool selfNeedsOverflowRecalcAfterStyleChange() const {
    return m_bitfields.selfNeedsOverflowRecalcAfterStyleChange();
  }
  bool childNeedsOverflowRecalcAfterStyleChange() const {
    return m_bitfields.childNeedsOverflowRecalcAfterStyleChange();
  }

  bool isSelectionBorder() const;

  bool hasClip() const {
    return isOutOfFlowPositioned() && !style()->hasAutoClip();
  }
  bool hasOverflowClip() const { return m_bitfields.hasOverflowClip(); }
  bool hasClipRelatedProperty() const {
    return hasClip() || hasOverflowClip() || hasClipPath() ||
           style()->containsPaint();
  }

  bool hasTransformRelatedProperty() const {
    return m_bitfields.hasTransformRelatedProperty();
  }
  bool isTransformApplicable() const { return isBox() || isSVG(); }
  bool hasMask() const { return style() && style()->hasMask(); }
  bool hasClipPath() const { return style() && style()->clipPath(); }
  bool hasHiddenBackface() const {
    return style() && style()->backfaceVisibility() == BackfaceVisibilityHidden;
  }
  bool hasBackdropFilter() const {
    return style() && style()->hasBackdropFilter();
  }

  // Returns |true| if any property that renders using filter operations is
  // used (including, but not limited to, 'filter' and 'box-reflect').
  // Not calling style()->hasFilterInducingProperty because some objects force
  // to ignore reflection style (e.g. LayoutInline).
  bool hasFilterInducingProperty() const {
    return (style() && style()->hasFilter()) || hasReflection();
  }

  bool hasShapeOutside() const { return style() && style()->shapeOutside(); }

  inline bool preservesNewline() const;

  // The pseudo element style can be cached or uncached.  Use the cached method
  // if the pseudo element doesn't respect any pseudo classes (and therefore
  // has no concept of changing state).
  ComputedStyle* getCachedPseudoStyle(
      PseudoId,
      const ComputedStyle* parentStyle = nullptr) const;
  PassRefPtr<ComputedStyle> getUncachedPseudoStyle(
      const PseudoStyleRequest&,
      const ComputedStyle* parentStyle = nullptr,
      const ComputedStyle* ownStyle = nullptr) const;

  LayoutView* view() const { return document().layoutView(); }
  FrameView* frameView() const { return document().view(); }

  bool isRooted() const;

  Node* node() const { return isAnonymous() ? nullptr : m_node; }

  Node* nonPseudoNode() const { return isPseudoElement() ? nullptr : node(); }

  void clearNode() { m_node = nullptr; }

  // Returns the styled node that caused the generation of this layoutObject.
  // This is the same as node() except for layoutObjects of :before, :after and
  // :first-letter pseudo elements for which their parent node is returned.
  Node* generatingNode() const {
    return isPseudoElement() ? node()->parentOrShadowHostNode() : node();
  }

  Document& document() const {
    ASSERT(m_node || parent());  // crbug.com/402056
    return m_node ? m_node->document() : parent()->document();
  }
  LocalFrame* frame() const { return document().frame(); }

  virtual LayoutMultiColumnSpannerPlaceholder* spannerPlaceholder() const {
    return nullptr;
  }
  bool isColumnSpanAll() const {
    return style()->getColumnSpan() == ColumnSpanAll && spannerPlaceholder();
  }

  // We include isLayoutButton() in this check, because buttons are
  // implemented using flex box but should still support things like
  // first-line, first-letter and text-overflow.
  // The flex box and grid specs require that flex box and grid do not
  // support first-line|first-letter, though.
  // TODO(cbiesinger): Remove when buttons are implemented with align-items
  // instead of flex box. crbug.com/226252.
  bool behavesLikeBlockContainer() const {
    return isLayoutBlockFlow() || isLayoutButton();
  }

  // May be optionally passed to container() and various other similar methods
  // that search the ancestry for some sort of containing block. Used to
  // determine if we skipped certain objects while walking the ancestry.
  class AncestorSkipInfo {
   public:
    AncestorSkipInfo(const LayoutObject* ancestor, bool checkForFilters = false)
        : m_ancestor(ancestor), m_checkForFilters(checkForFilters) {}

    // Update skip info output based on the layout object passed.
    void update(const LayoutObject& object) {
      if (&object == m_ancestor)
        m_ancestorSkipped = true;
      if (m_checkForFilters && object.hasFilterInducingProperty())
        m_filterSkipped = true;
    }

    // TODO(mstensho): Get rid of this. It's just a temporary thing to retain
    // old behavior in LayoutObject::container().
    void resetOutput() {
      m_ancestorSkipped = false;
      m_filterSkipped = false;
    }

    bool ancestorSkipped() const { return m_ancestorSkipped; }
    bool filterSkipped() const {
      DCHECK(m_checkForFilters);
      return m_filterSkipped;
    }

   private:
    // Input: A potential ancestor to look for. If we walk past this one while
    // walking the ancestry in search of some containing block, ancestorSkipped
    // will be set to true.
    const LayoutObject* m_ancestor;
    // Input: When set, we'll check if we skip objects with filter inducing
    // properties.
    bool m_checkForFilters;

    // Output: Set to true if |ancestor| was walked past while walking the
    // ancestry.
    bool m_ancestorSkipped = false;
    // Output: Set to true if we walked past a filter object. This will be set
    // regardless of the value of |ancestor|.
    bool m_filterSkipped = false;
  };

  // This function returns the containing block of the object.
  // Due to CSS being inconsistent, a containing block can be a relatively
  // positioned inline, thus we can't return a LayoutBlock from this function.
  //
  // This method is extremely similar to containingBlock(), but with a few
  // notable exceptions.
  // (1) For normal flow elements, it just returns the parent.
  // (2) For absolute positioned elements, it will return a relative
  //     positioned inline. containingBlock() simply skips relpositioned inlines
  //     and lets an enclosing block handle the layout of the positioned object.
  //     This does mean that computePositionedLogicalWidth and
  //     computePositionedLogicalHeight have to use container().
  //
  // Note that floating objects don't belong to either of the above exceptions.
  //
  // This function should be used for any invalidation as it would correctly
  // walk the containing block chain. See e.g. markContainerChainForLayout.
  // It is also used for correctly sizing absolutely positioned elements
  // (point 3 above).
  LayoutObject* container(AncestorSkipInfo* = nullptr) const;
  // Finds the container as if this object is fixed-position.
  LayoutBlock* containerForFixedPosition(AncestorSkipInfo* = nullptr) const;
  // Finds the containing block as if this object is absolute-position.
  LayoutBlock* containingBlockForAbsolutePosition(
      AncestorSkipInfo* = nullptr) const;

  virtual LayoutObject* hoverAncestor() const { return parent(); }

  Element* offsetParent(const Element* = nullptr) const;

  void markContainerChainForLayout(bool scheduleRelayout = true,
                                   SubtreeLayoutScope* = nullptr);
  void setNeedsLayout(LayoutInvalidationReasonForTracing,
                      MarkingBehavior = MarkContainerChain,
                      SubtreeLayoutScope* = nullptr);
  void setNeedsLayoutAndFullPaintInvalidation(
      LayoutInvalidationReasonForTracing,
      MarkingBehavior = MarkContainerChain,
      SubtreeLayoutScope* = nullptr);
  void clearNeedsLayout();
  void setChildNeedsLayout(MarkingBehavior = MarkContainerChain,
                           SubtreeLayoutScope* = nullptr);
  void setNeedsPositionedMovementLayout();
  void setPreferredLogicalWidthsDirty(MarkingBehavior = MarkContainerChain);
  void clearPreferredLogicalWidthsDirty();

  void setNeedsLayoutAndPrefWidthsRecalc(
      LayoutInvalidationReasonForTracing reason) {
    setNeedsLayout(reason);
    setPreferredLogicalWidthsDirty();
  }
  void setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReasonForTracing reason) {
    setNeedsLayoutAndFullPaintInvalidation(reason);
    setPreferredLogicalWidthsDirty();
  }

  void setPositionState(EPosition position) {
    ASSERT((position != AbsolutePosition && position != FixedPosition) ||
           isBox());
    m_bitfields.setPositionedState(position);
  }
  void clearPositionedState() { m_bitfields.clearPositionedState(); }

  void setFloating(bool isFloating) { m_bitfields.setFloating(isFloating); }
  void setInline(bool isInline) { m_bitfields.setIsInline(isInline); }

  void setHasBoxDecorationBackground(bool);

  enum BackgroundObscurationState {
    BackgroundObscurationStatusInvalid,
    BackgroundKnownToBeObscured,
    BackgroundMayBeVisible,
  };
  void invalidateBackgroundObscurationStatus();
  virtual bool computeBackgroundIsKnownToBeObscured() const { return false; }

  void setIsText() { m_bitfields.setIsText(true); }
  void setIsBox() { m_bitfields.setIsBox(true); }
  void setIsAtomicInlineLevel(bool isAtomicInlineLevel) {
    m_bitfields.setIsAtomicInlineLevel(isAtomicInlineLevel);
  }
  void setHorizontalWritingMode(bool hasHorizontalWritingMode) {
    m_bitfields.setHorizontalWritingMode(hasHorizontalWritingMode);
  }
  void setHasOverflowClip(bool hasOverflowClip) {
    m_bitfields.setHasOverflowClip(hasOverflowClip);
  }
  void setHasLayer(bool hasLayer) { m_bitfields.setHasLayer(hasLayer); }
  void setHasTransformRelatedProperty(bool hasTransform) {
    m_bitfields.setHasTransformRelatedProperty(hasTransform);
  }
  void setHasReflection(bool hasReflection) {
    m_bitfields.setHasReflection(hasReflection);
  }

  // paintOffset is the offset from the origin of the GraphicsContext at which
  // to paint the current object.
  virtual void paint(const PaintInfo&, const LayoutPoint& paintOffset) const;

  // Subclasses must reimplement this method to compute the size and position
  // of this object and all its descendants.
  //
  // By default, layout only lays out the children that are marked for layout.
  // In some cases, layout has to force laying out more children. An example is
  // when the width of the LayoutObject changes as this impacts children with
  // 'width' set to auto.
  virtual void layout() = 0;
  virtual bool updateImageLoadingPriorities() { return false; }

  void handleSubtreeModifications();
  virtual void subtreeDidChange() {}

  // Flags used to mark if an object consumes subtree change notifications.
  bool consumesSubtreeChangeNotification() const {
    return m_bitfields.consumesSubtreeChangeNotification();
  }
  void setConsumesSubtreeChangeNotification() {
    m_bitfields.setConsumesSubtreeChangeNotification(true);
  }

  // Flags used to mark if a descendant subtree of this object has changed.
  void notifyOfSubtreeChange();
  void notifyAncestorsOfSubtreeChange();
  bool wasNotifiedOfSubtreeChange() const {
    return m_bitfields.notifiedOfSubtreeChange();
  }

  // Flags used to signify that a layoutObject needs to be notified by its
  // descendants that they have had their child subtree changed.
  void registerSubtreeChangeListenerOnDescendants(bool);
  bool hasSubtreeChangeListenerRegistered() const {
    return m_bitfields.subtreeChangeListenerRegistered();
  }

  /* This function performs a layout only if one is needed. */
  DISABLE_CFI_PERF void layoutIfNeeded() {
    if (needsLayout())
      layout();
  }

  void forceLayout();
  void forceChildLayout();

  // Used for element state updates that cannot be fixed with a paint
  // invalidation and do not need a relayout.
  virtual void updateFromElement() {}

  virtual void addAnnotatedRegions(Vector<AnnotatedRegionValue>&);

  CompositingState compositingState() const;
  virtual CompositingReasons additionalCompositingReasons() const;

  bool hitTest(HitTestResult&,
               const HitTestLocation& locationInContainer,
               const LayoutPoint& accumulatedOffset,
               HitTestFilter = HitTestAll);
  virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&);
  virtual bool nodeAtPoint(HitTestResult&,
                           const HitTestLocation& locationInContainer,
                           const LayoutPoint& accumulatedOffset,
                           HitTestAction);

  virtual PositionWithAffinity positionForPoint(const LayoutPoint&);
  PositionWithAffinity createPositionWithAffinity(int offset, TextAffinity);
  PositionWithAffinity createPositionWithAffinity(int offset);
  PositionWithAffinity createPositionWithAffinity(const Position&);

  virtual void dirtyLinesFromChangedChild(
      LayoutObject*,
      MarkingBehavior markingBehaviour = MarkContainerChain);

  // Set the style of the object and update the state of the object accordingly.
  void setStyle(PassRefPtr<ComputedStyle>);

  // Set the style of the object if it's generated content.
  void setPseudoStyle(PassRefPtr<ComputedStyle>);

  // Updates only the local style ptr of the object.  Does not update the state
  // of the object, and so only should be called when the style is known not to
  // have changed (or from setStyle).
  void setStyleInternal(PassRefPtr<ComputedStyle> style) { m_style = style; }

  void setStyleWithWritingModeOf(PassRefPtr<ComputedStyle>,
                                 LayoutObject* parent);
  void setStyleWithWritingModeOfParent(PassRefPtr<ComputedStyle>);
  void addChildWithWritingModeOfParent(LayoutObject* newChild,
                                       LayoutObject* beforeChild);

  void firstLineStyleDidChange(const ComputedStyle& oldStyle,
                               const ComputedStyle& newStyle);

  void clearBaseComputedStyle();

  // This function returns an enclosing non-anonymous LayoutBlock for this
  // element. This function is not always returning the containing block as
  // defined by CSS. In particular:
  // - if the CSS containing block is a relatively positioned inline,
  //   the function returns the inline's enclosing non-anonymous LayoutBlock.
  //   This means that a LayoutInline would be skipped (expected as it's not a
  //   LayoutBlock) but so would be an inline LayoutTable or LayoutBlockFlow.
  //   TODO(jchaffraix): Is that REALLY what we want here?
  // - if the CSS containing block is anonymous, we find its enclosing
  //   non-anonymous LayoutBlock.
  //   Note that in the previous examples, the returned LayoutBlock has no
  //   logical relationship to the original element.
  //
  // LayoutBlocks are the one that handle laying out positioned elements,
  // thus this function is important during layout, to insert the positioned
  // elements into the correct LayoutBlock.
  //
  // See container() for the function that returns the containing block.
  // See LayoutBlock.h for some extra explanations on containing blocks.
  LayoutBlock* containingBlock(AncestorSkipInfo* = nullptr) const;

  bool canContainAbsolutePositionObjects() const {
    return m_style->canContainAbsolutePositionObjects() ||
           canContainFixedPositionObjects();
  }
  bool canContainFixedPositionObjects() const {
    return isLayoutView() || isSVGForeignObject() ||
           (isLayoutBlock() && m_style->canContainFixedPositionObjects());
  }

  // Convert the given local point to absolute coordinates
  // FIXME: Temporary. If UseTransforms is true, take transforms into account.
  // Eventually localToAbsolute() will always be transform-aware.
  FloatPoint localToAbsolute(const FloatPoint& localPoint = FloatPoint(),
                             MapCoordinatesFlags = 0) const;

  // If the LayoutBoxModelObject ancestor is non-null, the input point is in the
  // space of the ancestor.
  // Otherwise:
  //   If TraverseDocumentBoundaries is specified, the input point is in the
  //   space of the local root frame.
  //   Otherwise, the input point is in the space of the containing frame.
  FloatPoint ancestorToLocal(LayoutBoxModelObject*,
                             const FloatPoint&,
                             MapCoordinatesFlags = 0) const;
  FloatPoint absoluteToLocal(const FloatPoint& point,
                             MapCoordinatesFlags mode = 0) const {
    return ancestorToLocal(nullptr, point, mode);
  }

  // Convert a local quad to absolute coordinates, taking transforms into
  // account.
  FloatQuad localToAbsoluteQuad(const FloatQuad& quad,
                                MapCoordinatesFlags mode = 0) const {
    return localToAncestorQuad(quad, nullptr, mode);
  }

  // Convert a quad in ancestor coordinates to local coordinates.
  // If the LayoutBoxModelObject ancestor is non-null, the input quad is in the
  // space of the ancestor.
  // Otherwise:
  //   If TraverseDocumentBoundaries is specified, the input quad is in the
  //   space of the local root frame.
  //   Otherwise, the input quad is in the space of the containing frame.
  FloatQuad ancestorToLocalQuad(LayoutBoxModelObject*,
                                const FloatQuad&,
                                MapCoordinatesFlags mode = 0) const;
  FloatQuad absoluteToLocalQuad(const FloatQuad& quad,
                                MapCoordinatesFlags mode = 0) const {
    return ancestorToLocalQuad(nullptr, quad, mode);
  }

  // Convert a local quad into the coordinate system of container, taking
  // transforms into account.
  // If the LayoutBoxModelObject ancestor is non-null, the result will be in the
  // space of the ancestor.
  // Otherwise:
  //   If TraverseDocumentBoundaries is specified, the result will be in the
  //   space of the local root frame.
  //   Otherwise, the result will be in the space of the containing frame.
  FloatQuad localToAncestorQuad(const FloatQuad&,
                                const LayoutBoxModelObject* ancestor,
                                MapCoordinatesFlags = 0) const;
  FloatPoint localToAncestorPoint(const FloatPoint&,
                                  const LayoutBoxModelObject* ancestor,
                                  MapCoordinatesFlags = 0) const;
  void localToAncestorRects(Vector<LayoutRect>&,
                            const LayoutBoxModelObject* ancestor,
                            const LayoutPoint& preOffset,
                            const LayoutPoint& postOffset) const;

  // Convert a local quad into the coordinate system of container, not
  // include transforms. See localToAncestorQuad for details.
  FloatQuad localToAncestorQuadWithoutTransforms(
      const FloatQuad&,
      const LayoutBoxModelObject* ancestor,
      MapCoordinatesFlags = 0) const;

  // Return the transformation matrix to map points from local to the coordinate
  // system of a container, taking transforms into account.
  // Passing null for |ancestor| behaves the same as localToAncestorQuad.
  TransformationMatrix localToAncestorTransform(
      const LayoutBoxModelObject* ancestor,
      MapCoordinatesFlags = 0) const;
  TransformationMatrix localToAbsoluteTransform(
      MapCoordinatesFlags mode = 0) const {
    return localToAncestorTransform(nullptr, mode);
  }

  // Return the offset from the container() layoutObject (excluding transforms
  // and multicol).
  virtual LayoutSize offsetFromContainer(const LayoutObject*) const;
  // Return the offset from an object up the container() chain. Asserts that
  // none of the intermediate objects have transforms.
  LayoutSize offsetFromAncestorContainer(const LayoutObject*) const;

  virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint&) const {}

  FloatRect absoluteBoundingBoxFloatRect() const;
  // This returns an IntRect enclosing this object. If this object has an
  // integral size and the position has fractional values, the resultant
  // IntRect can be larger than the integral size.
  IntRect absoluteBoundingBoxRect() const;
  // FIXME: This function should go away eventually
  IntRect absoluteBoundingBoxRectIgnoringTransforms() const;

  // Build an array of quads in absolute coords for line boxes
  virtual void absoluteQuads(Vector<FloatQuad>&,
                             MapCoordinatesFlags mode = 0) const {}

  static FloatRect absoluteBoundingBoxRectForRange(const Range*);

  // The bounding box (see: absoluteBoundingBoxRect) including all descendant
  // bounding boxes.
  IntRect absoluteBoundingBoxRectIncludingDescendants() const;

  // For accessibility, we want the bounding box rect of this element
  // in local coordinates, which can then be converted to coordinates relative
  // to any ancestor using, e.g., localToAncestorTransform.
  virtual FloatRect localBoundingBoxRectForAccessibility() const = 0;

  // This function returns the minimal logical width this object can have
  // without overflowing. This means that all the opportunities for wrapping
  // have been taken.
  //
  // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS above.
  //
  // CSS 2.1 calls this width the "preferred minimum width" (thus this name)
  // and "minimum content width" (for table).
  // However CSS 3 calls it the "min-content inline size".
  // https://drafts.csswg.org/css-sizing-3/#min-content-inline-size
  // TODO(jchaffraix): We will probably want to rename it to match CSS 3.
  virtual LayoutUnit minPreferredLogicalWidth() const { return LayoutUnit(); }

  // This function returns the maximum logical width this object can have.
  //
  // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS above.
  //
  // CSS 2.1 calls this width the "preferred width". However CSS 3 calls it
  // the "max-content inline size".
  // https://drafts.csswg.org/css-sizing-3/#max-content-inline-size
  // TODO(jchaffraix): We will probably want to rename it to match CSS 3.
  virtual LayoutUnit maxPreferredLogicalWidth() const { return LayoutUnit(); }

  const ComputedStyle* style() const { return m_style.get(); }
  ComputedStyle* mutableStyle() const { return m_style.get(); }

  // m_style can only be nullptr before the first style is set, thus most
  // callers will never see a nullptr style and should use styleRef().
  // FIXME: It would be better if style() returned a const reference.
  const ComputedStyle& styleRef() const { return mutableStyleRef(); }
  ComputedStyle& mutableStyleRef() const {
    ASSERT(m_style);
    return *m_style;
  }

  /* The following methods are inlined in LayoutObjectInlines.h */
  inline const ComputedStyle* firstLineStyle() const;
  inline const ComputedStyle& firstLineStyleRef() const;
  inline const ComputedStyle* style(bool firstLine) const;
  inline const ComputedStyle& styleRef(bool firstLine) const;

  static inline Color resolveColor(const ComputedStyle& styleToUse,
                                   int colorProperty) {
    return styleToUse.visitedDependentColor(colorProperty);
  }

  inline Color resolveColor(int colorProperty) const {
    return style()->visitedDependentColor(colorProperty);
  }

  // Used only by Element::pseudoStyleCacheIsInvalid to get a first line style
  // based off of a given new style, without accessing the cache.
  PassRefPtr<ComputedStyle> uncachedFirstLineStyle(ComputedStyle*) const;

  virtual CursorDirective getCursor(const LayoutPoint&, Cursor&) const;

  // Return the LayoutBoxModelObject in the container chain which is responsible
  // for painting this object. The function crosses frames boundaries so the
  // returned value can be in a different document.
  //
  // This is the container that should be passed to the '*forPaintInvalidation'
  // methods.
  const LayoutBoxModelObject& containerForPaintInvalidation() const;

  bool isPaintInvalidationContainer() const;

  // Invalidate the paint of a specific subrectangle within a given object. The
  // rect is in the object's coordinate space.
  // If a DisplayItemClient is specified, that client is invalidated rather than
  // |this|.
  // Returns the visual rect that was invalidated (i.e, invalidation in the
  // space of the GraphicsLayer backing this LayoutObject).
  LayoutRect invalidatePaintRectangle(const LayoutRect&,
                                      DisplayItemClient* = nullptr) const;

  // Walk the tree after layout issuing paint invalidations for layoutObjects
  // that have changed or moved, updating bounds that have changed, and clearing
  // paint invalidation state.
  virtual void invalidateTreeIfNeeded(const PaintInvalidationState&);

  void setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();

  // Returns the rect that should have paint invalidated whenever this object
  // changes. The rect is in the view's coordinate space. This method deals with
  // outlines and overflow.
  virtual LayoutRect absoluteVisualRect() const;

  // Returns the rect that should have paint invalidated whenever this object
  // changes. The rect is in the object's local coordinate space. This is for
  // non-SVG objects and LayoutSVGRoot only. SVG objects (except LayoutSVGRoot)
  // should use visualRectInLocalSVGCoordinates() and map with SVG transforms
  // instead.
  virtual LayoutRect localVisualRect() const;

  // Given a rect in the object's coordinate space, mutates the rect into one
  // representing the size of its visual painted output as if |ancestor| was the
  // root of the page: the rect is modified by any intervening clips, transforms
  // and scrolls between |this| and |ancestor| (not inclusive of |ancestor|),
  // but not any above |ancestor|.
  // The output is in the physical, painted coordinate pixel space of
  // |ancestor|.
  // Overflow clipping, CSS clipping and scrolling is *not* applied for
  // |ancestor| itself if |ancestor| scrolls overflow.
  // The output rect is suitable for purposes such as paint invalidation.
  //
  // If visualRectFlags has the EdgeInclusive bit set, clipping operations will
  // use/ LayoutRect::inclusiveIntersect, and the return value of
  // inclusiveIntersect will be propagated to the return value of this method.
  // Otherwise, clipping operations will use LayoutRect::intersect, and the
  // return value will be true only if the clipped rect has non-zero area.
  // See the documentation for LayoutRect::inclusiveIntersect for more
  // information.
  virtual bool mapToVisualRectInAncestorSpace(
      const LayoutBoxModelObject* ancestor,
      LayoutRect&,
      VisualRectFlags = DefaultVisualRectFlags) const;

  // Allows objects to adjust |visualEffect|, which is in the space of the
  // paint invalidation container, for any special raster effects that might
  // expand the rastered pixel area. Returns true if the rect is expanded.
  virtual bool adjustVisualRectForRasterEffects(LayoutRect& visualRect) const {
    return false;
  }

  // Return the offset to the column in which the specified point (in
  // flow-thread coordinates) lives. This is used to convert a flow-thread point
  // to a point in the containing coordinate space.
  virtual LayoutSize columnOffset(const LayoutPoint&) const {
    return LayoutSize();
  }

  virtual unsigned length() const { return 1; }

  bool isFloatingOrOutOfFlowPositioned() const {
    return (isFloating() || isOutOfFlowPositioned());
  }

  bool isTransparent() const { return style()->hasOpacity(); }
  float opacity() const { return style()->opacity(); }

  bool hasReflection() const { return m_bitfields.hasReflection(); }

  // The current selection state for an object.  For blocks, the state refers to
  // the state of the leaf descendants (as described above in the SelectionState
  // enum declaration).
  SelectionState getSelectionState() const {
    return m_bitfields.getSelectionState();
  }
  virtual void setSelectionState(SelectionState state) {
    m_bitfields.setSelectionState(state);
  }
  inline void setSelectionStateIfNeeded(SelectionState);
  bool canUpdateSelectionOnRootLineBoxes() const;

  // A single rectangle that encompasses all of the selected objects within this
  // object. Used to determine the tightest possible bounding box for the
  // selection. The rect returned is in the object's local coordinate space.
  virtual LayoutRect localSelectionRect() const { return LayoutRect(); }

  // View coordinates means the coordinate space of |view()|.
  LayoutRect selectionRectInViewCoordinates() const;

  virtual bool canBeSelectionLeaf() const { return false; }
  bool hasSelectedChildren() const {
    return getSelectionState() != SelectionNone;
  }

  bool isSelectable() const;
  // Obtains the selection colors that should be used when painting a selection.
  Color selectionBackgroundColor() const;
  Color selectionForegroundColor(const GlobalPaintFlags) const;
  Color selectionEmphasisMarkColor(const GlobalPaintFlags) const;

  /**
     * Returns the local coordinates of the caret within this layout object.
     * @param caretOffset zero-based offset determining position within the
   * layout object.
     * @param extraWidthToEndOfLine optional out arg to give extra width to end
   * of line -
     * useful for character range rect computations
     */
  virtual LayoutRect localCaretRect(
      InlineBox*,
      int caretOffset,
      LayoutUnit* extraWidthToEndOfLine = nullptr);

  // When performing a global document tear-down, the layoutObject of the
  // document is cleared. We use this as a hook to detect the case of document
  // destruction and don't waste time doing unnecessary work.
  bool documentBeingDestroyed() const;

  void destroyAndCleanupAnonymousWrappers();

  // While the destroy() method is virtual, this should only be overriden in
  // very rare circumstances.
  // You want to override willBeDestroyed() instead unless you explicitly need
  // to stop this object from being destroyed (for example, LayoutPart
  // overrides destroy() for this purpose).
  virtual void destroy();

  // Virtual function helpers for the deprecated Flexible Box Layout (display:
  // -webkit-box).
  virtual bool isDeprecatedFlexibleBox() const { return false; }

  // Virtual function helper for the new FlexibleBox Layout (display:
  // -webkit-flex).
  virtual bool isFlexibleBox() const { return false; }

  bool isFlexibleBoxIncludingDeprecated() const {
    return isFlexibleBox() || isDeprecatedFlexibleBox();
  }

  virtual bool isCombineText() const { return false; }

  virtual int caretMinOffset() const;
  virtual int caretMaxOffset() const;

  // ImageResourceClient override.
  void imageChanged(ImageResourceContent*, const IntRect* = nullptr) final;
  bool willRenderImage() final;
  bool getImageAnimationPolicy(ImageAnimationPolicy&) final;

  // Sub-classes that have an associated image need to override this function
  // to get notified of any image change.
  virtual void imageChanged(WrappedImagePtr, const IntRect* = nullptr) {}

  void selectionStartEnd(int& spos, int& epos) const;

  void remove() {
    if (parent())
      parent()->removeChild(this);
  }

  bool visibleToHitTestRequest(const HitTestRequest& request) const {
    return style()->visibility() == EVisibility::kVisible &&
           (request.ignorePointerEventsNone() ||
            style()->pointerEvents() != EPointerEvents::kNone) &&
           !isInert();
  }

  // Warning: inertness can change without causing relayout.
  bool visibleToHitTesting() const {
    return style()->visibleToHitTesting() && !isInert();
  }

  // Map points and quads through elements, potentially via 3d transforms. You
  // should never need to call these directly; use localToAbsolute/
  // absoluteToLocal methods instead.
  virtual void mapLocalToAncestor(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      MapCoordinatesFlags = ApplyContainerFlip) const;
  // If the LayoutBoxModelObject ancestor is non-null, the input quad is in the
  // space of the ancestor.
  // Otherwise:
  //   If TraverseDocumentBoundaries is specified, the input quad is in the
  //   space of the local root frame.
  //   Otherwise, the input quad is in the space of the containing frame.
  virtual void mapAncestorToLocal(
      const LayoutBoxModelObject*,
      TransformState&,
      MapCoordinatesFlags = ApplyContainerFlip) const;

  // Pushes state onto LayoutGeometryMap about how to map coordinates from this
  // layoutObject to its container, or ancestorToStopAt (whichever is
  // encountered first). Returns the layoutObject which was mapped to (container
  // or ancestorToStopAt).
  virtual const LayoutObject* pushMappingToContainer(
      const LayoutBoxModelObject* ancestorToStopAt,
      LayoutGeometryMap&) const;

  bool shouldUseTransformFromContainer(const LayoutObject* container) const;
  void getTransformFromContainer(const LayoutObject* container,
                                 const LayoutSize& offsetInContainer,
                                 TransformationMatrix&) const;

  bool createsGroup() const {
    return isTransparent() || hasMask() || hasFilterInducingProperty() ||
           style()->hasBlendMode();
  }

  // Collects rectangles that the outline of this object would be drawing along
  // the outside of, even if the object isn't styled with a outline for now. The
  // rects also cover continuations.
  enum IncludeBlockVisualOverflowOrNot {
    DontIncludeBlockVisualOverflow,
    IncludeBlockVisualOverflow,
  };
  virtual void addOutlineRects(Vector<LayoutRect>&,
                               const LayoutPoint& additionalOffset,
                               IncludeBlockVisualOverflowOrNot) const {}

  // For history and compatibility reasons, we draw outline:auto (for focus
  // rings) and normal style outline differently.
  // Focus rings enclose block visual overflows (of line boxes and descendants),
  // while normal outlines don't.
  IncludeBlockVisualOverflowOrNot outlineRectsShouldIncludeBlockVisualOverflow()
      const {
    return styleRef().outlineStyleIsAuto() ? IncludeBlockVisualOverflow
                                           : DontIncludeBlockVisualOverflow;
  }

  // Collects rectangles enclosing visual overflows of the DOM subtree under
  // this object.
  // The rects also cover continuations which may be not in the layout subtree
  // of this object.
  // TODO(crbug.com/614781): Currently the result rects don't cover list markers
  // and outlines.
  void addElementVisualOverflowRects(
      Vector<LayoutRect>& rects,
      const LayoutPoint& additionalOffset) const {
    addOutlineRects(rects, additionalOffset, IncludeBlockVisualOverflow);
  }

  // Returns the rect enclosing united visual overflow of the DOM subtree under
  // this object. It includes continuations which may be not in the layout
  // subtree of this object.
  virtual IntRect absoluteElementBoundingBoxRect() const;

  // Compute a list of hit-test rectangles per layer rooted at this
  // layoutObject.
  virtual void computeLayerHitTestRects(LayerHitTestRects&) const;

  static RespectImageOrientationEnum shouldRespectImageOrientation(
      const LayoutObject*);

  bool isRelayoutBoundaryForInspector() const;

  // The previous visual rect, in the the space of the paint invalidation
  // container (*not* the graphics layer that paints this object).
  LayoutRect previousVisualRectIncludingCompositedScrolling(
      const LayoutBoxModelObject& paintInvalidationContainer) const;

  // The returned rect does *not* account for composited scrolling.
  const LayoutRect& previousVisualRect() const { return m_previousVisualRect; }

  // Called when the previous visual rect(s) is no longer valid.
  virtual void clearPreviousVisualRects();

  const LayoutPoint& paintOffset() const { return m_paintOffset; }

  PaintInvalidationReason fullPaintInvalidationReason() const {
    return m_bitfields.fullPaintInvalidationReason();
  }
  bool shouldDoFullPaintInvalidation() const {
    return m_bitfields.fullPaintInvalidationReason() != PaintInvalidationNone;
  }
  void setShouldDoFullPaintInvalidation(
      PaintInvalidationReason = PaintInvalidationFull);
  void clearShouldDoFullPaintInvalidation() {
    m_bitfields.setFullPaintInvalidationReason(PaintInvalidationNone);
  }

  void clearPaintInvalidationFlags();

  bool mayNeedPaintInvalidation() const {
    return m_bitfields.mayNeedPaintInvalidation();
  }
  void setMayNeedPaintInvalidation();

  bool mayNeedPaintInvalidationSubtree() const {
    return m_bitfields.mayNeedPaintInvalidationSubtree();
  }
  void setMayNeedPaintInvalidationSubtree();

  bool mayNeedPaintInvalidationAnimatedBackgroundImage() const {
    return m_bitfields.mayNeedPaintInvalidationAnimatedBackgroundImage();
  }
  void setMayNeedPaintInvalidationAnimatedBackgroundImage();

  bool shouldInvalidateSelection() const {
    return m_bitfields.shouldInvalidateSelection();
  }
  void setShouldInvalidateSelection();

  bool shouldCheckForPaintInvalidation(
      const PaintInvalidationState& paintInvalidationState) const {
    return paintInvalidationState.hasForcedSubtreeInvalidationFlags() ||
           shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState();
  }

  bool shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState()
      const {
    return mayNeedPaintInvalidation() || shouldDoFullPaintInvalidation() ||
           shouldInvalidateSelection() ||
           m_bitfields.childShouldCheckForPaintInvalidation();
  }

  virtual LayoutRect viewRect() const;

  // New version to replace the above old version.
  virtual PaintInvalidationReason invalidatePaintIfNeeded(
      const PaintInvalidatorContext&) const;

  // When this object is invalidated for paint, this method is called to
  // invalidate any DisplayItemClients owned by this object, including the
  // object itself, LayoutText/LayoutInline line boxes, etc.,
  // not including children which will be invalidated normally during
  // invalidateTreeIfNeeded() and parts which are invalidated separately (e.g.
  // scrollbars). The caller should ensure the painting layer has been
  // setNeedsRepaint before calling this function.
  virtual void invalidateDisplayItemClients(PaintInvalidationReason) const;

  virtual bool hasNonCompositedScrollbars() const { return false; }

  // Called before anonymousChild.setStyle(). Override to set custom styles for
  // the child.
  virtual void updateAnonymousChildStyle(const LayoutObject& anonymousChild,
                                         ComputedStyle& style) const {}

  // Returns a rect corresponding to this LayoutObject's bounds for use in
  // debugging output
  virtual LayoutRect debugRect() const;

  // Painters can use const methods only, except for these explicitly declared
  // methods.
  class MutableForPainting {
   public:
    // Convenience mutator that clears paint invalidation flags and this object
    // and its descendants' needs-paint-property-update flags.
    void clearPaintFlags() {
      DCHECK_EQ(m_layoutObject.document().lifecycle().state(),
                DocumentLifecycle::InPrePaint);
      m_layoutObject.clearPaintInvalidationFlags();
      m_layoutObject.m_bitfields.setNeedsPaintPropertyUpdate(false);
      m_layoutObject.m_bitfields.setSubtreeNeedsPaintPropertyUpdate(false);
      m_layoutObject.m_bitfields.setDescendantNeedsPaintPropertyUpdate(false);
    }
    void setShouldDoFullPaintInvalidation(PaintInvalidationReason reason) {
      m_layoutObject.setShouldDoFullPaintInvalidation(reason);
    }
    void setBackgroundChangedSinceLastPaintInvalidation() {
      m_layoutObject.setBackgroundChangedSinceLastPaintInvalidation();
    }
    void ensureIsReadyForPaintInvalidation() {
      m_layoutObject.ensureIsReadyForPaintInvalidation();
    }

    // The following setters store the current values as calculated during the
    // pre-paint tree walk. TODO(wangxianzhu): Add check of lifecycle states.
    void setPreviousVisualRect(const LayoutRect& r) {
      m_layoutObject.setPreviousVisualRect(r);
    }
    void setPaintOffset(const LayoutPoint& p) {
      DCHECK(RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
      DCHECK_EQ(m_layoutObject.document().lifecycle().state(),
                DocumentLifecycle::InPrePaint);
      m_layoutObject.m_paintOffset = p;
    }
    void setHasPreviousLocationInBacking(bool b) {
      m_layoutObject.m_bitfields.setHasPreviousLocationInBacking(b);
    }
    void setHasPreviousSelectionVisualRect(bool b) {
      m_layoutObject.m_bitfields.setHasPreviousSelectionVisualRect(b);
    }
    void setHasPreviousBoxGeometries(bool b) {
      m_layoutObject.m_bitfields.setHasPreviousBoxGeometries(b);
    }
    void setPreviousBackgroundObscured(bool b) {
      m_layoutObject.setPreviousBackgroundObscured(b);
    }

    void clearPreviousVisualRects() {
      m_layoutObject.clearPreviousVisualRects();
    }
    void setNeedsPaintPropertyUpdate() {
      m_layoutObject.setNeedsPaintPropertyUpdate();
    }
#if DCHECK_IS_ON()
    // Same as setNeedsPaintPropertyUpdate() but does not mark ancestors as
    // having a descendant needing a paint property update.
    void setOnlyThisNeedsPaintPropertyUpdateForTesting() {
      m_layoutObject.m_bitfields.setNeedsPaintPropertyUpdate(true);
    }
    void clearNeedsPaintPropertyUpdateForTesting() {
      m_layoutObject.m_bitfields.setNeedsPaintPropertyUpdate(false);
    }
#endif

   protected:
    friend class PaintPropertyTreeBuilder;
    FRIEND_TEST_ALL_PREFIXES(AnimationCompositorAnimationsTest,
                             canStartAnimationOnCompositorTransformSPv2);
    FRIEND_TEST_ALL_PREFIXES(AnimationCompositorAnimationsTest,
                             canStartAnimationOnCompositorEffectSPv2);

    // The following two functions can be called from PaintPropertyTreeBuilder
    // only.
    ObjectPaintProperties& ensurePaintProperties() {
      return m_layoutObject.ensurePaintProperties();
    }
    ObjectPaintProperties* paintProperties() {
      return const_cast<ObjectPaintProperties*>(
          m_layoutObject.paintProperties());
    }

    friend class LayoutObject;
    MutableForPainting(const LayoutObject& layoutObject)
        : m_layoutObject(const_cast<LayoutObject&>(layoutObject)) {}

    LayoutObject& m_layoutObject;
  };
  MutableForPainting getMutableForPainting() const {
    return MutableForPainting(*this);
  }

  // Paint properties (see: |ObjectPaintProperties|) are built from an object's
  // state (location, transform, etc) as well as properties from ancestors.
  // When these inputs change, setNeedsPaintPropertyUpdate will cause a property
  // tree update during the next document lifecycle update.
  //
  // In addition to tracking if an object needs its own paint properties
  // updated, setNeedsPaintPropertyUpdate marks all ancestors as having a
  // descendant needing a paint property update too.
  void setNeedsPaintPropertyUpdate();
  bool needsPaintPropertyUpdate() const {
    return m_bitfields.needsPaintPropertyUpdate();
  }
  void setSubtreeNeedsPaintPropertyUpdate() {
    m_bitfields.setSubtreeNeedsPaintPropertyUpdate(true);
    m_bitfields.setNeedsPaintPropertyUpdate(true);
  }
  bool subtreeNeedsPaintPropertyUpdate() const {
    return m_bitfields.subtreeNeedsPaintPropertyUpdate();
  }
  bool descendantNeedsPaintPropertyUpdate() const {
    return m_bitfields.descendantNeedsPaintPropertyUpdate();
  }
  // Main thread scrolling reasons require fully updating paint propeties of all
  // ancestors (see: ScrollPaintPropertyNode.h).
  void setAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();

  void setIsScrollAnchorObject() { m_bitfields.setIsScrollAnchorObject(true); }
  // Clears the IsScrollAnchorObject bit if and only if no ScrollAnchors still
  // reference this LayoutObject.
  void maybeClearIsScrollAnchorObject();

  bool scrollAnchorDisablingStyleChanged() {
    return m_bitfields.scrollAnchorDisablingStyleChanged();
  }
  void setScrollAnchorDisablingStyleChanged(bool changed) {
    m_bitfields.setScrollAnchorDisablingStyleChanged(changed);
  }

  void clearChildNeedsOverflowRecalcAfterStyleChange() {
    m_bitfields.setChildNeedsOverflowRecalcAfterStyleChange(false);
  }

  bool compositedScrollsWithRespectTo(
      const LayoutBoxModelObject& paintInvalidationContainer) const;
  IntSize scrollAdjustmentForPaintInvalidation(
      const LayoutBoxModelObject& paintInvalidationContainer) const;

  bool previousBackgroundObscured() const {
    return m_bitfields.previousBackgroundObscured();
  }
  void setPreviousBackgroundObscured(bool b) {
    m_bitfields.setPreviousBackgroundObscured(b);
  }

  bool isBackgroundAttachmentFixedObject() const {
    return m_bitfields.isBackgroundAttachmentFixedObject();
  }

  // Paint invalidators will access the internal global map storing the data
  // only when the flag is set, to avoid unnecessary map lookups.
  bool hasPreviousLocationInBacking() const {
    return m_bitfields.hasPreviousLocationInBacking();
  }
  bool hasPreviousSelectionVisualRect() const {
    return m_bitfields.hasPreviousSelectionVisualRect();
  }
  bool hasPreviousBoxGeometries() const {
    return m_bitfields.hasPreviousBoxGeometries();
  }

  bool backgroundChangedSinceLastPaintInvalidation() const {
    return m_bitfields.backgroundChangedSinceLastPaintInvalidation();
  }
  void setBackgroundChangedSinceLastPaintInvalidation() {
    m_bitfields.setBackgroundChangedSinceLastPaintInvalidation(true);
  }

 protected:
  enum LayoutObjectType {
    LayoutObjectBr,
    LayoutObjectCanvas,
    LayoutObjectFieldset,
    LayoutObjectCounter,
    LayoutObjectDetailsMarker,
    LayoutObjectEmbeddedObject,
    LayoutObjectFileUploadControl,
    LayoutObjectFrame,
    LayoutObjectFrameSet,
    LayoutObjectLayoutTableCol,
    LayoutObjectListBox,
    LayoutObjectListItem,
    LayoutObjectListMarker,
    LayoutObjectMedia,
    LayoutObjectMenuList,
    LayoutObjectNGBlockFlow,
    LayoutObjectProgress,
    LayoutObjectQuote,
    LayoutObjectLayoutButton,
    LayoutObjectLayoutFlowThread,
    LayoutObjectLayoutFullScreen,
    LayoutObjectLayoutFullScreenPlaceholder,
    LayoutObjectLayoutGrid,
    LayoutObjectLayoutIFrame,
    LayoutObjectLayoutImage,
    LayoutObjectLayoutInline,
    LayoutObjectLayoutMultiColumnSet,
    LayoutObjectLayoutMultiColumnSpannerPlaceholder,
    LayoutObjectLayoutPart,
    LayoutObjectLayoutScrollbarPart,
    LayoutObjectLayoutView,
    LayoutObjectRuby,
    LayoutObjectRubyBase,
    LayoutObjectRubyRun,
    LayoutObjectRubyText,
    LayoutObjectSlider,
    LayoutObjectSliderThumb,
    LayoutObjectTable,
    LayoutObjectTableCaption,
    LayoutObjectTableCell,
    LayoutObjectTableRow,
    LayoutObjectTableSection,
    LayoutObjectTextArea,
    LayoutObjectTextControl,
    LayoutObjectTextField,
    LayoutObjectVideo,
    LayoutObjectWidget,

    LayoutObjectSVG, /* Keep by itself? */
    LayoutObjectSVGRoot,
    LayoutObjectSVGContainer,
    LayoutObjectSVGTransformableContainer,
    LayoutObjectSVGViewportContainer,
    LayoutObjectSVGHiddenContainer,
    LayoutObjectSVGGradientStop,
    LayoutObjectSVGShape,
    LayoutObjectSVGText,
    LayoutObjectSVGTextPath,
    LayoutObjectSVGInline,
    LayoutObjectSVGInlineText,
    LayoutObjectSVGImage,
    LayoutObjectSVGForeignObject,
    LayoutObjectSVGResourceContainer,
    LayoutObjectSVGResourceFilter,
    LayoutObjectSVGResourceFilterPrimitive,
  };
  virtual bool isOfType(LayoutObjectType type) const { return false; }

  inline bool layerCreationAllowedForSubtree() const;

  // Overrides should call the superclass at the end. m_style will be 0 the
  // first time this function will be called.
  virtual void styleWillChange(StyleDifference, const ComputedStyle& newStyle);
  // Overrides should call the superclass at the start. |oldStyle| will be 0 the
  // first time this function is called.
  virtual void styleDidChange(StyleDifference, const ComputedStyle* oldStyle);
  void propagateStyleToAnonymousChildren();
  // Return true for objects that don't want style changes automatically
  // propagated via propagateStyleToAnonymousChildren(), but rather rely on
  // other custom mechanisms (if they need to be notified of parent style
  // changes at all).
  virtual bool anonymousHasStylePropagationOverride() { return false; }

 protected:
  // This function is called before calling the destructor so that some clean-up
  // can happen regardless of whether they call a virtual function or not. As a
  // rule of thumb, this function should be preferred to the destructor. See
  // destroy() that is the one calling willBeDestroyed().
  //
  // There are 2 types of destructions: regular destructions and tree tear-down.
  // Regular destructions happen when the renderer is not needed anymore (e.g.
  // 'display' changed or the DOM Node was removed).
  // Tree tear-down is when the whole tree destroyed during navigation. It is
  // handled in the code by checking if documentBeingDestroyed() returns 'true'.
  // In this case, the code skips some unneeded expensive operations as we know
  // the tree is not reused (e.g. avoid clearing the containing block's line
  // box).
  virtual void willBeDestroyed();

  virtual void insertedIntoTree();
  virtual void willBeRemovedFromTree();

  void setDocumentForAnonymous(Document* document) {
    ASSERT(isAnonymous());
    m_node = document;
  }

  // Add hit-test rects for the layout tree rooted at this node to the provided
  // collection on a per-Layer basis.
  // currentLayer must be the enclosing layer, and layerOffset is the current
  // offset within this layer. Subclass implementations will add any offset for
  // this layoutObject within it's container, so callers should provide only the
  // offset of the container within it's layer.
  // containerRect is a rect that has already been added for the currentLayer
  // which is likely to be a container for child elements. Any rect wholly
  // contained by containerRect can be skipped.
  virtual void addLayerHitTestRects(LayerHitTestRects&,
                                    const PaintLayer* currentLayer,
                                    const LayoutPoint& layerOffset,
                                    const LayoutRect& containerRect) const;

  // Add hit-test rects for this layoutObject only to the provided list.
  // layerOffset is the offset of this layoutObject within the current layer
  // that should be used for each result.
  virtual void computeSelfHitTestRects(Vector<LayoutRect>&,
                                       const LayoutPoint& layerOffset) const {}

  void setPreviousVisualRect(const LayoutRect& rect) {
    m_previousVisualRect = rect;
  }

#if DCHECK_IS_ON()
  virtual bool paintInvalidationStateIsDirty() const {
    return backgroundChangedSinceLastPaintInvalidation() ||
           shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState();
  }
#endif

  // Called before paint invalidation.
  virtual void ensureIsReadyForPaintInvalidation() { DCHECK(!needsLayout()); }

  // This function walks the descendants of |this|, following a
  // layout ordering.
  //
  // The ordering is important for PaintInvalidationState, as it requires to be
  // called following a descendant/container relationship.
  //
  // The function is overridden to handle special children (e.g. percentage
  // height descendants or reflections).
  virtual void invalidatePaintOfSubtreesIfNeeded(
      const PaintInvalidationState& childPaintInvalidationState);

  // This function generates the invalidation for this object only.
  // It doesn't recurse into other object, as this is handled by
  // invalidatePaintOfSubtreesIfNeeded.
  virtual PaintInvalidationReason invalidatePaintIfNeeded(
      const PaintInvalidationState&);

  void setIsBackgroundAttachmentFixedObject(bool);

  void clearSelfNeedsOverflowRecalcAfterStyleChange() {
    m_bitfields.setSelfNeedsOverflowRecalcAfterStyleChange(false);
  }
  void setEverHadLayout() { m_bitfields.setEverHadLayout(true); }

  // Remove this object and all descendants from the containing
  // LayoutFlowThread.
  void removeFromLayoutFlowThread();

  bool containsInlineWithOutlineAndContinuation() const {
    return m_bitfields.containsInlineWithOutlineAndContinuation();
  }
  void setContainsInlineWithOutlineAndContinuation(bool b) {
    m_bitfields.setContainsInlineWithOutlineAndContinuation(b);
  }

 private:
  // Adjusts a visual rect in the space of |m_previousVisualRect| to be in the
  // space of the |paintInvalidationContainer|, if needed. They can be different
  // only if |paintInvalidationContainer| is a composited scroller.
  void adjustVisualRectForCompositedScrolling(
      LayoutRect&,
      const LayoutBoxModelObject& paintInvalidationContainer) const;

  FloatQuad localToAncestorQuadInternal(const FloatQuad&,
                                        const LayoutBoxModelObject* ancestor,
                                        MapCoordinatesFlags = 0) const;

  void clearLayoutRootIfNeeded() const;

  bool isInert() const;

  void updateImage(StyleImage*, StyleImage*);

  void scheduleRelayout();

  void updateShapeImage(const ShapeValue*, const ShapeValue*);
  void updateFillImages(const FillLayer* oldLayers, const FillLayer& newLayers);
  void updateCursorImages(const CursorList* oldCursors,
                          const CursorList* newCursors);

  void setNeedsOverflowRecalcAfterStyleChange();

  // Walk up the parent chain and find the first scrolling block to disable
  // scroll anchoring on.
  void setScrollAnchorDisablingStyleChangedOnAncestor();

  // FIXME: This should be 'markContaingBoxChainForOverflowRecalc when we make
  // LayoutBox recomputeOverflow-capable. crbug.com/437012 and crbug.com/434700.
  inline void markAncestorsForOverflowRecalcIfNeeded();

  inline void markAncestorsForPaintInvalidation();

  inline void invalidateContainerPreferredLogicalWidths();

  void invalidatePaintIncludingNonSelfPaintingLayerDescendantsInternal(
      const LayoutBoxModelObject& paintInvalidationContainer);

  LayoutObject* containerForAbsolutePosition(AncestorSkipInfo* = nullptr) const;

  const LayoutBoxModelObject* enclosingCompositedContainer() const;

  LayoutFlowThread* locateFlowThreadContainingBlock() const;
  void removeFromLayoutFlowThreadRecursive(LayoutFlowThread*);

  ComputedStyle* cachedFirstLineStyle() const;
  StyleDifference adjustStyleDifference(StyleDifference) const;

  Color selectionColor(int colorProperty, const GlobalPaintFlags) const;

  void removeShapeImageClient(ShapeValue*);
  void removeCursorImageClient(const CursorList*);

#if DCHECK_IS_ON()
  void checkBlockPositionedObjectsNeedLayout();
#endif

  bool isTextOrSVGChild() const { return isText() || isSVGChild(); }

  static bool isAllowedToModifyLayoutTreeStructure(Document&);

  // Returns the parent for paint invalidation. For LayoutView, returns the
  // owner layout object in the containing frame if any, or nullptr.
  inline LayoutObject* paintInvalidationParent() const;
  LayoutObject* slowPaintInvalidationParentForTesting() const;

  RefPtr<ComputedStyle> m_style;

  // Oilpan: This untraced pointer to the owning Node is considered safe.
  UntracedMember<Node> m_node;

  LayoutObject* m_parent;
  LayoutObject* m_previous;
  LayoutObject* m_next;

#if DCHECK_IS_ON()
  unsigned m_hasAXObject : 1;
  unsigned m_setNeedsLayoutForbidden : 1;
#endif

#define ADD_BOOLEAN_BITFIELD(name, Name) \
 private:                                \
  unsigned m_##name : 1;                 \
                                         \
 public:                                 \
  bool name() const { return m_##name; } \
  void set##Name(bool name) { m_##name = name; }

  class LayoutObjectBitfields {
    enum PositionedState {
      IsStaticallyPositioned = 0,
      IsRelativelyPositioned = 1,
      IsOutOfFlowPositioned = 2,
      IsStickyPositioned = 3,
    };

   public:
    // LayoutObjectBitfields holds all the boolean values for LayoutObject.
    //
    // This is done to promote better packing on LayoutObject (at the expense of
    // preventing bit field packing for the subclasses). Classes concerned about
    // packing and memory use should hoist their boolean to this class. See
    // below the field from sub-classes (e.g. childrenInline).
    //
    // Some of those booleans are caches of ComputedStyle values (e.g.
    // positionState). This enables better memory locality and thus better
    // performance.
    //
    // This class is an artifact of the WebKit era where LayoutObject wasn't
    // allowed to grow and each sub-class was strictly monitored for memory
    // increase. Our measurements indicate that the size of LayoutObject and
    // subsequent classes do not impact memory or speed in a significant
    // manner. This is based on growing LayoutObject in
    // https://codereview.chromium.org/44673003 and subsequent relaxations
    // of the memory constraints on layout objects.
    LayoutObjectBitfields(Node* node)
        : m_selfNeedsLayout(false),
          m_needsPositionedMovementLayout(false),
          m_normalChildNeedsLayout(false),
          m_posChildNeedsLayout(false),
          m_needsSimplifiedNormalFlowLayout(false),
          m_selfNeedsOverflowRecalcAfterStyleChange(false),
          m_childNeedsOverflowRecalcAfterStyleChange(false),
          m_preferredLogicalWidthsDirty(false),
          m_childShouldCheckForPaintInvalidation(false),
          m_mayNeedPaintInvalidation(false),
          m_mayNeedPaintInvalidationSubtree(false),
          m_mayNeedPaintInvalidationAnimatedBackgroundImage(false),
          m_shouldInvalidateSelection(false),
          m_floating(false),
          m_isAnonymous(!node),
          m_isText(false),
          m_isBox(false),
          m_isInline(true),
          m_isAtomicInlineLevel(false),
          m_horizontalWritingMode(true),
          m_hasLayer(false),
          m_hasOverflowClip(false),
          m_hasTransformRelatedProperty(false),
          m_hasReflection(false),
          m_hasCounterNodeMap(false),
          m_everHadLayout(false),
          m_ancestorLineBoxDirty(false),
          m_isInsideFlowThread(false),
          m_subtreeChangeListenerRegistered(false),
          m_notifiedOfSubtreeChange(false),
          m_consumesSubtreeChangeNotification(false),
          m_childrenInline(false),
          m_containsInlineWithOutlineAndContinuation(false),
          m_alwaysCreateLineBoxesForLayoutInline(false),
          m_previousBackgroundObscured(false),
          m_isBackgroundAttachmentFixedObject(false),
          m_isScrollAnchorObject(false),
          m_scrollAnchorDisablingStyleChanged(false),
          m_hasBoxDecorationBackground(false),
          m_hasPreviousLocationInBacking(false),
          m_hasPreviousSelectionVisualRect(false),
          m_hasPreviousBoxGeometries(false),
          m_needsPaintPropertyUpdate(true),
          m_subtreeNeedsPaintPropertyUpdate(true),
          m_descendantNeedsPaintPropertyUpdate(true),
          m_backgroundChangedSinceLastPaintInvalidation(false),
          m_positionedState(IsStaticallyPositioned),
          m_selectionState(SelectionNone),
          m_backgroundObscurationState(BackgroundObscurationStatusInvalid),
          m_fullPaintInvalidationReason(PaintInvalidationNone) {}

    // Self needs layout means that this layout object is marked for a full
    // layout. This is the default layout but it is expensive as it recomputes
    // everything. For CSS boxes, this includes the width (laying out the line
    // boxes again), the margins (due to block collapsing margins), the
    // positions, the height and the potential overflow.
    ADD_BOOLEAN_BITFIELD(selfNeedsLayout, SelfNeedsLayout);

    // A positioned movement layout is a specialized type of layout used on
    // positioned objects that only visually moved. This layout is used when
    // changing 'top'/'left' on a positioned element or margins on an
    // out-of-flow one. Because the following operations don't impact the size
    // of the object or sibling LayoutObjects, this layout is very lightweight.
    //
    // Positioned movement layout is implemented in
    // LayoutBlock::simplifiedLayout.
    ADD_BOOLEAN_BITFIELD(needsPositionedMovementLayout,
                         NeedsPositionedMovementLayout);

    // This boolean is set when a normal flow ('position' == static || relative)
    // child requires layout (but this object doesn't). Due to the nature of
    // CSS, laying out a child can cause the parent to resize (e.g., if 'height'
    // is auto).
    ADD_BOOLEAN_BITFIELD(normalChildNeedsLayout, NormalChildNeedsLayout);

    // This boolean is set when an out-of-flow positioned ('position' == fixed
    // || absolute) child requires layout (but this object doesn't).
    ADD_BOOLEAN_BITFIELD(posChildNeedsLayout, PosChildNeedsLayout);

    // Simplified normal flow layout only relayouts the normal flow children,
    // ignoring the out-of-flow descendants.
    //
    // The implementation of this layout is in
    // LayoutBlock::simplifiedNormalFlowLayout.
    ADD_BOOLEAN_BITFIELD(needsSimplifiedNormalFlowLayout,
                         NeedsSimplifiedNormalFlowLayout);

    // Some properties only have a visual impact and don't impact the actual
    // layout position and sizes of the object. An example of this is the
    // 'transform' property, who doesn't modify the layout but gets applied at
    // paint time. Setting this flag only recomputes the overflow information.
    ADD_BOOLEAN_BITFIELD(selfNeedsOverflowRecalcAfterStyleChange,
                         SelfNeedsOverflowRecalcAfterStyleChange);

    // This flag is set on the ancestor of a LayoutObject needing
    // selfNeedsOverflowRecalcAfterStyleChange. This is needed as a descendant
    // overflow can bleed into its containing block's so we have to recompute it
    // in some cases.
    ADD_BOOLEAN_BITFIELD(childNeedsOverflowRecalcAfterStyleChange,
                         ChildNeedsOverflowRecalcAfterStyleChange);

    // This boolean marks preferred logical widths for lazy recomputation.
    //
    // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS above about those
    // widths.
    ADD_BOOLEAN_BITFIELD(preferredLogicalWidthsDirty,
                         PreferredLogicalWidthsDirty);

    ADD_BOOLEAN_BITFIELD(childShouldCheckForPaintInvalidation,
                         ChildShouldCheckForPaintInvalidation);
    ADD_BOOLEAN_BITFIELD(mayNeedPaintInvalidation, MayNeedPaintInvalidation);
    ADD_BOOLEAN_BITFIELD(mayNeedPaintInvalidationSubtree,
                         MayNeedPaintInvalidationSubtree);
    ADD_BOOLEAN_BITFIELD(mayNeedPaintInvalidationAnimatedBackgroundImage,
                         MayNeedPaintInvalidationAnimatedBackgroundImage);
    ADD_BOOLEAN_BITFIELD(shouldInvalidateSelection, ShouldInvalidateSelection);

    // This boolean is the cached value of 'float'
    // (see ComputedStyle::isFloating).
    ADD_BOOLEAN_BITFIELD(floating, Floating);

    ADD_BOOLEAN_BITFIELD(isAnonymous, IsAnonymous);
    ADD_BOOLEAN_BITFIELD(isText, IsText);
    ADD_BOOLEAN_BITFIELD(isBox, IsBox);

    // This boolean represents whether the LayoutObject is 'inline-level'
    // (a CSS concept). Inline-level boxes are laid out inside a line. If
    // unset, the box is 'block-level' and thus stack on top of its
    // siblings (think of paragraphs).
    ADD_BOOLEAN_BITFIELD(isInline, IsInline);

    // This boolean is set if the element is an atomic inline-level box.
    //
    // In CSS, atomic inline-level boxes are laid out on a line but they
    // are opaque from the perspective of line layout. This means that they
    // can't be split across lines like normal inline boxes (LayoutInline).
    // Examples of atomic inline-level elements: inline tables, inline
    // blocks and replaced inline elements.
    // See http://www.w3.org/TR/CSS2/visuren.html#inline-boxes.
    //
    // Our code is confused about the use of this boolean and confuses it
    // with being replaced (see LayoutReplaced about this).
    // TODO(jchaffraix): We should inspect callers and clarify their use.
    // TODO(jchaffraix): We set this boolean for replaced elements that are
    // not inline but shouldn't (crbug.com/567964). This should be enforced.
    ADD_BOOLEAN_BITFIELD(isAtomicInlineLevel, IsAtomicInlineLevel);
    ADD_BOOLEAN_BITFIELD(horizontalWritingMode, HorizontalWritingMode);

    ADD_BOOLEAN_BITFIELD(hasLayer, HasLayer);

    // This boolean is set if overflow != 'visible'.
    // This means that this object may need an overflow clip to be applied
    // at paint time to its visual overflow (see OverflowModel for more
    // details). Only set for LayoutBoxes and descendants.
    ADD_BOOLEAN_BITFIELD(hasOverflowClip, HasOverflowClip);

    // This boolean is the cached value from
    // ComputedStyle::hasTransformRelatedProperty.
    ADD_BOOLEAN_BITFIELD(hasTransformRelatedProperty,
                         HasTransformRelatedProperty);
    ADD_BOOLEAN_BITFIELD(hasReflection, HasReflection);

    // This boolean is used to know if this LayoutObject has one (or more)
    // associated CounterNode(s).
    // See class comment in LayoutCounter.h for more detail.
    ADD_BOOLEAN_BITFIELD(hasCounterNodeMap, HasCounterNodeMap);

    ADD_BOOLEAN_BITFIELD(everHadLayout, EverHadLayout);
    ADD_BOOLEAN_BITFIELD(ancestorLineBoxDirty, AncestorLineBoxDirty);

    ADD_BOOLEAN_BITFIELD(isInsideFlowThread, IsInsideFlowThread);

    ADD_BOOLEAN_BITFIELD(subtreeChangeListenerRegistered,
                         SubtreeChangeListenerRegistered);
    ADD_BOOLEAN_BITFIELD(notifiedOfSubtreeChange, NotifiedOfSubtreeChange);
    ADD_BOOLEAN_BITFIELD(consumesSubtreeChangeNotification,
                         ConsumesSubtreeChangeNotification);

    // from LayoutBlock
    ADD_BOOLEAN_BITFIELD(childrenInline, ChildrenInline);

    // from LayoutBlockFlow
    ADD_BOOLEAN_BITFIELD(containsInlineWithOutlineAndContinuation,
                         ContainsInlineWithOutlineAndContinuation);

    // from LayoutInline
    ADD_BOOLEAN_BITFIELD(alwaysCreateLineBoxesForLayoutInline,
                         AlwaysCreateLineBoxesForLayoutInline);

    // Background obscuration status of the previous frame.
    ADD_BOOLEAN_BITFIELD(previousBackgroundObscured,
                         PreviousBackgroundObscured);

    ADD_BOOLEAN_BITFIELD(isBackgroundAttachmentFixedObject,
                         IsBackgroundAttachmentFixedObject);
    ADD_BOOLEAN_BITFIELD(isScrollAnchorObject, IsScrollAnchorObject);

    // Whether changes in this LayoutObject's CSS properties since the last
    // layout should suppress any adjustments that would be made during the next
    // layout by ScrollAnchor objects for which this LayoutObject is on the path
    // from the anchor node to the scroller.
    // See http://bit.ly/sanaclap for more info.
    ADD_BOOLEAN_BITFIELD(scrollAnchorDisablingStyleChanged,
                         ScrollAnchorDisablingStyleChanged);

    ADD_BOOLEAN_BITFIELD(hasBoxDecorationBackground,
                         HasBoxDecorationBackground);

    ADD_BOOLEAN_BITFIELD(hasPreviousLocationInBacking,
                         HasPreviousLocationInBacking);
    ADD_BOOLEAN_BITFIELD(hasPreviousSelectionVisualRect,
                         HasPreviousSelectionVisualRect);
    ADD_BOOLEAN_BITFIELD(hasPreviousBoxGeometries, HasPreviousBoxGeometries);

    // Whether the paint properties need to be updated. For more details, see
    // LayoutObject::needsPaintPropertyUpdate().
    ADD_BOOLEAN_BITFIELD(needsPaintPropertyUpdate, NeedsPaintPropertyUpdate);
    // Whether paint properties of the whole subtree need to be updated.
    ADD_BOOLEAN_BITFIELD(subtreeNeedsPaintPropertyUpdate,
                         SubtreeNeedsPaintPropertyUpdate)
    // Whether the paint properties of a descendant need to be updated. For more
    // details, see LayoutObject::descendantNeedsPaintPropertyUpdate().
    ADD_BOOLEAN_BITFIELD(descendantNeedsPaintPropertyUpdate,
                         DescendantNeedsPaintPropertyUpdate);

    ADD_BOOLEAN_BITFIELD(backgroundChangedSinceLastPaintInvalidation,
                         BackgroundChangedSinceLastPaintInvalidation);

   protected:
    // Use protected to avoid warning about unused variable.
    unsigned m_unusedBits : 6;

   private:
    // This is the cached 'position' value of this object
    // (see ComputedStyle::position).
    unsigned m_positionedState : 2;  // PositionedState
    unsigned m_selectionState : 3;   // SelectionState
    // Mutable for getter which lazily update this field.
    mutable unsigned
        m_backgroundObscurationState : 2;        // BackgroundObscurationState
    unsigned m_fullPaintInvalidationReason : 5;  // PaintInvalidationReason

   public:
    bool isOutOfFlowPositioned() const {
      return m_positionedState == IsOutOfFlowPositioned;
    }
    bool isRelPositioned() const {
      return m_positionedState == IsRelativelyPositioned;
    }
    bool isStickyPositioned() const {
      return m_positionedState == IsStickyPositioned;
    }
    bool isInFlowPositioned() const {
      return m_positionedState == IsRelativelyPositioned ||
             m_positionedState == IsStickyPositioned;
    }
    bool isPositioned() const {
      return m_positionedState != IsStaticallyPositioned;
    }

    void setPositionedState(int positionState) {
      // This mask maps FixedPosition and AbsolutePosition to
      // IsOutOfFlowPositioned, saving one bit.
      m_positionedState = static_cast<PositionedState>(positionState & 0x3);
    }
    void clearPositionedState() { m_positionedState = StaticPosition; }

    ALWAYS_INLINE SelectionState getSelectionState() const {
      return static_cast<SelectionState>(m_selectionState);
    }
    ALWAYS_INLINE void setSelectionState(SelectionState selectionState) {
      m_selectionState = selectionState;
    }

    ALWAYS_INLINE BackgroundObscurationState
    getBackgroundObscurationState() const {
      return static_cast<BackgroundObscurationState>(
          m_backgroundObscurationState);
    }
    ALWAYS_INLINE void setBackgroundObscurationState(
        BackgroundObscurationState s) const {
      m_backgroundObscurationState = s;
    }

    PaintInvalidationReason fullPaintInvalidationReason() const {
      return static_cast<PaintInvalidationReason>(
          m_fullPaintInvalidationReason);
    }
    void setFullPaintInvalidationReason(PaintInvalidationReason reason) {
      m_fullPaintInvalidationReason = reason;
    }
  };

#undef ADD_BOOLEAN_BITFIELD

  LayoutObjectBitfields m_bitfields;

  void setSelfNeedsLayout(bool b) { m_bitfields.setSelfNeedsLayout(b); }
  void setNeedsPositionedMovementLayout(bool b) {
    m_bitfields.setNeedsPositionedMovementLayout(b);
  }
  void setNormalChildNeedsLayout(bool b) {
    m_bitfields.setNormalChildNeedsLayout(b);
  }
  void setPosChildNeedsLayout(bool b) { m_bitfields.setPosChildNeedsLayout(b); }
  void setNeedsSimplifiedNormalFlowLayout(bool b) {
    m_bitfields.setNeedsSimplifiedNormalFlowLayout(b);
  }
  void setSelfNeedsOverflowRecalcAfterStyleChange() {
    m_bitfields.setSelfNeedsOverflowRecalcAfterStyleChange(true);
  }
  void setChildNeedsOverflowRecalcAfterStyleChange() {
    m_bitfields.setChildNeedsOverflowRecalcAfterStyleChange(true);
  }

 private:
  // Store state between styleWillChange and styleDidChange
  static bool s_affectsParentBlock;

  // This stores the visual rect computed by the latest paint invalidation.
  // This rect does *not* account for composited scrolling. See
  // adjustVisualRectForCompositedScrolling().
  LayoutRect m_previousVisualRect;

  // This stores the paint offset computed by the latest paint property tree
  // building. It is relative to the containing transform space. It is the same
  // offset that will be used to paint the object on SPv2. It's used to detect
  // paint offset change for paint invalidation on SPv2, and partial paint
  // property tree update for SlimmingPaintInvalidation on SPv1 and SPv2.
  LayoutPoint m_paintOffset;

  // For SPv2 only. The ObjectPaintProperties structure holds references to the
  // property tree nodes that are created by the layout object for painting.
  std::unique_ptr<ObjectPaintProperties> m_paintProperties;
};

// FIXME: remove this once the layout object lifecycle ASSERTS are no longer
// hit.
class DeprecatedDisableModifyLayoutTreeStructureAsserts {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(DeprecatedDisableModifyLayoutTreeStructureAsserts);

 public:
  DeprecatedDisableModifyLayoutTreeStructureAsserts();

  static bool canModifyLayoutTreeStateInAnyState();

 private:
  AutoReset<bool> m_disabler;
};

// Allow equality comparisons of LayoutObjects by reference or pointer,
// interchangeably.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(LayoutObject)

inline bool LayoutObject::documentBeingDestroyed() const {
  return document().lifecycle().state() >= DocumentLifecycle::Stopping;
}

inline bool LayoutObject::isBeforeContent() const {
  if (style()->styleType() != PseudoIdBefore)
    return false;
  // Text nodes don't have their own styles, so ignore the style on a text node.
  if (isText() && !isBR())
    return false;
  return true;
}

inline bool LayoutObject::isAfterContent() const {
  if (style()->styleType() != PseudoIdAfter)
    return false;
  // Text nodes don't have their own styles, so ignore the style on a text node.
  if (isText() && !isBR())
    return false;
  return true;
}

inline bool LayoutObject::isBeforeOrAfterContent() const {
  return isBeforeContent() || isAfterContent();
}

// setNeedsLayout() won't cause full paint invalidations as
// setNeedsLayoutAndFullPaintInvalidation() does. Otherwise the two methods are
// identical.
inline void LayoutObject::setNeedsLayout(
    LayoutInvalidationReasonForTracing reason,
    MarkingBehavior markParents,
    SubtreeLayoutScope* layouter) {
#if DCHECK_IS_ON()
  DCHECK(!isSetNeedsLayoutForbidden());
#endif
  bool alreadyNeededLayout = m_bitfields.selfNeedsLayout();
  setSelfNeedsLayout(true);
  if (!alreadyNeededLayout) {
    TRACE_EVENT_INSTANT1(
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
        "LayoutInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorLayoutInvalidationTrackingEvent::data(this, reason));
    if (markParents == MarkContainerChain &&
        (!layouter || layouter->root() != this))
      markContainerChainForLayout(!layouter, layouter);
  }
}

inline void LayoutObject::setNeedsLayoutAndFullPaintInvalidation(
    LayoutInvalidationReasonForTracing reason,
    MarkingBehavior markParents,
    SubtreeLayoutScope* layouter) {
  setNeedsLayout(reason, markParents, layouter);
  setShouldDoFullPaintInvalidation();
}

inline void LayoutObject::clearNeedsLayout() {
  // Set flags for later stages/cycles.
  setEverHadLayout();
  setMayNeedPaintInvalidation();

  // Clear needsLayout flags.
  setSelfNeedsLayout(false);
  setPosChildNeedsLayout(false);
  setNeedsSimplifiedNormalFlowLayout(false);
  setNormalChildNeedsLayout(false);
  setNeedsPositionedMovementLayout(false);
  setAncestorLineBoxDirty(false);

#if DCHECK_IS_ON()
  checkBlockPositionedObjectsNeedLayout();
#endif

  setScrollAnchorDisablingStyleChanged(false);
}

inline void LayoutObject::setChildNeedsLayout(MarkingBehavior markParents,
                                              SubtreeLayoutScope* layouter) {
#if DCHECK_IS_ON()
  DCHECK(!isSetNeedsLayoutForbidden());
#endif
  bool alreadyNeededLayout = normalChildNeedsLayout();
  setNormalChildNeedsLayout(true);
  // FIXME: Replace MarkOnlyThis with the SubtreeLayoutScope code path and
  // remove the MarkingBehavior argument entirely.
  if (!alreadyNeededLayout && markParents == MarkContainerChain &&
      (!layouter || layouter->root() != this))
    markContainerChainForLayout(!layouter, layouter);
}

inline void LayoutObject::setNeedsPositionedMovementLayout() {
  bool alreadyNeededLayout = needsPositionedMovementLayout();
  setNeedsPositionedMovementLayout(true);
#if DCHECK_IS_ON()
  DCHECK(!isSetNeedsLayoutForbidden());
#endif
  if (!alreadyNeededLayout)
    markContainerChainForLayout();
}

inline bool LayoutObject::preservesNewline() const {
  if (isSVGInlineText())
    return false;

  return style()->preserveNewline();
}

inline bool LayoutObject::layerCreationAllowedForSubtree() const {
  LayoutObject* parentLayoutObject = parent();
  while (parentLayoutObject) {
    if (parentLayoutObject->isSVGHiddenContainer())
      return false;
    parentLayoutObject = parentLayoutObject->parent();
  }

  return true;
}

inline void LayoutObject::setSelectionStateIfNeeded(SelectionState state) {
  if (getSelectionState() == state)
    return;

  setSelectionState(state);
}

inline void LayoutObject::setHasBoxDecorationBackground(bool b) {
  if (b == m_bitfields.hasBoxDecorationBackground())
    return;

  m_bitfields.setHasBoxDecorationBackground(b);
  invalidateBackgroundObscurationStatus();
}

inline void LayoutObject::invalidateBackgroundObscurationStatus() {
  m_bitfields.setBackgroundObscurationState(BackgroundObscurationStatusInvalid);
}

DISABLE_CFI_PERF
inline bool LayoutObject::backgroundIsKnownToBeObscured() const {
  if (m_bitfields.getBackgroundObscurationState() ==
      BackgroundObscurationStatusInvalid) {
    BackgroundObscurationState state = computeBackgroundIsKnownToBeObscured()
                                           ? BackgroundKnownToBeObscured
                                           : BackgroundMayBeVisible;
    m_bitfields.setBackgroundObscurationState(state);
  }
  return m_bitfields.getBackgroundObscurationState() ==
         BackgroundKnownToBeObscured;
}

inline void makeMatrixRenderable(TransformationMatrix& matrix,
                                 bool has3DRendering) {
  if (!has3DRendering)
    matrix.makeAffine();
}

inline int adjustForAbsoluteZoom(int value, LayoutObject* layoutObject) {
  return adjustForAbsoluteZoom(value, layoutObject->style());
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value,
                                                  LayoutObject& layoutObject) {
  ASSERT(layoutObject.style());
  return adjustLayoutUnitForAbsoluteZoom(value, *layoutObject.style());
}

inline void adjustFloatQuadForAbsoluteZoom(FloatQuad& quad,
                                           LayoutObject& layoutObject) {
  float zoom = layoutObject.style()->effectiveZoom();
  if (zoom != 1)
    quad.scale(1 / zoom, 1 / zoom);
}

inline void adjustFloatRectForAbsoluteZoom(FloatRect& rect,
                                           LayoutObject& layoutObject) {
  float zoom = layoutObject.style()->effectiveZoom();
  if (zoom != 1)
    rect.scale(1 / zoom, 1 / zoom);
}

inline double adjustScrollForAbsoluteZoom(double value,
                                          LayoutObject& layoutObject) {
  ASSERT(layoutObject.style());
  return adjustScrollForAbsoluteZoom(value, *layoutObject.style());
}

#define DEFINE_LAYOUT_OBJECT_TYPE_CASTS(thisType, predicate)           \
  DEFINE_TYPE_CASTS(thisType, LayoutObject, object, object->predicate, \
                    object.predicate)

}  // namespace blink

#ifndef NDEBUG
// Outside the blink namespace for ease of invocation from gdb.
CORE_EXPORT void showTree(const blink::LayoutObject*);
CORE_EXPORT void showLineTree(const blink::LayoutObject*);
CORE_EXPORT void showLayoutTree(const blink::LayoutObject* object1);
// We don't make object2 an optional parameter so that showLayoutTree
// can be called from gdb easily.
CORE_EXPORT void showLayoutTree(const blink::LayoutObject* object1,
                                const blink::LayoutObject* object2);

#endif

#endif  // LayoutObject_h
