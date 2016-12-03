// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray.h"

#include "ash/common/ash_switches.h"
#include "ash/common/key_event_watcher.h"
#include "ash/common/login_status.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/cast/tray_cast.h"
#include "ash/common/system/date/tray_date.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_bubble_wrapper.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray_accessibility.h"
#include "ash/common/system/update/tray_update.h"
#include "ash/common/system/user/tray_user.h"
#include "ash/common/system/user/tray_user_separator.h"
#include "ash/common/system/web_notification/web_notification_tray.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "grit/ash_strings.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/audio/tray_audio_chromeos.h"
#include "ash/common/system/chromeos/bluetooth/tray_bluetooth.h"
#include "ash/common/system/chromeos/brightness/tray_brightness.h"
#include "ash/common/system/chromeos/enterprise/tray_enterprise.h"
#include "ash/common/system/chromeos/media_security/multi_profile_media_tray_item.h"
#include "ash/common/system/chromeos/network/tray_network.h"
#include "ash/common/system/chromeos/network/tray_sms.h"
#include "ash/common/system/chromeos/network/tray_vpn.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/power/tray_power.h"
#include "ash/common/system/chromeos/screen_security/screen_capture_tray_item.h"
#include "ash/common/system/chromeos/screen_security/screen_share_tray_item.h"
#include "ash/common/system/chromeos/session/tray_session_length_limit.h"
#include "ash/common/system/chromeos/settings/tray_settings.h"
#include "ash/common/system/chromeos/supervised/tray_supervised_user.h"
#include "ash/common/system/chromeos/tray_caps_lock.h"
#include "ash/common/system/chromeos/tray_tracing.h"
#include "ash/common/system/ime/tray_ime_chromeos.h"
#include "ui/message_center/message_center.h"
#endif

using views::TrayBubbleView;

namespace ash {

// The minimum width of the system tray menu width.
const int kMinimumSystemTrayMenuWidth = 300;

// Class to initialize and manage the SystemTrayBubble and TrayBubbleWrapper
// instances for a bubble.

class SystemBubbleWrapper {
 public:
  // Takes ownership of |bubble|.
  explicit SystemBubbleWrapper(SystemTrayBubble* bubble)
      : bubble_(bubble), is_persistent_(false) {}

  // Initializes the bubble view and creates |bubble_wrapper_|.
  void InitView(TrayBackgroundView* tray,
                views::View* anchor,
                TrayBubbleView::InitParams* init_params,
                bool is_persistent) {
    DCHECK(anchor);
    LoginStatus login_status =
        WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();
    bubble_->InitView(anchor, login_status, init_params);
    bubble_wrapper_.reset(new TrayBubbleWrapper(tray, bubble_->bubble_view()));
    // The system bubble should not have an arrow.
    bubble_->bubble_view()->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
    is_persistent_ = is_persistent;

    // If ChromeVox is enabled, focus the default item if no item is focused and
    // there isn't a delayed close.
    if (WmShell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled() &&
        !is_persistent) {
      bubble_->FocusDefaultIfNeeded();
    }
  }

  // Convenience accessors:
  SystemTrayBubble* bubble() const { return bubble_.get(); }
  SystemTrayBubble::BubbleType bubble_type() const {
    return bubble_->bubble_type();
  }
  TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }
  bool is_persistent() const { return is_persistent_; }

 private:
  std::unique_ptr<SystemTrayBubble> bubble_;
  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;
  bool is_persistent_;

  DISALLOW_COPY_AND_ASSIGN(SystemBubbleWrapper);
};

// An activation observer to close the bubble if the window other
// than system bubble nor popup notification is activated.
class SystemTray::ActivationObserver : public WmActivationObserver {
 public:
  explicit ActivationObserver(SystemTray* tray) : tray_(tray) {
    DCHECK(tray_);
    WmShell::Get()->AddActivationObserver(this);
  }

  ~ActivationObserver() override {
    WmShell::Get()->RemoveActivationObserver(this);
  }

