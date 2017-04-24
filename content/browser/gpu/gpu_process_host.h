// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
#define CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ipc/ipc_sender.h"
#include "ipc/message_filter.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/gpu/interfaces/gpu_host.mojom.h"
#include "services/ui/gpu/interfaces/gpu_main.mojom.h"
#include "services/ui/gpu/interfaces/gpu_service.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "url/gurl.h"

namespace base {
class Thread;
}

namespace IPC {
struct ChannelHandle;
}

namespace gpu {
struct GpuPreferences;
class ShaderDiskCache;
struct SyncToken;
}

namespace service_manager {
class InterfaceProvider;
}

namespace content {
class BrowserChildProcessHostImpl;

class GpuProcessHost : public BrowserChildProcessHostDelegate,
                       public IPC::Sender,
                       public ui::mojom::GpuHost,
                       public base::NonThreadSafe {
 public:
  enum GpuProcessKind {
    GPU_PROCESS_KIND_UNSANDBOXED,
    GPU_PROCESS_KIND_SANDBOXED,
    GPU_PROCESS_KIND_COUNT
  };

  typedef base::Callback<void(const IPC::ChannelHandle&, const gpu::GPUInfo&)>
      EstablishChannelCallback;

  struct EstablishChannelRequest {
    EstablishChannelRequest();
    EstablishChannelRequest(const EstablishChannelRequest& other);
    ~EstablishChannelRequest();
    int32_t client_id;
    EstablishChannelCallback callback;
  };

  typedef base::Callback<void(const gfx::GpuMemoryBufferHandle& handle)>
      CreateGpuMemoryBufferCallback;

  static bool gpu_enabled() { return gpu_enabled_; }
  static int gpu_crash_count() { return gpu_crash_count_; }

  // Creates a new GpuProcessHost (if |force_create| is turned on) or gets an
  // existing one, resulting in the launching of a GPU process if required.
  // Returns null on failure. It is not safe to store the pointer once control
  // has returned to the message loop as it can be destroyed. Instead store the
  // associated GPU host ID.  This could return NULL if GPU access is not
  // allowed (blacklisted).
  CONTENT_EXPORT static GpuProcessHost* Get(GpuProcessKind kind,
                                            bool force_create = true);

  // Retrieves a list of process handles for all gpu processes.
  static void GetProcessHandles(
      const GpuDataManager::GetGpuProcessHandlesCallback& callback);

  // Helper function to send the given message to the GPU process on the IO
  // thread. Calls Get and if a host is returned, sends it. |force_create| can
  // be set to force the creation of GpuProcessHost if one doesn't already
  // exist. This function can be called from any thread. Deletes the message if
  // it cannot be sent.
  CONTENT_EXPORT static void SendOnIO(GpuProcessKind kind,
                                      bool force_create,
                                      IPC::Message* message);

  // Helper function to run a callback on the IO thread. The callback receives
  // the appropriate GpuProcessHost instance. If |force_create| is false, and no
  // GpuProcessHost instance exists, then the callback is never called.
  static void CallOnIO(GpuProcessKind kind,
                       bool force_create,
                       const base::Callback<void(GpuProcessHost*)>& callback);

  service_manager::InterfaceProvider* GetRemoteInterfaces();

  // Get the GPU process host for the GPU process with the given ID. Returns
  // null if the process no longer exists.
  static GpuProcessHost* FromID(int host_id);
  int host_id() const { return host_id_; }

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

  // Adds a message filter to the GpuProcessHost's channel.
  void AddFilter(IPC::MessageFilter* filter);

  // Tells the GPU process to create a new channel for communication with a
  // client. Once the GPU process responds asynchronously with the IPC handle
  // and GPUInfo, we call the callback.
  void EstablishGpuChannel(int client_id,
                           uint64_t client_tracing_id,
                           bool preempts,
                           bool allow_view_command_buffers,
                           bool allow_real_time_streams,
                           const EstablishChannelCallback& callback);

  // Tells the GPU process to create a new GPU memory buffer.
  void CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             int client_id,
                             gpu::SurfaceHandle surface_handle,
                             const CreateGpuMemoryBufferCallback& callback);

  // Tells the GPU process to destroy GPU memory buffer.
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token);

#if defined(OS_ANDROID)
  // Tells the GPU process that the given surface is being destroyed so that it
  // can stop using it.
  void SendDestroyingVideoSurface(int surface_id, const base::Closure& done_cb);
