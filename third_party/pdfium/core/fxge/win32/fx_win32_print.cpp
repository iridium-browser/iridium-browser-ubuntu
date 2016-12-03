// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fxcrt/include/fx_system.h"

#if _FX_OS_ == _FX_WIN32_DESKTOP_ || _FX_OS_ == _FX_WIN64_DESKTOP_

#include <windows.h>

#include <algorithm>

#include "core/fxge/dib/dib_int.h"
#include "core/fxge/ge/fx_text_int.h"
#include "core/fxge/include/fx_freetype.h"
#include "core/fxge/include/cfx_fontcache.h"
#include "core/fxge/include/cfx_renderdevice.h"
#include "core/fxge/include/cfx_windowsdevice.h"
#include "core/fxge/win32/win32_int.h"

#if defined(PDFIUM_PRINT_TEXT_WITH_GDI)
namespace {

class ScopedState {
 public:
  ScopedState(HDC hDC, HFONT hFont) : m_hDC(hDC) {
    m_iState = SaveDC(m_hDC);
    m_hFont = SelectObject(m_hDC, hFont);
  }

  ~ScopedState() {
    HGDIOBJ hFont = SelectObject(m_hDC, m_hFont);
    DeleteObject(hFont);
    RestoreDC(m_hDC, m_iState);
  }

 private:
  HDC m_hDC;
  HGDIOBJ m_hFont;
  int m_iState;

  ScopedState(const ScopedState&) = delete;
  void operator=(const ScopedState&) = delete;
};

}  // namespace

bool g_pdfium_print_text_with_gdi = false;

PDFiumEnsureTypefaceCharactersAccessible g_pdfium_typeface_accessible_func =
    nullptr;
#endif

CGdiPrinterDriver::CGdiPrinterDriver(HDC hDC)
    : CGdiDeviceDriver(hDC, FXDC_PRINTER),
      m_HorzSize(::GetDeviceCaps(m_hDC, HORZSIZE)),
      m_VertSize(::GetDeviceCaps(m_hDC, VERTSIZE)) {}

CGdiPrinterDriver::~CGdiPrinterDriver() {}

int CGdiPrinterDriver::GetDeviceCaps(int caps_id) const {
  if (caps_id == FXDC_HORZ_SIZE)
    return m_HorzSize;
  if (caps_id == FXDC_VERT_SIZE)
    return m_VertSize;
  return CGdiDeviceDriver::GetDeviceCaps(caps_id);
}

FX_BOOL CGdiPrinterDriver::SetDIBits(const CFX_DIBSource* pSource,
                                     uint32_t color,
                                     const FX_RECT* pSrcRect,
                                     int left,
                                     int top,
                                     int blend_type) {
  if (pSource->IsAlphaMask()) {
    FX_RECT clip_rect(left, top, left + pSrcRect->Width(),
                      top + pSrcRect->Height());
    return StretchDIBits(pSource, color, left - pSrcRect->left,
                         top - pSrcRect->top, pSource->GetWidth(),
                         pSource->GetHeight(), &clip_rect, 0,
                         FXDIB_BLEND_NORMAL);
  }
  ASSERT(pSource && !pSource->IsAlphaMask() && pSrcRect);
  ASSERT(blend_type == FXDIB_BLEND_NORMAL);
  if (pSource->HasAlpha())
    return FALSE;

  CFX_DIBExtractor temp(pSource);
  CFX_DIBitmap* pBitmap = temp.GetBitmap();
  if (!pBitmap)
    return FALSE;

  return GDI_SetDIBits(pBitmap, pSrcRect, left, top);
}

FX_BOOL CGdiPrinterDriver::StretchDIBits(const CFX_DIBSource* pSource,
                                         uint32_t color,
                                         int dest_left,
                                         int dest_top,
                                         int dest_width,
                                         int dest_height,
                                         const FX_RECT* pClipRect,
                                         uint32_t flags,
                                         int blend_type) {
  if (pSource->IsAlphaMask()) {
    int alpha = FXARGB_A(color);
    if (pSource->GetBPP() != 1 || alpha != 255)
      return FALSE;

    if (dest_width < 0 || dest_height < 0) {
      std::unique_ptr<CFX_DIBitmap> pFlipped(
          pSource->FlipImage(dest_width < 0, dest_height < 0));
      if (!pFlipped)
        return FALSE;

      if (dest_width < 0)
        dest_left += dest_width;
      if (dest_height < 0)
        dest_top += dest_height;

      return GDI_StretchBitMask(pFlipped.get(), dest_left, dest_top,
                                abs(dest_width), abs(dest_height), color,
                                flags);
    }

    CFX_DIBExtractor temp(pSource);
    CFX_DIBitmap* pBitmap = temp.GetBitmap();
    if (!pBitmap)
      return FALSE;
    return GDI_StretchBitMask(pBitmap, dest_left, dest_top, dest_width,
                              dest_height, color, flags);
  }

  if (pSource->HasAlpha())
    return FALSE;

  if (dest_width < 0 || dest_height < 0) {
    std::unique_ptr<CFX_DIBitmap> pFlipped(
        pSource->FlipImage(dest_width < 0, dest_height < 0));
    if (!pFlipped)
      return FALSE;

    if (dest_width < 0)
      dest_left += dest_width;
    if (dest_height < 0)
      dest_top += dest_height;

    return GDI_StretchDIBits(pFlipped.get(), dest_left, dest_top,
                             abs(dest_width), abs(dest_height), flags);
  }

  CFX_DIBExtractor temp(pSource);
  CFX_DIBitmap* pBitmap = temp.GetBitmap();
  if (!pBitmap)
    return FALSE;
  return GDI_StretchDIBits(pBitmap, dest_left, dest_top, dest_width,
                           dest_height, flags);
}