  // WmActivationObserver:
  void OnWindowActivated(WmWindow* gained_active,
                         WmWindow* lost_active) override {
    if (!tray_->HasSystemBubble() || !gained_active)
      return;

    int container_id =
        wm::GetContainerForWindow(gained_active)->GetShellWindowId();

    // Don't close the bubble if a popup notification is activated.
    if (container_id == kShellWindowId_StatusContainer)
      return;

    if (tray_->GetSystemBubble()->bubble_view()->GetWidget() !=
        gained_active->GetInternalWidget()) {
      tray_->CloseSystemBubble();
    }
  }
  void OnAttemptToReactivateWindow(WmWindow* request_active,
                                   WmWindow* actual_active) override {}

 private:
  SystemTray* tray_;

  DISALLOW_COPY_AND_ASSIGN(ActivationObserver);
};

// SystemTray

SystemTray::SystemTray(WmShelf* wm_shelf)
    : TrayBackgroundView(wm_shelf),
      web_notification_tray_(nullptr),
      detailed_item_(nullptr),
      default_bubble_height_(0),
      hide_notifications_(false),
      full_system_tray_menu_(false),
      tray_accessibility_(nullptr),
      tray_audio_(nullptr),
      tray_cast_(nullptr),
      tray_date_(nullptr),
      tray_update_(nullptr),
      screen_capture_tray_item_(nullptr),
      screen_share_tray_item_(nullptr) {
  SetContentsBackground();
}

SystemTray::~SystemTray() {
  // Destroy any child views that might have back pointers before ~View().
  activation_observer_.reset();
  key_event_watcher_.reset();
  system_bubble_.reset();
  notification_bubble_.reset();
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end(); ++it) {
    (*it)->DestroyTrayView();
  }
}

void SystemTray::InitializeTrayItems(
    SystemTrayDelegate* delegate,
    WebNotificationTray* web_notification_tray) {
  DCHECK(web_notification_tray);
  web_notification_tray_ = web_notification_tray;
  TrayBackgroundView::Initialize();
  CreateItems(delegate);
}

void SystemTray::Shutdown() {
  DCHECK(web_notification_tray_);
  web_notification_tray_ = nullptr;
}

void SystemTray::CreateItems(SystemTrayDelegate* delegate) {
  WmShell* wm_shell = WmShell::Get();
#if !defined(OS_WIN)
  // Create user items for each possible user.
  int maximum_user_profiles =
      wm_shell->GetSessionStateDelegate()->GetMaximumNumberOfLoggedInUsers();
  for (int i = 0; i < maximum_user_profiles; i++)
    AddTrayItem(new TrayUser(this, i));

  if (maximum_user_profiles > 1) {
    // Add a special double line separator between users and the rest of the
    // menu if more than one user is logged in.
    AddTrayItem(new TrayUserSeparator(this));
  }
#endif

  tray_accessibility_ = new TrayAccessibility(this);
  tray_date_ = new TrayDate(this);
  tray_update_ = new TrayUpdate(this);

#if defined(OS_CHROMEOS)
  AddTrayItem(new TraySessionLengthLimit(this));
  AddTrayItem(new TrayEnterprise(this));
  AddTrayItem(new TraySupervisedUser(this));
  AddTrayItem(new TrayIME(this));
  AddTrayItem(tray_accessibility_);
  AddTrayItem(new TrayTracing(this));
  AddTrayItem(new TrayPower(this, message_center::MessageCenter::Get()));
  AddTrayItem(new TrayNetwork(this));
  AddTrayItem(new TrayVPN(this));
  AddTrayItem(new TraySms(this));
  AddTrayItem(new TrayBluetooth(this));
  tray_cast_ = new TrayCast(this);
  AddTrayItem(tray_cast_);
  // TODO(jamescook): Remove this when mus has support for display management
  // and we have a DisplayManager equivalent. See http://crbug.com/548429
  std::unique_ptr<SystemTrayItem> tray_display =
      delegate->CreateDisplayTrayItem(this);
  if (tray_display)
    AddTrayItem(tray_display.release());
  screen_capture_tray_item_ = new ScreenCaptureTrayItem(this);
  AddTrayItem(screen_capture_tray_item_);
  screen_share_tray_item_ = new ScreenShareTrayItem(this);
  AddTrayItem(screen_share_tray_item_);
  AddTrayItem(new MultiProfileMediaTrayItem(this));
  tray_audio_ = new TrayAudioChromeOs(this);
  AddTrayItem(tray_audio_);
  AddTrayItem(new TrayBrightness(this));
  AddTrayItem(new TrayCapsLock(this));
  // TODO(jamescook): Remove this when mus has support for display management
  // and we have a DisplayManager equivalent. See http://crbug.com/548429
  std::unique_ptr<SystemTrayItem> tray_rotation_lock =
      delegate->CreateRotationLockTrayItem(this);
  if (tray_rotation_lock)
    AddTrayItem(tray_rotation_lock.release());
  AddTrayItem(new TraySettings(this));
  AddTrayItem(tray_update_);
  AddTrayItem(tray_date_);
#elif defined(OS_WIN)
  AddTrayItem(tray_accessibility_);
  AddTrayItem(tray_update_);
  AddTrayItem(tray_date_);
#endif

  SetVisible(wm_shell->system_tray_delegate()->GetTrayVisibilityOnStartup());
}

