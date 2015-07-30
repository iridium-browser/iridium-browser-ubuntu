// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/render_frame_devtools_agent_host.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_frame_trace_recorder.h"
#include "content/browser/devtools/protocol/devtools_protocol_handler.h"
#include "content/browser/devtools/protocol/dom_handler.h"
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/input_handler.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/browser/devtools/protocol/power_handler.h"
#include "content/browser/devtools/protocol/service_worker_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents_delegate.h"

#if defined(OS_ANDROID)
#include "content/browser/power_save_blocker_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#endif

namespace content {

typedef std::vector<RenderFrameDevToolsAgentHost*> Instances;

namespace {
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;

static RenderFrameDevToolsAgentHost* FindAgentHost(RenderFrameHost* host) {
  if (g_instances == NULL)
    return NULL;
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if ((*it)->HasRenderFrameHost(host))
      return *it;
  }
  return NULL;
}

// Returns RenderFrameDevToolsAgentHost attached to any of RenderFrameHost
// instances associated with |web_contents|
static RenderFrameDevToolsAgentHost* FindAgentHost(WebContents* web_contents) {
  if (g_instances == NULL)
    return NULL;
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if ((*it)->GetWebContents() == web_contents)
      return *it;
  }
  return NULL;
}

bool ShouldCreateDevToolsFor(RenderFrameHost* rfh) {
  return rfh->IsCrossProcessSubframe() || !rfh->GetParent();
}

}  // namespace

scoped_refptr<DevToolsAgentHost>
DevToolsAgentHost::GetOrCreateFor(WebContents* web_contents) {
  RenderFrameDevToolsAgentHost* result = FindAgentHost(web_contents);
  if (!result) {
    // TODO(dgozman): this check should not be necessary. See
    // http://crbug.com/489664.
    if (!web_contents->GetMainFrame())
      return nullptr;
    result = new RenderFrameDevToolsAgentHost(web_contents->GetMainFrame());
  }
  return result;
}

// static
scoped_refptr<DevToolsAgentHost> RenderFrameDevToolsAgentHost::GetOrCreateFor(
    RenderFrameHost* host) {
  RenderFrameDevToolsAgentHost* result = FindAgentHost(host);
  if (!result)
    result = new RenderFrameDevToolsAgentHost(host);
  return result;
}

// static
void RenderFrameDevToolsAgentHost::AppendAgentHostForFrameIfApplicable(
    DevToolsAgentHost::List* result,
    RenderFrameHost* host) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(host);
  if (!rfh->IsRenderFrameLive())
    return;
  if (ShouldCreateDevToolsFor(rfh))
    result->push_back(RenderFrameDevToolsAgentHost::GetOrCreateFor(rfh));
}

// static
bool DevToolsAgentHost::HasFor(WebContents* web_contents) {
  return FindAgentHost(web_contents) != NULL;
}

// static
bool DevToolsAgentHost::IsDebuggerAttached(WebContents* web_contents) {
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(web_contents);
  return agent_host && agent_host->IsAttached();
}

//static
void RenderFrameDevToolsAgentHost::AddAllAgentHosts(
    DevToolsAgentHost::List* result) {
  base::Callback<void(RenderFrameHost*)> callback = base::Bind(
      RenderFrameDevToolsAgentHost::AppendAgentHostForFrameIfApplicable,
      base::Unretained(result));
  for (const auto& wc : WebContentsImpl::GetAllWebContents())
    wc->ForEachFrame(callback);
}

// static
void RenderFrameDevToolsAgentHost::OnCancelPendingNavigation(
    RenderFrameHost* pending,
    RenderFrameHost* current) {
  RenderFrameDevToolsAgentHost* agent_host = FindAgentHost(pending);
  if (!agent_host)
    return;
  agent_host->ReattachToRenderFrameHost(current);
}

