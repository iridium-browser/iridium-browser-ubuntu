// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_COOKIES_VIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_COOKIES_VIEW_HANDLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class CookiesTreeModelUtil;

namespace options {

class CookiesViewHandler : public OptionsPageUIHandler,
                           public CookiesTreeModel::Observer {
 public:
  CookiesViewHandler();
  ~CookiesViewHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void RegisterMessages() override;

  // CookiesTreeModel::Observer implementation.
  void TreeNodesAdded(ui::TreeModel* model,
                      ui::TreeModelNode* parent,
                      int start,
                      int count) override;
  void TreeNodesRemoved(ui::TreeModel* model,
                        ui::TreeModelNode* parent,
                        int start,
                        int count) override;
  void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) override {
  }
  void TreeModelBeginBatch(CookiesTreeModel* model) override;
  void TreeModelEndBatch(CookiesTreeModel* model) override;

 private:
  // Creates the CookiesTreeModel if neccessary.
  void EnsureCookiesTreeModelCreated();

  // Updates search filter for cookies tree model.
  void UpdateSearchResults(const base::ListValue* args);

  // Remove all sites data.
  void RemoveAll(const base::ListValue* args);

  // Remove selected sites data.
  void Remove(const base::ListValue* args);

  // Get the tree node using the tree path info in |args| and call
  // SendChildren to pass back children nodes data to WebUI.
  void LoadChildren(const base::ListValue* args);

  // Get children nodes data and pass it to 'CookiesView.loadChildren' to
  // update the WebUI.
  void SendChildren(const CookieTreeNode* parent);

  // Reloads the CookiesTreeModel and passes the nodes to
  // 'CookiesView.loadChildren' to update the WebUI.
  void ReloadCookies(const base::ListValue* args);

  // The Cookies Tree model
  std::unique_ptr<CookiesTreeModel> cookies_tree_model_;

  // Flag to indicate whether there is a batch update in progress.
  bool batch_update_;

  std::unique_ptr<CookiesTreeModelUtil> model_util_;

  DISALLOW_COPY_AND_ASSIGN(CookiesViewHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_COOKIES_VIEW_HANDLER_H_
