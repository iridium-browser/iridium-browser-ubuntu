// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_WIN32_CFX_WINDOWSDIB_H_
#define CORE_FXGE_WIN32_CFX_WINDOWSDIB_H_
#ifdef _WIN32
#ifndef _WINDOWS_
#include <windows.h>
#endif

#include "core/fxge/fx_dib.h"

#define WINDIB_OPEN_MEMORY 0x1
#define WINDIB_OPEN_PATHNAME 0x2

typedef struct WINDIB_Open_Args_ {
  int flags;

  const uint8_t* memory_base;

  size_t memory_size;

  const wchar_t* path_name;
} WINDIB_Open_Args_;

class CFX_WindowsDIB : public CFX_DIBitmap {
 public:
  template <typename T, typename... Args>
  friend CFX_RetainPtr<T> pdfium::MakeRetain(Args&&... args);

  ~CFX_WindowsDIB() override;

  static CFX_ByteString GetBitmapInfo(
      const CFX_RetainPtr<CFX_DIBitmap>& pBitmap);
  static HBITMAP GetDDBitmap(const CFX_RetainPtr<CFX_DIBitmap>& pBitmap,
                             HDC hDC);

  static CFX_RetainPtr<CFX_DIBitmap> LoadFromBuf(BITMAPINFO* pbmi, void* pData);
  static CFX_RetainPtr<CFX_DIBitmap> LoadFromFile(const wchar_t* filename);
  static CFX_RetainPtr<CFX_DIBitmap> LoadFromFile(const char* filename);
  static CFX_RetainPtr<CFX_DIBitmap> LoadDIBitmap(WINDIB_Open_Args_ args);

  HBITMAP GetWindowsBitmap() const { return m_hBitmap; }

  void LoadFromDevice(HDC hDC, int left, int top);
  void SetToDevice(HDC hDC, int left, int top);

 protected:
  CFX_WindowsDIB(HDC hDC, int width, int height);

  HDC m_hMemDC;
  HBITMAP m_hBitmap;
  HBITMAP m_hOldBitmap;
};

#endif  // _WIN32

#endif  // CORE_FXGE_WIN32_CFX_WINDOWSDIB_H_
