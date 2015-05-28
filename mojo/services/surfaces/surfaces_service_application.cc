// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/surfaces/surfaces_service_application.h"

#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/surfaces/display_factory_impl.h"
#include "mojo/services/surfaces/surfaces_impl.h"
#include "mojo/services/surfaces/surfaces_scheduler.h"

namespace surfaces {

SurfacesServiceApplication::SurfacesServiceApplication()
    : next_id_namespace_(1u) {
}

SurfacesServiceApplication::~SurfacesServiceApplication() {
}

void SurfacesServiceApplication::Initialize(mojo::ApplicationImpl* app) {
  tracing_.Initialize(app);
  scheduler_.reset(new SurfacesScheduler);
}

bool SurfacesServiceApplication::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<mojo::DisplayFactory>(this);
  connection->AddService<mojo::Surface>(this);
  return true;
}

void SurfacesServiceApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::DisplayFactory> request) {
  new DisplayFactoryImpl(&manager_, next_id_namespace_++, scheduler_.get(),
                         request.Pass());
}

void SurfacesServiceApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::Surface> request) {
  new SurfacesImpl(&manager_, next_id_namespace_++, scheduler_.get(),
                   request.Pass());
}

}  // namespace surfaces

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(
      new surfaces::SurfacesServiceApplication);
  return runner.Run(shell_handle);
}
