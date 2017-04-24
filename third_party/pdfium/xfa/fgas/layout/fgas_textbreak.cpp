// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fgas/layout/fgas_textbreak.h"

#include <algorithm>

#include "core/fxcrt/fx_arabic.h"
#include "core/fxcrt/fx_arb.h"
#include "core/fxcrt/fx_memory.h"
#include "third_party/base/ptr_util.h"
#include "xfa/fgas/font/cfgas_gefont.h"
#include "xfa/fgas/layout/fgas_linebreak.h"

namespace {

typedef uint32_t (CFX_TxtBreak::*FX_TxtBreak_LPFAppendChar)(
    CFX_TxtChar* pCurChar,
    int32_t iRotation);
const FX_TxtBreak_LPFAppendChar g_FX_TxtBreak_lpfAppendChar[16] = {
    &CFX_TxtBreak::AppendChar_Others,      &CFX_TxtBreak::AppendChar_Tab,
    &CFX_TxtBreak::AppendChar_Others,      &CFX_TxtBreak::AppendChar_Control,
    &CFX_TxtBreak::AppendChar_Combination, &CFX_TxtBreak::AppendChar_Others,
    &CFX_TxtBreak::AppendChar_Others,      &CFX_TxtBreak::AppendChar_Arabic,
    &CFX_TxtBreak::AppendChar_Arabic,      &CFX_TxtBreak::AppendChar_Arabic,
    &CFX_TxtBreak::AppendChar_Arabic,      &CFX_TxtBreak::AppendChar_Arabic,
    &CFX_TxtBreak::AppendChar_Arabic,      &CFX_TxtBreak::AppendChar_Others,
    &CFX_TxtBreak::AppendChar_Others,      &CFX_TxtBreak::AppendChar_Others,
};

}  // namespace

CFX_TxtBreak::CFX_TxtBreak(uint32_t dwPolicies)
    : m_dwPolicies(dwPolicies),
      m_iLineWidth(2000000),
      m_dwLayoutStyles(0),
      m_bVertical(false),
      m_bArabicContext(false),
      m_bArabicShapes(false),
      m_bRTL(false),
      m_bSingleLine(false),
      m_bCombText(false),
      m_iArabicContext(1),
      m_iCurArabicContext(1),
      m_pFont(nullptr),
      m_iFontSize(240),
      m_bEquidistant(true),
      m_iTabWidth(720000),
      m_wDefChar(0xFEFF),
      m_wParagBreakChar(L'\n'),
      m_iDefChar(0),
      m_iLineRotation(0),
      m_iCharRotation(0),
      m_iRotation(0),
      m_iAlignment(FX_TXTLINEALIGNMENT_Left),
      m_dwContextCharStyles(0),
      m_iCombWidth(360000),
      m_pUserData(nullptr),
      m_eCharType(FX_CHARTYPE_Unknown),
      m_bArabicNumber(false),
      m_bArabicComma(false),
      m_pCurLine(nullptr),
      m_iReady(0),
      m_iTolerance(0),
      m_iHorScale(100),
      m_iCharSpace(0) {
  m_bPagination = (m_dwPolicies & FX_TXTBREAKPOLICY_Pagination) != 0;
  int32_t iSize = m_bPagination ? sizeof(CFX_Char) : sizeof(CFX_TxtChar);
  m_pTxtLine1 = pdfium::MakeUnique<CFX_TxtLine>(iSize);
  m_pTxtLine2 = pdfium::MakeUnique<CFX_TxtLine>(iSize);
  m_pCurLine = m_pTxtLine1.get();
  ResetArabicContext();
}

CFX_TxtBreak::~CFX_TxtBreak() {
  Reset();
}

void CFX_TxtBreak::SetLineWidth(FX_FLOAT fLineWidth) {
  m_iLineWidth = FXSYS_round(fLineWidth * 20000.0f);
  ASSERT(m_iLineWidth >= 20000);
}

void CFX_TxtBreak::SetLinePos(FX_FLOAT fLinePos) {
  int32_t iLinePos =
      std::min(std::max(FXSYS_round(fLinePos * 20000.0f), 0), m_iLineWidth);
  m_pCurLine->m_iStart = iLinePos;
  m_pCurLine->m_iWidth += iLinePos;
}

void CFX_TxtBreak::SetLayoutStyles(uint32_t dwLayoutStyles) {
  m_dwLayoutStyles = dwLayoutStyles;
  m_bVertical = (m_dwLayoutStyles & FX_TXTLAYOUTSTYLE_VerticalChars) != 0;
  m_bArabicContext = (m_dwLayoutStyles & FX_TXTLAYOUTSTYLE_ArabicContext) != 0;
  m_bArabicShapes = (m_dwLayoutStyles & FX_TXTLAYOUTSTYLE_ArabicShapes) != 0;
  m_bRTL = (m_dwLayoutStyles & FX_TXTLAYOUTSTYLE_RTLReadingOrder) != 0;
  m_bSingleLine = (m_dwLayoutStyles & FX_TXTLAYOUTSTYLE_SingleLine) != 0;
  m_bCombText = (m_dwLayoutStyles & FX_TXTLAYOUTSTYLE_CombText) != 0;
  ResetArabicContext();
  m_iLineRotation = GetLineRotation(m_dwLayoutStyles);
  m_iRotation = m_iLineRotation + m_iCharRotation;
  m_iRotation %= 4;
}

void CFX_TxtBreak::SetFont(const CFX_RetainPtr<CFGAS_GEFont>& pFont) {
  if (!pFont || pFont == m_pFont)
    return;

  SetBreakStatus();
  m_pFont = pFont;
  FontChanged();
}

void CFX_TxtBreak::SetFontSize(FX_FLOAT fFontSize) {
  int32_t iFontSize = FXSYS_round(fFontSize * 20.0f);
  if (m_iFontSize == iFontSize)
    return;

  SetBreakStatus();
  m_iFontSize = iFontSize;
  FontChanged();
}

void CFX_TxtBreak::FontChanged() {
  m_iDefChar = 0;
  if (m_wDefChar == 0xFEFF || !m_pFont)
    return;

  m_pFont->GetCharWidth(m_wDefChar, m_iDefChar, false);
  m_iDefChar *= m_iFontSize;
}

void CFX_TxtBreak::SetTabWidth(FX_FLOAT fTabWidth, bool bEquidistant) {
  m_iTabWidth = FXSYS_round(fTabWidth * 20000.0f);
  if (m_iTabWidth < FX_TXTBREAK_MinimumTabWidth)
    m_iTabWidth = FX_TXTBREAK_MinimumTabWidth;

  m_bEquidistant = bEquidistant;
}

void CFX_TxtBreak::SetDefaultChar(FX_WCHAR wch) {
  m_wDefChar = wch;
  m_iDefChar = 0;
  if (m_wDefChar == 0xFEFF || !m_pFont)
    return;

  m_pFont->GetCharWidth(m_wDefChar, m_iDefChar, false);
  if (m_iDefChar < 0)
    m_iDefChar = 0;
  else
    m_iDefChar *= m_iFontSize;
}

void CFX_TxtBreak::SetParagraphBreakChar(FX_WCHAR wch) {
  if (wch != L'\r' && wch != L'\n')
    return;
  m_wParagBreakChar = wch;
}

void CFX_TxtBreak::SetLineBreakTolerance(FX_FLOAT fTolerance) {
  m_iTolerance = FXSYS_round(fTolerance * 20000.0f);
}

void CFX_TxtBreak::SetCharRotation(int32_t iCharRotation) {
  if (iCharRotation < 0)
    iCharRotation += (-iCharRotation / 4 + 1) * 4;
  else if (iCharRotation > 3)
    iCharRotation -= (iCharRotation / 4) * 4;

  if (m_iCharRotation == iCharRotation)
    return;

  SetBreakStatus();
  m_iCharRotation = iCharRotation;
  m_iRotation = m_iLineRotation + m_iCharRotation;
  m_iRotation %= 4;
}

void CFX_TxtBreak::SetAlignment(int32_t iAlignment) {
  ASSERT(iAlignment >= FX_TXTLINEALIGNMENT_Left &&
         iAlignment <= FX_TXTLINEALIGNMENT_Distributed);
  m_iAlignment = iAlignment;
  ResetArabicContext();
}

void CFX_TxtBreak::ResetContextCharStyles() {
  m_dwContextCharStyles = m_bArabicContext ? m_iCurAlignment : m_iAlignment;
  if (m_bArabicNumber)
    m_dwContextCharStyles |= FX_TXTCHARSTYLE_ArabicNumber;
  if (m_bArabicComma)
    m_dwContextCharStyles |= FX_TXTCHARSTYLE_ArabicComma;
  if ((m_bArabicContext && m_bCurRTL) || (!m_bArabicContext && m_bRTL))
    m_dwContextCharStyles |= FX_TXTCHARSTYLE_RTLReadingOrder;
  m_dwContextCharStyles |= (m_iArabicContext << 8);
}

