// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/web_notification/web_notification_tray.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_bubble_wrapper.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/system/web_notification/ash_popup_alignment_delegate.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "base/auto_reset.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/views/message_bubble_base.h"
#include "ui/message_center/views/message_center_bubble.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/tray_bubble_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/fill_layout.h"

namespace message_center {

MessageCenterTrayDelegate* CreateMessageCenterTray() {
  // On non-CrOS, the Tray will not be hosted in ash::Shell.
  NOTREACHED();
  return nullptr;
}

}  // namespace message_center

namespace ash {
namespace {

// Menu commands
constexpr int kToggleQuietMode = 0;
constexpr int kEnableQuietModeDay = 2;

constexpr int kMaximumSmallIconCount = 3;

constexpr gfx::Size kTrayItemInnerIconSize(16, 16);
constexpr gfx::Size kTrayItemInnerBellIconSizeNonMd(18, 18);
constexpr gfx::Size kTrayItemOuterSize(26, 26);
constexpr int kTrayMainAxisInset = 3;
constexpr int kTrayCrossAxisInset = 0;

constexpr int kTrayItemAnimationDurationMS = 200;

constexpr size_t kMaximumNotificationNumber = 99;

// Flag to disable animation. Only for testing.
bool disable_animations_for_test = false;
}

namespace {

const SkColor kWebNotificationColorNoUnread =
    SkColorSetARGB(128, 255, 255, 255);
const SkColor kWebNotificationColorWithUnread = SK_ColorWHITE;
const int kNoUnreadIconSize = 18;

}  // namespace

// Class to initialize and manage the WebNotificationBubble and
// TrayBubbleWrapper instances for a bubble.
class WebNotificationBubbleWrapper {
 public:
  // Takes ownership of |bubble| and creates |bubble_wrapper_|.
  WebNotificationBubbleWrapper(WebNotificationTray* tray,
                               TrayBackgroundView* anchor_tray,
                               message_center::MessageBubbleBase* bubble) {
    bubble_.reset(bubble);
    views::TrayBubbleView::AnchorAlignment anchor_alignment =
        tray->GetAnchorAlignment();
    views::TrayBubbleView::InitParams init_params =
        bubble->GetInitParams(anchor_alignment);
    views::TrayBubbleView* bubble_view = views::TrayBubbleView::Create(
        anchor_tray->GetBubbleAnchor(), tray, &init_params);
    bubble_view->set_anchor_view_insets(anchor_tray->GetBubbleAnchorInsets());
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_view));
    bubble->InitializeContents(bubble_view);
  }

  message_center::MessageBubbleBase* bubble() const { return bubble_.get(); }

  // Convenience accessors.
  views::TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }

 private:
  std::unique_ptr<message_center::MessageBubbleBase> bubble_;
  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationBubbleWrapper);
};

class WebNotificationItem : public views::View, public gfx::AnimationDelegate {
 public:
  WebNotificationItem(gfx::AnimationContainer* container,
                      WebNotificationTray* tray)
      : tray_(tray) {
    SetPaintToLayer(true);
    layer()->SetFillsBoundsOpaquely(false);
    views::View::SetVisible(false);
    set_owned_by_client();

    SetLayoutManager(new views::FillLayout);

    animation_.reset(new gfx::SlideAnimation(this));
    animation_->SetContainer(container);
    animation_->SetSlideDuration(kTrayItemAnimationDurationMS);
    animation_->SetTweenType(gfx::Tween::LINEAR);
  }

  void SetVisible(bool set_visible) override {
    if (!GetWidget() || disable_animations_for_test) {
      views::View::SetVisible(set_visible);
      return;
    }

    if (!set_visible) {
      animation_->Hide();
      AnimationProgressed(animation_.get());
    } else {
      animation_->Show();
      AnimationProgressed(animation_.get());
      views::View::SetVisible(true);
    }
  }

  void HideAndDelete() {
    SetVisible(false);

    if (!visible() && !animation_->is_animating()) {
      if (parent())
        parent()->RemoveChildView(this);
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    } else {
      delete_after_animation_ = true;
    }
  }

