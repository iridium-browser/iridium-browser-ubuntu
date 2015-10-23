// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/session_state_animator_impl.h"
#include "ui/aura/client/aura_constants.h"

typedef ash::test::AshTestBase SessionStateAnimatiorImplContainersTest;

namespace ash {
namespace {

bool ParentHasWindowWithId(const aura::Window* window, int id) {
  return window->parent()->id() == id;
}

bool ContainersHaveWindowWithId(const aura::Window::Windows windows, int id) {
  for (const aura::Window* window : windows) {
    if (window->id() == id)
      return true;
  }
  return false;
}

}  // namespace

TEST_F(SessionStateAnimatiorImplContainersTest, ContainersHaveIdTest) {
  aura::Window::Windows containers;

  // Test ROOT_CONTAINER mask.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  SessionStateAnimatorImpl::GetContainers(SessionStateAnimator::ROOT_CONTAINER,
                                          &containers);
  EXPECT_EQ(root_window, containers[0]);

  containers.clear();

  SessionStateAnimatorImpl::GetContainers(
      SessionStateAnimator::DESKTOP_BACKGROUND, &containers);
  EXPECT_TRUE(ContainersHaveWindowWithId(
      containers, kShellWindowId_DesktopBackgroundContainer));

  containers.clear();

  // Check for shelf in launcher.
  SessionStateAnimatorImpl::GetContainers(SessionStateAnimator::LAUNCHER,
                                          &containers);
  EXPECT_TRUE(
      ContainersHaveWindowWithId(containers, kShellWindowId_ShelfContainer));

  containers.clear();

  SessionStateAnimatorImpl::GetContainers(
      SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS, &containers);
  EXPECT_TRUE(ParentHasWindowWithId(
      containers[0], kShellWindowId_NonLockScreenContainersContainer));

  containers.clear();

  // Check for lock screen containers.
  SessionStateAnimatorImpl::GetContainers(
      SessionStateAnimator::LOCK_SCREEN_BACKGROUND, &containers);
  EXPECT_TRUE(ContainersHaveWindowWithId(
      containers, kShellWindowId_LockScreenBackgroundContainer));

  containers.clear();

  // Check for the lock screen containers container.
  SessionStateAnimatorImpl::GetContainers(
      SessionStateAnimator::LOCK_SCREEN_RELATED_CONTAINERS, &containers);
  EXPECT_TRUE(ContainersHaveWindowWithId(
      containers, kShellWindowId_LockScreenRelatedContainersContainer));

  // Empty mask should clear the container.
  SessionStateAnimatorImpl::GetContainers(0, &containers);
  EXPECT_TRUE(containers.empty());
}

}  // namespace ash
