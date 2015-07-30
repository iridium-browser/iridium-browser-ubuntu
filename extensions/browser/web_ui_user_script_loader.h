// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_WEB_UI_USER_SCRIPT_LOADER_H_
#define EXTENSIONS_BROWSER_WEB_UI_USER_SCRIPT_LOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "extensions/browser/user_script_loader.h"

class WebUIURLFetcher;

namespace content {
class BrowserContext;
}

// UserScriptLoader for WebUI.
class WebUIUserScriptLoader : public extensions::UserScriptLoader {
 public:
  WebUIUserScriptLoader(content::BrowserContext* browser_context,
                        const HostID& host_id);
  ~WebUIUserScriptLoader() override;

 private:
  struct UserScriptRenderInfo;
  using UserScriptRenderInfoMap = std::map<int, UserScriptRenderInfo>;

  // UserScriptLoader:
  void AddScripts(const std::set<extensions::UserScript>& scripts,
                  int render_process_id,
                  int render_view_id) override;
  void LoadScripts(scoped_ptr<extensions::UserScriptList> user_scripts,
                   const std::set<HostID>& changed_hosts,
                   const std::set<int>& added_script_ids,
                   LoadScriptsCallback callback) override;

  // Called at the end of each fetch, tracking whether all fetches are done.
  void OnSingleWebUIURLFetchComplete(extensions::UserScript::File* script_file,
                                     bool success,
                                     const std::string& data);

  // Called when the loads of the user scripts are done.
  void OnWebUIURLFetchComplete();

  // Creates WebUiURLFetchers for the given |script_files|.
  void CreateWebUIURLFetchers(extensions::UserScript::FileList* script_files,
                              content::BrowserContext* browser_context,
                              int render_process_id,
                              int render_view_id);

  // Caches the render info of script from WebUI when AddScripts is called.
  // When starting to load the script, we look up this map to retrieve the
  // render info. It is used for the script from WebUI only, since the fetch
  // of script content requires the info of associated render.
  UserScriptRenderInfoMap script_render_info_map_;

  // The number of complete fetchs.
  size_t complete_fetchers_;

  // Caches |user_scripts_| from UserScriptLoader when loading.
  scoped_ptr<extensions::UserScriptList> user_scripts_cache_;

  LoadScriptsCallback scripts_loaded_callback_;

  ScopedVector<WebUIURLFetcher> fetchers_;

  DISALLOW_COPY_AND_ASSIGN(WebUIUserScriptLoader);
};

#endif  // EXTENSIONS_BROWSER_WEB_UI_USER_SCRIPT_LOADER_H_
