/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 */

#ifndef CSSProperty_h
#define CSSProperty_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSValue.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/text/TextDirection.h"
#include "platform/text/WritingMode.h"
#include "wtf/Allocator.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

struct StylePropertyMetadata {
  DISALLOW_NEW();
  StylePropertyMetadata(CSSPropertyID propertyID,
                        bool isSetFromShorthand,
                        int indexInShorthandsVector,
                        bool important,
                        bool implicit,
                        bool inherited)
      : m_propertyID(propertyID),
        m_isSetFromShorthand(isSetFromShorthand),
        m_indexInShorthandsVector(indexInShorthandsVector),
        m_important(important),
        m_implicit(implicit),
        m_inherited(inherited) {}

  CSSPropertyID shorthandID() const;

  unsigned m_propertyID : 10;
  unsigned m_isSetFromShorthand : 1;
  // If this property was set as part of an ambiguous shorthand, gives the index
  // in the shorthands vector.
  unsigned m_indexInShorthandsVector : 2;
  unsigned m_important : 1;
  // Whether or not the property was set implicitly as the result of a
  // shorthand.
  unsigned m_implicit : 1;
  unsigned m_inherited : 1;
};

class CSSProperty {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  CSSProperty(CSSPropertyID propertyID,
              const CSSValue& value,
              bool important = false,
              bool isSetFromShorthand = false,
              int indexInShorthandsVector = 0,
              bool implicit = false)
      : m_metadata(propertyID,
                   isSetFromShorthand,
                   indexInShorthandsVector,
                   important,
                   implicit,
                   CSSPropertyMetadata::isInheritedProperty(propertyID)),
        m_value(value) {}

  // FIXME: Remove this.
  CSSProperty(StylePropertyMetadata metadata, const CSSValue& value)
      : m_metadata(metadata), m_value(value) {}

  CSSPropertyID id() const {
    return static_cast<CSSPropertyID>(m_metadata.m_propertyID);
  }
  bool isSetFromShorthand() const { return m_metadata.m_isSetFromShorthand; }
  CSSPropertyID shorthandID() const { return m_metadata.shorthandID(); }
  bool isImportant() const { return m_metadata.m_important; }

  const CSSValue* value() const { return m_value.get(); }

  static CSSPropertyID resolveDirectionAwareProperty(CSSPropertyID,
                                                     TextDirection,
                                                     WritingMode);
  static bool isAffectedByAllProperty(CSSPropertyID);

  const StylePropertyMetadata& metadata() const { return m_metadata; }

  bool operator==(const CSSProperty& other) const;

  DEFINE_INLINE_TRACE() { visitor->trace(m_value); }

 private:
  StylePropertyMetadata m_metadata;
  Member<const CSSValue> m_value;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::CSSProperty);

#endif  // CSSProperty_h
