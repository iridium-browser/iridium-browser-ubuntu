// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/formfiller/FormFiller.h"
#include "../../include/formfiller/FFL_FormFiller.h"
#include "../../include/formfiller/FFL_ListBox.h"
//#include "../../include/formfiller/FFL_Module.h"
#include "../../include/formfiller/FFL_IFormFiller.h"
//#include "../../include/formfiller/FFL_Undo.h"
#include "../../include/formfiller/FFL_CBA_Fontmap.h"

#define FFL_DEFAULTLISTBOXFONTSIZE 12.0f

/* ------------------------------- CFFL_ListBox -------------------------------
 */

CFFL_ListBox::CFFL_ListBox(CPDFDoc_Environment* pApp, CPDFSDK_Annot* pWidget)
    : CFFL_FormFiller(pApp, pWidget), m_pFontMap(NULL) {}

CFFL_ListBox::~CFFL_ListBox() {
  delete m_pFontMap;
}

PWL_CREATEPARAM CFFL_ListBox::GetCreateParam() {
  PWL_CREATEPARAM cp = CFFL_FormFiller::GetCreateParam();

  ASSERT(m_pWidget != NULL);
  FX_DWORD dwFieldFlag = m_pWidget->GetFieldFlags();

  if (dwFieldFlag & FIELDFLAG_MULTISELECT) {
    cp.dwFlags |= PLBS_MULTIPLESEL;
  }

  cp.dwFlags |= PWS_VSCROLL;

  if (cp.dwFlags & PWS_AUTOFONTSIZE)
    cp.fFontSize = FFL_DEFAULTLISTBOXFONTSIZE;

  if (!m_pFontMap) {
    m_pFontMap = new CBA_FontMap(m_pWidget, m_pApp->GetSysHandler());
    m_pFontMap->Initial();
  }
  cp.pFontMap = m_pFontMap;

  return cp;
}

CPWL_Wnd* CFFL_ListBox::NewPDFWindow(const PWL_CREATEPARAM& cp,
                                     CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pWnd = new CPWL_ListBox();
  pWnd->AttachFFLData(this);
  pWnd->Create(cp);

  ASSERT(m_pApp != NULL);
  CFFL_IFormFiller* pIFormFiller = m_pApp->GetIFormFiller();
  pWnd->SetFillerNotify(pIFormFiller);

  for (int32_t i = 0, sz = m_pWidget->CountOptions(); i < sz; i++)
    pWnd->AddString(m_pWidget->GetOptionLabel(i).c_str());

  if (pWnd->HasFlag(PLBS_MULTIPLESEL)) {
    m_OriginSelections.clear();

    FX_BOOL bSetCaret = FALSE;
    for (int32_t i = 0, sz = m_pWidget->CountOptions(); i < sz; i++) {
      if (m_pWidget->IsOptionSelected(i)) {
        if (!bSetCaret) {
          pWnd->SetCaret(i);
          bSetCaret = TRUE;
        }
        pWnd->Select(i);
        m_OriginSelections.insert(i);
      }
    }
  } else {
    for (int i = 0, sz = m_pWidget->CountOptions(); i < sz; i++) {
      if (m_pWidget->IsOptionSelected(i)) {
        pWnd->Select(i);
        break;
      }
    }
  }

  pWnd->SetTopVisibleIndex(m_pWidget->GetTopVisibleIndex());

  return pWnd;
}

FX_BOOL CFFL_ListBox::OnChar(CPDFSDK_Annot* pAnnot,
                             FX_UINT nChar,
                             FX_UINT nFlags) {
  return CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
}

FX_BOOL CFFL_ListBox::IsDataChanged(CPDFSDK_PageView* pPageView) {
  CPWL_ListBox* pListBox = (CPWL_ListBox*)GetPDFWindow(pPageView, FALSE);
  if (!pListBox)
    return FALSE;

  if (m_pWidget->GetFieldFlags() & FIELDFLAG_MULTISELECT) {
    size_t nSelCount = 0;
    for (int32_t i = 0, sz = pListBox->GetCount(); i < sz; ++i) {
      if (pListBox->IsItemSelected(i)) {
        if (m_OriginSelections.count(i) == 0)
          return TRUE;

        ++nSelCount;
      }
    }

    return nSelCount != m_OriginSelections.size();
  }
  return pListBox->GetCurSel() != m_pWidget->GetSelectedIndex(0);
}

