// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/output_protection_delegate.h"

#include "ash/shell.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromeos {

namespace {

bool GetCurrentDisplayId(content::RenderFrameHost* rfh, int64_t* display_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);
  DCHECK(display_id);

  display::Screen* screen = display::Screen::GetScreen();
  if (!screen)
    return false;
  display::Display display =
      screen->GetDisplayNearestWindow(rfh->GetNativeView());
  *display_id = display.id();
  return true;
}

void DoNothing(bool status) {
}

}  // namespace

OutputProtectionDelegate::OutputProtectionDelegate(int render_process_id,
                                                   int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      window_(nullptr),
      client_id_(display::DisplayConfigurator::kInvalidClientId),
      display_id_(0),
      weak_ptr_factory_(this) {
  // This can be constructed on IO or UI thread.
}

OutputProtectionDelegate::~OutputProtectionDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  display::DisplayConfigurator* configurator =
      ash::Shell::GetInstance()->display_configurator();
  configurator->UnregisterContentProtectionClient(client_id_);

  if (window_)
    window_->RemoveObserver(this);
}

display::DisplayConfigurator::ContentProtectionClientId
OutputProtectionDelegate::GetClientId() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (client_id_ == display::DisplayConfigurator::kInvalidClientId) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
    if (!rfh || !GetCurrentDisplayId(rfh, &display_id_))
      return display::DisplayConfigurator::kInvalidClientId;

    aura::Window* window = rfh->GetNativeView();
    if (!window)
      return display::DisplayConfigurator::kInvalidClientId;

    display::DisplayConfigurator* configurator =
        ash::Shell::GetInstance()->display_configurator();
    client_id_ = configurator->RegisterContentProtectionClient();

    if (client_id_ != display::DisplayConfigurator::kInvalidClientId) {
      window->AddObserver(this);
      window_ = window;
    }
  }
  return client_id_;
}

void OutputProtectionDelegate::QueryStatus(
    const QueryStatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!rfh) {
    LOG(WARNING) << "RenderFrameHost is not alive.";
    callback.Run(false, 0, 0);
    return;
  }

  display::DisplayConfigurator* configurator =
      ash::Shell::GetInstance()->display_configurator();
  configurator->QueryContentProtectionStatus(
      GetClientId(), display_id_,
      base::Bind(&OutputProtectionDelegate::QueryStatusComplete,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void OutputProtectionDelegate::EnableProtection(
    uint32_t desired_method_mask,
    const EnableProtectionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  display::DisplayConfigurator* configurator =
      ash::Shell::GetInstance()->display_configurator();
  configurator->EnableContentProtection(
      GetClientId(), display_id_, desired_method_mask,
      base::Bind(&OutputProtectionDelegate::EnableProtectionComplete,
                 weak_ptr_factory_.GetWeakPtr(), callback));
  desired_method_mask_ = desired_method_mask;
}

void OutputProtectionDelegate::QueryStatusComplete(
    const QueryStatusCallback& callback,
    const display::DisplayConfigurator::QueryProtectionResponse& response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  // TODO(xjz): Investigate whether this check (and the other one above) should
  // be removed.
  if (!rfh) {
    LOG(WARNING) << "RenderFrameHost is not alive.";
    callback.Run(false, 0, 0);
    return;
  }

  callback.Run(response.success, response.link_mask, response.protection_mask);
}

void OutputProtectionDelegate::EnableProtectionComplete(
    const EnableProtectionCallback& callback,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  callback.Run(success);
}

void OutputProtectionDelegate::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!rfh) {
    LOG(WARNING) << "RenderFrameHost is not alive.";
    return;
  }

  int64_t new_display_id = 0;
  if (!GetCurrentDisplayId(rfh, &new_display_id))
    return;
  if (display_id_ == new_display_id)
    return;

  if (desired_method_mask_ != display::CONTENT_PROTECTION_METHOD_NONE) {
    // Display changed and should enable output protections on new display.
    display::DisplayConfigurator* configurator =
        ash::Shell::GetInstance()->display_configurator();
    configurator->EnableContentProtection(GetClientId(), new_display_id,
                                          desired_method_mask_,
                                          base::Bind(&DoNothing));
    configurator->EnableContentProtection(
        GetClientId(), display_id_, display::CONTENT_PROTECTION_METHOD_NONE,
        base::Bind(&DoNothing));
  }
  display_id_ = new_display_id;
}

void OutputProtectionDelegate::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, window_);
  window_->RemoveObserver(this);
  window_ = nullptr;
}

}  // namespace chromeos