void SystemTray::AddTrayItem(SystemTrayItem* item) {
  items_.push_back(item);

  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
  views::View* tray_item = item->CreateTrayView(delegate->GetUserLoginStatus());
  item->UpdateAfterShelfAlignmentChange(shelf_alignment());

  if (tray_item) {
    tray_container()->AddChildViewAt(tray_item, 0);
    PreferredSizeChanged();
    tray_item_map_[item] = tray_item;
  }
}

const std::vector<SystemTrayItem*>& SystemTray::GetTrayItems() const {
  return items_.get();
}

void SystemTray::ShowDefaultView(BubbleCreationType creation_type) {
  ShowDefaultViewWithOffset(
      creation_type, TrayBubbleView::InitParams::kArrowDefaultOffset, false);
}

void SystemTray::ShowPersistentDefaultView() {
  ShowItems(items_.get(), false, false, BUBBLE_CREATE_NEW,
            TrayBubbleView::InitParams::kArrowDefaultOffset, true);
}

void SystemTray::ShowDetailedView(SystemTrayItem* item,
                                  int close_delay,
                                  bool activate,
                                  BubbleCreationType creation_type) {
  std::vector<SystemTrayItem*> items;
  // The detailed view with timeout means a UI to show the current system state,
  // like the audio level or brightness. Such UI should behave as persistent and
  // keep its own logic for the appearance.
  bool persistent =
      (!activate && close_delay > 0 && creation_type == BUBBLE_CREATE_NEW);
  items.push_back(item);
  ShowItems(items, true, activate, creation_type, GetTrayXOffset(item),
            persistent);
  if (system_bubble_)
    system_bubble_->bubble()->StartAutoCloseTimer(close_delay);
}

void SystemTray::SetDetailedViewCloseDelay(int close_delay) {
  if (HasSystemBubbleType(SystemTrayBubble::BUBBLE_TYPE_DETAILED))
    system_bubble_->bubble()->StartAutoCloseTimer(close_delay);
}

void SystemTray::HideDetailedView(SystemTrayItem* item, bool animate) {
  if (item != detailed_item_)
    return;

  if (!animate) {
    // In unittest, GetSystemBubble might return nullptr.
    if (GetSystemBubble()) {
      GetSystemBubble()
          ->bubble_view()
          ->GetWidget()
          ->SetVisibilityAnimationTransition(
              views::Widget::VisibilityTransition::ANIMATE_NONE);
    }
  }

  DestroySystemBubble();
  UpdateNotificationBubble();
}

