// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_
#define CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"

class Profile;
class ToolbarActionViewController;

// The registry for all component toolbar actions. Component toolbar actions
// are actions that live in the toolbar (like extension actions), but are
// components of chrome, such as ChromeCast.
class ComponentToolbarActionsFactory {
 public:
  ComponentToolbarActionsFactory();
  ~ComponentToolbarActionsFactory();

  static ComponentToolbarActionsFactory* GetInstance();

  // Returns a collection of controllers for Chrome Actions. Declared virtual
  // for testing.
  virtual ScopedVector<ToolbarActionViewController>
      GetComponentToolbarActions();

  // Returns the number of component actions.
  int GetNumComponentActions();

  // Sets the factory to use for testing purposes.
  // Ownership remains with the caller.
  static void SetTestingFactory(ComponentToolbarActionsFactory* factory);

 private:
  // The number of component actions. Initially set to -1 to denote that the
  // count has not been checked yet.
  int num_component_actions_;

  // Whether or not Media Router UI is currently enabled.
  // TODO(apacible): Replace this with a switch once there is more Media Router
  //                 functionality.
  bool media_router_ui_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ComponentToolbarActionsFactory);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_