void CFX_TxtBreak::SetCombWidth(FX_FLOAT fCombWidth) {
  m_iCombWidth = FXSYS_round(fCombWidth * 20000.0f);
}

void CFX_TxtBreak::SetUserData(void* pUserData) {
  if (m_pUserData == pUserData)
    return;

  SetBreakStatus();
  m_pUserData = pUserData;
}

void CFX_TxtBreak::SetBreakStatus() {
  if (m_bPagination)
    return;

  int32_t iCount = m_pCurLine->CountChars();
  if (iCount < 1)
    return;

  CFX_TxtChar* pTC = m_pCurLine->GetCharPtr(iCount - 1);
  if (pTC->m_dwStatus == 0)
    pTC->m_dwStatus = FX_TXTBREAK_PieceBreak;
}

void CFX_TxtBreak::SetHorizontalScale(int32_t iScale) {
  if (iScale < 0)
    iScale = 0;
  if (iScale == m_iHorScale)
    return;

  SetBreakStatus();
  m_iHorScale = iScale;
}

void CFX_TxtBreak::SetCharSpace(FX_FLOAT fCharSpace) {
  m_iCharSpace = FXSYS_round(fCharSpace * 20000.0f);
}

static const int32_t gs_FX_TxtLineRotations[8] = {0, 3, 1, 0, 2, 1, 3, 2};

int32_t CFX_TxtBreak::GetLineRotation(uint32_t dwStyles) const {
  return gs_FX_TxtLineRotations[(dwStyles & 0x0E) >> 1];
}

CFX_TxtChar* CFX_TxtBreak::GetLastChar(int32_t index, bool bOmitChar) const {
  std::vector<CFX_TxtChar>& ca = *m_pCurLine->m_pLineChars.get();
  int32_t iCount = pdfium::CollectionSize<int32_t>(ca);
  if (index < 0 || index >= iCount)
    return nullptr;

  int32_t iStart = iCount - 1;
  while (iStart > -1) {
    CFX_TxtChar* pTC = &ca[iStart--];
    if (bOmitChar && pTC->GetCharType() == FX_CHARTYPE_Combination)
      continue;
    if (--index < 0)
      return pTC;
  }
  return nullptr;
}

CFX_TxtLine* CFX_TxtBreak::GetTxtLine() const {
  if (m_iReady == 1)
    return m_pTxtLine1.get();
  if (m_iReady == 2)
    return m_pTxtLine2.get();
  return nullptr;
}

CFX_TxtPieceArray* CFX_TxtBreak::GetTxtPieces() const {
  CFX_TxtLine* pTxtLine = GetTxtLine();
  return pTxtLine ? pTxtLine->m_pLinePieces.get() : nullptr;
}

inline FX_CHARTYPE CFX_TxtBreak::GetUnifiedCharType(
    FX_CHARTYPE chartype) const {
  return chartype >= FX_CHARTYPE_ArabicAlef ? FX_CHARTYPE_Arabic : chartype;
}

void CFX_TxtBreak::ResetArabicContext() {
  if (m_bArabicContext) {
    m_bCurRTL = m_iCurArabicContext > 1;
    m_iCurAlignment = m_iCurArabicContext > 1 ? FX_TXTLINEALIGNMENT_Right
                                              : FX_TXTLINEALIGNMENT_Left;
    m_iCurAlignment |= (m_iAlignment & FX_TXTLINEALIGNMENT_HigherMask);
    m_bArabicNumber = m_iArabicContext >= 1 && m_bArabicShapes;
  } else {
    if (m_bPagination) {
      m_bCurRTL = false;
      m_iCurAlignment = 0;
    } else {
      m_bCurRTL = m_bRTL;
      m_iCurAlignment = m_iAlignment;
    }
    if (m_bRTL)
      m_bArabicNumber = m_iArabicContext >= 1;
    else
      m_bArabicNumber = m_iArabicContext > 1;
    m_bArabicNumber = m_bArabicNumber && m_bArabicShapes;
  }
  m_bArabicComma = m_bArabicNumber;
  ResetContextCharStyles();
}

void CFX_TxtBreak::AppendChar_PageLoad(CFX_TxtChar* pCurChar,
                                       uint32_t dwProps) {
  if (!m_bPagination) {
    pCurChar->m_dwStatus = 0;
    pCurChar->m_pUserData = m_pUserData;
  }
  if (m_bArabicContext || m_bArabicShapes) {
    int32_t iBidiCls = (dwProps & FX_BIDICLASSBITSMASK) >> FX_BIDICLASSBITS;
    int32_t iArabicContext =
        (iBidiCls == FX_BIDICLASS_R || iBidiCls == FX_BIDICLASS_AL)
            ? 2
            : ((iBidiCls == FX_BIDICLASS_L || iBidiCls == FX_BIDICLASS_S) ? 0
                                                                          : 1);
    if (iArabicContext != m_iArabicContext && iArabicContext != 1) {
      m_iArabicContext = iArabicContext;
      if (m_iCurArabicContext == 1)
        m_iCurArabicContext = iArabicContext;

      ResetArabicContext();
      if (!m_bPagination) {
        CFX_TxtChar* pLastChar = GetLastChar(1, false);
        if (pLastChar && pLastChar->m_dwStatus < 1)
          pLastChar->m_dwStatus = FX_TXTBREAK_PieceBreak;
      }
    }
  }
  pCurChar->m_dwCharStyles = m_dwContextCharStyles;
}

uint32_t CFX_TxtBreak::AppendChar_Combination(CFX_TxtChar* pCurChar,
                                              int32_t iRotation) {
  FX_WCHAR wch = pCurChar->m_wCharCode;
  FX_WCHAR wForm;
  int32_t iCharWidth = 0;
  pCurChar->m_iCharWidth = -1;
  if (m_bCombText) {
    iCharWidth = m_iCombWidth;
  } else {
    if (m_bVertical != FX_IsOdd(iRotation)) {
      iCharWidth = 1000;
    } else {
      wForm = wch;
      if (!m_bPagination) {
        CFX_TxtChar* pLastChar = GetLastChar(0, false);
        if (pLastChar &&
            (pLastChar->m_dwCharStyles & FX_TXTCHARSTYLE_ArabicShadda) == 0) {
          bool bShadda = false;
          if (wch == 0x0651) {
            FX_WCHAR wLast = pLastChar->m_wCharCode;
            if (wLast >= 0x064C && wLast <= 0x0650) {
              wForm = FX_GetArabicFromShaddaTable(wLast);
              bShadda = true;
            }
          } else if (wch >= 0x064C && wch <= 0x0650) {
            if (pLastChar->m_wCharCode == 0x0651) {
              wForm = FX_GetArabicFromShaddaTable(wch);
              bShadda = true;
            }
          }
          if (bShadda) {
            pLastChar->m_dwCharStyles |= FX_TXTCHARSTYLE_ArabicShadda;
            pLastChar->m_iCharWidth = 0;
            pCurChar->m_dwCharStyles |= FX_TXTCHARSTYLE_ArabicShadda;
          }
        }
      }
      if (!m_pFont->GetCharWidth(wForm, iCharWidth, false))
        iCharWidth = 0;
    }
    iCharWidth *= m_iFontSize;
    iCharWidth = iCharWidth * m_iHorScale / 100;
  }
  pCurChar->m_iCharWidth = -iCharWidth;
  return FX_TXTBREAK_None;
}

uint32_t CFX_TxtBreak::AppendChar_Tab(CFX_TxtChar* pCurChar,
                                      int32_t iRotation) {
  m_eCharType = FX_CHARTYPE_Tab;
  if ((m_dwLayoutStyles & FX_TXTLAYOUTSTYLE_ExpandTab) == 0)
    return FX_TXTBREAK_None;

  int32_t& iLineWidth = m_pCurLine->m_iWidth;
  int32_t iCharWidth;
  if (m_bCombText) {
    iCharWidth = m_iCombWidth;
  } else {
    if (m_bEquidistant) {
      iCharWidth = iLineWidth;
      iCharWidth = m_iTabWidth * (iCharWidth / m_iTabWidth + 1) - iCharWidth;
      if (iCharWidth < FX_TXTBREAK_MinimumTabWidth)
        iCharWidth += m_iTabWidth;
    } else {
      iCharWidth = m_iTabWidth;
    }
  }

  pCurChar->m_iCharWidth = iCharWidth;
  iLineWidth += iCharWidth;
  if (!m_bSingleLine && iLineWidth >= m_iLineWidth + m_iTolerance)
    return EndBreak(FX_TXTBREAK_LineBreak);

  return FX_TXTBREAK_None;
}

