// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfdoc/include/cpdf_bookmarktree.h"

#include "core/fpdfapi/fpdf_parser/include/cpdf_document.h"

CPDF_Bookmark CPDF_BookmarkTree::GetFirstChild(
    const CPDF_Bookmark& parent) const {
  CPDF_Dictionary* pParentDict = parent.GetDict();
  if (pParentDict)
    return CPDF_Bookmark(pParentDict->GetDictBy("First"));

  CPDF_Dictionary* pRoot = m_pDocument->GetRoot();
  if (!pRoot)
    return CPDF_Bookmark();

  CPDF_Dictionary* pOutlines = pRoot->GetDictBy("Outlines");
  return pOutlines ? CPDF_Bookmark(pOutlines->GetDictBy("First"))
                   : CPDF_Bookmark();
}

CPDF_Bookmark CPDF_BookmarkTree::GetNextSibling(
    const CPDF_Bookmark& bookmark) const {
  CPDF_Dictionary* pDict = bookmark.GetDict();
  if (!pDict)
    return CPDF_Bookmark();

  CPDF_Dictionary* pNext = pDict->GetDictBy("Next");
  return pNext == pDict ? CPDF_Bookmark() : CPDF_Bookmark(pNext);
}
