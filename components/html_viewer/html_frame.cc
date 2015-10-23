// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_frame.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "components/html_viewer/ax_provider_impl.h"
#include "components/html_viewer/blink_basic_type_converters.h"
#include "components/html_viewer/blink_input_events_type_converters.h"
#include "components/html_viewer/blink_text_input_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/geolocation_client_impl.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_frame_delegate.h"
#include "components/html_viewer/html_frame_properties.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/html_viewer/media_factory.h"
#include "components/html_viewer/stats_collection_controller.h"
#include "components/html_viewer/touch_handler.h"
#include "components/html_viewer/web_layer_impl.h"
#include "components/html_viewer/web_layer_tree_view_impl.h"
#include "components/html_viewer/web_storage_namespace_impl.h"
#include "components/html_viewer/web_url_loader_impl.h"
#include "components/view_manager/ids.h"
#include "components/view_manager/public/cpp/scoped_view_ptr.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrameClient.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

using mandoline::HTMLMessageEvent;
using mandoline::HTMLMessageEventPtr;
using mojo::AxProvider;
using mojo::Rect;
using mojo::ServiceProviderPtr;
using mojo::URLResponsePtr;
using mojo::View;
using mojo::WeakBindToRequest;

namespace html_viewer {
namespace {

mandoline::NavigationTargetType WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return mandoline::NAVIGATION_TARGET_TYPE_EXISTING_FRAME;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return mandoline::NAVIGATION_TARGET_TYPE_NEW_FRAME;
    default:
      return mandoline::NAVIGATION_TARGET_TYPE_NO_PREFERENCE;
  }
}

void ConfigureSettings(blink::WebSettings* settings) {
  settings->setCookieEnabled(true);
  settings->setDefaultFixedFontSize(13);
  settings->setDefaultFontSize(16);
  settings->setLoadsImagesAutomatically(true);
  settings->setJavaScriptEnabled(true);
}

HTMLFrame* GetPreviousSibling(HTMLFrame* frame) {
  DCHECK(frame->parent());
  auto iter = std::find(frame->parent()->children().begin(),
                        frame->parent()->children().end(), frame);
  return (iter == frame->parent()->children().begin()) ? nullptr : *(--iter);
}

}  // namespace

HTMLFrame::HTMLFrame(CreateParams* params)
    : frame_tree_manager_(params->manager),
      parent_(params->parent),
      view_(nullptr),
      id_(params->id),
      web_frame_(nullptr),
      web_widget_(nullptr),
      delegate_(params->delegate),
      weak_factory_(this) {
  if (parent_)
    parent_->children_.push_back(this);

  if (params->view && params->view->id() == id_)
    SetView(params->view);

  SetReplicatedFrameStateFromClientProperties(params->properties, &state_);

  if (!parent_) {
    CreateRootWebWidget();
    // This is the root of the tree (aka the main frame).
    // Expected order for creating webframes is:
    // . Create local webframe (first webframe must always be local).
    // . Set as main frame on WebView.
    // . Swap to remote (if not local).
    blink::WebLocalFrame* local_web_frame =
        blink::WebLocalFrame::create(state_.tree_scope, this);
    // We need to set the main frame before creating children so that state is
    // properly set up in blink.
    web_view()->setMainFrame(local_web_frame);
    const gfx::Size size_in_pixels(params->view->bounds().width,
                                   params->view->bounds().height);
    const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
        params->view->viewport_metrics().device_pixel_ratio, size_in_pixels);
    web_widget_->resize(size_in_dips);
    web_frame_ = local_web_frame;
    web_view()->setDeviceScaleFactor(global_state()->device_pixel_ratio());
    if (id_ != params->view->id()) {
      blink::WebRemoteFrame* remote_web_frame =
          blink::WebRemoteFrame::create(state_.tree_scope, this);
      local_web_frame->swap(remote_web_frame);
      web_frame_ = remote_web_frame;
    } else {
      // Collect startup perf data for local main frames in test environments.
      // Child frames aren't tracked, and tracking remote frames is redundant.
      startup_performance_data_collector_ =
          StatsCollectionController::Install(web_frame_, GetLocalRootApp());
    }
  } else if (!params->allow_local_shared_frame && params->view &&
             id_ == params->view->id()) {
    // Frame represents the local frame, and it isn't the root of the tree.
    HTMLFrame* previous_sibling = GetPreviousSibling(this);
    blink::WebFrame* previous_web_frame =
        previous_sibling ? previous_sibling->web_frame() : nullptr;
    DCHECK(!parent_->IsLocal());
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createLocalChild(
        state_.tree_scope, state_.name, state_.sandbox_flags, this,
        previous_web_frame);
    CreateLocalRootWebWidget(web_frame_->toWebLocalFrame());
  } else if (!parent_->IsLocal()) {
    web_frame_ = parent_->web_frame()->toWebRemoteFrame()->createRemoteChild(
        state_.tree_scope, state_.name, state_.sandbox_flags, this);
  } else {
    // TODO(sky): this DCHECK, and |allow_local_shared_frame| should be
    // moved to HTMLFrameTreeManager. It makes more sense there.
    // This should never happen (if we create a local child we don't call
    // Init(), and the frame server should not being creating child frames of
    // this frame).
    DCHECK(params->allow_local_shared_frame);

    blink::WebLocalFrame* child_web_frame =
        blink::WebLocalFrame::create(state_.tree_scope, this);
    web_frame_ = child_web_frame;
    parent_->web_frame_->appendChild(child_web_frame);
  }

  if (!IsLocal()) {
    blink::WebRemoteFrame* remote_web_frame = web_frame_->toWebRemoteFrame();
    if (remote_web_frame) {
      remote_web_frame->setReplicatedOrigin(state_.origin);
      remote_web_frame->setReplicatedName(state_.name);
    }
  }
}