#endif

  // What kind of GPU process, e.g. sandboxed or unsandboxed.
  GpuProcessKind kind();

  // Forcefully terminates the GPU process.
  void ForceShutdown();

  // Asks the GPU process to stop by itself.
  void StopGpuProcess();

  void LoadedShader(const std::string& key, const std::string& data);

  ui::mojom::GpuService* gpu_service() { return gpu_service_ptr_.get(); }

 private:
  class ConnectionFilterImpl;

  static bool ValidateHost(GpuProcessHost* host);

  GpuProcessHost(int host_id, GpuProcessKind kind);
  ~GpuProcessHost() override;

  bool Init();

  // Post an IPC message to the UI shim's message handler on the UI thread.
  void RouteOnUIThread(const IPC::Message& message);

  // BrowserChildProcessHostDelegate implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnProcessLaunched() override;
  void OnProcessLaunchFailed(int error_code) override;
  void OnProcessCrashed(int exit_code) override;

  // ui::mojom::GpuHost:
  void DidInitialize(const gpu::GPUInfo& gpu_info) override;
  void DidCreateOffscreenContext(const GURL& url) override;
  void DidDestroyOffscreenContext(const GURL& url) override;
  void DidDestroyChannel(int32_t client_id) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void SetChildSurface(gpu::SurfaceHandle parent,
                       gpu::SurfaceHandle child) override;
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override;

  // Message handlers.
  void OnInitialized(bool result,
                     const gpu::GPUInfo& gpu_info,
                     const gpu::GpuFeatureInfo& gpu_feature_info);
  void OnChannelEstablished(const IPC::ChannelHandle& channel_handle);
  void OnGpuMemoryBufferCreated(const gfx::GpuMemoryBufferHandle& handle);
#if defined(OS_ANDROID)
  void OnDestroyingVideoSurfaceAck(int surface_id);
#endif
  void OnFieldTrialActivated(const std::string& trial_name);

  void CreateChannelCache(int32_t client_id);

  bool LaunchGpuProcess(gpu::GpuPreferences* gpu_preferences);

  void SendOutstandingReplies();

  void BlockLiveOffscreenContexts();

  // Update GPU crash counters.  Disable GPU if crash limit is reached.
  void RecordProcessCrash();

  std::string GetShaderPrefixKey(const std::string& shader);

  // The serial number of the GpuProcessHost / GpuProcessHostUIShim pair.
  int host_id_;

  // These are the channel requests that we have already sent to
  // the GPU process, but haven't heard back about yet.
  std::queue<EstablishChannelRequest> channel_requests_;

  // The pending create gpu memory buffer requests we need to reply to.
  std::queue<CreateGpuMemoryBufferCallback> create_gpu_memory_buffer_requests_;

  // A callback to signal the completion of a SendDestroyingVideoSurface call.
  base::Closure send_destroying_video_surface_done_cb_;

  // Qeueud messages to send when the process launches.
  std::queue<IPC::Message*> queued_messages_;

  // Whether the GPU process is valid, set to false after Send() failed.
  bool valid_;

  // Whether we are running a GPU thread inside the browser process instead
  // of a separate GPU process.
  bool in_process_;

  bool swiftshader_rendering_;
  GpuProcessKind kind_;

  // The GPUInfo for the connected process. Only valid after initialized_ is
  // true.
  gpu::GPUInfo gpu_info_;

  std::unique_ptr<base::Thread> in_process_gpu_thread_;

  // Whether we actually launched a GPU process.
  bool process_launched_;

  // Whether the GPU process successfully initialized.
  bool initialized_;

  // Time Init started.  Used to log total GPU process startup time to UMA.
  base::TimeTicks init_start_time_;

  // Master switch for enabling/disabling GPU acceleration for the current
  // browser session. It does not change the acceleration settings for
  // existing tabs, just the future ones.
  static bool gpu_enabled_;

  static bool hardware_gpu_enabled_;

  static int gpu_crash_count_;
  static int gpu_recent_crash_count_;
  static bool crashed_before_;
  static int swiftshader_crash_count_;

  std::unique_ptr<BrowserChildProcessHostImpl> process_;

  // Track the URLs of the pages which have live offscreen contexts,
  // assumed to be associated with untrusted content such as WebGL.
  // For best robustness, when any context lost notification is
  // received, assume all of these URLs are guilty, and block
  // automatic execution of 3D content from those domains.
  std::multiset<GURL> urls_with_live_offscreen_contexts_;

  typedef std::map<int32_t, scoped_refptr<gpu::ShaderDiskCache>>
      ClientIdToShaderCacheMap;
  ClientIdToShaderCacheMap client_id_to_shader_cache_;

  std::string shader_prefix_key_info_;

  ui::mojom::GpuMainAssociatedPtr gpu_main_ptr_;
  ui::mojom::GpuServicePtr gpu_service_ptr_;
  mojo::Binding<ui::mojom::GpuHost> gpu_host_binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_H_
