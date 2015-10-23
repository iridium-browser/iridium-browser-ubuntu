// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef _JBIG2_MODULE_H_
#define _JBIG2_MODULE_H_
#include "JBig2_Define.h"
class CJBig2_Module {
 public:
  virtual ~CJBig2_Module() {}

  virtual void* JBig2_Malloc(FX_DWORD dwSize) = 0;

  virtual void* JBig2_Malloc2(FX_DWORD num, FX_DWORD dwSize) = 0;

  virtual void* JBig2_Malloc3(FX_DWORD num,
                              FX_DWORD dwSize,
                              FX_DWORD dwSize2) = 0;

  virtual void* JBig2_Realloc(void* pMem, FX_DWORD dwSize) = 0;

  virtual void JBig2_Free(void* pMem) = 0;

  virtual void JBig2_Assert(int32_t nExpression) {}

  virtual void JBig2_Error(const FX_CHAR* format, ...) {}

  virtual void JBig2_Warn(const FX_CHAR* format, ...) {}

  virtual void JBig2_Log(const FX_CHAR* format, ...) {}
};
#endif