void HTMLFrame::Close() {
  if (web_widget_) {
    // Closing the root widget (WebView) implicitly detaches. For children
    // (which have a WebFrameWidget) a detach() is required. Use a temporary
    // as if 'this' is the root the call to web_widget_->close() deletes
    // 'this'.
    const bool is_child = parent_ != nullptr;
    web_widget_->close();
    if (is_child)
      web_frame_->detach();
  } else {
    web_frame_->detach();
  }
}

const HTMLFrame* HTMLFrame::FindFrame(uint32_t id) const {
  if (id == id_)
    return this;

  for (const HTMLFrame* child : children_) {
    const HTMLFrame* match = child->FindFrame(id);
    if (match)
      return match;
  }
  return nullptr;
}

blink::WebView* HTMLFrame::web_view() {
  return web_widget_ && web_widget_->isWebView()
             ? static_cast<blink::WebView*>(web_widget_)
             : nullptr;
}

bool HTMLFrame::HasLocalDescendant() const {
  if (IsLocal())
    return true;

  for (HTMLFrame* child : children_) {
    if (child->HasLocalDescendant())
      return true;
  }
  return false;
}

HTMLFrame::~HTMLFrame() {
  DCHECK(children_.empty());

  if (parent_) {
    auto iter =
        std::find(parent_->children_.begin(), parent_->children_.end(), this);
    parent_->children_.erase(iter);
  }
  parent_ = nullptr;

  frame_tree_manager_->OnFrameDestroyed(this);

  if (view_) {
    view_->RemoveObserver(this);
    mojo::ScopedViewPtr::DeleteViewOrViewManager(view_);
  }
}

void HTMLFrame::Bind(mandoline::FrameTreeServerPtr frame_tree_server,
                     mojo::InterfaceRequest<mandoline::FrameTreeClient>
                         frame_tree_client_request) {
  DCHECK(IsLocal());
  // TODO(sky): error handling.
  server_ = frame_tree_server.Pass();
  frame_tree_client_binding_.reset(
      new mojo::Binding<mandoline::FrameTreeClient>(
          this, frame_tree_client_request.Pass()));
}

