// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_FM2JS_XFA_PROGRAM_H_
#define XFA_FXFA_FM2JS_XFA_PROGRAM_H_

#include <memory>

#include "xfa/fxfa/fm2js/xfa_error.h"
#include "xfa/fxfa/fm2js/xfa_fmparse.h"

class CXFA_FMProgram {
 public:
  explicit CXFA_FMProgram(const CFX_WideStringC& wsFormcalc);
  ~CXFA_FMProgram();

  int32_t ParseProgram();
  int32_t TranslateProgram(CFX_WideTextBuf& wsJavaScript);

  const CXFA_FMErrorInfo& GetError() const { return m_pErrorInfo; }

 private:
  CXFA_FMErrorInfo m_pErrorInfo;
  CXFA_FMParse m_parse;
  std::unique_ptr<CXFA_FMFunctionDefinition> m_globalFunction;
};

#endif  // XFA_FXFA_FM2JS_XFA_PROGRAM_H_
