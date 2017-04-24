/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2011 Apple Inc. All rights reserved.
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

#ifndef TextRun_h
#define TextRun_h

#include "platform/PlatformExport.h"
#include "platform/fonts/Glyph.h"
#include "platform/geometry/FloatRect.h"
#include "platform/heap/Heap.h"
#include "platform/text/TabSize.h"
#include "platform/text/TextDirection.h"
#include "wtf/Allocator.h"
#include "wtf/text/StringView.h"
#include "wtf/text/WTFString.h"

#include <SkRefCnt.h>
#include <unicode/utf16.h>

namespace blink {

enum TextJustify {
  TextJustifyAuto = 0x0,
  TextJustifyNone = 0x1,
  TextJustifyInterWord = 0x2,
  TextJustifyDistribute = 0x3
};

class PLATFORM_EXPORT TextRun final {
  DISALLOW_NEW();

 public:
  enum ExpansionBehaviorFlags {
    ForbidTrailingExpansion = 0 << 0,
    AllowTrailingExpansion = 1 << 0,
    ForbidLeadingExpansion = 0 << 1,
    AllowLeadingExpansion = 1 << 1,
  };

  enum TextCodePath { Auto = 0, ForceSimple = 1, ForceComplex = 2 };

  typedef unsigned ExpansionBehavior;

  TextRun(const LChar* c,
          unsigned len,
          float xpos = 0,
          float expansion = 0,
          ExpansionBehavior expansionBehavior = AllowTrailingExpansion |
                                                ForbidLeadingExpansion,
          TextDirection direction = TextDirection::kLtr,
          bool directionalOverride = false)
      : m_charactersLength(len),
        m_len(len),
        m_xpos(xpos),
        m_horizontalGlyphStretch(1),
        m_expansion(expansion),
        m_expansionBehavior(expansionBehavior),
        m_is8Bit(true),
        m_allowTabs(false),
        m_direction(static_cast<unsigned>(direction)),
        m_directionalOverride(directionalOverride),
        m_disableSpacing(false),
        m_textJustify(TextJustifyAuto),
        m_normalizeSpace(false),
        m_tabSize(0) {
    m_data.characters8 = c;
  }

  TextRun(const UChar* c,
          unsigned len,
          float xpos = 0,
          float expansion = 0,
          ExpansionBehavior expansionBehavior = AllowTrailingExpansion |
                                                ForbidLeadingExpansion,
          TextDirection direction = TextDirection::kLtr,
          bool directionalOverride = false)
      : m_charactersLength(len),
        m_len(len),
        m_xpos(xpos),
        m_horizontalGlyphStretch(1),
        m_expansion(expansion),
        m_expansionBehavior(expansionBehavior),
        m_is8Bit(false),
        m_allowTabs(false),
        m_direction(static_cast<unsigned>(direction)),
        m_directionalOverride(directionalOverride),
        m_disableSpacing(false),
        m_textJustify(TextJustifyAuto),
        m_normalizeSpace(false),
        m_tabSize(0) {
    m_data.characters16 = c;
  }

  TextRun(const StringView& string,
          float xpos = 0,
          float expansion = 0,
          ExpansionBehavior expansionBehavior = AllowTrailingExpansion |
                                                ForbidLeadingExpansion,
          TextDirection direction = TextDirection::kLtr,
          bool directionalOverride = false)
      : m_charactersLength(string.length()),
        m_len(string.length()),
        m_xpos(xpos),
        m_horizontalGlyphStretch(1),
        m_expansion(expansion),
        m_expansionBehavior(expansionBehavior),
        m_allowTabs(false),
        m_direction(static_cast<unsigned>(direction)),
        m_directionalOverride(directionalOverride),
        m_disableSpacing(false),
        m_textJustify(TextJustifyAuto),
        m_normalizeSpace(false),
        m_tabSize(0) {
    if (!m_charactersLength) {
      m_is8Bit = true;
      m_data.characters8 = 0;
    } else if (string.is8Bit()) {
      m_data.characters8 = string.characters8();
      m_is8Bit = true;
    } else {
      m_data.characters16 = string.characters16();
      m_is8Bit = false;
    }
  }

  TextRun subRun(unsigned startOffset, unsigned length) const {
    ASSERT(startOffset < m_len);

    TextRun result = *this;

    if (is8Bit()) {
      result.setText(data8(startOffset), length);
      return result;
    }
    result.setText(data16(startOffset), length);
    return result;
  }

  UChar operator[](unsigned i) const {
    SECURITY_DCHECK(i < m_len);
    return is8Bit() ? m_data.characters8[i] : m_data.characters16[i];
  }
  const LChar* data8(unsigned i) const {
    SECURITY_DCHECK(i < m_len);
    ASSERT(is8Bit());
    return &m_data.characters8[i];
  }
  const UChar* data16(unsigned i) const {
    SECURITY_DCHECK(i < m_len);
    ASSERT(!is8Bit());
    return &m_data.characters16[i];
  }

