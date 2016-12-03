// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_item_view.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/download/download_feedback_dialog_view.h"
#include "chrome/browser/ui/views/download/download_shelf_context_menu_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_danger_type.h"
#include "grit/theme_resources.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using content::DownloadItem;
using extensions::ExperienceSamplingEvent;

namespace {

// TODO(paulg): These may need to be adjusted when download progress
//              animation is added, and also possibly to take into account
//              different screen resolutions.
const int kTextWidth = 140;            // Pixels
const int kDangerousTextWidth = 200;   // Pixels
const int kVerticalPadding = 3;        // Pixels
const int kVerticalTextPadding = 2;    // Pixels
const int kTooltipMaxWidth = 800;      // Pixels

// Padding around progress indicator, on all sides.
const int kProgressPadding = 7;

// We add some padding before the left image so that the progress animation icon
// hides the corners of the left image.
const int kLeftPadding = 0;  // Pixels.

// The space between the Save and Discard buttons when prompting for a dangerous
// download.
const int kButtonPadding = 5;  // Pixels.

// The space on the left and right side of the dangerous download label.
const int kLabelPadding = 4;  // Pixels.

const SkColor kFileNameDisabledColor = SkColorSetRGB(171, 192, 212);

// How long the 'download complete' animation should last for.
const int kCompleteAnimationDurationMs = 2500;

// How long the 'download interrupted' animation should last for.
const int kInterruptedAnimationDurationMs = 2500;

// How long we keep the item disabled after the user clicked it to open the
// downloaded item.
const int kDisabledOnOpenDuration = 3000;

}  // namespace

DownloadItemView::DownloadItemView(DownloadItem* download_item,
    DownloadShelfView* parent)
  : warning_icon_(NULL),
    shelf_(parent),
    status_text_(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING)),
    body_state_(NORMAL),
    drop_down_state_(NORMAL),
    mode_(NORMAL_MODE),
    drop_down_pressed_(false),
    dragging_(false),
    starting_drag_(false),
    model_(download_item),
    save_button_(NULL),
    discard_button_(NULL),
    dangerous_download_label_(NULL),
    dangerous_download_label_sized_(false),
    disabled_while_opening_(false),
    creation_time_(base::Time::Now()),
    time_download_warning_shown_(base::Time()),
    weak_ptr_factory_(this) {
  DCHECK(download());
  download()->AddObserver(this);
  set_context_menu_controller(this);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  BodyImageSet normal_body_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM)
  };
  normal_body_image_set_ = normal_body_image_set;

  DropDownImageSet normal_drop_down_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM)
  };
  normal_drop_down_image_set_ = normal_drop_down_image_set;

  BodyImageSet hot_body_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_H)
  };
  hot_body_image_set_ = hot_body_image_set;

  DropDownImageSet hot_drop_down_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_H),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_H)
  };
  hot_drop_down_image_set_ = hot_drop_down_image_set;

  BodyImageSet pushed_body_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_P)
  };
  pushed_body_image_set_ = pushed_body_image_set;

  DropDownImageSet pushed_drop_down_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_TOP_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_MIDDLE_P),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_MENU_BOTTOM_P)
  };
  pushed_drop_down_image_set_ = pushed_drop_down_image_set;

  BodyImageSet dangerous_mode_body_image_set = {
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_LEFT_BOTTOM),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_TOP),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_MIDDLE),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_CENTER_BOTTOM),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_TOP_NO_DD),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_MIDDLE_NO_DD),
    rb.GetImageSkiaNamed(IDR_DOWNLOAD_BUTTON_RIGHT_BOTTOM_NO_DD)
  };
  dangerous_mode_body_image_set_ = dangerous_mode_body_image_set;

  malicious_mode_body_image_set_ = normal_body_image_set;

  LoadIcon();

  font_list_ = rb.GetFontList(ui::ResourceBundle::BaseFont);
  box_height_ = std::max<int>(2 * kVerticalPadding + font_list_.GetHeight() +
                                  kVerticalTextPadding + font_list_.GetHeight(),
                              2 * kVerticalPadding +
                                  normal_body_image_set_.top_left->height() +
                                  normal_body_image_set_.bottom_left->height());

  box_y_ = std::max(0, (2 * kProgressPadding +
                        DownloadShelf::kProgressIndicatorSize - box_height_) /
                           2);

  body_hover_animation_.reset(new gfx::SlideAnimation(this));
  drop_hover_animation_.reset(new gfx::SlideAnimation(this));

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  OnDownloadUpdated(download());
  UpdateDropDownButtonPosition();
}

DownloadItemView::~DownloadItemView() {
  StopDownloadProgress();
  download()->RemoveObserver(this);

  // ExperienceSampling: If the user took no action to remove the warning
  // before it disappeared, then the user effectively dismissed the download
  // without keeping it.
  if (sampling_event_.get())
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kIgnore);
}

// Progress animation handlers.

void DownloadItemView::StartDownloadProgress() {
  if (progress_timer_.IsRunning())
    return;
  progress_start_time_ = base::TimeTicks::Now();
  progress_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(
                                       DownloadShelf::kProgressRateMs),
                        base::Bind(&DownloadItemView::ProgressTimerFired,
                                   base::Unretained(this)));
}

void DownloadItemView::StopDownloadProgress() {
  if (!progress_timer_.IsRunning())
    return;
  previous_progress_elapsed_ += base::TimeTicks::Now() - progress_start_time_;
  progress_start_time_ = base::TimeTicks();
  progress_timer_.Stop();
}

void DownloadItemView::OnExtractIconComplete(gfx::Image* icon_bitmap) {
  if (icon_bitmap)
    shelf_->SchedulePaint();
}

