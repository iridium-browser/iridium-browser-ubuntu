// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/ipc_echo.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/shell/common/shell_messages.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/web/WebDOMCustomEvent.h"
#include "third_party/WebKit/public/web/WebDOMEvent.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"

namespace content {

class IPCEchoBindings : public gin::Wrappable<IPCEchoBindings> {
public:
  static gin::WrapperInfo kWrapperInfo;
  static void Install(base::WeakPtr<IPCEcho> echo, blink::WebFrame* frame);

  explicit IPCEchoBindings(base::WeakPtr<IPCEcho>);

  void RequestEcho(int id, int size);
  int GetLastEchoId() const;
  int GetLastEchoSize() const;

private:
 gin::ObjectTemplateBuilder GetObjectTemplateBuilder(v8::Isolate*) override;

  base::WeakPtr<IPCEcho> native_;
};

IPCEchoBindings::IPCEchoBindings(base::WeakPtr<IPCEcho> native)
    : native_(native) {
}

void IPCEchoBindings::RequestEcho(int id, int size) {
  if (!native_)
    return;
  native_->RequestEcho(id, size);
}

int IPCEchoBindings::GetLastEchoId() const {
  if (!native_)
    return 0;
  return native_->last_echo_id();
}

int IPCEchoBindings::GetLastEchoSize() const {
  if (!native_)
    return 0;
  return native_->last_echo_size();
}

gin::WrapperInfo IPCEchoBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin };

gin::ObjectTemplateBuilder IPCEchoBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<IPCEchoBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("requestEcho",
                 &IPCEchoBindings::RequestEcho)
      .SetProperty("lastEchoId",
                   &IPCEchoBindings::GetLastEchoId)
      .SetProperty("lastEchoSize",
                   &IPCEchoBindings::GetLastEchoSize);
}

// static
void IPCEchoBindings::Install(base::WeakPtr<IPCEcho> echo,
                              blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  IPCEchoBindings* wrapped = new IPCEchoBindings(echo);
  gin::Handle<IPCEchoBindings> bindings = gin::CreateHandle(isolate, wrapped);
  if (bindings.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Value> v8_bindings = bindings.ToV8();
  global->Set(gin::StringToV8(isolate, "ipcEcho"), v8_bindings);
}

IPCEcho::IPCEcho(blink::WebDocument document,
                 IPC::Sender* sender,
                 int routing_id)
    : document_(document), sender_(sender), routing_id_(routing_id),
      last_echo_id_(0),
      weak_factory_(this) {
}

IPCEcho::~IPCEcho() {
}

void IPCEcho::RequestEcho(int id, int size) {
  sender_->Send(new ShellViewHostMsg_EchoPing(
      routing_id_, id, std::string(size, '*')));
}

void IPCEcho::DidRespondEcho(int id, int size) {
  last_echo_id_ = id;
  last_echo_size_ = size;
  blink::WebString eventName = blink::WebString::fromUTF8("CustomEvent");
  blink::WebString eventType = blink::WebString::fromUTF8("pong");
  blink::WebDOMEvent event = document_.createEvent(eventName);
  event.to<blink::WebDOMCustomEvent>().initCustomEvent(
      eventType, false, false, blink::WebSerializedScriptValue());
  document_.dispatchEvent(event);
}

void IPCEcho::Install(blink::WebFrame* frame) {
  IPCEchoBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

}  // namespace content