void HTMLFrame::SetValueFromClientProperty(const std::string& name,
                                           mojo::Array<uint8_t> new_data) {
  if (IsLocal())
    return;

  // Only the name and origin dynamically change.
  if (name == kPropertyFrameOrigin) {
    state_.origin = FrameOriginFromClientProperty(new_data);
    web_frame_->toWebRemoteFrame()->setReplicatedOrigin(state_.origin);
  } else if (name == kPropertyFrameName) {
    state_.name = FrameNameFromClientProperty(new_data);
    web_frame_->toWebRemoteFrame()->setReplicatedName(state_.name);
  }
}

bool HTMLFrame::IsLocal() const {
  return web_frame_->isWebLocalFrame();
}

HTMLFrame* HTMLFrame::GetLocalRoot() {
  HTMLFrame* frame = this;
  while (frame && !frame->delegate_)
    frame = frame->parent_;
  return frame;
}

mojo::ApplicationImpl* HTMLFrame::GetLocalRootApp() {
  return GetLocalRoot()->delegate_->GetApp();
}

mandoline::FrameTreeServer* HTMLFrame::GetFrameTreeServer() {
  // Prefer the local root.
  HTMLFrame* local_root = GetLocalRoot();
  if (local_root)
    return local_root->server_.get();

  // No local root. This means we're a remote frame with no local frame
  // ancestors. Use the local frame from the FrameTreeServer.
  return frame_tree_manager_->local_root_->server_.get();
}

void HTMLFrame::SetView(mojo::View* view) {
  if (view_)
    view_->RemoveObserver(this);
  view_ = view;
  if (view_)
    view_->AddObserver(this);
}

void HTMLFrame::CreateRootWebWidget() {
  DCHECK(!web_widget_);
  blink::WebViewClient* web_view_client =
      (view_ && view_->id() == id_) ? this : nullptr;
  web_widget_ = blink::WebView::create(web_view_client);

  InitializeWebWidget();

  ConfigureSettings(web_view()->settings());
}

void HTMLFrame::CreateLocalRootWebWidget(blink::WebLocalFrame* local_frame) {
  DCHECK(!web_widget_);
  DCHECK(IsLocal());
  web_widget_ = blink::WebFrameWidget::create(this, local_frame);

  InitializeWebWidget();
}

void HTMLFrame::InitializeWebWidget() {
  // Creating the widget calls initializeLayerTreeView() to create the
  // |web_layer_tree_view_impl_|. As we haven't yet assigned the |web_widget_|
  // we have to set it here.
  if (web_layer_tree_view_impl_) {
    web_layer_tree_view_impl_->set_widget(web_widget_);
    web_layer_tree_view_impl_->set_view(view_);
    UpdateWebViewSizeFromViewSize();
  }
}

void HTMLFrame::UpdateFocus() {
  if (!web_widget_ || !view_)
    return;
  const bool is_focused = view_ && view_->HasFocus();
  web_widget_->setFocus(is_focused);
  if (web_widget_->isWebView())
    static_cast<blink::WebView*>(web_widget_)->setIsActive(is_focused);
}

void HTMLFrame::UpdateWebViewSizeFromViewSize() {
  if (!web_widget_ || !view_)
    return;

  const gfx::Size size_in_pixels(view_->bounds().width, view_->bounds().height);
  const gfx::Size size_in_dips = gfx::ConvertSizeToDIP(
      view_->viewport_metrics().device_pixel_ratio, size_in_pixels);
  web_widget_->resize(
      blink::WebSize(size_in_dips.width(), size_in_dips.height()));
  web_layer_tree_view_impl_->setViewportSize(size_in_pixels);
}

void HTMLFrame::SwapToRemote() {
  DCHECK(IsLocal());

  HTMLFrameDelegate* delegate = delegate_;
  delegate_ = nullptr;

  blink::WebRemoteFrame* remote_frame =
      blink::WebRemoteFrame::create(state_.tree_scope, this);
  remote_frame->initializeFromFrame(web_frame_->toWebLocalFrame());
  // swap() ends up calling us back and we then close the frame, which deletes
  // it.
  web_frame_->swap(remote_frame);
  // TODO(sky): this isn't quite right, but WebLayerImpl is temporary.
  if (owned_view_) {
    web_layer_.reset(
        new WebLayerImpl(owned_view_->view(),
                         global_state()->device_pixel_ratio()));
  }
  remote_frame->setRemoteWebLayer(web_layer_.get());
  remote_frame->setReplicatedName(state_.name);
  remote_frame->setReplicatedOrigin(state_.origin);
  remote_frame->setReplicatedSandboxFlags(state_.sandbox_flags);
  web_frame_ = remote_frame;
  SetView(nullptr);
  if (delegate)
    delegate->OnFrameSwappedToRemote();
}

