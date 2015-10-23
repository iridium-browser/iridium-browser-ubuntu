// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_INCLUDE_FORMFILLER_FFL_COMBOBOX_H_
#define FPDFSDK_INCLUDE_FORMFILLER_FFL_COMBOBOX_H_

#include "../../../core/include/fxcrt/fx_string.h"
#include "FFL_FormFiller.h"

class CBA_FontMap;
class CPDFSDK_Document;

struct FFL_ComboBoxState {
  int nIndex;
  int nStart;
  int nEnd;
  CFX_WideString sValue;
};

class CFFL_ComboBox : public CFFL_FormFiller,
                      public IPWL_FocusHandler,
                      public IPWL_Edit_Notify {
 public:
  CFFL_ComboBox(CPDFDoc_Environment* pApp, CPDFSDK_Annot* pWidget);
  ~CFFL_ComboBox() override;

  // CFFL_FormFiller:
  PWL_CREATEPARAM GetCreateParam() override;
  CPWL_Wnd* NewPDFWindow(const PWL_CREATEPARAM& cp,
                         CPDFSDK_PageView* pPageView) override;
  FX_BOOL OnChar(CPDFSDK_Annot* pAnnot, FX_UINT nChar, FX_UINT nFlags) override;
  FX_BOOL IsDataChanged(CPDFSDK_PageView* pPageView) override;
  void SaveData(CPDFSDK_PageView* pPageView) override;
  void GetActionData(CPDFSDK_PageView* pPageView,
                     CPDF_AAction::AActionType type,
                     PDFSDK_FieldAction& fa) override;
  void SetActionData(CPDFSDK_PageView* pPageView,
                     CPDF_AAction::AActionType type,
                     const PDFSDK_FieldAction& fa) override;
  FX_BOOL IsActionDataChanged(CPDF_AAction::AActionType type,
                              const PDFSDK_FieldAction& faOld,
                              const PDFSDK_FieldAction& faNew) override;
  void SaveState(CPDFSDK_PageView* pPageView) override;
  void RestoreState(CPDFSDK_PageView* pPageView) override;
  CPWL_Wnd* ResetPDFWindow(CPDFSDK_PageView* pPageView,
                           FX_BOOL bRestoreValue) override;
  void OnKeyStroke(FX_BOOL bKeyDown, FX_DWORD nFlag) override;

  // IPWL_FocusHandler:
  void OnSetFocus(CPWL_Wnd* pWnd) override;
  void OnKillFocus(CPWL_Wnd* pWnd) override;

  // IPWL_Edit_Notify:
  void OnAddUndo(CPWL_Edit* pEdit) override;

 private:
  CFX_WideString GetSelectExportText();

  CBA_FontMap* m_pFontMap;
  FFL_ComboBoxState m_State;
};

#endif  // FPDFSDK_INCLUDE_FORMFILLER_FFL_COMBOBOX_H_
