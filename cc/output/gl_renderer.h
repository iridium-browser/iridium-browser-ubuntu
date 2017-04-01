// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_GL_RENDERER_H_
#define CC_OUTPUT_GL_RENDERER_H_

#include <deque>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/output/color_lut_cache.h"
#include "cc/output/context_cache_controller.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer_draw_cache.h"
#include "cc/output/program_binding.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/geometry/quad_f.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace cc {
class GLRendererShaderTest;
class OutputSurface;
class Resource;
class ResourcePool;
class ScopedResource;
class StreamVideoDrawQuad;
class TextureDrawQuad;
class TextureMailboxDeleter;
class StaticGeometryBinding;
class DynamicGeometryBinding;
struct DrawRenderPassDrawQuadParams;

// Class that handles drawing of composited render layers using GL.
class CC_EXPORT GLRenderer : public DirectRenderer {
 public:
  class ScopedUseGrContext;

  GLRenderer(const RendererSettings* settings,
             OutputSurface* output_surface,
             ResourceProvider* resource_provider,
             TextureMailboxDeleter* texture_mailbox_deleter,
             int highp_threshold_min);
  ~GLRenderer() override;

  void SwapBuffers(std::vector<ui::LatencyInfo> latency_info) override;
  void SwapBuffersComplete() override;

  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override;

  virtual bool IsContextLost();

 protected:
  void DidChangeVisibility() override;

  const gfx::QuadF& SharedGeometryQuad() const { return shared_geometry_quad_; }
  const StaticGeometryBinding* SharedGeometry() const {
    return shared_geometry_.get();
  }

  void GetFramebufferPixelsAsync(const DrawingFrame* frame,
                                 const gfx::Rect& rect,
                                 std::unique_ptr<CopyOutputRequest> request);
  void GetFramebufferTexture(unsigned texture_id,
                             const gfx::Rect& device_rect);
  void ReleaseRenderPassTextures();
  enum BoundGeometry { NO_BINDING, SHARED_BINDING, CLIPPED_BINDING };
  void PrepareGeometry(BoundGeometry geometry_to_bind);
  void SetStencilEnabled(bool enabled);
  bool stencil_enabled() const { return stencil_shadow_; }
  void SetBlendEnabled(bool enabled);
  bool blend_enabled() const { return blend_shadow_; }

  bool CanPartialSwap() override;
  void BindFramebufferToOutputSurface(DrawingFrame* frame) override;
  bool BindFramebufferToTexture(DrawingFrame* frame,
                                const ScopedResource* resource) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void PrepareSurfaceForPass(DrawingFrame* frame,
                             SurfaceInitializationMode initialization_mode,
                             const gfx::Rect& render_pass_scissor) override;
  void DoDrawQuad(DrawingFrame* frame,
                  const class DrawQuad*,
                  const gfx::QuadF* draw_region) override;
  void BeginDrawingFrame(DrawingFrame* frame) override;
  void FinishDrawingFrame(DrawingFrame* frame) override;
  bool FlippedFramebuffer(const DrawingFrame* frame) const override;
  bool FlippedRootFramebuffer() const;
  void EnsureScissorTestEnabled() override;
  void EnsureScissorTestDisabled() override;
  void CopyCurrentRenderPassToBitmap(
      DrawingFrame* frame,
      std::unique_ptr<CopyOutputRequest> request) override;
  void FinishDrawingQuadList() override;

  // Returns true if quad requires antialiasing and false otherwise.
  static bool ShouldAntialiasQuad(const gfx::QuadF& device_layer_quad,
                                  bool clipped,
                                  bool force_aa);

  // Inflate the quad and fill edge array for fragment shader.
  // |local_quad| is set to inflated quad. |edge| array is filled with
  // inflated quad's edge data.
  static void SetupQuadForClippingAndAntialiasing(
      const gfx::Transform& device_transform,
      const DrawQuad* quad,
      const gfx::QuadF* device_layer_quad,
      const gfx::QuadF* clip_region,
      gfx::QuadF* local_quad,
      float edge[24]);
  static void SetupRenderPassQuadForClippingAndAntialiasing(
      const gfx::Transform& device_transform,
      const RenderPassDrawQuad* quad,
      const gfx::QuadF* device_layer_quad,
      const gfx::QuadF* clip_region,
      gfx::QuadF* local_quad,
      float edge[24]);

 private:
  friend class GLRendererShaderPixelTest;
  friend class GLRendererShaderTest;