  const LChar* characters8() const {
    ASSERT(is8Bit());
    return m_data.characters8;
  }
  const UChar* characters16() const {
    ASSERT(!is8Bit());
    return m_data.characters16;
  }

  UChar32 codepointAt(unsigned i) const {
    SECURITY_DCHECK(i < m_len);
    if (is8Bit())
      return (*this)[i];
    UChar32 codepoint;
    U16_GET(characters16(), 0, i, m_len, codepoint);
    return codepoint;
  }

  UChar32 codepointAtAndNext(unsigned& i) const {
    SECURITY_DCHECK(i < m_len);
    if (is8Bit())
      return (*this)[i++];
    UChar32 codepoint;
    U16_NEXT(characters16(), i, m_len, codepoint);
    return codepoint;
  }

  bool is8Bit() const { return m_is8Bit; }
  unsigned length() const { return m_len; }
  unsigned charactersLength() const { return m_charactersLength; }

  bool normalizeSpace() const { return m_normalizeSpace; }
  void setNormalizeSpace(bool normalizeSpace) {
    m_normalizeSpace = normalizeSpace;
  }

  void setText(const LChar* c, unsigned len) {
    m_data.characters8 = c;
    m_len = len;
    m_is8Bit = true;
  }
  void setText(const UChar* c, unsigned len) {
    m_data.characters16 = c;
    m_len = len;
    m_is8Bit = false;
  }
  void setText(const String&);
  void setCharactersLength(unsigned charactersLength) {
    m_charactersLength = charactersLength;
  }

  void setExpansionBehavior(ExpansionBehavior behavior) {
    m_expansionBehavior = behavior;
  }
  float horizontalGlyphStretch() const { return m_horizontalGlyphStretch; }
  void setHorizontalGlyphStretch(float scale) {
    m_horizontalGlyphStretch = scale;
  }

  bool allowTabs() const { return m_allowTabs; }
  TabSize getTabSize() const { return m_tabSize; }
  void setTabSize(bool, TabSize);

  float xPos() const { return m_xpos; }
  void setXPos(float xPos) { m_xpos = xPos; }
  float expansion() const { return m_expansion; }
  void setExpansion(float expansion) { m_expansion = expansion; }
  bool allowsLeadingExpansion() const {
    return m_expansionBehavior & AllowLeadingExpansion;
  }
  bool allowsTrailingExpansion() const {
    return m_expansionBehavior & AllowTrailingExpansion;
  }
  TextDirection direction() const {
    return static_cast<TextDirection>(m_direction);
  }
  bool rtl() const { return direction() == TextDirection::kRtl; }
  bool ltr() const { return direction() == TextDirection::kLtr; }
  bool directionalOverride() const { return m_directionalOverride; }
  bool spacingDisabled() const { return m_disableSpacing; }

  void disableSpacing() { m_disableSpacing = true; }
  void setDirection(TextDirection direction) {
    m_direction = static_cast<unsigned>(direction);
  }
  void setDirectionalOverride(bool override) {
    m_directionalOverride = override;
  }

  void setTextJustify(TextJustify textJustify) {
    m_textJustify = static_cast<unsigned>(textJustify);
  }
  TextJustify getTextJustify() const {
    return static_cast<TextJustify>(m_textJustify);
  }

  // Up-converts to UTF-16 as needed and normalizes spaces and Unicode control
  // characters as per the CSS Text Module Level 3 specification.
  // https://drafts.csswg.org/css-text-3/#white-space-processing
  std::unique_ptr<UChar[]> normalizedUTF16(unsigned* resultLength) const;

 private:
  union {
    const LChar* characters8;
    const UChar* characters16;
  } m_data;
  // Marks the end of the characters buffer.  Default equals to m_len.
  unsigned m_charactersLength;
  unsigned m_len;

  // m_xpos is the x position relative to the left start of the text line, not
  // relative to the left start of the containing block. In the case of right
  // alignment or center alignment, left start of the text line is not the same
  // as left start of the containing block.
  float m_xpos;
  float m_horizontalGlyphStretch;

  float m_expansion;
  ExpansionBehavior m_expansionBehavior : 2;
  unsigned m_is8Bit : 1;
  unsigned m_allowTabs : 1;
  unsigned m_direction : 1;
  // Was this direction set by an override character.
  unsigned m_directionalOverride : 1;
  unsigned m_disableSpacing : 1;
  unsigned m_textJustify : 2;
  unsigned m_normalizeSpace : 1;
  TabSize m_tabSize;
};

inline void TextRun::setTabSize(bool allow, TabSize size) {
  m_allowTabs = allow;
  m_tabSize = size;
}

// Container for parameters needed to paint TextRun.
struct TextRunPaintInfo {
  STACK_ALLOCATED();

 public:
  explicit TextRunPaintInfo(const TextRun& r)
      : run(r), from(0), to(r.length()) {}

  const TextRun& run;
  unsigned from;
  unsigned to;
  FloatRect bounds;
};

}  // namespace blink
#endif
