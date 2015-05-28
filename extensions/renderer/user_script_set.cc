// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_set.h"

#include "base/memory/ref_counted.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/extension_injection_host.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/user_script_injector.h"
#include "extensions/renderer/web_ui_injection_host.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "url/gurl.h"

namespace extensions {

namespace {

GURL GetDocumentUrlForFrame(blink::WebFrame* frame) {
  GURL data_source_url = ScriptContext::GetDataSourceURLForFrame(frame);
  if (!data_source_url.is_empty() && frame->isViewSourceModeEnabled()) {
    data_source_url = GURL(content::kViewSourceScheme + std::string(":") +
                           data_source_url.spec());
  }

  return data_source_url;
}

}  // namespace

UserScriptSet::UserScriptSet(const ExtensionSet* extensions)
    : extensions_(extensions) {
}

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
  for (ScopedVector<UserScript>::const_iterator iter = scripts_.begin();
       iter != scripts_.end();
       ++iter) {
    if ((*iter)->host_id().type() != HostID::EXTENSIONS)
      continue;
    DCHECK(!(*iter)->extension_id().empty());
    ids->insert((*iter)->extension_id());
  }
}

void UserScriptSet::GetInjections(
    ScopedVector<ScriptInjection>* injections,
    blink::WebFrame* web_frame,
    int tab_id,
    UserScript::RunLocation run_location) {
  GURL document_url = GetDocumentUrlForFrame(web_frame);
  for (ScopedVector<UserScript>::const_iterator iter = scripts_.begin();
       iter != scripts_.end();
       ++iter) {
    scoped_ptr<ScriptInjection> injection = GetInjectionForScript(
        *iter,
        web_frame,
        tab_id,
        run_location,
        document_url,
        false /* is_declarative */);
    if (injection.get())
      injections->push_back(injection.release());
  }
}

bool UserScriptSet::UpdateUserScripts(base::SharedMemoryHandle shared_memory,
                                      const std::set<HostID>& changed_hosts) {
  bool only_inject_incognito =
      ExtensionsRendererClient::Get()->IsIncognitoProcess();

  // Create the shared memory object (read only).
  shared_memory_.reset(new base::SharedMemory(shared_memory, true));
  if (!shared_memory_.get())
    return false;

  // First get the size of the memory block.
  if (!shared_memory_->Map(sizeof(Pickle::Header)))
    return false;
  Pickle::Header* pickle_header =
      reinterpret_cast<Pickle::Header*>(shared_memory_->memory());

  // Now map in the rest of the block.
  int pickle_size = sizeof(Pickle::Header) + pickle_header->payload_size;
  shared_memory_->Unmap();
  if (!shared_memory_->Map(pickle_size))
    return false;

  // Unpickle scripts.
  size_t num_scripts = 0;
  Pickle pickle(reinterpret_cast<char*>(shared_memory_->memory()), pickle_size);
  PickleIterator iter(pickle);
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

    scripts_.push_back(script.release());
  }

  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OnUserScriptsUpdated(changed_hosts, scripts_.get()));
  return true;
}

scoped_ptr<ScriptInjection> UserScriptSet::GetDeclarativeScriptInjection(
    int script_id,
    blink::WebFrame* web_frame,
    int tab_id,
    UserScript::RunLocation run_location,
    const GURL& document_url) {
  for (ScopedVector<UserScript>::const_iterator it = scripts_.begin();
       it != scripts_.end();
       ++it) {
    if ((*it)->id() == script_id) {
      return GetInjectionForScript(*it,
                                   web_frame,
                                   tab_id,
                                   run_location,
                                   document_url,
                                   true /* is_declarative */);
    }
  }
  return scoped_ptr<ScriptInjection>();
}

// TODO(dcheng): Scripts can't be injected on a remote frame, so this function
// signature needs to be updated.
scoped_ptr<ScriptInjection> UserScriptSet::GetInjectionForScript(
    UserScript* script,
    blink::WebFrame* web_frame,
    int tab_id,
    UserScript::RunLocation run_location,
    const GURL& document_url,
    bool is_declarative) {
  scoped_ptr<ScriptInjection> injection;
  scoped_ptr<const InjectionHost> injection_host;

  const HostID& host_id = script->host_id();
  if (host_id.type() == HostID::EXTENSIONS) {
    injection_host = ExtensionInjectionHost::Create(host_id.id(), extensions_);
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

  blink::WebFrame* top_frame = web_frame->top();
  // It doesn't make sense to do script injection for remote frames, since they
  // cannot host any documents or content.
  // TODO(kalman): Fix this properly by moving all security checks into the
  // browser. See http://crbug.com/466373 for ongoing work here.
  if (top_frame->isWebRemoteFrame())
    return injection.Pass();

  if (injector->CanExecuteOnFrame(injection_host.get(), web_frame,
                                  -1,  // Content scripts are not tab-specific.
                                  top_frame->document().url()) ==
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
        web_frame->toWebLocalFrame(),
        injection_host.Pass(),
        run_location,
        tab_id));
  }
  return injection.Pass();
}

}  // namespace extensions