void HTMLFrame::SwapToLocal(
    HTMLFrameDelegate* delegate,
    mojo::View* view,
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties) {
  CHECK(!IsLocal());
  // It doesn't make sense for the root to swap to local.
  CHECK(parent_);
  delegate_ = delegate;
  SetView(view);
  SetReplicatedFrameStateFromClientProperties(properties, &state_);
  blink::WebLocalFrame* local_web_frame =
      blink::WebLocalFrame::create(state_.tree_scope, this);
  local_web_frame->initializeToReplaceRemoteFrame(
      web_frame_->toWebRemoteFrame(), state_.name, state_.sandbox_flags);
  // The swap() ends up calling to frameDetached() and deleting the old.
  web_frame_->swap(local_web_frame);
  web_frame_ = local_web_frame;

  web_layer_.reset();
}

HTMLFrame* HTMLFrame::FindFrameWithWebFrame(blink::WebFrame* web_frame) {
  if (web_frame_ == web_frame)
    return this;
  for (HTMLFrame* child_frame : children_) {
    HTMLFrame* result = child_frame->FindFrameWithWebFrame(web_frame);
    if (result)
      return result;
  }
  return nullptr;
}

void HTMLFrame::FrameDetachedImpl(blink::WebFrame* web_frame) {
  DCHECK_EQ(web_frame_, web_frame);

  while (!children_.empty()) {
    HTMLFrame* child = children_.front();
    child->Close();
    DCHECK(children_.empty() || children_.front() != child);
  }

  if (web_frame->parent())
    web_frame->parent()->removeChild(web_frame);

  delete this;
}

void HTMLFrame::OnViewBoundsChanged(View* view,
                                    const Rect& old_bounds,
                                    const Rect& new_bounds) {
  DCHECK_EQ(view, view_);
  UpdateWebViewSizeFromViewSize();
}

void HTMLFrame::OnViewDestroyed(View* view) {
  DCHECK_EQ(view, view_);
  view_->RemoveObserver(this);
  view_ = nullptr;
  Close();
}

void HTMLFrame::OnViewInputEvent(View* view, const mojo::EventPtr& event) {
  if (event->pointer_data) {
    // Blink expects coordintes to be in DIPs.
    event->pointer_data->x /= global_state()->device_pixel_ratio();
    event->pointer_data->y /= global_state()->device_pixel_ratio();
    event->pointer_data->screen_x /= global_state()->device_pixel_ratio();
    event->pointer_data->screen_y /= global_state()->device_pixel_ratio();
  }

  if (!touch_handler_ && web_widget_)
    touch_handler_.reset(new TouchHandler(web_widget_));

  if ((event->action == mojo::EVENT_TYPE_POINTER_DOWN ||
       event->action == mojo::EVENT_TYPE_POINTER_UP ||
       event->action == mojo::EVENT_TYPE_POINTER_CANCEL ||
       event->action == mojo::EVENT_TYPE_POINTER_MOVE) &&
      event->pointer_data->kind == mojo::POINTER_KIND_TOUCH) {
    touch_handler_->OnTouchEvent(*event);
    return;
  }

  if (!web_widget_)
    return;

  scoped_ptr<blink::WebInputEvent> web_event =
      event.To<scoped_ptr<blink::WebInputEvent>>();
  if (web_event)
    web_widget_->handleInputEvent(*web_event);
}

void HTMLFrame::OnViewFocusChanged(mojo::View* gained_focus,
                                   mojo::View* lost_focus) {
  UpdateFocus();
}

