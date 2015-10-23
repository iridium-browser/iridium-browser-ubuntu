// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_INCLUDE_FORMFILLER_FFL_IFORMFILLER_H_
#define FPDFSDK_INCLUDE_FORMFILLER_FFL_IFORMFILLER_H_

#include <map>

#include "FormFiller.h"

class CFFL_FormFiller;
class CFFL_PrivateData;

class CFFL_IFormFiller : public IPWL_Filler_Notify {
 public:
  explicit CFFL_IFormFiller(CPDFDoc_Environment* pApp);
  ~CFFL_IFormFiller() override;

  virtual FX_BOOL Annot_HitTest(CPDFSDK_PageView* pPageView,
                                CPDFSDK_Annot* pAnnot,
                                CPDF_Point point);
  virtual FX_RECT GetViewBBox(CPDFSDK_PageView* pPageView,
                              CPDFSDK_Annot* pAnnot);
  virtual void OnDraw(CPDFSDK_PageView* pPageView,
                      CPDFSDK_Annot* pAnnot,
                      CFX_RenderDevice* pDevice,
                      CPDF_Matrix* pUser2Device,
                      FX_DWORD dwFlags);

  virtual void OnCreate(CPDFSDK_Annot* pAnnot);
  virtual void OnLoad(CPDFSDK_Annot* pAnnot);
  virtual void OnDelete(CPDFSDK_Annot* pAnnot);

  virtual void OnMouseEnter(CPDFSDK_PageView* pPageView,
                            CPDFSDK_Annot* pAnnot,
                            FX_UINT nFlag);
  virtual void OnMouseExit(CPDFSDK_PageView* pPageView,
                           CPDFSDK_Annot* pAnnot,
                           FX_UINT nFlag);

  virtual FX_BOOL OnLButtonDown(CPDFSDK_PageView* pPageView,
                                CPDFSDK_Annot* pAnnot,
                                FX_UINT nFlags,
                                const CPDF_Point& point);
  virtual FX_BOOL OnLButtonUp(CPDFSDK_PageView* pPageView,
                              CPDFSDK_Annot* pAnnot,
                              FX_UINT nFlags,
                              const CPDF_Point& point);
  virtual FX_BOOL OnLButtonDblClk(CPDFSDK_PageView* pPageView,
                                  CPDFSDK_Annot* pAnnot,
                                  FX_UINT nFlags,
                                  const CPDF_Point& point);
  virtual FX_BOOL OnMouseMove(CPDFSDK_PageView* pPageView,
                              CPDFSDK_Annot* pAnnot,
                              FX_UINT nFlags,
                              const CPDF_Point& point);
  virtual FX_BOOL OnMouseWheel(CPDFSDK_PageView* pPageView,
                               CPDFSDK_Annot* pAnnot,
                               FX_UINT nFlags,
                               short zDelta,
                               const CPDF_Point& point);
  virtual FX_BOOL OnRButtonDown(CPDFSDK_PageView* pPageView,
                                CPDFSDK_Annot* pAnnot,
                                FX_UINT nFlags,
                                const CPDF_Point& point);
  virtual FX_BOOL OnRButtonUp(CPDFSDK_PageView* pPageView,
                              CPDFSDK_Annot* pAnnot,
                              FX_UINT nFlags,
                              const CPDF_Point& point);

  virtual FX_BOOL OnKeyDown(CPDFSDK_Annot* pAnnot,
                            FX_UINT nKeyCode,
                            FX_UINT nFlags);
  virtual FX_BOOL OnChar(CPDFSDK_Annot* pAnnot, FX_UINT nChar, FX_UINT nFlags);

  virtual FX_BOOL OnSetFocus(CPDFSDK_Annot* pAnnot, FX_UINT nFlag);
  virtual FX_BOOL OnKillFocus(CPDFSDK_Annot* pAnnot, FX_UINT nFlag);

  CFFL_FormFiller* GetFormFiller(CPDFSDK_Annot* pAnnot, FX_BOOL bRegister);
  void RemoveFormFiller(CPDFSDK_Annot* pAnnot);

  static FX_BOOL IsVisible(CPDFSDK_Widget* pWidget);
  static FX_BOOL IsReadOnly(CPDFSDK_Widget* pWidget);
  static FX_BOOL IsFillingAllowed(CPDFSDK_Widget* pWidget);
  static FX_BOOL IsValidAnnot(CPDFSDK_PageView* pPageView,
                              CPDFSDK_Annot* pAnnot);

  void OnKeyStrokeCommit(CPDFSDK_Widget* pWidget,
                         CPDFSDK_PageView* pPageView,
                         FX_BOOL& bRC,
                         FX_BOOL& bExit,
                         FX_DWORD nFlag);
  void OnValidate(CPDFSDK_Widget* pWidget,
                  CPDFSDK_PageView* pPageView,
                  FX_BOOL& bRC,
                  FX_BOOL& bExit,
                  FX_DWORD nFlag);

  void OnCalculate(CPDFSDK_Widget* pWidget,
                   CPDFSDK_PageView* pPageView,
                   FX_BOOL& bExit,
                   FX_DWORD nFlag);
  void OnFormat(CPDFSDK_Widget* pWidget,
                CPDFSDK_PageView* pPageView,
                FX_BOOL& bExit,
                FX_DWORD nFlag);
  void OnButtonUp(CPDFSDK_Widget* pWidget,
                  CPDFSDK_PageView* pPageView,
                  FX_BOOL& bReset,
                  FX_BOOL& bExit,
                  FX_UINT nFlag);

 private:
  using CFFL_Widget2Filler = std::map<CPDFSDK_Annot*, CFFL_FormFiller*>;

  // IPWL_Filler_Notify:
  void QueryWherePopup(void* pPrivateData,
                       FX_FLOAT fPopupMin,
                       FX_FLOAT fPopupMax,
                       int32_t& nRet,
                       FX_FLOAT& fPopupRet) override;
  void OnBeforeKeyStroke(FX_BOOL bEditOrList,
                         void* pPrivateData,
                         int32_t nKeyCode,
                         CFX_WideString& strChange,
                         const CFX_WideString& strChangeEx,
                         int nSelStart,
                         int nSelEnd,
                         FX_BOOL bKeyDown,
                         FX_BOOL& bRC,
                         FX_BOOL& bExit,
                         FX_DWORD nFlag) override;
  void OnAfterKeyStroke(FX_BOOL bEditOrList,
                        void* pPrivateData,
                        FX_BOOL& bExit,
                        FX_DWORD nFlag) override;

  void UnRegisterFormFiller(CPDFSDK_Annot* pAnnot);

  CPDFDoc_Environment* m_pApp;
  CFFL_Widget2Filler m_Maps;
  FX_BOOL m_bNotifying;
};

class CFFL_PrivateData {
 public:
  CPDFSDK_Widget* pWidget;
  CPDFSDK_PageView* pPageView;
  int nWidgetAge;
  int nValueAge;
};

#endif  // FPDFSDK_INCLUDE_FORMFILLER_FFL_IFORMFILLER_H_