// DownloadObserver interface.

// Update the progress graphic on the icon and our text status label
// to reflect our current bytes downloaded, time remaining.
void DownloadItemView::OnDownloadUpdated(DownloadItem* download_item) {
  DCHECK_EQ(download(), download_item);

  if (!model_.ShouldShowInShelf()) {
    shelf_->RemoveDownloadView(this);  // This will delete us!
    return;
  }

  if (IsShowingWarningDialog() != model_.IsDangerous()) {
    ToggleWarningDialog();
  } else {
    switch (download()->GetState()) {
      case DownloadItem::IN_PROGRESS:
        download()->IsPaused() ? StopDownloadProgress()
                               : StartDownloadProgress();
        LoadIconIfItemPathChanged();
        break;
      case DownloadItem::INTERRUPTED:
        StopDownloadProgress();
        complete_animation_.reset(new gfx::SlideAnimation(this));
        complete_animation_->SetSlideDuration(kInterruptedAnimationDurationMs);
        complete_animation_->SetTweenType(gfx::Tween::LINEAR);
        complete_animation_->Show();
        LoadIcon();
        break;
      case DownloadItem::COMPLETE:
        if (model_.ShouldRemoveFromShelfWhenComplete()) {
          shelf_->RemoveDownloadView(this);  // This will delete us!
          return;
        }
        StopDownloadProgress();
        complete_animation_.reset(new gfx::SlideAnimation(this));
        complete_animation_->SetSlideDuration(kCompleteAnimationDurationMs);
        complete_animation_->SetTweenType(gfx::Tween::LINEAR);
        complete_animation_->Show();
        LoadIcon();
        break;
      case DownloadItem::CANCELLED:
        StopDownloadProgress();
        if (complete_animation_)
          complete_animation_->Stop();
        LoadIcon();
        break;
      default:
        NOTREACHED();
    }
    status_text_ = model_.GetStatusText();
    SchedulePaint();
  }

  base::string16 new_tip = model_.GetTooltipText(font_list_, kTooltipMaxWidth);
  if (new_tip != tooltip_text_) {
    tooltip_text_ = new_tip;
    TooltipTextChanged();
  }

  UpdateAccessibleName();
}

void DownloadItemView::OnDownloadDestroyed(DownloadItem* download) {
  shelf_->RemoveDownloadView(this);  // This will delete us!
}

void DownloadItemView::OnDownloadOpened(DownloadItem* download) {
  disabled_while_opening_ = true;
  SetEnabled(false);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DownloadItemView::Reenable, weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDisabledOnOpenDuration));

  // Notify our parent.
  shelf_->OpenedDownload();
}

// View overrides

// In dangerous mode we have to layout our buttons.
void DownloadItemView::Layout() {
  if (IsShowingWarningDialog()) {
    BodyImageSet* body_image_set =
        (mode_ == DANGEROUS_MODE) ? &dangerous_mode_body_image_set_ :
            &malicious_mode_body_image_set_;
    int x = kLeftPadding + body_image_set->top_left->width() +
        warning_icon_->width() + kLabelPadding;
    int y = (height() - dangerous_download_label_->height()) / 2;
    dangerous_download_label_->SetBounds(x, y,
                                         dangerous_download_label_->width(),
                                         dangerous_download_label_->height());
    gfx::Size button_size = GetButtonSize();
    x += dangerous_download_label_->width() + kLabelPadding;
    y = (height() - button_size.height()) / 2;
    if (save_button_) {
      save_button_->SetBounds(x, y, button_size.width(), button_size.height());
      x += button_size.width() + kButtonPadding;
    }
    discard_button_->SetBounds(x, y, button_size.width(), button_size.height());
    UpdateColorsFromTheme();
  }
}

gfx::Size DownloadItemView::GetPreferredSize() const {
  int width, height;

  // First, we set the height to the height of two rows or text plus margins.
  height = 2 * kVerticalPadding + 2 * font_list_.GetHeight() +
      kVerticalTextPadding;
  // Then we increase the size if the progress icon doesn't fit.
  height = std::max<int>(
      height, DownloadShelf::kProgressIndicatorSize + 2 * kProgressPadding);

  if (IsShowingWarningDialog()) {
    const BodyImageSet* body_image_set =
        (mode_ == DANGEROUS_MODE) ? &dangerous_mode_body_image_set_ :
            &malicious_mode_body_image_set_;
    width = kLeftPadding + body_image_set->top_left->width();
    width += warning_icon_->width() + kLabelPadding;
    width += dangerous_download_label_->width() + kLabelPadding;
    gfx::Size button_size = GetButtonSize();
    // Make sure the button fits.
    height = std::max<int>(height, 2 * kVerticalPadding + button_size.height());
    // Then we make sure the warning icon fits.
    height = std::max<int>(height, 2 * kVerticalPadding +
                                   warning_icon_->height());
    if (save_button_)
      width += button_size.width() + kButtonPadding;
    width += button_size.width();
    width += body_image_set->top_right->width();
    if (mode_ == MALICIOUS_MODE)
      width += normal_drop_down_image_set_.top->width();
  } else {
    width = kLeftPadding + normal_body_image_set_.top_left->width();
    width += DownloadShelf::kProgressIndicatorSize + 2 * kProgressPadding;
    width += kTextWidth;
    width += normal_body_image_set_.top_right->width();
    width += normal_drop_down_image_set_.top->width();
  }
  return gfx::Size(width, height);
}

// Handle a mouse click and open the context menu if the mouse is
// over the drop-down region.
bool DownloadItemView::OnMousePressed(const ui::MouseEvent& event) {
  HandlePressEvent(event, event.IsOnlyLeftMouseButton());
  return true;
}

