// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/app/xfa_fwladapter.h"

#include "xfa/fxfa/app/xfa_fffield.h"
#include "xfa/fxfa/xfa_ffdoc.h"

CXFA_FWLAdapterWidgetMgr::CXFA_FWLAdapterWidgetMgr() {}

CXFA_FWLAdapterWidgetMgr::~CXFA_FWLAdapterWidgetMgr() {}

void CXFA_FWLAdapterWidgetMgr::RepaintWidget(CFWL_Widget* pWidget) {
  if (!pWidget)
    return;

  CXFA_FFWidget* pFFWidget = pWidget->GetLayoutItem();
  if (!pFFWidget)
    return;

  pFFWidget->AddInvalidateRect(nullptr);
}

bool CXFA_FWLAdapterWidgetMgr::GetPopupPos(CFWL_Widget* pWidget,
                                           FX_FLOAT fMinHeight,
                                           FX_FLOAT fMaxHeight,
                                           const CFX_RectF& rtAnchor,
                                           CFX_RectF& rtPopup) {
  CXFA_FFWidget* pFFWidget = pWidget->GetLayoutItem();
  CFX_RectF rtRotateAnchor(rtAnchor);
  pFFWidget->GetRotateMatrix().TransformRect(rtRotateAnchor);
  pFFWidget->GetDoc()->GetDocEnvironment()->GetPopupPos(
      pFFWidget, fMinHeight, fMaxHeight, rtRotateAnchor, rtPopup);
  return true;
}
