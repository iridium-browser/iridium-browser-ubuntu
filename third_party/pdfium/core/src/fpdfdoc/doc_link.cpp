// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/fpdfdoc/fpdf_doc.h"

CPDF_LinkList::CPDF_LinkList() {
}

CPDF_LinkList::~CPDF_LinkList() {
}

const std::vector<CPDF_Dictionary*>* CPDF_LinkList::GetPageLinks(
    CPDF_Page* pPage) {
  FX_DWORD objnum = pPage->m_pFormDict->GetObjNum();
  if (objnum == 0)
    return nullptr;

  auto it = m_PageMap.find(objnum);
  if (it != m_PageMap.end())
    return &it->second;

  // std::map::operator[] forces the creation of a map entry.
  std::vector<CPDF_Dictionary*>& page_link_list = m_PageMap[objnum];
  LoadPageLinks(pPage, &page_link_list);
  return &page_link_list;
}

CPDF_Link CPDF_LinkList::GetLinkAtPoint(CPDF_Page* pPage,
                                        FX_FLOAT pdf_x,
                                        FX_FLOAT pdf_y,
                                        int* z_order) {
  const std::vector<CPDF_Dictionary*>* pPageLinkList = GetPageLinks(pPage);
  if (!pPageLinkList)
    return CPDF_Link();

  for (size_t i = pPageLinkList->size(); i > 0; --i) {
    size_t annot_index = i - 1;
    CPDF_Dictionary* pAnnot = (*pPageLinkList)[annot_index];
    if (!pAnnot)
      continue;

    CPDF_Link link(pAnnot);
    CPDF_Rect rect = link.GetRect();
    if (!rect.Contains(pdf_x, pdf_y))
      continue;

    if (z_order)
      *z_order = annot_index;
    return link;
  }
  return CPDF_Link();
}

void CPDF_LinkList::LoadPageLinks(CPDF_Page* pPage,
                                  std::vector<CPDF_Dictionary*>* pList) {
  CPDF_Array* pAnnotList = pPage->m_pFormDict->GetArray("Annots");
  if (!pAnnotList)
    return;

  for (FX_DWORD i = 0; i < pAnnotList->GetCount(); ++i) {
    CPDF_Dictionary* pAnnot = pAnnotList->GetDict(i);
    bool add_link = (pAnnot && pAnnot->GetString("Subtype") == "Link");
    // Add non-links as nullptrs to preserve z-order.
    pList->push_back(add_link ? pAnnot : nullptr);
  }
}

CPDF_Rect CPDF_Link::GetRect() {
  return m_pDict->GetRect("Rect");
}
CPDF_Dest CPDF_Link::GetDest(CPDF_Document* pDoc) {
  CPDF_Object* pDest = m_pDict->GetElementValue("Dest");
  if (pDest == NULL) {
    return CPDF_Dest();
  }
  if (pDest->GetType() == PDFOBJ_STRING || pDest->GetType() == PDFOBJ_NAME) {
    CPDF_NameTree name_tree(pDoc, FX_BSTRC("Dests"));
    CFX_ByteStringC name = pDest->GetString();
    return CPDF_Dest(name_tree.LookupNamedDest(pDoc, name));
  }
  if (pDest->GetType() == PDFOBJ_ARRAY) {
    return CPDF_Dest((CPDF_Array*)pDest);
  }
  return CPDF_Dest();
}
CPDF_Action CPDF_Link::GetAction() {
  return CPDF_Action(m_pDict->GetDict("A"));
}