// Handle drag (file copy) operations.
bool DownloadItemView::OnMouseDragged(const ui::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (IsShowingWarningDialog())
    return true;

  if (!starting_drag_) {
    starting_drag_ = true;
    drag_start_point_ = event.location();
  }
  if (dragging_) {
    if (download()->GetState() == DownloadItem::COMPLETE) {
      IconManager* im = g_browser_process->icon_manager();
      gfx::Image* icon = im->LookupIconFromFilepath(
          download()->GetTargetFilePath(), IconLoader::SMALL);
      views::Widget* widget = GetWidget();
      DragDownloadItem(
          download(), icon, widget ? widget->GetNativeView() : NULL);
    }
  } else if (ExceededDragThreshold(event.location() - drag_start_point_)) {
    dragging_ = true;
  }
  return true;
}

void DownloadItemView::OnMouseReleased(const ui::MouseEvent& event) {
  HandleClickEvent(event, event.IsOnlyLeftMouseButton());
}

void DownloadItemView::OnMouseCaptureLost() {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  if (dragging_) {
    // Starting a drag results in a MouseCaptureLost.
    dragging_ = false;
    starting_drag_ = false;
  }
  SetState(NORMAL, NORMAL);
}

void DownloadItemView::OnMouseMoved(const ui::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  bool on_body = !InDropDownButtonXCoordinateRange(event.x());
  SetState(on_body ? HOT : NORMAL, on_body ? NORMAL : HOT);
}

void DownloadItemView::OnMouseExited(const ui::MouseEvent& event) {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  SetState(NORMAL, drop_down_pressed_ ? PUSHED : NORMAL);
}

bool DownloadItemView::OnKeyPressed(const ui::KeyEvent& event) {
  // Key press should not activate us in dangerous mode.
  if (IsShowingWarningDialog())
    return true;

  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    // OpenDownload may delete this, so don't add any code after this line.
    OpenDownload();
    return true;
  }
  return false;
}

bool DownloadItemView::GetTooltipText(const gfx::Point& p,
                                      base::string16* tooltip) const {
  if (IsShowingWarningDialog()) {
    tooltip->clear();
    return false;
  }

  tooltip->assign(tooltip_text_);

  return true;
}

void DownloadItemView::GetAccessibleState(ui::AXViewState* state) {
  state->name = accessible_name_;
  state->role = ui::AX_ROLE_BUTTON;
  if (model_.IsDangerous())
    state->AddStateFlag(ui::AX_STATE_DISABLED);
  else
    state->AddStateFlag(ui::AX_STATE_HASPOPUP);
}

void DownloadItemView::OnThemeChanged() {
  UpdateColorsFromTheme();
}

void DownloadItemView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    HandlePressEvent(*event, true);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP) {
    HandleClickEvent(*event, true);
    event->SetHandled();
    return;
  }

  SetState(NORMAL, NORMAL);
  views::View::OnGestureEvent(event);
}

void DownloadItemView::ShowContextMenuForView(View* source,
                                              const gfx::Point& point,
                                              ui::MenuSourceType source_type) {
  // |point| is in screen coordinates. So convert it to local coordinates first.
  gfx::Point local_point = point;
  ConvertPointFromScreen(this, &local_point);
  ShowContextMenuImpl(local_point, source_type);
}

void DownloadItemView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  base::TimeDelta warning_duration;
  if (!time_download_warning_shown_.is_null())
    warning_duration = base::Time::Now() - time_download_warning_shown_;

  if (save_button_ && sender == save_button_) {
    // The user has confirmed a dangerous download.  We'd record how quickly the
    // user did this to detect whether we're being clickjacked.
    UMA_HISTOGRAM_LONG_TIMES("clickjacking.save_download", warning_duration);
    // ExperienceSampling: User chose to proceed with a dangerous download.
    if (sampling_event_.get()) {
      sampling_event_->CreateUserDecisionEvent(
          ExperienceSamplingEvent::kProceed);
      sampling_event_.reset(NULL);
    }
    // This will change the state and notify us.
    download()->ValidateDangerousDownload();
    return;
  }

  // WARNING: all end states after this point delete |this|.
  DCHECK_EQ(discard_button_, sender);
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.discard_download", warning_duration);
  if (!model_.IsMalicious() && model_.ShouldAllowDownloadFeedback() &&
      !shelf_->browser()->profile()->IsOffTheRecord()) {
    if (!shelf_->browser()->profile()->GetPrefs()->HasPrefPath(
        prefs::kSafeBrowsingExtendedReportingEnabled)) {
      // Show dialog, because the dialog hasn't been shown before.
      DownloadFeedbackDialogView::Show(
          shelf_->get_parent()->GetNativeWindow(),
          shelf_->browser()->profile(),
          shelf_->GetNavigator(),
          base::Bind(
              &DownloadItemView::PossiblySubmitDownloadToFeedbackService,
              weak_ptr_factory_.GetWeakPtr()));
    } else {
      PossiblySubmitDownloadToFeedbackService(
          shelf_->browser()->profile()->GetPrefs()->GetBoolean(
               prefs::kSafeBrowsingExtendedReportingEnabled));
    }
    return;
  }
  download()->Remove();
}

void DownloadItemView::AnimationProgressed(const gfx::Animation* animation) {
  // We don't care if what animation (body button/drop button/complete),
  // is calling back, as they all have to go through the same paint call.
  SchedulePaint();
}

void DownloadItemView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  if (HasFocus())
    canvas->DrawFocusRect(GetLocalBounds());
}

