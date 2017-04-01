// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/theme/cfwl_carettp.h"

#include "xfa/fwl/cfwl_caret.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/cfwl_widget.h"
#include "xfa/fxgraphics/cfx_color.h"
#include "xfa/fxgraphics/cfx_path.h"

CFWL_CaretTP::CFWL_CaretTP() {}
CFWL_CaretTP::~CFWL_CaretTP() {}

void CFWL_CaretTP::DrawBackground(CFWL_ThemeBackground* pParams) {
  if (!pParams)
    return;

  switch (pParams->m_iPart) {
    case CFWL_Part::Background: {
      if (!(pParams->m_dwStates & CFWL_PartState_HightLight))
        return;

      DrawCaretBK(pParams->m_pGraphics, pParams->m_dwStates,
                  &(pParams->m_rtPart), (CFX_Color*)pParams->m_pData,
                  &(pParams->m_matrix));
      break;
    }
    default:
      break;
  }
}

void CFWL_CaretTP::DrawCaretBK(CFX_Graphics* pGraphics,
                               uint32_t dwStates,
                               const CFX_RectF* pRect,
                               CFX_Color* crFill,
                               CFX_Matrix* pMatrix) {
  CFX_Path path;
  path.Create();
  CFX_RectF rect = *pRect;
  path.AddRectangle(rect.left, rect.top, rect.width, rect.height);
  if (crFill) {
    pGraphics->SetFillColor(crFill);
  } else {
    CFX_Color crFilltemp(ArgbEncode(255, 0, 0, 0));
    pGraphics->SetFillColor(&crFilltemp);
  }
  pGraphics->FillPath(&path, FXFILL_WINDING, pMatrix);
}