void HTMLFrame::OnConnect(mandoline::FrameTreeServerPtr server,
                          uint32_t change_id,
                          mojo::Array<mandoline::FrameDataPtr> frame_data) {
  // OnConnect() is only sent once, and has been received (by
  // DocumentResourceWaiter) by the time we get here.
  NOTREACHED();
}

void HTMLFrame::OnFrameAdded(uint32_t change_id,
                             mandoline::FrameDataPtr frame_data) {
  frame_tree_manager_->ProcessOnFrameAdded(this, change_id, frame_data.Pass());
}

void HTMLFrame::OnFrameRemoved(uint32_t change_id, uint32_t frame_id) {
  frame_tree_manager_->ProcessOnFrameRemoved(this, change_id, frame_id);
}

void HTMLFrame::OnFrameClientPropertyChanged(uint32_t frame_id,
                                             const mojo::String& name,
                                             mojo::Array<uint8_t> new_value) {
  frame_tree_manager_->ProcessOnFrameClientPropertyChanged(this, frame_id, name,
                                                           new_value.Pass());
}

void HTMLFrame::OnPostMessageEvent(uint32_t source_frame_id,
                                   uint32_t target_frame_id,
                                   HTMLMessageEventPtr serialized_event) {
  NOTIMPLEMENTED();  // For message ports.

  HTMLFrame* target = frame_tree_manager_->root_->FindFrame(target_frame_id);
  HTMLFrame* source = frame_tree_manager_->root_->FindFrame(source_frame_id);
  if (!target || !source) {
    DVLOG(1) << "Invalid source or target for PostMessage";
    return;
  }

  if (!target->IsLocal()) {
    DVLOG(1) << "Target for PostMessage is not lot local";
    return;
  }

  blink::WebLocalFrame* target_web_frame =
      target->web_frame_->toWebLocalFrame();

  blink::WebSerializedScriptValue serialized_script_value;
  serialized_script_value = blink::WebSerializedScriptValue::fromString(
      serialized_event->data.To<blink::WebString>());

  blink::WebMessagePortChannelArray channels;

  // Create an event with the message.  The next-to-last parameter to
  // initMessageEvent is the last event ID, which is not used with postMessage.
  blink::WebDOMEvent event =
      target_web_frame->document().createEvent("MessageEvent");
  blink::WebDOMMessageEvent msg_event = event.to<blink::WebDOMMessageEvent>();
  msg_event.initMessageEvent(
      "message",
      // |canBubble| and |cancellable| are always false
      false, false, serialized_script_value,
      serialized_event->source_origin.To<blink::WebString>(),
      source->web_frame_, target_web_frame->document(), "", channels);

  // We must pass in the target_origin to do the security check on this side,
  // since it may have changed since the original postMessage call was made.
  blink::WebSecurityOrigin target_origin;
  if (!serialized_event->target_origin.is_null()) {
    target_origin = blink::WebSecurityOrigin::createFromString(
        serialized_event->target_origin.To<blink::WebString>());
  }
  target_web_frame->dispatchMessageEventWithOriginCheck(target_origin,
                                                        msg_event);
}

void HTMLFrame::OnWillNavigate(uint32_t target_frame_id,
                               const OnWillNavigateCallback& callback) {
  // Assume this process won't service the connection and swap to remote.
  // It's entirely possible this process will service the connection and we
  // don't need to swap, but the naive approach is much simpler.
  HTMLFrame* target = frame_tree_manager_->root_->FindFrame(target_frame_id);
  if (target && target->IsLocal() &&
      target != frame_tree_manager_->local_root_) {
    target->SwapToRemote();
  }
  callback.Run();
}

blink::WebStorageNamespace* HTMLFrame::createSessionStorageNamespace() {
  return new WebStorageNamespaceImpl();
}

void HTMLFrame::didCancelCompositionOnSelectionChange() {
  // TODO(penghuang): Update text input state.
}

void HTMLFrame::didChangeContents() {
  // TODO(penghuang): Update text input state.
}