// The DownloadItemView can be in three major modes (NORMAL_MODE, DANGEROUS_MODE
// and MALICIOUS_MODE).
//
// NORMAL_MODE: We are displaying an in-progress or completed download.
// .-------------------------------+-.
// | [icon] Filename               |v|
// | [    ] Status                 | |
// `-------------------------------+-'
//  |  |                            \_ Drop down button. Invokes menu. Responds
//  |  |                               to mouse. (NORMAL, HOT or PUSHED).
//  |   \_ Icon is overlaid on top of in-progress animation.
//   \_ Both the body and the drop down button respond to mouse hover and can be
//      pushed (NORMAL, HOT or PUSHED).
//
// DANGEROUS_MODE: The file could be potentially dangerous.
// .-------------------------------------------------------.
// | [ ! ] [This type of file can  ]  [ Keep ] [ Discard ] |
// | [   ] [destroy your computer..]  [      ] [         ] |
// `-------------------------------------------------------'
//  |  |    |                          |                 \_ No drop down button.
//  |  |    |                           \_ Buttons are views::LabelButtons.
//  |  |     \_ Text is in a label (dangerous_download_label_)
//  |   \_ Warning icon.  No progress animation.
//   \_ Body is static.  Doesn't respond to mouse hover or press. (NORMAL only)
//
// MALICIOUS_MODE: The file is known malware.
// .---------------------------------------------+-.
// | [ - ] [This file is malicious.] [ Discard ] |v|
// | [   ] [                       ] [         ] | |-.
// `---------------------------------------------+-' |
//  |  |    |                         |            Drop down button. Responds to
//  |  |    |                         |            mouse.(NORMAL, HOT or PUSHED)
//  |  |    |                          \_ Button is a views::LabelButton.
//  |  |     \_ Text is in a label (dangerous_download_label_)
//  |   \_ Warning icon.  No progress animation.
//   \_ Body is static.  Doesn't respond to mouse hover or press. (NORMAL only)
//
void DownloadItemView::OnPaintBackground(gfx::Canvas* canvas) {
  BodyImageSet* body_image_set = NULL;
  switch (mode_) {
    case NORMAL_MODE:
      if (body_state_ == PUSHED)
        body_image_set = &pushed_body_image_set_;
      else                      // NORMAL or HOT
        body_image_set = &normal_body_image_set_;
      break;
    case DANGEROUS_MODE:
      body_image_set = &dangerous_mode_body_image_set_;
      break;
    case MALICIOUS_MODE:
      body_image_set = &malicious_mode_body_image_set_;
      break;
    default:
      NOTREACHED();
  }

  DropDownImageSet* drop_down_image_set = NULL;
  switch (mode_) {
    case NORMAL_MODE:
    case MALICIOUS_MODE:
      if (drop_down_state_ == PUSHED)
        drop_down_image_set = &pushed_drop_down_image_set_;
      else                        // NORMAL or HOT
        drop_down_image_set = &normal_drop_down_image_set_;
      break;
    case DANGEROUS_MODE:
      // We don't use a drop down button for mode_ == DANGEROUS_MODE.  So we let
      // drop_down_image_set == NULL.
      break;
    default:
      NOTREACHED();
  }

  int center_width = width() - kLeftPadding -
                     body_image_set->left->width() -
                     body_image_set->right->width() -
                     (drop_down_image_set ?
                        normal_drop_down_image_set_.center->width() :
                        0);

  // May be caused by animation.
  if (center_width <= 0)
    return;

  // Draw status before button image to effectively lighten text.  No status for
  // warning dialogs.
  if (!IsShowingWarningDialog()) {
    if (!status_text_.empty()) {
      int mirrored_x = GetMirroredXWithWidthInView(
          2 * kProgressPadding + DownloadShelf::kProgressIndicatorSize,
          kTextWidth);
      // Add font_list_.height() to compensate for title, which is drawn later.
      int y = box_y_ + kVerticalPadding + font_list_.GetHeight() +
              kVerticalTextPadding;
      SkColor file_name_color = GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_BOOKMARK_TEXT);
      // If text is light-on-dark, lightening it alone will do nothing.  In this
      // case we multiply color components by 80% before drawing.
      if (!color_utils::IsDark(file_name_color)) {
        file_name_color =
            color_utils::AlphaBlend(SK_ColorBLACK, file_name_color, 255 / 5);
      }
      canvas->DrawStringRect(status_text_, font_list_, file_name_color,
                             gfx::Rect(mirrored_x, y, kTextWidth,
                                       font_list_.GetHeight()));
    }
  }

  // Paint the background images.
  {
    gfx::ScopedRTLFlipCanvas scoped_canvas(canvas, width());

    int x = kLeftPadding;
    PaintImages(canvas,
                body_image_set->top_left, body_image_set->left,
                body_image_set->bottom_left,
                x, box_y_, box_height_, body_image_set->top_left->width());
    x += body_image_set->top_left->width();
    PaintImages(canvas,
                body_image_set->top, body_image_set->center,
                body_image_set->bottom,
                x, box_y_, box_height_, center_width);
    x += center_width;
    PaintImages(canvas,
                body_image_set->top_right, body_image_set->right,
                body_image_set->bottom_right,
                x, box_y_, box_height_, body_image_set->top_right->width());

    // Overlay our body hot state. Warning dialogs don't display body a hot
    // state.
    if (!IsShowingWarningDialog() &&
        body_hover_animation_->GetCurrentValue() > 0) {
      canvas->SaveLayerAlpha(
          static_cast<int>(body_hover_animation_->GetCurrentValue() * 255));

      int x = kLeftPadding;
      PaintImages(canvas,
                  hot_body_image_set_.top_left, hot_body_image_set_.left,
                  hot_body_image_set_.bottom_left,
                  x, box_y_, box_height_,
                  hot_body_image_set_.top_left->width());
      x += body_image_set->top_left->width();
      PaintImages(canvas,
                  hot_body_image_set_.top, hot_body_image_set_.center,
                  hot_body_image_set_.bottom,
                  x, box_y_, box_height_, center_width);
      x += center_width;
      PaintImages(canvas,
                  hot_body_image_set_.top_right, hot_body_image_set_.right,
                  hot_body_image_set_.bottom_right,
                  x, box_y_, box_height_,
                  hot_body_image_set_.top_right->width());
      canvas->Restore();
    }

    x += body_image_set->top_right->width();

    // Paint the drop-down.
    if (drop_down_image_set) {
      PaintImages(canvas,
                  drop_down_image_set->top, drop_down_image_set->center,
                  drop_down_image_set->bottom,
                  x, box_y_, box_height_, drop_down_image_set->top->width());

      // Overlay our drop-down hot state.
      if (drop_hover_animation_->GetCurrentValue() > 0) {
        canvas->SaveLayerAlpha(
            static_cast<int>(drop_hover_animation_->GetCurrentValue() * 255));

        PaintImages(canvas,
                    drop_down_image_set->top, drop_down_image_set->center,
                    drop_down_image_set->bottom,
                    x, box_y_, box_height_, drop_down_image_set->top->width());

        canvas->Restore();
      }
    }
  }

  // Print the text, left aligned and always print the file extension.
  // Last value of x was the end of the right image, just before the button.
  // Note that in dangerous mode we use a label (as the text is multi-line).
  if (!IsShowingWarningDialog()) {
    base::string16 filename;
    if (!disabled_while_opening_) {
      filename = gfx::ElideFilename(download()->GetFileNameToReportUser(),
                                   font_list_, kTextWidth);
    } else {
      // First, Calculate the download status opening string width.
      base::string16 status_string =
          l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPENING,
                                     base::string16());
      int status_string_width = gfx::GetStringWidth(status_string, font_list_);
      // Then, elide the file name.
      base::string16 filename_string =
          gfx::ElideFilename(download()->GetFileNameToReportUser(), font_list_,
                            kTextWidth - status_string_width);
      // Last, concat the whole string.
      filename = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPENING,
                                            filename_string);
    }

    int mirrored_x = GetMirroredXWithWidthInView(
        2 * kProgressPadding + DownloadShelf::kProgressIndicatorSize,
        kTextWidth);
    SkColor file_name_color = GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_BOOKMARK_TEXT);
    int y =
        box_y_ + (status_text_.empty() ?
            ((box_height_ - font_list_.GetHeight()) / 2) : kVerticalPadding);

    // Draw the file's name.
    canvas->DrawStringRect(
        filename, font_list_,
        enabled() ? file_name_color : kFileNameDisabledColor,
        gfx::Rect(mirrored_x, y, kTextWidth, font_list_.GetHeight()));
  }

  // Load the icon.
  IconManager* im = g_browser_process->icon_manager();
  gfx::Image* image = im->LookupIconFromFilepath(
      download()->GetTargetFilePath(), IconLoader::SMALL);
  const gfx::ImageSkia* icon = NULL;
  if (IsShowingWarningDialog())
    icon = warning_icon_;
  else if (image)
    icon = image->ToImageSkia();

  // We count on the fact that the icon manager will cache the icons and if one
  // is available, it will be cached here. We *don't* want to request the icon
  // to be loaded here, since this will also get called if the icon can't be
  // loaded, in which case LookupIcon will always be NULL. The loading will be
  // triggered only when we think the status might change.
  if (icon) {
    int progress_x =
        base::i18n::IsRTL()
            ? width() - kProgressPadding - DownloadShelf::kProgressIndicatorSize
            : kProgressPadding;
    int progress_y = kProgressPadding;

    if (!IsShowingWarningDialog()) {
      canvas->Save();
      canvas->Translate(gfx::Vector2d(progress_x, progress_y));

      DownloadItem::DownloadState state = download()->GetState();
      if (state == DownloadItem::IN_PROGRESS) {
        base::TimeDelta progress_time = previous_progress_elapsed_;
        if (!download()->IsPaused())
          progress_time += base::TimeTicks::Now() - progress_start_time_;
        DownloadShelf::PaintDownloadProgress(canvas, *GetThemeProvider(),
                                             progress_time,
                                             model_.PercentComplete());
      } else if (complete_animation_.get() &&
                 complete_animation_->is_animating()) {
        if (state == DownloadItem::INTERRUPTED) {
          DownloadShelf::PaintDownloadInterrupted(
              canvas, *GetThemeProvider(),
              complete_animation_->GetCurrentValue());
        } else {
          DCHECK_EQ(DownloadItem::COMPLETE, state);
          DownloadShelf::PaintDownloadComplete(
              canvas, *GetThemeProvider(),
              complete_animation_->GetCurrentValue());
        }
      }
      canvas->Restore();
    }

    // Draw the icon image.
    int icon_x, icon_y;

    if (IsShowingWarningDialog()) {
      icon_x = kLeftPadding + body_image_set->top_left->width();
      icon_x = GetMirroredXWithWidthInView(icon_x, icon->width());
      icon_y = (height() - icon->height()) / 2;
    } else {
      icon_x = progress_x + DownloadShelf::kFiletypeIconOffset;
      icon_y = progress_y + DownloadShelf::kFiletypeIconOffset;
    }
    if (enabled()) {
      canvas->DrawImageInt(*icon, icon_x, icon_y);
    } else {
      // Use an alpha to make the image look disabled.
      SkPaint paint;
      paint.setAlpha(120);
      canvas->DrawImageInt(*icon, icon_x, icon_y, paint);
    }
  }
}

