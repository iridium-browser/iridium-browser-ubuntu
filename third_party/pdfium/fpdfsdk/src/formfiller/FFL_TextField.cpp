// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/formfiller/FFL_TextField.h"
#include "../../include/formfiller/FFL_CBA_Fontmap.h"

/* ------------------------------- CFFL_TextField
 * ------------------------------- */

CFFL_TextField::CFFL_TextField(CPDFDoc_Environment* pApp, CPDFSDK_Annot* pAnnot)
    : CFFL_FormFiller(pApp, pAnnot),
      m_pFontMap(NULL)  //,
// m_pSpellCheck(NULL)
{
  m_State.nStart = m_State.nEnd = 0;
}

CFFL_TextField::~CFFL_TextField() {
  delete m_pFontMap;
}

PWL_CREATEPARAM CFFL_TextField::GetCreateParam() {
  PWL_CREATEPARAM cp = CFFL_FormFiller::GetCreateParam();

  ASSERT(m_pWidget != NULL);
  int nFlags = m_pWidget->GetFieldFlags();

  if (nFlags & FIELDFLAG_PASSWORD) {
    cp.dwFlags |= PES_PASSWORD;
  }

  if (!(nFlags & FIELDFLAG_DONOTSPELLCHECK)) {
  }

  if (nFlags & FIELDFLAG_MULTILINE) {
    cp.dwFlags |= PES_MULTILINE | PES_AUTORETURN | PES_TOP;

    if (!(nFlags & FIELDFLAG_DONOTSCROLL)) {
      cp.dwFlags |= PWS_VSCROLL | PES_AUTOSCROLL;
    }
  } else {
    cp.dwFlags |= PES_CENTER;

    if (!(nFlags & FIELDFLAG_DONOTSCROLL)) {
      cp.dwFlags |= PES_AUTOSCROLL;
    }
  }

  if (nFlags & FIELDFLAG_COMB) {
    cp.dwFlags |= PES_CHARARRAY;
  }

  if (nFlags & FIELDFLAG_RICHTEXT) {
    cp.dwFlags |= PES_RICH;
  }

  cp.dwFlags |= PES_UNDO;

  switch (m_pWidget->GetAlignment()) {
    default:
    case BF_ALIGN_LEFT:
      cp.dwFlags |= PES_LEFT;
      break;
    case BF_ALIGN_MIDDLE:
      cp.dwFlags |= PES_MIDDLE;
      break;
    case BF_ALIGN_RIGHT:
      cp.dwFlags |= PES_RIGHT;
      break;
  }

  if (!m_pFontMap) {
    m_pFontMap = new CBA_FontMap(m_pWidget, m_pApp->GetSysHandler());
    m_pFontMap->Initial();
  }
  cp.pFontMap = m_pFontMap;
  cp.pFocusHandler = this;

  return cp;
}

CPWL_Wnd* CFFL_TextField::NewPDFWindow(const PWL_CREATEPARAM& cp,
                                       CPDFSDK_PageView* pPageView) {
  CPWL_Edit* pWnd = new CPWL_Edit();
  pWnd->AttachFFLData(this);
  pWnd->Create(cp);

  ASSERT(m_pApp != NULL);
  CFFL_IFormFiller* pIFormFiller = m_pApp->GetIFormFiller();
  pWnd->SetFillerNotify(pIFormFiller);

  ASSERT(m_pWidget != NULL);
  int32_t nMaxLen = m_pWidget->GetMaxLen();
  CFX_WideString swValue = m_pWidget->GetValue();

  if (nMaxLen > 0) {
    if (pWnd->HasFlag(PES_CHARARRAY)) {
      pWnd->SetCharArray(nMaxLen);
      pWnd->SetAlignFormatV(PEAV_CENTER);
    } else {
      pWnd->SetLimitChar(nMaxLen);
    }
  }

  pWnd->SetText(swValue.c_str());
  return pWnd;
}

FX_BOOL CFFL_TextField::OnChar(CPDFSDK_Annot* pAnnot,
                               FX_UINT nChar,
                               FX_UINT nFlags) {
  switch (nChar) {
    case FWL_VKEY_Return:
      if (!(m_pWidget->GetFieldFlags() & FIELDFLAG_MULTILINE)) {
        CPDFSDK_PageView* pPageView = GetCurPageView();
        ASSERT(pPageView != NULL);
        m_bValid = !m_bValid;
        CPDF_Rect rcAnnot = pAnnot->GetRect();
        m_pApp->FFI_Invalidate(pAnnot->GetPDFPage(), rcAnnot.left, rcAnnot.top,
                               rcAnnot.right, rcAnnot.bottom);

        if (m_bValid) {
          if (CPWL_Wnd* pWnd = GetPDFWindow(pPageView, TRUE))
            pWnd->SetFocus();
        } else {
          if (CommitData(pPageView, nFlags)) {
            DestroyPDFWindow(pPageView);
            return TRUE;
          }
          return FALSE;
        }
      }
      break;
    case FWL_VKEY_Escape: {
      CPDFSDK_PageView* pPageView = GetCurPageView();
      ASSERT(pPageView != NULL);
      EscapeFiller(pPageView, TRUE);
      return TRUE;
    }
  }

  return CFFL_FormFiller::OnChar(pAnnot, nChar, nFlags);
}

