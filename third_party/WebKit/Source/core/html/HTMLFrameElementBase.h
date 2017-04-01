/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLFrameElementBase_h
#define HTMLFrameElementBase_h

#include "core/CoreExport.h"
#include "core/html/HTMLFrameOwnerElement.h"

namespace blink {

class CORE_EXPORT HTMLFrameElementBase : public HTMLFrameOwnerElement {
 public:
  bool canContainRangeEndPoint() const final { return false; }

  // FrameOwner overrides:
  ScrollbarMode scrollingMode() const final { return m_scrollingMode; }
  int marginWidth() const final { return m_marginWidth; }
  int marginHeight() const final { return m_marginHeight; }

 protected:
  HTMLFrameElementBase(const QualifiedName&, Document&);

  void parseAttribute(const AttributeModificationParams&) override;
  InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void didNotifySubtreeInsertionsToDocument() final;
  void attachLayoutTree(const AttachContext& = AttachContext()) override;

  // FIXME: Remove this method once we have input routing in the browser
  // process. See http://crbug.com/339659.
  void defaultEventHandler(Event*) override;

  void setScrollingMode(ScrollbarMode);
  void setMarginWidth(int);
  void setMarginHeight(int);

  void frameOwnerPropertiesChanged();

 private:
  bool supportsFocus() const final;
  void setFocused(bool) final;

  bool isURLAttribute(const Attribute&) const final;
  bool hasLegalLinkAttribute(const QualifiedName&) const final;
  bool isHTMLContentAttribute(const Attribute&) const final;

  bool areAuthorShadowsAllowed() const final { return false; }

  void setLocation(const String&);
  void setNameAndOpenURL();
  bool isURLAllowed() const;
  void openURL(bool replaceCurrentItem = true);

  ScrollbarMode m_scrollingMode;
  int m_marginWidth;
  int m_marginHeight;

  AtomicString m_URL;
  AtomicString m_frameName;
};

inline bool isHTMLFrameElementBase(const HTMLElement& element) {
  return isHTMLFrameElement(element) || isHTMLIFrameElement(element);
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLFrameElementBase);

}  // namespace blink

#endif  // HTMLFrameElementBase_h
