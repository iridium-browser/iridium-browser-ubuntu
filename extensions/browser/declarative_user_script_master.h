// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DECLARATIVE_USER_SCRIPT_MASTER_H_
#define EXTENSIONS_BROWSER_DECLARATIVE_USER_SCRIPT_MASTER_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "extensions/common/host_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class UserScript;
class UserScriptLoader;

// Manages declarative user scripts for a single extension. Owns a
// UserScriptLoader to which file loading and shared memory management
// operations are delegated, and provides an interface for adding, removing,
// and clearing scripts.
class DeclarativeUserScriptMaster {
 public:
  DeclarativeUserScriptMaster(content::BrowserContext* browser_context,
                              const HostID& host_id);
  ~DeclarativeUserScriptMaster();

  // Adds script to shared memory region. This may not happen right away if a
  // script load is in progress.
  void AddScript(const UserScript& script);

  // Adds a set of scripts to shared meomory region. The fetch of the content
  // of the script on WebUI requires to start URL request to the associated
  // render specified by |render_process_id, render_view_id|.
  // This may not happen right away if a script load is in progress.
  void AddScripts(const std::set<UserScript>& scripts,
                  int render_process_id,
                  int render_view_id);

  // Removes script from shared memory region. This may not happen right away if
  // a script load is in progress.
  void RemoveScript(const UserScript& script);

  // Removes a set of scripts from shared memory region. This may not happen
  // right away if a script load is in progress.
  void RemoveScripts(const std::set<UserScript>& scripts);

  // Removes all scripts from shared memory region. This may not happen right
  // away if a script load is in progress.
  void ClearScripts();

  const HostID& host_id() const { return host_id_; }

  UserScriptLoader* loader() { return loader_.get(); }

 private:
  // ID of host that owns scripts that this component manages.
  HostID host_id_;

  // Script loader that handles loading contents of scripts into shared memory
  // and notifying renderers of script updates.
  scoped_ptr<UserScriptLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeUserScriptMaster);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_DECLARATIVE_USER_SCRIPT_MASTER_H_