  // If any of the following functions returns false, then it means that drawing
  // is not possible.
  bool InitializeRPDQParameters(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQShadersForBlending(DrawRenderPassDrawQuadParams* params);
  bool UpdateRPDQWithSkiaFilters(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQTexturesForSampling(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQBlendMode(DrawRenderPassDrawQuadParams* params);
  void ChooseRPDQProgram(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQUniforms(DrawRenderPassDrawQuadParams* params);
  void DrawRPDQ(const DrawRenderPassDrawQuadParams& params);

  static void ToGLMatrix(float* gl_matrix, const gfx::Transform& transform);

  void DiscardPixels();
  void ClearFramebuffer(DrawingFrame* frame);
  void SetViewport();

  void DrawDebugBorderQuad(const DrawingFrame* frame,
                           const DebugBorderDrawQuad* quad);
  static bool IsDefaultBlendMode(SkBlendMode blend_mode) {
    return blend_mode == SkBlendMode::kSrcOver;
  }
  bool CanApplyBlendModeUsingBlendFunc(SkBlendMode blend_mode);
  void ApplyBlendModeUsingBlendFunc(SkBlendMode blend_mode);
  void RestoreBlendFuncToDefault(SkBlendMode blend_mode);

  gfx::Rect GetBackdropBoundingBoxForRenderPassQuad(
      DrawingFrame* frame,
      const RenderPassDrawQuad* quad,
      const gfx::Transform& contents_device_transform,
      const FilterOperations* filters,
      const FilterOperations* background_filters,
      const gfx::QuadF* clip_region,
      bool use_aa,
      gfx::Rect* unclipped_rect);
  std::unique_ptr<ScopedResource> GetBackdropTexture(
      DrawingFrame* frame,
      const gfx::Rect& bounding_rect);

  static bool ShouldApplyBackgroundFilters(
      const RenderPassDrawQuad* quad,
      const FilterOperations* background_filters);
  sk_sp<SkImage> ApplyBackgroundFilters(
      const RenderPassDrawQuad* quad,
      const FilterOperations& background_filters,
      ScopedResource* background_texture,
      const gfx::RectF& rect,
      const gfx::RectF& unclipped_rect);

  const TileDrawQuad* CanPassBeDrawnDirectly(const RenderPass* pass) override;

  void DrawRenderPassQuad(DrawingFrame* frame,
                          const RenderPassDrawQuad* quadi,
                          const gfx::QuadF* clip_region);
  void DrawRenderPassQuadInternal(DrawRenderPassDrawQuadParams* params);
  void DrawSolidColorQuad(const DrawingFrame* frame,
                          const SolidColorDrawQuad* quad,
                          const gfx::QuadF* clip_region);
  void DrawStreamVideoQuad(const DrawingFrame* frame,
                           const StreamVideoDrawQuad* quad,
                           const gfx::QuadF* clip_region);
  void DrawTextureQuad(const DrawingFrame* frame,
                       const TextureDrawQuad* quad,
                       const gfx::QuadF* clip_region);
  void EnqueueTextureQuad(const DrawingFrame* frame,
                          const TextureDrawQuad* quad,
                          const gfx::QuadF* clip_region);
  void FlushTextureQuadCache(BoundGeometry flush_binding);
  void DrawTileQuad(const DrawingFrame* frame,
                    const TileDrawQuad* quad,
                    const gfx::QuadF* clip_region);
  void DrawContentQuad(const DrawingFrame* frame,
                       const ContentDrawQuadBase* quad,
                       ResourceId resource_id,
                       const gfx::QuadF* clip_region);
  void DrawContentQuadAA(const DrawingFrame* frame,
                         const ContentDrawQuadBase* quad,
                         ResourceId resource_id,
                         const gfx::Transform& device_transform,
                         const gfx::QuadF& aa_quad,
                         const gfx::QuadF* clip_region);
  void DrawContentQuadNoAA(const DrawingFrame* frame,
                           const ContentDrawQuadBase* quad,
                           ResourceId resource_id,
                           const gfx::QuadF* clip_region);
  void DrawYUVVideoQuad(const DrawingFrame* frame,
                        const YUVVideoDrawQuad* quad,
                        const gfx::QuadF* clip_region);

  void SetShaderOpacity(float opacity, int alpha_location);
  void SetShaderQuadF(const gfx::QuadF& quad, int quad_location);
  void DrawQuadGeometryClippedByQuadF(const DrawingFrame* frame,
                                      const gfx::Transform& draw_transform,
                                      const gfx::RectF& quad_rect,
                                      const gfx::QuadF& clipping_region_quad,
                                      int matrix_location,
                                      const float uv[8]);
  void DrawQuadGeometry(const gfx::Transform& projection_matrix,
                        const gfx::Transform& draw_transform,
                        const gfx::RectF& quad_rect,
                        int matrix_location);
  void SetUseProgram(unsigned program);

  bool MakeContextCurrent();

  void InitializeSharedObjects();
  void CleanupSharedObjects();

  typedef base::Callback<void(std::unique_ptr<CopyOutputRequest> copy_request,
                              bool success)>
      AsyncGetFramebufferPixelsCleanupCallback;
  void FinishedReadback(unsigned source_buffer,
                        unsigned query,
                        const gfx::Size& size);

  void ReinitializeGLState();
  void RestoreGLState();

  void ScheduleCALayers(DrawingFrame* frame);
  void ScheduleOverlays(DrawingFrame* frame);

  // Copies the contents of the render pass draw quad, including filter effects,
  // to an overlay resource, returned in |resource|. The resource is allocated
  // from |overlay_resource_pool_|.
  // The resulting Resource may be larger than the original quad. The new size
  // and position is placed in |new_bounds|.
  void CopyRenderPassDrawQuadToOverlayResource(
      const CALayerOverlay* ca_layer_overlay,
      Resource** resource,
      DrawingFrame* frame,
      gfx::RectF* new_bounds);

  // Schedules the |ca_layer_overlay|, which is guaranteed to have a non-null
  // |rpdq| parameter.
  void ScheduleRenderPassDrawQuad(const CALayerOverlay* ca_layer_overlay,
                                  DrawingFrame* external_frame);

  // Setup/flush all pending overdraw feedback to framebuffer.
  void SetupOverdrawFeedback();
  void FlushOverdrawFeedback(const DrawingFrame* frame,
                             const gfx::Rect& output_rect);
  // Process overdraw feedback from query.
  using OverdrawFeedbackCallback = base::Callback<void(unsigned, int)>;
  void ProcessOverdrawFeedback(std::vector<int>* overdraw,
                               size_t num_expected_results,
                               unsigned query,
                               int multiplier);

  using OverlayResourceLock =
      std::unique_ptr<ResourceProvider::ScopedReadLockGL>;
  using OverlayResourceLockList = std::vector<OverlayResourceLock>;

  // Resources that have been sent to the GPU process, but not yet swapped.
  OverlayResourceLockList pending_overlay_resources_;

  // Resources that should be shortly swapped by the GPU process.
  std::deque<OverlayResourceLockList> swapping_overlay_resources_;

  // Resources that the GPU process has finished swapping. The key is the
  // texture id of the resource.
  std::map<unsigned, OverlayResourceLock> swapped_and_acked_overlay_resources_;

  unsigned offscreen_framebuffer_id_;

  std::unique_ptr<StaticGeometryBinding> shared_geometry_;
  std::unique_ptr<DynamicGeometryBinding> clipped_geometry_;
  gfx::QuadF shared_geometry_quad_;

  // If the requested program has not yet been initialized, this will initialize
  // the program before returning it.
  const Program* GetProgram(const ProgramKey& key);

  // This will return nullptr if the requested program has not yet been
  // initialized.
  const Program* GetProgramIfInitialized(const ProgramKey& key) const;

  std::unordered_map<ProgramKey, std::unique_ptr<Program>, ProgramKeyHash>
      program_cache_;

  gpu::gles2::GLES2Interface* gl_;
  gpu::ContextSupport* context_support_;
  std::unique_ptr<ContextCacheController::ScopedVisibility> context_visibility_;

  TextureMailboxDeleter* texture_mailbox_deleter_;

  gfx::Rect swap_buffer_rect_;
  gfx::Rect scissor_rect_;
  bool is_using_bind_uniform_;
  bool is_scissor_enabled_;
  bool stencil_shadow_;
  bool blend_shadow_;
  unsigned program_shadow_;
  TexturedQuadDrawCache draw_cache_;
  int highp_threshold_min_;
  int highp_threshold_cache_;

  struct PendingAsyncReadPixels;
  std::vector<std::unique_ptr<PendingAsyncReadPixels>>
      pending_async_read_pixels_;

  std::unique_ptr<ResourceProvider::ScopedWriteLockGL>
      current_framebuffer_lock_;
  // This is valid when current_framebuffer_lock_ is not null.
  ResourceFormat current_framebuffer_format_;

  class SyncQuery;
  std::deque<std::unique_ptr<SyncQuery>> pending_sync_queries_;
  std::deque<std::unique_ptr<SyncQuery>> available_sync_queries_;
  std::unique_ptr<SyncQuery> current_sync_query_;
  bool use_discard_framebuffer_;
  bool use_sync_query_;
  bool use_blend_equation_advanced_;
  bool use_blend_equation_advanced_coherent_;

  // Some overlays require that content is copied from a render pass into an
  // overlay resource. This means the GLRenderer needs its own ResourcePool.
  std::unique_ptr<ResourcePool> overlay_resource_pool_;

  // If true, draw a green border after compositing a texture quad using GL.
  bool gl_composited_texture_quad_border_;

  // The method FlippedFramebuffer determines whether the framebuffer associated
  // with a DrawingFrame is flipped. It makes the assumption that the
  // DrawingFrame is being used as part of a render pass. If a DrawingFrame is
  // not being used as part of a render pass, setting it here forces
  // FlippedFramebuffer to return |true|.
  bool force_drawing_frame_framebuffer_unflipped_ = false;

  BoundGeometry bound_geometry_;
  ColorLUTCache color_lut_cache_;

  unsigned offscreen_stencil_renderbuffer_id_ = 0;
  gfx::Size offscreen_stencil_renderbuffer_size_;

  base::WeakPtrFactory<GLRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GLRenderer);
};

}  // namespace cc

#endif  // CC_OUTPUT_GL_RENDERER_H_
