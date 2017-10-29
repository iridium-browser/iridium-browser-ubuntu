// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_STRUCTTREE_H_
#define CORE_FPDFDOC_CPDF_STRUCTTREE_H_

#include <map>
#include <memory>
#include <vector>

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/cfx_unowned_ptr.h"

class CPDF_Dictionary;
class CPDF_Document;
class CPDF_StructElement;

class CPDF_StructTree {
 public:
  static std::unique_ptr<CPDF_StructTree> LoadPage(
      const CPDF_Document* pDoc,
      const CPDF_Dictionary* pPageDict);

  explicit CPDF_StructTree(const CPDF_Document* pDoc);
  ~CPDF_StructTree();

  int CountTopElements() const;
  CPDF_StructElement* GetTopElement(int i) const;
  const CPDF_Dictionary* GetRoleMap() const { return m_pRoleMap.Get(); }
  const CPDF_Dictionary* GetPage() const { return m_pPage.Get(); }
  const CPDF_Dictionary* GetTreeRoot() const { return m_pTreeRoot.Get(); }

 private:
  void LoadPageTree(const CPDF_Dictionary* pPageDict);
  CFX_RetainPtr<CPDF_StructElement> AddPageNode(
      CPDF_Dictionary* pElement,
      std::map<CPDF_Dictionary*, CFX_RetainPtr<CPDF_StructElement>>* map,
      int nLevel);
  bool AddTopLevelNode(CPDF_Dictionary* pDict,
                       const CFX_RetainPtr<CPDF_StructElement>& pElement);

  CFX_UnownedPtr<const CPDF_Dictionary> const m_pTreeRoot;
  CFX_UnownedPtr<const CPDF_Dictionary> const m_pRoleMap;
  CFX_UnownedPtr<const CPDF_Dictionary> m_pPage;
  std::vector<CFX_RetainPtr<CPDF_StructElement>> m_Kids;
};

#endif  // CORE_FPDFDOC_CPDF_STRUCTTREE_H_