FX_BOOL CFFL_TextField::IsDataChanged(CPDFSDK_PageView* pPageView) {
  ASSERT(m_pWidget != NULL);

  if (CPWL_Edit* pEdit = (CPWL_Edit*)GetPDFWindow(pPageView, FALSE))
    return pEdit->GetText() != m_pWidget->GetValue();

  return FALSE;
}

void CFFL_TextField::SaveData(CPDFSDK_PageView* pPageView) {
  ASSERT(m_pWidget != NULL);

  if (CPWL_Edit* pWnd = (CPWL_Edit*)GetPDFWindow(pPageView, FALSE)) {
    CFX_WideString sOldValue = m_pWidget->GetValue();
    CFX_WideString sNewValue = pWnd->GetText();

    m_pWidget->SetValue(sNewValue, FALSE);
    m_pWidget->ResetFieldAppearance(TRUE);
    m_pWidget->UpdateField();
    SetChangeMark();
  }
}

void CFFL_TextField::GetActionData(CPDFSDK_PageView* pPageView,
                                   CPDF_AAction::AActionType type,
                                   PDFSDK_FieldAction& fa) {
  switch (type) {
    case CPDF_AAction::KeyStroke:
      if (CPWL_Edit* pWnd = (CPWL_Edit*)GetPDFWindow(pPageView, FALSE)) {
        fa.bFieldFull = pWnd->IsTextFull();

        fa.sValue = pWnd->GetText();

        if (fa.bFieldFull) {
          fa.sChange = L"";
          fa.sChangeEx = L"";
        }
      }
      break;
    case CPDF_AAction::Validate:
      if (CPWL_Edit* pWnd = (CPWL_Edit*)GetPDFWindow(pPageView, FALSE)) {
        fa.sValue = pWnd->GetText();
      }
      break;
    case CPDF_AAction::LoseFocus:
    case CPDF_AAction::GetFocus:
      ASSERT(m_pWidget != NULL);
      fa.sValue = m_pWidget->GetValue();
      break;
    default:
      break;
  }
}

void CFFL_TextField::SetActionData(CPDFSDK_PageView* pPageView,
                                   CPDF_AAction::AActionType type,
                                   const PDFSDK_FieldAction& fa) {
  switch (type) {
    case CPDF_AAction::KeyStroke:
      if (CPWL_Edit* pEdit = (CPWL_Edit*)GetPDFWindow(pPageView, FALSE)) {
        pEdit->SetFocus();
        pEdit->SetSel(fa.nSelStart, fa.nSelEnd);
        pEdit->ReplaceSel(fa.sChange.c_str());
      }
      break;
    default:
      break;
  }
}

FX_BOOL CFFL_TextField::IsActionDataChanged(CPDF_AAction::AActionType type,
                                            const PDFSDK_FieldAction& faOld,
                                            const PDFSDK_FieldAction& faNew) {
  switch (type) {
    case CPDF_AAction::KeyStroke:
      return (!faOld.bFieldFull && faOld.nSelEnd != faNew.nSelEnd) ||
             faOld.nSelStart != faNew.nSelStart ||
             faOld.sChange != faNew.sChange;
    default:
      break;
  }

  return FALSE;
}

void CFFL_TextField::SaveState(CPDFSDK_PageView* pPageView) {
  ASSERT(pPageView != NULL);

  if (CPWL_Edit* pWnd = (CPWL_Edit*)GetPDFWindow(pPageView, FALSE)) {
    pWnd->GetSel(m_State.nStart, m_State.nEnd);
    m_State.sValue = pWnd->GetText();
  }
}

void CFFL_TextField::RestoreState(CPDFSDK_PageView* pPageView) {
  ASSERT(pPageView != NULL);

  if (CPWL_Edit* pWnd = (CPWL_Edit*)GetPDFWindow(pPageView, TRUE)) {
    pWnd->SetText(m_State.sValue.c_str());
    pWnd->SetSel(m_State.nStart, m_State.nEnd);
  }
}

CPWL_Wnd* CFFL_TextField::ResetPDFWindow(CPDFSDK_PageView* pPageView,
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

void CFFL_TextField::OnSetFocus(CPWL_Wnd* pWnd) {
  ASSERT(m_pApp != NULL);

  ASSERT(pWnd != NULL);

  if (pWnd->GetClassName() == PWL_CLASSNAME_EDIT) {
    CPWL_Edit* pEdit = (CPWL_Edit*)pWnd;
    pEdit->SetCharSet(134);
    pEdit->SetCodePage(936);

    pEdit->SetReadyToInput();
    CFX_WideString wsText = pEdit->GetText();
    int nCharacters = wsText.GetLength();
    CFX_ByteString bsUTFText = wsText.UTF16LE_Encode();
    unsigned short* pBuffer = (unsigned short*)bsUTFText.c_str();
    m_pApp->FFI_OnSetFieldInputFocus(m_pWidget->GetFormField(), pBuffer,
                                     nCharacters, TRUE);

    pEdit->SetEditNotify(this);
  }
}

void CFFL_TextField::OnKillFocus(CPWL_Wnd* pWnd) {}

void CFFL_TextField::OnAddUndo(CPWL_Edit* pEdit) {}
