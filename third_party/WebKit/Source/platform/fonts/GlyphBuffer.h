/*
 * Copyright (C) 2006, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile Inc.
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

#ifndef GlyphBuffer_h
#define GlyphBuffer_h

#include "platform/fonts/Glyph.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/heap/Heap.h"
#include "wtf/Vector.h"

namespace blink {

class SimpleFontData;

class GlyphBuffer {
  STACK_ALLOCATED();

 public:
  enum class Type { Normal, TextIntercepts };
  explicit GlyphBuffer(Type type = Type::Normal) : m_type(type) {}

  Type type() const { return m_type; }

  bool isEmpty() const { return m_fontData.isEmpty(); }
  unsigned size() const {
    ASSERT(m_fontData.size() == m_glyphs.size());
    ASSERT(m_fontData.size() == m_offsets.size() ||
           2 * m_fontData.size() == m_offsets.size());
    return m_fontData.size();
  }

  bool hasVerticalOffsets() const {
    // We exclusively store either horizontal/x-only ofssets -- in which case
    // m_offsets.size == size, or vertical/xy offsets -- in which case
    // m_offsets.size == size * 2.
    return size() != m_offsets.size();
  }

  const Glyph* glyphs(unsigned from) const {
    ASSERT(from < size());
    return m_glyphs.data() + from;
  }

  // Depending on the GlyphBuffer-wide positioning mode, this either points to
  // an array of x-only offsets for horizontal positioning ([x1, x2, ... xn]),
  // or interleaved x,y offsets for full positioning ([x1, y1, ... xn, yn]).
  const float* offsets(unsigned from) const {
    ASSERT(from < size());
    return m_offsets.data() + (hasVerticalOffsets() ? from * 2 : from);
  }

  const SimpleFontData* fontDataAt(unsigned index) const {
    ASSERT(index < size());
    return m_fontData[index];
  }

  Glyph glyphAt(unsigned index) const {
    ASSERT(index < size());
    return m_glyphs[index];
  }

  float xOffsetAt(unsigned index) const {
    ASSERT(index < size());
    return hasVerticalOffsets() ? m_offsets[index * 2] : m_offsets[index];
  }

  float yOffsetAt(unsigned index) const {
    ASSERT(index < size());
    ASSERT(hasVerticalOffsets());
    return m_offsets[index * 2 + 1];
  }

  void add(Glyph glyph, const SimpleFontData* font, float x) {
    // cannot mix x-only/xy offsets
    ASSERT(!hasVerticalOffsets());

    m_fontData.push_back(font);
    m_glyphs.push_back(glyph);
    m_offsets.push_back(x);
  }

  void add(Glyph glyph, const SimpleFontData* font, const FloatPoint& offset) {
    // cannot mix x-only/xy offsets
    ASSERT(isEmpty() || hasVerticalOffsets());

    m_fontData.push_back(font);
    m_glyphs.push_back(glyph);
    m_offsets.push_back(offset.x());
    m_offsets.push_back(offset.y());
  }

 protected:
  Vector<const SimpleFontData*, 2048> m_fontData;
  Vector<Glyph, 2048> m_glyphs;

  // Glyph positioning: either x-only offsets, or interleaved x,y offsets
  // (depending on the buffer-wide positioning mode). This matches the
  // glyph positioning format used by Skia.
  Vector<float, 2048> m_offsets;

  Type m_type;
};

}  // namespace blink

#endif