 protected:
  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override {
    if (!animation_.get() || !animation_->is_animating())
      return kTrayItemOuterSize;

    // Animate the width (or height) when this item shows (or hides) so that
    // the icons on the left are shifted with the animation.
    // Note that TrayItemView does the same thing.
    gfx::Size size = kTrayItemOuterSize;
    if (IsHorizontalLayout()) {
      size.set_width(std::max(
          1, gfx::ToRoundedInt(size.width() * animation_->GetCurrentValue())));
    } else {
      size.set_height(std::max(
          1, gfx::ToRoundedInt(size.height() * animation_->GetCurrentValue())));
    }
    return size;
  }

  int GetHeightForWidth(int width) const override {
    return GetPreferredSize().height();
  }

  bool IsHorizontalLayout() const {
    return IsHorizontalAlignment(tray_->shelf_alignment());
  }

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    gfx::Transform transform;
    if (IsHorizontalLayout()) {
      transform.Translate(0, animation->CurrentValueBetween(
                                 static_cast<double>(height()) / 2., 0.));
    } else {
      transform.Translate(
          animation->CurrentValueBetween(static_cast<double>(width() / 2.), 0.),
          0);
    }
    transform.Scale(animation->GetCurrentValue(), animation->GetCurrentValue());
    layer()->SetTransform(transform);
    PreferredSizeChanged();
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    if (animation->GetCurrentValue() < 0.1)
      views::View::SetVisible(false);

    if (delete_after_animation_) {
      if (parent())
        parent()->RemoveChildView(this);
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    }
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  std::unique_ptr<gfx::SlideAnimation> animation_;
  bool delete_after_animation_ = false;
  WebNotificationTray* tray_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationItem);
};

class WebNotificationImage : public WebNotificationItem {
 public:
  WebNotificationImage(const gfx::ImageSkia& image,
                       const gfx::Size& size,
                       gfx::AnimationContainer* container,
                       WebNotificationTray* tray)
      : WebNotificationItem(container, tray) {
    view_ = new views::ImageView();
    view_->SetImage(image);
    view_->SetImageSize(size);
    AddChildView(view_);
  }

 private:
  views::ImageView* view_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationImage);
};

class WebNotificationLabel : public WebNotificationItem {
 public:
  WebNotificationLabel(gfx::AnimationContainer* container,
                       WebNotificationTray* tray)
      : WebNotificationItem(container, tray) {
    view_ = new views::Label();
    SetupLabelForTray(view_);
    AddChildView(view_);
  }

  void SetNotificationCount(bool small_icons_exist, size_t notification_count) {
    notification_count = std::min(notification_count,
                                  kMaximumNotificationNumber);  // cap with 99

    // TODO(yoshiki): Use a string for "99" and "+99".

    base::string16 str = base::FormatNumber(notification_count);
    if (small_icons_exist) {
      if (!base::i18n::IsRTL())
        str = base::ASCIIToUTF16("+") + str;
      else
        str = str + base::ASCIIToUTF16("+");
    }

    view_->SetText(str);
    view_->SetEnabledColor(kWebNotificationColorWithUnread);
    SchedulePaint();
  }

 private:
  views::Label* view_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationLabel);
};

