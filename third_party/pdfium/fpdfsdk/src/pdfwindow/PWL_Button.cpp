// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/pdfwindow/PDFWindow.h"
#include "../../include/pdfwindow/PWL_Wnd.h"
#include "../../include/pdfwindow/PWL_Button.h"
#include "../../include/pdfwindow/PWL_Utils.h"

/* ------------------------------- CPWL_Button
 * ---------------------------------- */

CPWL_Button::CPWL_Button() : m_bMouseDown(FALSE) {}

CPWL_Button::~CPWL_Button() {
  //	PWL_TRACE("~CPWL_Button\n");
}

CFX_ByteString CPWL_Button::GetClassName() const {
  return "CPWL_Button";
}

void CPWL_Button::OnCreate(PWL_CREATEPARAM& cp) {
  cp.eCursorType = FXCT_HAND;
}

FX_BOOL CPWL_Button::OnLButtonDown(const CPDF_Point& point, FX_DWORD nFlag) {
  CPWL_Wnd::OnLButtonDown(point, nFlag);

  m_bMouseDown = TRUE;
  SetCapture();

  return TRUE;
}

FX_BOOL CPWL_Button::OnLButtonUp(const CPDF_Point& point, FX_DWORD nFlag) {
  CPWL_Wnd::OnLButtonUp(point, nFlag);

  ReleaseCapture();
  m_bMouseDown = FALSE;

  return TRUE;
}
