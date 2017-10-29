// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/render/cpdf_transferfunc.h"

#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/render/cpdf_dibtransferfunc.h"
#include "core/fxge/dib/cfx_dibsource.h"

CPDF_TransferFunc::CPDF_TransferFunc(CPDF_Document* pDoc) : m_pPDFDoc(pDoc) {}

CPDF_TransferFunc::~CPDF_TransferFunc() {}

FX_COLORREF CPDF_TransferFunc::TranslateColor(FX_COLORREF rgb) const {
  return FXSYS_RGB(m_Samples[FXSYS_GetRValue(rgb)],
                   m_Samples[256 + FXSYS_GetGValue(rgb)],
                   m_Samples[512 + FXSYS_GetBValue(rgb)]);
}

CFX_RetainPtr<CFX_DIBSource> CPDF_TransferFunc::TranslateImage(
    const CFX_RetainPtr<CFX_DIBSource>& pSrc) {
  CFX_RetainPtr<CPDF_TransferFunc> pHolder(this);
  auto pDest = pdfium::MakeRetain<CPDF_DIBTransferFunc>(pHolder);
  pDest->LoadSrc(pSrc);
  return pDest;
}
