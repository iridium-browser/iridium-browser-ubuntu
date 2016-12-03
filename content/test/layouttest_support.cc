// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/layouttest_support.h"

#include <stddef.h>
#include <utility>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "cc/output/copy_output_request.h"
#include "cc/test/pixel_test_output_surface.h"
#include "cc/test/test_delegating_output_surface.h"
#include "components/test_runner/test_common.h"
#include "components/test_runner/web_frame_test_proxy.h"
#include "components/test_runner/web_view_test_proxy.h"
#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/common/page_state.h"
#include "content/public/renderer/renderer_gamepad_provider.h"
#include "content/renderer/fetchers/manifest_fetcher.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/history_entry.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/layout_test_dependencies.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/shell/common/shell_switches.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceMotionData.h"
#include "third_party/WebKit/public/platform/modules/device_orientation/WebDeviceOrientationData.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(OS_MACOSX)
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#elif defined(OS_WIN)
#include "content/child/font_warmup_win.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "ui/gfx/win/direct_write.h"
#endif

using blink::WebDeviceMotionData;
using blink::WebDeviceOrientationData;
using blink::WebGamepad;
using blink::WebGamepads;
using blink::WebRect;
using blink::WebSize;

namespace content {

namespace {

base::LazyInstance<ViewProxyCreationCallback>::Leaky
    g_view_test_proxy_callback = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<FrameProxyCreationCallback>::Leaky
    g_frame_test_proxy_callback = LAZY_INSTANCE_INITIALIZER;

using WebViewTestProxyType =
    test_runner::WebViewTestProxy<RenderViewImpl,
                                  CompositorDependencies*,
                                  const ViewMsg_New_Params&>;
using WebFrameTestProxyType =
    test_runner::WebFrameTestProxy<RenderFrameImpl,
                                   const RenderFrameImpl::CreateParams&>;

RenderViewImpl* CreateWebViewTestProxy(CompositorDependencies* compositor_deps,
                                       const ViewMsg_New_Params& params) {
  WebViewTestProxyType* render_view_proxy =
      new WebViewTestProxyType(compositor_deps, params);
  if (g_view_test_proxy_callback == 0)
    return render_view_proxy;
  g_view_test_proxy_callback.Get().Run(render_view_proxy, render_view_proxy);
  return render_view_proxy;
}

RenderFrameImpl* CreateWebFrameTestProxy(
    const RenderFrameImpl::CreateParams& params) {
  WebFrameTestProxyType* render_frame_proxy = new WebFrameTestProxyType(params);
  if (g_frame_test_proxy_callback == 0)
    return render_frame_proxy;
  g_frame_test_proxy_callback.Get().Run(render_frame_proxy, render_frame_proxy);
  return render_frame_proxy;
}

#if defined(OS_WIN)
// DirectWrite only has access to %WINDIR%\Fonts by default. For developer
// side-loading, support kRegisterFontFiles to allow access to additional fonts.
void RegisterSideloadedTypefaces(SkFontMgr* fontmgr) {
  std::vector<std::string> files = switches::GetSideloadFontFiles();
  for (std::vector<std::string>::const_iterator i(files.begin());
       i != files.end();
       ++i) {
    SkTypeface* typeface = fontmgr->createFromFile(i->c_str());
    blink::WebFontRendering::addSideloadedFontForTesting(typeface);
  }
}
#endif  // OS_WIN

}  // namespace

test_runner::WebViewTestProxyBase* GetWebViewTestProxyBase(
    RenderView* render_view) {
  WebViewTestProxyType* render_view_proxy =
      static_cast<WebViewTestProxyType*>(render_view);
  return static_cast<test_runner::WebViewTestProxyBase*>(render_view_proxy);
}

test_runner::WebFrameTestProxyBase* GetWebFrameTestProxyBase(
    RenderFrame* render_frame) {
  WebFrameTestProxyType* render_frame_proxy =
      static_cast<WebFrameTestProxyType*>(render_frame);
  return static_cast<test_runner::WebFrameTestProxyBase*>(render_frame_proxy);
}

void EnableWebTestProxyCreation(
    const ViewProxyCreationCallback& view_proxy_creation_callback,
    const FrameProxyCreationCallback& frame_proxy_creation_callback) {
  g_view_test_proxy_callback.Get() = view_proxy_creation_callback;
  g_frame_test_proxy_callback.Get() = frame_proxy_creation_callback;
  RenderViewImpl::InstallCreateHook(CreateWebViewTestProxy);
  RenderFrameImpl::InstallCreateHook(CreateWebFrameTestProxy);
}

void FetchManifestDoneCallback(std::unique_ptr<ManifestFetcher> fetcher,
                               const FetchManifestCallback& callback,
                               const blink::WebURLResponse& response,
                               const std::string& data) {
  // |fetcher| will be autodeleted here as it is going out of scope.
  callback.Run(response, data);
}

void FetchManifest(blink::WebView* view, const GURL& url,
                   const FetchManifestCallback& callback) {
  ManifestFetcher* fetcher = new ManifestFetcher(url);
  std::unique_ptr<ManifestFetcher> autodeleter(fetcher);

  // Start is called on fetcher which is also bound to the callback.
  // A raw pointer is used instead of a scoped_ptr as base::Passes passes
  // ownership and thus nulls the scoped_ptr. On MSVS this happens before
  // the call to Start, resulting in a crash.
  fetcher->Start(view->mainFrame(),
                 false,
                 base::Bind(&FetchManifestDoneCallback,
                            base::Passed(&autodeleter),
                            callback));
}

void SetMockGamepadProvider(std::unique_ptr<RendererGamepadProvider> provider) {
  RenderThreadImpl::current()
      ->blink_platform_impl()
      ->SetPlatformEventObserverForTesting(blink::WebPlatformEventTypeGamepad,
                                           std::move(provider));
}

void SetMockDeviceLightData(const double data) {
  RendererBlinkPlatformImpl::SetMockDeviceLightDataForTesting(data);
}

void SetMockDeviceMotionData(const WebDeviceMotionData& data) {
  RendererBlinkPlatformImpl::SetMockDeviceMotionDataForTesting(data);
}

void SetMockDeviceOrientationData(const WebDeviceOrientationData& data) {
  RendererBlinkPlatformImpl::SetMockDeviceOrientationDataForTesting(data);
}

namespace {

// Invokes a callback on commit (on the main thread) to obtain the output
// surface that should be used, then asks that output surface to submit the copy
// request at SwapBuffers time.
class CopyRequestSwapPromise : public cc::SwapPromise {
 public:
  using FindOutputSurfaceCallback =
      base::Callback<cc::TestDelegatingOutputSurface*()>;
  CopyRequestSwapPromise(std::unique_ptr<cc::CopyOutputRequest> request,
                         FindOutputSurfaceCallback output_surface_callback)
      : copy_request_(std::move(request)),
        output_surface_callback_(std::move(output_surface_callback)) {}

