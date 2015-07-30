// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SERVER_VIEW_OBSERVER_H_
#define COMPONENTS_VIEW_MANAGER_SERVER_VIEW_OBSERVER_H_

#include "components/view_manager/public/interfaces/view_manager_constants.mojom.h"

namespace gfx {
class Rect;
}

namespace mojo {
class ViewportMetrics;
}

namespace view_manager {

class ServerView;

// TODO(sky): rename to OnDid and OnWill everywhere.
class ServerViewObserver {
 public:
  // Invoked when a view is about to be destroyed; before any of the children
  // have been removed and before the view has been removed from its parent.
  virtual void OnWillDestroyView(ServerView* view) {}

  // Invoked at the end of the View's destructor (after it has been removed from
  // the hierarchy).
  virtual void OnViewDestroyed(ServerView* view) {}

  virtual void OnWillChangeViewHierarchy(ServerView* view,
                                         ServerView* new_parent,
                                         ServerView* old_parent) {}

  virtual void OnViewHierarchyChanged(ServerView* view,
                                      ServerView* new_parent,
                                      ServerView* old_parent) {}

  virtual void OnViewBoundsChanged(ServerView* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) {}

  virtual void OnViewReordered(ServerView* view,
                               ServerView* relative,
                               mojo::OrderDirection direction) {}

  virtual void OnWillChangeViewVisibility(ServerView* view) {}
  virtual void OnViewVisibilityChanged(ServerView* view) {}

  virtual void OnViewSharedPropertyChanged(
      ServerView* view,
      const std::string& name,
      const std::vector<uint8_t>* new_data) {}

 protected:
  virtual ~ServerViewObserver() {}
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_SERVER_VIEW_OBSERVER_H_
