// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/win/registry.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/win/direct_write.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace gfx {

namespace {

FontRenderParams::SubpixelRendering GetSubpixelRenderingGeometry() {
  DISPLAY_DEVICE display_device = {sizeof(DISPLAY_DEVICE), 0};
  for (int i = 0; EnumDisplayDevices(nullptr, i, &display_device, 0); ++i) {
    // TODO(scottmg): We only support the primary device currently.
    if (display_device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
      base::FilePath trimmed =
          base::FilePath(display_device.DeviceName).BaseName();
      base::win::RegKey key(
          HKEY_LOCAL_MACHINE,
          (L"SOFTWARE\\Microsoft\\Avalon.Graphics\\" + trimmed.value()).c_str(),
          KEY_READ);
      DWORD pixel_structure;
      if (key.ReadValueDW(L"PixelStructure", &pixel_structure) ==
          ERROR_SUCCESS) {
        if (pixel_structure == 1)
          return FontRenderParams::SUBPIXEL_RENDERING_RGB;
        if (pixel_structure == 2)
          return FontRenderParams::SUBPIXEL_RENDERING_BGR;
      }
      break;
    }
  }

  // No explicit ClearType settings, default to RGB.
  return FontRenderParams::SUBPIXEL_RENDERING_RGB;
}

// Caches font render params and updates them on system notifications.
class CachedFontRenderParams : public gfx::SingletonHwnd::Observer {
 public:
  static CachedFontRenderParams* GetInstance() {
    return Singleton<CachedFontRenderParams>::get();
  }

  const FontRenderParams& GetParams() {
    if (params_)
      return *params_;

    params_.reset(new FontRenderParams());
    params_->antialiasing = false;
    params_->subpixel_positioning = false;
    params_->autohinter = false;
    params_->use_bitmaps = false;
    params_->hinting = FontRenderParams::HINTING_MEDIUM;
    params_->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_NONE;

    BOOL enabled = false;
    if (SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &enabled, 0) && enabled) {
      params_->antialiasing = true;
      // GDI does not support subpixel positioning.
      params_->subpixel_positioning = win::IsDirectWriteEnabled();

      UINT type = 0;
      if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &type, 0) &&
          type == FE_FONTSMOOTHINGCLEARTYPE) {
        params_->subpixel_rendering = GetSubpixelRenderingGeometry();
      }
    }
    gfx::SingletonHwnd::GetInstance()->AddObserver(this);
    return *params_;
  }

 private:
  friend struct DefaultSingletonTraits<CachedFontRenderParams>;

  CachedFontRenderParams() {}
  virtual ~CachedFontRenderParams() {
    // Can't remove the SingletonHwnd observer here since SingletonHwnd may have
    // been destroyed already (both singletons).
  }

  virtual void OnWndProc(HWND hwnd,
                         UINT message,
                         WPARAM wparam,
                         LPARAM lparam) override {
    if (message == WM_SETTINGCHANGE) {
      params_.reset();
      gfx::SingletonHwnd::GetInstance()->RemoveObserver(this);
    }
  }

  scoped_ptr<FontRenderParams> params_;

  DISALLOW_COPY_AND_ASSIGN(CachedFontRenderParams);
};

}  // namespace

FontRenderParams GetFontRenderParams(const FontRenderParamsQuery& query,
                                     std::string* family_out) {
  if (family_out)
    NOTIMPLEMENTED();
  // Customized font rendering settings are not supported, only defaults.
  return CachedFontRenderParams::GetInstance()->GetParams();
}

}  // namespace gfx