  // cc::SwapPromise implementation.
  void OnCommit() override {
    output_surface_from_commit_ = output_surface_callback_.Run();
    DCHECK(output_surface_from_commit_);
  }
  void DidActivate() override {}
  void DidSwap(cc::CompositorFrameMetadata*) override {
    output_surface_from_commit_->RequestCopyOfOutput(std::move(copy_request_));
  }
  DidNotSwapAction DidNotSwap(DidNotSwapReason r) override {
    // The compositor should always swap in layout test mode.
    NOTREACHED() << "did not swap for reason " << r;
    return DidNotSwapAction::BREAK_PROMISE;
  }
  int64_t TraceId() const override { return 0; }

 private:
  std::unique_ptr<cc::CopyOutputRequest> copy_request_;
  FindOutputSurfaceCallback output_surface_callback_;
  cc::TestDelegatingOutputSurface* output_surface_from_commit_ = nullptr;
};

}  // namespace

class LayoutTestDependenciesImpl : public LayoutTestDependencies {
 public:
  std::unique_ptr<cc::OutputSurface> CreateOutputSurface(
      int32_t routing_id,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel,
      scoped_refptr<cc::ContextProvider> compositor_context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider,
      CompositorDependencies* deps) override {
    // This is for an offscreen context for the compositor. So the default
    // framebuffer doesn't need alpha, depth, stencil, antialiasing.
    gpu::gles2::ContextCreationAttribHelper attributes;
    attributes.alpha_size = -1;
    attributes.depth_size = 0;
    attributes.stencil_size = 0;
    attributes.samples = 0;
    attributes.sample_buffers = 0;
    attributes.bind_generates_resource = false;
    attributes.lose_context_when_out_of_memory = true;
    const bool automatic_flushes = false;
    const bool support_locking = false;

    bool flipped_output_surface = false;
    std::unique_ptr<cc::OutputSurface> display_output_surface(
        new cc::PixelTestOutputSurface(
            make_scoped_refptr(new ContextProviderCommandBuffer(
                std::move(gpu_channel), gpu::GPU_STREAM_DEFAULT,
                gpu::GpuStreamPriority::NORMAL, gpu::kNullSurfaceHandle,
                GURL("chrome://gpu/"
                     "LayoutTestDependenciesImpl::CreateOutputSurface"),
                automatic_flushes, support_locking, gpu::SharedMemoryLimits(),
                attributes, nullptr,
                command_buffer_metrics::OFFSCREEN_CONTEXT_FOR_TESTING)),
            nullptr, flipped_output_surface));

    auto* task_runner = deps->GetCompositorImplThreadTaskRunner().get();
    bool synchronous_composite = !task_runner;
    if (!task_runner)
      task_runner = base::ThreadTaskRunnerHandle::Get().get();

    cc::LayerTreeSettings settings =
        RenderWidgetCompositor::GenerateLayerTreeSettings(
            *base::CommandLine::ForCurrentProcess(), deps, 1.f);

    auto output_surface = base::MakeUnique<cc::TestDelegatingOutputSurface>(
        std::move(compositor_context_provider),
        std::move(worker_context_provider), std::move(display_output_surface),
        deps->GetSharedBitmapManager(), deps->GetGpuMemoryBufferManager(),
        settings.renderer_settings, task_runner, synchronous_composite,
        false /* force_disable_reclaim_resources */);
    output_surfaces_[routing_id] = output_surface.get();
    return std::move(output_surface);
  }