uint32_t CFX_TxtBreak::AppendChar_Control(CFX_TxtChar* pCurChar,
                                          int32_t iRotation) {
  m_eCharType = FX_CHARTYPE_Control;
  uint32_t dwRet = FX_TXTBREAK_None;
  if (!m_bSingleLine) {
    FX_WCHAR wch = pCurChar->m_wCharCode;
    switch (wch) {
      case L'\v':
      case 0x2028:
        dwRet = FX_TXTBREAK_LineBreak;
        break;
      case L'\f':
        dwRet = FX_TXTBREAK_PageBreak;
        break;
      case 0x2029:
        dwRet = FX_TXTBREAK_ParagraphBreak;
        break;
      default:
        if (wch == m_wParagBreakChar)
          dwRet = FX_TXTBREAK_ParagraphBreak;
        break;
    }
    if (dwRet != FX_TXTBREAK_None)
      dwRet = EndBreak(dwRet);
  }
  return dwRet;
}

uint32_t CFX_TxtBreak::AppendChar_Arabic(CFX_TxtChar* pCurChar,
                                         int32_t iRotation) {
  FX_CHARTYPE chartype = pCurChar->GetCharType();
  int32_t& iLineWidth = m_pCurLine->m_iWidth;
  FX_WCHAR wForm;
  int32_t iCharWidth = 0;
  CFX_TxtChar* pLastChar = nullptr;
  bool bAlef = false;
  if (!m_bCombText && m_eCharType >= FX_CHARTYPE_ArabicAlef &&
      m_eCharType <= FX_CHARTYPE_ArabicDistortion) {
    pLastChar = GetLastChar(1);
    if (pLastChar) {
      iCharWidth = pLastChar->m_iCharWidth;
      if (iCharWidth > 0)
        iLineWidth -= iCharWidth;

      CFX_Char* pPrevChar = GetLastChar(2);
      wForm = pdfium::arabic::GetFormChar(pLastChar, pPrevChar, pCurChar);
      bAlef = (wForm == 0xFEFF &&
               pLastChar->GetCharType() == FX_CHARTYPE_ArabicAlef);
      int32_t iLastRotation = pLastChar->m_nRotation + m_iLineRotation;
      if (m_bVertical && (pLastChar->m_dwCharProps & 0x8000) != 0)
        iLastRotation++;
      if (m_bVertical != FX_IsOdd(iLastRotation))
        iCharWidth = 1000;
      else
        m_pFont->GetCharWidth(wForm, iCharWidth, false);

      if (wForm == 0xFEFF)
        iCharWidth = m_iDefChar;

      iCharWidth *= m_iFontSize;
      iCharWidth = iCharWidth * m_iHorScale / 100;
      pLastChar->m_iCharWidth = iCharWidth;
      iLineWidth += iCharWidth;
      iCharWidth = 0;
    }
  }

  m_eCharType = chartype;
  wForm = pdfium::arabic::GetFormChar(pCurChar, bAlef ? nullptr : pLastChar,
                                      nullptr);
  if (m_bCombText) {
    iCharWidth = m_iCombWidth;
  } else {
    if (m_bVertical != FX_IsOdd(iRotation))
      iCharWidth = 1000;
    else
      m_pFont->GetCharWidth(wForm, iCharWidth, false);

    if (wForm == 0xFEFF)
      iCharWidth = m_iDefChar;

    iCharWidth *= m_iFontSize;
    iCharWidth = iCharWidth * m_iHorScale / 100;
  }
  pCurChar->m_iCharWidth = iCharWidth;
  iLineWidth += iCharWidth;
  m_pCurLine->m_iArabicChars++;
  if (!m_bSingleLine && iLineWidth > m_iLineWidth + m_iTolerance)
    return EndBreak(FX_TXTBREAK_LineBreak);
  return FX_TXTBREAK_None;
}

uint32_t CFX_TxtBreak::AppendChar_Others(CFX_TxtChar* pCurChar,
                                         int32_t iRotation) {
  uint32_t dwProps = pCurChar->m_dwCharProps;
  FX_CHARTYPE chartype = pCurChar->GetCharType();
  int32_t& iLineWidth = m_pCurLine->m_iWidth;
  int32_t iCharWidth = 0;
  m_eCharType = chartype;
  FX_WCHAR wch = pCurChar->m_wCharCode;
  FX_WCHAR wForm = wch;
  if (chartype == FX_CHARTYPE_Numeric) {
    if (m_bArabicNumber) {
      wForm = wch + 0x0630;
      pCurChar->m_dwCharStyles |= FX_TXTCHARSTYLE_ArabicIndic;
    }
  } else if (wch == L',') {
    if (m_bArabicShapes && m_iCurArabicContext > 0) {
      wForm = 0x060C;
      pCurChar->m_dwCharStyles |= FX_TXTCHARSTYLE_ArabicComma;
    }
  } else if (m_bCurRTL || m_bVertical) {
    wForm = FX_GetMirrorChar(wch, dwProps, m_bCurRTL, m_bVertical);
  }

  if (m_bCombText) {
    iCharWidth = m_iCombWidth;
  } else {
    if (m_bVertical != FX_IsOdd(iRotation))
      iCharWidth = 1000;
    else if (!m_pFont->GetCharWidth(wForm, iCharWidth, false))
      iCharWidth = m_iDefChar;

    iCharWidth *= m_iFontSize;
    iCharWidth = iCharWidth * m_iHorScale / 100;
  }

  iCharWidth += m_iCharSpace;
  pCurChar->m_iCharWidth = iCharWidth;
  iLineWidth += iCharWidth;
  bool bBreak = (chartype != FX_CHARTYPE_Space ||
                 (m_dwPolicies & FX_TXTBREAKPOLICY_SpaceBreak) != 0);
  if (!m_bSingleLine && bBreak && iLineWidth > m_iLineWidth + m_iTolerance)
    return EndBreak(FX_TXTBREAK_LineBreak);

  return FX_TXTBREAK_None;
}

uint32_t CFX_TxtBreak::AppendChar(FX_WCHAR wch) {
  uint32_t dwProps = kTextLayoutCodeProperties[static_cast<uint16_t>(wch)];
  FX_CHARTYPE chartype = GetCharTypeFromProp(dwProps);
  m_pCurLine->m_pLineChars->emplace_back();

  CFX_TxtChar* pCurChar = &m_pCurLine->m_pLineChars->back();
  pCurChar->m_wCharCode = static_cast<uint16_t>(wch);
  pCurChar->m_nRotation = m_iCharRotation;
  pCurChar->m_dwCharProps = dwProps;
  pCurChar->m_dwCharStyles = 0;
  pCurChar->m_iCharWidth = 0;
  pCurChar->m_iHorizontalScale = m_iHorScale;
  pCurChar->m_iVerticalScale = 100;
  pCurChar->m_dwStatus = 0;
  pCurChar->m_iBidiClass = 0;
  pCurChar->m_iBidiLevel = 0;
  pCurChar->m_iBidiPos = 0;
  pCurChar->m_iBidiOrder = 0;
  pCurChar->m_pUserData = nullptr;
  AppendChar_PageLoad(pCurChar, dwProps);
  uint32_t dwRet1 = FX_TXTBREAK_None;
  if (chartype != FX_CHARTYPE_Combination &&
      GetUnifiedCharType(m_eCharType) != GetUnifiedCharType(chartype) &&
      m_eCharType != FX_CHARTYPE_Unknown &&
      m_pCurLine->m_iWidth > m_iLineWidth + m_iTolerance && !m_bSingleLine &&
      (m_eCharType != FX_CHARTYPE_Space || chartype != FX_CHARTYPE_Control)) {
    dwRet1 = EndBreak(FX_TXTBREAK_LineBreak);
    int32_t iCount = m_pCurLine->CountChars();
    if (iCount > 0)
      pCurChar = &(*m_pCurLine->m_pLineChars)[iCount - 1];
  }

  int32_t iRotation = m_iRotation;
  if (m_bVertical && (dwProps & 0x8000) != 0)
    iRotation = (iRotation + 1) % 4;

  uint32_t dwRet2 =
      (this->*g_FX_TxtBreak_lpfAppendChar[chartype >> FX_CHARTYPEBITS])(
          pCurChar, iRotation);
  return std::max(dwRet1, dwRet2);
}

