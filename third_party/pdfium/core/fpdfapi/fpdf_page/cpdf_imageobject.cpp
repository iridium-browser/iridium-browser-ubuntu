// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/fpdf_page/include/cpdf_imageobject.h"

#include <memory>

#include "core/fpdfapi/fpdf_page/include/cpdf_image.h"
#include "core/fpdfapi/fpdf_page/pageint.h"
#include "core/fpdfapi/fpdf_parser/include/cpdf_document.h"

CPDF_ImageObject::CPDF_ImageObject()
    : m_pImage(nullptr), m_pImageOwned(false) {}

CPDF_ImageObject::~CPDF_ImageObject() {
  Release();
}

CPDF_ImageObject* CPDF_ImageObject::Clone() const {
  CPDF_ImageObject* obj = new CPDF_ImageObject;
  obj->CopyData(this);

  obj->m_pImage = m_pImage->Clone();
  obj->m_Matrix = m_Matrix;
  return obj;
}

CPDF_PageObject::Type CPDF_ImageObject::GetType() const {
  return IMAGE;
}

void CPDF_ImageObject::Transform(const CFX_Matrix& matrix) {
  m_Matrix.Concat(matrix);
  CalcBoundingBox();
}

bool CPDF_ImageObject::IsImage() const {
  return true;
}

CPDF_ImageObject* CPDF_ImageObject::AsImage() {
  return this;
}

const CPDF_ImageObject* CPDF_ImageObject::AsImage() const {
  return this;
}

void CPDF_ImageObject::CalcBoundingBox() {
  m_Left = m_Bottom = 0;
  m_Right = m_Top = 1.0f;
  m_Matrix.TransformRect(m_Left, m_Right, m_Top, m_Bottom);
}

void CPDF_ImageObject::SetOwnedImage(std::unique_ptr<CPDF_Image> pImage) {
  Release();
  m_pImage = pImage.release();
  m_pImageOwned = true;
}

void CPDF_ImageObject::SetUnownedImage(CPDF_Image* pImage) {
  Release();
  m_pImage = pImage;
  m_pImageOwned = false;
}

void CPDF_ImageObject::Release() {
  if (m_pImageOwned) {
    delete m_pImage;
    m_pImage = nullptr;
    m_pImageOwned = false;
    return;
  }

  if (!m_pImage)
    return;

  CPDF_DocPageData* pPageData = m_pImage->GetDocument()->GetPageData();
  pPageData->ReleaseImage(m_pImage->GetStream());
  m_pImage = nullptr;
}
