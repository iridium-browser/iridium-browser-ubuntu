// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com
/*
 * Copyright 2011 ZXing authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "xfa/fxbarcode/cbc_datamatrix.h"

#include "xfa/fxbarcode/datamatrix/BC_DataMatrixWriter.h"

CBC_DataMatrix::CBC_DataMatrix() : CBC_CodeBase(new CBC_DataMatrixWriter) {}

CBC_DataMatrix::~CBC_DataMatrix() {}

bool CBC_DataMatrix::Encode(const CFX_WideStringC& contents,
                            bool isDevice,
                            int32_t& e) {
  int32_t outWidth = 0;
  int32_t outHeight = 0;
  uint8_t* data =
      static_cast<CBC_DataMatrixWriter*>(m_pBCWriter.get())
          ->Encode(CFX_WideString(contents), outWidth, outHeight, e);
  if (e != BCExceptionNO)
    return false;
  static_cast<CBC_TwoDimWriter*>(m_pBCWriter.get())
      ->RenderResult(data, outWidth, outHeight, e);
  FX_Free(data);
  if (e != BCExceptionNO)
    return false;
  return true;
}

bool CBC_DataMatrix::RenderDevice(CFX_RenderDevice* device,
                                  const CFX_Matrix* matrix,
                                  int32_t& e) {
  static_cast<CBC_TwoDimWriter*>(m_pBCWriter.get())
      ->RenderDeviceResult(device, matrix);
  return true;
}

bool CBC_DataMatrix::RenderBitmap(CFX_DIBitmap*& pOutBitmap, int32_t& e) {
  static_cast<CBC_TwoDimWriter*>(m_pBCWriter.get())
      ->RenderBitmapResult(pOutBitmap, e);
  if (e != BCExceptionNO)
    return false;
  return true;
}

BC_TYPE CBC_DataMatrix::GetType() {
  return BC_DATAMATRIX;
}
