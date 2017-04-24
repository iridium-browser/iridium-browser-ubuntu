// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_STROKE_H_
#define XFA_FXFA_PARSER_CXFA_STROKE_H_

#include "core/fxcrt/fx_system.h"
#include "core/fxge/fx_dib.h"
#include "xfa/fxfa/fxfa_basic.h"
#include "xfa/fxfa/parser/cxfa_data.h"

enum StrokeSameStyle {
  XFA_STROKE_SAMESTYLE_NoPresence = 1,
  XFA_STROKE_SAMESTYLE_Corner = 2
};

class CXFA_Node;

class CXFA_Stroke : public CXFA_Data {
 public:
  CXFA_Stroke() : CXFA_Stroke(nullptr) {}
  explicit CXFA_Stroke(CXFA_Node* pNode) : CXFA_Data(pNode) {}

  bool IsCorner() const { return GetElementType() == XFA_Element::Corner; }
  bool IsEdge() const { return GetElementType() == XFA_Element::Edge; }
  bool IsVisible() const { return GetPresence() == XFA_ATTRIBUTEENUM_Visible; }
  int32_t GetPresence() const;
  int32_t GetCapType() const;
  int32_t GetStrokeType() const;
  FX_FLOAT GetThickness() const;
  CXFA_Measurement GetMSThickness() const;
  void SetMSThickness(CXFA_Measurement msThinkness);
  FX_ARGB GetColor() const;
  void SetColor(FX_ARGB argb);
  int32_t GetJoinType() const;
  bool IsInverted() const;
  FX_FLOAT GetRadius() const;
  bool SameStyles(CXFA_Stroke stroke, uint32_t dwFlags = 0) const;
};

#endif  // XFA_FXFA_PARSER_CXFA_STROKE_H_