void SystemTray::ShowNotificationView(SystemTrayItem* item) {
  if (std::find(notification_items_.begin(), notification_items_.end(), item) !=
      notification_items_.end())
    return;
  notification_items_.push_back(item);
  UpdateNotificationBubble();
}

void SystemTray::HideNotificationView(SystemTrayItem* item) {
  std::vector<SystemTrayItem*>::iterator found_iter =
      std::find(notification_items_.begin(), notification_items_.end(), item);
  if (found_iter == notification_items_.end())
    return;
  notification_items_.erase(found_iter);
  // Only update the notification bubble if visible (i.e. don't create one).
  if (notification_bubble_)
    UpdateNotificationBubble();
}

void SystemTray::UpdateAfterLoginStatusChange(LoginStatus login_status) {
  DestroySystemBubble();
  UpdateNotificationBubble();

  for (SystemTrayItem* item : items_)
    item->UpdateAfterLoginStatusChange(login_status);

  // Items default to SHELF_ALIGNMENT_BOTTOM. Update them if the initial
  // position of the shelf differs.
  if (!IsHorizontalAlignment(shelf_alignment()))
    UpdateAfterShelfAlignmentChange(shelf_alignment());

  SetVisible(true);
  PreferredSizeChanged();
}

void SystemTray::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  for (SystemTrayItem* item : items_)
    item->UpdateAfterShelfAlignmentChange(alignment);
}

void SystemTray::SetHideNotifications(bool hide_notifications) {
  if (notification_bubble_)
    notification_bubble_->bubble()->SetVisible(!hide_notifications);
  hide_notifications_ = hide_notifications;
}

bool SystemTray::ShouldShowShelf() const {
  return system_bubble_.get() && system_bubble_->bubble()->ShouldShowShelf();
}

bool SystemTray::HasSystemBubble() const {
  return system_bubble_.get() != NULL;
}

bool SystemTray::HasNotificationBubble() const {
  return notification_bubble_.get() != NULL;
}

SystemTrayBubble* SystemTray::GetSystemBubble() {
  if (!system_bubble_)
    return NULL;
  return system_bubble_->bubble();
}

bool SystemTray::IsAnyBubbleVisible() const {
  return ((system_bubble_.get() && system_bubble_->bubble()->IsVisible()) ||
          (notification_bubble_.get() &&
           notification_bubble_->bubble()->IsVisible()));
}

bool SystemTray::IsMouseInNotificationBubble() const {
  if (!notification_bubble_)
    return false;
  return notification_bubble_->bubble_view()->GetBoundsInScreen().Contains(
      display::Screen::GetScreen()->GetCursorScreenPoint());
}

bool SystemTray::CloseSystemBubble() const {
  if (!system_bubble_)
    return false;
  system_bubble_->bubble()->Close();
  return true;
}

views::View* SystemTray::GetHelpButtonView() const {
  return tray_date_->GetHelpButtonView();
}

TrayAudio* SystemTray::GetTrayAudio() const {
  return tray_audio_;
}

bool SystemTray::CloseNotificationBubbleForTest() const {
  if (!notification_bubble_)
    return false;
  notification_bubble_->bubble()->Close();
  return true;
}

// Private methods.

bool SystemTray::HasSystemBubbleType(SystemTrayBubble::BubbleType type) {
  DCHECK(type != SystemTrayBubble::BUBBLE_TYPE_NOTIFICATION);
  return system_bubble_.get() && system_bubble_->bubble_type() == type;
}

void SystemTray::DestroySystemBubble() {
  CloseSystemBubbleAndDeactivateSystemTray();
  detailed_item_ = NULL;
  UpdateWebNotifications();
}

void SystemTray::DestroyNotificationBubble() {
  if (notification_bubble_) {
    notification_bubble_.reset();
    UpdateWebNotifications();
  }
}

base::string16 SystemTray::GetAccessibleNameForTray() {
  base::string16 time = GetAccessibleTimeString(base::Time::Now());
  base::string16 battery = base::ASCIIToUTF16("");
#if defined(OS_CHROMEOS)
  battery = PowerStatus::Get()->GetAccessibleNameString(false);
#endif
  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_ACCESSIBLE_DESCRIPTION,
                                    time, battery);
}

