// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_arraynodelist.h"

#include <vector>

#include "third_party/base/stl_util.h"

CXFA_ArrayNodeList::CXFA_ArrayNodeList(CXFA_Document* pDocument)
    : CXFA_NodeList(pDocument) {}

CXFA_ArrayNodeList::~CXFA_ArrayNodeList() {}

void CXFA_ArrayNodeList::SetArrayNodeList(
    const std::vector<CXFA_Node*>& srcArray) {
  if (!srcArray.empty())
    m_array = srcArray;
}

int32_t CXFA_ArrayNodeList::GetLength() {
  return pdfium::CollectionSize<int32_t>(m_array);
}

bool CXFA_ArrayNodeList::Append(CXFA_Node* pNode) {
  m_array.push_back(pNode);
  return true;
}

bool CXFA_ArrayNodeList::Insert(CXFA_Node* pNewNode, CXFA_Node* pBeforeNode) {
  if (!pBeforeNode) {
    m_array.push_back(pNewNode);
  } else {
    auto it = std::find(m_array.begin(), m_array.end(), pBeforeNode);
    if (it != m_array.end())
      m_array.insert(it, pNewNode);
  }
  return true;
}

bool CXFA_ArrayNodeList::Remove(CXFA_Node* pNode) {
  auto it = std::find(m_array.begin(), m_array.end(), pNode);
  if (it != m_array.end())
    m_array.erase(it);
  return true;
}

CXFA_Node* CXFA_ArrayNodeList::Item(int32_t iIndex) {
  int32_t iSize = pdfium::CollectionSize<int32_t>(m_array);
  return (iIndex >= 0 && iIndex < iSize) ? m_array[iIndex] : nullptr;
}
