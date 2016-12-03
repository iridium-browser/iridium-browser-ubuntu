// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include <windows.h>
#include <shellapi.h>

#include "base/logging.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

#if defined(USE_ASH)
#include "ash/root_window_controller.h"
#endif

static bool IsPlatformFullScreenMode() {
  // SHQueryUserNotificationState is only available for Vista and above.
#if defined(NTDDI_VERSION) && (NTDDI_VERSION >= NTDDI_VISTA)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;

  typedef HRESULT(WINAPI *SHQueryUserNotificationStatePtr)(
      QUERY_USER_NOTIFICATION_STATE* state);

  HMODULE shell32_base = ::GetModuleHandle(L"shell32.dll");
  if (!shell32_base) {
    NOTREACHED();
    return false;
  }
  SHQueryUserNotificationStatePtr query_user_notification_state_ptr =
        reinterpret_cast<SHQueryUserNotificationStatePtr>
            (::GetProcAddress(shell32_base, "SHQueryUserNotificationState"));
  if (!query_user_notification_state_ptr) {
    NOTREACHED();
    return false;
  }

  QUERY_USER_NOTIFICATION_STATE state;
  if (FAILED((*query_user_notification_state_ptr)(&state)))
    return false;
  return state == QUNS_RUNNING_D3D_FULL_SCREEN ||
         state == QUNS_PRESENTATION_MODE;
#else
  return false;
#endif
}

static bool IsFullScreenWindowMode() {
  // Get the foreground window which the user is currently working on.
  HWND wnd = ::GetForegroundWindow();
  if (!wnd)
    return false;

  // Get the monitor where the window is located.
  RECT wnd_rect;
  if (!::GetWindowRect(wnd, &wnd_rect))
    return false;
  HMONITOR monitor = ::MonitorFromRect(&wnd_rect, MONITOR_DEFAULTTONULL);
  if (!monitor)
    return false;
  MONITORINFO monitor_info = { sizeof(monitor_info) };
  if (!::GetMonitorInfo(monitor, &monitor_info))
    return false;

  // It should be the main monitor.
  if (!(monitor_info.dwFlags & MONITORINFOF_PRIMARY))
    return false;

  // The window should be at least as large as the monitor.
  if (!::IntersectRect(&wnd_rect, &wnd_rect, &monitor_info.rcMonitor))
    return false;
  if (!::EqualRect(&wnd_rect, &monitor_info.rcMonitor))
    return false;

  // At last, the window style should not have WS_DLGFRAME and WS_THICKFRAME and
  // its extended style should not have WS_EX_WINDOWEDGE and WS_EX_TOOLWINDOW.
  LONG style = ::GetWindowLong(wnd, GWL_STYLE);
  LONG ext_style = ::GetWindowLong(wnd, GWL_EXSTYLE);
  return !((style & (WS_DLGFRAME | WS_THICKFRAME)) ||
           (ext_style & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
}

static bool IsFullScreenConsoleMode() {
  // We detect this by attaching the current process to the console of the
  // foreground window and then checking if it is in full screen mode.
  DWORD pid = 0;
  ::GetWindowThreadProcessId(::GetForegroundWindow(), &pid);
  if (!pid)
    return false;

  if (!::AttachConsole(pid))
    return false;

  DWORD modes = 0;
  ::GetConsoleDisplayMode(&modes);
  ::FreeConsole();

  return (modes & (CONSOLE_FULLSCREEN | CONSOLE_FULLSCREEN_HARDWARE)) != 0;
}

bool IsFullScreenMode() {
#if defined(USE_ASH)
  ash::RootWindowController* controller =
      ash::RootWindowController::ForTargetRootWindow();
  return controller && controller->GetWindowForFullscreenMode();
#else
  return IsPlatformFullScreenMode() ||
         IsFullScreenWindowMode() ||
         IsFullScreenConsoleMode();
#endif  // USE_ASH
}
