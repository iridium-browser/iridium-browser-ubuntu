// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_DATAMATRIX_BC_HIGHLEVELENCODER_H_
#define XFA_FXBARCODE_DATAMATRIX_BC_HIGHLEVELENCODER_H_

#include <vector>

#include "xfa/fxbarcode/datamatrix/BC_SymbolShapeHint.h"

#define ASCII_ENCODATION 0
#define C40_ENCODATION 1
#define TEXT_ENCODATION 2
#define X12_ENCODATION 3
#define EDIFACT_ENCODATION 4
#define BASE256_ENCODATION 5

class CBC_HighLevelEncoder : public CBC_SymbolShapeHint {
 public:
  CBC_HighLevelEncoder();
  ~CBC_HighLevelEncoder() override;

  CFX_ArrayTemplate<uint8_t>& getBytesForMessage(CFX_WideString msg);
  static CFX_WideString encodeHighLevel(CFX_WideString msg,
                                        CFX_WideString ecLevel,
                                        int32_t& e);
  static CFX_WideString encodeHighLevel(CFX_WideString msg,
                                        CFX_WideString ecLevel,
                                        SymbolShapeHint shape,
                                        CBC_Dimension* minSize,
                                        CBC_Dimension* maxSize,
                                        int32_t& e);
  static int32_t lookAheadTest(CFX_WideString msg,
                               int32_t startpos,
                               int32_t currentMode);
  static bool isDigit(FX_WCHAR ch);
  static bool isExtendedASCII(FX_WCHAR ch);
  static int32_t determineConsecutiveDigitCount(CFX_WideString msg,
                                                int32_t startpos);
  static void illegalCharacter(FX_WCHAR c, int32_t& e);

 public:
  static FX_WCHAR LATCH_TO_C40;
  static FX_WCHAR LATCH_TO_BASE256;
  static FX_WCHAR UPPER_SHIFT;
  static FX_WCHAR LATCH_TO_ANSIX12;
  static FX_WCHAR LATCH_TO_TEXT;
  static FX_WCHAR LATCH_TO_EDIFACT;
  static FX_WCHAR C40_UNLATCH;
  static FX_WCHAR X12_UNLATCH;

 private:
  static FX_WCHAR PAD;
  static FX_WCHAR MACRO_05;
  static FX_WCHAR MACRO_06;
  static const wchar_t* MACRO_05_HEADER;
  static const wchar_t* MACRO_06_HEADER;
  static const wchar_t MACRO_TRAILER;
  CFX_ArrayTemplate<uint8_t> m_bytearray;

 private:
  static FX_WCHAR randomize253State(FX_WCHAR ch, int32_t codewordPosition);
  static int32_t findMinimums(std::vector<FX_FLOAT>& charCounts,
                              CFX_ArrayTemplate<int32_t>& intCharCounts,
                              int32_t min,
                              CFX_ArrayTemplate<uint8_t>& mins);
  static int32_t getMinimumCount(CFX_ArrayTemplate<uint8_t>& mins);
  static bool isNativeC40(FX_WCHAR ch);
  static bool isNativeText(FX_WCHAR ch);
  static bool isNativeX12(FX_WCHAR ch);
  static bool isX12TermSep(FX_WCHAR ch);
  static bool isNativeEDIFACT(FX_WCHAR ch);
  static bool isSpecialB256(FX_WCHAR ch);
};

#endif  // XFA_FXBARCODE_DATAMATRIX_BC_HIGHLEVELENCODER_H_
