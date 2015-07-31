// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/browser_process_task.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_MACOSX)
#include "ui/gfx/image/image_skia_util_mac.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include "chrome/browser/app_icon_win.h"
#include "ui/gfx/icon_util.h"
#endif  // defined(OS_WIN)

namespace task_management {

namespace {

gfx::ImageSkia* g_default_icon = nullptr;

gfx::ImageSkia* GetDefaultIcon() {
  if (!g_default_icon) {
#if defined(OS_WIN)
    HICON icon = GetAppIcon();
    if (icon) {
      scoped_ptr<SkBitmap> bitmap(IconUtil::CreateSkBitmapFromHICON(icon));
      g_default_icon = new gfx::ImageSkia(gfx::ImageSkiaRep(*bitmap, 1.0f));
    }
#elif defined(OS_POSIX)
    if (ResourceBundle::HasSharedInstance()) {
      g_default_icon = ResourceBundle::GetSharedInstance().
          GetImageSkiaNamed(IDR_PRODUCT_LOGO_16);
    }
#else
    // TODO(port): Port icon code.
    NOTIMPLEMENTED();
#endif  // defined(OS_WIN)
    if (g_default_icon)
      g_default_icon->MakeThreadSafe();
  }

  return g_default_icon;
}

bool ReportsV8Stats() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kWinHttpProxyResolver) &&
      !command_line->HasSwitch(switches::kSingleProcess);
}

}  // namespace

BrowserProcessTask::BrowserProcessTask()
    : Task(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_WEB_BROWSER_CELL_TEXT),
           GetDefaultIcon(),
           base::GetCurrentProcessHandle()),
       allocated_v8_memory_(-1),
       used_v8_memory_(-1),
       used_sqlite_memory_(-1),
       reports_v8_stats_(ReportsV8Stats()){
}

BrowserProcessTask::~BrowserProcessTask() {
}

void BrowserProcessTask::Refresh(const base::TimeDelta& update_interval) {
  Task::Refresh(update_interval);

  // TODO(afakhry): Add code to skip v8 and sqlite stats update if they have
  // never been requested.
  if (reports_v8_stats_) {
    allocated_v8_memory_ =
        static_cast<int64>(net::ProxyResolverV8::GetTotalHeapSize());
    used_v8_memory_ =
        static_cast<int64>(net::ProxyResolverV8::GetUsedHeapSize());
  }

  used_sqlite_memory_ = static_cast<int64>(sqlite3_memory_used());
}

Task::Type BrowserProcessTask::GetType() const {
  return Task::BROWSER;
}

int BrowserProcessTask::GetChildProcessUniqueID() const {
  return 0;
}

int64 BrowserProcessTask::GetSqliteMemoryUsed() const {
  return used_sqlite_memory_;
}

int64 BrowserProcessTask::GetV8MemoryAllocated() const {
  return allocated_v8_memory_;
}

int64 BrowserProcessTask::GetV8MemoryUsed() const {
  return used_v8_memory_;
}

}  // namespace task_management
