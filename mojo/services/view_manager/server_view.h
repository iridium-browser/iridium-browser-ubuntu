// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIEW_MANAGER_SERVER_VIEW_H_
#define SERVICES_VIEW_MANAGER_SERVER_VIEW_H_

#include <vector>

#include "base/logging.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/services/view_manager/ids.h"
#include "third_party/mojo_services/src/view_manager/public/interfaces/view_manager.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace view_manager {

class ServerViewDelegate;

// Server side representation of a view. Delegate is informed of interesting
// events.
//
// It is assumed that all functions that mutate the tree have validated the
// mutation is possible before hand. For example, Reorder() assumes the supplied
// view is a child and not already in position.
class ServerView {
 public:
  ServerView(ServerViewDelegate* delegate, const ViewId& id);
  virtual ~ServerView();

  const ViewId& id() const { return id_; }

  void Add(ServerView* child);
  void Remove(ServerView* child);
  void Reorder(ServerView* child,
               ServerView* relative,
               mojo::OrderDirection direction);

  const gfx::Rect& bounds() const { return bounds_; }
  void SetBounds(const gfx::Rect& bounds);

  const ServerView* parent() const { return parent_; }
  ServerView* parent() { return parent_; }

  const ServerView* GetRoot() const;
  ServerView* GetRoot() {
    return const_cast<ServerView*>(
        const_cast<const ServerView*>(this)->GetRoot());
  }

  std::vector<const ServerView*> GetChildren() const;
  std::vector<ServerView*> GetChildren();

  // Returns true if this contains |view| or is |view|.
  bool Contains(const ServerView* view) const;

  // Returns true if the window is visible. This does not consider visibility
  // of any ancestors.
  bool visible() const { return visible_; }
  void SetVisible(bool value);

  float opacity() const { return opacity_; }
  void SetOpacity(float value);

  const gfx::Transform& transform() const { return transform_; }
  void SetTransform(const gfx::Transform& transform);

  const std::map<std::string, std::vector<uint8_t>>& properties() const {
    return properties_;
  }
  void SetProperty(const std::string& name, const std::vector<uint8_t>* value);

  // Returns true if this view is attached to |root| and all ancestors are
  // visible.
  bool IsDrawn(const ServerView* root) const;

  void SetSurfaceId(cc::SurfaceId surface_id);
  const cc::SurfaceId& surface_id() const { return surface_id_; }

#if !defined(NDEBUG)
  std::string GetDebugWindowHierarchy() const;
  void BuildDebugInfo(const std::string& depth, std::string* result) const;
#endif

 private:
  typedef std::vector<ServerView*> Views;

  // Implementation of removing a view. Doesn't send any notification.
  void RemoveImpl(ServerView* view);

  ServerViewDelegate* delegate_;
  const ViewId id_;
  ServerView* parent_;
  Views children_;
  bool visible_;
  gfx::Rect bounds_;
  cc::SurfaceId surface_id_;
  float opacity_;
  gfx::Transform transform_;

  std::map<std::string, std::vector<uint8_t>> properties_;

  DISALLOW_COPY_AND_ASSIGN(ServerView);
};

}  // namespace view_manager

#endif  // SERVICES_VIEW_MANAGER_SERVER_VIEW_H_