void HTMLFrame::initializeLayerTreeView() {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:view_manager");
  mojo::SurfacePtr surface;
  GetLocalRootApp()->ConnectToService(request.Pass(), &surface);

  mojo::URLRequestPtr request2(mojo::URLRequest::New());
  request2->url = mojo::String::From("mojo:view_manager");
  mojo::GpuPtr gpu_service;
  GetLocalRootApp()->ConnectToService(request2.Pass(), &gpu_service);
  web_layer_tree_view_impl_.reset(new WebLayerTreeViewImpl(
      global_state()->compositor_thread(),
      global_state()->gpu_memory_buffer_manager(),
      global_state()->raster_thread_helper()->task_graph_runner(),
      surface.Pass(), gpu_service.Pass()));
}

blink::WebLayerTreeView* HTMLFrame::layerTreeView() {
  return web_layer_tree_view_impl_.get();
}

void HTMLFrame::resetInputMethod() {
  // When this method gets called, WebWidgetClient implementation should
  // reset the input method by cancelling any ongoing composition.
  // TODO(penghuang): Reset IME.
}

void HTMLFrame::didHandleGestureEvent(const blink::WebGestureEvent& event,
                                      bool eventCancelled) {
  // Called when a gesture event is handled.
  if (eventCancelled)
    return;

  if (event.type == blink::WebInputEvent::GestureTap) {
    const bool show_ime = true;
    UpdateTextInputState(show_ime);
  } else if (event.type == blink::WebInputEvent::GestureLongPress) {
    // Only show IME if the textfield contains text.
    const bool show_ime =
        !web_view()->textInputInfo().value.isEmpty();
    UpdateTextInputState(show_ime);
  }
}

void HTMLFrame::didUpdateTextOfFocusedElementByNonUserInput() {
  // Called when value of focused textfield gets dirty, e.g. value is
  // modified by script, not by user input.
  const bool show_ime = false;
  UpdateTextInputState(show_ime);
}

void HTMLFrame::showImeIfNeeded() {
  // Request the browser to show the IME for current input type.
  const bool show_ime = true;
  UpdateTextInputState(show_ime);
}

blink::WebMediaPlayer* HTMLFrame::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    blink::WebContentDecryptionModule* initial_cdm) {
  return global_state()->media_factory()->CreateMediaPlayer(
      frame, url, client, encrypted_client, initial_cdm,
      GetLocalRootApp()->shell());
}

blink::WebFrame* HTMLFrame::createChildFrame(
    blink::WebLocalFrame* parent,
    blink::WebTreeScopeType scope,
    const blink::WebString& frame_name,
    blink::WebSandboxFlags sandbox_flags) {
  DCHECK(IsLocal());  // Can't create children of remote frames.
  DCHECK_EQ(parent, web_frame_);
  DCHECK(view_);  // If we're local we have to have a view.
  // Create the view that will house the frame now. We embed once we know the
  // url (see decidePolicyForNavigation()).
  mojo::View* child_view = view_->view_manager()->CreateView();
  ReplicatedFrameState child_state;
  child_state.name = frame_name;
  child_state.tree_scope = scope;
  child_state.sandbox_flags = sandbox_flags;
  mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties;
  client_properties.mark_non_null();
  ClientPropertiesFromReplicatedFrameState(child_state, &client_properties);

  child_view->SetVisible(true);
  view_->AddChild(child_view);

  GetLocalRoot()->server_->OnCreatedFrame(id_, child_view->id(),
                                          client_properties.Pass());

  HTMLFrame::CreateParams params(frame_tree_manager_, this, child_view->id(),
                                 child_view, client_properties, nullptr);
  params.allow_local_shared_frame = true;
  HTMLFrame* child_frame = GetLocalRoot()->delegate_->CreateHTMLFrame(&params);
  child_frame->owned_view_.reset(new mojo::ScopedViewPtr(child_view));
  return child_frame->web_frame_;
}

void HTMLFrame::frameDetached(blink::WebFrame* web_frame,
                              blink::WebFrameClient::DetachType type) {
  if (type == blink::WebFrameClient::DetachType::Swap) {
    web_frame->close();
    return;
  }

  DCHECK(type == blink::WebFrameClient::DetachType::Remove);
  FrameDetachedImpl(web_frame);
}

