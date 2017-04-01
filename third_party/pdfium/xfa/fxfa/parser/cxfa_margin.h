// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_MARGIN_H_
#define XFA_FXFA_PARSER_CXFA_MARGIN_H_

#include "core/fxcrt/fx_system.h"
#include "xfa/fxfa/parser/cxfa_data.h"

class CXFA_Node;

class CXFA_Margin : public CXFA_Data {
 public:
  explicit CXFA_Margin(CXFA_Node* pNode);

  bool GetLeftInset(FX_FLOAT& fInset, FX_FLOAT fDefInset = 0) const;
  bool GetTopInset(FX_FLOAT& fInset, FX_FLOAT fDefInset = 0) const;
  bool GetRightInset(FX_FLOAT& fInset, FX_FLOAT fDefInset = 0) const;
  bool GetBottomInset(FX_FLOAT& fInset, FX_FLOAT fDefInset = 0) const;
};

#endif  // XFA_FXFA_PARSER_CXFA_MARGIN_H_
