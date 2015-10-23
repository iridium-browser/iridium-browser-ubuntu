// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <limits>

#include "../../../include/fxge/fx_ge.h"
#include "../../../include/fxge/fx_freetype.h"
#include "text_int.h"

#define GET_TT_SHORT(w) (FX_WORD)(((w)[0] << 8) | (w)[1])
#define GET_TT_LONG(w) \
  (FX_DWORD)(((w)[0] << 24) | ((w)[1] << 16) | ((w)[2] << 8) | (w)[3])

namespace {

CFX_ByteString KeyNameFromFace(const CFX_ByteString& face_name,
                               int weight,
                               FX_BOOL bItalic) {
  CFX_ByteString key(face_name);
  key += ',';
  key += CFX_ByteString::FormatInteger(weight);
  key += bItalic ? 'I' : 'N';
  return key;
}

CFX_ByteString KeyNameFromSize(int ttc_size, FX_DWORD checksum) {
  CFX_ByteString key;
  key.Format("%d:%d", ttc_size, checksum);
  return key;
}

}  // namespace

CFX_SubstFont::CFX_SubstFont() {
  m_ExtHandle = NULL;
  m_Charset = 0;
  m_SubstFlags = 0;
  m_Weight = 0;
  m_ItalicAngle = 0;
  m_bSubstOfCJK = FALSE;
  m_WeightCJK = 0;
  m_bItlicCJK = FALSE;
}
CTTFontDesc::~CTTFontDesc() {
  if (m_Type == 1) {
    if (m_SingleFace.m_pFace) {
      FXFT_Done_Face(m_SingleFace.m_pFace);
    }
  } else if (m_Type == 2) {
    for (int i = 0; i < 16; i++)
      if (m_TTCFace.m_pFaces[i]) {
        FXFT_Done_Face(m_TTCFace.m_pFaces[i]);
      }
  }
  FX_Free(m_pFontData);
}
FX_BOOL CTTFontDesc::ReleaseFace(FXFT_Face face) {
  if (m_Type == 1) {
    if (m_SingleFace.m_pFace != face) {
      return FALSE;
    }
  } else if (m_Type == 2) {
    int i;
    for (i = 0; i < 16; i++)
      if (m_TTCFace.m_pFaces[i] == face) {
        break;
      }
    if (i == 16) {
      return FALSE;
    }
  }
  m_RefCount--;
  if (m_RefCount) {
    return FALSE;
  }
  delete this;
  return TRUE;
}
CFX_FontMgr::CFX_FontMgr() : m_FTLibrary(nullptr) {
  m_pBuiltinMapper = new CFX_FontMapper(this);
  FXSYS_memset(m_ExternalFonts, 0, sizeof m_ExternalFonts);
}
CFX_FontMgr::~CFX_FontMgr() {
  delete m_pBuiltinMapper;
  FreeCache();
  if (m_FTLibrary) {
    FXFT_Done_FreeType(m_FTLibrary);
  }
}
void CFX_FontMgr::InitFTLibrary() {
  if (m_FTLibrary == NULL) {
    FXFT_Init_FreeType(&m_FTLibrary);
  }
}
void CFX_FontMgr::FreeCache() {
  for (const auto& pair : m_FaceMap) {
    delete pair.second;
  }
  m_FaceMap.clear();
}
void CFX_FontMgr::SetSystemFontInfo(IFX_SystemFontInfo* pFontInfo) {
  m_pBuiltinMapper->SetSystemFontInfo(pFontInfo);
}
FXFT_Face CFX_FontMgr::FindSubstFont(const CFX_ByteString& face_name,
                                     FX_BOOL bTrueType,
                                     FX_DWORD flags,
                                     int weight,
                                     int italic_angle,
                                     int CharsetCP,
                                     CFX_SubstFont* pSubstFont) {
  if (!m_FTLibrary) {
    FXFT_Init_FreeType(&m_FTLibrary);
  }
  return m_pBuiltinMapper->FindSubstFont(face_name, bTrueType, flags, weight,
                                         italic_angle, CharsetCP, pSubstFont);
}
FXFT_Face CFX_FontMgr::GetCachedFace(const CFX_ByteString& face_name,
                                     int weight,
                                     FX_BOOL bItalic,
                                     uint8_t*& pFontData) {
  auto it = m_FaceMap.find(KeyNameFromFace(face_name, weight, bItalic));
  if (it == m_FaceMap.end())
    return nullptr;

  CTTFontDesc* pFontDesc = it->second;
  pFontData = pFontDesc->m_pFontData;
  pFontDesc->m_RefCount++;
  return pFontDesc->m_SingleFace.m_pFace;
}
FXFT_Face CFX_FontMgr::AddCachedFace(const CFX_ByteString& face_name,
                                     int weight,
                                     FX_BOOL bItalic,
                                     uint8_t* pData,
                                     FX_DWORD size,
                                     int face_index) {
  CTTFontDesc* pFontDesc = new CTTFontDesc;
  pFontDesc->m_Type = 1;
  pFontDesc->m_SingleFace.m_pFace = NULL;
  pFontDesc->m_SingleFace.m_bBold = weight;
  pFontDesc->m_SingleFace.m_bItalic = bItalic;
  pFontDesc->m_pFontData = pData;
  pFontDesc->m_RefCount = 1;
  FXFT_Library library;
  if (m_FTLibrary == NULL) {
    FXFT_Init_FreeType(&m_FTLibrary);
  }
  library = m_FTLibrary;
  int ret = FXFT_New_Memory_Face(library, pData, size, face_index,
                                 &pFontDesc->m_SingleFace.m_pFace);
  if (ret) {
    delete pFontDesc;
    return NULL;
  }
  ret = FXFT_Set_Pixel_Sizes(pFontDesc->m_SingleFace.m_pFace, 64, 64);
  if (ret) {
    delete pFontDesc;
    return NULL;
  }
  m_FaceMap[KeyNameFromFace(face_name, weight, bItalic)] = pFontDesc;
  return pFontDesc->m_SingleFace.m_pFace;
}
const FX_CHAR* const g_Base14FontNames[14] = {
    "Courier",
    "Courier-Bold",
    "Courier-BoldOblique",
    "Courier-Oblique",
    "Helvetica",
    "Helvetica-Bold",
    "Helvetica-BoldOblique",
    "Helvetica-Oblique",
    "Times-Roman",
    "Times-Bold",
    "Times-BoldItalic",
    "Times-Italic",
    "Symbol",
    "ZapfDingbats",
};
const struct _AltFontName {
  const FX_CHAR* m_pName;
  int m_Index;
} g_AltFontNames[] = {
    {"Arial", 4},
    {"Arial,Bold", 5},
    {"Arial,BoldItalic", 6},
    {"Arial,Italic", 7},
    {"Arial-Bold", 5},
    {"Arial-BoldItalic", 6},
    {"Arial-BoldItalicMT", 6},
    {"Arial-BoldMT", 5},
    {"Arial-Italic", 7},
    {"Arial-ItalicMT", 7},
    {"ArialBold", 5},
    {"ArialBoldItalic", 6},
    {"ArialItalic", 7},
    {"ArialMT", 4},
    {"ArialMT,Bold", 5},
    {"ArialMT,BoldItalic", 6},
    {"ArialMT,Italic", 7},
    {"ArialRoundedMTBold", 5},
    {"Courier", 0},
    {"Courier,Bold", 1},
    {"Courier,BoldItalic", 2},
    {"Courier,Italic", 3},
    {"Courier-Bold", 1},
    {"Courier-BoldOblique", 2},
    {"Courier-Oblique", 3},
    {"CourierBold", 1},
    {"CourierBoldItalic", 2},
    {"CourierItalic", 3},
    {"CourierNew", 0},
    {"CourierNew,Bold", 1},
    {"CourierNew,BoldItalic", 2},
    {"CourierNew,Italic", 3},
    {"CourierNew-Bold", 1},
    {"CourierNew-BoldItalic", 2},
    {"CourierNew-Italic", 3},
    {"CourierNewBold", 1},
    {"CourierNewBoldItalic", 2},
    {"CourierNewItalic", 3},
    {"CourierNewPS-BoldItalicMT", 2},
    {"CourierNewPS-BoldMT", 1},
    {"CourierNewPS-ItalicMT", 3},
    {"CourierNewPSMT", 0},
    {"CourierStd", 0},
    {"CourierStd-Bold", 1},
    {"CourierStd-BoldOblique", 2},
    {"CourierStd-Oblique", 3},
    {"Helvetica", 4},
    {"Helvetica,Bold", 5},
    {"Helvetica,BoldItalic", 6},
    {"Helvetica,Italic", 7},
    {"Helvetica-Bold", 5},
    {"Helvetica-BoldItalic", 6},
    {"Helvetica-BoldOblique", 6},
    {"Helvetica-Italic", 7},
    {"Helvetica-Oblique", 7},
    {"HelveticaBold", 5},
    {"HelveticaBoldItalic", 6},
    {"HelveticaItalic", 7},
    {"Symbol", 12},
    {"SymbolMT", 12},
    {"Times-Bold", 9},
    {"Times-BoldItalic", 10},
    {"Times-Italic", 11},
    {"Times-Roman", 8},
    {"TimesBold", 9},
    {"TimesBoldItalic", 10},
    {"TimesItalic", 11},
    {"TimesNewRoman", 8},
    {"TimesNewRoman,Bold", 9},
    {"TimesNewRoman,BoldItalic", 10},
    {"TimesNewRoman,Italic", 11},
    {"TimesNewRoman-Bold", 9},
    {"TimesNewRoman-BoldItalic", 10},
    {"TimesNewRoman-Italic", 11},
    {"TimesNewRomanBold", 9},
    {"TimesNewRomanBoldItalic", 10},
    {"TimesNewRomanItalic", 11},
    {"TimesNewRomanPS", 8},
    {"TimesNewRomanPS-Bold", 9},
    {"TimesNewRomanPS-BoldItalic", 10},
    {"TimesNewRomanPS-BoldItalicMT", 10},
    {"TimesNewRomanPS-BoldMT", 9},
    {"TimesNewRomanPS-Italic", 11},
    {"TimesNewRomanPS-ItalicMT", 11},
    {"TimesNewRomanPSMT", 8},
    {"TimesNewRomanPSMT,Bold", 9},
    {"TimesNewRomanPSMT,BoldItalic", 10},
    {"TimesNewRomanPSMT,Italic", 11},
    {"ZapfDingbats", 13},
};
extern "C" {
static int compareString(const void* key, const void* element) {
  return FXSYS_stricmp((const FX_CHAR*)key, ((_AltFontName*)element)->m_pName);
}
}
int _PDF_GetStandardFontName(CFX_ByteString& name) {
  _AltFontName* found =
      (_AltFontName*)FXSYS_bsearch(name.c_str(), g_AltFontNames,
                                   sizeof g_AltFontNames / sizeof(_AltFontName),
                                   sizeof(_AltFontName), compareString);
  if (found == NULL) {
    return -1;
  }
  name = g_Base14FontNames[found->m_Index];
  return found->m_Index;
}
int GetTTCIndex(const uint8_t* pFontData,
                FX_DWORD ttc_size,
                FX_DWORD font_offset) {
  int face_index = 0;
  const uint8_t* p = pFontData + 8;
  FX_DWORD nfont = GET_TT_LONG(p);
  FX_DWORD index;
  for (index = 0; index < nfont; index++) {
    p = pFontData + 12 + index * 4;
    if (GET_TT_LONG(p) == font_offset) {
      break;
    }
  }
  if (index >= nfont) {
    face_index = 0;
  } else {
    face_index = index;
  }
  return face_index;
}
FXFT_Face CFX_FontMgr::GetCachedTTCFace(int ttc_size,
                                        FX_DWORD checksum,
                                        int font_offset,
                                        uint8_t*& pFontData) {
  auto it = m_FaceMap.find(KeyNameFromSize(ttc_size, checksum));
  if (it == m_FaceMap.end())
    return nullptr;

  CTTFontDesc* pFontDesc = it->second;
  pFontData = pFontDesc->m_pFontData;
  pFontDesc->m_RefCount++;
  int face_index = GetTTCIndex(pFontDesc->m_pFontData, ttc_size, font_offset);
  if (!pFontDesc->m_TTCFace.m_pFaces[face_index]) {
    pFontDesc->m_TTCFace.m_pFaces[face_index] =
        GetFixedFace(pFontDesc->m_pFontData, ttc_size, face_index);
  }
  return pFontDesc->m_TTCFace.m_pFaces[face_index];
}
FXFT_Face CFX_FontMgr::AddCachedTTCFace(int ttc_size,
                                        FX_DWORD checksum,
                                        uint8_t* pData,
                                        FX_DWORD size,
                                        int font_offset) {
  CTTFontDesc* pFontDesc = new CTTFontDesc;
  pFontDesc->m_Type = 2;
  pFontDesc->m_pFontData = pData;
  for (int i = 0; i < 16; i++) {
    pFontDesc->m_TTCFace.m_pFaces[i] = NULL;
  }
  pFontDesc->m_RefCount++;
  m_FaceMap[KeyNameFromSize(ttc_size, checksum)] = pFontDesc;
  int face_index = GetTTCIndex(pFontDesc->m_pFontData, ttc_size, font_offset);
  pFontDesc->m_TTCFace.m_pFaces[face_index] =
      GetFixedFace(pFontDesc->m_pFontData, ttc_size, face_index);
  return pFontDesc->m_TTCFace.m_pFaces[face_index];
}
FXFT_Face CFX_FontMgr::GetFixedFace(const uint8_t* pData,
                                    FX_DWORD size,
                                    int face_index) {
  FXFT_Library library;
  if (m_FTLibrary == NULL) {
    FXFT_Init_FreeType(&m_FTLibrary);
  }
  library = m_FTLibrary;
  FXFT_Face face = NULL;
  int ret = FXFT_New_Memory_Face(library, pData, size, face_index, &face);
  if (ret) {
    return NULL;
  }
  ret = FXFT_Set_Pixel_Sizes(face, 64, 64);
  if (ret) {
    return NULL;
  }
  return face;
}
FXFT_Face CFX_FontMgr::GetFileFace(const FX_CHAR* filename, int face_index) {
  FXFT_Library library;
  if (m_FTLibrary == NULL) {
    FXFT_Init_FreeType(&m_FTLibrary);
  }
  library = m_FTLibrary;
  FXFT_Face face = NULL;
  int ret = FXFT_New_Face(library, filename, face_index, &face);
  if (ret) {
    return NULL;
  }
  ret = FXFT_Set_Pixel_Sizes(face, 64, 64);
  if (ret) {
    return NULL;
  }
  return face;
}
void CFX_FontMgr::ReleaseFace(FXFT_Face face) {
  if (!face) {
    return;
  }
  auto it = m_FaceMap.begin();
  while (it != m_FaceMap.end()) {
    auto temp = it++;
    if (temp->second->ReleaseFace(face)) {
      m_FaceMap.erase(temp);
    }
  }
}
extern "C" {
extern const unsigned char g_FoxitFixedItalicFontData[18746];
extern const unsigned char g_FoxitFixedFontData[17597];
extern const unsigned char g_FoxitSansItalicFontData[16339];
extern const unsigned char g_FoxitSansFontData[15025];
extern const unsigned char g_FoxitSerifItalicFontData[21227];
extern const unsigned char g_FoxitSerifFontData[19469];
extern const unsigned char g_FoxitFixedBoldItalicFontData[19151];
extern const unsigned char g_FoxitFixedBoldFontData[18055];
extern const unsigned char g_FoxitSansBoldItalicFontData[16418];
extern const unsigned char g_FoxitSansBoldFontData[16344];
extern const unsigned char g_FoxitSerifBoldItalicFontData[20733];
extern const unsigned char g_FoxitSerifBoldFontData[19395];
extern const unsigned char g_FoxitSymbolFontData[16729];
extern const unsigned char g_FoxitDingbatsFontData[29513];
extern const unsigned char g_FoxitSerifMMFontData[113417];
extern const unsigned char g_FoxitSansMMFontData[66919];
};
const FoxitFonts g_FoxitFonts[14] = {
    {g_FoxitFixedFontData, 17597},
    {g_FoxitFixedBoldFontData, 18055},
    {g_FoxitFixedBoldItalicFontData, 19151},
    {g_FoxitFixedItalicFontData, 18746},
    {g_FoxitSansFontData, 15025},
    {g_FoxitSansBoldFontData, 16344},
    {g_FoxitSansBoldItalicFontData, 16418},
    {g_FoxitSansItalicFontData, 16339},
    {g_FoxitSerifFontData, 19469},
    {g_FoxitSerifBoldFontData, 19395},
    {g_FoxitSerifBoldItalicFontData, 20733},
    {g_FoxitSerifItalicFontData, 21227},
    {g_FoxitSymbolFontData, 16729},
    {g_FoxitDingbatsFontData, 29513},
};
void _FPDFAPI_GetInternalFontData(int id,
                                  const uint8_t*& data,
                                  FX_DWORD& size) {
  CFX_GEModule::Get()->GetFontMgr()->GetStandardFont(data, size, id);
}
FX_BOOL CFX_FontMgr::GetStandardFont(const uint8_t*& pFontData,
                                     FX_DWORD& size,
                                     int index) {
  if (index > 15 || index < 0) {
    return FALSE;
  }
  {
    if (index >= 14) {
      if (index == 14) {
        pFontData = g_FoxitSerifMMFontData;
        size = 113417;
      } else {
        pFontData = g_FoxitSansMMFontData;
        size = 66919;
      }
    } else {
      pFontData = g_FoxitFonts[index].m_pFontData;
      size = g_FoxitFonts[index].m_dwSize;
    }
  }
  return TRUE;
}
CFX_FontMapper::CFX_FontMapper(CFX_FontMgr* mgr)
    : m_bListLoaded(FALSE),
      m_pFontInfo(nullptr),
      m_pFontEnumerator(nullptr),
      m_pFontMgr(mgr) {
  m_MMFaces[0] = nullptr;
  m_MMFaces[1] = nullptr;
  FXSYS_memset(m_FoxitFaces, 0, sizeof(m_FoxitFaces));
}
CFX_FontMapper::~CFX_FontMapper() {
  for (int i = 0; i < 14; i++)
    if (m_FoxitFaces[i]) {
      FXFT_Done_Face(m_FoxitFaces[i]);
    }
  if (m_MMFaces[0]) {
    FXFT_Done_Face(m_MMFaces[0]);
  }
  if (m_MMFaces[1]) {
    FXFT_Done_Face(m_MMFaces[1]);
  }
  if (m_pFontInfo) {
    m_pFontInfo->Release();
  }
}
void CFX_FontMapper::SetSystemFontInfo(IFX_SystemFontInfo* pFontInfo) {
  if (pFontInfo == NULL) {
    return;
  }
  if (m_pFontInfo) {
    m_pFontInfo->Release();
  }
  m_pFontInfo = pFontInfo;
}
static CFX_ByteString _TT_NormalizeName(const FX_CHAR* family) {
  CFX_ByteString norm(family, -1);
  norm.Remove(' ');
  norm.Remove('-');
  norm.Remove(',');
  int pos = norm.Find('+');
  if (pos > 0) {
    norm = norm.Left(pos);
  }
  norm.MakeLower();
  return norm;
}
CFX_ByteString _FPDF_GetNameFromTT(const uint8_t* name_table,
                                   FX_DWORD name_id) {
  const uint8_t* ptr = name_table + 2;
  int name_count = GET_TT_SHORT(ptr);
  int string_offset = GET_TT_SHORT(ptr + 2);
  const uint8_t* string_ptr = name_table + string_offset;
  ptr += 4;
  for (int i = 0; i < name_count; i++) {
    if (GET_TT_SHORT(ptr + 6) == name_id && GET_TT_SHORT(ptr) == 1 &&
        GET_TT_SHORT(ptr + 2) == 0) {
      return CFX_ByteStringC(string_ptr + GET_TT_SHORT(ptr + 10),
                             GET_TT_SHORT(ptr + 8));
    }
    ptr += 12;
  }
  return CFX_ByteString();
}
static CFX_ByteString _FPDF_ReadStringFromFile(FXSYS_FILE* pFile,
                                               FX_DWORD size) {
  CFX_ByteString buffer;
  if (!FXSYS_fread(buffer.GetBuffer(size), size, 1, pFile)) {
    return CFX_ByteString();
  }
  buffer.ReleaseBuffer(size);
  return buffer;
}
CFX_ByteString _FPDF_LoadTableFromTT(FXSYS_FILE* pFile,
                                     const uint8_t* pTables,
                                     FX_DWORD nTables,
                                     FX_DWORD tag) {
  for (FX_DWORD i = 0; i < nTables; i++) {
    const uint8_t* p = pTables + i * 16;
    if (GET_TT_LONG(p) == tag) {
      FX_DWORD offset = GET_TT_LONG(p + 8);
      FX_DWORD size = GET_TT_LONG(p + 12);
      FXSYS_fseek(pFile, offset, FXSYS_SEEK_SET);
      return _FPDF_ReadStringFromFile(pFile, size);
    }
  }
  return CFX_ByteString();
}
CFX_ByteString _FPDF_LoadTableFromTTStreamFile(IFX_FileStream* pFile,
                                               const uint8_t* pTables,
                                               FX_DWORD nTables,
                                               FX_DWORD tag) {
  for (FX_DWORD i = 0; i < nTables; i++) {
    const uint8_t* p = pTables + i * 16;
    if (GET_TT_LONG(p) == tag) {
      FX_DWORD offset = GET_TT_LONG(p + 8);
      FX_DWORD size = GET_TT_LONG(p + 12);
      CFX_ByteString buffer;
      if (!pFile->ReadBlock(buffer.GetBuffer(size), offset, size)) {
        return CFX_ByteString();
      }
      buffer.ReleaseBuffer(size);
      return buffer;
    }
  }
  return CFX_ByteString();
}
CFX_ByteString CFX_FontMapper::GetPSNameFromTT(void* hFont) {
  if (m_pFontInfo == NULL) {
    CFX_ByteString();
  }
  CFX_ByteString result;
  FX_DWORD size = m_pFontInfo->GetFontData(hFont, 0x6e616d65, NULL, 0);
  if (size) {
    uint8_t* buffer = FX_Alloc(uint8_t, size);
    m_pFontInfo->GetFontData(hFont, 0x6e616d65, buffer, size);
    result = _FPDF_GetNameFromTT(buffer, 6);
    FX_Free(buffer);
  }
  return result;
}
void CFX_FontMapper::AddInstalledFont(const CFX_ByteString& name, int charset) {
  if (m_pFontInfo == NULL) {
    return;
  }
  if (m_CharsetArray.Find((FX_DWORD)charset) == -1) {
    m_CharsetArray.Add((FX_DWORD)charset);
    m_FaceArray.Add(name);
  }
  if (name == m_LastFamily) {
    return;
  }
  const uint8_t* ptr = name;
  FX_BOOL bLocalized = FALSE;
  for (int i = 0; i < name.GetLength(); i++)
    if (ptr[i] > 0x80) {
      bLocalized = TRUE;
      break;
    }
  if (bLocalized) {
    void* hFont = m_pFontInfo->GetFont(name);
    if (hFont == NULL) {
      int iExact;
      hFont =
          m_pFontInfo->MapFont(0, 0, FXFONT_DEFAULT_CHARSET, 0, name, iExact);
      if (hFont == NULL) {
        return;
      }
    }
    CFX_ByteString new_name = GetPSNameFromTT(hFont);
    if (!new_name.IsEmpty()) {
      new_name.Insert(0, ' ');
      m_InstalledTTFonts.Add(new_name);
    }
    m_pFontInfo->DeleteFont(hFont);
  }
  m_InstalledTTFonts.Add(name);
  m_LastFamily = name;
}
void CFX_FontMapper::LoadInstalledFonts() {
  if (m_pFontInfo == NULL) {
    return;
  }
  if (m_bListLoaded) {
    return;
  }
  if (m_bListLoaded) {
    return;
  }
  m_pFontInfo->EnumFontList(this);
  m_bListLoaded = TRUE;
}
CFX_ByteString CFX_FontMapper::MatchInstalledFonts(
    const CFX_ByteString& norm_name) {
  LoadInstalledFonts();
  int i;
  for (i = m_InstalledTTFonts.GetSize() - 1; i >= 0; i--) {
    CFX_ByteString norm1 = _TT_NormalizeName(m_InstalledTTFonts[i]);
    if (norm1 == norm_name) {
      break;
    }
  }
  if (i < 0) {
    return CFX_ByteString();
  }
  CFX_ByteString match = m_InstalledTTFonts[i];
  if (match[0] == ' ') {
    match = m_InstalledTTFonts[i + 1];
  }
  return match;
}
typedef struct _CHARSET_MAP_ {
  uint8_t charset;
  FX_WORD codepage;
} CHARSET_MAP;
static const CHARSET_MAP g_Codepage2CharsetTable[] = {
    {1, 0},      {2, 42},     {254, 437},  {255, 850},  {222, 874},
    {128, 932},  {134, 936},  {129, 949},  {136, 950},  {238, 1250},
    {204, 1251}, {0, 1252},   {161, 1253}, {162, 1254}, {177, 1255},
    {178, 1256}, {186, 1257}, {163, 1258}, {130, 1361}, {77, 10000},
    {78, 10001}, {79, 10003}, {80, 10008}, {81, 10002}, {83, 10005},
    {84, 10004}, {85, 10006}, {86, 10081}, {87, 10021}, {88, 10029},
    {89, 10007},
};
uint8_t _GetCharsetFromCodePage(FX_WORD codepage) {
  int32_t iEnd = sizeof(g_Codepage2CharsetTable) / sizeof(CHARSET_MAP) - 1;
  FXSYS_assert(iEnd >= 0);
  int32_t iStart = 0, iMid;
  do {
    iMid = (iStart + iEnd) / 2;
    const CHARSET_MAP& cp = g_Codepage2CharsetTable[iMid];
    if (codepage == cp.codepage) {
      return cp.charset;
    }
    if (codepage < cp.codepage) {
      iEnd = iMid - 1;
    } else {
      iStart = iMid + 1;
    }
  } while (iStart <= iEnd);
  return 1;
}
FX_DWORD _GetCodePageRangeFromCharset(int charset) {
  if (charset == FXFONT_EASTEUROPE_CHARSET) {
    return 1 << 1;
  }
  if (charset == FXFONT_GREEK_CHARSET) {
    return 1 << 3;
  }
  if (charset == FXFONT_TURKISH_CHARSET) {
    return 1 << 4;
  }
  if (charset == FXFONT_HEBREW_CHARSET) {
    return 1 << 5;
  }
  if (charset == FXFONT_ARABIC_CHARSET) {
    return 1 << 6;
  }
  if (charset == FXFONT_BALTIC_CHARSET) {
    return 1 << 7;
  }
  if (charset == FXFONT_THAI_CHARSET) {
    return 1 << 16;
  }
  if (charset == FXFONT_SHIFTJIS_CHARSET) {
    return 1 << 17;
  }
  if (charset == FXFONT_GB2312_CHARSET) {
    return 1 << 18;
  }
  if (charset == FXFONT_CHINESEBIG5_CHARSET) {
    return 1 << 20;
  }
  if (charset == FXFONT_HANGEUL_CHARSET) {
    return 1 << 19;
  }
  if (charset == FXFONT_SYMBOL_CHARSET) {
    return 1 << 31;
  }
  return 1 << 21;
}
FXFT_Face CFX_FontMapper::UseInternalSubst(CFX_SubstFont* pSubstFont,
                                           int iBaseFont,
                                           int italic_angle,
                                           int weight,
                                           int picthfamily) {
  if (iBaseFont < 12) {
    if (m_FoxitFaces[iBaseFont]) {
      return m_FoxitFaces[iBaseFont];
    }
    const uint8_t* pFontData = NULL;
    FX_DWORD size = 0;
    if (m_pFontMgr->GetStandardFont(pFontData, size, iBaseFont)) {
      m_FoxitFaces[iBaseFont] = m_pFontMgr->GetFixedFace(pFontData, size, 0);
      return m_FoxitFaces[iBaseFont];
    }
  }
  pSubstFont->m_SubstFlags |= FXFONT_SUBST_MM;
  pSubstFont->m_ItalicAngle = italic_angle;
  if (weight) {
    pSubstFont->m_Weight = weight;
  }
  if (picthfamily & FXFONT_FF_ROMAN) {
    pSubstFont->m_Weight = pSubstFont->m_Weight * 4 / 5;
    pSubstFont->m_Family = "Chrome Serif";
    if (m_MMFaces[1]) {
      return m_MMFaces[1];
    }
    const uint8_t* pFontData = NULL;
    FX_DWORD size;
    m_pFontMgr->GetStandardFont(pFontData, size, 14);
    m_MMFaces[1] = m_pFontMgr->GetFixedFace(pFontData, size, 0);
    return m_MMFaces[1];
  }
  pSubstFont->m_Family = "Chrome Sans";
  if (m_MMFaces[0]) {
    return m_MMFaces[0];
  }
  const uint8_t* pFontData = NULL;
  FX_DWORD size = 0;
  m_pFontMgr->GetStandardFont(pFontData, size, 15);
  m_MMFaces[0] = m_pFontMgr->GetFixedFace(pFontData, size, 0);
  return m_MMFaces[0];
}
const struct _AltFontFamily {
  const FX_CHAR* m_pFontName;
  const FX_CHAR* m_pFontFamily;
} g_AltFontFamilies[] = {
    {"AGaramondPro", "Adobe Garamond Pro"},
    {"BankGothicBT-Medium", "BankGothic Md BT"},
    {"ForteMT", "Forte"},
};
extern "C" {
static int compareFontFamilyString(const void* key, const void* element) {
  CFX_ByteString str_key((const FX_CHAR*)key);
  if (str_key.Find(((_AltFontFamily*)element)->m_pFontName) != -1) {
    return 0;
  }
  return FXSYS_stricmp((const FX_CHAR*)key,
                       ((_AltFontFamily*)element)->m_pFontName);
}
}
#define FX_FONT_STYLE_None 0x00
#define FX_FONT_STYLE_Bold 0x01
#define FX_FONT_STYLE_Italic 0x02
#define FX_FONT_STYLE_BoldBold 0x04
static CFX_ByteString _GetFontFamily(CFX_ByteString fontName, int nStyle) {
  if (fontName.Find("Script") >= 0) {
    if ((nStyle & FX_FONT_STYLE_Bold) == FX_FONT_STYLE_Bold) {
      fontName = "ScriptMTBold";
    } else if (fontName.Find("Palace") >= 0) {
      fontName = "PalaceScriptMT";
    } else if (fontName.Find("French") >= 0) {
      fontName = "FrenchScriptMT";
    } else if (fontName.Find("FreeStyle") >= 0) {
      fontName = "FreeStyleScript";
    }
    return fontName;
  }
  _AltFontFamily* found = (_AltFontFamily*)FXSYS_bsearch(
      fontName.c_str(), g_AltFontFamilies,
      sizeof g_AltFontFamilies / sizeof(_AltFontFamily), sizeof(_AltFontFamily),
      compareFontFamilyString);
  if (found == NULL) {
    return fontName;
  }
  return found->m_pFontFamily;
};
typedef struct _FX_FontStyle {
  const FX_CHAR* style;
  int32_t len;
} FX_FontStyle;
const FX_FontStyle g_FontStyles[] = {
    {"Bold", 4}, {"Italic", 6}, {"BoldItalic", 10}, {"Reg", 3}, {"Regular", 7},
};
CFX_ByteString ParseStyle(const FX_CHAR* pStyle, int iLen, int iIndex) {
  CFX_ByteTextBuf buf;
  if (!iLen || iLen <= iIndex) {
    return buf.GetByteString();
  }
  while (iIndex < iLen) {
    if (pStyle[iIndex] == ',') {
      break;
    }
    buf.AppendChar(pStyle[iIndex]);
    ++iIndex;
  }
  return buf.GetByteString();
}
int32_t GetStyleType(const CFX_ByteString& bsStyle, FX_BOOL bRevert) {
  int32_t iLen = bsStyle.GetLength();
  if (!iLen) {
    return -1;
  }
  int iSize = sizeof(g_FontStyles) / sizeof(FX_FontStyle);
  const FX_FontStyle* pStyle = NULL;
  for (int i = iSize - 1; i >= 0; --i) {
    pStyle = g_FontStyles + i;
    if (!pStyle || pStyle->len > iLen) {
      continue;
    }
    if (!bRevert) {
      if (bsStyle.Left(pStyle->len).Compare(pStyle->style) == 0) {
        return i;
      }
    } else {
      if (bsStyle.Right(pStyle->len).Compare(pStyle->style) == 0) {
        return i;
      }
    }
  }
  return -1;
}
FX_BOOL CheckSupportThirdPartFont(CFX_ByteString name, int& PitchFamily) {
  if (name == FX_BSTRC("MyriadPro")) {
    PitchFamily &= ~FXFONT_FF_ROMAN;
    return TRUE;
  }
  return FALSE;
}
FXFT_Face CFX_FontMapper::FindSubstFont(const CFX_ByteString& name,
                                        FX_BOOL bTrueType,
                                        FX_DWORD flags,
                                        int weight,
                                        int italic_angle,
                                        int WindowCP,
                                        CFX_SubstFont* pSubstFont) {
  if (!(flags & FXFONT_USEEXTERNATTR)) {
    weight = FXFONT_FW_NORMAL;
    italic_angle = 0;
  }
  CFX_ByteString SubstName = name;
  SubstName.Remove(0x20);
  if (bTrueType) {
    if (name[0] == '@') {
      SubstName = name.Mid(1);
    }
  }
  _PDF_GetStandardFontName(SubstName);
  if (SubstName == FX_BSTRC("Symbol") && !bTrueType) {
    pSubstFont->m_Family = "Chrome Symbol";
    pSubstFont->m_Charset = FXFONT_SYMBOL_CHARSET;
    pSubstFont->m_SubstFlags |= FXFONT_SUBST_STANDARD;
    if (m_FoxitFaces[12]) {
      return m_FoxitFaces[12];
    }
    const uint8_t* pFontData = NULL;
    FX_DWORD size = 0;
    m_pFontMgr->GetStandardFont(pFontData, size, 12);
    m_FoxitFaces[12] = m_pFontMgr->GetFixedFace(pFontData, size, 0);
    return m_FoxitFaces[12];
  }
  if (SubstName == FX_BSTRC("ZapfDingbats")) {
    pSubstFont->m_Family = "Chrome Dingbats";
    pSubstFont->m_Charset = FXFONT_SYMBOL_CHARSET;
    pSubstFont->m_SubstFlags |= FXFONT_SUBST_STANDARD;
    if (m_FoxitFaces[13]) {
      return m_FoxitFaces[13];
    }
    const uint8_t* pFontData = NULL;
    FX_DWORD size = 0;
    m_pFontMgr->GetStandardFont(pFontData, size, 13);
    m_FoxitFaces[13] = m_pFontMgr->GetFixedFace(pFontData, size, 0);
    return m_FoxitFaces[13];
  }
  int iBaseFont = 0;
  CFX_ByteString family, style;
  FX_BOOL bHasComma = FALSE;
  FX_BOOL bHasHypen = FALSE;
  int find = SubstName.Find(FX_BSTRC(","), 0);
  if (find >= 0) {
    family = SubstName.Left(find);
    _PDF_GetStandardFontName(family);
    style = SubstName.Mid(find + 1);
    bHasComma = TRUE;
  } else {
    family = SubstName;
  }
  for (; iBaseFont < 12; iBaseFont++)
    if (family == CFX_ByteStringC(g_Base14FontNames[iBaseFont])) {
      break;
    }
  int PitchFamily = 0;
  FX_BOOL bItalic = FALSE;
  FX_DWORD nStyle = 0;
  FX_BOOL bStyleAvail = FALSE;
  if (iBaseFont < 12) {
    family = g_Base14FontNames[iBaseFont];
    if ((iBaseFont % 4) == 1 || (iBaseFont % 4) == 2) {
      nStyle |= FX_FONT_STYLE_Bold;
    }
    if ((iBaseFont % 4) / 2) {
      nStyle |= FX_FONT_STYLE_Italic;
    }
    if (iBaseFont < 4) {
      PitchFamily |= FXFONT_FF_FIXEDPITCH;
    }
    if (iBaseFont >= 8) {
      PitchFamily |= FXFONT_FF_ROMAN;
    }
  } else {
    if (!bHasComma) {
      find = family.ReverseFind('-');
      if (find >= 0) {
        style = family.Mid(find + 1);
        family = family.Left(find);
        bHasHypen = TRUE;
      }
    }
    if (!bHasHypen) {
      int nLen = family.GetLength();
      int32_t nRet = GetStyleType(family, TRUE);
      if (nRet > -1) {
        family = family.Left(nLen - g_FontStyles[nRet].len);
        if (nRet == 0) {
          nStyle |= FX_FONT_STYLE_Bold;
        }
        if (nRet == 1) {
          nStyle |= FX_FONT_STYLE_Italic;
        }
        if (nRet == 2) {
          nStyle |= (FX_FONT_STYLE_Bold | FX_FONT_STYLE_Italic);
        }
      }
    }
    if (flags & FXFONT_SERIF) {
      PitchFamily |= FXFONT_FF_ROMAN;
    }
    if (flags & FXFONT_SCRIPT) {
      PitchFamily |= FXFONT_FF_SCRIPT;
    }
    if (flags & FXFONT_FIXED_PITCH) {
      PitchFamily |= FXFONT_FF_FIXEDPITCH;
    }
  }
  if (!style.IsEmpty()) {
    int nLen = style.GetLength();
    const FX_CHAR* pStyle = style;
    int i = 0;
    FX_BOOL bFirstItem = TRUE;
    CFX_ByteString buf;
    while (i < nLen) {
      buf = ParseStyle(pStyle, nLen, i);
      int32_t nRet = GetStyleType(buf, FALSE);
      if ((i && !bStyleAvail) || (!i && nRet < 0)) {
        family = SubstName;
        iBaseFont = 12;
        break;
      } else if (nRet >= 0) {
        bStyleAvail = TRUE;
      }
      if (nRet == 0) {
        if (nStyle & FX_FONT_STYLE_Bold) {
          nStyle |= FX_FONT_STYLE_BoldBold;
        } else {
          nStyle |= FX_FONT_STYLE_Bold;
        }
        bFirstItem = FALSE;
      }
      if (nRet == 1) {
        if (bFirstItem) {
          nStyle |= FX_FONT_STYLE_Italic;
        } else {
          family = SubstName;
          iBaseFont = 12;
        }
        break;
      }
      if (nRet == 2) {
        nStyle |= FX_FONT_STYLE_Italic;
        if (nStyle & FX_FONT_STYLE_Bold) {
          nStyle |= FX_FONT_STYLE_BoldBold;
        } else {
          nStyle |= FX_FONT_STYLE_Bold;
        }
        bFirstItem = FALSE;
      }
      i += buf.GetLength() + 1;
    }
  }
  weight = weight ? weight : FXFONT_FW_NORMAL;
  int old_weight = weight;
  if (nStyle) {
    weight =
        nStyle & FX_FONT_STYLE_BoldBold
            ? 900
            : (nStyle & FX_FONT_STYLE_Bold ? FXFONT_FW_BOLD : FXFONT_FW_NORMAL);
  }
  if (nStyle & FX_FONT_STYLE_Italic) {
    bItalic = TRUE;
  }
  FX_BOOL bCJK = FALSE;
  int iExact = 0;
  int Charset = FXFONT_ANSI_CHARSET;
  if (WindowCP) {
    Charset = _GetCharsetFromCodePage(WindowCP);
  } else if (iBaseFont == 12 && (flags & FXFONT_SYMBOLIC)) {
    Charset = FXFONT_SYMBOL_CHARSET;
  }
  if (Charset == FXFONT_SHIFTJIS_CHARSET || Charset == FXFONT_GB2312_CHARSET ||
      Charset == FXFONT_HANGEUL_CHARSET ||
      Charset == FXFONT_CHINESEBIG5_CHARSET) {
    bCJK = TRUE;
  }
  if (m_pFontInfo == NULL) {
    pSubstFont->m_SubstFlags |= FXFONT_SUBST_STANDARD;
    return UseInternalSubst(pSubstFont, iBaseFont, italic_angle, old_weight,
                            PitchFamily);
  }
  family = _GetFontFamily(family, nStyle);
  CFX_ByteString match = MatchInstalledFonts(_TT_NormalizeName(family));
  if (match.IsEmpty() && family != SubstName &&
      (!bHasComma && (!bHasHypen || (bHasHypen && !bStyleAvail)))) {
    match = MatchInstalledFonts(_TT_NormalizeName(SubstName));
  }
  if (match.IsEmpty() && iBaseFont >= 12) {
    if (!bCJK) {
      if (!CheckSupportThirdPartFont(family, PitchFamily)) {
        if (italic_angle != 0) {
          bItalic = TRUE;
        } else {
          bItalic = FALSE;
        }
        weight = old_weight;
      }
    } else {
      pSubstFont->m_bSubstOfCJK = TRUE;
      if (nStyle) {
        pSubstFont->m_WeightCJK = weight;
      } else {
        pSubstFont->m_WeightCJK = FXFONT_FW_NORMAL;
      }
      if (nStyle & FX_FONT_STYLE_Italic) {
        pSubstFont->m_bItlicCJK = TRUE;
      }
    }
  } else {
    italic_angle = 0;
    weight =
        nStyle & FX_FONT_STYLE_BoldBold
            ? 900
            : (nStyle & FX_FONT_STYLE_Bold ? FXFONT_FW_BOLD : FXFONT_FW_NORMAL);
  }
  if (!match.IsEmpty() || iBaseFont < 12) {
    pSubstFont->m_SubstFlags |= FXFONT_SUBST_EXACT;
    if (!match.IsEmpty()) {
      family = match;
    }
    if (iBaseFont < 12) {
      if (nStyle && !(iBaseFont % 4)) {
        if ((nStyle & 0x3) == 1) {
          iBaseFont += 1;
        }
        if ((nStyle & 0x3) == 2) {
          iBaseFont += 3;
        }
        if ((nStyle & 0x3) == 3) {
          iBaseFont += 2;
        }
      }
      if (m_pFontMgr->m_ExternalFonts[iBaseFont].m_pFontData) {
        if (m_FoxitFaces[iBaseFont]) {
          return m_FoxitFaces[iBaseFont];
        }
        m_FoxitFaces[iBaseFont] = m_pFontMgr->GetFixedFace(
            m_pFontMgr->m_ExternalFonts[iBaseFont].m_pFontData,
            m_pFontMgr->m_ExternalFonts[iBaseFont].m_dwSize, 0);
        if (m_FoxitFaces[iBaseFont]) {
          return m_FoxitFaces[iBaseFont];
        }
      } else {
        family = g_Base14FontNames[iBaseFont];
      }
      pSubstFont->m_SubstFlags |= FXFONT_SUBST_STANDARD;
    }
  } else {
    if (flags & FXFONT_ITALIC) {
      bItalic = TRUE;
    }
  }
  iExact = !match.IsEmpty();
  void* hFont = m_pFontInfo->MapFont(weight, bItalic, Charset, PitchFamily,
                                     family, iExact);
  if (iExact) {
    pSubstFont->m_SubstFlags |= FXFONT_SUBST_EXACT;
  }
  if (hFont == NULL) {
    if (bCJK) {
      if (italic_angle != 0) {
        bItalic = TRUE;
      } else {
        bItalic = FALSE;
      }
      weight = old_weight;
    }
    if (!match.IsEmpty()) {
      hFont = m_pFontInfo->GetFont(match);
      if (hFont == NULL) {
        return UseInternalSubst(pSubstFont, iBaseFont, italic_angle, old_weight,
                                PitchFamily);
      }
    } else {
      if (Charset == FXFONT_SYMBOL_CHARSET) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_ || \
    _FXM_PLATFORM_ == _FXM_PLATFORM_ANDROID_
        if (SubstName == FX_BSTRC("Symbol")) {
          pSubstFont->m_Family = "Chrome Symbol";
          pSubstFont->m_SubstFlags |= FXFONT_SUBST_STANDARD;
          pSubstFont->m_Charset = FXFONT_SYMBOL_CHARSET;
          if (m_FoxitFaces[12]) {
            return m_FoxitFaces[12];
          }
          const uint8_t* pFontData = NULL;
          FX_DWORD size = 0;
          m_pFontMgr->GetStandardFont(pFontData, size, 12);
          m_FoxitFaces[12] = m_pFontMgr->GetFixedFace(pFontData, size, 0);
          return m_FoxitFaces[12];
        }
#endif
        pSubstFont->m_SubstFlags |= FXFONT_SUBST_NONSYMBOL;
        return FindSubstFont(family, bTrueType, flags & ~FXFONT_SYMBOLIC,
                             weight, italic_angle, 0, pSubstFont);
      }
      if (Charset == FXFONT_ANSI_CHARSET) {
        pSubstFont->m_SubstFlags |= FXFONT_SUBST_STANDARD;
        return UseInternalSubst(pSubstFont, iBaseFont, italic_angle, old_weight,
                                PitchFamily);
      }
      int index = m_CharsetArray.Find(Charset);
      if (index < 0) {
        return UseInternalSubst(pSubstFont, iBaseFont, italic_angle, old_weight,
                                PitchFamily);
      }
      hFont = m_pFontInfo->GetFont(m_FaceArray[index]);
    }
  }
  pSubstFont->m_ExtHandle = m_pFontInfo->RetainFont(hFont);
  if (hFont == NULL) {
    return NULL;
  }
  m_pFontInfo->GetFaceName(hFont, SubstName);
  if (Charset == FXFONT_DEFAULT_CHARSET) {
    m_pFontInfo->GetFontCharset(hFont, Charset);
  }
  FX_DWORD ttc_size = m_pFontInfo->GetFontData(hFont, 0x74746366, NULL, 0);
  FX_DWORD font_size = m_pFontInfo->GetFontData(hFont, 0, NULL, 0);
  if (font_size == 0 && ttc_size == 0) {
    m_pFontInfo->DeleteFont(hFont);
    return NULL;
  }
  FXFT_Face face = NULL;
  if (ttc_size) {
    uint8_t temp[1024];
    m_pFontInfo->GetFontData(hFont, 0x74746366, temp, 1024);
    FX_DWORD checksum = 0;
    for (int i = 0; i < 256; i++) {
      checksum += ((FX_DWORD*)temp)[i];
    }
    uint8_t* pFontData;
    face = m_pFontMgr->GetCachedTTCFace(ttc_size, checksum,
                                        ttc_size - font_size, pFontData);
    if (face == NULL) {
      pFontData = FX_Alloc(uint8_t, ttc_size);
      m_pFontInfo->GetFontData(hFont, 0x74746366, pFontData, ttc_size);
      face = m_pFontMgr->AddCachedTTCFace(ttc_size, checksum, pFontData,
                                          ttc_size, ttc_size - font_size);
    }
  } else {
    uint8_t* pFontData;
    face = m_pFontMgr->GetCachedFace(SubstName, weight, bItalic, pFontData);
    if (face == NULL) {
      pFontData = FX_Alloc(uint8_t, font_size);
      m_pFontInfo->GetFontData(hFont, 0, pFontData, font_size);
      face = m_pFontMgr->AddCachedFace(SubstName, weight, bItalic, pFontData,
                                       font_size,
                                       m_pFontInfo->GetFaceIndex(hFont));
    }
  }
  if (face == NULL) {
    m_pFontInfo->DeleteFont(hFont);
    return NULL;
  }
  pSubstFont->m_Family = SubstName;
  pSubstFont->m_Charset = Charset;
  FX_BOOL bNeedUpdateWeight = FALSE;
  if (FXFT_Is_Face_Bold(face)) {
    if (weight == FXFONT_FW_BOLD) {
      bNeedUpdateWeight = FALSE;
    } else {
      bNeedUpdateWeight = TRUE;
    }
  } else {
    if (weight == FXFONT_FW_NORMAL) {
      bNeedUpdateWeight = FALSE;
    } else {
      bNeedUpdateWeight = TRUE;
    }
  }
  if (bNeedUpdateWeight) {
    pSubstFont->m_Weight = weight;
  }
  if (bItalic && !FXFT_Is_Face_Italic(face)) {
    if (italic_angle == 0) {
      italic_angle = -12;
    } else if (FXSYS_abs(italic_angle) < 5) {
      italic_angle = 0;
    }
    pSubstFont->m_ItalicAngle = italic_angle;
  }
  m_pFontInfo->DeleteFont(hFont);
  return face;
}
extern "C" {
unsigned long _FTStreamRead(FXFT_Stream stream,
                            unsigned long offset,
                            unsigned char* buffer,
                            unsigned long count);
void _FTStreamClose(FXFT_Stream stream);
};
CFontFileFaceInfo::CFontFileFaceInfo() {
  m_pFile = NULL;
  m_Face = NULL;
  m_Charsets = 0;
  m_FileSize = 0;
  m_FontOffset = 0;
  m_Weight = 0;
  m_bItalic = FALSE;
  m_PitchFamily = 0;
}
CFontFileFaceInfo::~CFontFileFaceInfo() {
  if (m_Face) {
    FXFT_Done_Face(m_Face);
  }
  m_Face = NULL;
}
extern FX_BOOL _LoadFile(FXFT_Library library,
                         FXFT_Face* Face,
                         IFX_FileRead* pFile,
                         FXFT_Stream* stream);
