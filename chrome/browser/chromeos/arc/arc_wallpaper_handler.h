// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_WALLPAPER_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_WALLPAPER_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/image_decoder.h"
#include "components/arc/set_wallpaper_delegate.h"

class SkBitmap;

namespace arc {

// Lives on the UI thread.
class ArcWallpaperHandler : public SetWallpaperDelegate,
                            public ImageDecoder::ImageRequest {
 public:
  ArcWallpaperHandler();
  ~ArcWallpaperHandler() override;

  // SetWallpaperDelegate implementation.
  void SetWallpaper(std::vector<uint8_t> jpeg_data) override;

  // ImageDecoder::ImageRequest implementation.
  void OnImageDecoded(const SkBitmap& bitmap) override;
  void OnDecodeImageFailed() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcWallpaperHandler);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_WALLPAPER_HANDLER_H_
