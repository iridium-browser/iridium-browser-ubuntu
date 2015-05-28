// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/platform_viewport_headless.h"

#include "mojo/converters/geometry/geometry_type_converters.h"

namespace native_viewport {

PlatformViewportHeadless::PlatformViewportHeadless(Delegate* delegate)
    : delegate_(delegate) {
}

PlatformViewportHeadless::~PlatformViewportHeadless() {
}

void PlatformViewportHeadless::Init(const gfx::Rect& bounds) {
  metrics_ = mojo::ViewportMetrics::New();
  metrics_->size = mojo::Size::From(bounds.size());
}

void PlatformViewportHeadless::Show() {
}

void PlatformViewportHeadless::Hide() {
}

void PlatformViewportHeadless::Close() {
  delegate_->OnDestroyed();
}

gfx::Size PlatformViewportHeadless::GetSize() {
  return metrics_->size.To<gfx::Size>();
}

void PlatformViewportHeadless::SetBounds(const gfx::Rect& bounds) {
  metrics_->size = mojo::Size::From(bounds.size());
  delegate_->OnMetricsChanged(metrics_->Clone());
}

// static
scoped_ptr<PlatformViewport> PlatformViewportHeadless::Create(
    Delegate* delegate) {
  return scoped_ptr<PlatformViewport>(
      new PlatformViewportHeadless(delegate)).Pass();
}

}  // namespace native_viewport