WebNotificationTray::WebNotificationTray(WmShelf* shelf,
                                         WmWindow* status_area_window,
                                         SystemTray* system_tray)
    : TrayBackgroundView(shelf),
      status_area_window_(status_area_window),
      system_tray_(system_tray),
      show_message_center_on_unlock_(false),
      should_update_tray_content_(false),
      should_block_shelf_auto_hide_(false) {
  DCHECK(shelf);
  DCHECK(status_area_window_);
  DCHECK(system_tray_);

  if (MaterialDesignController::IsShelfMaterial()) {
    SetInkDropMode(InkDropMode::ON);
    SetContentsBackground(false);
    gfx::ImageSkia bell_image =
        CreateVectorIcon(kShelfNotificationsIcon, kShelfIconColor);
    const gfx::Size bell_icon_size = kTrayItemInnerIconSize;
    bell_icon_.reset(new WebNotificationImage(
        bell_image, bell_icon_size, animation_container_.get(), this));
  } else {
    SetContentsBackground(true);
    gfx::ImageSkia bell_image =
        CreateVectorIcon(gfx::VectorIconId::NOTIFICATIONS, kNoUnreadIconSize,
                         kWebNotificationColorNoUnread);
    const gfx::Size bell_icon_size = kTrayItemInnerBellIconSizeNonMd;
    bell_icon_.reset(new WebNotificationImage(
        bell_image, bell_icon_size, animation_container_.get(), this));
  }
  tray_container()->AddChildView(bell_icon_.get());

  counter_.reset(new WebNotificationLabel(animation_container_.get(), this));
  tray_container()->AddChildView(counter_.get());

  message_center_tray_.reset(new message_center::MessageCenterTray(
      this, message_center::MessageCenter::Get()));
  popup_alignment_delegate_.reset(new AshPopupAlignmentDelegate(shelf));
  popup_collection_.reset(new message_center::MessagePopupCollection(
      message_center(), message_center_tray_.get(),
      popup_alignment_delegate_.get()));
  const display::Display& display =
      status_area_window_->GetDisplayNearestWindow();
  popup_alignment_delegate_->StartObserving(display::Screen::GetScreen(),
                                            display);
  OnMessageCenterTrayChanged();

  tray_container()->SetMargin(kTrayMainAxisInset, kTrayCrossAxisInset);
}

WebNotificationTray::~WebNotificationTray() {
  // Release any child views that might have back pointers before ~View().
  message_center_bubble_.reset();
  popup_alignment_delegate_.reset();
  popup_collection_.reset();
}

// static
void WebNotificationTray::DisableAnimationsForTest(bool disable) {
  disable_animations_for_test = disable;
}

// Public methods.

bool WebNotificationTray::ShowMessageCenterInternal(bool show_settings) {
  if (!ShouldShowMessageCenter())
    return false;

  should_block_shelf_auto_hide_ = true;
  message_center::MessageCenterBubble* message_center_bubble =
      new message_center::MessageCenterBubble(message_center(),
                                              message_center_tray_.get());

  int max_height;
  if (IsHorizontalAlignment(shelf_alignment())) {
    max_height = shelf()->GetIdealBounds().y();
  } else {
    // Assume the status area and bubble bottoms are aligned when vertical.
    gfx::Rect bounds_in_root =
        status_area_window_->GetRootWindow()->ConvertRectFromScreen(
            status_area_window_->GetBoundsInScreen());
    max_height = bounds_in_root.bottom();
  }
  message_center_bubble->SetMaxHeight(
      std::max(0, max_height - GetTrayConstant(TRAY_SPACING)));
  if (show_settings)
    message_center_bubble->SetSettingsVisible();

  // For vertical shelf alignments, anchor to the WebNotificationTray, but for
  // horizontal (i.e. bottom) shelves, anchor to the system tray.
  TrayBackgroundView* anchor_tray = this;
  if (IsHorizontalAlignment(shelf_alignment())) {
    anchor_tray = WmShelf::ForWindow(status_area_window_)
                      ->GetStatusAreaWidget()
                      ->system_tray();
  }

  message_center_bubble_.reset(new WebNotificationBubbleWrapper(
      this, anchor_tray, message_center_bubble));

  system_tray_->SetHideNotifications(true);
  shelf()->UpdateAutoHideState();
  SetIsActive(true);
  return true;
}

bool WebNotificationTray::ShowMessageCenter() {
  return ShowMessageCenterInternal(false /* show_settings */);
}

void WebNotificationTray::HideMessageCenter() {
  if (!message_center_bubble())
    return;
  SetIsActive(false);
  message_center_bubble_.reset();
  should_block_shelf_auto_hide_ = false;
  show_message_center_on_unlock_ = false;
  system_tray_->SetHideNotifications(false);
  shelf()->UpdateAutoHideState();
}

void WebNotificationTray::SetTrayBubbleHeight(int height) {
  popup_alignment_delegate_->SetTrayBubbleHeight(height);
}

