// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_icon_win.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/icon_util.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/installer/util/install_util.h"
#endif

namespace {

// Returns the resource id of the application icon.
int GetAppIconResourceId() {
  int icon_id = IDR_MAINFRAME;
#if defined(GOOGLE_CHROME_BUILD)
  if (InstallUtil::IsChromeSxSProcess())
    icon_id = IDR_SXS;
#endif
  return icon_id;
}

}  // namespace

HICON GetAppIcon() {
  const int icon_id = GetAppIconResourceId();
  // HICON returned from LoadIcon do not leak and do not have to be destroyed.
  return LoadIcon(GetModuleHandle(chrome::kBrowserResourcesDll),
                  MAKEINTRESOURCE(icon_id));
}

HICON GetSmallAppIcon() {
  const int icon_id = GetAppIconResourceId();
  // HICON returned from LoadImage must be released using DestroyIcon.
  return static_cast<HICON>(LoadImage(
      GetModuleHandle(chrome::kBrowserResourcesDll), MAKEINTRESOURCE(icon_id),
      IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
      LR_DEFAULTCOLOR | LR_SHARED));
}

scoped_ptr<SkBitmap> GetAppIconForSize(int size) {
  const int icon_id = GetAppIconResourceId();
  return IconUtil::CreateSkBitmapFromIconResource(
      GetModuleHandle(chrome::kBrowserResourcesDll), icon_id, size);
}
