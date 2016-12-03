// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/include/cpdfsdk_xfawidget.h"

#include "fpdfsdk/include/ipdfsdk_annothandler.h"
#include "xfa/fxfa/include/xfa_ffwidget.h"

CPDFSDK_XFAWidget::CPDFSDK_XFAWidget(CXFA_FFWidget* pAnnot,
                                     CPDFSDK_PageView* pPageView,
                                     CPDFSDK_InterForm* pInterForm)
    : CPDFSDK_Annot(pPageView),
      m_pInterForm(pInterForm),
      m_hXFAWidget(pAnnot) {}

FX_BOOL CPDFSDK_XFAWidget::IsXFAField() {
  return TRUE;
}

CXFA_FFWidget* CPDFSDK_XFAWidget::GetXFAWidget() const {
  return m_hXFAWidget;
}

CFX_ByteString CPDFSDK_XFAWidget::GetType() const {
  return FSDK_XFAWIDGET_TYPENAME;
}

CFX_ByteString CPDFSDK_XFAWidget::GetSubType() const {
  return "";
}

CFX_FloatRect CPDFSDK_XFAWidget::GetRect() const {
  CFX_RectF rcBBox;
  GetXFAWidget()->GetRect(rcBBox);
  return CFX_FloatRect(rcBBox.left, rcBBox.top, rcBBox.left + rcBBox.width,
                       rcBBox.top + rcBBox.height);
}
