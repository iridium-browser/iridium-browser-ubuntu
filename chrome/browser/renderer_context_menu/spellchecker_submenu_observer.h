// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_SPELLCHECKER_SUBMENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_SPELLCHECKER_SUBMENU_OBSERVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/base/models/simple_menu_model.h"

class RenderViewContextMenuProxy;

// A class that implements the 'spell-checker options' submenu. This class
// creates the submenu, add it to the parent menu, and handles events.
class SpellCheckerSubMenuObserver : public RenderViewContextMenuObserver {
 public:
  SpellCheckerSubMenuObserver(RenderViewContextMenuProxy* proxy,
                              ui::SimpleMenuModel::Delegate* delegate,
                              int group);
  ~SpellCheckerSubMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  // The interface for adding a submenu to the parent.
  RenderViewContextMenuProxy* proxy_;

  // The submenu of the 'spell-checker options'. This class adds items to this
  // submenu and add it to the parent menu.
  ui::SimpleMenuModel submenu_model_;

#if !defined(OS_MACOSX)
  // Hunspell spelling submenu.
  // The radio items representing languages available for spellchecking.
  int language_group_;
  // The number of languages currently selected for spellchecking, which are
  // also the first elements in |languages_|.
  size_t num_selected_languages_;
  // A vector of all languages available for spellchecking.
  std::vector<std::string> languages_;
#endif  // !OS_MACOSX

  DISALLOW_COPY_AND_ASSIGN(SpellCheckerSubMenuObserver);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_SPELLCHECKER_SUBMENU_OBSERVER_H_