RenderFrameDevToolsAgentHost::RenderFrameDevToolsAgentHost(RenderFrameHost* rfh)
    : render_frame_host_(NULL),
      dom_handler_(new devtools::dom::DOMHandler()),
      input_handler_(new devtools::input::InputHandler()),
      inspector_handler_(new devtools::inspector::InspectorHandler()),
      network_handler_(new devtools::network::NetworkHandler()),
      page_handler_(nullptr),
      power_handler_(new devtools::power::PowerHandler()),
      service_worker_handler_(
          new devtools::service_worker::ServiceWorkerHandler()),
      tracing_handler_(new devtools::tracing::TracingHandler(
          devtools::tracing::TracingHandler::Renderer)),
      emulation_handler_(nullptr),
      frame_trace_recorder_(nullptr),
      reattaching_(false) {
  DevToolsProtocolDispatcher* dispatcher = protocol_handler_->dispatcher();
  dispatcher->SetDOMHandler(dom_handler_.get());
  dispatcher->SetInputHandler(input_handler_.get());
  dispatcher->SetInspectorHandler(inspector_handler_.get());
  dispatcher->SetNetworkHandler(network_handler_.get());
  dispatcher->SetPowerHandler(power_handler_.get());
  dispatcher->SetServiceWorkerHandler(service_worker_handler_.get());
  dispatcher->SetTracingHandler(tracing_handler_.get());

  if (!rfh->GetParent()) {
    page_handler_.reset(new devtools::page::PageHandler());
    emulation_handler_.reset(
        new devtools::emulation::EmulationHandler(page_handler_.get()));
    dispatcher->SetPageHandler(page_handler_.get());
    dispatcher->SetEmulationHandler(emulation_handler_.get());
  }

  SetRenderFrameHost(rfh);
  g_instances.Get().push_back(this);
  AddRef();  // Balanced in RenderFrameHostDestroyed.
}

BrowserContext* RenderFrameDevToolsAgentHost::GetBrowserContext() {
  WebContents* contents = web_contents();
  return contents ? contents->GetBrowserContext() : nullptr;
}

WebContents* RenderFrameDevToolsAgentHost::GetWebContents() {
  return web_contents();
}

void RenderFrameDevToolsAgentHost::SendMessageToAgent(IPC::Message* msg) {
  if (!render_frame_host_)
    return;
  msg->set_routing_id(render_frame_host_->GetRoutingID());
  render_frame_host_->Send(msg);
}

void RenderFrameDevToolsAgentHost::OnClientAttached(bool reattached) {
  if (!render_frame_host_)
    return;

  InnerOnClientAttached();

  // TODO(kaznacheev): Move this call back to DevToolsManager when
  // extensions::ProcessManager no longer relies on this notification.
  if (!reattaching_)
    DevToolsAgentHostImpl::NotifyCallbacks(this, true);
}

void RenderFrameDevToolsAgentHost::InnerOnClientAttached() {
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadRawCookies(
      render_frame_host_->GetProcess()->GetID());

#if defined(OS_ANDROID)
  power_save_blocker_.reset(static_cast<PowerSaveBlockerImpl*>(
      PowerSaveBlocker::Create(
          PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
          PowerSaveBlocker::kReasonOther, "DevTools").release()));
  power_save_blocker_->InitDisplaySleepBlocker(
      WebContents::FromRenderFrameHost(render_frame_host_));
#endif

  frame_trace_recorder_.reset(new DevToolsFrameTraceRecorder());
}

void RenderFrameDevToolsAgentHost::OnClientDetached() {
#if defined(OS_ANDROID)
  power_save_blocker_.reset();
#endif
  if (emulation_handler_)
    emulation_handler_->Detached();
  if (page_handler_)
    page_handler_->Detached();
  power_handler_->Detached();
  service_worker_handler_->Detached();
  tracing_handler_->Detached();
  ClientDetachedFromRenderer();

  // TODO(kaznacheev): Move this call back to DevToolsManager when
  // extensions::ProcessManager no longer relies on this notification.
  if (!reattaching_)
    DevToolsAgentHostImpl::NotifyCallbacks(this, false);
}

void RenderFrameDevToolsAgentHost::ClientDetachedFromRenderer() {
  if (!render_frame_host_)
    return;

  InnerClientDetachedFromRenderer();
}

void RenderFrameDevToolsAgentHost::InnerClientDetachedFromRenderer() {
  bool process_has_agents = false;
  RenderProcessHost* render_process_host = render_frame_host_->GetProcess();
  for (Instances::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if (*it == this || !(*it)->IsAttached())
      continue;
    RenderFrameHost* rfh = (*it)->render_frame_host_;
    if (rfh && rfh->GetProcess() == render_process_host)
      process_has_agents = true;
  }

  // We are the last to disconnect from the renderer -> revoke permissions.
  if (!process_has_agents) {
    ChildProcessSecurityPolicyImpl::GetInstance()->RevokeReadRawCookies(
        render_process_host->GetID());
  }
  frame_trace_recorder_.reset();
}

