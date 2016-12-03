// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACE_FACTORY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class DrmThreadProxy;
class GbmDevice;
class GbmSurfaceless;

class GbmSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit GbmSurfaceFactory(DrmThreadProxy* drm_thread);
  ~GbmSurfaceFactory() override;

  void RegisterSurface(gfx::AcceleratedWidget widget, GbmSurfaceless* surface);
  void UnregisterSurface(gfx::AcceleratedWidget widget);
  GbmSurfaceless* GetSurface(gfx::AcceleratedWidget widget) const;

  // SurfaceFactoryOzone:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLImplementation implementation,
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gl::GLImplementation implementation,
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLImplementation implementation,
      const gfx::Size& size) override;

  intptr_t GetNativeDisplay() override;
  std::vector<gfx::BufferFormat> GetScanoutFormats(
      gfx::AcceleratedWidget widget) override;
  bool LoadEGLGLES2Bindings() override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<ui::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  scoped_refptr<NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      const gfx::NativePixmapHandle& handle) override;

 private:
  base::ThreadChecker thread_checker_;

  DrmThreadProxy* drm_thread_;

  std::map<gfx::AcceleratedWidget, GbmSurfaceless*> widget_to_surface_map_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACE_FACTORY_H_
