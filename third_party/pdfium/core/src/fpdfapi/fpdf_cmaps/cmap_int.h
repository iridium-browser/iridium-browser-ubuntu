// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_SRC_FPDFAPI_FPDF_CMAPS_CMAP_INT_H_
#define CORE_SRC_FPDFAPI_FPDF_CMAPS_CMAP_INT_H_

#include "../../../include/fxcrt/fx_system.h"  // For FX_WORD.

struct FXCMAP_CMap {
  enum MapType { None, Single, Range, Reverse };

  const char* m_Name;
  MapType m_WordMapType;
  const FX_WORD* m_pWordMap;
  int m_WordCount;
  MapType m_DWordMapType;
  const FX_WORD* m_pDWordMap;
  int m_DWordCount;
  int m_UseOffset;
};

#endif  // CORE_SRC_FPDFAPI_FPDF_CMAPS_CMAP_INT_H_