int SystemTray::GetTrayXOffset(SystemTrayItem* item) const {
  // Don't attempt to align the arrow if the shelf is on the left or right.
  if (!IsHorizontalAlignment(shelf_alignment()))
    return TrayBubbleView::InitParams::kArrowDefaultOffset;

  std::map<SystemTrayItem*, views::View*>::const_iterator it =
      tray_item_map_.find(item);
  if (it == tray_item_map_.end())
    return TrayBubbleView::InitParams::kArrowDefaultOffset;

  const views::View* item_view = it->second;
  if (item_view->bounds().IsEmpty()) {
    // The bounds of item could be still empty if it does not have a visible
    // tray view. In that case, use the default (minimum) offset.
    return TrayBubbleView::InitParams::kArrowDefaultOffset;
  }

  gfx::Point point(item_view->width() / 2, 0);
  ConvertPointToWidget(item_view, &point);
  return point.x();
}

void SystemTray::ShowDefaultViewWithOffset(BubbleCreationType creation_type,
                                           int arrow_offset,
                                           bool persistent) {
  if (creation_type != BUBBLE_USE_EXISTING)
    WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_MENU_OPENED);
  ShowItems(items_.get(), false, true, creation_type, arrow_offset, persistent);
}

void SystemTray::ShowItems(const std::vector<SystemTrayItem*>& items,
                           bool detailed,
                           bool can_activate,
                           BubbleCreationType creation_type,
                           int arrow_offset,
                           bool persistent) {
  // No system tray bubbles in kiosk mode.
  if (WmShell::Get()->system_tray_delegate()->GetUserLoginStatus() ==
      LoginStatus::KIOSK_APP) {
    return;
  }

  // Destroy any existing bubble and create a new one.
  SystemTrayBubble::BubbleType bubble_type =
      detailed ? SystemTrayBubble::BUBBLE_TYPE_DETAILED
               : SystemTrayBubble::BUBBLE_TYPE_DEFAULT;

  // Destroy the notification bubble here so that it doesn't get rebuilt
  // while we add items to the main bubble_ (e.g. in HideNotificationView).
  notification_bubble_.reset();
  if (system_bubble_.get() && creation_type == BUBBLE_USE_EXISTING) {
    system_bubble_->bubble()->UpdateView(items, bubble_type);
    // If ChromeVox is enabled, focus the default item if no item is focused.
    if (WmShell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled())
      system_bubble_->bubble()->FocusDefaultIfNeeded();
  } else {
    // Cleanup the existing bubble before showing a new one. Otherwise, it's
    // possible to confuse the new system bubble with the old one during
    // destruction, leading to subtle errors/crashes such as crbug.com/545166.
    DestroySystemBubble();

    // Remember if the menu is a single property (like e.g. volume) or the
    // full tray menu. Note that in case of the |BUBBLE_USE_EXISTING| case
    // above, |full_system_tray_menu_| does not get changed since the fact that
    // the menu is full (or not) doesn't change even if a "single property"
    // (like network) replaces most of the menu.
    full_system_tray_menu_ = items.size() > 1;
    // The menu width is fixed, and it is a per language setting.
    int menu_width = std::max(
        kMinimumSystemTrayMenuWidth,
        WmShell::Get()->system_tray_delegate()->GetSystemTrayMenuWidth());

    TrayBubbleView::InitParams init_params(TrayBubbleView::ANCHOR_TYPE_TRAY,
                                           GetAnchorAlignment(), menu_width,
                                           kTrayPopupMaxWidth);
    // TODO(oshima): Change TrayBubbleView itself.
    init_params.can_activate = false;
    init_params.first_item_has_no_margin = true;
    if (detailed) {
      // This is the case where a volume control or brightness control bubble
      // is created.
      init_params.max_height = default_bubble_height_;
      init_params.arrow_color = kBackgroundColor;
    } else {
      init_params.arrow_color = kHeaderBackgroundColor;
    }
    init_params.arrow_offset = arrow_offset;
    if (bubble_type == SystemTrayBubble::BUBBLE_TYPE_DEFAULT)
      init_params.close_on_deactivate = !persistent;
    // For Volume and Brightness we don't want to show an arrow when
    // they are shown in a bubble by themselves.
    init_params.arrow_paint_type = views::BubbleBorder::PAINT_NORMAL;
    if (items.size() == 1 && items[0]->ShouldHideArrow())
      init_params.arrow_paint_type = views::BubbleBorder::PAINT_TRANSPARENT;
    SystemTrayBubble* bubble = new SystemTrayBubble(this, items, bubble_type);

    system_bubble_.reset(new SystemBubbleWrapper(bubble));
    system_bubble_->InitView(this, tray_container(), &init_params, persistent);

    activation_observer_.reset(persistent ? nullptr
                                          : new ActivationObserver(this));

    // Record metrics for the system menu when the default view is invoked.
    if (!detailed)
      RecordSystemMenuMetrics();
  }
  // Save height of default view for creating detailed views directly.
  if (!detailed)
    default_bubble_height_ = system_bubble_->bubble_view()->height();

  key_event_watcher_.reset();
  if (can_activate)
    CreateKeyEventWatcher();

  if (detailed && items.size() > 0)
    detailed_item_ = items[0];
  else
    detailed_item_ = NULL;

  UpdateNotificationBubble();  // State changed, re-create notifications.
  if (!notification_bubble_)
    UpdateWebNotifications();
  shelf()->UpdateAutoHideState();

  // When we show the system menu in our alternate shelf layout, we need to
  // tint the background.
  if (full_system_tray_menu_)
    SetDrawBackgroundAsActive(true);
}

