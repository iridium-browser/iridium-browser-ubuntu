// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_INJECTOR_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_INJECTOR_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/user_script_set.h"

class InjectionHost;

namespace blink {
class WebLocalFrame;
}

namespace extensions {

// A ScriptInjector for UserScripts.
class UserScriptInjector : public ScriptInjector,
                           public UserScriptSet::Observer {
 public:
  UserScriptInjector(const UserScript* user_script,
                     UserScriptSet* user_script_set,
                     bool is_declarative);
  ~UserScriptInjector() override;

 private:
  // UserScriptSet::Observer implementation.
  void OnUserScriptsUpdated(const std::set<HostID>& changed_hosts,
                            const std::vector<UserScript*>& scripts) override;

  // ScriptInjector implementation.
  UserScript::InjectionType script_type() const override;
  bool ShouldExecuteInMainWorld() const override;
  bool IsUserGesture() const override;
  bool ExpectsResults() const override;
  bool ShouldInjectJs(UserScript::RunLocation run_location) const override;
  bool ShouldInjectCss(UserScript::RunLocation run_location) const override;
  PermissionsData::AccessType CanExecuteOnFrame(
      const InjectionHost* injection_host,
      blink::WebLocalFrame* web_frame,
      int tab_id) const override;
  std::vector<blink::WebScriptSource> GetJsSources(
      UserScript::RunLocation run_location) const override;
  std::vector<std::string> GetCssSources(
      UserScript::RunLocation run_location) const override;
  void GetRunInfo(ScriptsRunInfo* scripts_run_info,
                  UserScript::RunLocation run_location) const override;
  void OnInjectionComplete(scoped_ptr<base::Value> execution_result,
                           UserScript::RunLocation run_location) override;
  void OnWillNotInject(InjectFailureReason reason) override;

  // The associated user script. Owned by the UserScriptInjector that created
  // this object.
  const UserScript* script_;

  // The id of the associated user script. We cache this because when we update
  // the |script_| associated with this injection, the old referance may be
  // deleted.
  int script_id_;

  // The associated host id, preserved for the same reason as |script_id|.
  HostID host_id_;

  // Indicates whether or not this script is declarative. This influences which
  // script permissions are checked before injection.
  bool is_declarative_;

  ScopedObserver<UserScriptSet, UserScriptSet::Observer>
      user_script_set_observer_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptInjector);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_INJECTOR_H_