  std::unique_ptr<cc::SwapPromise> RequestCopyOfOutput(
      int32_t routing_id,
      std::unique_ptr<cc::CopyOutputRequest> request) override {
    // Note that we can't immediately check output_surfaces_, since it may not
    // have been created yet. Instead, we wait until OnCommit to find the
    // currently active OutputSurface for the given RenderWidget routing_id.
    return base::MakeUnique<CopyRequestSwapPromise>(
        std::move(request),
        base::Bind(
            &LayoutTestDependenciesImpl::FindOutputSurface,
            // |this| will still be valid, because its lifetime is tied to
            // RenderThreadImpl, which outlives layout test execution.
            base::Unretained(this), routing_id));
  }

 private:
  cc::TestDelegatingOutputSurface* FindOutputSurface(int32_t routing_id) {
    auto it = output_surfaces_.find(routing_id);
    return it == output_surfaces_.end() ? nullptr : it->second;
  }

  // Entries are not removed, so this map can grow. However, it is only used in
  // layout tests, so this memory usage does not occur in production.
  // Entries in this map will outlive the output surface, because this object is
  // owned by RenderThreadImpl, which outlives layout test execution.
  std::unordered_map<int32_t, cc::TestDelegatingOutputSurface*>
      output_surfaces_;
};

void EnableRendererLayoutTestMode() {
  RenderThreadImpl::current()->set_layout_test_dependencies(
      base::MakeUnique<LayoutTestDependenciesImpl>());

#if defined(OS_WIN)
  RegisterSideloadedTypefaces(SkFontMgr_New_DirectWrite());
#endif
}

void EnableBrowserLayoutTestMode() {
#if defined(OS_MACOSX)
  gpu::ImageTransportSurface::SetAllowOSMesaForTesting(true);
  PopupMenuHelper::DontShowPopupMenuForTesting();
#endif
  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

int GetLocalSessionHistoryLength(RenderView* render_view) {
  return static_cast<RenderViewImpl*>(render_view)->
      GetLocalSessionHistoryLengthForTesting();
}

void SyncNavigationState(RenderView* render_view) {
  // TODO(creis): Add support for testing in OOPIF-enabled modes.
  // See https://crbug.com/477150.
  if (SiteIsolationPolicy::UseSubframeNavigationEntries())
    return;
  static_cast<RenderViewImpl*>(render_view)->SendUpdateState();
}

void SetFocusAndActivate(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)->
      SetFocusAndActivateForTesting(enable);
}

void ForceResizeRenderView(RenderView* render_view,
                           const WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  render_view_impl->ForceResizeForTesting(new_size);
}

void SetDeviceScaleFactor(RenderView* render_view, float factor) {
  static_cast<RenderViewImpl*>(render_view)->
      SetDeviceScaleFactorForTesting(factor);
}

float GetWindowToViewportScale(RenderView* render_view) {
  blink::WebFloatRect rect(0, 0, 1.0f, 0.0);
  static_cast<RenderViewImpl*>(render_view)->convertWindowToViewport(&rect);
  return rect.width;
}

void SetDeviceColorProfile(RenderView* render_view, const std::string& name) {
  std::vector<char> color_profile;

  struct TestColorProfile { // A color spin profile.
    char* data() {
      static unsigned char color_profile_data[] = {
        0x00,0x00,0x01,0xea,0x54,0x45,0x53,0x54,0x00,0x00,0x00,0x00,
        0x6d,0x6e,0x74,0x72,0x52,0x47,0x42,0x20,0x58,0x59,0x5a,0x20,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x61,0x63,0x73,0x70,0x74,0x65,0x73,0x74,0x00,0x00,0x00,0x00,
        0x74,0x65,0x73,0x74,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0xd6,
        0x00,0x01,0x00,0x00,0x00,0x00,0xd3,0x2d,0x74,0x65,0x73,0x74,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,
        0x63,0x70,0x72,0x74,0x00,0x00,0x00,0xf0,0x00,0x00,0x00,0x0d,
        0x64,0x65,0x73,0x63,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x8c,
        0x77,0x74,0x70,0x74,0x00,0x00,0x01,0x8c,0x00,0x00,0x00,0x14,
        0x72,0x58,0x59,0x5a,0x00,0x00,0x01,0xa0,0x00,0x00,0x00,0x14,
        0x67,0x58,0x59,0x5a,0x00,0x00,0x01,0xb4,0x00,0x00,0x00,0x14,
        0x62,0x58,0x59,0x5a,0x00,0x00,0x01,0xc8,0x00,0x00,0x00,0x14,
        0x72,0x54,0x52,0x43,0x00,0x00,0x01,0xdc,0x00,0x00,0x00,0x0e,
        0x67,0x54,0x52,0x43,0x00,0x00,0x01,0xdc,0x00,0x00,0x00,0x0e,
        0x62,0x54,0x52,0x43,0x00,0x00,0x01,0xdc,0x00,0x00,0x00,0x0e,
        0x74,0x65,0x78,0x74,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x64,0x65,0x73,0x63,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x10,0x77,0x68,0x61,0x63,0x6b,0x65,0x64,0x2e,
        0x69,0x63,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0xf3,0x52,
        0x00,0x01,0x00,0x00,0x00,0x01,0x16,0xcc,0x58,0x59,0x5a,0x20,
        0x00,0x00,0x00,0x00,0x00,0x00,0x34,0x8d,0x00,0x00,0xa0,0x2c,
        0x00,0x00,0x0f,0x95,0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,
        0x00,0x00,0x26,0x31,0x00,0x00,0x10,0x2f,0x00,0x00,0xbe,0x9b,
        0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x9c,0x18,
        0x00,0x00,0x4f,0xa5,0x00,0x00,0x04,0xfc,0x63,0x75,0x72,0x76,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x33
      };

      return reinterpret_cast<char*>(color_profile_data);
    }

    size_t size() {
      const size_t kTestColorProfileSizeInBytes = 490u;
      return kTestColorProfileSizeInBytes;
    }
  };

  struct AdobeRGBColorProfile {
    char* data() {
      static unsigned char color_profile_data[] = {
        0x00,0x00,0x02,0x30,0x41,0x44,0x42,0x45,0x02,0x10,0x00,0x00,
        0x6d,0x6e,0x74,0x72,0x52,0x47,0x42,0x20,0x58,0x59,0x5a,0x20,
        0x07,0xd0,0x00,0x08,0x00,0x0b,0x00,0x13,0x00,0x33,0x00,0x3b,
        0x61,0x63,0x73,0x70,0x41,0x50,0x50,0x4c,0x00,0x00,0x00,0x00,
        0x6e,0x6f,0x6e,0x65,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0xd6,
        0x00,0x01,0x00,0x00,0x00,0x00,0xd3,0x2d,0x41,0x44,0x42,0x45,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0a,
        0x63,0x70,0x72,0x74,0x00,0x00,0x00,0xfc,0x00,0x00,0x00,0x32,
        0x64,0x65,0x73,0x63,0x00,0x00,0x01,0x30,0x00,0x00,0x00,0x6b,
        0x77,0x74,0x70,0x74,0x00,0x00,0x01,0x9c,0x00,0x00,0x00,0x14,
        0x62,0x6b,0x70,0x74,0x00,0x00,0x01,0xb0,0x00,0x00,0x00,0x14,
        0x72,0x54,0x52,0x43,0x00,0x00,0x01,0xc4,0x00,0x00,0x00,0x0e,
        0x67,0x54,0x52,0x43,0x00,0x00,0x01,0xd4,0x00,0x00,0x00,0x0e,
        0x62,0x54,0x52,0x43,0x00,0x00,0x01,0xe4,0x00,0x00,0x00,0x0e,
        0x72,0x58,0x59,0x5a,0x00,0x00,0x01,0xf4,0x00,0x00,0x00,0x14,
        0x67,0x58,0x59,0x5a,0x00,0x00,0x02,0x08,0x00,0x00,0x00,0x14,
        0x62,0x58,0x59,0x5a,0x00,0x00,0x02,0x1c,0x00,0x00,0x00,0x14,
        0x74,0x65,0x78,0x74,0x00,0x00,0x00,0x00,0x43,0x6f,0x70,0x79,
        0x72,0x69,0x67,0x68,0x74,0x20,0x32,0x30,0x30,0x30,0x20,0x41,
        0x64,0x6f,0x62,0x65,0x20,0x53,0x79,0x73,0x74,0x65,0x6d,0x73,
        0x20,0x49,0x6e,0x63,0x6f,0x72,0x70,0x6f,0x72,0x61,0x74,0x65,
        0x64,0x00,0x00,0x00,0x64,0x65,0x73,0x63,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x11,0x41,0x64,0x6f,0x62,0x65,0x20,0x52,0x47,
        0x42,0x20,0x28,0x31,0x39,0x39,0x38,0x29,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,
        0x00,0x00,0xf3,0x51,0x00,0x01,0x00,0x00,0x00,0x01,0x16,0xcc,
        0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x63,0x75,0x72,0x76,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x33,0x00,0x00,
        0x63,0x75,0x72,0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
        0x02,0x33,0x00,0x00,0x63,0x75,0x72,0x76,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x01,0x02,0x33,0x00,0x00,0x58,0x59,0x5a,0x20,
        0x00,0x00,0x00,0x00,0x00,0x00,0x9c,0x18,0x00,0x00,0x4f,0xa5,
        0x00,0x00,0x04,0xfc,0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,
        0x00,0x00,0x34,0x8d,0x00,0x00,0xa0,0x2c,0x00,0x00,0x0f,0x95,
        0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x26,0x31,
        0x00,0x00,0x10,0x2f,0x00,0x00,0xbe,0x9c
      };

      return reinterpret_cast<char*>(color_profile_data);
    }

    size_t size() {
      const size_t kAdobeRGBColorProfileSizeInBytes = 560u;
      return kAdobeRGBColorProfileSizeInBytes;
    }
  };

  if (name == "sRGB") {
    color_profile.assign(name.data(), name.data() + name.size());
  } else if (name == "test" || name == "colorSpin") {
    TestColorProfile test;
    color_profile.assign(test.data(), test.data() + test.size());
  } else if (name == "adobeRGB") {
    AdobeRGBColorProfile test;
    color_profile.assign(test.data(), test.data() + test.size());
  }

  static_cast<RenderViewImpl*>(render_view)
      ->GetWidget()
      ->SetDeviceColorProfileForTesting(color_profile);
}

void SetTestBluetoothScanDuration() {
  BluetoothDeviceChooserController::SetTestScanDurationForTesting();
}

void UseSynchronousResizeMode(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)->
      UseSynchronousResizeModeForTesting(enable);
}