blink::WebCookieJar* HTMLFrame::cookieJar(blink::WebLocalFrame* frame) {
  // TODO(darin): Blink does not fallback to the Platform provided WebCookieJar.
  // Either it should, as it once did, or we should find another solution here.
  return blink::Platform::current()->cookieJar();
}

blink::WebNavigationPolicy HTMLFrame::decidePolicyForNavigation(
    const NavigationPolicyInfo& info) {
  // Allow the delegate to force a navigation type for the root.
  if (info.frame == web_frame() && this == frame_tree_manager_->root_ &&
      delegate_ && delegate_->ShouldNavigateLocallyInMainFrame()) {
    return info.defaultPolicy;
  }

  // If we have extraData() it means we already have the url response
  // (presumably because we are being called via Navigate()). In that case we
  // can go ahead and navigate locally.
  if (info.urlRequest.extraData()) {
    DCHECK_EQ(blink::WebNavigationPolicyCurrentTab, info.defaultPolicy);
    return blink::WebNavigationPolicyCurrentTab;
  }

  // Ask the FrameTreeServer to handle the navigation. By returning
  // WebNavigationPolicyIgnore the load is suppressed.
  mojo::URLRequestPtr url_request = mojo::URLRequest::From(info.urlRequest);
  GetLocalRoot()->server_->RequestNavigate(
      WebNavigationPolicyToNavigationTarget(info.defaultPolicy), id_,
      url_request.Pass());
  return blink::WebNavigationPolicyIgnore;
}

void HTMLFrame::didAddMessageToConsole(const blink::WebConsoleMessage& message,
                                       const blink::WebString& source_name,
                                       unsigned source_line,
                                       const blink::WebString& stack_trace) {
  VLOG(1) << "[" << source_name.utf8() << "(" << source_line << ")] "
          << message.text.utf8();
}

void HTMLFrame::didHandleOnloadEvents(blink::WebLocalFrame* frame) {
  static bool recorded = false;
  if (!recorded && startup_performance_data_collector_) {
    startup_performance_data_collector_->SetFirstWebContentsMainFrameLoadTime(
        base::Time::Now().ToInternalValue());
    recorded = true;
  }
}

void HTMLFrame::didFinishLoad(blink::WebLocalFrame* frame) {
  if (GetLocalRoot() == this)
    delegate_->OnFrameDidFinishLoad();
}

void HTMLFrame::didNavigateWithinPage(blink::WebLocalFrame* frame,
                                      const blink::WebHistoryItem& history_item,
                                      blink::WebHistoryCommitType commit_type) {
  GetLocalRoot()->server_->DidNavigateLocally(id_,
                                              history_item.urlString().utf8());
}

void HTMLFrame::didFirstVisuallyNonEmptyLayout(blink::WebLocalFrame* frame) {
  static bool recorded = false;
  if (!recorded && startup_performance_data_collector_) {
    startup_performance_data_collector_->SetFirstVisuallyNonEmptyLayoutTime(
        base::Time::Now().ToInternalValue());
    recorded = true;
  }
}

blink::WebGeolocationClient* HTMLFrame::geolocationClient() {
  if (!geolocation_client_impl_)
    geolocation_client_impl_.reset(new GeolocationClientImpl);
  return geolocation_client_impl_.get();
}

blink::WebEncryptedMediaClient* HTMLFrame::encryptedMediaClient() {
  return global_state()->media_factory()->GetEncryptedMediaClient();
}

void HTMLFrame::didStartLoading(bool to_different_document) {
  GetLocalRoot()->server_->LoadingStarted(id_);
}

void HTMLFrame::didStopLoading() {
  GetLocalRoot()->server_->LoadingStopped(id_);
}

void HTMLFrame::didChangeLoadProgress(double load_progress) {
  GetLocalRoot()->server_->ProgressChanged(id_, load_progress);
}

void HTMLFrame::didChangeName(blink::WebLocalFrame* frame,
                              const blink::WebString& name) {
  state_.name = name;
  GetLocalRoot()->server_->SetClientProperty(id_, kPropertyFrameName,
                                             FrameNameToClientProperty(name));
}

