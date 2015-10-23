// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTOR_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTOR_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

class GURL;
class InjectionHost;

namespace blink {
class WebLocalFrame;
}

namespace extensions {
struct ScriptsRunInfo;

// The pseudo-delegate class for a ScriptInjection that provides all necessary
// information about how to inject the script, including what code to inject,
// when (run location), and where (world), but without any injection logic.
class ScriptInjector {
 public:
  // The possible reasons for not injecting the script.
  enum InjectFailureReason {
    EXTENSION_REMOVED,  // The extension was removed before injection.
    NOT_ALLOWED,        // The script is not allowed to inject.
    WONT_INJECT         // The injection won't inject because the user rejected
                        // (or just did not accept) the injection.
  };

  virtual ~ScriptInjector() {}

  // Returns the script type of this particular injection.
  virtual UserScript::InjectionType script_type() const = 0;

  // Returns true if the script should execute in the main world.
  virtual bool ShouldExecuteInMainWorld() const = 0;

  // Returns true if the script is running inside a user gesture.
  virtual bool IsUserGesture() const = 0;

  // Returns true if the script expects results.
  virtual bool ExpectsResults() const = 0;

  // Returns true if the script should inject JS source at the given
  // |run_location|.
  virtual bool ShouldInjectJs(UserScript::RunLocation run_location) const = 0;

  // Returns true if the script should inject CSS at the given |run_location|.
  virtual bool ShouldInjectCss(UserScript::RunLocation run_location) const = 0;

  // Returns true if the script should execute on the given |frame|.
  virtual PermissionsData::AccessType CanExecuteOnFrame(
      const InjectionHost* injection_host,
      blink::WebLocalFrame* web_frame,
      int tab_id) const = 0;

  // Returns the javascript sources to inject at the given |run_location|.
  // Only called if ShouldInjectJs() is true.
  virtual std::vector<blink::WebScriptSource> GetJsSources(
      UserScript::RunLocation run_location) const = 0;

  // Returns the css to inject at the given |run_location|.
  // Only called if ShouldInjectCss() is true.
  virtual std::vector<std::string> GetCssSources(
      UserScript::RunLocation run_location) const = 0;

  // Fill scriptrs run info based on information about injection.
  virtual void GetRunInfo(
      ScriptsRunInfo* scripts_run_info,
      UserScript::RunLocation run_location) const = 0;

  // Notifies the script that injection has completed, with a possibly-populated
  // list of results (depending on whether or not ExpectsResults() was true).
  virtual void OnInjectionComplete(
      scoped_ptr<base::Value> execution_result,
      UserScript::RunLocation run_location) = 0;

  // Notifies the script that injection will never occur.
  virtual void OnWillNotInject(InjectFailureReason reason) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTOR_H_