void SystemTray::UpdateNotificationBubble() {
  // Only show the notification bubble if we have notifications.
  if (notification_items_.empty()) {
    DestroyNotificationBubble();
    return;
  }
  // Destroy the existing bubble before constructing a new one.
  notification_bubble_.reset();
  SystemTrayBubble* notification_bubble;
  notification_bubble = new SystemTrayBubble(
      this, notification_items_, SystemTrayBubble::BUBBLE_TYPE_NOTIFICATION);
  views::View* anchor;
  TrayBubbleView::AnchorType anchor_type;
  // Tray items might want to show notifications while we are creating and
  // initializing the |system_bubble_| - but it might not be fully initialized
  // when coming here - this would produce a crashed like crbug.com/247416.
  // As such we check the existence of the widget here.
  if (system_bubble_.get() && system_bubble_->bubble_view() &&
      system_bubble_->bubble_view()->GetWidget()) {
    anchor = system_bubble_->bubble_view();
    anchor_type = TrayBubbleView::ANCHOR_TYPE_BUBBLE;
  } else {
    anchor = tray_container();
    anchor_type = TrayBubbleView::ANCHOR_TYPE_TRAY;
  }
  TrayBubbleView::InitParams init_params(anchor_type, GetAnchorAlignment(),
                                         kTrayPopupMinWidth,
                                         kTrayPopupMaxWidth);
  init_params.first_item_has_no_margin = true;
  init_params.arrow_color = kBackgroundColor;
  init_params.arrow_offset = GetTrayXOffset(notification_items_[0]);
  notification_bubble_.reset(new SystemBubbleWrapper(notification_bubble));
  notification_bubble_->InitView(this, anchor, &init_params, false);

  if (notification_bubble->bubble_view()->child_count() == 0) {
    // It is possible that none of the items generated actual notifications.
    DestroyNotificationBubble();
    return;
  }
  if (hide_notifications_)
    notification_bubble->SetVisible(false);
  else
    UpdateWebNotifications();
}