void EnableAutoResizeMode(RenderView* render_view,
                          const WebSize& min_size,
                          const WebSize& max_size) {
  static_cast<RenderViewImpl*>(render_view)->
      EnableAutoResizeForTesting(min_size, max_size);
}

void DisableAutoResizeMode(RenderView* render_view, const WebSize& new_size) {
  static_cast<RenderViewImpl*>(render_view)->
      DisableAutoResizeForTesting(new_size);
}

// Returns True if node1 < node2.
bool HistoryEntryCompareLess(HistoryEntry::HistoryNode* node1,
                             HistoryEntry::HistoryNode* node2) {
  base::string16 target1 = node1->item().target();
  base::string16 target2 = node2->item().target();
  return base::CompareCaseInsensitiveASCII(target1, target2) < 0;
}

std::string DumpHistoryItem(HistoryEntry::HistoryNode* node,
                            int indent,
                            bool is_current_index) {
  std::string result;

  const blink::WebHistoryItem& item = node->item();
  if (is_current_index) {
    result.append("curr->");
    result.append(indent - 6, ' '); // 6 == "curr->".length()
  } else {
    result.append(indent, ' ');
  }

  std::string url =
      test_runner::NormalizeLayoutTestURL(item.urlString().utf8());
  result.append(url);
  if (!item.target().isEmpty()) {
    result.append(" (in frame \"");
    result.append(item.target().utf8());
    result.append("\")");
  }
  result.append("\n");

  std::vector<HistoryEntry::HistoryNode*> children = node->children();
  if (!children.empty()) {
    std::sort(children.begin(), children.end(), HistoryEntryCompareLess);
    for (size_t i = 0; i < children.size(); ++i)
      result += DumpHistoryItem(children[i], indent + 4, false);
  }

  return result;
}

std::string DumpBackForwardList(std::vector<PageState>& page_state,
                                size_t current_index) {
  std::string result;
  result.append("\n============== Back Forward List ==============\n");
  for (size_t index = 0; index < page_state.size(); ++index) {
    std::unique_ptr<HistoryEntry> entry(
        PageStateToHistoryEntry(page_state[index]));
    result.append(
        DumpHistoryItem(entry->root_history_node(),
                        8,
                        index == current_index));
  }
  result.append("===============================================\n");
  return result;
}

void SchedulerRunIdleTasks(const base::Closure& callback) {
  blink::scheduler::RendererScheduler* scheduler =
      content::RenderThreadImpl::current()->GetRendererScheduler();
  blink::scheduler::RunIdleTasksForTesting(scheduler, callback);
}

}  // namespace content