void CFX_TxtBreak::EndBreak_UpdateArabicShapes() {
  ASSERT(m_bArabicShapes);
  int32_t iCount = m_pCurLine->CountChars();
  if (iCount < 2)
    return;

  int32_t& iLineWidth = m_pCurLine->m_iWidth;
  CFX_TxtChar* pCur = m_pCurLine->GetCharPtr(0);
  bool bPrevNum = (pCur->m_dwCharStyles & FX_TXTCHARSTYLE_ArabicIndic) != 0;
  pCur = m_pCurLine->GetCharPtr(1);
  FX_WCHAR wch, wForm;
  bool bNextNum;
  int32_t i = 1;
  int32_t iCharWidth;
  int32_t iRotation;
  CFX_TxtChar* pNext;
  do {
    i++;
    if (i < iCount) {
      pNext = m_pCurLine->GetCharPtr(i);
      bNextNum = (pNext->m_dwCharStyles & FX_TXTCHARSTYLE_ArabicIndic) != 0;
    } else {
      pNext = nullptr;
      bNextNum = false;
    }

    wch = pCur->m_wCharCode;
    if (wch == L'.') {
      if (bPrevNum && bNextNum) {
        iRotation = m_iRotation;
        if (m_bVertical && (pCur->m_dwCharProps & 0x8000) != 0)
          iRotation = ((iRotation + 1) & 0x03);

        wForm = wch == L'.' ? 0x066B : 0x066C;
        iLineWidth -= pCur->m_iCharWidth;
        if (m_bCombText) {
          iCharWidth = m_iCombWidth;
        } else {
          if (m_bVertical != FX_IsOdd(iRotation))
            iCharWidth = 1000;
          else if (!m_pFont->GetCharWidth(wForm, iCharWidth, false))
            iCharWidth = m_iDefChar;

          iCharWidth *= m_iFontSize;
          iCharWidth = iCharWidth * m_iHorScale / 100;
        }
        pCur->m_iCharWidth = iCharWidth;
        iLineWidth += iCharWidth;
      }
    }
    bPrevNum = (pCur->m_dwCharStyles & FX_TXTCHARSTYLE_ArabicIndic) != 0;
    pCur = pNext;
  } while (i < iCount);
}

bool CFX_TxtBreak::EndBreak_SplitLine(CFX_TxtLine* pNextLine,
                                      bool bAllChars,
                                      uint32_t dwStatus) {
  int32_t iCount = m_pCurLine->CountChars();
  bool bDone = false;
  CFX_TxtChar* pTC;
  if (!m_bSingleLine && m_pCurLine->m_iWidth > m_iLineWidth + m_iTolerance) {
    pTC = m_pCurLine->GetCharPtr(iCount - 1);
    switch (pTC->GetCharType()) {
      case FX_CHARTYPE_Tab:
      case FX_CHARTYPE_Control:
        break;
      case FX_CHARTYPE_Space:
        if ((m_dwPolicies & FX_TXTBREAKPOLICY_SpaceBreak) != 0) {
          SplitTextLine(m_pCurLine, pNextLine, !m_bPagination && bAllChars);
          bDone = true;
        }
        break;
      default:
        SplitTextLine(m_pCurLine, pNextLine, !m_bPagination && bAllChars);
        bDone = true;
        break;
    }
  }

  iCount = m_pCurLine->CountChars();
  CFX_TxtPieceArray* pCurPieces = m_pCurLine->m_pLinePieces.get();
  CFX_TxtPiece tp;
  if (m_bPagination) {
    tp.m_dwStatus = dwStatus;
    tp.m_iStartPos = m_pCurLine->m_iStart;
    tp.m_iWidth = m_pCurLine->m_iWidth;
    tp.m_iStartChar = 0;
    tp.m_iChars = iCount;
    tp.m_pChars = m_pCurLine->m_pLineChars.get();
    tp.m_pUserData = m_pUserData;
    pTC = m_pCurLine->GetCharPtr(0);
    tp.m_dwCharStyles = pTC->m_dwCharStyles;
    tp.m_iHorizontalScale = pTC->m_iHorizontalScale;
    tp.m_iVerticalScale = pTC->m_iVerticalScale;
    pCurPieces->Add(tp);
    m_pCurLine = pNextLine;
    m_eCharType = FX_CHARTYPE_Unknown;
    return true;
  }
  if (bAllChars && !bDone) {
    int32_t iEndPos = m_pCurLine->m_iWidth;
    GetBreakPos(*m_pCurLine->m_pLineChars.get(), iEndPos, bAllChars, true);
  }
  return false;
}

void CFX_TxtBreak::EndBreak_BidiLine(std::deque<FX_TPO>* tpos,
                                     uint32_t dwStatus) {
  CFX_TxtPiece tp;
  FX_TPO tpo;
  CFX_TxtChar* pTC;
  int32_t i;
  int32_t j;
  std::vector<CFX_TxtChar>& chars = *m_pCurLine->m_pLineChars.get();
  int32_t iCount = m_pCurLine->CountChars();
  bool bDone = (m_pCurLine->m_iArabicChars > 0 || m_bCurRTL);
  if (!m_bPagination && bDone) {
    int32_t iBidiNum = 0;
    for (i = 0; i < iCount; i++) {
      pTC = &chars[i];
      pTC->m_iBidiPos = i;
      if (pTC->GetCharType() != FX_CHARTYPE_Control)
        iBidiNum = i;
      if (i == 0)
        pTC->m_iBidiLevel = 1;
    }
    FX_BidiLine(chars, iBidiNum + 1, m_bCurRTL ? 1 : 0);
  }

  CFX_TxtPieceArray* pCurPieces = m_pCurLine->m_pLinePieces.get();
  if (!m_bPagination &&
      (bDone || (m_dwLayoutStyles & FX_TXTLAYOUTSTYLE_MutipleFormat) != 0)) {
    tp.m_dwStatus = FX_TXTBREAK_PieceBreak;
    tp.m_iStartPos = m_pCurLine->m_iStart;
    tp.m_pChars = m_pCurLine->m_pLineChars.get();
    int32_t iBidiLevel = -1;
    int32_t iCharWidth;
    i = 0;
    j = -1;
    while (i < iCount) {
      pTC = &chars[i];
      if (iBidiLevel < 0) {
        iBidiLevel = pTC->m_iBidiLevel;
        tp.m_iWidth = 0;
        tp.m_iBidiLevel = iBidiLevel;
        tp.m_iBidiPos = pTC->m_iBidiOrder;
        tp.m_dwCharStyles = pTC->m_dwCharStyles;
        tp.m_pUserData = pTC->m_pUserData;
        tp.m_iHorizontalScale = pTC->m_iHorizontalScale;
        tp.m_iVerticalScale = pTC->m_iVerticalScale;
        tp.m_dwStatus = FX_TXTBREAK_PieceBreak;
      }
      if (iBidiLevel != pTC->m_iBidiLevel || pTC->m_dwStatus != 0) {
        if (iBidiLevel == pTC->m_iBidiLevel) {
          tp.m_dwStatus = pTC->m_dwStatus;
          iCharWidth = pTC->m_iCharWidth;
          if (iCharWidth > 0)
            tp.m_iWidth += iCharWidth;

          i++;
        }
        tp.m_iChars = i - tp.m_iStartChar;
        pCurPieces->Add(tp);
        tp.m_iStartPos += tp.m_iWidth;
        tp.m_iStartChar = i;
        tpo.index = ++j;
        tpo.pos = tp.m_iBidiPos;
        tpos->push_back(tpo);
        iBidiLevel = -1;
      } else {
        iCharWidth = pTC->m_iCharWidth;
        if (iCharWidth > 0)
          tp.m_iWidth += iCharWidth;

        i++;
      }
    }
    if (i > tp.m_iStartChar) {
      tp.m_dwStatus = dwStatus;
      tp.m_iChars = i - tp.m_iStartChar;
      pCurPieces->Add(tp);
      tpo.index = ++j;
      tpo.pos = tp.m_iBidiPos;
      tpos->push_back(tpo);
    }
    if (j > -1) {
      if (j > 0) {
        std::sort(tpos->begin(), tpos->end());
        int32_t iStartPos = 0;
        for (i = 0; i <= j; i++) {
          tpo = (*tpos)[i];
          CFX_TxtPiece& ttp = pCurPieces->GetAt(tpo.index);
          ttp.m_iStartPos = iStartPos;
          iStartPos += ttp.m_iWidth;
        }
      }
      CFX_TxtPiece& ttp = pCurPieces->GetAt(j);
      ttp.m_dwStatus = dwStatus;
    }
  } else {
    tp.m_dwStatus = dwStatus;
    tp.m_iStartPos = m_pCurLine->m_iStart;
    tp.m_iWidth = m_pCurLine->m_iWidth;
    tp.m_iStartChar = 0;
    tp.m_iChars = iCount;
    tp.m_pChars = m_pCurLine->m_pLineChars.get();
    tp.m_pUserData = m_pUserData;
    pTC = &chars[0];
    tp.m_dwCharStyles = pTC->m_dwCharStyles;
    tp.m_iHorizontalScale = pTC->m_iHorizontalScale;
    tp.m_iVerticalScale = pTC->m_iVerticalScale;
    pCurPieces->Add(tp);
    tpos->push_back({0, 0});
  }
}

