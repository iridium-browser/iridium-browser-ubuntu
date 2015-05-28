// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_DEFERRED_GPU_COMMAND_SERVICE_H_
#define ANDROID_WEBVIEW_BROWSER_DEFERRED_GPU_COMMAND_SERVICE_H_

#include <queue>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"

namespace android_webview {

class ScopedAllowGL {
 public:
  ScopedAllowGL();
  ~ScopedAllowGL();

  static bool IsAllowed();

 private:
  static base::LazyInstance<base::ThreadLocalBoolean> allow_gl;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowGL);
};

class DeferredGpuCommandService
    : public gpu::InProcessCommandBuffer::Service,
      public base::RefCountedThreadSafe<DeferredGpuCommandService> {
 public:
  static void SetInstance();
  static DeferredGpuCommandService* GetInstance();

  void ScheduleTask(const base::Closure& task) override;
  void ScheduleIdleWork(const base::Closure& task) override;
  bool UseVirtualizedGLContexts() override;
  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache()
      override;

  void RunTasks();
  // If |is_idle| is false, this will only run older idle tasks.
  void PerformIdleWork(bool is_idle);
  // Flush the idle queue until it is empty. This is different from
  // PerformIdleWork(is_idle = true), which does not run any newly scheduled
  // idle tasks during the idle run.
  void PerformAllIdleWork();

  void AddRef() const override;
  void Release() const override;

 protected:
  ~DeferredGpuCommandService() override;
  friend class base::RefCountedThreadSafe<DeferredGpuCommandService>;

 private:
  friend class ScopedAllowGL;
  static void RequestProcessGL();

  DeferredGpuCommandService();
  size_t IdleQueueSize();

  base::Lock tasks_lock_;
  std::queue<base::Closure> tasks_;
  std::queue<std::pair<base::Time, base::Closure> > idle_tasks_;

  scoped_refptr<gpu::gles2::ShaderTranslatorCache> shader_translator_cache_;
  DISALLOW_COPY_AND_ASSIGN(DeferredGpuCommandService);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_DEFERRED_GPU_COMMAND_SERVICE_H_
