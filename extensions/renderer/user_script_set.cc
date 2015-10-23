// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_set.h"

#include "base/memory/ref_counted.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/extension_injection_host.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/user_script_injector.h"
#include "extensions/renderer/web_ui_injection_host.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace extensions {

namespace {

GURL GetDocumentUrlForFrame(blink::WebLocalFrame* frame) {
  GURL data_source_url = ScriptContext::GetDataSourceURLForFrame(frame);
  if (!data_source_url.is_empty() && frame->isViewSourceModeEnabled()) {
    data_source_url = GURL(content::kViewSourceScheme + std::string(":") +
                           data_source_url.spec());
  }

  return data_source_url;
}

}  // namespace

UserScriptSet::UserScriptSet() {}

UserScriptSet::~UserScriptSet() {
}

void UserScriptSet::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserScriptSet::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserScriptSet::GetActiveExtensionIds(
    std::set<std::string>* ids) const {
  for (const UserScript* script : scripts_) {
    if (script->host_id().type() != HostID::EXTENSIONS)
      continue;
    DCHECK(!script->extension_id().empty());
    ids->insert(script->extension_id());
  }
}

void UserScriptSet::GetInjections(
    ScopedVector<ScriptInjection>* injections,
    content::RenderFrame* render_frame,
    int tab_id,
    UserScript::RunLocation run_location) {
  GURL document_url = GetDocumentUrlForFrame(render_frame->GetWebFrame());
  for (const UserScript* script : scripts_) {
    scoped_ptr<ScriptInjection> injection = GetInjectionForScript(
        script,
        render_frame,
        tab_id,
        run_location,
        document_url,
        false /* is_declarative */);
    if (injection.get())
      injections->push_back(injection.Pass());
  }
}

bool UserScriptSet::UpdateUserScripts(base::SharedMemoryHandle shared_memory,
                                      const std::set<HostID>& changed_hosts,
                                      bool whitelisted_only) {
  bool only_inject_incognito =
      ExtensionsRendererClient::Get()->IsIncognitoProcess();

  // Create the shared memory object (read only).
  shared_memory_.reset(new base::SharedMemory(shared_memory, true));
  if (!shared_memory_.get())
    return false;

  // First get the size of the memory block.
  if (!shared_memory_->Map(sizeof(base::Pickle::Header)))
    return false;
  base::Pickle::Header* pickle_header =
      reinterpret_cast<base::Pickle::Header*>(shared_memory_->memory());

  // Now map in the rest of the block.
  int pickle_size = sizeof(base::Pickle::Header) + pickle_header->payload_size;
  shared_memory_->Unmap();
  if (!shared_memory_->Map(pickle_size))
    return false;

  // Unpickle scripts.
  size_t num_scripts = 0;
  base::Pickle pickle(reinterpret_cast<char*>(shared_memory_->memory()),
                      pickle_size);
  base::PickleIterator iter(pickle);
  CHECK(iter.ReadSizeT(&num_scripts));

  scripts_.clear();
  scripts_.reserve(num_scripts);
  for (size_t i = 0; i < num_scripts; ++i) {
    scoped_ptr<UserScript> script(new UserScript());
    script->Unpickle(pickle, &iter);

    // Note that this is a pointer into shared memory. We don't own it. It gets
    // cleared up when the last renderer or browser process drops their
    // reference to the shared memory.
    for (size_t j = 0; j < script->js_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(iter.ReadData(&body, &body_length));
      script->js_scripts()[j].set_external_content(
          base::StringPiece(body, body_length));
    }
    for (size_t j = 0; j < script->css_scripts().size(); ++j) {
      const char* body = NULL;
      int body_length = 0;
      CHECK(iter.ReadData(&body, &body_length));
      script->css_scripts()[j].set_external_content(
          base::StringPiece(body, body_length));
    }

    if (only_inject_incognito && !script->is_incognito_enabled())
      continue;  // This script shouldn't run in an incognito tab.

    const Extension* extension =
        RendererExtensionRegistry::Get()->GetByID(script->extension_id());
    if (whitelisted_only &&
        (!extension ||
         !PermissionsData::CanExecuteScriptEverywhere(extension))) {
      continue;
    }

    scripts_.push_back(script.Pass());
  }

  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OnUserScriptsUpdated(changed_hosts, scripts_.get()));
  return true;
}

scoped_ptr<ScriptInjection> UserScriptSet::GetDeclarativeScriptInjection(
    int script_id,
    content::RenderFrame* render_frame,
    int tab_id,
    UserScript::RunLocation run_location,
    const GURL& document_url) {
  for (const UserScript* script : scripts_) {
    if (script->id() == script_id) {
      return GetInjectionForScript(script,
                                   render_frame,
                                   tab_id,
                                   run_location,
                                   document_url,
                                   true /* is_declarative */);
    }
  }
  return scoped_ptr<ScriptInjection>();
}

scoped_ptr<ScriptInjection> UserScriptSet::GetInjectionForScript(
    const UserScript* script,
    content::RenderFrame* render_frame,
    int tab_id,
    UserScript::RunLocation run_location,
    const GURL& document_url,
    bool is_declarative) {
  scoped_ptr<ScriptInjection> injection;
  scoped_ptr<const InjectionHost> injection_host;
  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();

  const HostID& host_id = script->host_id();
  if (host_id.type() == HostID::EXTENSIONS) {
    injection_host = ExtensionInjectionHost::Create(host_id.id());
    if (!injection_host)
      return injection.Pass();
  } else {
    DCHECK_EQ(host_id.type(), HostID::WEBUI);
    injection_host.reset(new WebUIInjectionHost(host_id));
  }

  if (web_frame->parent() && !script->match_all_frames())
    return injection.Pass();  // Only match subframes if the script declared it.

  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      web_frame, document_url, script->match_about_blank());

  if (!script->MatchesURL(effective_document_url))
    return injection.Pass();

  scoped_ptr<ScriptInjector> injector(new UserScriptInjector(script,
                                                             this,
                                                             is_declarative));

  if (injector->CanExecuteOnFrame(
          injection_host.get(),
          web_frame,
          tab_id) ==
      PermissionsData::ACCESS_DENIED) {
    return injection.Pass();
  }

  bool inject_css = !script->css_scripts().empty() &&
                    run_location == UserScript::DOCUMENT_START;
  bool inject_js =
      !script->js_scripts().empty() && script->run_location() == run_location;
  if (inject_css || inject_js) {
    injection.reset(new ScriptInjection(
        injector.Pass(),
        render_frame,
        injection_host.Pass(),
        run_location));
  }
  return injection.Pass();
}

}  // namespace extensions