void CFX_TxtBreak::EndBreak_Alignment(const std::deque<FX_TPO>& tpos,
                                      bool bAllChars,
                                      uint32_t dwStatus) {
  int32_t iNetWidth = m_pCurLine->m_iWidth;
  int32_t iGapChars = 0;
  int32_t iCharWidth;
  CFX_TxtPieceArray* pCurPieces = m_pCurLine->m_pLinePieces.get();
  int32_t i;
  int32_t j;
  int32_t iCount = pCurPieces->GetSize();
  bool bFind = false;
  FX_TPO tpo;
  CFX_TxtChar* pTC;
  FX_CHARTYPE chartype;
  for (i = iCount - 1; i > -1; i--) {
    tpo = tpos[i];
    CFX_TxtPiece& ttp = pCurPieces->GetAt(tpo.index);
    if (!bFind)
      iNetWidth = ttp.GetEndPos();

    bool bArabic = FX_IsOdd(ttp.m_iBidiLevel);
    j = bArabic ? 0 : ttp.m_iChars - 1;
    while (j > -1 && j < ttp.m_iChars) {
      pTC = ttp.GetCharPtr(j);
      if (pTC->m_nBreakType == FX_LBT_DIRECT_BRK)
        iGapChars++;
      if (!bFind || !bAllChars) {
        chartype = pTC->GetCharType();
        if (chartype == FX_CHARTYPE_Space || chartype == FX_CHARTYPE_Control) {
          if (!bFind) {
            iCharWidth = pTC->m_iCharWidth;
            if (bAllChars && iCharWidth > 0)
              iNetWidth -= iCharWidth;
          }
        } else {
          bFind = true;
          if (!bAllChars)
            break;
        }
      }
      j += bArabic ? 1 : -1;
    }
    if (!bAllChars && bFind)
      break;
  }

  int32_t iOffset = m_iLineWidth - iNetWidth;
  int32_t iLowerAlignment = (m_iCurAlignment & FX_TXTLINEALIGNMENT_LowerMask);
  int32_t iHigherAlignment = (m_iCurAlignment & FX_TXTLINEALIGNMENT_HigherMask);
  if (iGapChars > 0 && (iHigherAlignment == FX_TXTLINEALIGNMENT_Distributed ||
                        (iHigherAlignment == FX_TXTLINEALIGNMENT_Justified &&
                         dwStatus != FX_TXTBREAK_ParagraphBreak))) {
    int32_t iStart = -1;
    for (i = 0; i < iCount; i++) {
      tpo = tpos[i];
      CFX_TxtPiece& ttp = pCurPieces->GetAt(tpo.index);
      if (iStart < -1)
        iStart = ttp.m_iStartPos;
      else
        ttp.m_iStartPos = iStart;

      for (j = 0; j < ttp.m_iChars; j++) {
        pTC = ttp.GetCharPtr(j);
        if (pTC->m_nBreakType != FX_LBT_DIRECT_BRK || pTC->m_iCharWidth < 0)
          continue;

        int32_t k = iOffset / iGapChars;
        pTC->m_iCharWidth += k;
        ttp.m_iWidth += k;
        iOffset -= k;
        iGapChars--;
        if (iGapChars < 1)
          break;
      }
      iStart += ttp.m_iWidth;
    }
  } else if (iLowerAlignment > FX_TXTLINEALIGNMENT_Left) {
    if (iLowerAlignment == FX_TXTLINEALIGNMENT_Center)
      iOffset /= 2;
    if (iOffset > 0) {
      for (i = 0; i < iCount; i++) {
        CFX_TxtPiece& ttp = pCurPieces->GetAt(i);
        ttp.m_iStartPos += iOffset;
      }
    }
  }
}

uint32_t CFX_TxtBreak::EndBreak(uint32_t dwStatus) {
  ASSERT(dwStatus >= FX_TXTBREAK_PieceBreak &&
         dwStatus <= FX_TXTBREAK_PageBreak);
  CFX_TxtPieceArray* pCurPieces = m_pCurLine->m_pLinePieces.get();
  int32_t iCount = pCurPieces->GetSize();
  if (iCount > 0) {
    CFX_TxtPiece* pLastPiece = pCurPieces->GetPtrAt(--iCount);
    if (dwStatus > FX_TXTBREAK_PieceBreak)
      pLastPiece->m_dwStatus = dwStatus;
    else
      dwStatus = pLastPiece->m_dwStatus;
    return dwStatus;
  } else {
    CFX_TxtLine* pLastLine = GetTxtLine();
    if (pLastLine) {
      pCurPieces = pLastLine->m_pLinePieces.get();
      iCount = pCurPieces->GetSize();
      if (iCount-- > 0) {
        CFX_TxtPiece* pLastPiece = pCurPieces->GetPtrAt(iCount);
        if (dwStatus > FX_TXTBREAK_PieceBreak)
          pLastPiece->m_dwStatus = dwStatus;
        else
          dwStatus = pLastPiece->m_dwStatus;
        return dwStatus;
      }
      return FX_TXTBREAK_None;
    }

    iCount = m_pCurLine->CountChars();
    if (iCount < 1)
      return FX_TXTBREAK_None;
    if (!m_bPagination) {
      CFX_TxtChar* pTC = m_pCurLine->GetCharPtr(iCount - 1);
      pTC->m_dwStatus = dwStatus;
    }
    if (dwStatus <= FX_TXTBREAK_PieceBreak)
      return dwStatus;
  }

  m_iReady = (m_pCurLine == m_pTxtLine1.get()) ? 1 : 2;
  CFX_TxtLine* pNextLine =
      (m_pCurLine == m_pTxtLine1.get()) ? m_pTxtLine2.get() : m_pTxtLine1.get();
  bool bAllChars = (m_iCurAlignment > FX_TXTLINEALIGNMENT_Right);
  if (m_bArabicShapes)
    EndBreak_UpdateArabicShapes();

  if (!EndBreak_SplitLine(pNextLine, bAllChars, dwStatus)) {
    std::deque<FX_TPO> tpos;
    EndBreak_BidiLine(&tpos, dwStatus);
    if (!m_bPagination && m_iCurAlignment > FX_TXTLINEALIGNMENT_Left)
      EndBreak_Alignment(tpos, bAllChars, dwStatus);
  }

  m_pCurLine = pNextLine;
  CFX_Char* pTC = GetLastChar(0, false);
  m_eCharType = pTC ? pTC->GetCharType() : FX_CHARTYPE_Unknown;
  if (dwStatus == FX_TXTBREAK_ParagraphBreak) {
    m_iArabicContext = m_iCurArabicContext = 1;
    ResetArabicContext();
  }
  return dwStatus;
}

int32_t CFX_TxtBreak::GetBreakPos(std::vector<CFX_TxtChar>& ca,
                                  int32_t& iEndPos,
                                  bool bAllChars,
                                  bool bOnlyBrk) {
  int32_t iLength = pdfium::CollectionSize<int32_t>(ca) - 1;
  if (iLength < 1)
    return iLength;

  int32_t iBreak = -1;
  int32_t iBreakPos = -1;
  int32_t iIndirect = -1;
  int32_t iIndirectPos = -1;
  int32_t iLast = -1;
  int32_t iLastPos = -1;
  if (m_bSingleLine || iEndPos <= m_iLineWidth) {
    if (!bAllChars)
      return iLength;

    iBreak = iLength;
    iBreakPos = iEndPos;
  }

  bool bSpaceBreak = (m_dwPolicies & FX_TXTBREAKPOLICY_SpaceBreak) != 0;
  bool bNumberBreak = (m_dwPolicies & FX_TXTBREAKPOLICY_NumberBreak) != 0;
  FX_LINEBREAKTYPE eType;
  uint32_t nCodeProp;
  uint32_t nCur;
  uint32_t nNext;
  CFX_Char* pCur = &ca[iLength--];
  if (bAllChars)
    pCur->m_nBreakType = FX_LBT_UNKNOWN;

  nCodeProp = pCur->m_dwCharProps;
  nNext = nCodeProp & 0x003F;
  int32_t iCharWidth = pCur->m_iCharWidth;
  if (iCharWidth > 0)
    iEndPos -= iCharWidth;

  while (iLength >= 0) {
    pCur = &ca[iLength];
    nCodeProp = pCur->m_dwCharProps;
    nCur = nCodeProp & 0x003F;
    if (nCur == FX_CBP_SP) {
      if (nNext == FX_CBP_SP)
        eType = bSpaceBreak ? FX_LBT_DIRECT_BRK : FX_LBT_PROHIBITED_BRK;
      else
        eType = gs_FX_LineBreak_PairTable[nCur][nNext];
    } else if (bNumberBreak && nCur == FX_CBP_NU && nNext == FX_CBP_NU) {
      eType = FX_LBT_DIRECT_BRK;
    } else {
      if (nNext == FX_CBP_SP)
        eType = FX_LBT_PROHIBITED_BRK;
      else
        eType = gs_FX_LineBreak_PairTable[nCur][nNext];
    }
    if (bAllChars)
      pCur->m_nBreakType = static_cast<uint8_t>(eType);
    if (!bOnlyBrk) {
      if (m_bSingleLine || iEndPos <= m_iLineWidth ||
          (nCur == FX_CBP_SP && !bSpaceBreak)) {
        if (eType == FX_LBT_DIRECT_BRK && iBreak < 0) {
          iBreak = iLength;
          iBreakPos = iEndPos;
          if (!bAllChars)
            return iLength;
        } else if (eType == FX_LBT_INDIRECT_BRK && iIndirect < 0) {
          iIndirect = iLength;
          iIndirectPos = iEndPos;
        }
        if (iLast < 0) {
          iLast = iLength;
          iLastPos = iEndPos;
        }
      }
      iCharWidth = pCur->m_iCharWidth;
      if (iCharWidth > 0)
        iEndPos -= iCharWidth;
    }
    nNext = nCodeProp & 0x003F;
    iLength--;
  }
  if (bOnlyBrk)
    return 0;
  if (iBreak > -1) {
    iEndPos = iBreakPos;
    return iBreak;
  }
  if (iIndirect > -1) {
    iEndPos = iIndirectPos;
    return iIndirect;
  }
  if (iLast > -1) {
    iEndPos = iLastPos;
    return iLast;
  }
  return 0;
}

