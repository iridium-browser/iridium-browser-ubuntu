/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXNodeObject_h
#define AXNodeObject_h

#include "modules/ModulesExport.h"
#include "modules/accessibility/AXObject.h"
#include "wtf/Forward.h"

namespace blink {

class AXObjectCacheImpl;
class Element;
class HTMLLabelElement;
class Node;

class MODULES_EXPORT AXNodeObject : public AXObject {
  WTF_MAKE_NONCOPYABLE(AXNodeObject);

 protected:
  AXNodeObject(Node*, AXObjectCacheImpl&);

 public:
  static AXNodeObject* create(Node*, AXObjectCacheImpl&);
  ~AXNodeObject() override;
  DECLARE_VIRTUAL_TRACE();

 protected:
  // Protected data.
  AccessibilityRole m_ariaRole;
  bool m_childrenDirty;
#if DCHECK_IS_ON()
  bool m_initialized = false;
#endif

  bool computeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  const AXObject* inheritsPresentationalRoleFrom() const override;
  virtual AccessibilityRole determineAccessibilityRole();
  virtual AccessibilityRole nativeAccessibilityRoleIgnoringAria() const;
  String accessibilityDescriptionForElements(
      HeapVector<Member<Element>>& elements) const;
  void alterSliderValue(bool increase);
  AXObject* activeDescendant() override;
  String ariaAccessibilityDescription() const;
  String ariaAutoComplete() const;
  AccessibilityRole determineAriaRoleAttribute() const;
  void accessibilityChildrenFromAttribute(QualifiedName attr,
                                          AXObject::AXObjectVector&) const;

  bool hasContentEditableAttributeSet() const;
  bool isTextControl() const override;
  // This returns true if it's focusable but it's not content editable and it's
  // not a control or ARIA control.
  bool isGenericFocusableElement() const;
  AXObject* menuButtonForMenu() const;
  Element* menuItemElementForMenu() const;
  Element* mouseButtonListener() const;
  AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;
  bool isNativeCheckboxOrRadio() const;
  void setNode(Node*);
  AXObject* correspondingControlForLabelElement() const;
  HTMLLabelElement* labelElementContainer() const;

  //
  // Overridden from AXObject.
  //

  void init() override;
  void detach() override;
  bool isDetached() const override { return !m_node; }
  bool isAXNodeObject() const final { return true; }

  void getSparseAXAttributes(AXSparseAttributeClient&) const override;

  // Check object role or purpose.
  bool isAnchor() const final;
  bool isControl() const override;
  bool isControllingVideoElement() const;
  bool isEditable() const override { return isNativeTextControl(); }
  bool isEmbeddedObject() const final;
  bool isFieldset() const final;
  bool isHeading() const final;
  bool isHovered() const final;
  bool isImage() const final;
  bool isImageButton() const;
  bool isInputImage() const final;
  bool isLink() const override;
  bool isMenu() const final;
  bool isMenuButton() const final;
  bool isMeter() const final;
  bool isMultiSelectable() const override;
  bool isNativeImage() const;
  bool isNativeTextControl() const final;
  bool isNonNativeTextControl() const final;
  bool isPasswordField() const final;
  bool isProgressIndicator() const override;
  bool isRichlyEditable() const override;
  bool isSlider() const override;
  bool isNativeSlider() const override;

  // Check object state.
  bool isChecked() const final;
  bool isClickable() const final;
  bool isEnabled() const override;
  AccessibilityExpanded isExpanded() const override;
  bool isModal() const final;
  bool isPressed() const final;
  bool isReadOnly() const override;
  bool isRequired() const final;

  // Check whether certain properties can be modified.
  bool canSetFocusAttribute() const override;
  bool canSetValueAttribute() const override;
  bool canSetSelectedAttribute() const override;

  // Properties of static elements.
  RGBA32 colorValue() const final;
  bool canvasHasFallbackContent() const final;
  int headingLevel() const final;
  unsigned hierarchicalLevel() const final;
  void markers(Vector<DocumentMarker::MarkerType>&,
               Vector<AXRange>&) const override;
  AccessibilityOrientation orientation() const override;
  String text() const override;

  // Properties of interactive elements.
  AccessibilityButtonState checkboxOrRadioValue() const final;
  AriaCurrentState ariaCurrentState() const final;
  InvalidState getInvalidState() const final;
  // Only used when invalidState() returns InvalidStateOther.
  String ariaInvalidValue() const final;
  String valueDescription() const override;
  float valueForRange() const override;
  float maxValueForRange() const override;
  float minValueForRange() const override;
  String stringValue() const override;

  // ARIA attributes.
  AccessibilityRole ariaRoleAttribute() const final;

  // AX name calculation.
  String textAlternative(bool recursive,
                         bool inAriaLabelledByTraversal,
                         AXObjectSet& visited,
                         AXNameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  String description(AXNameFrom,
                     AXDescriptionFrom&,
                     AXObjectVector* descriptionObjects) const override;
  String description(AXNameFrom,
                     AXDescriptionFrom&,
                     DescriptionSources*,
                     AXRelatedObjectVector*) const override;
  String placeholder(AXNameFrom) const override;
  bool nameFromLabelElement() const override;
  bool nameFromContents() const override;

  // Location
  void getRelativeBounds(AXObject** outContainer,
                         FloatRect& outBoundsInContainer,
                         SkMatrix44& outContainerTransform) const override;

  // High-level accessibility tree access.
  AXObject* computeParent() const override;
  AXObject* computeParentIfExists() const override;

  // Low-level accessibility tree exploration.
  AXObject* rawFirstChild() const override;
  AXObject* rawNextSibling() const override;
  void addChildren() override;
  bool canHaveChildren() const override;
  void addChild(AXObject*);
  void insertChild(AXObject*, unsigned index);

  // DOM and Render tree access.
  Element* actionElement() const override;
  Element* anchorElement() const override;
  Document* getDocument() const override;
  Node* getNode() const override { return m_node; }

  // Modify or take an action on an object.
  void setFocused(bool) final;
  void increment() final;
  void decrement() final;
  void setSequentialFocusNavigationStartingPoint() final;

  // Notifications that this object may have changed.
  void childrenChanged() override;
  void selectionChanged() final;
  void textChanged() override;
  void updateAccessibilityRole() final;

  // Position in set and Size of set
  int posInSet() const override;
  int setSize() const override;

  // Aria-owns.
  void computeAriaOwnsChildren(
      HeapVector<Member<AXObject>>& ownedChildren) const;

 private:
  Member<Node> m_node;

  bool isNativeCheckboxInMixedState() const;
  String textFromDescendants(AXObjectSet& visited,
                             bool recursive) const override;
  String nativeTextAlternative(AXObjectSet& visited,
                               AXNameFrom&,
                               AXRelatedObjectVector*,
                               NameSources*,
                               bool* foundTextAlternative) const;
  float stepValueForRange() const;
  bool isDescendantOfElementType(HashSet<QualifiedName>& tagNames) const;
  String placeholderFromNativeAttribute() const;
};

}  // namespace blink

#endif  // AXNodeObject_h
