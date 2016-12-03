// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_DATAEXPORTER_H_
#define XFA_FXFA_PARSER_CXFA_DATAEXPORTER_H_

#include "core/fxcrt/include/fx_string.h"

class CXFA_Document;
class CXFA_Node;
class IFX_FileWrite;
class IFX_Stream;

class CXFA_DataExporter {
 public:
  explicit CXFA_DataExporter(CXFA_Document* pDocument);

  FX_BOOL Export(IFX_FileWrite* pWrite);
  FX_BOOL Export(IFX_FileWrite* pWrite,
                 CXFA_Node* pNode,
                 uint32_t dwFlag,
                 const FX_CHAR* pChecksum);

 protected:
  FX_BOOL Export(IFX_Stream* pStream,
                 CXFA_Node* pNode,
                 uint32_t dwFlag,
                 const FX_CHAR* pChecksum);

  CXFA_Document* const m_pDocument;
};

#endif  // XFA_FXFA_PARSER_CXFA_DATAEXPORTER_H_