int WebNotificationTray::tray_bubble_height_for_test() const {
  return popup_alignment_delegate_->tray_bubble_height_for_test();
}

bool WebNotificationTray::ShowPopups() {
  if (message_center_bubble())
    return false;

  popup_collection_->DoUpdateIfPossible();
  return true;
}

void WebNotificationTray::HidePopups() {
  DCHECK(popup_collection_.get());
  popup_collection_->MarkAllPopupsShown();
}

// Private methods.

bool WebNotificationTray::ShouldShowMessageCenter() {
  return WmShell::Get()->system_tray_delegate()->ShouldShowNotificationTray() &&
         !system_tray_->HasNotificationBubble();
}

bool WebNotificationTray::ShouldBlockShelfAutoHide() const {
  return should_block_shelf_auto_hide_;
}

bool WebNotificationTray::IsMessageCenterBubbleVisible() const {
  return (message_center_bubble() &&
          message_center_bubble()->bubble()->IsVisible());
}

bool WebNotificationTray::IsMouseInNotificationBubble() const {
  return false;
}

void WebNotificationTray::ShowMessageCenterBubble() {
  if (!IsMessageCenterBubbleVisible())
    message_center_tray_->ShowMessageCenterBubble();
}

void WebNotificationTray::UpdateAfterLoginStatusChange(
    LoginStatus login_status) {
  message_center()->SetLockedState(login_status == LoginStatus::LOCKED);
  OnMessageCenterTrayChanged();
}

void WebNotificationTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  TrayBackgroundView::SetShelfAlignment(alignment);
  // Destroy any existing bubble so that it will be rebuilt correctly.
  message_center_tray_->HideMessageCenterBubble();
  message_center_tray_->HidePopupBubble();
}

void WebNotificationTray::AnchorUpdated() {
  if (message_center_bubble()) {
    message_center_bubble()->bubble_view()->UpdateBubble();
    UpdateBubbleViewArrow(message_center_bubble()->bubble_view());
  }
}

base::string16 WebNotificationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_ACCESSIBLE_NAME);
}

void WebNotificationTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {
  if (message_center_bubble() &&
      bubble_view == message_center_bubble()->bubble_view()) {
    message_center_tray_->HideMessageCenterBubble();
  } else if (popup_collection_.get()) {
    message_center_tray_->HidePopupBubble();
  }
}

bool WebNotificationTray::PerformAction(const ui::Event& event) {
  if (message_center_bubble())
    message_center_tray_->HideMessageCenterBubble();
  else
    message_center_tray_->ShowMessageCenterBubble();
  return true;
}

void WebNotificationTray::BubbleViewDestroyed() {
  if (message_center_bubble())
    message_center_bubble()->bubble()->BubbleViewDestroyed();
}

void WebNotificationTray::OnMouseEnteredView() {}

void WebNotificationTray::OnMouseExitedView() {}

base::string16 WebNotificationTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

void WebNotificationTray::OnBeforeBubbleWidgetInit(
    views::Widget* anchor_widget,
    views::Widget* bubble_widget,
    views::Widget::InitParams* params) const {
  // Place the bubble in the same root window as |anchor_widget|.
  WmLookup::Get()
      ->GetWindowForWidget(anchor_widget)
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          bubble_widget, kShellWindowId_SettingBubbleContainer, params);
}

void WebNotificationTray::HideBubble(const views::TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

bool WebNotificationTray::ShowNotifierSettings() {
  if (message_center_bubble()) {
    static_cast<message_center::MessageCenterBubble*>(
        message_center_bubble()->bubble())
        ->SetSettingsVisible();
    return true;
  }
  return ShowMessageCenterInternal(true /* show_settings */);
}

bool WebNotificationTray::IsContextMenuEnabled() const {
  return IsLoggedIn();
}

message_center::MessageCenterTray* WebNotificationTray::GetMessageCenterTray() {
  return message_center_tray_.get();
}

bool WebNotificationTray::IsCommandIdChecked(int command_id) const {
  if (command_id != kToggleQuietMode)
    return false;
  return message_center()->IsQuietMode();
}

bool WebNotificationTray::IsCommandIdEnabled(int command_id) const {
  return true;
}

void WebNotificationTray::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == kToggleQuietMode) {
    bool in_quiet_mode = message_center()->IsQuietMode();
    message_center()->SetQuietMode(!in_quiet_mode);
    return;
  }
  base::TimeDelta expires_in = command_id == kEnableQuietModeDay
                                   ? base::TimeDelta::FromDays(1)
                                   : base::TimeDelta::FromHours(1);
  message_center()->EnterQuietModeWithExpire(expires_in);
}