FX_BOOL CGdiPrinterDriver::StartDIBits(const CFX_DIBSource* pSource,
                                       int bitmap_alpha,
                                       uint32_t color,
                                       const CFX_Matrix* pMatrix,
                                       uint32_t render_flags,
                                       void*& handle,
                                       int blend_type) {
  if (bitmap_alpha < 255 || pSource->HasAlpha() ||
      (pSource->IsAlphaMask() && (pSource->GetBPP() != 1))) {
    return FALSE;
  }
  CFX_FloatRect unit_rect = pMatrix->GetUnitRect();
  FX_RECT full_rect = unit_rect.GetOuterRect();
  if (FXSYS_fabs(pMatrix->b) < 0.5f && pMatrix->a != 0 &&
      FXSYS_fabs(pMatrix->c) < 0.5f && pMatrix->d != 0) {
    FX_BOOL bFlipX = pMatrix->a < 0;
    FX_BOOL bFlipY = pMatrix->d > 0;
    return StretchDIBits(pSource, color,
                         bFlipX ? full_rect.right : full_rect.left,
                         bFlipY ? full_rect.bottom : full_rect.top,
                         bFlipX ? -full_rect.Width() : full_rect.Width(),
                         bFlipY ? -full_rect.Height() : full_rect.Height(),
                         nullptr, 0, blend_type);
  }
  if (FXSYS_fabs(pMatrix->a) >= 0.5f || FXSYS_fabs(pMatrix->d) >= 0.5f)
    return FALSE;

  std::unique_ptr<CFX_DIBitmap> pTransformed(
      pSource->SwapXY(pMatrix->c > 0, pMatrix->b < 0));
  if (!pTransformed)
    return FALSE;

  return StretchDIBits(pTransformed.get(), color, full_rect.left, full_rect.top,
                       full_rect.Width(), full_rect.Height(), nullptr, 0,
                       blend_type);
}

