/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLOptGroupElement_h
#define HTMLOptGroupElement_h

#include "core/CoreExport.h"
#include "core/html/HTMLElement.h"

namespace blink {

class ComputedStyle;
class HTMLSelectElement;
class HTMLDivElement;

class CORE_EXPORT HTMLOptGroupElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static HTMLOptGroupElement* create(Document&);

  bool isDisabledFormControl() const override;
  String defaultToolTip() const override;
  HTMLSelectElement* ownerSelectElement() const;

  String groupLabelText() const;
  HTMLDivElement& optGroupLabelElement() const;

 private:
  explicit HTMLOptGroupElement(Document&);
  ~HTMLOptGroupElement();

  bool supportsFocus() const override;
  void parseAttribute(const AttributeModificationParams&) override;
  void accessKeyAction(bool sendMouseEvents) override;
  void didAddUserAgentShadowRoot(ShadowRoot&) override;
  void attachLayoutTree(const AttachContext& = AttachContext()) override;
  void detachLayoutTree(const AttachContext& = AttachContext()) override;
  bool matchesEnabledPseudoClass() const override;
  InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void removedFrom(ContainerNode*) override;

  // <optgroup> might not have a layoutObject so we manually manage a cached
  // style.
  void updateNonComputedStyle();
  ComputedStyle* nonLayoutObjectComputedStyle() const override;
  PassRefPtr<ComputedStyle> customStyleForLayoutObject() override;

  void updateGroupLabel();

  RefPtr<ComputedStyle> m_style;
};

}  // namespace blink

#endif  // HTMLOptGroupElement_h