RenderFrameDevToolsAgentHost::~RenderFrameDevToolsAgentHost() {
  Instances::iterator it = std::find(g_instances.Get().begin(),
                                     g_instances.Get().end(),
                                     this);
  if (it != g_instances.Get().end())
    g_instances.Get().erase(it);
}

// TODO(creis): Consider removing this in favor of RenderFrameHostChanged.
void RenderFrameDevToolsAgentHost::AboutToNavigateRenderFrame(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (render_frame_host_ != old_host)
    return;

  // TODO(creis): This will need to be updated for --site-per-process, since
  // RenderViewHost is going away and navigations could happen in any frame.
  if (render_frame_host_ == new_host) {
    RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
        render_frame_host_->GetRenderViewHost());
    if (rvh->render_view_termination_status() ==
            base::TERMINATION_STATUS_STILL_RUNNING)
      return;
  }
  ReattachToRenderFrameHost(new_host);
}

void RenderFrameDevToolsAgentHost::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (old_host == render_frame_host_ && new_host != render_frame_host_) {
    // AboutToNavigateRenderFrame was not called for renderer-initiated
    // navigation.
    ReattachToRenderFrameHost(new_host);
  }
}

void
RenderFrameDevToolsAgentHost::ReattachToRenderFrameHost(RenderFrameHost* rfh) {
  if (!ShouldCreateDevToolsFor(rfh)) {
    DestroyOnRenderFrameGone();
    // |this| may be deleted at this point.
    return;
  }

  DCHECK(!reattaching_);
  reattaching_ = true;
  DisconnectRenderFrameHost();
  ConnectRenderFrameHost(rfh);
  reattaching_ = false;
}

void RenderFrameDevToolsAgentHost::FrameDeleted(RenderFrameHost* rfh) {
  if (rfh != render_frame_host_)
    return;
  DestroyOnRenderFrameGone();
  // |this| may be deleted at this point.
}

void RenderFrameDevToolsAgentHost::RenderFrameDeleted(RenderFrameHost* rfh) {
  FrameDeleted(rfh);
}

void RenderFrameDevToolsAgentHost::DestroyOnRenderFrameGone() {
  DCHECK(render_frame_host_);
  scoped_refptr<RenderFrameDevToolsAgentHost> protect(this);
  if (IsAttached())
    OnClientDetached();
  HostClosed();
  ClearRenderFrameHost();
  Release();
}

void RenderFrameDevToolsAgentHost::RenderProcessGone(
    base::TerminationStatus status) {
  switch(status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
      RenderFrameCrashed();
      break;
    default:
      break;
  }
}

bool RenderFrameDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& message) {
  if (!render_frame_host_)
    return false;
  if (message.type() == ViewHostMsg_SwapCompositorFrame::ID)
    OnSwapCompositorFrame(message);
  return false;
}

bool RenderFrameDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  if (!render_frame_host_ || render_frame_host != render_frame_host_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameDevToolsAgentHost, message)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderFrameDevToolsAgentHost::DidAttachInterstitialPage() {
  if (page_handler_)
    page_handler_->DidAttachInterstitialPage();

  if (!render_frame_host_)
    return;
  // The rvh set in AboutToNavigateRenderFrame turned out to be interstitial.
  // Connect back to the real one.
  WebContents* web_contents =
    WebContents::FromRenderFrameHost(render_frame_host_);
  if (!web_contents)
    return;
  DisconnectRenderFrameHost();
  ConnectRenderFrameHost(web_contents->GetMainFrame());
}

void RenderFrameDevToolsAgentHost::DidDetachInterstitialPage() {
  if (page_handler_)
    page_handler_->DidDetachInterstitialPage();
}

void RenderFrameDevToolsAgentHost::DidCommitProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  service_worker_handler_->UpdateHosts();
}

void RenderFrameDevToolsAgentHost::SetRenderFrameHost(RenderFrameHost* rfh) {
  DCHECK(ShouldCreateDevToolsFor(rfh));
  DCHECK(!render_frame_host_);
  render_frame_host_ = static_cast<RenderFrameHostImpl*>(rfh);
  DCHECK(render_frame_host_);

  WebContentsObserver::Observe(WebContents::FromRenderFrameHost(rfh));
  dom_handler_->SetRenderFrameHost(render_frame_host_);
  input_handler_->SetRenderWidgetHost(
      render_frame_host_->GetRenderWidgetHost());
  network_handler_->SetRenderFrameHost(render_frame_host_);
  service_worker_handler_->SetRenderFrameHost(render_frame_host_);

  if (emulation_handler_)
    emulation_handler_->SetRenderFrameHost(render_frame_host_);
  if (page_handler_)
    page_handler_->SetRenderFrameHost(render_frame_host_);
}

