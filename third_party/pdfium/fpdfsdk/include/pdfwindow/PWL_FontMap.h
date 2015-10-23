// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_INCLUDE_PDFWINDOW_PWL_FONTMAP_H_
#define FPDFSDK_INCLUDE_PDFWINDOW_PWL_FONTMAP_H_

#include "../../../public/fpdf_sysfontinfo.h"
#include "../fxedit/fx_edit.h"

class CPDF_Document;
class IFX_SystemHandler;

struct CPWL_FontMap_Data {
  CPDF_Font* pFont;
  int32_t nCharset;
  CFX_ByteString sFontName;
};

struct CPWL_FontMap_Native {
  int32_t nCharset;
  CFX_ByteString sFontName;
};

#ifndef ANSI_CHARSET

#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define SYMBOL_CHARSET 2
#define SHIFTJIS_CHARSET 128
#define HANGEUL_CHARSET 129
#define HANGUL_CHARSET 129
#define GB2312_CHARSET 134
#define CHINESEBIG5_CHARSET 136
#define OEM_CHARSET 255
#define JOHAB_CHARSET 130
#define HEBREW_CHARSET 177
#define ARABIC_CHARSET 178
#define GREEK_CHARSET 161
#define TURKISH_CHARSET 162
#define VIETNAMESE_CHARSET 163
#define THAI_CHARSET 222
#define EASTEUROPE_CHARSET 238
#define RUSSIAN_CHARSET 204
#define BALTIC_CHARSET 186

#endif

class CPWL_FontMap : public IFX_Edit_FontMap {
 public:
  CPWL_FontMap(IFX_SystemHandler* pSystemHandler);
  ~CPWL_FontMap() override;

  // IFX_Edit_FontMap
  CPDF_Font* GetPDFFont(int32_t nFontIndex) override;
  CFX_ByteString GetPDFFontAlias(int32_t nFontIndex) override;
  int32_t GetWordFontIndex(FX_WORD word,
                           int32_t nCharset,
                           int32_t nFontIndex) override;
  int32_t CharCodeFromUnicode(int32_t nFontIndex, FX_WORD word) override;
  int32_t CharSetFromUnicode(FX_WORD word, int32_t nOldCharset) override;

  void SetSystemHandler(IFX_SystemHandler* pSystemHandler);
  int32_t GetFontMapCount() const;
  const CPWL_FontMap_Data* GetFontMapData(int32_t nIndex) const;
  static int32_t GetNativeCharset();
  CFX_ByteString GetNativeFontName(int32_t nCharset);

  static CFX_ByteString GetDefaultFontByCharset(int32_t nCharset);

  CPDF_Font* AddFontToDocument(CPDF_Document* pDoc,
                               CFX_ByteString& sFontName,
                               uint8_t nCharset);
  static FX_BOOL IsStandardFont(const CFX_ByteString& sFontName);
  CPDF_Font* AddStandardFont(CPDF_Document* pDoc, CFX_ByteString& sFontName);
  CPDF_Font* AddSystemFont(CPDF_Document* pDoc,
                           CFX_ByteString& sFontName,
                           uint8_t nCharset);

 protected:
  void Initial(const FX_CHAR* fontname = NULL);
  CPDF_Document* GetDocument();
  CPDF_Font* FindFontSameCharset(CFX_ByteString& sFontAlias, int32_t nCharset);
  void AddedFont(CPDF_Font* pFont, const CFX_ByteString& sFontAlias);

  FX_BOOL KnowWord(int32_t nFontIndex, FX_WORD word);

  void Empty();
  int32_t GetFontIndex(const CFX_ByteString& sFontName,
                       int32_t nCharset,
                       FX_BOOL bFind);
  int32_t GetPWLFontIndex(FX_WORD word, int32_t nCharset);
  int32_t AddFontData(CPDF_Font* pFont,
                      const CFX_ByteString& sFontAlias,
                      int32_t nCharset = DEFAULT_CHARSET);

  CFX_ByteString EncodeFontAlias(const CFX_ByteString& sFontName,
                                 int32_t nCharset);
  CFX_ByteString EncodeFontAlias(const CFX_ByteString& sFontName);

 private:
  CFX_ByteString GetFontName(int32_t nFontIndex);
  int32_t FindFont(const CFX_ByteString& sFontName,
                   int32_t nCharset = DEFAULT_CHARSET);

  CFX_ByteString GetNativeFont(int32_t nCharset);

 public:
  using CharsetFontMap = FPDF_CharsetFontMap;
  static const CharsetFontMap defaultTTFMap[];

 protected:
  CFX_ArrayTemplate<CPWL_FontMap_Data*> m_aData;
  CFX_ArrayTemplate<CPWL_FontMap_Native*> m_aNativeFont;

 private:
  CPDF_Document* m_pPDFDoc;
  IFX_SystemHandler* m_pSystemHandler;
};

class CPWL_DocFontMap : public CPWL_FontMap {
 public:
  CPWL_DocFontMap(IFX_SystemHandler* pSystemHandler,
                  CPDF_Document* pAttachedDoc);
  ~CPWL_DocFontMap() override;

  virtual CPDF_Document* GetDocument();

 private:
  CPDF_Document* m_pAttachedDoc;
};

#endif  // FPDFSDK_INCLUDE_PDFWINDOW_PWL_FONTMAP_H_