void SystemTray::UpdateWebNotifications() {
  TrayBubbleView* bubble_view = NULL;
  if (notification_bubble_)
    bubble_view = notification_bubble_->bubble_view();
  else if (system_bubble_)
    bubble_view = system_bubble_->bubble_view();

  int height = 0;
  if (bubble_view) {
    gfx::Rect work_area =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(bubble_view->GetWidget()->GetNativeView())
            .work_area();
    height =
        std::max(0, work_area.height() - bubble_view->GetBoundsInScreen().y());
  }
  if (web_notification_tray_)
    web_notification_tray_->SetTrayBubbleHeight(height);
}

base::string16 SystemTray::GetAccessibleTimeString(
    const base::Time& now) const {
  base::HourClockType hour_type =
      WmShell::Get()->system_tray_delegate()->GetHourClockType();
  return base::TimeFormatTimeOfDayWithHourClockType(now, hour_type,
                                                    base::kKeepAmPm);
}

void SystemTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  TrayBackgroundView::SetShelfAlignment(alignment);
  UpdateAfterShelfAlignmentChange(alignment);
  // Destroy any existing bubble so that it is rebuilt correctly.
  CloseSystemBubbleAndDeactivateSystemTray();
  // Rebuild any notification bubble.
  if (notification_bubble_) {
    notification_bubble_.reset();
    UpdateNotificationBubble();
    // UpdateWebNotifications() should be called in UpdateNotificationBubble().
  } else if (!hide_notifications_) {
    UpdateWebNotifications();
  }
}

void SystemTray::AnchorUpdated() {
  if (notification_bubble_) {
    notification_bubble_->bubble_view()->UpdateBubble();
    // Ensure that the notification buble is above the shelf/status area.
    notification_bubble_->bubble_view()->GetWidget()->StackAtTop();
    UpdateBubbleViewArrow(notification_bubble_->bubble_view());
  }
  if (system_bubble_) {
    system_bubble_->bubble_view()->UpdateBubble();
    UpdateBubbleViewArrow(system_bubble_->bubble_view());
  }
}

void SystemTray::BubbleResized(const TrayBubbleView* bubble_view) {
  UpdateWebNotifications();
}

void SystemTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (system_bubble_.get() && bubble_view == system_bubble_->bubble_view()) {
    DestroySystemBubble();
    UpdateNotificationBubble();  // State changed, re-create notifications.
    shelf()->UpdateAutoHideState();
  } else if (notification_bubble_.get() &&
             bubble_view == notification_bubble_->bubble_view()) {
    DestroyNotificationBubble();
  }
}

void SystemTray::ClickedOutsideBubble() {
  if (!system_bubble_ || system_bubble_->is_persistent())
    return;
  HideBubbleWithView(system_bubble_->bubble_view());
}

void SystemTray::BubbleViewDestroyed() {
  if (system_bubble_) {
    system_bubble_->bubble()->DestroyItemViews();
    system_bubble_->bubble()->BubbleViewDestroyed();
  }
}

void SystemTray::OnMouseEnteredView() {
  if (system_bubble_)
    system_bubble_->bubble()->StopAutoCloseTimer();
}

void SystemTray::OnMouseExitedView() {
  if (system_bubble_)
    system_bubble_->bubble()->RestartAutoCloseTimer();
}

base::string16 SystemTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

gfx::Rect SystemTray::GetAnchorRect(
    views::Widget* anchor_widget,
    TrayBubbleView::AnchorType anchor_type,
    TrayBubbleView::AnchorAlignment anchor_alignment) const {
  return GetBubbleAnchorRect(anchor_widget, anchor_type, anchor_alignment);
}

