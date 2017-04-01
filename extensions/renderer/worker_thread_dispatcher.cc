// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_dispatcher.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "base/values.h"
#include "content/public/child/worker_thread.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/feature_switch.h"
#include "extensions/renderer/extension_bindings_system.h"
#include "extensions/renderer/js_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/service_worker_data.h"

namespace extensions {

namespace {

base::LazyInstance<WorkerThreadDispatcher> g_instance =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::ThreadLocalPointer<extensions::ServiceWorkerData>>
    g_data_tls = LAZY_INSTANCE_INITIALIZER;

void OnResponseOnWorkerThread(int request_id,
                              bool succeeded,
                              const std::unique_ptr<base::ListValue>& response,
                              const std::string& error) {
  // TODO(devlin): Using the RequestSender directly here won't work with
  // NativeExtensionBindingsSystem (since there is no associated RequestSender
  // in that case). We should instead be going
  // ExtensionBindingsSystem::HandleResponse().
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  WorkerThreadDispatcher::GetRequestSender()->HandleWorkerResponse(
      request_id, data->service_worker_version_id(), succeeded, *response,
      error);
}

ServiceWorkerData* GetServiceWorkerData() {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  DCHECK(data);
  return data;
}

// Handler for sending IPCs with native extension bindings.
void SendRequestIPC(ScriptContext* context,
                    const ExtensionHostMsg_Request_Params& params) {
  // TODO(devlin): This won't handle incrementing/decrementing service worker
  // lifetime.
  WorkerThreadDispatcher::Get()->Send(
      new ExtensionHostMsg_RequestWorker(params));
}

void SendEventListenersIPC(binding::EventListenersChanged changed,
                           ScriptContext* context,
                           const std::string& event_name) {
  // TODO(devlin/lazyboy): Wire this up once extension workers support events.
}

}  // namespace

WorkerThreadDispatcher::WorkerThreadDispatcher() {}
WorkerThreadDispatcher::~WorkerThreadDispatcher() {}

WorkerThreadDispatcher* WorkerThreadDispatcher::Get() {
  return g_instance.Pointer();
}

void WorkerThreadDispatcher::Init(content::RenderThread* render_thread) {
  DCHECK(render_thread);
  DCHECK_EQ(content::RenderThread::Get(), render_thread);
  DCHECK(!message_filter_);
  message_filter_ = render_thread->GetSyncMessageFilter();
  render_thread->AddObserver(this);
}

// static
ExtensionBindingsSystem* WorkerThreadDispatcher::GetBindingsSystem() {
  return GetServiceWorkerData()->bindings_system();
}

// static
ServiceWorkerRequestSender* WorkerThreadDispatcher::GetRequestSender() {
  return static_cast<ServiceWorkerRequestSender*>(
      GetBindingsSystem()->GetRequestSender());
}

// static
V8SchemaRegistry* WorkerThreadDispatcher::GetV8SchemaRegistry() {
  return GetServiceWorkerData()->v8_schema_registry();
}

bool WorkerThreadDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerThreadDispatcher, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ResponseWorker, OnResponseWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool WorkerThreadDispatcher::Send(IPC::Message* message) {
  return message_filter_->Send(message);
}

void WorkerThreadDispatcher::OnResponseWorker(int worker_thread_id,
                                              int request_id,
                                              bool succeeded,
                                              const base::ListValue& response,
                                              const std::string& error) {
  content::WorkerThread::PostTask(
      worker_thread_id,
      base::Bind(&OnResponseOnWorkerThread, request_id, succeeded,
                 // TODO(lazyboy): Can we avoid CreateDeepCopy()?
                 base::Passed(response.CreateDeepCopy()), error));
}

void WorkerThreadDispatcher::AddWorkerData(
    int64_t service_worker_version_id,
    ResourceBundleSourceMap* source_map) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  if (!data) {
    std::unique_ptr<ExtensionBindingsSystem> bindings_system;
    if (FeatureSwitch::native_crx_bindings()->IsEnabled()) {
      bindings_system = base::MakeUnique<NativeExtensionBindingsSystem>(
          base::Bind(&SendRequestIPC), base::Bind(&SendEventListenersIPC));
    } else {
      bindings_system = base::MakeUnique<JsExtensionBindingsSystem>(
          source_map, base::MakeUnique<ServiceWorkerRequestSender>(
                          this, service_worker_version_id));
    }
    ServiceWorkerData* new_data = new ServiceWorkerData(
        service_worker_version_id, std::move(bindings_system));
    g_data_tls.Pointer()->Set(new_data);
  }
}

void WorkerThreadDispatcher::RemoveWorkerData(
    int64_t service_worker_version_id) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  if (data) {
    DCHECK_EQ(service_worker_version_id, data->service_worker_version_id());
    delete data;
    g_data_tls.Pointer()->Set(nullptr);
  }
}

}  // namespace extensions