FX_BOOL CGdiPrinterDriver::DrawDeviceText(int nChars,
                                          const FXTEXT_CHARPOS* pCharPos,
                                          CFX_Font* pFont,
                                          CFX_FontCache* pCache,
                                          const CFX_Matrix* pObject2Device,
                                          FX_FLOAT font_size,
                                          uint32_t color) {
#if defined(PDFIUM_PRINT_TEXT_WITH_GDI)
  if (!g_pdfium_print_text_with_gdi)
    return FALSE;

  if (nChars < 1 || !pFont || !pFont->IsEmbedded() || !pFont->IsTTFont())
    return FALSE;

  // Scale factor used to minimize the kerning problems caused by rounding
  // errors below. Value choosen based on the title of https://crbug.com/18383
  const double kScaleFactor = 10;

  // Font
  //
  // Note that |pFont| has the actual font to render with embedded within, but
  // but unfortunately AddFontMemResourceEx() does not seem to cooperate.
  // Loading font data to memory seems to work, but then enumerating the fonts
  // fails to find it. This requires more investigation. In the meanwhile,
  // assume the printing is happening on the machine that generated the PDF, so
  // the embedded font, if not a web font, is available through GDI anyway.
  // TODO(thestig): Figure out why AddFontMemResourceEx() does not work.
  // Generalize this method to work for all PDFs with embedded fonts.
  // In sandboxed environments, font loading may not work at all, so this may be
  // the best possible effort.
  LOGFONT lf = {};
  lf.lfHeight = -font_size * kScaleFactor;
  lf.lfWeight = pFont->IsBold() ? FW_BOLD : FW_NORMAL;
  lf.lfItalic = pFont->IsItalic();
  lf.lfCharSet = DEFAULT_CHARSET;

  const CFX_WideString wsName = pFont->GetFaceName().UTF8Decode();
  int iNameLen = std::min(wsName.GetLength(), LF_FACESIZE - 1);
  memcpy(lf.lfFaceName, wsName.c_str(), sizeof(lf.lfFaceName[0]) * iNameLen);
  lf.lfFaceName[iNameLen] = 0;

  HFONT hFont = CreateFontIndirect(&lf);
  if (!hFont)
    return FALSE;

  ScopedState state(m_hDC, hFont);
  size_t nTextMetricSize = GetOutlineTextMetrics(m_hDC, 0, nullptr);
  if (nTextMetricSize == 0) {
    // Give up and fail if there is no way to get the font to try again.
    if (!g_pdfium_typeface_accessible_func)
      return FALSE;

    // Try to get the font. Any letter will do.
    g_pdfium_typeface_accessible_func(&lf, L"A", 1);
    nTextMetricSize = GetOutlineTextMetrics(m_hDC, 0, nullptr);
    if (nTextMetricSize == 0)
      return FALSE;
  }

  std::vector<BYTE> buf(nTextMetricSize);
  OUTLINETEXTMETRIC* pTextMetric =
      reinterpret_cast<OUTLINETEXTMETRIC*>(buf.data());
  if (GetOutlineTextMetrics(m_hDC, nTextMetricSize, pTextMetric) == 0)
    return FALSE;

  // If the selected font is not the requested font, then bail out. This can
  // happen with web fonts, for example.
  wchar_t* wsSelectedName = reinterpret_cast<wchar_t*>(
      buf.data() + reinterpret_cast<size_t>(pTextMetric->otmpFaceName));
  if (wsName != wsSelectedName)
    return FALSE;

  // Transforms
  SetGraphicsMode(m_hDC, GM_ADVANCED);
  XFORM xform;
  xform.eM11 = pObject2Device->GetA() / kScaleFactor;
  xform.eM12 = pObject2Device->GetB() / kScaleFactor;
  xform.eM21 = -pObject2Device->GetC() / kScaleFactor;
  xform.eM22 = -pObject2Device->GetD() / kScaleFactor;
  xform.eDx = pObject2Device->GetE();
  xform.eDy = pObject2Device->GetF();
  ModifyWorldTransform(m_hDC, &xform, MWT_LEFTMULTIPLY);

  // Color
  int iUnusedAlpha;
  FX_COLORREF rgb;
  ArgbDecode(color, iUnusedAlpha, rgb);
  SetTextColor(m_hDC, rgb);
  SetBkMode(m_hDC, TRANSPARENT);

  // Text
  CFX_WideString wsText;
  std::vector<INT> spacing(nChars);
  FX_FLOAT fPreviousOriginX = 0;
  for (int i = 0; i < nChars; ++i) {
    // Only works with PDFs from Skia's PDF generator. Cannot handle arbitrary
    // values from PDFs.
    const FXTEXT_CHARPOS& charpos = pCharPos[i];
    ASSERT(charpos.m_AdjustMatrix[0] == 0);
    ASSERT(charpos.m_AdjustMatrix[1] == 0);
    ASSERT(charpos.m_AdjustMatrix[2] == 0);
    ASSERT(charpos.m_AdjustMatrix[3] == 0);
    ASSERT(charpos.m_OriginY == 0);

    // Round the spacing to the nearest integer, but keep track of the rounding
    // error for calculating the next spacing value.
    FX_FLOAT fOriginX = charpos.m_OriginX * kScaleFactor;
    FX_FLOAT fPixelSpacing = fOriginX - fPreviousOriginX;
    spacing[i] = FXSYS_round(fPixelSpacing);
    fPreviousOriginX = fOriginX - (fPixelSpacing - spacing[i]);

    wsText += charpos.m_GlyphIndex;
  }

  // Draw
  SetTextAlign(m_hDC, TA_LEFT | TA_BASELINE);
  if (ExtTextOutW(m_hDC, 0, 0, ETO_GLYPH_INDEX, nullptr, wsText.c_str(), nChars,
                  nChars > 1 ? &spacing[1] : nullptr)) {
    return TRUE;
  }

  // Give up and fail if there is no way to get the font to try again.
  if (!g_pdfium_typeface_accessible_func)
    return FALSE;

  // Try to get the font and draw again.
  g_pdfium_typeface_accessible_func(&lf, wsText.c_str(), nChars);
  return ExtTextOutW(m_hDC, 0, 0, ETO_GLYPH_INDEX, nullptr, wsText.c_str(),
                     nChars, nChars > 1 ? &spacing[1] : nullptr);
#else
  return FALSE;
#endif
}

#endif  // _FX_OS_ == _FX_WIN32_DESKTOP_ || _FX_OS_ == _FX_WIN64_DESKTOP_
