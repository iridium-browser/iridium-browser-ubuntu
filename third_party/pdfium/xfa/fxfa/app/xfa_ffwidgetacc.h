// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_APP_XFA_FFWIDGETACC_H_
#define XFA_FXFA_APP_XFA_FFWIDGETACC_H_

#include "core/fxcrt/fx_string.h"
#include "xfa/fxfa/app/cxfa_textlayout.h"
#include "xfa/fxfa/fxfa_widget.h"
#include "xfa/fxfa/parser/cxfa_font.h"
#include "xfa/fxfa/parser/cxfa_para.h"

class CXFA_FFDoc;
class CXFA_Node;

enum XFA_TEXTPROVIDERTYPE {
  XFA_TEXTPROVIDERTYPE_Text,
  XFA_TEXTPROVIDERTYPE_Datasets,
  XFA_TEXTPROVIDERTYPE_Caption,
  XFA_TEXTPROVIDERTYPE_Rollover,
  XFA_TEXTPROVIDERTYPE_Down,
};

class CXFA_TextProvider {
 public:
  CXFA_TextProvider(CXFA_WidgetAcc* pWidgetAcc,
                    XFA_TEXTPROVIDERTYPE eType,
                    CXFA_Node* pTextNode = nullptr)
      : m_pWidgetAcc(pWidgetAcc), m_eType(eType), m_pTextNode(pTextNode) {
    ASSERT(m_pWidgetAcc);
  }
  ~CXFA_TextProvider() {}

  CXFA_Node* GetTextNode(bool& bRichText);
  CXFA_Para GetParaNode();
  CXFA_Font GetFontNode();
  bool IsCheckButtonAndAutoWidth();
  CXFA_FFDoc* GetDocNode() { return m_pWidgetAcc->GetDoc(); }
  bool GetEmbbedObj(bool bURI,
                    bool bRaw,
                    const CFX_WideString& wsAttr,
                    CFX_WideString& wsValue);

 protected:
  CXFA_WidgetAcc* m_pWidgetAcc;
  XFA_TEXTPROVIDERTYPE m_eType;
  CXFA_Node* m_pTextNode;
};

#endif  // XFA_FXFA_APP_XFA_FFWIDGETACC_H_
