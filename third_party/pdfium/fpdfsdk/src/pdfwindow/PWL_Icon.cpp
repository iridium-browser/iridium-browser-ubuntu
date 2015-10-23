// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/pdfwindow/PDFWindow.h"
#include "../../include/pdfwindow/PWL_Wnd.h"
#include "../../include/pdfwindow/PWL_Icon.h"
#include "../../include/pdfwindow/PWL_Utils.h"

/* ------------------------------- CPWL_Image ----------------------------------
 */

CPWL_Image::CPWL_Image() : m_pPDFStream(NULL) {}

CPWL_Image::~CPWL_Image() {}

CFX_ByteString CPWL_Image::GetImageAppStream() {
  CFX_ByteTextBuf sAppStream;

  CFX_ByteString sAlias = GetImageAlias();
  CPDF_Rect rcPlate = GetClientRect();
  CPDF_Matrix mt;
  mt.SetReverse(GetImageMatrix());

  FX_FLOAT fHScale = 1.0f;
  FX_FLOAT fVScale = 1.0f;
  GetScale(fHScale, fVScale);

  FX_FLOAT fx = 0.0f;
  FX_FLOAT fy = 0.0f;
  GetImageOffset(fx, fy);

  if (m_pPDFStream && sAlias.GetLength() > 0) {
    sAppStream << "q\n";
    sAppStream << rcPlate.left << " " << rcPlate.bottom << " "
               << rcPlate.right - rcPlate.left << " "
               << rcPlate.top - rcPlate.bottom << " re W n\n";

    sAppStream << fHScale << " 0 0 " << fVScale << " " << rcPlate.left + fx
               << " " << rcPlate.bottom + fy << " cm\n";
    sAppStream << mt.GetA() << " " << mt.GetB() << " " << mt.GetC() << " "
               << mt.GetD() << " " << mt.GetE() << " " << mt.GetF() << " cm\n";

    sAppStream << "0 g 0 G 1 w /" << sAlias << " Do\n"
               << "Q\n";
  }

  return sAppStream.GetByteString();
}

void CPWL_Image::SetPDFStream(CPDF_Stream* pStream) {
  m_pPDFStream = pStream;
}

CPDF_Stream* CPWL_Image::GetPDFStream() {
  return m_pPDFStream;
}

void CPWL_Image::GetImageSize(FX_FLOAT& fWidth, FX_FLOAT& fHeight) {
  fWidth = 0.0f;
  fHeight = 0.0f;

  if (m_pPDFStream) {
    if (CPDF_Dictionary* pDict = m_pPDFStream->GetDict()) {
      CPDF_Rect rect = pDict->GetRect("BBox");

      fWidth = rect.right - rect.left;
      fHeight = rect.top - rect.bottom;
    }
  }
}

CPDF_Matrix CPWL_Image::GetImageMatrix() {
  if (m_pPDFStream) {
    if (CPDF_Dictionary* pDict = m_pPDFStream->GetDict()) {
      return pDict->GetMatrix("Matrix");
    }
  }

  return CPDF_Matrix();
}

CFX_ByteString CPWL_Image::GetImageAlias() {
  if (m_sImageAlias.IsEmpty()) {
    if (m_pPDFStream) {
      if (CPDF_Dictionary* pDict = m_pPDFStream->GetDict()) {
        return pDict->GetString("Name");
      }
    }
  } else
    return m_sImageAlias;

  return CFX_ByteString();
}

void CPWL_Image::SetImageAlias(const FX_CHAR* sImageAlias) {
  m_sImageAlias = sImageAlias;
}

void CPWL_Image::GetScale(FX_FLOAT& fHScale, FX_FLOAT& fVScale) {
  fHScale = 1.0f;
  fVScale = 1.0f;
}

void CPWL_Image::GetImageOffset(FX_FLOAT& x, FX_FLOAT& y) {
  x = 0.0f;
  y = 0.0f;
}

/* ------------------------------- CPWL_Icon ----------------------------------
 */

CPWL_Icon::CPWL_Icon() : m_pIconFit(NULL) {}

CPWL_Icon::~CPWL_Icon() {}

