// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window_linux.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_restrictions.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/graphics/cast_vsync_settings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if defined(USE_AURA)
#include "chromecast/graphics/cast_screen.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/input_method.h"
#endif

namespace chromecast {
namespace shell {

#if defined(USE_AURA)
class CastFillLayout : public aura::LayoutManager {
 public:
  explicit CastFillLayout(aura::Window* root) : root_(root) {}
  ~CastFillLayout() override {}

 private:
  void OnWindowResized() override {}

  void OnWindowAddedToLayout(aura::Window* child) override {
    child->SetBounds(root_->bounds());
  }

  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}

  void OnWindowRemovedFromLayout(aura::Window* child) override {}

  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}

  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* root_;

  DISALLOW_COPY_AND_ASSIGN(CastFillLayout);
};

// An aura::WindowTreeHost that correctly converts input events.
class CastWindowTreeHost : public aura::WindowTreeHostPlatform {
 public:
  CastWindowTreeHost(bool enable_input, const gfx::Rect& bounds);
  ~CastWindowTreeHost() override;

  // aura::WindowTreeHostPlatform implementation:
  void DispatchEvent(ui::Event* event) override;

 private:
  const bool enable_input_;

  DISALLOW_COPY_AND_ASSIGN(CastWindowTreeHost);
};

CastWindowTreeHost::CastWindowTreeHost(bool enable_input,
                                       const gfx::Rect& bounds)
    : WindowTreeHostPlatform(bounds), enable_input_(enable_input) {}

CastWindowTreeHost::~CastWindowTreeHost() {}

void CastWindowTreeHost::DispatchEvent(ui::Event* event) {
  if (!enable_input_) {
    return;
  }

  if (event->IsKeyEvent()) {
    // Convert a RawKeyDown into a character insertion; otherwise
    // the WebContents will ignore most keyboard input.
    GetInputMethod()->DispatchKeyEvent(event->AsKeyEvent());
  } else {
    WindowTreeHostPlatform::DispatchEvent(event);
  }
}
#endif

// static
std::unique_ptr<CastContentWindow> CastContentWindow::Create(
    CastContentWindow::Delegate* delegate) {
  DCHECK(delegate);
  return base::WrapUnique(new CastContentWindowLinux());
}

CastContentWindowLinux::CastContentWindowLinux() : transparent_(false) {}

CastContentWindowLinux::~CastContentWindowLinux() {
#if defined(USE_AURA)
  CastVSyncSettings::GetInstance()->RemoveObserver(this);
  window_tree_host_.reset();
// We don't delete the screen here to avoid a CHECK failure when
// the screen size is queried periodically for metric gathering. b/18101124
#endif
}

void CastContentWindowLinux::SetTransparent() {
  DCHECK(!window_tree_host_);
  transparent_ = true;
}

void CastContentWindowLinux::ShowWebContents(
    content::WebContents* web_contents) {
#if defined(USE_AURA)
  // Aura initialization
  DCHECK(display::Screen::GetScreen());
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  CHECK(aura::Env::GetInstance());
  window_tree_host_.reset(new CastWindowTreeHost(
      CAST_IS_DEBUG_BUILD() /* enable input */, gfx::Rect(display_size)));
  window_tree_host_->InitHost();
  window_tree_host_->window()->Show();
  window_tree_host_->window()->SetLayoutManager(
      new CastFillLayout(window_tree_host_->window()));

  if (transparent_) {
    window_tree_host_->compositor()->SetBackgroundColor(SK_ColorTRANSPARENT);
    window_tree_host_->compositor()->SetHostHasTransparentBackground(true);
  } else {
    window_tree_host_->compositor()->SetBackgroundColor(SK_ColorBLACK);
  }

  CastVSyncSettings::GetInstance()->AddObserver(this);
  window_tree_host_->compositor()->SetAuthoritativeVSyncInterval(
      CastVSyncSettings::GetInstance()->GetVSyncInterval());

  window_tree_host_->Show();

  // Add and show content's view/window
  aura::Window* content_window = web_contents->GetNativeView();
  aura::Window* parent = window_tree_host_->window();
  if (!parent->Contains(content_window)) {
    parent->AddChild(content_window);
  }
  content_window->Show();
#endif
}

std::unique_ptr<content::WebContents> CastContentWindowLinux::CreateWebContents(
    content::BrowserContext* browser_context) {
  CHECK(display::Screen::GetScreen());
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().size();

  content::WebContents::CreateParams create_params(browser_context, NULL);
  create_params.routing_id = MSG_ROUTING_NONE;
  create_params.initial_size = display_size;
  content::WebContents* web_contents =
      content::WebContents::Create(create_params);

#if defined(USE_AURA)
  // Resize window
  aura::Window* content_window = web_contents->GetNativeView();
  content_window->SetBounds(
      gfx::Rect(display_size.width(), display_size.height()));
#endif

  content::WebContentsObserver::Observe(web_contents);
  return base::WrapUnique(web_contents);
}

void CastContentWindowLinux::DidFirstVisuallyNonEmptyPaint() {
  metrics::CastMetricsHelper::GetInstance()->LogTimeToFirstPaint();
}

void CastContentWindowLinux::MediaStartedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPlay();
}

void CastContentWindowLinux::MediaStoppedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPause();
}

void CastContentWindowLinux::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::RenderWidgetHostView* view =
      render_view_host->GetWidget()->GetView();
  if (view) {
    view->SetBackgroundColor(transparent_ ? SK_ColorTRANSPARENT
                                          : SK_ColorBLACK);
  }
}

void CastContentWindowLinux::OnVSyncIntervalChanged(base::TimeDelta interval) {
#if defined(USE_AURA)
  window_tree_host_->compositor()->SetAuthoritativeVSyncInterval(interval);
#endif
}

}  // namespace shell
}  // namespace chromecast
