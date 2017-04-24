/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSFontSelector_h
#define CSSFontSelector_h

#include "core/CoreExport.h"
#include "core/css/FontFaceCache.h"
#include "platform/fonts/FontSelector.h"
#include "platform/fonts/GenericFontFamilySettings.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"

namespace blink {

class CSSFontSelectorClient;
class Document;
class FontDescription;

class CORE_EXPORT CSSFontSelector : public FontSelector {
 public:
  static CSSFontSelector* create(Document* document) {
    return new CSSFontSelector(document);
  }
  ~CSSFontSelector() override;

  unsigned version() const override { return m_fontFaceCache.version(); }

  PassRefPtr<FontData> getFontData(const FontDescription&,
                                   const AtomicString&) override;
  void willUseFontData(const FontDescription&,
                       const AtomicString& family,
                       const String& text) override;
  void willUseRange(const FontDescription&,
                    const AtomicString& familyName,
                    const FontDataForRangeSet&) override;
  bool isPlatformFamilyMatchAvailable(const FontDescription&,
                                      const AtomicString& family);

  void fontFaceInvalidated();

  // FontCacheClient implementation
  void fontCacheInvalidated() override;

  void registerForInvalidationCallbacks(CSSFontSelectorClient*);
  void unregisterForInvalidationCallbacks(CSSFontSelectorClient*);

  Document* document() const { return m_document; }
  FontFaceCache* fontFaceCache() { return &m_fontFaceCache; }

  const GenericFontFamilySettings& genericFontFamilySettings() const {
    return m_genericFontFamilySettings;
  }
  void updateGenericFontFamilySettings(Document&);

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit CSSFontSelector(Document*);

  void dispatchInvalidationCallbacks();

 private:
  // TODO(Oilpan): Ideally this should just be a traced Member but that will
  // currently leak because ComputedStyle and its data are not on the heap.
  // See crbug.com/383860 for details.
  WeakMember<Document> m_document;
  // FIXME: Move to Document or StyleEngine.
  FontFaceCache m_fontFaceCache;
  HeapHashSet<WeakMember<CSSFontSelectorClient>> m_clients;
  GenericFontFamilySettings m_genericFontFamilySettings;
};

}  // namespace blink

#endif  // CSSFontSelector_h
