// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/linux/blimp_display_manager.h"

#include "blimp/client/app/compositor/browser_compositor.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/feature/compositor/blimp_compositor_manager.h"
#include "blimp/client/feature/render_widget_feature.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/size.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/x11/x11_window.h"

namespace blimp {
namespace client {

BlimpDisplayManager::BlimpDisplayManager(
    const gfx::Size& window_size,
    BlimpDisplayManagerDelegate* delegate,
    RenderWidgetFeature* render_widget_feature,
    TabControlFeature* tab_control_feature)
    : device_pixel_ratio_(1.f),
      delegate_(delegate),
      tab_control_feature_(tab_control_feature),
      compositor_(base::MakeUnique<BrowserCompositor>()),
      platform_window_(new ui::X11Window(this)) {
  blimp_compositor_manager_ = base::MakeUnique<BlimpCompositorManager>(
      render_widget_feature, BrowserCompositor::GetSurfaceManager(),
      BrowserCompositor::GetGpuMemoryBufferManager(),
      base::Bind(&BrowserCompositor::AllocateSurfaceClientId));
  platform_window_->SetBounds(gfx::Rect(window_size));
  platform_window_->Show();

  tab_control_feature_->SetSizeAndScale(platform_window_->GetBounds().size(),
                                        device_pixel_ratio_);

  compositor_->SetSize(platform_window_->GetBounds().size());
  compositor_->SetContentLayer(blimp_compositor_manager_->layer());
}

BlimpDisplayManager::~BlimpDisplayManager() {}

void BlimpDisplayManager::OnBoundsChanged(const gfx::Rect& new_bounds) {
  compositor_->SetSize(new_bounds.size());
  tab_control_feature_->SetSizeAndScale(new_bounds.size(), device_pixel_ratio_);
}

void BlimpDisplayManager::DispatchEvent(ui::Event* event) {
  // TODO(dtrainor): Look into using web_input_event_aura to translate these to
  // blink events.
}

void BlimpDisplayManager::OnCloseRequest() {
  blimp_compositor_manager_->SetVisible(false);
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  platform_window_->Close();
}

void BlimpDisplayManager::OnClosed() {
  if (delegate_)
    delegate_->OnClosed();
}

void BlimpDisplayManager::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  device_pixel_ratio_ = device_pixel_ratio;
  tab_control_feature_->SetSizeAndScale(platform_window_->GetBounds().size(),
                                        device_pixel_ratio_);

  if (widget != gfx::kNullAcceleratedWidget) {
    blimp_compositor_manager_->SetVisible(true);
    compositor_->SetAcceleratedWidget(widget);
  }
}

void BlimpDisplayManager::OnAcceleratedWidgetDestroyed() {
  blimp_compositor_manager_->SetVisible(false);
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
}

}  // namespace client
}  // namespace blimp
