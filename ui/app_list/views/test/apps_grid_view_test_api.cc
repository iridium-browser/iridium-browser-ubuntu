// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/test/apps_grid_view_test_api.h"

#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/events/event.h"

namespace app_list {
namespace test {

AppsGridViewTestApi::AppsGridViewTestApi(AppsGridView* view)
    : view_(view) {
}

AppsGridViewTestApi::~AppsGridViewTestApi() {
}

views::View* AppsGridViewTestApi::GetViewAtModelIndex(int index) const {
  return view_->view_model_.view_at(index);
}

void AppsGridViewTestApi::LayoutToIdealBounds() {
  if (view_->reorder_timer_.IsRunning()) {
    view_->reorder_timer_.Stop();
    view_->OnReorderTimer();
  }
  if (view_->folder_dropping_timer_.IsRunning()) {
    view_->folder_dropping_timer_.Stop();
    view_->OnFolderDroppingTimer();
  }
  view_->bounds_animator_.Cancel();
  view_->Layout();
}

void AppsGridViewTestApi::SetPageFlipDelay(int page_flip_delay_in_ms) {
  view_->page_flip_delay_in_ms_ = page_flip_delay_in_ms;
}

void AppsGridViewTestApi::PressItemAt(int index) {
  GetViewAtModelIndex(index)->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE));
}

void AppsGridViewTestApi::DisableSynchronousDrag() {
#if defined(OS_WIN)
  DCHECK(view_->synchronous_drag_.Get() == NULL)
      << "DisableSynchronousDrag needs to "
         "be called before "
         "synchronous_drag_ is set up.";
  view_->use_synchronous_drag_ = false;
#endif
}

bool AppsGridViewTestApi::HasPendingPageFlip() const {
  return view_->page_flip_timer_.IsRunning() ||
         view_->pagination_model()->has_transition();
}

}  // namespace test
}  // namespace app_list
