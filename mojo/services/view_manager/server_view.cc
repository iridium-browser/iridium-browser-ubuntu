// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/server_view.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "mojo/services/view_manager/server_view_delegate.h"

namespace view_manager {

ServerView::ServerView(ServerViewDelegate* delegate, const ViewId& id)
    : delegate_(delegate),
      id_(id),
      parent_(nullptr),
      visible_(false),
      opacity_(1) {
  DCHECK(delegate);  // Must provide a delegate.
}

ServerView::~ServerView() {
  delegate_->OnWillDestroyView(this);

  while (!children_.empty())
    children_.front()->parent()->Remove(children_.front());

  if (parent_)
    parent_->Remove(this);

  delegate_->OnViewDestroyed(this);
}

void ServerView::Add(ServerView* child) {
  // We assume validation checks happened already.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(!child->Contains(this));
  if (child->parent() == this) {
    if (children_.size() == 1)
      return;  // Already in the right position.
    Reorder(child, children_.back(), mojo::ORDER_DIRECTION_ABOVE);
    return;
  }

  ServerView* old_parent = child->parent();
  child->delegate_->OnWillChangeViewHierarchy(child, this, old_parent);
  if (child->parent())
    child->parent()->RemoveImpl(child);

  child->parent_ = this;
  children_.push_back(child);
  child->delegate_->OnViewHierarchyChanged(child, this, old_parent);
}

void ServerView::Remove(ServerView* child) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(child->parent() == this);

  child->delegate_->OnWillChangeViewHierarchy(child, NULL, this);
  RemoveImpl(child);
  child->delegate_->OnViewHierarchyChanged(child, NULL, this);
}

void ServerView::Reorder(ServerView* child,
                         ServerView* relative,
                         mojo::OrderDirection direction) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child->parent() == this);
  DCHECK_GT(children_.size(), 1u);
  children_.erase(std::find(children_.begin(), children_.end(), child));
  Views::iterator i = std::find(children_.begin(), children_.end(), relative);
  if (direction == mojo::ORDER_DIRECTION_ABOVE) {
    DCHECK(i != children_.end());
    children_.insert(++i, child);
  } else if (direction == mojo::ORDER_DIRECTION_BELOW) {
    DCHECK(i != children_.end());
    children_.insert(i, child);
  }
  delegate_->OnViewReordered(this, relative, direction);
}

void ServerView::SetBounds(const gfx::Rect& bounds) {
  if (bounds_ == bounds)
    return;

  const gfx::Rect old_bounds = bounds_;
  bounds_ = bounds;
  delegate_->OnViewBoundsChanged(this, old_bounds, bounds);
}

const ServerView* ServerView::GetRoot() const {
  const ServerView* view = this;
  while (view && view->parent())
    view = view->parent();
  return view;
}

std::vector<const ServerView*> ServerView::GetChildren() const {
  std::vector<const ServerView*> children;
  children.reserve(children_.size());
  for (size_t i = 0; i < children_.size(); ++i)
    children.push_back(children_[i]);
  return children;
}

std::vector<ServerView*> ServerView::GetChildren() {
  // TODO(sky): rename to children() and fix return type.
  return children_;
}

bool ServerView::Contains(const ServerView* view) const {
  for (const ServerView* parent = view; parent; parent = parent->parent_) {
    if (parent == this)
      return true;
  }
  return false;
}

void ServerView::SetVisible(bool value) {
  if (visible_ == value)
    return;

  delegate_->OnWillChangeViewVisibility(this);
  visible_ = value;
}

void ServerView::SetOpacity(float value) {
  if (value == opacity_)
    return;
  opacity_ = value;
  delegate_->OnScheduleViewPaint(this);
}

void ServerView::SetTransform(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;

  transform_ = transform;
  delegate_->OnScheduleViewPaint(this);
}

void ServerView::SetProperty(const std::string& name,
                             const std::vector<uint8_t>* value) {
  auto it = properties_.find(name);
  if (it != properties_.end()) {
    if (value && it->second == *value)
      return;
  } else if (!value) {
    // This property isn't set in |properties_| and |value| is NULL, so there's
    // no change.
    return;
  }

  if (value) {
    properties_[name] = *value;
  } else if (it != properties_.end()) {
    properties_.erase(it);
  }

  delegate_->OnViewSharedPropertyChanged(this, name, value);
}

bool ServerView::IsDrawn(const ServerView* root) const {
  if (!root->visible_)
    return false;
  const ServerView* view = this;
  while (view && view != root && view->visible_)
    view = view->parent_;
  return view == root;
}

void ServerView::SetSurfaceId(cc::SurfaceId surface_id) {
  surface_id_ = surface_id;
  delegate_->OnViewSurfaceIdChanged(this);
}

#if !defined(NDEBUG)
std::string ServerView::GetDebugWindowHierarchy() const {
  std::string result;
  BuildDebugInfo(std::string(), &result);
  return result;
}

void ServerView::BuildDebugInfo(const std::string& depth,
                                std::string* result) const {
  *result += base::StringPrintf(
      "%sid=%d,%d visible=%s bounds=%d,%d %dx%d surface_id=%" PRIu64 "\n",
      depth.c_str(), static_cast<int>(id_.connection_id),
      static_cast<int>(id_.view_id), visible_ ? "true" : "false", bounds_.x(),
      bounds_.y(), bounds_.width(), bounds_.height(), surface_id_.id);
  for (const ServerView* child : children_)
    child->BuildDebugInfo(depth + "  ", result);
}
#endif

void ServerView::RemoveImpl(ServerView* view) {
  view->parent_ = NULL;
  children_.erase(std::find(children_.begin(), children_.end(), view));
}

}  // namespace view_manager
