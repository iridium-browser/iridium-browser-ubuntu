// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_proxy_host.h"

#include "base/lazy_instance.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_message.h"

namespace content {

namespace {

// The (process id, routing id) pair that identifies one RenderFrameProxy.
typedef std::pair<int32, int32> RenderFrameProxyHostID;
typedef base::hash_map<RenderFrameProxyHostID, RenderFrameProxyHost*>
    RoutingIDFrameProxyMap;
base::LazyInstance<RoutingIDFrameProxyMap> g_routing_id_frame_proxy_map =
  LAZY_INSTANCE_INITIALIZER;

}

// static
RenderFrameProxyHost* RenderFrameProxyHost::FromID(int process_id,
                                                   int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameProxyMap* frames = g_routing_id_frame_proxy_map.Pointer();
  RoutingIDFrameProxyMap::iterator it = frames->find(
      RenderFrameProxyHostID(process_id, routing_id));
  return it == frames->end() ? NULL : it->second;
}

RenderFrameProxyHost::RenderFrameProxyHost(SiteInstance* site_instance,
                                           FrameTreeNode* frame_tree_node)
    : routing_id_(site_instance->GetProcess()->GetNextRoutingID()),
      site_instance_(site_instance),
      process_(site_instance->GetProcess()),
      frame_tree_node_(frame_tree_node),
      render_frame_proxy_created_(false) {
  GetProcess()->AddRoute(routing_id_, this);
  CHECK(g_routing_id_frame_proxy_map.Get().insert(
      std::make_pair(
          RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_),
          this)).second);

  if (!frame_tree_node_->IsMainFrame() &&
      frame_tree_node_->parent()
              ->render_manager()
              ->current_frame_host()
              ->GetSiteInstance() == site_instance) {
    // The RenderFrameHost navigating cross-process is destroyed and a proxy for
    // it is created in the parent's process. CrossProcessFrameConnector
    // initialization only needs to happen on an initial cross-process
    // navigation, when the RenderFrameHost leaves the same process as its
    // parent. The same CrossProcessFrameConnector is used for subsequent cross-
    // process navigations, but it will be destroyed if the frame is
    // navigated back to the same SiteInstance as its parent.
    cross_process_frame_connector_.reset(new CrossProcessFrameConnector(this));
  }
}

RenderFrameProxyHost::~RenderFrameProxyHost() {
  if (GetProcess()->HasConnection()) {
    // TODO(nasko): For now, don't send this IPC for top-level frames, as
    // the top-level RenderFrame will delete the RenderFrameProxy.
    // This can be removed once we don't have a swapped out state on
    // RenderFrame. See https://crbug.com/357747
    if (!frame_tree_node_->IsMainFrame())
      Send(new FrameMsg_DeleteProxy(routing_id_));
  }

  GetProcess()->RemoveRoute(routing_id_);
  g_routing_id_frame_proxy_map.Get().erase(
      RenderFrameProxyHostID(GetProcess()->GetID(), routing_id_));
}

void RenderFrameProxyHost::SetChildRWHView(RenderWidgetHostView* view) {
  cross_process_frame_connector_->set_view(
      static_cast<RenderWidgetHostViewChildFrame*>(view));
}

RenderViewHostImpl* RenderFrameProxyHost::GetRenderViewHost() {
  return frame_tree_node_->frame_tree()->GetRenderViewHost(
      site_instance_.get());
}

void RenderFrameProxyHost::TakeFrameHostOwnership(
    scoped_ptr<RenderFrameHostImpl> render_frame_host) {
  render_frame_host_ = render_frame_host.Pass();
  render_frame_host_->set_render_frame_proxy_host(this);
}

scoped_ptr<RenderFrameHostImpl> RenderFrameProxyHost::PassFrameHostOwnership() {
  render_frame_host_->set_render_frame_proxy_host(NULL);
  return render_frame_host_.Pass();
}

bool RenderFrameProxyHost::Send(IPC::Message *msg) {
  return GetProcess()->Send(msg);
}

bool RenderFrameProxyHost::OnMessageReceived(const IPC::Message& msg) {
  if (cross_process_frame_connector_.get() &&
      cross_process_frame_connector_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameProxyHost, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool RenderFrameProxyHost::InitRenderFrameProxy() {
  DCHECK(!render_frame_proxy_created_);
  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;

  DCHECK(GetProcess()->HasConnection());

  int parent_routing_id = MSG_ROUTING_NONE;
  if (frame_tree_node_->parent()) {
    parent_routing_id = frame_tree_node_->parent()
                            ->render_manager()
                            ->GetRoutingIdForSiteInstance(site_instance_.get());
    CHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
  }

  Send(new FrameMsg_NewFrameProxy(routing_id_,
                                  parent_routing_id,
                                  frame_tree_node_->frame_tree()
                                      ->GetRenderViewHost(site_instance_.get())
                                      ->GetRoutingID(),
                                  frame_tree_node_
                                      ->current_replication_state()));

  render_frame_proxy_created_ = true;
  return true;
}

void RenderFrameProxyHost::DisownOpener() {
  Send(new FrameMsg_DisownOpener(GetRoutingID()));
}

void RenderFrameProxyHost::OnOpenURL(
    const FrameHostMsg_OpenURL_Params& params) {
  frame_tree_node_->current_frame_host()->OpenURL(params, site_instance_.get());
}

}  // namespace content
