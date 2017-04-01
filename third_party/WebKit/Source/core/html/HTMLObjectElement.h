/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLObjectElement_h
#define HTMLObjectElement_h

#include "core/CoreExport.h"
#include "core/html/FormAssociated.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/ListedElement.h"

namespace blink {

class HTMLFormElement;

// Inheritance of ListedElement was used for NPAPI form association, but
// is still kept here so that legacy APIs such as form attribute can keep
// working according to the spec.  See:
// https://html.spec.whatwg.org/multipage/embedded-content.html#the-object-element
class CORE_EXPORT HTMLObjectElement final : public HTMLPlugInElement,
                                            public ListedElement,
                                            public FormAssociated {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLObjectElement);

 public:
  static HTMLObjectElement* create(Document&,
                                   bool createdByParser);
  ~HTMLObjectElement() override;
  DECLARE_VIRTUAL_TRACE();

  const String& classId() const { return m_classId; }

  HTMLFormElement* formOwner() const override;

  bool containsJavaApplet() const;

  bool hasFallbackContent() const override;
  bool useFallbackContent() const override;
  bool canRenderFallbackContent() const override { return true; }
  void renderFallbackContent() override;

  bool isFormControlElement() const override { return false; }

  bool isEnumeratable() const override { return true; }
  bool isInteractiveContent() const override;

  // Implementations of constraint validation API.
  // Note that the object elements are always barred from constraint validation.
  String validationMessage() const override { return String(); }
  bool checkValidity() { return true; }
  bool reportValidity() { return true; }
  void setCustomValidity(const String&) override {}

  bool canContainRangeEndPoint() const override { return useFallbackContent(); }

  bool isExposed() const;

  bool willUseFallbackContentAtLayout() const;

  FormAssociated* toFormAssociatedOrNull() override { return this; };
  void associateWith(HTMLFormElement*) override;

 private:
  HTMLObjectElement(Document&, bool createdByParser);

  void parseAttribute(const AttributeModificationParams&) override;
  bool isPresentationAttribute(const QualifiedName&) const override;
  void collectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void removedFrom(ContainerNode*) override;

  void didMoveToNewDocument(Document& oldDocument) override;

  void childrenChanged(const ChildrenChange&) override;

  bool isURLAttribute(const Attribute&) const override;
  bool hasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& subResourceAttributeName() const override;
  const AtomicString imageSourceURL() const override;

  LayoutPart* existingLayoutPart() const override;

  void updateWidgetInternal() override;
  void updateDocNamedItem();

  void reattachFallbackContent();

  // FIXME: This function should not deal with url or serviceType
  // so that we can better share code between <object> and <embed>.
  void parametersForPlugin(Vector<String>& paramNames,
                           Vector<String>& paramValues,
                           String& url,
                           String& serviceType);

  bool hasValidClassId() const;

  void reloadPluginOnAttributeChange(const QualifiedName&);

  bool shouldRegisterAsNamedItem() const override { return true; }
  bool shouldRegisterAsExtraNamedItem() const override { return true; }

  String m_classId;
  bool m_useFallbackContent : 1;
};

// Intentionally left unimplemented, template specialization needs to be
// provided for specific return types.
template <typename T>
inline const T& toElement(const ListedElement&);
template <typename T>
inline const T* toElement(const ListedElement*);

// Make toHTMLObjectElement() accept a ListedElement as input instead of
// a Node.
template <>
inline const HTMLObjectElement* toElement<HTMLObjectElement>(
    const ListedElement* element) {
  SECURITY_DCHECK(!element || !element->isFormControlElement());
  const HTMLObjectElement* objectElement =
      static_cast<const HTMLObjectElement*>(element);
  // We need to assert after the cast because ListedElement doesn't
  // have hasTagName.
  SECURITY_DCHECK(!objectElement ||
                  objectElement->hasTagName(HTMLNames::objectTag));
  return objectElement;
}

template <>
inline const HTMLObjectElement& toElement<HTMLObjectElement>(
    const ListedElement& element) {
  SECURITY_DCHECK(!element.isFormControlElement());
  const HTMLObjectElement& objectElement =
      static_cast<const HTMLObjectElement&>(element);
  // We need to assert after the cast because ListedElement doesn't
  // have hasTagName.
  SECURITY_DCHECK(objectElement.hasTagName(HTMLNames::objectTag));
  return objectElement;
}

}  // namespace blink

#endif  // HTMLObjectElement_h