void CFX_TxtBreak::SplitTextLine(CFX_TxtLine* pCurLine,
                                 CFX_TxtLine* pNextLine,
                                 bool bAllChars) {
  ASSERT(pCurLine && pNextLine);
  int32_t iCount = pCurLine->CountChars();
  if (iCount < 2)
    return;

  int32_t iEndPos = pCurLine->m_iWidth;
  std::vector<CFX_TxtChar>& curChars = *pCurLine->m_pLineChars;
  int32_t iCharPos = GetBreakPos(curChars, iEndPos, bAllChars, false);
  if (iCharPos < 0)
    iCharPos = 0;

  iCharPos++;
  if (iCharPos >= iCount) {
    pNextLine->RemoveAll(true);
    CFX_Char* pTC = &curChars[iCharPos - 1];
    pTC->m_nBreakType = FX_LBT_UNKNOWN;
    return;
  }

  // m_pLineChars is a unique_ptr<vector>. Assign the ref into nextChars
  // so we can change the m_pLineChars vector ...
  std::vector<CFX_TxtChar>& nextChars = *pNextLine->m_pLineChars;
  nextChars =
      std::vector<CFX_TxtChar>(curChars.begin() + iCharPos, curChars.end());
  curChars.erase(curChars.begin() + iCharPos, curChars.end());
  pCurLine->m_iWidth = iEndPos;
  CFX_TxtChar* pTC = &curChars[iCharPos - 1];
  pTC->m_nBreakType = FX_LBT_UNKNOWN;
  iCount = pdfium::CollectionSize<int>(nextChars);
  int32_t iWidth = 0;
  for (int32_t i = 0; i < iCount; i++) {
    if (nextChars[i].GetCharType() >= FX_CHARTYPE_ArabicAlef) {
      pCurLine->m_iArabicChars--;
      pNextLine->m_iArabicChars++;
    }
    int32_t iCharWidth = nextChars[i].m_iCharWidth;
    if (iCharWidth > 0)
      iWidth += iCharWidth;
    if (m_bPagination)
      continue;

    nextChars[i].m_dwStatus = 0;
  }
  pNextLine->m_iWidth = iWidth;
}

int32_t CFX_TxtBreak::CountBreakPieces() const {
  CFX_TxtPieceArray* pTxtPieces = GetTxtPieces();
  return pTxtPieces ? pTxtPieces->GetSize() : 0;
}

const CFX_TxtPiece* CFX_TxtBreak::GetBreakPiece(int32_t index) const {
  CFX_TxtPieceArray* pTxtPieces = GetTxtPieces();
  if (!pTxtPieces)
    return nullptr;
  if (index < 0 || index >= pTxtPieces->GetSize())
    return nullptr;
  return pTxtPieces->GetPtrAt(index);
}

void CFX_TxtBreak::ClearBreakPieces() {
  CFX_TxtLine* pTxtLine = GetTxtLine();
  if (pTxtLine)
    pTxtLine->RemoveAll(true);
  m_iReady = 0;
}

void CFX_TxtBreak::Reset() {
  m_eCharType = FX_CHARTYPE_Unknown;
  m_iArabicContext = 1;
  m_iCurArabicContext = 1;
  ResetArabicContext();
  m_pTxtLine1->RemoveAll(true);
  m_pTxtLine2->RemoveAll(true);
}

struct FX_FORMCHAR {
  uint16_t wch;
  uint16_t wForm;
  int32_t iWidth;
};

