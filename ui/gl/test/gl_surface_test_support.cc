// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_surface_test_support.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/platform_window/x11/x11_window.h"
#endif

namespace gl {

// static
void GLSurfaceTestSupport::InitializeOneOff() {
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

#if defined(USE_X11)
  XInitThreads();
  ui::test::SetUseOverrideRedirectWindowByDefault(true);
#endif

  bool use_osmesa = true;

  // We usually use OSMesa as this works on all bots. The command line can
  // override this behaviour to use hardware GL.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    use_osmesa = false;
  }

#if defined(OS_ANDROID)
  // On Android we always use hardware GL.
  use_osmesa = false;
#endif

  std::vector<GLImplementation> allowed_impls =
      init::GetAllowedGLImplementations();
  DCHECK(!allowed_impls.empty());

  GLImplementation impl = allowed_impls[0];
  if (use_osmesa)
    impl = kGLImplementationOSMesaGL;

  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGL))
      << "kUseGL has not effect in tests";

  bool fallback_to_osmesa = false;
  bool gpu_service_logging = false;
  bool disable_gl_drawing = true;

  CHECK(init::InitializeGLOneOffImplementation(
      impl, fallback_to_osmesa, gpu_service_logging, disable_gl_drawing));
}

// static
void GLSurfaceTestSupport::InitializeOneOffImplementation(
    GLImplementation impl,
    bool fallback_to_osmesa) {
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGL))
      << "kUseGL has not effect in tests";

  // This method may be called multiple times in the same process to set up
  // bindings in different ways.
  init::ShutdownGL();

  bool gpu_service_logging = false;
  bool disable_gl_drawing = false;

  CHECK(init::InitializeGLOneOffImplementation(
      impl, fallback_to_osmesa, gpu_service_logging, disable_gl_drawing));
}

// static
void GLSurfaceTestSupport::InitializeOneOffWithMockBindings() {
#if defined(USE_OZONE)
  // This function skips where Ozone is otherwise initialized.
  ui::OzonePlatform::InitializeForGPU();
#endif
  InitializeOneOffImplementation(kGLImplementationMockGL, false);
}

}  // namespace gl
