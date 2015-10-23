//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// WindowSurfaceCGL.h: CGL implementation of egl::Surface for windows

#ifndef LIBANGLE_RENDERER_GL_CGL_WINDOWSURFACECGL_H_
#define LIBANGLE_RENDERER_GL_CGL_WINDOWSURFACECGL_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"

namespace rx
{

class WindowSurfaceCGL : public SurfaceGL
{
  public:
    WindowSurfaceCGL();
    ~WindowSurfaceCGL() override;

    egl::Error initialize() override;
    egl::Error makeCurrent() override;

    egl::Error swap() override;
    egl::Error postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height) override;
    egl::Error querySurfacePointerANGLE(EGLint attribute, void **value) override;
    egl::Error bindTexImage(EGLint buffer) override;
    egl::Error releaseTexImage(EGLint buffer) override;
    void setSwapInterval(EGLint interval) override;

    EGLint getWidth() const override;
    EGLint getHeight() const override;

    EGLint isPostSubBufferSupported() const override;
    EGLint getSwapBehavior() const override;
};

}

#endif // LIBANGLE_RENDERER_GL_CGL_WINDOWSURFACECGL_H_