void DownloadItemView::OnFocus() {
  View::OnFocus();
  // We render differently when focused.
  SchedulePaint();
}

void DownloadItemView::OnBlur() {
  View::OnBlur();
  // We render differently when focused.
  SchedulePaint();
}

void DownloadItemView::OpenDownload() {
  DCHECK(!IsShowingWarningDialog());
  // We're interested in how long it takes users to open downloads.  If they
  // open downloads super quickly, we should be concerned about clickjacking.
  UMA_HISTOGRAM_LONG_TIMES("clickjacking.open_download",
                           base::Time::Now() - creation_time_);

  UpdateAccessibleName();

  // Calling download()->OpenDownload may delete this, so this must be
  // the last thing we do.
  download()->OpenDownload();
}

bool DownloadItemView::SubmitDownloadToFeedbackService() {
#if defined(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service)
    return false;
  safe_browsing::DownloadProtectionService* download_protection_service =
      sb_service->download_protection_service();
  if (!download_protection_service)
    return false;
  download_protection_service->feedback_service()->BeginFeedbackForDownload(
      download());
  // WARNING: we are deleted at this point.  Don't access 'this'.
  return true;
#else
  NOTREACHED();
  return false;
#endif
}

void DownloadItemView::PossiblySubmitDownloadToFeedbackService(bool enabled) {
  if (!enabled || !SubmitDownloadToFeedbackService())
    download()->Remove();
  // WARNING: 'this' is deleted at this point. Don't access 'this'.
}

