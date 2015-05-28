// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_context.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "gin/per_context_data.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace extensions {

namespace {

std::string GetContextTypeDescriptionString(Feature::Context context_type) {
  switch (context_type) {
    case Feature::UNSPECIFIED_CONTEXT:
      return "UNSPECIFIED";
    case Feature::BLESSED_EXTENSION_CONTEXT:
      return "BLESSED_EXTENSION";
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      return "UNBLESSED_EXTENSION";
    case Feature::CONTENT_SCRIPT_CONTEXT:
      return "CONTENT_SCRIPT";
    case Feature::WEB_PAGE_CONTEXT:
      return "WEB_PAGE";
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      return "BLESSED_WEB_PAGE";
    case Feature::WEBUI_CONTEXT:
      return "WEBUI";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

// A gin::Runner that delegates to its ScriptContext.
class ScriptContext::Runner : public gin::Runner {
 public:
  explicit Runner(ScriptContext* context);

  // gin::Runner overrides.
  void Run(const std::string& source,
           const std::string& resource_name) override;
  v8::Handle<v8::Value> Call(v8::Handle<v8::Function> function,
                             v8::Handle<v8::Value> receiver,
                             int argc,
                             v8::Handle<v8::Value> argv[]) override;
  gin::ContextHolder* GetContextHolder() override;

 private:
  ScriptContext* context_;
};

ScriptContext::ScriptContext(const v8::Handle<v8::Context>& v8_context,
                             blink::WebFrame* web_frame,
                             const Extension* extension,
                             Feature::Context context_type,
                             const Extension* effective_extension,
                             Feature::Context effective_context_type)
    : v8_context_(v8_context->GetIsolate(), v8_context),
      web_frame_(web_frame),
      extension_(extension),
      context_type_(context_type),
      effective_extension_(effective_extension),
      effective_context_type_(effective_context_type),
      safe_builtins_(this),
      isolate_(v8_context->GetIsolate()),
      url_(web_frame_ ? GetDataSourceURLForFrame(web_frame_) : GURL()),
      runner_(new Runner(this)) {
  VLOG(1) << "Created context:\n"
          << "  extension id: " << GetExtensionID() << "\n"
          << "  frame:        " << web_frame_ << "\n"
          << "  URL:          " << GetURL() << "\n"
          << "  context type: " << GetContextTypeDescription() << "\n"
          << "  effective extension id: "
          << (effective_extension_.get() ? effective_extension_->id() : "")
          << "  effective context type: "
          << GetEffectiveContextTypeDescription();
  gin::PerContextData::From(v8_context)->set_runner(runner_.get());
}

ScriptContext::~ScriptContext() {
  VLOG(1) << "Destroyed context for extension\n"
          << "  extension id: " << GetExtensionID() << "\n"
          << "  effective extension id: "
          << (effective_extension_.get() ? effective_extension_->id() : "");
  Invalidate();
}

void ScriptContext::Invalidate() {
  if (!is_valid())
    return;
  if (module_system_)
    module_system_->Invalidate();
  web_frame_ = NULL;
  v8_context_.Reset();
  runner_.reset();
}

const std::string& ScriptContext::GetExtensionID() const {
  return extension_.get() ? extension_->id() : base::EmptyString();
}

content::RenderView* ScriptContext::GetRenderView() const {
  if (web_frame_ && web_frame_->view())
    return content::RenderView::FromWebView(web_frame_->view());
  return NULL;
}

content::RenderFrame* ScriptContext::GetRenderFrame() const {
  if (web_frame_)
    return content::RenderFrame::FromWebFrame(web_frame_);
  return NULL;
}

v8::Local<v8::Value> ScriptContext::CallFunction(
    v8::Handle<v8::Function> function,
    int argc,
    v8::Handle<v8::Value> argv[]) const {
  v8::EscapableHandleScope handle_scope(isolate());
  v8::Context::Scope scope(v8_context());

  blink::WebScopedMicrotaskSuppression suppression;
  if (!is_valid()) {
    return handle_scope.Escape(
        v8::Local<v8::Primitive>(v8::Undefined(isolate())));
  }

  v8::Handle<v8::Object> global = v8_context()->Global();
  if (!web_frame_)
    return handle_scope.Escape(function->Call(global, argc, argv));
  return handle_scope.Escape(
      v8::Local<v8::Value>(web_frame_->callFunctionEvenIfScriptDisabled(
          function, global, argc, argv)));
}

Feature::Availability ScriptContext::GetAvailability(
    const std::string& api_name) {
  // Hack: Hosted apps should have the availability of messaging APIs based on
  // the URL of the page (which might have access depending on some extension
  // with externally_connectable), not whether the app has access to messaging
  // (which it won't).
  const Extension* extension = extension_.get();
  if (extension && extension->is_hosted_app() &&
      (api_name == "runtime.connect" || api_name == "runtime.sendMessage")) {
    extension = NULL;
  }
  return ExtensionAPI::GetSharedInstance()->IsAvailable(
      api_name, extension, context_type_, GetURL());
}

void ScriptContext::DispatchEvent(const char* event_name,
                                  v8::Handle<v8::Array> args) const {
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8_context());

  v8::Handle<v8::Value> argv[] = {
      v8::String::NewFromUtf8(isolate(), event_name), args};
  module_system_->CallModuleMethod(
      kEventBindings, "dispatchEvent", arraysize(argv), argv);
}

void ScriptContext::DispatchOnUnloadEvent() {
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8_context());
  module_system_->CallModuleMethod("unload_event", "dispatch");
}

std::string ScriptContext::GetContextTypeDescription() {
  return GetContextTypeDescriptionString(context_type_);
}

std::string ScriptContext::GetEffectiveContextTypeDescription() {
  return GetContextTypeDescriptionString(effective_context_type_);
}

GURL ScriptContext::GetURL() const {
  return url_;
}

bool ScriptContext::IsAnyFeatureAvailableToContext(const Feature& api) {
  return ExtensionAPI::GetSharedInstance()->IsAnyFeatureAvailableToContext(
      api, extension(), context_type(), GetDataSourceURLForFrame(web_frame()));
}

// static
GURL ScriptContext::GetDataSourceURLForFrame(const blink::WebFrame* frame) {
  // Normally we would use frame->document().url() to determine the document's
  // URL, but to decide whether to inject a content script, we use the URL from
  // the data source. This "quirk" helps prevents content scripts from
  // inadvertently adding DOM elements to the compose iframe in Gmail because
  // the compose iframe's dataSource URL is about:blank, but the document URL
  // changes to match the parent document after Gmail document.writes into
  // it to create the editor.
  // http://code.google.com/p/chromium/issues/detail?id=86742
  blink::WebDataSource* data_source = frame->provisionalDataSource()
                                          ? frame->provisionalDataSource()
                                          : frame->dataSource();
  return data_source ? GURL(data_source->request().url()) : GURL();
}

// static
GURL ScriptContext::GetEffectiveDocumentURL(const blink::WebFrame* frame,
                                            const GURL& document_url,
                                            bool match_about_blank) {
  // Common scenario. If |match_about_blank| is false (as is the case in most
  // extensions), or if the frame is not an about:-page, just return
  // |document_url| (supposedly the URL of the frame).
  if (!match_about_blank || !document_url.SchemeIs(url::kAboutScheme))
    return document_url;

  // Non-sandboxed about:blank and about:srcdoc pages inherit their security
  // origin from their parent frame/window. So, traverse the frame/window
  // hierarchy to find the closest non-about:-page and return its URL.
  const blink::WebFrame* parent = frame;
  do {
    parent = parent->parent() ? parent->parent() : parent->opener();
  } while (parent != NULL && !parent->document().isNull() &&
           GURL(parent->document().url()).SchemeIs(url::kAboutScheme));

  if (parent && !parent->document().isNull()) {
    // Only return the parent URL if the frame can access it.
    const blink::WebDocument& parent_document = parent->document();
    if (frame->document().securityOrigin().canAccess(
            parent_document.securityOrigin()))
      return parent_document.url();
  }
  return document_url;
}

ScriptContext* ScriptContext::GetContext() { return this; }

void ScriptContext::OnResponseReceived(const std::string& name,
                                       int request_id,
                                       bool success,
                                       const base::ListValue& response,
                                       const std::string& error) {
  v8::HandleScope handle_scope(isolate());

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  v8::Handle<v8::Value> argv[] = {
      v8::Integer::New(isolate(), request_id),
      v8::String::NewFromUtf8(isolate(), name.c_str()),
      v8::Boolean::New(isolate(), success),
      converter->ToV8Value(&response,
                           v8::Local<v8::Context>::New(isolate(), v8_context_)),
      v8::String::NewFromUtf8(isolate(), error.c_str())};

  v8::Handle<v8::Value> retval = module_system()->CallModuleMethod(
      "sendRequest", "handleResponse", arraysize(argv), argv);

  // In debug, the js will validate the callback parameters and return a
  // string if a validation error has occured.
  DCHECK(retval.IsEmpty() || retval->IsUndefined())
      << *v8::String::Utf8Value(retval);
}

void ScriptContext::SetContentCapabilities(
    const APIPermissionSet& permissions) {
  content_capabilities_ = permissions;
}

bool ScriptContext::HasAPIPermission(APIPermission::ID permission) const {
  if (effective_extension_.get()) {
    return effective_extension_->permissions_data()->HasAPIPermission(
        permission);
  } else if (context_type() == Feature::WEB_PAGE_CONTEXT) {
    // Only web page contexts may be granted content capabilities. Other
    // contexts are either privileged WebUI or extensions with their own set of
    // permissions.
    if (content_capabilities_.find(permission) != content_capabilities_.end())
      return true;
  }
  return false;
}

ScriptContext::Runner::Runner(ScriptContext* context) : context_(context) {
}
void ScriptContext::Runner::Run(const std::string& source,
                                const std::string& resource_name) {
  context_->module_system()->RunString(source, resource_name);
}

v8::Handle<v8::Value> ScriptContext::Runner::Call(
    v8::Handle<v8::Function> function,
    v8::Handle<v8::Value> receiver,
    int argc,
    v8::Handle<v8::Value> argv[]) {
  return context_->CallFunction(function, argc, argv);
}

gin::ContextHolder* ScriptContext::Runner::GetContextHolder() {
  v8::HandleScope handle_scope(context_->isolate());
  return gin::PerContextData::From(context_->v8_context())->context_holder();
}

}  // namespace extensions
