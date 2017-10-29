// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_DIB_CFX_BITMAPSTORER_H_
#define CORE_FXGE_DIB_CFX_BITMAPSTORER_H_

#include <memory>
#include <vector>

#include "core/fxcrt/cfx_retain_ptr.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxge/dib/cfx_dibitmap.h"
#include "core/fxge/dib/ifx_scanlinecomposer.h"
#include "third_party/base/stl_util.h"

class CFX_BitmapStorer : public IFX_ScanlineComposer {
 public:
  CFX_BitmapStorer();
  ~CFX_BitmapStorer() override;

  // IFX_ScanlineComposer
  void ComposeScanline(int line,
                       const uint8_t* scanline,
                       const uint8_t* scan_extra_alpha) override;
  bool SetInfo(int width,
               int height,
               FXDIB_Format src_format,
               uint32_t* pSrcPalette) override;

  CFX_RetainPtr<CFX_DIBitmap> GetBitmap() { return m_pBitmap; }
  CFX_RetainPtr<CFX_DIBitmap> Detach();
  void Replace(CFX_RetainPtr<CFX_DIBitmap>&& pBitmap);

 private:
  CFX_RetainPtr<CFX_DIBitmap> m_pBitmap;
};

#endif  // CORE_FXGE_DIB_CFX_BITMAPSTORER_H_
