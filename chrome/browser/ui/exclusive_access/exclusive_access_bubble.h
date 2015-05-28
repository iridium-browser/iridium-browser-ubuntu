// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

class ExclusiveAccessManager;

namespace gfx {
class Rect;
}

// Bubble that informs the user when an exclusive access state is in effect and
// as to how to exit out of the state. Currently there are two exclusive access
// state, namely fullscreen and mouse lock.
class ExclusiveAccessBubble : public gfx::AnimationDelegate {
 public:
  explicit ExclusiveAccessBubble(ExclusiveAccessManager* manager,
                                 const GURL& url,
                                 ExclusiveAccessBubbleType bubble_type);
  ~ExclusiveAccessBubble() override;

 protected:
  static const int kPaddingPx;        // Amount of padding around the link
  static const int kInitialDelayMs;   // Initial time bubble remains onscreen
  static const int kIdleTimeMs;       // Time before mouse idle triggers hide
  static const int kPositionCheckHz;  // How fast to check the mouse position
  static const int kSlideInRegionHeightPx;
  // Height of region triggering
  // slide-in
  static const int kPopupTopPx;          // Space between the popup and the top
                                         // of the screen.
  static const int kSlideInDurationMs;   // Duration of slide-in animation
  static const int kSlideOutDurationMs;  // Duration of slide-out animation

  // Returns the current desirable rect for the popup window.  If
  // |ignore_animation_state| is true this returns the rect assuming the popup
  // is fully onscreen.
  virtual gfx::Rect GetPopupRect(bool ignore_animation_state) const = 0;
  virtual gfx::Point GetCursorScreenPoint() = 0;
  virtual bool WindowContainsPoint(gfx::Point pos) = 0;

  // Returns true if the window is active.
  virtual bool IsWindowActive() = 0;

  // Hides the bubble.  This is a separate function so it can be called by a
  // timer.
  virtual void Hide() = 0;

  // Shows the bubble.
  virtual void Show() = 0;

  virtual bool IsAnimating() = 0;

  // True if the mouse position can trigger sliding in the exit fullscreen
  // bubble when the bubble is hidden.
  virtual bool CanMouseTriggerSlideIn() const = 0;

  void StartWatchingMouse();
  void StopWatchingMouse();
  bool IsWatchingMouse() const;

  // Called repeatedly to get the current mouse position and animate the bubble
  // on or off the screen as appropriate.
  void CheckMousePosition();

  void ExitExclusiveAccess();
  // Accepts the request. Can cause FullscreenExitBubble to be deleted.
  void Accept();
  // Denys the request. Can cause FullscreenExitBubble to be deleted.
  void Cancel();

  // The following strings may change according to the content type and URL.
  base::string16 GetCurrentMessageText() const;
  base::string16 GetCurrentDenyButtonText() const;
  base::string16 GetCurrentAllowButtonText() const;

  // The following strings never change.
  base::string16 GetInstructionText() const;

  // The Manager associated with this bubble.
  ExclusiveAccessManager* const manager_;

  // The host the bubble is for, can be empty.
  GURL url_;

  // The type of the bubble; controls e.g. which buttons to show.
  ExclusiveAccessBubbleType bubble_type_;

 private:
  // Timer to delay before allowing the bubble to hide after it's initially
  // shown.
  base::OneShotTimer<ExclusiveAccessBubble> initial_delay_;

  // Timer to see how long the mouse has been idle.
  base::OneShotTimer<ExclusiveAccessBubble> idle_timeout_;

  // Timer to poll the current mouse position.  We can't just listen for mouse
  // events without putting a non-empty HWND onscreen (or hooking Windows, which
  // has other problems), so instead we run a low-frequency poller to see if the
  // user has moved in or out of our show/hide regions.
  base::RepeatingTimer<ExclusiveAccessBubble> mouse_position_checker_;

  // The most recently seen mouse position, in screen coordinates.  Used to see
  // if the mouse has moved since our last check.
  gfx::Point last_mouse_pos_;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveAccessBubble);
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_H_