int32_t CPWL_Icon::GetScaleMethod() {
  if (m_pIconFit)
    return m_pIconFit->GetScaleMethod();

  return 0;
}

FX_BOOL CPWL_Icon::IsProportionalScale() {
  if (m_pIconFit)
    return m_pIconFit->IsProportionalScale();

  return FALSE;
}

void CPWL_Icon::GetIconPosition(FX_FLOAT& fLeft, FX_FLOAT& fBottom) {
  if (m_pIconFit) {
    // m_pIconFit->GetIconPosition(fLeft,fBottom);
    fLeft = 0.0f;
    fBottom = 0.0f;
    CPDF_Array* pA =
        m_pIconFit->m_pDict ? m_pIconFit->m_pDict->GetArray("A") : NULL;
    if (pA != NULL) {
      FX_DWORD dwCount = pA->GetCount();
      if (dwCount > 0)
        fLeft = pA->GetNumber(0);
      if (dwCount > 1)
        fBottom = pA->GetNumber(1);
    }
  } else {
    fLeft = 0.0f;
    fBottom = 0.0f;
  }
}

FX_BOOL CPWL_Icon::GetFittingBounds() {
  if (m_pIconFit)
    return m_pIconFit->GetFittingBounds();

  return FALSE;
}

void CPWL_Icon::GetScale(FX_FLOAT& fHScale, FX_FLOAT& fVScale) {
  fHScale = 1.0f;
  fVScale = 1.0f;

  if (m_pPDFStream) {
    FX_FLOAT fImageWidth, fImageHeight;
    FX_FLOAT fPlateWidth, fPlateHeight;

    CPDF_Rect rcPlate = GetClientRect();
    fPlateWidth = rcPlate.right - rcPlate.left;
    fPlateHeight = rcPlate.top - rcPlate.bottom;

    GetImageSize(fImageWidth, fImageHeight);

    int32_t nScaleMethod = GetScaleMethod();

    switch (nScaleMethod) {
      default:
      case 0:
        fHScale = fPlateWidth / PWL_MAX(fImageWidth, 1.0f);
        fVScale = fPlateHeight / PWL_MAX(fImageHeight, 1.0f);
        break;
      case 1:
        if (fPlateWidth < fImageWidth)
          fHScale = fPlateWidth / PWL_MAX(fImageWidth, 1.0f);
        if (fPlateHeight < fImageHeight)
          fVScale = fPlateHeight / PWL_MAX(fImageHeight, 1.0f);
        break;
      case 2:
        if (fPlateWidth > fImageWidth)
          fHScale = fPlateWidth / PWL_MAX(fImageWidth, 1.0f);
        if (fPlateHeight > fImageHeight)
          fVScale = fPlateHeight / PWL_MAX(fImageHeight, 1.0f);
        break;
      case 3:
        break;
    }

    FX_FLOAT fMinScale;
    if (IsProportionalScale()) {
      fMinScale = PWL_MIN(fHScale, fVScale);
      fHScale = fMinScale;
      fVScale = fMinScale;
    }
  }
}

void CPWL_Icon::GetImageOffset(FX_FLOAT& x, FX_FLOAT& y) {
  FX_FLOAT fLeft, fBottom;

  GetIconPosition(fLeft, fBottom);
  x = 0.0f;
  y = 0.0f;

  FX_FLOAT fImageWidth, fImageHeight;
  GetImageSize(fImageWidth, fImageHeight);

  FX_FLOAT fHScale, fVScale;
  GetScale(fHScale, fVScale);

  FX_FLOAT fImageFactWidth = fImageWidth * fHScale;
  FX_FLOAT fImageFactHeight = fImageHeight * fVScale;

  FX_FLOAT fPlateWidth, fPlateHeight;
  CPDF_Rect rcPlate = GetClientRect();
  fPlateWidth = rcPlate.right - rcPlate.left;
  fPlateHeight = rcPlate.top - rcPlate.bottom;

  x = (fPlateWidth - fImageFactWidth) * fLeft;
  y = (fPlateHeight - fImageFactHeight) * fBottom;
}