void CFFL_ListBox::SaveData(CPDFSDK_PageView* pPageView) {
  ASSERT(m_pWidget != NULL);

  if (CPWL_ListBox* pListBox = (CPWL_ListBox*)GetPDFWindow(pPageView, FALSE)) {
    CFX_IntArray aOldSelect, aNewSelect;

    {
      for (int i = 0, sz = m_pWidget->CountOptions(); i < sz; i++) {
        if (m_pWidget->IsOptionSelected(i)) {
          aOldSelect.Add(i);
        }
      }
    }

    int32_t nNewTopIndex = pListBox->GetTopVisibleIndex();

    m_pWidget->ClearSelection(FALSE);

    if (m_pWidget->GetFieldFlags() & FIELDFLAG_MULTISELECT) {
      for (int32_t i = 0, sz = pListBox->GetCount(); i < sz; i++) {
        if (pListBox->IsItemSelected(i)) {
          m_pWidget->SetOptionSelection(i, TRUE, FALSE);
          aNewSelect.Add(i);
        }
      }
    } else {
      m_pWidget->SetOptionSelection(pListBox->GetCurSel(), TRUE, FALSE);
      aNewSelect.Add(pListBox->GetCurSel());
    }

    m_pWidget->SetTopVisibleIndex(nNewTopIndex);
    m_pWidget->ResetFieldAppearance(TRUE);
    m_pWidget->UpdateField();
    SetChangeMark();
  }
}

void CFFL_ListBox::GetActionData(CPDFSDK_PageView* pPageView,
                                 CPDF_AAction::AActionType type,
                                 PDFSDK_FieldAction& fa) {
  switch (type) {
    case CPDF_AAction::Validate:
      if (m_pWidget->GetFieldFlags() & FIELDFLAG_MULTISELECT) {
        fa.sValue = L"";
      } else {
        if (CPWL_ListBox* pListBox =
                (CPWL_ListBox*)GetPDFWindow(pPageView, FALSE)) {
          ASSERT(m_pWidget != NULL);
          int32_t nCurSel = pListBox->GetCurSel();
          if (nCurSel >= 0)
            fa.sValue = m_pWidget->GetOptionLabel(nCurSel);
        }
      }
      break;
    case CPDF_AAction::LoseFocus:
    case CPDF_AAction::GetFocus:
      if (m_pWidget->GetFieldFlags() & FIELDFLAG_MULTISELECT) {
        fa.sValue = L"";
      } else {
        ASSERT(m_pWidget != NULL);
        int32_t nCurSel = m_pWidget->GetSelectedIndex(0);
        if (nCurSel >= 0)
          fa.sValue = m_pWidget->GetOptionLabel(nCurSel);
      }
      break;
    default:
      break;
  }
}

void CFFL_ListBox::SetActionData(CPDFSDK_PageView* pPageView,
                                 CPDF_AAction::AActionType type,
                                 const PDFSDK_FieldAction& fa) {}

void CFFL_ListBox::SaveState(CPDFSDK_PageView* pPageView) {
  ASSERT(pPageView != NULL);

  if (CPWL_ListBox* pListBox = (CPWL_ListBox*)GetPDFWindow(pPageView, FALSE)) {
    for (int32_t i = 0, sz = pListBox->GetCount(); i < sz; i++) {
      if (pListBox->IsItemSelected(i)) {
        m_State.Add(i);
      }
    }
  }
}

void CFFL_ListBox::RestoreState(CPDFSDK_PageView* pPageView) {
  if (CPWL_ListBox* pListBox = (CPWL_ListBox*)GetPDFWindow(pPageView, FALSE)) {
    for (int i = 0, sz = m_State.GetSize(); i < sz; i++)
      pListBox->Select(m_State[i]);
  }
}

CPWL_Wnd* CFFL_ListBox::ResetPDFWindow(CPDFSDK_PageView* pPageView,
                                       FX_BOOL bRestoreValue) {
  if (bRestoreValue)
    SaveState(pPageView);

  DestroyPDFWindow(pPageView);

  CPWL_Wnd* pRet = NULL;

  if (bRestoreValue) {
    RestoreState(pPageView);
    pRet = GetPDFWindow(pPageView, FALSE);
  } else
    pRet = GetPDFWindow(pPageView, TRUE);

  m_pWidget->UpdateField();

  return pRet;
}

void CFFL_ListBox::OnKeyStroke(FX_BOOL bKeyDown, FX_DWORD nFlag) {
  ASSERT(m_pWidget != NULL);

  int nFlags = m_pWidget->GetFieldFlags();

  if (nFlags & FIELDFLAG_COMMITONSELCHANGE) {
    if (m_bValid) {
      CPDFSDK_PageView* pPageView = GetCurPageView();
      ASSERT(pPageView != NULL);

      if (CommitData(pPageView, nFlag)) {
        DestroyPDFWindow(pPageView);
        m_bValid = FALSE;
      }
    }
  }
}
