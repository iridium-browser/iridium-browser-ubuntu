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
class LayoutRect;
class Node;

class MODULES_EXPORT AXNodeObject : public AXObject {
protected:
    AXNodeObject(Node*, AXObjectCacheImpl*);

public:
    static PassRefPtr<AXNodeObject> create(Node*, AXObjectCacheImpl*);
    virtual ~AXNodeObject();

protected:
    // Protected data.
    AccessibilityRole m_ariaRole;
    bool m_childrenDirty;
#if ENABLE(ASSERT)
    bool m_initialized;
#endif

    virtual bool computeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
    virtual const AXObject* inheritsPresentationalRoleFrom() const override;
    virtual AccessibilityRole determineAccessibilityRole();
    AccessibilityRole determineAccessibilityRoleUtil();
    String accessibilityDescriptionForElements(WillBeHeapVector<RawPtrWillBeMember<Element>> &elements) const;
    void alterSliderValue(bool increase);
    String ariaAccessibilityDescription() const;
    String ariaAutoComplete() const;
    void ariaLabeledByElements(WillBeHeapVector<RawPtrWillBeMember<Element>>& elements) const;
    AccessibilityRole determineAriaRoleAttribute() const;
    void elementsFromAttribute(WillBeHeapVector<RawPtrWillBeMember<Element>>& elements, const QualifiedName&) const;
    bool hasContentEditableAttributeSet() const;
    bool isTextControl() const override;
    bool allowsTextRanges() const { return isTextControl(); }
    // This returns true if it's focusable but it's not content editable and it's not a control or ARIA control.
    bool isGenericFocusableElement() const;
    HTMLLabelElement* labelForElement(Element*) const;
    AXObject* menuButtonForMenu() const;
    Element* menuItemElementForMenu() const;
    Element* mouseButtonListener() const;
    String deprecatedPlaceholder() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;
    bool isNativeCheckboxOrRadio() const;
    void setNode(Node*);
    AXObject* correspondingControlForLabelElement() const;
    HTMLLabelElement* labelElementContainer() const;

    //
    // Overridden from AXObject.
    //

    virtual void init() override;
    virtual void detach() override;
    virtual bool isDetached() const override { return !m_node; }
    virtual bool isAXNodeObject() const override final { return true; }

    // Check object role or purpose.
    virtual bool isAnchor() const override final;
    virtual bool isControl() const override;
    bool isControllingVideoElement() const;
    virtual bool isEmbeddedObject() const override final;
    virtual bool isFieldset() const override final;
    virtual bool isHeading() const override final;
    virtual bool isHovered() const override final;
    virtual bool isImage() const override final;
    bool isImageButton() const;
    virtual bool isInputImage() const override final;
    virtual bool isLink() const override final;
    virtual bool isMenu() const override final;
    virtual bool isMenuButton() const override final;
    virtual bool isMeter() const override final;
    virtual bool isMultiSelectable() const override;
    bool isNativeImage() const;
    virtual bool isNativeTextControl() const override final;
    virtual bool isNonNativeTextControl() const override final;
    virtual bool isPasswordField() const override final;
    virtual bool isProgressIndicator() const override;
    virtual bool isSlider() const override;
    virtual bool isNativeSlider() const override;

    // Check object state.
    virtual bool isChecked() const override final;
    virtual bool isClickable() const override final;
    virtual bool isEnabled() const override;
    virtual AccessibilityExpanded isExpanded() const override;
    virtual bool isIndeterminate() const override final;
    virtual bool isPressed() const override final;
    virtual bool isReadOnly() const override;
    virtual bool isRequired() const override final;

    // Check whether certain properties can be modified.
    virtual bool canSetFocusAttribute() const override;
    virtual bool canSetValueAttribute() const override;

    // Properties of static elements.
    virtual RGBA32 colorValue() const override final;
    virtual bool canvasHasFallbackContent() const override final;
    virtual bool deprecatedExposesTitleUIElement() const override;
    virtual int headingLevel() const override final;
    virtual unsigned hierarchicalLevel() const override final;
    virtual String text() const override;
    virtual AXObject* deprecatedTitleUIElement() const override;

    // Properties of interactive elements.
    virtual AccessibilityButtonState checkboxOrRadioValue() const override final;
    virtual InvalidState invalidState() const override final;
    // Only used when invalidState() returns InvalidStateOther.
    virtual String ariaInvalidValue() const override final;
    virtual String valueDescription() const override;
    virtual float valueForRange() const override;
    virtual float maxValueForRange() const override;
    virtual float minValueForRange() const override;
    virtual String stringValue() const override;
    virtual const AtomicString& textInputType() const override;

    // ARIA attributes.
    virtual String ariaDescribedByAttribute() const override final;
    virtual const AtomicString& ariaDropEffect() const override final;
    virtual String ariaLabeledByAttribute() const override final;
    virtual AccessibilityRole ariaRoleAttribute() const override final;
    virtual AccessibilityOptionalBool isAriaGrabbed() const override final;

    // Accessibility Text.
    virtual String deprecatedTextUnderElement(TextUnderElementMode) const override;
    virtual String deprecatedAccessibilityDescription() const override;
    virtual String deprecatedTitle(TextUnderElementMode) const override;
    virtual String deprecatedHelpText() const override;
    virtual String computedName() const override;

    // New AX name calculation.
    virtual String textAlternative(bool recursive, bool inAriaLabelledByTraversal, HashSet<AXObject*>& visited, AXNameFrom*, Vector<AXObject*>* nameObjects) override;

    // Location and click point in frame-relative coordinates.
    virtual LayoutRect elementRect() const override;

    // High-level accessibility tree access.
    virtual AXObject* computeParent() const override;
    virtual AXObject* computeParentIfExists() const override;

    // Low-level accessibility tree exploration.
    virtual AXObject* firstChild() const override;
    virtual AXObject* nextSibling() const override;
    virtual void addChildren() override;
    virtual bool canHaveChildren() const override;
    void addChild(AXObject*);
    void insertChild(AXObject*, unsigned index);

    // DOM and Render tree access.
    virtual Element* actionElement() const override final;
    virtual Element* anchorElement() const override;
    virtual Document* document() const override;
    virtual Node* node() const override { return m_node; }

    // Modify or take an action on an object.
    virtual void setFocused(bool) override final;
    virtual void increment() override final;
    virtual void decrement() override final;

    // Notifications that this object may have changed.
    virtual void childrenChanged() override;
    virtual void selectionChanged() override final;
    virtual void textChanged() override;
    virtual void updateAccessibilityRole() override final;

    // Position in set and Size of set
    virtual int posInSet() const override;
    virtual int setSize() const override;

private:
    Node* m_node;

    String alternativeTextForWebArea() const;
    void alternativeText(Vector<AccessibilityText>&) const;
    void ariaLabeledByText(Vector<AccessibilityText>&) const;
    float stepValueForRange() const;
    AXObject* findChildWithTagName(const HTMLQualifiedName&) const;
    bool isDescendantOfElementType(const HTMLQualifiedName& tagName) const;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXNodeObject, isAXNodeObject());

} // namespace blink

#endif // AXNodeObject_h
