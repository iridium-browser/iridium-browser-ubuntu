// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class HeadlessWindowManager;

class HeadlessSurfaceFactory : public SurfaceFactoryOzone {
 public:
  HeadlessSurfaceFactory();
  explicit HeadlessSurfaceFactory(HeadlessWindowManager* window_manager);
  ~HeadlessSurfaceFactory() override;

  // SurfaceFactoryOzone:
  std::vector<gl::GLImplementation> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(gl::GLImplementation implementation) override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget w) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;

 private:
  HeadlessWindowManager* window_manager_;

  std::unique_ptr<GLOzone> osmesa_implementation_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_
