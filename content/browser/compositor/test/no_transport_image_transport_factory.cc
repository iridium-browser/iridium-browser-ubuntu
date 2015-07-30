// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/test/no_transport_image_transport_factory.h"

#include "cc/output/context_provider.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/common/gpu/client/gl_helper.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/in_process_context_factory.h"

namespace content {

NoTransportImageTransportFactory::NoTransportImageTransportFactory()
    : surface_manager_(UseSurfacesEnabled() ? new cc::SurfaceManager : nullptr),
      // The context factory created here is for unit tests, thus passing in
      // true in constructor.
      context_factory_(
          new ui::InProcessContextFactory(true, surface_manager_.get())) {
}

NoTransportImageTransportFactory::~NoTransportImageTransportFactory() {
  scoped_ptr<GLHelper> lost_gl_helper = gl_helper_.Pass();
  FOR_EACH_OBSERVER(
      ImageTransportFactoryObserver, observer_list_, OnLostResources());
}

ui::ContextFactory* NoTransportImageTransportFactory::GetContextFactory() {
  return context_factory_.get();
}

gfx::GLSurfaceHandle
NoTransportImageTransportFactory::GetSharedSurfaceHandle() {
  return gfx::GLSurfaceHandle();
}

cc::SurfaceManager* NoTransportImageTransportFactory::GetSurfaceManager() {
  return surface_manager_.get();
}

GLHelper* NoTransportImageTransportFactory::GetGLHelper() {
  if (!gl_helper_) {
    context_provider_ = context_factory_->SharedMainThreadContextProvider();
    gl_helper_.reset(new GLHelper(context_provider_->ContextGL(),
                                  context_provider_->ContextSupport()));
  }
  return gl_helper_.get();
}

void NoTransportImageTransportFactory::AddObserver(
    ImageTransportFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void NoTransportImageTransportFactory::RemoveObserver(
    ImageTransportFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

#if defined(OS_MACOSX)
bool NoTransportImageTransportFactory::
    SurfaceShouldNotShowFramesAfterSuspendForRecycle(int surface_id) const {
  return false;
}
#endif

}  // namespace content