void DownloadItemView::LoadIcon() {
  IconManager* im = g_browser_process->icon_manager();
  last_download_item_path_ = download()->GetTargetFilePath();
  im->LoadIcon(last_download_item_path_,
               IconLoader::SMALL,
               base::Bind(&DownloadItemView::OnExtractIconComplete,
                          base::Unretained(this)),
               &cancelable_task_tracker_);
}

void DownloadItemView::LoadIconIfItemPathChanged() {
  base::FilePath current_download_path = download()->GetTargetFilePath();
  if (last_download_item_path_ == current_download_path)
    return;

  LoadIcon();
}

void DownloadItemView::UpdateColorsFromTheme() {
  if (dangerous_download_label_ && GetThemeProvider()) {
    dangerous_download_label_->SetEnabledColor(
        GetThemeProvider()->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT));
  }
}

void DownloadItemView::ShowContextMenuImpl(const gfx::Point& p,
                                           ui::MenuSourceType source_type) {
  gfx::Point point = p;
  gfx::Size size;

  // Similar hack as in MenuButton.
  // We're about to show the menu from a mouse press. By showing from the
  // mouse press event we block RootView in mouse dispatching. This also
  // appears to cause RootView to get a mouse pressed BEFORE the mouse
  // release is seen, which means RootView sends us another mouse press no
  // matter where the user pressed. To force RootView to recalculate the
  // mouse target during the mouse press we explicitly set the mouse handler
  // to NULL.
  static_cast<views::internal::RootView*>(GetWidget()->GetRootView())->
      SetMouseHandler(NULL);

  // If |is_mouse_gesture| is false, |p| is ignored. The menu is shown aligned
  // to drop down arrow button.
  if (source_type != ui::MENU_SOURCE_MOUSE &&
      source_type != ui::MENU_SOURCE_TOUCH) {
    drop_down_pressed_ = true;
    SetState(NORMAL, PUSHED);
    point.SetPoint(drop_down_x_left_, box_y_);
    size.SetSize(drop_down_x_right_ - drop_down_x_left_, box_height_);
  }
  // Post a task to release the button.  When we call the Run method on the menu
  // below, it runs an inner message loop that might cause us to be deleted.
  // Posting a task with a WeakPtr lets us safely handle the button release.
  base::ThreadTaskRunnerHandle::Get()->PostNonNestableTask(
      FROM_HERE, base::Bind(&DownloadItemView::ReleaseDropDown,
                            weak_ptr_factory_.GetWeakPtr()));
  views::View::ConvertPointToScreen(this, &point);

  if (!context_menu_.get())
    context_menu_.reset(new DownloadShelfContextMenuView(download()));

  context_menu_->Run(GetWidget()->GetTopLevelWidget(),
                     gfx::Rect(point, size), source_type);
  // We could be deleted now.
}

void DownloadItemView::HandlePressEvent(const ui::LocatedEvent& event,
                                        bool active_event) {
  // The event should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  // Stop any completion animation.
  if (complete_animation_.get() && complete_animation_->is_animating())
    complete_animation_->End();

  if (active_event) {
    if (InDropDownButtonXCoordinateRange(event.x())) {
      if (context_menu_.get()) {
        // Ignore two close clicks. This typically happens when the user clicks
        // the button to close the menu.
        base::TimeDelta delta =
            base::TimeTicks::Now() - context_menu_->close_time();
        if (delta.InMilliseconds() < views::kMinimumMsBetweenButtonClicks)
          return;
      }
      drop_down_pressed_ = true;
      SetState(NORMAL, PUSHED);
      // We are setting is_mouse_gesture to false when calling ShowContextMenu
      // so that the positioning of the context menu will be similar to a
      // keyboard invocation.  I.e. we want the menu to always be positioned
      // next to the drop down button instead of the next to the pointer.
      ShowContextMenuImpl(event.location(), ui::MENU_SOURCE_KEYBOARD);
      // Once called, it is possible that *this was deleted (e.g.: due to
      // invoking the 'Discard' action.)
    } else if (!IsShowingWarningDialog()) {
      SetState(PUSHED, NORMAL);
    }
  }
}

