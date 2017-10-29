// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker/websharedworker_proxy.h"

#include <stddef.h>

#include "content/child/child_thread_impl.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/view_messages.h"
#include "ipc/message_router.h"

namespace content {

WebSharedWorkerProxy::WebSharedWorkerProxy(
    std::unique_ptr<blink::WebSharedWorkerConnectListener> listener,
    ViewHostMsg_CreateWorker_Params params,
    std::unique_ptr<blink::WebMessagePortChannel> channel)
    : route_id_(MSG_ROUTING_NONE),
      router_(ChildThreadImpl::current()->GetRouter()),
      listener_(std::move(listener)) {
  connect(params, std::move(channel));
}

WebSharedWorkerProxy::~WebSharedWorkerProxy() {
  DCHECK_NE(MSG_ROUTING_NONE, route_id_);
  router_->RemoveRoute(route_id_);
}

void WebSharedWorkerProxy::connect(
    ViewHostMsg_CreateWorker_Params params,
    std::unique_ptr<blink::WebMessagePortChannel> channel) {
  // Send synchronous IPC to get |route_id|.
  // TODO(nhiroki): Stop using synchronous IPC (https://crbug.com/679654).
  ViewHostMsg_CreateWorker_Reply reply;
  router_->Send(new ViewHostMsg_CreateWorker(params, &reply));
  route_id_ = reply.route_id;
  router_->AddRoute(route_id_, this);
  listener_->WorkerCreated(reply.error);

  message_port_ = static_cast<WebMessagePortChannelImpl*>(channel.get())
                      ->ReleaseMessagePort();

  // An actual connection request will be issued on OnWorkerCreated().
}

bool WebSharedWorkerProxy::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebSharedWorkerProxy, message)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerCreated, OnWorkerCreated)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerScriptLoadFailed,
                        OnWorkerScriptLoadFailed)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerConnected,
                        OnWorkerConnected)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerDestroyed, OnWorkerDestroyed)
    IPC_MESSAGE_HANDLER(ViewMsg_CountFeatureOnSharedWorker, OnCountFeature)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebSharedWorkerProxy::OnWorkerCreated() {
  DCHECK(message_port_.GetHandle().is_valid());

  // The worker is created - now send off the connect message.
  router_->Send(new ViewHostMsg_ConnectToWorker(route_id_, message_port_));
}

void WebSharedWorkerProxy::OnWorkerScriptLoadFailed() {
  listener_->ScriptLoadFailed();
  delete this;
}

void WebSharedWorkerProxy::OnWorkerConnected(
    const std::set<uint32_t>& used_features) {
  listener_->Connected();
  for (uint32_t feature : used_features)
    listener_->CountFeature(feature);
}

void WebSharedWorkerProxy::OnWorkerDestroyed() {
  delete this;
}

void WebSharedWorkerProxy::OnCountFeature(uint32_t feature) {
  listener_->CountFeature(feature);
}

}  // namespace content
