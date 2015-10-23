// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "JBig2_PatternDict.h"
CJBig2_PatternDict::CJBig2_PatternDict() {
  NUMPATS = 0;
  HDPATS = NULL;
}

CJBig2_PatternDict::~CJBig2_PatternDict() {
  if (HDPATS) {
    for (FX_DWORD i = 0; i < NUMPATS; i++) {
      delete HDPATS[i];
    }
    m_pModule->JBig2_Free(HDPATS);
  }
}