void HTMLFrame::didCommitProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type) {
  state_.origin = FrameOrigin(frame);
  GetLocalRoot()->server_->SetClientProperty(
      id_, kPropertyFrameOrigin, FrameOriginToClientProperty(frame));
}

void HTMLFrame::frameDetached(blink::WebRemoteFrameClient::DetachType type) {
  if (type == blink::WebRemoteFrameClient::DetachType::Swap) {
    web_frame_->close();
    return;
  }

  DCHECK(type == blink::WebRemoteFrameClient::DetachType::Remove);
  FrameDetachedImpl(web_frame_);
}

void HTMLFrame::UpdateTextInputState(bool show_ime) {
  blink::WebTextInputInfo new_info = web_view()->textInputInfo();
  // Only show IME if the focused element is editable.
  show_ime = show_ime && new_info.type != blink::WebTextInputTypeNone;
  if (show_ime || text_input_info_ != new_info) {
    text_input_info_ = new_info;
    mojo::TextInputStatePtr state = mojo::TextInputState::New();
    state->type = mojo::ConvertTo<mojo::TextInputType>(new_info.type);
    state->flags = new_info.flags;
    state->text = mojo::String::From(new_info.value.utf8());
    state->selection_start = new_info.selectionStart;
    state->selection_end = new_info.selectionEnd;
    state->composition_start = new_info.compositionStart;
    state->composition_end = new_info.compositionEnd;
    if (show_ime)
      view_->SetImeVisibility(true, state.Pass());
    else
      view_->SetTextInputState(state.Pass());
  }
}

void HTMLFrame::postMessageEvent(blink::WebLocalFrame* source_web_frame,
                                 blink::WebRemoteFrame* target_web_frame,
                                 blink::WebSecurityOrigin target_origin,
                                 blink::WebDOMMessageEvent web_event) {
  NOTIMPLEMENTED();  // message_ports aren't implemented yet.

  HTMLFrame* source_frame =
      frame_tree_manager_->root_->FindFrameWithWebFrame(source_web_frame);
  DCHECK(source_frame);
  HTMLFrame* target_frame =
      frame_tree_manager_->root_->FindFrameWithWebFrame(target_web_frame);
  DCHECK(target_frame);

  HTMLMessageEventPtr event(HTMLMessageEvent::New());
  event->data = mojo::Array<uint8_t>::From(web_event.data().toString());
  event->source_origin = mojo::String::From(web_event.origin());
  if (!target_origin.isNull())
    event->target_origin = mojo::String::From(target_origin.toString());

  GetFrameTreeServer()->PostMessageEventToFrame(
      source_frame->id_, target_frame->id_, event.Pass());
}

void HTMLFrame::initializeChildFrame(const blink::WebRect& frame_rect,
                                     float scale_factor) {
  // NOTE: |scale_factor| is always 1.
  const gfx::Rect rect_in_dip(frame_rect.x, frame_rect.y, frame_rect.width,
                              frame_rect.height);
  const gfx::Rect rect_in_pixels(gfx::ConvertRectToPixel(
      global_state()->device_pixel_ratio(), rect_in_dip));
  const mojo::RectPtr mojo_rect_in_pixels(mojo::Rect::From(rect_in_pixels));
  view_->SetBounds(*mojo_rect_in_pixels);
}

void HTMLFrame::navigate(const blink::WebURLRequest& request,
                         bool should_replace_current_entry) {
  // TODO: support |should_replace_current_entry|.
  NOTIMPLEMENTED();  // for |should_replace_current_entry
  mojo::URLRequestPtr url_request = mojo::URLRequest::From(request);
  GetFrameTreeServer()->RequestNavigate(
      mandoline::NAVIGATION_TARGET_TYPE_EXISTING_FRAME, id_,
      url_request.Pass());
}

void HTMLFrame::reload(bool ignore_cache, bool is_client_redirect) {
  NOTIMPLEMENTED();
}

void HTMLFrame::forwardInputEvent(const blink::WebInputEvent* event) {
  NOTIMPLEMENTED();
}

}  // namespace mojo