void DownloadItemView::HandleClickEvent(const ui::LocatedEvent& event,
                                        bool active_event) {
  // Mouse should not activate us in dangerous mode.
  if (mode_ == DANGEROUS_MODE)
    return;

  SetState(NORMAL, NORMAL);

  if (!active_event ||
      InDropDownButtonXCoordinateRange(event.x()) ||
      IsShowingWarningDialog()) {
    return;
  }

  // OpenDownload may delete this, so don't add any code after this line.
  OpenDownload();
}

// Load an icon for the file type we're downloading, and animate any in progress
// download state.
void DownloadItemView::PaintImages(gfx::Canvas* canvas,
                                   const gfx::ImageSkia* top_image,
                                   const gfx::ImageSkia* center_image,
                                   const gfx::ImageSkia* bottom_image,
                                   int x, int y, int height, int width) {
  int middle_height = height - top_image->height() - bottom_image->height();
  // Draw the top.
  canvas->DrawImageInt(*top_image,
                       0, 0, top_image->width(), top_image->height(),
                       x, y, width, top_image->height(), false);
  y += top_image->height();
  // Draw the center.
  canvas->DrawImageInt(*center_image,
                       0, 0, center_image->width(), center_image->height(),
                       x, y, width, middle_height, false);
  y += middle_height;
  // Draw the bottom.
  canvas->DrawImageInt(*bottom_image,
                       0, 0, bottom_image->width(), bottom_image->height(),
                       x, y, width, bottom_image->height(), false);
}

void DownloadItemView::SetState(State new_body_state, State new_drop_state) {
  // If we are showing a warning dialog, we don't change body state.
  if (IsShowingWarningDialog()) {
    new_body_state = NORMAL;

    // Current body_state_ should always be NORMAL for warning dialogs.
    DCHECK_EQ(NORMAL, body_state_);
    // We shouldn't be calling SetState if we are in DANGEROUS_MODE.
    DCHECK_NE(DANGEROUS_MODE, mode_);
  }
  // Avoid extra SchedulePaint()s if the state is going to be the same.
  if (body_state_ == new_body_state && drop_down_state_ == new_drop_state)
    return;

  AnimateStateTransition(body_state_, new_body_state,
                         body_hover_animation_.get());
  AnimateStateTransition(drop_down_state_, new_drop_state,
                         drop_hover_animation_.get());
  body_state_ = new_body_state;
  drop_down_state_ = new_drop_state;
  SchedulePaint();
}

void DownloadItemView::ToggleWarningDialog() {
  if (model_.IsDangerous())
    ShowWarningDialog();
  else
    ClearWarningDialog();

  UpdateDropDownButtonPosition();

  // Force the shelf to layout again as our size has changed.
  shelf_->Layout();
  shelf_->SchedulePaint();
}

void DownloadItemView::ClearWarningDialog() {
  DCHECK(download()->GetDangerType() ==
         content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED);
  DCHECK(mode_ == DANGEROUS_MODE || mode_ == MALICIOUS_MODE);

  mode_ = NORMAL_MODE;
  body_state_ = NORMAL;
  drop_down_state_ = NORMAL;

  // ExperienceSampling: User proceeded through the warning.
  if (sampling_event_.get()) {
    sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kProceed);
    sampling_event_.reset(NULL);
  }
  // Remove the views used by the warning dialog.
  if (save_button_) {
    RemoveChildView(save_button_);
    delete save_button_;
    save_button_ = NULL;
  }
  RemoveChildView(discard_button_);
  delete discard_button_;
  discard_button_ = NULL;
  RemoveChildView(dangerous_download_label_);
  delete dangerous_download_label_;
  dangerous_download_label_ = NULL;
  dangerous_download_label_sized_ = false;

  // We need to load the icon now that the download has the real path.
  LoadIcon();
}

void DownloadItemView::ShowWarningDialog() {
  DCHECK(mode_ != DANGEROUS_MODE && mode_ != MALICIOUS_MODE);
  time_download_warning_shown_ = base::Time::Now();
  content::DownloadDangerType danger_type = download()->GetDangerType();
  RecordDangerousDownloadWarningShown(danger_type);
#if defined(FULL_SAFE_BROWSING)
  if (model_.ShouldAllowDownloadFeedback()) {
    safe_browsing::DownloadFeedbackService::RecordEligibleDownloadShown(
        danger_type);
  }
#endif
  mode_ = model_.MightBeMalicious() ? MALICIOUS_MODE : DANGEROUS_MODE;

  // ExperienceSampling: Dangerous or malicious download warning is being shown
  // to the user, so we start a new SamplingEvent and track it.
  std::string event_name = model_.MightBeMalicious()
                               ? ExperienceSamplingEvent::kMaliciousDownload
                               : ExperienceSamplingEvent::kDangerousDownload;
  sampling_event_.reset(
      new ExperienceSamplingEvent(event_name,
                                  download()->GetURL(),
                                  download()->GetReferrerUrl(),
                                  download()->GetBrowserContext()));

  body_state_ = NORMAL;
  drop_down_state_ = NORMAL;
  if (mode_ == DANGEROUS_MODE) {
    save_button_ = new views::LabelButton(
        this, model_.GetWarningConfirmButtonText());
    save_button_->SetStyle(views::Button::STYLE_BUTTON);
    AddChildView(save_button_);
  }
  discard_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_DISCARD_DOWNLOAD));
  discard_button_->SetStyle(views::Button::STYLE_BUTTON);
  AddChildView(discard_button_);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  switch (danger_type) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      warning_icon_ = rb.GetImageSkiaNamed(IDR_SAFEBROWSING_WARNING);
      break;

    case content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case content::DOWNLOAD_DANGER_TYPE_MAX:
      NOTREACHED();
      // fallthrough

    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      warning_icon_ = rb.GetImageSkiaNamed(IDR_WARNING);
  }
  base::string16 dangerous_label =
      model_.GetWarningText(font_list_, kTextWidth);
  dangerous_download_label_ = new views::Label(dangerous_label);
  dangerous_download_label_->SetMultiLine(true);
  dangerous_download_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  dangerous_download_label_->SetAutoColorReadabilityEnabled(false);
  AddChildView(dangerous_download_label_);
  SizeLabelToMinWidth();
}