int32_t CFX_TxtBreak::GetDisplayPos(const FX_TXTRUN* pTxtRun,
                                    FXTEXT_CHARPOS* pCharPos,
                                    bool bCharCode,
                                    CFX_WideString* pWSForms) const {
  if (!pTxtRun || pTxtRun->iLength < 1)
    return 0;

  IFX_TxtAccess* pAccess = pTxtRun->pAccess;
  const FDE_TEXTEDITPIECE* pIdentity = pTxtRun->pIdentity;
  const FX_WCHAR* pStr = pTxtRun->wsStr.c_str();
  int32_t* pWidths = pTxtRun->pWidths;
  int32_t iLength = pTxtRun->iLength - 1;
  CFX_RetainPtr<CFGAS_GEFont> pFont = pTxtRun->pFont;
  uint32_t dwStyles = pTxtRun->dwStyles;
  CFX_RectF rtText(*pTxtRun->pRect);
  bool bRTLPiece = (pTxtRun->dwCharStyles & FX_TXTCHARSTYLE_OddBidiLevel) != 0;
  bool bArabicNumber =
      (pTxtRun->dwCharStyles & FX_TXTCHARSTYLE_ArabicNumber) != 0;
  bool bArabicComma =
      (pTxtRun->dwCharStyles & FX_TXTCHARSTYLE_ArabicComma) != 0;
  FX_FLOAT fFontSize = pTxtRun->fFontSize;
  int32_t iFontSize = FXSYS_round(fFontSize * 20.0f);
  int32_t iAscent = pFont->GetAscent();
  int32_t iDescent = pFont->GetDescent();
  int32_t iMaxHeight = iAscent - iDescent;
  FX_FLOAT fFontHeight = fFontSize;
  FX_FLOAT fAscent = fFontHeight * (FX_FLOAT)iAscent / (FX_FLOAT)iMaxHeight;
  FX_FLOAT fDescent = fFontHeight * (FX_FLOAT)iDescent / (FX_FLOAT)iMaxHeight;
  bool bVerticalDoc = (dwStyles & FX_TXTLAYOUTSTYLE_VerticalLayout) != 0;
  bool bVerticalChar = (dwStyles & FX_TXTLAYOUTSTYLE_VerticalChars) != 0;
  int32_t iRotation = GetLineRotation(dwStyles) + pTxtRun->iCharRotation;
  FX_FLOAT fX = rtText.left;
  FX_FLOAT fY;
  FX_FLOAT fCharWidth;
  FX_FLOAT fCharHeight;
  int32_t iHorScale = pTxtRun->iHorizontalScale;
  int32_t iVerScale = pTxtRun->iVerticalScale;
  bool bSkipSpace = pTxtRun->bSkipSpace;
  FX_FORMCHAR formChars[3];
  FX_FLOAT fYBase;

  if (bVerticalDoc) {
    fX += (rtText.width - fFontSize) / 2.0f;
    fYBase = bRTLPiece ? rtText.bottom() : rtText.top;
    fY = fYBase;
  } else {
    if (bRTLPiece)
      fX = rtText.right();

    fYBase = rtText.top + (rtText.height - fFontSize) / 2.0f;
    fY = fYBase + fAscent;
  }

  int32_t iCount = 0;
  int32_t iNext = 0;
  FX_WCHAR wPrev = 0xFEFF;
  FX_WCHAR wNext = 0xFEFF;
  FX_WCHAR wForm = 0xFEFF;
  FX_WCHAR wLast = 0xFEFF;
  bool bShadda = false;
  bool bLam = false;
  for (int32_t i = 0; i <= iLength; i++) {
    int32_t iWidth;
    FX_WCHAR wch;
    if (pAccess) {
      wch = pAccess->GetChar(pIdentity, i);
      iWidth = pAccess->GetWidth(pIdentity, i);
    } else {
      wch = *pStr++;
      iWidth = *pWidths++;
    }

    uint32_t dwProps = FX_GetUnicodeProperties(wch);
    FX_CHARTYPE chartype = GetCharTypeFromProp(dwProps);
    if (chartype == FX_CHARTYPE_ArabicAlef && iWidth == 0) {
      wPrev = 0xFEFF;
      wLast = wch;
      continue;
    }

    if (chartype >= FX_CHARTYPE_ArabicAlef) {
      if (i < iLength) {
        if (pAccess) {
          iNext = i + 1;
          while (iNext <= iLength) {
            wNext = pAccess->GetChar(pIdentity, iNext);
            dwProps = FX_GetUnicodeProperties(wNext);
            if ((dwProps & FX_CHARTYPEBITSMASK) != FX_CHARTYPE_Combination)
              break;

            iNext++;
          }
          if (iNext > iLength)
            wNext = 0xFEFF;
        } else {
          int32_t j = -1;
          do {
            j++;
            if (i + j >= iLength)
              break;

            wNext = pStr[j];
            dwProps = FX_GetUnicodeProperties(wNext);
          } while ((dwProps & FX_CHARTYPEBITSMASK) == FX_CHARTYPE_Combination);
          if (i + j >= iLength)
            wNext = 0xFEFF;
        }
      } else {
        wNext = 0xFEFF;
      }

      wForm = pdfium::arabic::GetFormChar(wch, wPrev, wNext);
      bLam = (wPrev == 0x0644 && wch == 0x0644 && wNext == 0x0647);
    } else if (chartype == FX_CHARTYPE_Combination) {
      wForm = wch;
      if (wch >= 0x064C && wch <= 0x0651) {
        if (bShadda) {
          wForm = 0xFEFF;
          bShadda = false;
        } else {
          wNext = 0xFEFF;
          if (pAccess) {
            iNext = i + 1;
            if (iNext <= iLength)
              wNext = pAccess->GetChar(pIdentity, iNext);
          } else {
            if (i < iLength)
              wNext = *pStr;
          }
          if (wch == 0x0651) {
            if (wNext >= 0x064C && wNext <= 0x0650) {
              wForm = FX_GetArabicFromShaddaTable(wNext);
              bShadda = true;
            }
          } else {
            if (wNext == 0x0651) {
              wForm = FX_GetArabicFromShaddaTable(wch);
              bShadda = true;
            }
          }
        }
      } else {
        bShadda = false;
      }
    } else if (chartype == FX_CHARTYPE_Numeric) {
      wForm = wch;
      if (bArabicNumber)
        wForm += 0x0630;
    } else if (wch == L'.') {
      wForm = wch;
      if (bArabicNumber) {
        wNext = 0xFEFF;
        if (pAccess) {
          iNext = i + 1;
          if (iNext <= iLength)
            wNext = pAccess->GetChar(pIdentity, iNext);
        } else {
          if (i < iLength)
            wNext = *pStr;
        }
        if (wNext >= L'0' && wNext <= L'9')
          wForm = 0x066B;
      }
    } else if (wch == L',') {
      wForm = wch;
      if (bArabicComma)
        wForm = 0x060C;
    } else if (bRTLPiece || bVerticalChar) {
      wForm = FX_GetMirrorChar(wch, dwProps, bRTLPiece, bVerticalChar);
    } else {
      wForm = wch;
    }
    if (chartype != FX_CHARTYPE_Combination)
      bShadda = false;
    if (chartype < FX_CHARTYPE_ArabicAlef)
      bLam = false;

    dwProps = FX_GetUnicodeProperties(wForm);
    int32_t iCharRotation = iRotation;
    if (bVerticalChar && (dwProps & 0x8000) != 0)
      iCharRotation++;

    iCharRotation %= 4;
    bool bEmptyChar =
        (chartype >= FX_CHARTYPE_Tab && chartype <= FX_CHARTYPE_Control);
    if (wForm == 0xFEFF)
      bEmptyChar = true;

    int32_t iForms = bLam ? 3 : 1;
    iCount += (bEmptyChar && bSkipSpace) ? 0 : iForms;
    if (!pCharPos) {
      if (iWidth > 0)
        wPrev = wch;
      wLast = wch;
      continue;
    }

    int32_t iCharWidth = iWidth;
    if (iCharWidth < 0)
      iCharWidth = -iCharWidth;

    iCharWidth /= iFontSize;
    formChars[0].wch = wch;
    formChars[0].wForm = wForm;
    formChars[0].iWidth = iCharWidth;
    if (bLam) {
      formChars[1].wForm = 0x0651;
      iCharWidth = 0;
      pFont->GetCharWidth(0x0651, iCharWidth, false);
      formChars[1].iWidth = iCharWidth;
      formChars[2].wForm = 0x0670;
      iCharWidth = 0;
      pFont->GetCharWidth(0x0670, iCharWidth, false);
      formChars[2].iWidth = iCharWidth;
    }

    for (int32_t j = 0; j < iForms; j++) {
      wForm = (FX_WCHAR)formChars[j].wForm;
      iCharWidth = formChars[j].iWidth;
      if (j > 0) {
        chartype = FX_CHARTYPE_Combination;
        wch = wForm;
        wLast = (FX_WCHAR)formChars[j - 1].wForm;
      }
      if (!bEmptyChar || (bEmptyChar && !bSkipSpace)) {
        pCharPos->m_GlyphIndex =
            bCharCode ? wch : pFont->GetGlyphIndex(wForm, false);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
        pCharPos->m_ExtGID = pCharPos->m_GlyphIndex;
#endif
        pCharPos->m_FontCharWidth = iCharWidth;
        if (pWSForms)
          *pWSForms += wForm;
      }

      int32_t iCharHeight;
      if (bVerticalDoc) {
        iCharHeight = iCharWidth;
        iCharWidth = 1000;
      } else {
        iCharHeight = 1000;
      }

      fCharWidth = fFontSize * iCharWidth / 1000.0f;
      fCharHeight = fFontSize * iCharHeight / 1000.0f;
      if (bRTLPiece && chartype != FX_CHARTYPE_Combination) {
        if (bVerticalDoc)
          fY -= fCharHeight;
        else
          fX -= fCharWidth;
      }
      if (!bEmptyChar || (bEmptyChar && !bSkipSpace)) {
        pCharPos->m_Origin = CFX_PointF(fX, fY);
        if ((dwStyles & FX_TXTLAYOUTSTYLE_CombText) != 0) {
          int32_t iFormWidth = iCharWidth;
          pFont->GetCharWidth(wForm, iFormWidth, false);
          FX_FLOAT fOffset = fFontSize * (iCharWidth - iFormWidth) / 2000.0f;
          if (bVerticalDoc)
            pCharPos->m_Origin.y += fOffset;
          else
            pCharPos->m_Origin.x += fOffset;
        }

        if (chartype == FX_CHARTYPE_Combination) {
          CFX_Rect rtBBox;
          if (pFont->GetCharBBox(wForm, &rtBBox, false)) {
            pCharPos->m_Origin.y =
                fYBase + fFontSize -
                fFontSize * (FX_FLOAT)rtBBox.height / (FX_FLOAT)iMaxHeight;
          }
          if (wForm == wch && wLast != 0xFEFF) {
            uint32_t dwLastProps = FX_GetUnicodeProperties(wLast);
            if ((dwLastProps & FX_CHARTYPEBITSMASK) ==
                FX_CHARTYPE_Combination) {
              CFX_Rect rtBox;
              if (pFont->GetCharBBox(wLast, &rtBox, false))
                pCharPos->m_Origin.y -= fFontSize * rtBox.height / iMaxHeight;
            }
          }
        }
        CFX_PointF ptOffset;
        if (bVerticalChar && (dwProps & 0x00010000) != 0) {
          CFX_Rect rtBBox;
          if (pFont->GetCharBBox(wForm, &rtBBox, false)) {
            ptOffset.x = fFontSize * (850 - rtBBox.right()) / iMaxHeight;
            ptOffset.y = fFontSize * (iAscent - rtBBox.top - 150) / iMaxHeight;
          }
        }
        pCharPos->m_Origin.x += ptOffset.x;
        pCharPos->m_Origin.y -= ptOffset.y;
      }
      if (!bRTLPiece && chartype != FX_CHARTYPE_Combination) {
        if (bVerticalDoc)
          fY += fCharHeight;
        else
          fX += fCharWidth;
      }

      if (!bEmptyChar || (bEmptyChar && !bSkipSpace)) {
        pCharPos->m_bGlyphAdjust = true;
        if (bVerticalDoc) {
          if (iCharRotation == 0) {
            pCharPos->m_AdjustMatrix[0] = -1;
            pCharPos->m_AdjustMatrix[1] = 0;
            pCharPos->m_AdjustMatrix[2] = 0;
            pCharPos->m_AdjustMatrix[3] = 1;
            pCharPos->m_Origin.y += fAscent;
          } else if (iCharRotation == 1) {
            pCharPos->m_AdjustMatrix[0] = 0;
            pCharPos->m_AdjustMatrix[1] = -1;
            pCharPos->m_AdjustMatrix[2] = -1;
            pCharPos->m_AdjustMatrix[3] = 0;
            pCharPos->m_Origin.x -= fDescent;
          } else if (iCharRotation == 2) {
            pCharPos->m_AdjustMatrix[0] = 1;
            pCharPos->m_AdjustMatrix[1] = 0;
            pCharPos->m_AdjustMatrix[2] = 0;
            pCharPos->m_AdjustMatrix[3] = -1;
            pCharPos->m_Origin.x += fCharWidth;
            pCharPos->m_Origin.y += fAscent;
          } else {
            pCharPos->m_AdjustMatrix[0] = 0;
            pCharPos->m_AdjustMatrix[1] = 1;
            pCharPos->m_AdjustMatrix[2] = 1;
            pCharPos->m_AdjustMatrix[3] = 0;
            pCharPos->m_Origin.x += fAscent;
          }
        } else {
          if (iCharRotation == 0) {
            pCharPos->m_AdjustMatrix[0] = -1;
            pCharPos->m_AdjustMatrix[1] = 0;
            pCharPos->m_AdjustMatrix[2] = 0;
            pCharPos->m_AdjustMatrix[3] = 1;
          } else if (iCharRotation == 1) {
            pCharPos->m_AdjustMatrix[0] = 0;
            pCharPos->m_AdjustMatrix[1] = -1;
            pCharPos->m_AdjustMatrix[2] = -1;
            pCharPos->m_AdjustMatrix[3] = 0;
            pCharPos->m_Origin.x -= fDescent;
            pCharPos->m_Origin.y -= fAscent + fDescent;
          } else if (iCharRotation == 2) {
            pCharPos->m_AdjustMatrix[0] = 1;
            pCharPos->m_AdjustMatrix[1] = 0;
            pCharPos->m_AdjustMatrix[2] = 0;
            pCharPos->m_AdjustMatrix[3] = -1;
            pCharPos->m_Origin.x += fCharWidth;
            pCharPos->m_Origin.y -= fAscent;
          } else {
            pCharPos->m_AdjustMatrix[0] = 0;
            pCharPos->m_AdjustMatrix[1] = 1;
            pCharPos->m_AdjustMatrix[2] = 1;
            pCharPos->m_AdjustMatrix[3] = 0;
            pCharPos->m_Origin.x += fAscent;
          }
        }
        if (iHorScale != 100 || iVerScale != 100) {
          pCharPos->m_AdjustMatrix[0] =
              pCharPos->m_AdjustMatrix[0] * iHorScale / 100.0f;
          pCharPos->m_AdjustMatrix[1] =
              pCharPos->m_AdjustMatrix[1] * iHorScale / 100.0f;
          pCharPos->m_AdjustMatrix[2] =
              pCharPos->m_AdjustMatrix[2] * iVerScale / 100.0f;
          pCharPos->m_AdjustMatrix[3] =
              pCharPos->m_AdjustMatrix[3] * iVerScale / 100.0f;
        }
        pCharPos++;
      }
    }
    if (iWidth > 0)
      wPrev = static_cast<FX_WCHAR>(formChars[0].wch);
    wLast = wch;
  }
  return iCount;
}

