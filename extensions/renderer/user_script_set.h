// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/observer_list.h"
#include "content/public/renderer/render_process_observer.h"
#include "extensions/common/user_script.h"

class GURL;

namespace content {
class RenderFrame;
}

namespace extensions {
class ScriptInjection;

// The UserScriptSet is a collection of UserScripts which knows how to update
// itself from SharedMemory and create ScriptInjections for UserScripts to
// inject on a page.
class UserScriptSet {
 public:
  class Observer {
   public:
    virtual void OnUserScriptsUpdated(
        const std::set<HostID>& changed_hosts,
        const std::vector<UserScript*>& scripts) = 0;
  };

  UserScriptSet();
  ~UserScriptSet();

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Appends the ids of the extensions that have user scripts to |ids|.
  void GetActiveExtensionIds(std::set<std::string>* ids) const;

  // Append any ScriptInjections that should run on the given |render_frame| and
  // |tab_id|, at the given |run_location|, to |injections|.
  // |extensions| is passed in to verify the corresponding extension is still
  // valid.
  void GetInjections(ScopedVector<ScriptInjection>* injections,
                     content::RenderFrame* render_frame,
                     int tab_id,
                     UserScript::RunLocation run_location);

  scoped_ptr<ScriptInjection> GetDeclarativeScriptInjection(
      int script_id,
      content::RenderFrame* render_frame,
      int tab_id,
      UserScript::RunLocation run_location,
      const GURL& document_url);

  // Updates scripts given the shared memory region containing user scripts.
  // Returns true if the scripts were successfully updated.
  bool UpdateUserScripts(base::SharedMemoryHandle shared_memory,
                         const std::set<HostID>& changed_hosts,
                         bool whitelisted_only);

  const std::vector<UserScript*>& scripts() const { return scripts_.get(); }

 private:
  // Returns a new ScriptInjection for the given |script| to execute in the
  // |render_frame|, or NULL if the script should not execute.
  scoped_ptr<ScriptInjection> GetInjectionForScript(
      const UserScript* script,
      content::RenderFrame* render_frame,
      int tab_id,
      UserScript::RunLocation run_location,
      const GURL& document_url,
      bool is_declarative);

  // Shared memory containing raw script data.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // The UserScripts this injector manages.
  ScopedVector<UserScript> scripts_;

  // The associated observers.
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptSet);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_
