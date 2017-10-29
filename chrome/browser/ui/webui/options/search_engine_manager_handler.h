// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_SEARCH_ENGINE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_SEARCH_ENGINE_MANAGER_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "ui/base/models/table_model_observer.h"

class KeywordEditorController;

namespace extensions {
class Extension;
}

namespace options {

class SearchEngineManagerHandler : public OptionsPageUIHandler,
                                   public ui::TableModelObserver,
                                   public EditSearchEngineControllerDelegate {
 public:
  SearchEngineManagerHandler();
  ~SearchEngineManagerHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;
  void InitializePage() override;

  // ui::TableModelObserver implementation.
  void OnModelChanged() override;
  void OnItemsChanged(int start, int length) override;
  void OnItemsAdded(int start, int length) override;
  void OnItemsRemoved(int start, int length) override;

  // EditSearchEngineControllerDelegate implementation.
  void OnEditedKeyword(TemplateURL* template_url,
                       const base::string16& title,
                       const base::string16& keyword,
                       const std::string& url) override;

  void RegisterMessages() override;

 private:
  std::unique_ptr<KeywordEditorController> list_controller_;
  std::unique_ptr<EditSearchEngineController> edit_controller_;

  // Removes the search engine at the given index. Called from WebUI.
  void RemoveSearchEngine(const base::ListValue* args);

  // Sets the search engine at the given index to be default. Called from WebUI.
  void SetDefaultSearchEngine(const base::ListValue* args);

  // Starts an edit session for the search engine at the given index. If the
  // index is -1, starts editing a new search engine instead of an existing one.
  // Called from WebUI.
  void EditSearchEngine(const base::ListValue* args);

  // Validates the given search engine values, and reports the results back
  // to WebUI. Called from WebUI.
  void CheckSearchEngineInfoValidity(const base::ListValue* args);

  // Called when an edit is cancelled.
  // Called from WebUI.
  void EditCancelled(const base::ListValue* args);

  // Called when an edit is finished and should be saved.
  // Called from WebUI.
  void EditCompleted(const base::ListValue* args);

  // Returns a dictionary to pass to WebUI representing the given search engine.
  std::unique_ptr<base::DictionaryValue> CreateDictionaryForEngine(
      int index,
      bool is_default);

  // Returns a dictionary to pass to WebUI representing the extension.
  base::DictionaryValue* CreateDictionaryForExtension(
      const extensions::Extension& extension);

  DISALLOW_COPY_AND_ASSIGN(SearchEngineManagerHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_SEARCH_ENGINE_MANAGER_HANDLER_H_