#if _FX_OS_ == _FX_ANDROID_
IFX_SystemFontInfo* IFX_SystemFontInfo::CreateDefault(const char** pUnused) {
  return NULL;
}
#endif
CFX_FolderFontInfo::CFX_FolderFontInfo() {}
CFX_FolderFontInfo::~CFX_FolderFontInfo() {
  for (const auto& pair : m_FontList) {
    delete pair.second;
  }
}
void CFX_FolderFontInfo::AddPath(const CFX_ByteStringC& path) {
  m_PathList.Add(path);
}
void CFX_FolderFontInfo::Release() {
  delete this;
}
FX_BOOL CFX_FolderFontInfo::EnumFontList(CFX_FontMapper* pMapper) {
  m_pMapper = pMapper;
  for (int i = 0; i < m_PathList.GetSize(); i++) {
    ScanPath(m_PathList[i]);
  }
  return TRUE;
}
void CFX_FolderFontInfo::ScanPath(CFX_ByteString& path) {
  void* handle = FX_OpenFolder(path);
  if (handle == NULL) {
    return;
  }
  CFX_ByteString filename;
  FX_BOOL bFolder;
  while (FX_GetNextFile(handle, filename, bFolder)) {
    if (bFolder) {
      if (filename == "." || filename == "..") {
        continue;
      }
    } else {
      CFX_ByteString ext = filename.Right(4);
      ext.MakeUpper();
      if (ext != ".TTF" && ext != ".OTF" && ext != ".TTC") {
        continue;
      }
    }
    CFX_ByteString fullpath = path;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
    fullpath += "\\";
#else
    fullpath += "/";
#endif
    fullpath += filename;
    if (bFolder) {
      ScanPath(fullpath);
    } else {
      ScanFile(fullpath);
    }
  }
  FX_CloseFolder(handle);
}
void CFX_FolderFontInfo::ScanFile(CFX_ByteString& path) {
  FXSYS_FILE* pFile = FXSYS_fopen(path, "rb");
  if (pFile == NULL) {
    return;
  }
  FXSYS_fseek(pFile, 0, FXSYS_SEEK_END);
  FX_DWORD filesize = FXSYS_ftell(pFile);
  uint8_t buffer[16];
  FXSYS_fseek(pFile, 0, FXSYS_SEEK_SET);
  size_t readCnt = FXSYS_fread(buffer, 12, 1, pFile);
  if (readCnt != 1) {
    FXSYS_fclose(pFile);
    return;
  }

  if (GET_TT_LONG(buffer) == 0x74746366) {
    FX_DWORD nFaces = GET_TT_LONG(buffer + 8);
    if (nFaces > std::numeric_limits<FX_DWORD>::max() / 4) {
      FXSYS_fclose(pFile);
      return;
    }
    FX_DWORD face_bytes = nFaces * 4;
    uint8_t* offsets = FX_Alloc(uint8_t, face_bytes);
    readCnt = FXSYS_fread(offsets, 1, face_bytes, pFile);
    if (readCnt != face_bytes) {
      FX_Free(offsets);
      FXSYS_fclose(pFile);
      return;
    }
    for (FX_DWORD i = 0; i < nFaces; i++) {
      uint8_t* p = offsets + i * 4;
      ReportFace(path, pFile, filesize, GET_TT_LONG(p));
    }
    FX_Free(offsets);
  } else {
    ReportFace(path, pFile, filesize, 0);
  }
  FXSYS_fclose(pFile);
}
void CFX_FolderFontInfo::ReportFace(CFX_ByteString& path,
                                    FXSYS_FILE* pFile,
                                    FX_DWORD filesize,
                                    FX_DWORD offset) {
  FXSYS_fseek(pFile, offset, FXSYS_SEEK_SET);
  char buffer[16];
  if (!FXSYS_fread(buffer, 12, 1, pFile)) {
    return;
  }
  FX_DWORD nTables = GET_TT_SHORT(buffer + 4);
  CFX_ByteString tables = _FPDF_ReadStringFromFile(pFile, nTables * 16);
  if (tables.IsEmpty()) {
    return;
  }
  CFX_ByteString names =
      _FPDF_LoadTableFromTT(pFile, tables, nTables, 0x6e616d65);
  CFX_ByteString facename = _FPDF_GetNameFromTT(names, 1);
  CFX_ByteString style = _FPDF_GetNameFromTT(names, 2);
  if (style != "Regular") {
    facename += " " + style;
  }
  if (m_FontList.find(facename) != m_FontList.end()) {
    return;
  }
  CFX_FontFaceInfo* pInfo =
      new CFX_FontFaceInfo(path, facename, tables, offset, filesize);
  CFX_ByteString os2 =
      _FPDF_LoadTableFromTT(pFile, tables, nTables, 0x4f532f32);
  if (os2.GetLength() >= 86) {
    const uint8_t* p = (const uint8_t*)os2 + 78;
    FX_DWORD codepages = GET_TT_LONG(p);
    if (codepages & (1 << 17)) {
      m_pMapper->AddInstalledFont(facename, FXFONT_SHIFTJIS_CHARSET);
      pInfo->m_Charsets |= CHARSET_FLAG_SHIFTJIS;
    }
    if (codepages & (1 << 18)) {
      m_pMapper->AddInstalledFont(facename, FXFONT_GB2312_CHARSET);
      pInfo->m_Charsets |= CHARSET_FLAG_GB;
    }
    if (codepages & (1 << 20)) {
      m_pMapper->AddInstalledFont(facename, FXFONT_CHINESEBIG5_CHARSET);
      pInfo->m_Charsets |= CHARSET_FLAG_BIG5;
    }
    if ((codepages & (1 << 19)) || (codepages & (1 << 21))) {
      m_pMapper->AddInstalledFont(facename, FXFONT_HANGEUL_CHARSET);
      pInfo->m_Charsets |= CHARSET_FLAG_KOREAN;
    }
    if (codepages & (1 << 31)) {
      m_pMapper->AddInstalledFont(facename, FXFONT_SYMBOL_CHARSET);
      pInfo->m_Charsets |= CHARSET_FLAG_SYMBOL;
    }
  }
  m_pMapper->AddInstalledFont(facename, FXFONT_ANSI_CHARSET);
  pInfo->m_Charsets |= CHARSET_FLAG_ANSI;
  pInfo->m_Styles = 0;
  if (style.Find(FX_BSTRC("Bold")) > -1) {
    pInfo->m_Styles |= FXFONT_BOLD;
  }
  if (style.Find(FX_BSTRC("Italic")) > -1 ||
      style.Find(FX_BSTRC("Oblique")) > -1) {
    pInfo->m_Styles |= FXFONT_ITALIC;
  }
  if (facename.Find(FX_BSTRC("Serif")) > -1) {
    pInfo->m_Styles |= FXFONT_SERIF;
  }
  m_FontList[facename] = pInfo;
}
void* CFX_FolderFontInfo::MapFont(int weight,
                                  FX_BOOL bItalic,
                                  int charset,
                                  int pitch_family,
                                  const FX_CHAR* family,
                                  int& iExact) {
  return NULL;
}
void* CFX_FolderFontInfo::GetFont(const FX_CHAR* face) {
  auto it = m_FontList.find(face);
  return it != m_FontList.end() ? it->second : nullptr;
}
FX_DWORD CFX_FolderFontInfo::GetFontData(void* hFont,
                                         FX_DWORD table,
                                         uint8_t* buffer,
                                         FX_DWORD size) {
  if (hFont == NULL) {
    return 0;
  }
  CFX_FontFaceInfo* pFont = (CFX_FontFaceInfo*)hFont;
  FXSYS_FILE* pFile = NULL;
  if (size > 0) {
    pFile = FXSYS_fopen(pFont->m_FilePath, "rb");
    if (pFile == NULL) {
      return 0;
    }
  }
  FX_DWORD datasize = 0;
  FX_DWORD offset;
  if (table == 0) {
    datasize = pFont->m_FontOffset ? 0 : pFont->m_FileSize;
    offset = 0;
  } else if (table == 0x74746366) {
    datasize = pFont->m_FontOffset ? pFont->m_FileSize : 0;
    offset = 0;
  } else {
    FX_DWORD nTables = pFont->m_FontTables.GetLength() / 16;
    for (FX_DWORD i = 0; i < nTables; i++) {
      const uint8_t* p = (const uint8_t*)pFont->m_FontTables + i * 16;
      if (GET_TT_LONG(p) == table) {
        offset = GET_TT_LONG(p + 8);
        datasize = GET_TT_LONG(p + 12);
      }
    }
  }
  if (datasize && size >= datasize && pFile) {
    FXSYS_fseek(pFile, offset, FXSYS_SEEK_SET);
    FXSYS_fread(buffer, datasize, 1, pFile);
  }
  if (pFile) {
    FXSYS_fclose(pFile);
  }
  return datasize;
}
void CFX_FolderFontInfo::DeleteFont(void* hFont) {}
FX_BOOL CFX_FolderFontInfo::GetFaceName(void* hFont, CFX_ByteString& name) {
  if (hFont == NULL) {
    return FALSE;
  }
  CFX_FontFaceInfo* pFont = (CFX_FontFaceInfo*)hFont;
  name = pFont->m_FaceName;
  return TRUE;
}
FX_BOOL CFX_FolderFontInfo::GetFontCharset(void* hFont, int& charset) {
  return FALSE;
}
