// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_PDF417_BC_PDF417WRITER_H_
#define XFA_FXBARCODE_PDF417_BC_PDF417WRITER_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fxbarcode/BC_TwoDimWriter.h"

class CBC_PDF417Writer : public CBC_TwoDimWriter {
 public:
  CBC_PDF417Writer();
  ~CBC_PDF417Writer() override;

  uint8_t* Encode(const CFX_WideString& contents,
                  int32_t& outWidth,
                  int32_t& outHeight,
                  int32_t& e);

  // CBC_TwoDimWriter
  bool SetErrorCorrectionLevel(int32_t level) override;

  void SetTruncated(bool truncated);

 private:
  void rotateArray(CFX_ArrayTemplate<uint8_t>& bitarray,
                   int32_t width,
                   int32_t height);
  bool m_bTruncated;
};

#endif  // XFA_FXBARCODE_PDF417_BC_PDF417WRITER_H_
