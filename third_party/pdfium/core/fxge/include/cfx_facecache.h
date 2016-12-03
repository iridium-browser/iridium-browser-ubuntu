// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_INCLUDE_CFX_FACECACHE_H_
#define CORE_FXGE_INCLUDE_CFX_FACECACHE_H_

#include <map>

#include "core/fxge/include/fx_font.h"
#include "core/fxge/include/fx_freetype.h"

class CFX_FaceCache {
 public:
  explicit CFX_FaceCache(FXFT_Face face);
  ~CFX_FaceCache();
  const CFX_GlyphBitmap* LoadGlyphBitmap(CFX_Font* pFont,
                                         uint32_t glyph_index,
                                         FX_BOOL bFontStyle,
                                         const CFX_Matrix* pMatrix,
                                         int dest_width,
                                         int anti_alias,
                                         int& text_flags);
  const CFX_PathData* LoadGlyphPath(CFX_Font* pFont,
                                    uint32_t glyph_index,
                                    int dest_width);

#ifdef _SKIA_SUPPORT_
  CFX_TypeFace* GetDeviceCache(CFX_Font* pFont);
#endif

 private:
  CFX_GlyphBitmap* RenderGlyph(CFX_Font* pFont,
                               uint32_t glyph_index,
                               FX_BOOL bFontStyle,
                               const CFX_Matrix* pMatrix,
                               int dest_width,
                               int anti_alias);
  CFX_GlyphBitmap* RenderGlyph_Nativetext(CFX_Font* pFont,
                                          uint32_t glyph_index,
                                          const CFX_Matrix* pMatrix,
                                          int dest_width,
                                          int anti_alias);
  CFX_GlyphBitmap* LookUpGlyphBitmap(CFX_Font* pFont,
                                     const CFX_Matrix* pMatrix,
                                     const CFX_ByteString& FaceGlyphsKey,
                                     uint32_t glyph_index,
                                     FX_BOOL bFontStyle,
                                     int dest_width,
                                     int anti_alias);
  void InitPlatform();
  void DestroyPlatform();

  FXFT_Face const m_Face;
  std::map<CFX_ByteString, std::unique_ptr<CFX_SizeGlyphCache>> m_SizeMap;
  std::map<uint32_t, std::unique_ptr<CFX_PathData>> m_PathMap;
#ifdef _SKIA_SUPPORT_
  CFX_TypeFace* m_pTypeface;
#endif
};

#endif  //  CORE_FXGE_INCLUDE_CFX_FACECACHE_H_
