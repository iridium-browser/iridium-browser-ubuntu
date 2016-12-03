// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_INCLUDE_CFX_FONTCACHE_H_
#define CORE_FXGE_INCLUDE_CFX_FONTCACHE_H_

#include <map>

#include "core/fxcrt/include/fx_system.h"
#include "core/fxge/include/fx_font.h"
#include "core/fxge/include/fx_freetype.h"

class CFX_FaceCache;

class CFX_FontCache {
 public:
  CFX_FontCache();
  ~CFX_FontCache();
  CFX_FaceCache* GetCachedFace(CFX_Font* pFont);
  void ReleaseCachedFace(CFX_Font* pFont);
  void FreeCache(FX_BOOL bRelease = FALSE);
#ifdef _SKIA_SUPPORT_
  CFX_TypeFace* GetDeviceCache(CFX_Font* pFont);
#endif

 private:
  using CFX_FTCacheMap = std::map<FXFT_Face, CFX_CountedFaceCache*>;
  CFX_FTCacheMap m_FTFaceMap;
  CFX_FTCacheMap m_ExtFaceMap;
};

#endif  // CORE_FXGE_INCLUDE_CFX_FONTCACHE_H_