void RenderFrameDevToolsAgentHost::ClearRenderFrameHost() {
  DCHECK(render_frame_host_);
  render_frame_host_ = nullptr;
  dom_handler_->SetRenderFrameHost(nullptr);
  if (emulation_handler_)
    emulation_handler_->SetRenderFrameHost(nullptr);
  input_handler_->SetRenderWidgetHost(nullptr);
  network_handler_->SetRenderFrameHost(nullptr);
  if (page_handler_)
    page_handler_->SetRenderFrameHost(nullptr);
  service_worker_handler_->SetRenderFrameHost(nullptr);
}

void RenderFrameDevToolsAgentHost::DisconnectWebContents() {
  DisconnectRenderFrameHost();
}

void RenderFrameDevToolsAgentHost::ConnectWebContents(WebContents* wc) {
  ConnectRenderFrameHost(wc->GetMainFrame());
}

DevToolsAgentHost::Type RenderFrameDevToolsAgentHost::GetType() {
  return IsChildFrame() ? TYPE_FRAME : TYPE_WEB_CONTENTS;
}

std::string RenderFrameDevToolsAgentHost::GetTitle() {
  if (IsChildFrame())
    return GetURL().spec();
  if (WebContents* web_contents = GetWebContents())
    return base::UTF16ToUTF8(web_contents->GetTitle());
  return "";
}

GURL RenderFrameDevToolsAgentHost::GetURL() {
  WebContents* web_contents = GetWebContents();
  if (web_contents && !IsChildFrame())
    return web_contents->GetVisibleURL();
  return render_frame_host_ ?
      render_frame_host_->GetLastCommittedURL() : GURL();
}

bool RenderFrameDevToolsAgentHost::Activate() {
  if (render_frame_host_) {
    render_frame_host_->GetRenderViewHost()->GetDelegate()->Activate();
    return true;
  }
  return false;
}

bool RenderFrameDevToolsAgentHost::Close() {
  if (web_contents()) {
    web_contents()->ClosePage();
    return true;
  }
  return false;
}

void RenderFrameDevToolsAgentHost::ConnectRenderFrameHost(
    RenderFrameHost* rfh) {
  SetRenderFrameHost(rfh);
  if (IsAttached())
    Reattach();
}

void RenderFrameDevToolsAgentHost::DisconnectRenderFrameHost() {
  ClientDetachedFromRenderer();
  ClearRenderFrameHost();
}

void RenderFrameDevToolsAgentHost::RenderFrameCrashed() {
  inspector_handler_->TargetCrashed();
}

void RenderFrameDevToolsAgentHost::OnSwapCompositorFrame(
    const IPC::Message& message) {
  ViewHostMsg_SwapCompositorFrame::Param param;
  if (!ViewHostMsg_SwapCompositorFrame::Read(&message, &param))
    return;
  if (page_handler_)
    page_handler_->OnSwapCompositorFrame(get<1>(param).metadata);
  if (input_handler_)
    input_handler_->OnSwapCompositorFrame(get<1>(param).metadata);
  if (frame_trace_recorder_) {
    frame_trace_recorder_->OnSwapCompositorFrame(
        render_frame_host_, get<1>(param).metadata);
  }
}

void RenderFrameDevToolsAgentHost::SynchronousSwapCompositorFrame(
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (!render_frame_host_)
    return;
  if (page_handler_)
    page_handler_->OnSwapCompositorFrame(frame_metadata);
  if (input_handler_)
    input_handler_->OnSwapCompositorFrame(frame_metadata);
  if (frame_trace_recorder_) {
    frame_trace_recorder_->OnSwapCompositorFrame(
        render_frame_host_, frame_metadata);
  }
}

bool RenderFrameDevToolsAgentHost::HasRenderFrameHost(
    RenderFrameHost* host) {
  return host == render_frame_host_;
}

void RenderFrameDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    const DevToolsMessageChunk& message) {
  if (!IsAttached() || !render_frame_host_)
    return;
  ProcessChunkedMessageFromAgent(message);
}

bool RenderFrameDevToolsAgentHost::IsChildFrame() {
  return render_frame_host_ && render_frame_host_->GetParent();
}

}  // namespace content
