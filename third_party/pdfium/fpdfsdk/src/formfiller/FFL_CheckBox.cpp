// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/formfiller/FormFiller.h"
#include "../../include/formfiller/FFL_FormFiller.h"
#include "../../include/formfiller/FFL_CheckBox.h"

/* ------------------------------- CFFL_CheckBox -------------------------------
 */

CFFL_CheckBox::CFFL_CheckBox(CPDFDoc_Environment* pApp, CPDFSDK_Widget* pWidget)
    : CFFL_Button(pApp, pWidget) {}

CFFL_CheckBox::~CFFL_CheckBox() {}

CPWL_Wnd* CFFL_CheckBox::NewPDFWindow(const PWL_CREATEPARAM& cp,
                                      CPDFSDK_PageView* pPageView) {
  CPWL_CheckBox* pWnd = new CPWL_CheckBox();
  pWnd->Create(cp);

  ASSERT(m_pWidget != NULL);
  pWnd->SetCheck(m_pWidget->IsChecked());

  return pWnd;
}

FX_BOOL CFFL_CheckBox::OnKeyDown(CPDFSDK_Annot* pAnnot,
                                 FX_UINT nKeyCode,
                                 FX_UINT nFlags) {
  switch (nKeyCode) {
    case FWL_VKEY_Return:
    case FWL_VKEY_Space:
      return TRUE;
    default:
      return CFFL_FormFiller::OnKeyDown(pAnnot, nKeyCode, nFlags);
  }
}
FX_BOOL CFFL_CheckBox::OnChar(CPDFSDK_Annot* pAnnot,
                              FX_UINT nChar,
                              FX_UINT nFlags) {
  switch (nChar) {
    case FWL_VKEY_Return:
    case FWL_VKEY_Space: {
      CFFL_IFormFiller* pIFormFiller = m_pApp->GetIFormFiller();
      ASSERT(pIFormFiller != NULL);

      CPDFSDK_PageView* pPageView = pAnnot->GetPageView();
      ASSERT(pPageView != NULL);

      FX_BOOL bReset = FALSE;
      FX_BOOL bExit = FALSE;

      pIFormFiller->OnButtonUp(m_pWidget, pPageView, bReset, bExit, nFlags);

      if (bReset)
        return TRUE;
      if (bExit)
        return TRUE;

      CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);

      if (CPWL_CheckBox* pWnd = (CPWL_CheckBox*)GetPDFWindow(pPageView, TRUE))
        pWnd->SetCheck(!pWnd->IsChecked());

      CommitData(pPageView, nFlags);
      return TRUE;
    }
    default:
      return CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
  }
}

FX_BOOL CFFL_CheckBox::OnLButtonUp(CPDFSDK_PageView* pPageView,
                                   CPDFSDK_Annot* pAnnot,
                                   FX_UINT nFlags,
                                   const CPDF_Point& point) {
  CFFL_Button::OnLButtonUp(pPageView, pAnnot, nFlags, point);

  if (IsValid()) {
    if (CPWL_CheckBox* pWnd = (CPWL_CheckBox*)GetPDFWindow(pPageView, TRUE)) {
      CPDFSDK_Widget* pWidget = (CPDFSDK_Widget*)pAnnot;
      pWnd->SetCheck(!pWidget->IsChecked());
      //	pWnd->SetCheck(!pWnd->IsChecked());
    }

    if (!CommitData(pPageView, nFlags))
      return FALSE;
  }

  return TRUE;
}

FX_BOOL CFFL_CheckBox::IsDataChanged(CPDFSDK_PageView* pPageView) {
  ASSERT(m_pWidget != NULL);

  if (CPWL_CheckBox* pWnd = (CPWL_CheckBox*)GetPDFWindow(pPageView, FALSE)) {
    return pWnd->IsChecked() != m_pWidget->IsChecked();
  }

  return FALSE;
}

void CFFL_CheckBox::SaveData(CPDFSDK_PageView* pPageView) {
  ASSERT(m_pWidget != NULL);

  if (CPWL_CheckBox* pWnd = (CPWL_CheckBox*)GetPDFWindow(pPageView, FALSE)) {
    FX_BOOL bNewChecked = pWnd->IsChecked();

    if (bNewChecked) {
      CPDF_FormField* pField = m_pWidget->GetFormField();
      ASSERT(pField != NULL);

      for (int32_t i = 0, sz = pField->CountControls(); i < sz; i++) {
        if (CPDF_FormControl* pCtrl = pField->GetControl(i)) {
          if (pCtrl->IsChecked()) {
            break;
          }
        }
      }
    }

    m_pWidget->SetCheck(bNewChecked, FALSE);
    m_pWidget->UpdateField();
    SetChangeMark();
  }
}
