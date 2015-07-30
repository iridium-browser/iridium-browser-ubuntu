// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/console.h"

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_helper.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace extensions {
namespace console {

namespace {

// Finds the RenderView associated with a context. Note: there will be multiple
// contexts in each RenderView.
class ByContextFinder : public content::RenderViewVisitor {
 public:
  static content::RenderView* Find(v8::Local<v8::Context> context) {
    ByContextFinder finder(context);
    content::RenderView::ForEach(&finder);
    return finder.found_;
  }

 private:
  explicit ByContextFinder(v8::Local<v8::Context> context)
      : context_(context), found_(NULL) {}

  bool Visit(content::RenderView* render_view) override {
    ExtensionHelper* helper = ExtensionHelper::Get(render_view);
    if (helper) {
      ScriptContext* script_context =
          helper->dispatcher()->script_context_set().GetByV8Context(context_);
      if (script_context && script_context->GetRenderView() == render_view)
        found_ = render_view;
    }
    return !found_;
  }

  v8::Local<v8::Context> context_;
  content::RenderView* found_;

  DISALLOW_COPY_AND_ASSIGN(ByContextFinder);
};

// Writes |message| to stack to show up in minidump, then crashes.
void CheckWithMinidump(const std::string& message) {
  char minidump[1024];
  base::debug::Alias(&minidump);
  base::snprintf(
      minidump, arraysize(minidump), "e::console: %s", message.c_str());
  CHECK(false) << message;
}

typedef void (*LogMethod)(v8::Local<v8::Context> context,
                          const std::string& message);

void BoundLogMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  LogMethod log_method =
      reinterpret_cast<LogMethod>(info.Data().As<v8::External>()->Value());
  std::string message;
  for (int i = 0; i < info.Length(); ++i) {
    if (i > 0)
      message += " ";
    message += *v8::String::Utf8Value(info[i]);
  }
  (*log_method)(info.GetIsolate()->GetCallingContext(), message);
}

void BindLogMethod(v8::Isolate* isolate,
                   v8::Local<v8::Object> target,
                   const std::string& name,
                   LogMethod log_method) {
  v8::Local<v8::FunctionTemplate> tmpl = v8::FunctionTemplate::New(
      isolate,
      &BoundLogMethodCallback,
      v8::External::New(isolate, reinterpret_cast<void*>(log_method)));
  target->Set(v8::String::NewFromUtf8(isolate, name.c_str()),
              tmpl->GetFunction());
}

}  // namespace

void Debug(content::RenderView* render_view, const std::string& message) {
  AddMessage(render_view, content::CONSOLE_MESSAGE_LEVEL_DEBUG, message);
}

void Log(content::RenderView* render_view, const std::string& message) {
  AddMessage(render_view, content::CONSOLE_MESSAGE_LEVEL_LOG, message);
}

void Warn(content::RenderView* render_view, const std::string& message) {
  AddMessage(render_view, content::CONSOLE_MESSAGE_LEVEL_WARNING, message);
}

void Error(content::RenderView* render_view, const std::string& message) {
  AddMessage(render_view, content::CONSOLE_MESSAGE_LEVEL_ERROR, message);
}

void Fatal(content::RenderView* render_view, const std::string& message) {
  Error(render_view, message);
  CheckWithMinidump(message);
}

void AddMessage(content::RenderView* render_view,
                content::ConsoleMessageLevel level,
                const std::string& message) {
  blink::WebView* web_view = render_view->GetWebView();
  if (!web_view || !web_view->mainFrame())
    return;
  blink::WebConsoleMessage::Level target_level =
      blink::WebConsoleMessage::LevelLog;
  switch (level) {
    case content::CONSOLE_MESSAGE_LEVEL_DEBUG:
      target_level = blink::WebConsoleMessage::LevelDebug;
      break;
    case content::CONSOLE_MESSAGE_LEVEL_LOG:
      target_level = blink::WebConsoleMessage::LevelLog;
      break;
    case content::CONSOLE_MESSAGE_LEVEL_WARNING:
      target_level = blink::WebConsoleMessage::LevelWarning;
      break;
    case content::CONSOLE_MESSAGE_LEVEL_ERROR:
      target_level = blink::WebConsoleMessage::LevelError;
      break;
  }
  web_view->mainFrame()->addMessageToConsole(
      blink::WebConsoleMessage(target_level, base::UTF8ToUTF16(message)));
}

void Debug(v8::Local<v8::Context> context, const std::string& message) {
  AddMessage(context, content::CONSOLE_MESSAGE_LEVEL_DEBUG, message);
}

void Log(v8::Local<v8::Context> context, const std::string& message) {
  AddMessage(context, content::CONSOLE_MESSAGE_LEVEL_LOG, message);
}

void Warn(v8::Local<v8::Context> context, const std::string& message) {
  AddMessage(context, content::CONSOLE_MESSAGE_LEVEL_WARNING, message);
}

void Error(v8::Local<v8::Context> context, const std::string& message) {
  AddMessage(context, content::CONSOLE_MESSAGE_LEVEL_ERROR, message);
}

void Fatal(v8::Local<v8::Context> context, const std::string& message) {
  Error(context, message);
  CheckWithMinidump(message);
}

void AddMessage(v8::Local<v8::Context> context,
                content::ConsoleMessageLevel level,
                const std::string& message) {
  if (context.IsEmpty()) {
    LOG(WARNING) << "Could not log \"" << message << "\": no context given";
    return;
  }
  content::RenderView* render_view = ByContextFinder::Find(context);
  if (!render_view) {
    LOG(WARNING) << "Could not log \"" << message << "\": no render view found";
    return;
  }
  AddMessage(render_view, level, message);
}

v8::Local<v8::Object> AsV8Object(v8::Isolate* isolate) {
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Object> console_object = v8::Object::New(isolate);
  BindLogMethod(isolate, console_object, "debug", &Debug);
  BindLogMethod(isolate, console_object, "log", &Log);
  BindLogMethod(isolate, console_object, "warn", &Warn);
  BindLogMethod(isolate, console_object, "error", &Error);
  return handle_scope.Escape(console_object);
}

}  // namespace console
}  // namespace extensions
