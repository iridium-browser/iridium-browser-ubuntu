// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/app/xfa_textpiece.h"

#include "xfa/fxfa/app/cxfa_linkuserdata.h"

XFA_TextPiece::XFA_TextPiece()
    : pszText(nullptr), pWidths(nullptr), pFont(nullptr), pLinkData(nullptr) {}

XFA_TextPiece::~XFA_TextPiece() {
  if (pLinkData)
    pLinkData->Release();

  FX_Free(pszText);
  FX_Free(pWidths);
}
