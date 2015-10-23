// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/notification_service.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

static inline int Round(double x) {
  return static_cast<int>(x + 0.5);
}

// static
const char AvatarMenuButton::kViewClassName[] = "AvatarMenuButton";

AvatarMenuButton::AvatarMenuButton(Browser* browser, bool disabled)
    : MenuButton(NULL, base::string16(), this, false),
      browser_(browser),
      disabled_(disabled),
      is_rectangle_(false),
      old_height_(0),
      button_on_right_(false) {
  // In RTL mode, the avatar icon should be looking the opposite direction.
  EnableCanvasFlippingForRTLUI(true);

  SetEventTargeter(
      scoped_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
}

AvatarMenuButton::~AvatarMenuButton() {
}

const char* AvatarMenuButton::GetClassName() const {
  return kViewClassName;
}

void AvatarMenuButton::OnPaint(gfx::Canvas* canvas) {
  if (!icon_.get())
    return;

  if (old_height_ != height() || button_icon_.isNull()) {
    old_height_ = height();
    button_icon_ = *profiles::GetAvatarIconForTitleBar(
        *icon_, is_rectangle_, width(), height()).ToImageSkia();
  }

  // Scale the image to fit the width of the button.
  int dst_width = std::min(button_icon_.width(), width());
  // Truncate rather than rounding, so that for odd widths we put the extra
  // pixel on the left.
  int dst_x = (width() - dst_width) / 2;

  // Scale the height and maintain aspect ratio. This means that the
  // icon may not fit in the view. That's ok, we just vertically center it.
  float scale =
      static_cast<float>(dst_width) / static_cast<float>(button_icon_.width());
  // Round here so that we minimize the aspect ratio drift.
  int dst_height = Round(button_icon_.height() * scale);
  // Round rather than truncating, so that for odd heights we select an extra
  // pixel below the image center rather than above.  This is because the
  // incognito image has shadows at the top that make the apparent center below
  // the real center.
  int dst_y = Round((height() - dst_height) / 2.0);
  canvas->DrawImageInt(button_icon_, 0, 0, button_icon_.width(),
      button_icon_.height(), dst_x, dst_y, dst_width, dst_height, false);
}

void AvatarMenuButton::SetAvatarIcon(const gfx::Image& icon,
                                     bool is_rectangle) {
  icon_.reset(new gfx::Image(icon));
  button_icon_ = gfx::ImageSkia();
  is_rectangle_ = is_rectangle;
  SchedulePaint();
}

// static
bool AvatarMenuButton::GetAvatarImages(Profile* profile,
                                       bool should_show_avatar_menu,
                                       gfx::Image* avatar,
                                       gfx::Image* taskbar_badge_avatar,
                                       bool* is_rectangle) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (profile->GetProfileType() == Profile::GUEST_PROFILE) {
    *avatar = rb.
        GetImageNamed(profiles::GetPlaceholderAvatarIconResourceID());
  } else if (profile->GetProfileType() == Profile::INCOGNITO_PROFILE) {
    *avatar = rb.GetImageNamed(IDR_OTR_ICON);
    // TODO(nkostylev): Allow this on ChromeOS once the ChromeOS test
    // environment handles profile directories correctly.
#if !defined(OS_CHROMEOS)
    bool is_badge_rectangle = false;
    // The taskbar badge should be the profile avatar, not the OTR avatar.
    AvatarMenu::GetImageForMenuButton(profile->GetPath(),
                                      taskbar_badge_avatar,
                                      &is_badge_rectangle);
#endif
  } else if (should_show_avatar_menu) {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
    if (index == std::string::npos)
      return false;

    if (switches::IsNewAvatarMenu()) {
      *avatar = cache.GetAvatarIconOfProfileAtIndex(index);
      // TODO(noms): Once the code for the old avatar menu button is removed,
      // this function will only be called for badging the taskbar icon.  The
      // function can be renamed to something like GetAvatarImageForBadging()
      // and only needs to return the avatar from
      // AvatarMenu::GetImageForMenuButton().
#if !defined(OS_CHROMEOS)
      bool is_badge_rectangle = false;
      AvatarMenu::GetImageForMenuButton(profile->GetPath(),
                                        taskbar_badge_avatar,
                                        &is_badge_rectangle);
#endif
    } else {
      AvatarMenu::GetImageForMenuButton(profile->GetPath(),
                                        avatar,
                                        is_rectangle);
    }
  }
  return true;
}

// views::ViewTargeterDelegate:
bool AvatarMenuButton::DoesIntersectRect(const views::View* target,
                                         const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  return !disabled_ &&
         views::ViewTargeterDelegate::DoesIntersectRect(target, rect);
}

// views::MenuButtonListener implementation
void AvatarMenuButton::OnMenuButtonClicked(views::View* source,
                                           const gfx::Point& point) {
  if (!disabled_)
    chrome::ShowAvatarMenu(browser_);
}
