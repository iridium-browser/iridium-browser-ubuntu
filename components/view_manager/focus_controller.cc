// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/focus_controller.h"

#include "components/view_manager/focus_controller_delegate.h"
#include "components/view_manager/server_view.h"
#include "components/view_manager/server_view_drawn_tracker.h"

namespace view_manager {

FocusController::FocusController(FocusControllerDelegate* delegate)
    : delegate_(delegate) {
}

FocusController::~FocusController() {
}

void FocusController::SetFocusedView(ServerView* view) {
  if (GetFocusedView() == view)
    return;

  SetFocusedViewImpl(view, CHANGE_SOURCE_EXPLICIT);
}

ServerView* FocusController::GetFocusedView() {
  return drawn_tracker_ ? drawn_tracker_->view() : nullptr;
}

void FocusController::SetFocusedViewImpl(ServerView* view,
                                         ChangeSource change_source) {
  ServerView* old = GetFocusedView();

  DCHECK(!view || view->IsDrawn());

  if (view)
    drawn_tracker_.reset(new ServerViewDrawnTracker(view, this));
  else
    drawn_tracker_.reset();

  if (change_source == CHANGE_SOURCE_DRAWN_STATE_CHANGED)
    delegate_->OnFocusChanged(old, view);
}

void FocusController::OnDrawnStateChanged(ServerView* ancestor,
                                          ServerView* view,
                                          bool is_drawn) {
  DCHECK(!is_drawn);  // We only observe when drawn.
  SetFocusedViewImpl(ancestor, CHANGE_SOURCE_DRAWN_STATE_CHANGED);
}

}  // namespace view_manager
