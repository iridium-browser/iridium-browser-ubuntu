// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/screen_manager_stub_internal.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace {

// Build a ViewportMetric for a 1024x768 display.
ViewportMetrics DefaultViewportMetrics() {
  ViewportMetrics metrics;

  metrics.device_scale_factor = 1.0f;
  if (Display::HasForceDeviceScaleFactor())
    metrics.device_scale_factor = Display::GetForcedDeviceScaleFactor();

  metrics.pixel_size = gfx::Size(1024, 768);
  gfx::Size scaled_size = gfx::ScaleToRoundedSize(
      metrics.pixel_size, 1.0f / metrics.device_scale_factor);

  metrics.bounds = gfx::Rect(scaled_size);
  metrics.work_area = gfx::Rect(scaled_size);

  return metrics;
}

}  // namespace

// static
std::unique_ptr<ScreenManager> ScreenManager::Create() {
  return base::MakeUnique<ScreenManagerStubInternal>();
}

ScreenManagerStubInternal::ScreenManagerStubInternal()
    : weak_ptr_factory_(this) {}

ScreenManagerStubInternal::~ScreenManagerStubInternal() {}

void ScreenManagerStubInternal::FixedSizeScreenConfiguration() {
  delegate_->OnDisplayAdded(display_id_, display_metrics_);
}

void ScreenManagerStubInternal::AddInterfaces(
    service_manager::InterfaceRegistry* registry) {}

void ScreenManagerStubInternal::Init(ScreenManagerDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
  display_metrics_ = DefaultViewportMetrics();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ScreenManagerStubInternal::FixedSizeScreenConfiguration,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ScreenManagerStubInternal::RequestCloseDisplay(int64_t display_id) {
  if (display_id == display_id_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ScreenManagerDelegate::OnDisplayRemoved,
                              base::Unretained(delegate_), display_id));
  }
}

}  // namespace display
