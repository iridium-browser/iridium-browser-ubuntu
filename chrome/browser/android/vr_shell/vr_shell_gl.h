// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class ListValue;
}

namespace blink {
class WebInputEvent;
}

namespace gl {
class GLContext;
class GLSurface;
class ScopedJavaSurface;
class SurfaceTexture;
}

namespace vr_shell {

class UiScene;
class VrController;
class VrInputManager;
class VrShell;
class VrShellRenderer;
struct ContentRectangle;

// This class manages all GLThread owned objects and GL rendering for VrShell.
// It is not threadsafe and must only be used on the GL thread.
class VrShellGl {
 public:
  enum class InputTarget {
    NONE = 0,
    CONTENT,
    UI
  };

  VrShellGl(
      const base::WeakPtr<VrShell>& weak_vr_shell,
      const base::WeakPtr<VrInputManager>& content_input_manager,
      const base::WeakPtr<VrInputManager>& ui_input_manager,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      gvr_context* gvr_api,
      bool initially_web_vr,
      bool reprojected_rendering);
  ~VrShellGl();

  void Initialize();
  void InitializeGl(gfx::AcceleratedWidget window);

  void DrawFrame();

  void OnTriggerEvent();
  void OnPause();
  void OnResume();

  void SetWebVrMode(bool enabled);
  void ContentBoundsChanged(int width, int height);
  void ContentPhysicalBoundsChanged(int width, int height);
  void UIBoundsChanged(int width, int height);
  void UIPhysicalBoundsChanged(int width, int height);
  base::WeakPtr<VrShellGl> GetWeakPtr();

  void UpdateWebVRTextureBounds(const gvr::Rectf& left_bounds,
                                const gvr::Rectf& right_bounds);
  gvr::GvrApi* gvr_api();
  void SetGvrPoseForWebVr(const gvr::Mat4f& pose, uint32_t pose_num);
  gvr::Sizei GetWebVRCompositorSurfaceSize();

  void UpdateScene(std::unique_ptr<base::ListValue> commands);

 private:
  void GvrInit(gvr_context* gvr_api);
  void InitializeRenderer();
  void DrawVrShell(const gvr::Mat4f& head_pose, gvr::Frame &frame);
  void DrawUiView(const gvr::Mat4f* head_pose,
                  const std::vector<const ContentRectangle*>& elements,
                  const gvr::Sizei& render_size, int viewport_offset);
  void DrawElements(const gvr::Mat4f& render_matrix,
                    const std::vector<const ContentRectangle*>& elements);
  void DrawCursor(const gvr::Mat4f& render_matrix);
  void DrawWebVr();
  bool WebVrPoseByteIsValid(int pose_index_byte);

  void UpdateController(const gvr::Vec3f& forward_vector);
  void SendEventsToTarget(InputTarget input_target, int pixel_x,
                          int pixel_y);
  void SendGesture(InputTarget input_target,
                   std::unique_ptr<blink::WebInputEvent> event);

  void OnUIFrameAvailable();
  void OnContentFrameAvailable();

  void UpdateVSyncParameters(const base::TimeTicks timebase,
                             const base::TimeDelta interval);
  void ScheduleNextDrawFrame();

  void ForceExitVr();

  // samplerExternalOES texture data for UI content image.
  int ui_texture_id_ = 0;
  // samplerExternalOES texture data for main content image.
  int content_texture_id_ = 0;

  std::unique_ptr<UiScene> scene_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::SurfaceTexture> ui_surface_texture_;
  scoped_refptr<gl::SurfaceTexture> content_surface_texture_;

  std::unique_ptr<gl::ScopedJavaSurface> ui_surface_;
  std::unique_ptr<gl::ScopedJavaSurface> content_surface_;

  std::unique_ptr<gvr::GvrApi> gvr_api_;
  std::unique_ptr<gvr::BufferViewportList> buffer_viewport_list_;
  std::unique_ptr<gvr::BufferViewport> buffer_viewport_;
  std::unique_ptr<gvr::BufferViewport> headlocked_left_viewport_;
  std::unique_ptr<gvr::BufferViewport> headlocked_right_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_left_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_right_viewport_;
  std::unique_ptr<gvr::SwapChain> swap_chain_;

  // Current sizes for the render buffers.
  gvr::Sizei render_size_primary_;
  gvr::Sizei render_size_headlocked_;

  std::unique_ptr<VrShellRenderer> vr_shell_renderer_;

  bool touch_pending_ = false;
  gvr::Quatf controller_quat_;

  gvr::Vec3f target_point_;
  const ContentRectangle* target_element_ = nullptr;
  InputTarget current_input_target_ = InputTarget::NONE;
  int ui_tex_css_width_ = 0;
  int ui_tex_css_height_ = 0;
  int content_tex_css_width_ = 0;
  int content_tex_css_height_ = 0;
  gvr::Sizei content_tex_physical_size_ = {0, 0};
  gvr::Sizei ui_tex_physical_size_ = {0, 0};

  // The pose ring buffer size must be a power of two to avoid glitches when
  // the pose index wraps around. It should be large enough to handle the
  // current backlog of poses which is 2-3 frames.
  static constexpr int kPoseRingBufferSize = 8;
  std::vector<gvr::Mat4f> webvr_head_pose_;
  std::vector<bool> webvr_head_pose_valid_;
  int webvr_texture_id_ = 0;
  bool web_vr_mode_;
  bool ready_to_draw_ = false;
  bool surfaceless_rendering_;

  std::unique_ptr<VrController> controller_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::CancelableClosure draw_task_;
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;

  base::WeakPtr<VrShell> weak_vr_shell_;
  base::WeakPtr<VrInputManager> content_input_manager_;
  base::WeakPtr<VrInputManager> ui_input_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  base::WeakPtrFactory<VrShellGl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShellGl);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_
