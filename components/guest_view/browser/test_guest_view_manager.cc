// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/test_guest_view_manager.h"

#include "base/memory/scoped_ptr.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"

namespace guest_view {

TestGuestViewManager::TestGuestViewManager(
    content::BrowserContext* context,
    scoped_ptr<GuestViewManagerDelegate> delegate)
    : GuestViewManager(context, delegate.Pass()),
      num_guests_created_(0) {
}

TestGuestViewManager::~TestGuestViewManager() {
}

size_t TestGuestViewManager::GetNumGuestsActive() const {
  return guest_web_contents_by_instance_id_.size();
}

size_t TestGuestViewManager::GetNumRemovedInstanceIDs() const {
  return removed_instance_ids_.size();
}

content::WebContents* TestGuestViewManager::GetLastGuestCreated() {
  content::WebContents* web_contents = nullptr;
  for (int i = current_instance_id_; i >= 0; i--) {
    web_contents = GetGuestByInstanceID(i);
    if (web_contents) {
      break;
    }
  }
  return web_contents;
}

void TestGuestViewManager::WaitForAllGuestsDeleted() {
  // Make sure that every guest that was created have been removed.
  for (auto& watcher : guest_web_contents_watchers_)
    watcher->Wait();
}

void TestGuestViewManager::WaitForGuestCreated() {
  created_message_loop_runner_ = new content::MessageLoopRunner;
  created_message_loop_runner_->Run();
}

content::WebContents* TestGuestViewManager::WaitForSingleGuestCreated() {
  if (!GetNumGuestsActive()) {
    // Guests have been created and subsequently destroyed.
    if (num_guests_created() > 0)
      return nullptr;
    WaitForGuestCreated();
  }

  return GetLastGuestCreated();
}

void TestGuestViewManager::AddGuest(int guest_instance_id,
                                    content::WebContents* guest_web_contents) {
  GuestViewManager::AddGuest(guest_instance_id, guest_web_contents);

  guest_web_contents_watchers_.push_back(
      linked_ptr<content::WebContentsDestroyedWatcher>(
          new content::WebContentsDestroyedWatcher(guest_web_contents)));

  ++num_guests_created_;

  if (created_message_loop_runner_.get())
    created_message_loop_runner_->Quit();
}

void TestGuestViewManager::RemoveGuest(int guest_instance_id) {
  GuestViewManager::RemoveGuest(guest_instance_id);
}

// Test factory for creating test instances of GuestViewManager.
TestGuestViewManagerFactory::TestGuestViewManagerFactory()
    : test_guest_view_manager_(NULL) {
}

TestGuestViewManagerFactory::~TestGuestViewManagerFactory() {
}

GuestViewManager* TestGuestViewManagerFactory::CreateGuestViewManager(
    content::BrowserContext* context,
    scoped_ptr<GuestViewManagerDelegate> delegate) {
  if (!test_guest_view_manager_) {
    test_guest_view_manager_ =
        new TestGuestViewManager(context, delegate.Pass());
  }
  return test_guest_view_manager_;
}

}  // namespace guest_view