std::vector<CFX_RectF> CFX_TxtBreak::GetCharRects(const FX_TXTRUN* pTxtRun,
                                                  bool bCharBBox) const {
  if (!pTxtRun || pTxtRun->iLength < 1)
    return std::vector<CFX_RectF>();

  IFX_TxtAccess* pAccess = pTxtRun->pAccess;
  const FDE_TEXTEDITPIECE* pIdentity = pTxtRun->pIdentity;
  const FX_WCHAR* pStr = pTxtRun->wsStr.c_str();
  int32_t* pWidths = pTxtRun->pWidths;
  int32_t iLength = pTxtRun->iLength;
  CFX_RectF rect(*pTxtRun->pRect);
  FX_FLOAT fFontSize = pTxtRun->fFontSize;
  int32_t iFontSize = FXSYS_round(fFontSize * 20.0f);
  FX_FLOAT fScale = fFontSize / 1000.0f;
  CFX_RetainPtr<CFGAS_GEFont> pFont = pTxtRun->pFont;
  if (!pFont)
    bCharBBox = false;

  CFX_Rect bbox;
  if (bCharBBox)
    bCharBBox = pFont->GetBBox(&bbox);

  FX_FLOAT fLeft = std::max(0.0f, bbox.left * fScale);
  FX_FLOAT fHeight = FXSYS_fabs(bbox.height * fScale);
  bool bRTLPiece = !!(pTxtRun->dwCharStyles & FX_TXTCHARSTYLE_OddBidiLevel);
  bool bVertical = !!(pTxtRun->dwStyles & FX_TXTLAYOUTSTYLE_VerticalLayout);
  bool bSingleLine = !!(pTxtRun->dwStyles & FX_TXTLAYOUTSTYLE_SingleLine);
  bool bCombText = !!(pTxtRun->dwStyles & FX_TXTLAYOUTSTYLE_CombText);
  FX_WCHAR wch;
  FX_WCHAR wLineBreakChar = pTxtRun->wLineBreakChar;
  int32_t iCharSize;
  FX_FLOAT fCharSize;
  FX_FLOAT fStart;
  if (bVertical)
    fStart = bRTLPiece ? rect.bottom() : rect.top;
  else
    fStart = bRTLPiece ? rect.right() : rect.left;

  std::vector<CFX_RectF> rtArray(iLength);
  for (int32_t i = 0; i < iLength; i++) {
    if (pAccess) {
      wch = pAccess->GetChar(pIdentity, i);
      iCharSize = pAccess->GetWidth(pIdentity, i);
    } else {
      wch = *pStr++;
      iCharSize = *pWidths++;
    }
    fCharSize = static_cast<FX_FLOAT>(iCharSize) / 20000.0f;
    bool bRet = (!bSingleLine && FX_IsCtrlCode(wch));
    if (!(wch == L'\v' || wch == L'\f' || wch == 0x2028 || wch == 0x2029 ||
          (wLineBreakChar != 0xFEFF && wch == wLineBreakChar))) {
      bRet = false;
    }
    if (bRet) {
      iCharSize = iFontSize * 500;
      fCharSize = fFontSize / 2.0f;
    }
    if (bVertical) {
      rect.top = fStart;
      if (bRTLPiece) {
        rect.top -= fCharSize;
        fStart -= fCharSize;
      } else {
        fStart += fCharSize;
      }
      rect.height = fCharSize;
    } else {
      rect.left = fStart;
      if (bRTLPiece) {
        rect.left -= fCharSize;
        fStart -= fCharSize;
      } else {
        fStart += fCharSize;
      }
      rect.width = fCharSize;
    }

    if (bCharBBox && !bRet) {
      int32_t iCharWidth = 1000;
      pFont->GetCharWidth(wch, iCharWidth, false);
      FX_FLOAT fRTLeft = 0, fCharWidth = 0;
      if (iCharWidth > 0) {
        fCharWidth = iCharWidth * fScale;
        fRTLeft = fLeft;
        if (bCombText)
          fRTLeft = (rect.width - fCharWidth) / 2.0f;
      }
      CFX_RectF rtBBoxF;
      if (bVertical) {
        rtBBoxF.top = rect.left + fRTLeft;
        rtBBoxF.left = rect.top + (rect.height - fHeight) / 2.0f;
        rtBBoxF.height = fCharWidth;
        rtBBoxF.width = fHeight;
        rtBBoxF.left = std::max(rtBBoxF.left, 0.0f);
      } else {
        rtBBoxF.left = rect.left + fRTLeft;
        rtBBoxF.top = rect.top + (rect.height - fHeight) / 2.0f;
        rtBBoxF.width = fCharWidth;
        rtBBoxF.height = fHeight;
        rtBBoxF.top = std::max(rtBBoxF.top, 0.0f);
      }
      rtArray[i] = rtBBoxF;
      continue;
    }
    rtArray[i] = rect;
  }
  return rtArray;
}

FX_TXTRUN::FX_TXTRUN()
    : pAccess(nullptr),
      pIdentity(nullptr),
      pWidths(nullptr),
      iLength(0),
      pFont(nullptr),
      fFontSize(12),
      dwStyles(0),
      iHorizontalScale(100),
      iVerticalScale(100),
      iCharRotation(0),
      dwCharStyles(0),
      pRect(nullptr),
      wLineBreakChar(L'\n'),
      bSkipSpace(true) {}

FX_TXTRUN::~FX_TXTRUN() {}

FX_TXTRUN::FX_TXTRUN(const FX_TXTRUN& other) = default;

CFX_TxtPiece::CFX_TxtPiece()
    : m_dwStatus(FX_TXTBREAK_PieceBreak),
      m_iStartPos(0),
      m_iWidth(-1),
      m_iStartChar(0),
      m_iChars(0),
      m_iBidiLevel(0),
      m_iBidiPos(0),
      m_iHorizontalScale(100),
      m_iVerticalScale(100),
      m_dwCharStyles(0),
      m_pChars(nullptr),
      m_pUserData(nullptr) {}

CFX_TxtLine::CFX_TxtLine(int32_t iBlockSize)
    : m_pLineChars(new std::vector<CFX_TxtChar>),
      m_pLinePieces(new CFX_TxtPieceArray(16)),
      m_iStart(0),
      m_iWidth(0),
      m_iArabicChars(0) {}

CFX_TxtLine::~CFX_TxtLine() {
  RemoveAll();
}