void SystemTray::OnBeforeBubbleWidgetInit(
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

void SystemTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

views::View* SystemTray::GetTrayItemViewForTest(SystemTrayItem* item) {
  std::map<SystemTrayItem*, views::View*>::iterator it =
      tray_item_map_.find(item);
  return it == tray_item_map_.end() ? NULL : it->second;
}

TrayCast* SystemTray::GetTrayCastForTesting() const {
  return tray_cast_;
}

TrayDate* SystemTray::GetTrayDateForTesting() const {
  return tray_date_;
}

TrayUpdate* SystemTray::GetTrayUpdateForTesting() const {
  return tray_update_;
}

void SystemTray::CloseBubble(const ui::KeyEvent& key_event) {
  CloseSystemBubble();
}

void SystemTray::ActivateAndStartNavigation(const ui::KeyEvent& key_event) {
  if (!system_bubble_)
    return;
  ActivateBubble();
  if (!system_bubble_)
    return;
  views::Widget* widget = GetSystemBubble()->bubble_view()->GetWidget();
  widget->GetFocusManager()->OnKeyEvent(key_event);
}

void SystemTray::CreateKeyEventWatcher() {
  key_event_watcher_ = WmShell::Get()->CreateKeyEventWatcher();
  key_event_watcher_->AddKeyEventCallback(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE),
      base::Bind(&SystemTray::CloseBubble, base::Unretained(this)));
  key_event_watcher_->AddKeyEventCallback(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE),
      base::Bind(&SystemTray::ActivateAndStartNavigation,
                 base::Unretained(this)));
  key_event_watcher_->AddKeyEventCallback(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN),
      base::Bind(&SystemTray::ActivateAndStartNavigation,
                 base::Unretained(this)));
}

void SystemTray::ActivateBubble() {
  TrayBubbleView* bubble_view = GetSystemBubble()->bubble_view();
  bubble_view->set_can_activate(true);
  bubble_view->GetWidget()->Activate();
}

bool SystemTray::PerformAction(const ui::Event& event) {
  // If we're already showing the default view, hide it; otherwise, show it
  // (and hide any popup that's currently shown).
  if (HasSystemBubbleType(SystemTrayBubble::BUBBLE_TYPE_DEFAULT)) {
    system_bubble_->bubble()->Close();
  } else {
    int arrow_offset = TrayBubbleView::InitParams::kArrowDefaultOffset;
    if (event.IsMouseEvent() || event.type() == ui::ET_GESTURE_TAP) {
      const ui::LocatedEvent& located_event =
          static_cast<const ui::LocatedEvent&>(event);
      if (IsHorizontalAlignment(shelf_alignment())) {
        gfx::Point point(located_event.x(), 0);
        ConvertPointToWidget(this, &point);
        arrow_offset = point.x();
      }
    }
    ShowDefaultViewWithOffset(BUBBLE_CREATE_NEW, arrow_offset, false);
    if (event.IsKeyEvent() || (event.flags() & ui::EF_TOUCH_ACCESSIBILITY))
      ActivateBubble();
  }
  return true;
}

void SystemTray::CloseSystemBubbleAndDeactivateSystemTray() {
  activation_observer_.reset();
  key_event_watcher_.reset();
  system_bubble_.reset();
  // When closing a system bubble with the alternate shelf layout, we need to
  // turn off the active tinting of the shelf.
  if (full_system_tray_menu_) {
    SetDrawBackgroundAsActive(false);
    full_system_tray_menu_ = false;
  }
}

void SystemTray::RecordSystemMenuMetrics() {
  DCHECK(system_bubble_);

  system_bubble_->bubble()->RecordVisibleRowMetrics();

  TrayBubbleView* bubble_view = system_bubble_->bubble_view();
  int num_rows = 0;
  for (int i = 0; i < bubble_view->child_count(); i++) {
    // Certain menu rows are attached by default but can set themselves as
    // invisible (IME is one such example). Count only user-visible rows.
    if (bubble_view->child_at(i)->visible())
      num_rows++;
  }
  UMA_HISTOGRAM_COUNTS_100("Ash.SystemMenu.Rows", num_rows);

  int work_area_height =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(bubble_view->GetWidget()->GetNativeView())
          .work_area()
          .height();
  if (work_area_height > 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Ash.SystemMenu.PercentageOfWorkAreaHeightCoveredByMenu",
        100 * bubble_view->height() / work_area_height, 1, 300, 100);
  }
}

}  // namespace ash