gfx::Size DownloadItemView::GetButtonSize() const {
  DCHECK(discard_button_ && (mode_ == MALICIOUS_MODE || save_button_));
  gfx::Size size = discard_button_->GetPreferredSize();
  if (save_button_)
    size.SetToMax(save_button_->GetPreferredSize());
  return size;
}

// This method computes the minimum width of the label for displaying its text
// on 2 lines.  It just breaks the string in 2 lines on the spaces and keeps the
// configuration with minimum width.
void DownloadItemView::SizeLabelToMinWidth() {
  if (dangerous_download_label_sized_)
    return;

  base::string16 label_text = dangerous_download_label_->text();
  base::TrimWhitespace(label_text, base::TRIM_ALL, &label_text);
  DCHECK_EQ(base::string16::npos, label_text.find('\n'));

  // Make the label big so that GetPreferredSize() is not constrained by the
  // current width.
  dangerous_download_label_->SetBounds(0, 0, 1000, 1000);

  // Use a const string from here. BreakIterator requies that text.data() not
  // change during its lifetime.
  const base::string16 original_text(label_text);
  // Using BREAK_WORD can work in most cases, but it can also break
  // lines where it should not. Using BREAK_LINE is safer although
  // slower for Chinese/Japanese. This is not perf-critical at all, though.
  base::i18n::BreakIterator iter(original_text,
                                 base::i18n::BreakIterator::BREAK_LINE);
  bool status = iter.Init();
  DCHECK(status);

  base::string16 prev_text = original_text;
  gfx::Size size = dangerous_download_label_->GetPreferredSize();
  int min_width = size.width();

  // Go through the string and try each line break (starting with no line break)
  // searching for the optimal line break position.  Stop if we find one that
  // yields one that is less than kDangerousTextWidth wide.  This is to prevent
  // a short string (e.g.: "This file is malicious") from being broken up
  // unnecessarily.
  while (iter.Advance() && min_width > kDangerousTextWidth) {
    size_t pos = iter.pos();
    if (pos >= original_text.length())
      break;
    base::string16 current_text = original_text;
    // This can be a low surrogate codepoint, but u_isUWhiteSpace will
    // return false and inserting a new line after a surrogate pair
    // is perfectly ok.
    base::char16 line_end_char = current_text[pos - 1];
    if (u_isUWhiteSpace(line_end_char))
      current_text.replace(pos - 1, 1, 1, base::char16('\n'));
    else
      current_text.insert(pos, 1, base::char16('\n'));
    dangerous_download_label_->SetText(current_text);
    size = dangerous_download_label_->GetPreferredSize();

    // If the width is growing again, it means we passed the optimal width spot.
    if (size.width() > min_width) {
      dangerous_download_label_->SetText(prev_text);
      break;
    } else {
      min_width = size.width();
    }
    prev_text = current_text;
  }

  dangerous_download_label_->SetBounds(0, 0, size.width(), size.height());
  dangerous_download_label_sized_ = true;
}

void DownloadItemView::Reenable() {
  disabled_while_opening_ = false;
  SetEnabled(true);  // Triggers a repaint.
}

void DownloadItemView::ReleaseDropDown() {
  drop_down_pressed_ = false;
  SetState(NORMAL, NORMAL);
}

bool DownloadItemView::InDropDownButtonXCoordinateRange(int x) {
  if (x > drop_down_x_left_ && x < drop_down_x_right_)
    return true;
  return false;
}

void DownloadItemView::UpdateAccessibleName() {
  base::string16 new_name;
  if (IsShowingWarningDialog()) {
    new_name = dangerous_download_label_->text();
  } else {
    new_name = status_text_ + base::char16(' ') +
        download()->GetFileNameToReportUser().LossyDisplayName();
  }

  // If the name has changed, notify assistive technology that the name
  // has changed so they can announce it immediately.
  if (new_name != accessible_name_) {
    accessible_name_ = new_name;
    NotifyAccessibilityEvent(ui::AX_EVENT_TEXT_CHANGED, true);
  }
}

void DownloadItemView::UpdateDropDownButtonPosition() {
  gfx::Size size = GetPreferredSize();
  if (base::i18n::IsRTL()) {
    // Drop down button is glued to the left of the download shelf.
    drop_down_x_left_ = 0;
    drop_down_x_right_ = normal_drop_down_image_set_.top->width();
  } else {
    // Drop down button is glued to the right of the download shelf.
    drop_down_x_left_ =
      size.width() - normal_drop_down_image_set_.top->width();
    drop_down_x_right_ = size.width();
  }
}

void DownloadItemView::AnimateStateTransition(State from, State to,
                                              gfx::SlideAnimation* animation) {
  if (from == NORMAL && to == HOT) {
    animation->Show();
  } else if (from == HOT && to == NORMAL) {
    animation->Hide();
  } else if (from != to) {
    animation->Reset((to == HOT) ? 1.0 : 0.0);
  }
}

void DownloadItemView::ProgressTimerFired() {
  // Only repaint for the indeterminate size case. Otherwise, we'll repaint only
  // when there's an update notified via OnDownloadUpdated().
  if (model_.PercentComplete() < 0)
    SchedulePaint();
}
