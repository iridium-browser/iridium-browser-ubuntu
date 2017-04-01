// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_CHILD_THREAD_H_
#define CONTENT_GPU_GPU_CHILD_THREAD_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/x_util.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/ui/gpu/interfaces/gpu_main.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class GpuMemoryBufferFactory;
class GpuWatchdogThread;
class SyncPointManager;
}

namespace media {
class MediaGpuChannelManager;
}

namespace sandbox {
class TargetServices;
}

namespace content {
class GpuServiceFactory;
struct EstablishChannelParams;

// The main thread of the GPU child process. There will only ever be one of
// these per process. It does process initialization and shutdown. It forwards
// IPC messages to gpu::GpuChannelManager, which is responsible for issuing
// rendering commands to the GPU.
class GpuChildThread : public ChildThreadImpl,
                       public gpu::GpuChannelManagerDelegate,
                       public base::FieldTrialList::Observer {
 public:
  struct LogMessage {
    int severity;
    std::string header;
    std::string message;
  };
  typedef std::queue<LogMessage> DeferredMessages;

  GpuChildThread(std::unique_ptr<gpu::GpuWatchdogThread> gpu_watchdog_thread,
                 bool dead_on_arrival,
                 const gpu::GPUInfo& gpu_info,
                 const DeferredMessages& deferred_messages,
                 gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory);

  GpuChildThread(const InProcessChildThreadParams& params,
                 const gpu::GPUInfo& gpu_info,
                 gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory);

  ~GpuChildThread() override;

  void Shutdown() override;

  void Init(const base::Time& process_start_time);

  gpu::GpuWatchdogThread* watchdog_thread() { return watchdog_thread_.get(); }

 private:
  void CreateGpuMainService(ui::mojom::GpuMainAssociatedRequest request);

  // ChildThreadImpl:.
  bool Send(IPC::Message* msg) override;
  bool OnControlMessageReceived(const IPC::Message& msg) override;
  bool OnMessageReceived(const IPC::Message& msg) override;

  // gpu::GpuChannelManagerDelegate:
  void SetActiveURL(const GURL& url) override;
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
#if defined(OS_WIN)
  void SendAcceleratedSurfaceCreatedChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override;

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                  const std::string& group_name) override;

  // Message handlers.
  void OnInitialize(const gpu::GpuPreferences& gpu_preferences);
  void OnFinalize();
  void OnCollectGraphicsInfo();
  void OnGetVideoMemoryUsageStats();
  void OnSetVideoMemoryWindowCount(uint32_t window_count);

  void OnClean();
  void OnCrash();
  void OnHang();
  void OnGpuSwitched();

  void OnEstablishChannel(const EstablishChannelParams& params);
  void OnCloseChannel(int32_t client_id);
  void OnLoadedShader(const std::string& shader);
  void OnDestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                int client_id,
                                const gpu::SyncToken& sync_token);
#if defined(OS_ANDROID)
  void OnWakeUpGpu();
  void OnDestroyingVideoSurface(int surface_id);
#endif
  void OnLoseAllContexts();

  void BindServiceFactoryRequest(
      service_manager::mojom::ServiceFactoryRequest request);

  // Set this flag to true if a fatal error occurred before we receive the
  // OnInitialize message, in which case we just declare ourselves DOA.
  const bool dead_on_arrival_;
  base::Time process_start_time_;
  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;

#if defined(OS_WIN)
  // Windows specific client sandbox interface.
  sandbox::TargetServices* target_services_;
#endif

  // Can be null if overridden by ContentGpuClient.
  std::unique_ptr<gpu::SyncPointManager> owned_sync_point_manager_;

  std::unique_ptr<gpu::GpuChannelManager> gpu_channel_manager_;

  std::unique_ptr<media::MediaGpuChannelManager> media_gpu_channel_manager_;

  // Information about the GPU, such as device and vendor ID.
  gpu::GPUInfo gpu_info_;

  // Error messages collected in gpu_main() before the thread is created.
  DeferredMessages deferred_messages_;

  // Whether the GPU thread is running in the browser process.
  bool in_browser_process_;

  // The gpu::GpuMemoryBufferFactory instance used to allocate GpuMemoryBuffers.
  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;

  // ServiceFactory for service_manager::Service hosting.
  std::unique_ptr<GpuServiceFactory> service_factory_;

  // Bindings to the service_manager::mojom::ServiceFactory impl.
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  DISALLOW_COPY_AND_ASSIGN(GpuChildThread);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_CHILD_THREAD_H_
