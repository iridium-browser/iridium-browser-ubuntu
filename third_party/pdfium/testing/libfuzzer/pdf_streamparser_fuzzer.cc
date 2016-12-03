// Copyright 2016 The PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/fpdf_page/pageint.h"

#include <cstdint>

#include "core/fpdfapi/fpdf_parser/include/cpdf_object.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  CPDF_StreamParser parser(data, size);
  while (CPDF_Object* pObj = parser.ReadNextObject(true, 0))
    pObj->Release();

  return 0;
}
