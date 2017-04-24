// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/app/xfa_ffexclgroup.h"

#include "xfa/fxfa/xfa_ffapp.h"
#include "xfa/fxfa/xfa_ffdoc.h"
#include "xfa/fxfa/xfa_ffpageview.h"
#include "xfa/fxfa/xfa_ffwidget.h"

CXFA_FFExclGroup::CXFA_FFExclGroup(CXFA_WidgetAcc* pDataAcc)
    : CXFA_FFWidget(pDataAcc) {}

CXFA_FFExclGroup::~CXFA_FFExclGroup() {}

void CXFA_FFExclGroup::RenderWidget(CFX_Graphics* pGS,
                                    CFX_Matrix* pMatrix,
                                    uint32_t dwStatus) {
  if (!IsMatchVisibleStatus(dwStatus))
    return;

  CFX_Matrix mtRotate = GetRotateMatrix();
  if (pMatrix)
    mtRotate.Concat(*pMatrix);

  CXFA_FFWidget::RenderWidget(pGS, &mtRotate, dwStatus);
}