void WebNotificationTray::OnMessageCenterTrayChanged() {
  // Do not update the tray contents directly. Multiple change events can happen
  // consecutively, and calling Update in the middle of those events will show
  // intermediate unread counts for a moment.
  should_update_tray_content_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&WebNotificationTray::UpdateTrayContent, AsWeakPtr()));
}

void WebNotificationTray::UpdateTrayContent() {
  if (!should_update_tray_content_)
    return;
  should_update_tray_content_ = false;

  std::unordered_set<std::string> notification_ids;
  for (auto pair : visible_small_icons_)
    notification_ids.insert(pair.first);

  // Add small icons (up to kMaximumSmallIconCount = 3).
  message_center::MessageCenter* message_center =
      message_center_tray_->message_center();
  size_t visible_small_icon_count = 0;
  for (const auto* notification : message_center->GetVisibleNotifications()) {
    gfx::Image image = notification->small_image();
    if (image.IsEmpty())
      continue;

    if (visible_small_icon_count >= kMaximumSmallIconCount)
      break;
    visible_small_icon_count++;

    notification_ids.erase(notification->id());
    if (visible_small_icons_.count(notification->id()) != 0)
      continue;

    auto* item =
        new WebNotificationImage(image.AsImageSkia(), kTrayItemInnerIconSize,
                                 animation_container_.get(), this);
    visible_small_icons_.insert(std::make_pair(notification->id(), item));

    tray_container()->AddChildViewAt(item, 0);
    item->SetVisible(true);
  }

  // Remove unnecessary icons.
  for (const std::string& id : notification_ids) {
    WebNotificationImage* item = visible_small_icons_[id];
    visible_small_icons_.erase(id);
    item->HideAndDelete();
  }

  // Show or hide the bell icon.
  size_t visible_notification_count = message_center->NotificationCount();
  bell_icon_->SetVisible(visible_notification_count == 0);

  // Show or hide the counter.
  size_t hidden_icon_count =
      visible_notification_count - visible_small_icon_count;
  if (hidden_icon_count != 0) {
    counter_->SetVisible(true);
    counter_->SetNotificationCount(
        (visible_small_icon_count != 0),  // small_icons_exist
        hidden_icon_count);
  } else {
    counter_->SetVisible(false);
  }

  SetVisible(IsLoggedIn() && ShouldShowMessageCenter());
  PreferredSizeChanged();
  Layout();
  SchedulePaint();
  if (IsLoggedIn())
    system_tray_->SetNextFocusableView(this);
}

void WebNotificationTray::ClickedOutsideBubble() {
  // Only hide the message center
  if (!message_center_bubble())
    return;

  message_center_tray_->HideMessageCenterBubble();
}

message_center::MessageCenter* WebNotificationTray::message_center() const {
  return message_center_tray_->message_center();
}

bool WebNotificationTray::IsLoggedIn() const {
  WmShell* shell = WmShell::Get();
  return shell->system_tray_delegate()->GetUserLoginStatus() !=
             LoginStatus::NOT_LOGGED_IN &&
         !shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen();
}

// Methods for testing

bool WebNotificationTray::IsPopupVisible() const {
  return message_center_tray_->popups_visible();
}

message_center::MessageCenterBubble*
WebNotificationTray::GetMessageCenterBubbleForTest() {
  if (!message_center_bubble())
    return nullptr;
  return static_cast<message_center::MessageCenterBubble*>(
      message_center_bubble()->bubble());
}

}  // namespace ash
