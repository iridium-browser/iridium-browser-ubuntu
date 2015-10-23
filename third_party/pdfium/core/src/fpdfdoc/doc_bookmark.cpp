// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <vector>

#include "../../../third_party/base/nonstd_unique_ptr.h"
#include "../../include/fpdfdoc/fpdf_doc.h"

CPDF_Bookmark CPDF_BookmarkTree::GetFirstChild(
    const CPDF_Bookmark& parent) const {
  if (!parent.m_pDict) {
    CPDF_Dictionary* pRoot = m_pDocument->GetRoot()->GetDict("Outlines");
    if (!pRoot) {
      return CPDF_Bookmark();
    }
    return CPDF_Bookmark(pRoot->GetDict("First"));
  }
  return CPDF_Bookmark(parent.m_pDict->GetDict("First"));
}
CPDF_Bookmark CPDF_BookmarkTree::GetNextSibling(
    const CPDF_Bookmark& bookmark) const {
  if (!bookmark.m_pDict) {
    return CPDF_Bookmark();
  }
  CPDF_Dictionary* pNext = bookmark.m_pDict->GetDict("Next");
  return pNext == bookmark.m_pDict ? CPDF_Bookmark() : CPDF_Bookmark(pNext);
}
FX_DWORD CPDF_Bookmark::GetColorRef() const {
  if (!m_pDict) {
    return 0;
  }
  CPDF_Array* pColor = m_pDict->GetArray("C");
  if (!pColor) {
    return FXSYS_RGB(0, 0, 0);
  }
  int r = FXSYS_round(pColor->GetNumber(0) * 255);
  int g = FXSYS_round(pColor->GetNumber(1) * 255);
  int b = FXSYS_round(pColor->GetNumber(2) * 255);
  return FXSYS_RGB(r, g, b);
}
FX_DWORD CPDF_Bookmark::GetFontStyle() const {
  if (!m_pDict) {
    return 0;
  }
  return m_pDict->GetInteger("F");
}
CFX_WideString CPDF_Bookmark::GetTitle() const {
  if (!m_pDict) {
    return CFX_WideString();
  }
  CPDF_String* pString = (CPDF_String*)m_pDict->GetElementValue("Title");
  if (!pString || pString->GetType() != PDFOBJ_STRING) {
    return CFX_WideString();
  }
  CFX_WideString title = pString->GetUnicodeText();
  int len = title.GetLength();
  if (!len) {
    return CFX_WideString();
  }
  nonstd::unique_ptr<FX_WCHAR[]> buf(new FX_WCHAR[len]);
  for (int i = 0; i < len; i++) {
    FX_WCHAR w = title[i];
    buf[i] = w > 0x20 ? w : 0x20;
  }
  return CFX_WideString(buf.get(), len);
}
CPDF_Dest CPDF_Bookmark::GetDest(CPDF_Document* pDocument) const {
  if (!m_pDict) {
    return CPDF_Dest();
  }
  CPDF_Object* pDest = m_pDict->GetElementValue("Dest");
  if (!pDest) {
    return CPDF_Dest();
  }
  if (pDest->GetType() == PDFOBJ_STRING || pDest->GetType() == PDFOBJ_NAME) {
    CPDF_NameTree name_tree(pDocument, FX_BSTRC("Dests"));
    CFX_ByteStringC name = pDest->GetString();
    return CPDF_Dest(name_tree.LookupNamedDest(pDocument, name));
  }
  if (pDest->GetType() == PDFOBJ_ARRAY) {
    return CPDF_Dest((CPDF_Array*)pDest);
  }
  return CPDF_Dest();
}
CPDF_Action CPDF_Bookmark::GetAction() const {
  if (!m_pDict) {
    return CPDF_Action();
  }
  return CPDF_Action(m_pDict->GetDict("A"));
}
