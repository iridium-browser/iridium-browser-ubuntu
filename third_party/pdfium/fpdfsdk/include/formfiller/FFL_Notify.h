// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_INCLUDE_FORMFILLER_FFL_NOTIFY_H_
#define FPDFSDK_INCLUDE_FORMFILLER_FFL_NOTIFY_H_

#include "../../../core/include/fpdfdoc/fpdf_doc.h"
#include "../../../core/include/fxcrt/fx_string.h"

class CFFL_FormFiller;
class CPDF_FormField;

class CFFL_Notify {
 public:
  CFFL_Notify(CFFL_FormFiller* pFormFiller);
  virtual ~CFFL_Notify();

 public:
  FX_BOOL OnSetFocus(FX_BOOL& bExit);
  FX_BOOL OnMouseEnter(FX_BOOL& bExit);
  FX_BOOL OnMouseDown(FX_BOOL& bExit);
  FX_BOOL OnMouseUp(FX_BOOL& bExit);
  FX_BOOL OnMouseExit(FX_BOOL& bExit);
  FX_BOOL OnKillFocus(FX_BOOL& bExit);

  FX_BOOL OnCalculate();
  FX_BOOL OnFormat(int iCommitKey);
  FX_BOOL OnValidate(CPDF_FormField* pFormField,
                     CFX_WideString& strValue,
                     CFX_WideString& strChange,
                     const CFX_WideString& strChangeEx,
                     FX_BOOL bKeyDown,
                     FX_BOOL bModifier,
                     FX_BOOL bShift,
                     FX_BOOL& bRC);
  FX_BOOL OnKeyStroke(CPDF_FormField* pFormField,
                      int nCommitKey,
                      CFX_WideString& strValue,
                      CFX_WideString& strChange,
                      const CFX_WideString& strChangeEx,
                      FX_BOOL bKeyDown,
                      FX_BOOL bModifier,
                      FX_BOOL bShift,
                      FX_BOOL bWillCommit,
                      FX_BOOL bFieldFull,
                      int& nSelStart,
                      int& nSelEnd,
                      FX_BOOL& bRC);

  void BeforeNotify();
  void AfterNotify();
  FX_BOOL IsNotifying() const { return m_nNotifyFlag > 0; }

 private:
  FX_BOOL DoAAction(CPDF_AAction::AActionType eAAT, FX_BOOL& bExit);
  FX_BOOL FindAAction(CPDF_AAction::AActionType eAAT, CPDF_Action& action);
  FX_BOOL FindAAction(CPDF_AAction aaction,
                      CPDF_AAction::AActionType eAAT,
                      CPDF_Action& action);
  FX_BOOL ExecuteActionTree(CPDF_AAction::AActionType eAAT,
                            CPDF_Action& action,
                            FX_BOOL& bExit);
  FX_BOOL ExecuteAction(CPDF_AAction::AActionType eAAT,
                        CPDF_Action& action,
                        FX_BOOL& bExit);

  FX_BOOL m_bDoActioning;
  int32_t m_nNotifyFlag;
};

#endif  // FPDFSDK_INCLUDE_FORMFILLER_FFL_NOTIFY_H_
