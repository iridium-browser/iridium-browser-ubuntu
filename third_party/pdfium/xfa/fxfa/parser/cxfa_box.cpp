// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fxfa/parser/cxfa_box.h"

#include "xfa/fxfa/parser/cxfa_corner.h"
#include "xfa/fxfa/parser/cxfa_measurement.h"
#include "xfa/fxfa/parser/xfa_object.h"

namespace {

void GetStrokesInternal(CXFA_Node* pNode,
                        std::vector<CXFA_Stroke>* strokes,
                        bool bNull) {
  strokes->clear();
  if (!pNode)
    return;

  strokes->resize(8);
  int32_t i, j;
  for (i = 0, j = 0; i < 4; i++) {
    CXFA_Corner corner =
        CXFA_Corner(pNode->GetProperty(i, XFA_Element::Corner, i == 0));
    if (corner || i == 0) {
      (*strokes)[j] = corner;
    } else if (!bNull) {
      if (i == 1 || i == 2)
        (*strokes)[j] = (*strokes)[0];
      else
        (*strokes)[j] = (*strokes)[2];
    }
    j++;
    CXFA_Edge edge =
        CXFA_Edge(pNode->GetProperty(i, XFA_Element::Edge, i == 0));
    if (edge || i == 0) {
      (*strokes)[j] = edge;
    } else if (!bNull) {
      if (i == 1 || i == 2)
        (*strokes)[j] = (*strokes)[1];
      else
        (*strokes)[j] = (*strokes)[3];
    }
    j++;
  }
}

static int32_t Style3D(const std::vector<CXFA_Stroke>& strokes,
                       CXFA_Stroke& stroke) {
  if (strokes.empty())
    return 0;

  stroke = strokes[0];
  for (size_t i = 1; i < strokes.size(); i++) {
    CXFA_Stroke find = strokes[i];
    if (!find)
      continue;

    if (!stroke)
      stroke = find;
    else if (stroke.GetStrokeType() != find.GetStrokeType())
      stroke = find;
    break;
  }
  int32_t iType = stroke.GetStrokeType();
  if (iType == XFA_ATTRIBUTEENUM_Lowered || iType == XFA_ATTRIBUTEENUM_Raised ||
      iType == XFA_ATTRIBUTEENUM_Etched ||
      iType == XFA_ATTRIBUTEENUM_Embossed) {
    return iType;
  }
  return 0;
}

}  // namespace

int32_t CXFA_Box::GetHand() const {
  if (!m_pNode)
    return XFA_ATTRIBUTEENUM_Even;
  return m_pNode->GetEnum(XFA_ATTRIBUTE_Hand);
}

int32_t CXFA_Box::GetPresence() const {
  if (!m_pNode)
    return XFA_ATTRIBUTEENUM_Hidden;
  return m_pNode->GetEnum(XFA_ATTRIBUTE_Presence);
}

int32_t CXFA_Box::CountEdges() const {
  if (!m_pNode)
    return 0;
  return m_pNode->CountChildren(XFA_Element::Edge);
}

CXFA_Edge CXFA_Box::GetEdge(int32_t nIndex) const {
  return CXFA_Edge(
      m_pNode ? m_pNode->GetProperty(nIndex, XFA_Element::Edge, nIndex == 0)
              : nullptr);
}

void CXFA_Box::GetStrokes(std::vector<CXFA_Stroke>* strokes) const {
  GetStrokesInternal(m_pNode, strokes, false);
}

bool CXFA_Box::IsCircular() const {
  if (!m_pNode)
    return false;
  return m_pNode->GetBoolean(XFA_ATTRIBUTE_Circular);
}

bool CXFA_Box::GetStartAngle(FX_FLOAT& fStartAngle) const {
  fStartAngle = 0;
  if (!m_pNode)
    return false;

  CXFA_Measurement ms;
  bool bRet = m_pNode->TryMeasure(XFA_ATTRIBUTE_StartAngle, ms, false);
  if (bRet)
    fStartAngle = ms.GetValue();

  return bRet;
}

bool CXFA_Box::GetSweepAngle(FX_FLOAT& fSweepAngle) const {
  fSweepAngle = 360;
  if (!m_pNode)
    return false;

  CXFA_Measurement ms;
  bool bRet = m_pNode->TryMeasure(XFA_ATTRIBUTE_SweepAngle, ms, false);
  if (bRet)
    fSweepAngle = ms.GetValue();

  return bRet;
}

CXFA_Fill CXFA_Box::GetFill(bool bModified) const {
  if (!m_pNode)
    return CXFA_Fill(nullptr);

  CXFA_Node* pFillNode = m_pNode->GetProperty(0, XFA_Element::Fill, bModified);
  return CXFA_Fill(pFillNode);
}

CXFA_Margin CXFA_Box::GetMargin() const {
  return CXFA_Margin(m_pNode ? m_pNode->GetChild(0, XFA_Element::Margin)
                             : nullptr);
}

int32_t CXFA_Box::Get3DStyle(bool& bVisible, FX_FLOAT& fThickness) const {
  if (IsArc())
    return 0;

  std::vector<CXFA_Stroke> strokes;
  GetStrokesInternal(m_pNode, &strokes, true);
  CXFA_Stroke stroke(nullptr);
  int32_t iType = Style3D(strokes, stroke);
  if (iType) {
    bVisible = stroke.IsVisible();
    fThickness = stroke.GetThickness();
  }
  return iType;
}
