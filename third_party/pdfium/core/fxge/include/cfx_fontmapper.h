// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_INCLUDE_CFX_FONTMAPPER_H_
#define CORE_FXGE_INCLUDE_CFX_FONTMAPPER_H_

#include <memory>
#include <vector>

#include "core/fxge/include/cfx_fontmgr.h"
#include "core/fxge/include/fx_font.h"

class CFX_FontMapper {
 public:
  explicit CFX_FontMapper(CFX_FontMgr* mgr);
  ~CFX_FontMapper();

  void SetSystemFontInfo(std::unique_ptr<IFX_SystemFontInfo> pFontInfo);
  IFX_SystemFontInfo* GetSystemFontInfo() { return m_pFontInfo.get(); }
  void AddInstalledFont(const CFX_ByteString& name, int charset);
  void LoadInstalledFonts();

  FXFT_Face FindSubstFont(const CFX_ByteString& face_name,
                          FX_BOOL bTrueType,
                          uint32_t flags,
                          int weight,
                          int italic_angle,
                          int CharsetCP,
                          CFX_SubstFont* pSubstFont);
#ifdef PDF_ENABLE_XFA
  FXFT_Face FindSubstFontByUnicode(uint32_t dwUnicode,
                                   uint32_t flags,
                                   int weight,
                                   int italic_angle);
#endif  // PDF_ENABLE_XFA
  FX_BOOL IsBuiltinFace(const FXFT_Face face) const;
  int GetFaceSize() const;
  CFX_ByteString GetFaceName(int index) const {
    return m_FaceArray[index].name;
  }

  std::vector<CFX_ByteString> m_InstalledTTFonts;

 private:
  static const size_t MM_FACE_COUNT = 2;
  static const size_t FOXIT_FACE_COUNT = 14;

  CFX_ByteString GetPSNameFromTT(void* hFont);
  CFX_ByteString MatchInstalledFonts(const CFX_ByteString& norm_name);
  FXFT_Face UseInternalSubst(CFX_SubstFont* pSubstFont,
                             int iBaseFont,
                             int italic_angle,
                             int weight,
                             int picthfamily);
  FXFT_Face GetCachedTTCFace(void* hFont,
                             const uint32_t tableTTCF,
                             uint32_t ttc_size,
                             uint32_t font_size);
  FXFT_Face GetCachedFace(void* hFont,
                          CFX_ByteString SubstName,
                          int weight,
                          FX_BOOL bItalic,
                          uint32_t font_size);

  struct FaceData {
    CFX_ByteString name;
    uint32_t charset;
  };

  FX_BOOL m_bListLoaded;
  FXFT_Face m_MMFaces[MM_FACE_COUNT];
  CFX_ByteString m_LastFamily;
  std::vector<FaceData> m_FaceArray;
  std::unique_ptr<IFX_SystemFontInfo> m_pFontInfo;
  FXFT_Face m_FoxitFaces[FOXIT_FACE_COUNT];
  CFX_FontMgr* const m_pFontMgr;
};

#endif  // CORE_FXGE_INCLUDE_CFX_FONTMAPPER_H_
