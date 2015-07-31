// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_widget_host_view_child_frame.h"

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"

namespace content {

RenderWidgetHostViewChildFrame::RenderWidgetHostViewChildFrame(
    RenderWidgetHost* widget_host)
    : host_(RenderWidgetHostImpl::From(widget_host)),
      frame_connector_(NULL) {
  host_->SetView(this);
}

RenderWidgetHostViewChildFrame::~RenderWidgetHostViewChildFrame() {
}

void RenderWidgetHostViewChildFrame::InitAsChild(
    gfx::NativeView parent_view) {
  NOTREACHED();
}

RenderWidgetHost* RenderWidgetHostViewChildFrame::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewChildFrame::SetSize(const gfx::Size& size) {
  host_->WasResized();
}

void RenderWidgetHostViewChildFrame::SetBounds(const gfx::Rect& rect) {
  SetSize(rect.size());
}

void RenderWidgetHostViewChildFrame::Focus() {
}

bool RenderWidgetHostViewChildFrame::HasFocus() const {
  return false;
}

bool RenderWidgetHostViewChildFrame::IsSurfaceAvailableForCopy() const {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewChildFrame::Show() {
  if (!host_->is_hidden())
    return;
  host_->WasShown(ui::LatencyInfo());
}

void RenderWidgetHostViewChildFrame::Hide() {
  if (host_->is_hidden())
    return;
  host_->WasHidden();
}

bool RenderWidgetHostViewChildFrame::IsShowing() {
  return !host_->is_hidden();
}

gfx::Rect RenderWidgetHostViewChildFrame::GetViewBounds() const {
  gfx::Rect rect;
  if (frame_connector_)
    rect = frame_connector_->ChildFrameRect();
  return rect;
}

gfx::Vector2dF RenderWidgetHostViewChildFrame::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewChildFrame::GetNativeView() const {
  NOTREACHED();
  return NULL;
}

gfx::NativeViewId RenderWidgetHostViewChildFrame::GetNativeViewId() const {
  NOTREACHED();
  return 0;
}

gfx::NativeViewAccessible
RenderWidgetHostViewChildFrame::GetNativeViewAccessible() {
  NOTREACHED();
  return NULL;
}

void RenderWidgetHostViewChildFrame::SetBackgroundColor(SkColor color) {
}

gfx::Size RenderWidgetHostViewChildFrame::GetPhysicalBackingSize() const {
  gfx::Size size;
  if (frame_connector_)
    size = frame_connector_->ChildFrameRect().size();
  return size;
}

void RenderWidgetHostViewChildFrame::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::ImeCancelComposition() {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::MovePluginWindows(
    const std::vector<WebPluginGeometry>& moves) {
}

void RenderWidgetHostViewChildFrame::UpdateCursor(const WebCursor& cursor) {
}

void RenderWidgetHostViewChildFrame::SetIsLoading(bool is_loading) {
  NOTREACHED();
}

void RenderWidgetHostViewChildFrame::TextInputTypeChanged(
    ui::TextInputType type,
    ui::TextInputMode input_mode,
    bool can_compose_inline,
    int flags) {
  // TODO(kenrb): Implement.
}

void RenderWidgetHostViewChildFrame::RenderProcessGone(
    base::TerminationStatus status,
    int error_code) {
  if (frame_connector_)
    frame_connector_->RenderProcessGone();
  Destroy();
}

void RenderWidgetHostViewChildFrame::Destroy() {
  if (frame_connector_) {
    frame_connector_->set_view(NULL);
    frame_connector_ = NULL;
  }

  host_->SetView(NULL);
  host_ = NULL;
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RenderWidgetHostViewChildFrame::SetTooltipText(
    const base::string16& tooltip_text) {
}

void RenderWidgetHostViewChildFrame::SelectionChanged(
    const base::string16& text,
    size_t offset,
    const gfx::Range& range) {
}

void RenderWidgetHostViewChildFrame::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
}

#if defined(OS_ANDROID)
void RenderWidgetHostViewChildFrame::LockCompositingSurface() {
}

void RenderWidgetHostViewChildFrame::UnlockCompositingSurface() {
}
#endif

void RenderWidgetHostViewChildFrame::OnSwapCompositorFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::CompositorFrame> frame) {
  last_scroll_offset_ = frame->metadata.root_scroll_offset;
  if (frame_connector_) {
    frame_connector_->ChildFrameCompositorFrameSwapped(
        output_surface_id,
        host_->GetProcess()->GetID(),
        host_->GetRoutingID(),
        frame.Pass());
  }
}

void RenderWidgetHostViewChildFrame::GetScreenInfo(
    blink::WebScreenInfo* results) {
}

gfx::Rect RenderWidgetHostViewChildFrame::GetBoundsInRootWindow() {
  // We do not have any root window specific parts in this view.
  return GetViewBounds();
}

#if defined(USE_AURA)
void RenderWidgetHostViewChildFrame::ProcessAckedTouchEvent(
    const TouchEventWithLatencyInfo& touch,
    InputEventAckState ack_result) {
}
#endif  // defined(USE_AURA)

bool RenderWidgetHostViewChildFrame::LockMouse() {
  return false;
}

void RenderWidgetHostViewChildFrame::UnlockMouse() {
}

uint32_t RenderWidgetHostViewChildFrame::GetSurfaceIdNamespace() {
  // TODO(kenrb): Create SurfaceFactory here when RWHVChildFrame
  // gets compositor surface support.
  return 0;
}

#if defined(OS_MACOSX)
void RenderWidgetHostViewChildFrame::SetActive(bool active) {
}

void RenderWidgetHostViewChildFrame::SetWindowVisibility(bool visible) {
}

void RenderWidgetHostViewChildFrame::WindowFrameChanged() {
}

void RenderWidgetHostViewChildFrame::ShowDefinitionForSelection() {
}

bool RenderWidgetHostViewChildFrame::SupportsSpeech() const {
  return false;
}

void RenderWidgetHostViewChildFrame::SpeakSelection() {
}

bool RenderWidgetHostViewChildFrame::IsSpeaking() const {
  return false;
}

void RenderWidgetHostViewChildFrame::StopSpeaking() {
}

bool RenderWidgetHostViewChildFrame::PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) {
  return false;
}
#endif // defined(OS_MACOSX)

void RenderWidgetHostViewChildFrame::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& /* dst_size */,
    ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  callback.Run(SkBitmap(), READBACK_FAILED);
}

void RenderWidgetHostViewChildFrame::CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) {
  NOTIMPLEMENTED();
  callback.Run(false);
}

bool RenderWidgetHostViewChildFrame::CanCopyToVideoFrame() const {
  return false;
}

bool RenderWidgetHostViewChildFrame::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  return false;
}

gfx::GLSurfaceHandle RenderWidgetHostViewChildFrame::GetCompositingSurface() {
  return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NULL_TRANSPORT);
}

#if defined(OS_WIN)
void RenderWidgetHostViewChildFrame::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
}

gfx::NativeViewId RenderWidgetHostViewChildFrame::GetParentForWindowlessPlugin()
    const {
  return NULL;
}
#endif // defined(OS_WIN)

BrowserAccessibilityManager*
RenderWidgetHostViewChildFrame::CreateBrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate) {
  return BrowserAccessibilityManager::Create(
      BrowserAccessibilityManager::GetEmptyDocument(), delegate);
}

}  // namespace content
