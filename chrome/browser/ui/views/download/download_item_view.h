// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A ChromeView that implements one download on the Download shelf.
// Each DownloadItemView contains an application icon, a text label
// indicating the download's file name, a text label indicating the
// download's status (such as the number of bytes downloaded so far)
// and a button for canceling an in progress download, or opening
// the completed download.
//
// The DownloadItemView lives in the Browser, and has a corresponding
// DownloadController that receives / writes data which lives in the
// Renderer.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_ITEM_VIEW_H__
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_ITEM_VIEW_H__

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/icon_manager.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/font_list.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class DownloadShelfView;
class DownloadShelfContextMenuView;

namespace extensions {
class ExperienceSamplingEvent;
}

namespace gfx {
class Image;
class ImageSkia;
class SlideAnimation;
}

namespace views {
class Label;
class LabelButton;
}

class DownloadItemView : public views::ButtonListener,
                         public views::View,
                         public views::ContextMenuController,
                         public content::DownloadItem::Observer,
                         public gfx::AnimationDelegate {
 public:
  DownloadItemView(content::DownloadItem* download, DownloadShelfView* parent);
  ~DownloadItemView() override;

  // Timer callback for handling animations
  void UpdateDownloadProgress();
  void StartDownloadProgress();
  void StopDownloadProgress();

  // IconManager::Client interface.
  void OnExtractIconComplete(gfx::Image* icon);

  // Returns the DownloadItem model object belonging to this item.
  content::DownloadItem* download() { return model_.download(); }

  // DownloadItem::Observer methods
  void OnDownloadUpdated(content::DownloadItem* download) override;
  void OnDownloadOpened(content::DownloadItem* download) override;
  void OnDownloadDestroyed(content::DownloadItem* download) override;

  // Overridden from views::View:
  void Layout() override;
  gfx::Size GetPreferredSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  void GetAccessibleState(ui::AXViewState* state) override;
  void OnThemeChanged() override;

  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from views::ContextMenuController.
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // gfx::AnimationDelegate implementation.
  void AnimationProgressed(const gfx::Animation* animation) override;

 protected:
  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;

 private:
  enum State {
    NORMAL = 0,
    HOT,
    PUSHED
  };

  enum Mode {
    NORMAL_MODE = 0,        // Showing download item.
    DANGEROUS_MODE,         // Displaying the dangerous download warning.
    MALICIOUS_MODE          // Displaying the malicious download warning.
  };

  // The image set associated with the part containing the icon and text.
  struct BodyImageSet {
    gfx::ImageSkia* top_left;
    gfx::ImageSkia* left;
    gfx::ImageSkia* bottom_left;
    gfx::ImageSkia* top;
    gfx::ImageSkia* center;
    gfx::ImageSkia* bottom;
    gfx::ImageSkia* top_right;
    gfx::ImageSkia* right;
    gfx::ImageSkia* bottom_right;
  };

  // The image set associated with the drop-down button on the right.
  struct DropDownImageSet {
    gfx::ImageSkia* top;
    gfx::ImageSkia* center;
    gfx::ImageSkia* bottom;
  };

  void OpenDownload();

  // Submits the downloaded file to the safebrowsing download feedback service.
  // Returns whether submission was successful. On successful submission,
  // |this| and the DownloadItem will have been deleted.
  bool SubmitDownloadToFeedbackService();

  // If the user has |enabled| uploading, calls SubmitDownloadToFeedbackService.
  // Otherwise, it simply removes the DownloadItem without uploading.
  void PossiblySubmitDownloadToFeedbackService(bool enabled);

  void LoadIcon();
  void LoadIconIfItemPathChanged();

  // Update the button colors based on the current theme.
  void UpdateColorsFromTheme();

  // Shows the context menu at the specified location. |point| is in the view's
  // coordinate system.
  void ShowContextMenuImpl(const gfx::Point& point,
                           ui::MenuSourceType source_type);

  // Common code for handling pointer events (i.e. mouse or gesture).
  void HandlePressEvent(const ui::LocatedEvent& event, bool active_event);
  void HandleClickEvent(const ui::LocatedEvent& event, bool active_event);

  // Convenience method to paint the 3 vertical images (bottom, middle, top)
  // that form the background.
  void PaintImages(gfx::Canvas* canvas,
                    const gfx::ImageSkia* top_image,
                    const gfx::ImageSkia* center_image,
                    const gfx::ImageSkia* bottom_image,
                    int x,
                    int y,
                    int height,
                    int width);

  // Sets the state and triggers a repaint.
  void SetState(State body_state, State drop_down_state);

  // Whether we are in the dangerous mode.
  bool IsShowingWarningDialog() const {
    return mode_ == DANGEROUS_MODE || mode_ == MALICIOUS_MODE;
  }

  // Clears or shows the warning dialog as per the state of |model_|.
  void ToggleWarningDialog();

  // Reverts from dangerous mode to normal download mode.
  void ClearWarningDialog();

  // Start displaying the dangerous download warning or the malicious download
  // warning.
  void ShowWarningDialog();

  // Sets |size| with the size of the Save and Discard buttons (they have the
  // same size).
  gfx::Size GetButtonSize() const;

  // Sizes the dangerous download label to a minimum width available using 2
  // lines.  The size is computed only the first time this method is invoked
  // and simply returned on subsequent calls.
  void SizeLabelToMinWidth();

  // Reenables the item after it has been disabled when a user clicked it to
  // open the downloaded file.
  void Reenable();

  // Releases drop down button after showing a context menu.
  void ReleaseDropDown();

  // Given |x|, returns whether |x| is within the x coordinate range of
  // the drop-down button or not.
  bool InDropDownButtonXCoordinateRange(int x);

  // Update the accessible name to reflect the current state of the control,
  // so that screenreaders can access the filename, status text, and
  // dangerous download warning message (if any).
  void UpdateAccessibleName();

  // Update the location of the drop down button.
  void UpdateDropDownButtonPosition();

  // Show/Hide/Reset |animation| based on the state transition specified by
  // |from| and |to|.
  void AnimateStateTransition(State from, State to,
                              gfx::SlideAnimation* animation);

  // Callback for |progress_timer_|.
  void ProgressTimerFired();

  // The different images used for the background.
  BodyImageSet normal_body_image_set_;
  BodyImageSet hot_body_image_set_;
  BodyImageSet pushed_body_image_set_;
  BodyImageSet dangerous_mode_body_image_set_;
  BodyImageSet malicious_mode_body_image_set_;
  DropDownImageSet normal_drop_down_image_set_;
  DropDownImageSet hot_drop_down_image_set_;
  DropDownImageSet pushed_drop_down_image_set_;

  // The warning icon showns for dangerous downloads.
  const gfx::ImageSkia* warning_icon_;

  // The download shelf that owns us.
  DownloadShelfView* shelf_;

  // Elements of our particular download
  base::string16 status_text_;

  // The font list used to print the file name and status.
  gfx::FontList font_list_;

  // The tooltip.  Only displayed when not showing a warning dialog.
  base::string16 tooltip_text_;

  // The current state (normal, hot or pushed) of the body and drop-down.
  State body_state_;
  State drop_down_state_;

  // Mode of the download item view.
  Mode mode_;

  // When download progress last began animating (pausing and resuming will
  // update this). Used for downloads of unknown size.
  base::TimeTicks progress_start_time_;

  // Keeps the amount of time spent already animating. Used to keep track of
  // total active time for downloads of unknown size.
  base::TimeDelta previous_progress_elapsed_;

  // The left and right x coordinates of the drop-down button.
  int drop_down_x_left_;
  int drop_down_x_right_;

  // Used when we are showing the menu to show the drop-down as pressed.
  bool drop_down_pressed_;

  // The height of the box formed by the background images and its labels.
  int box_height_;

  // The y coordinate of the box formed by the background images and its labels.
  int box_y_;

  // Whether we are dragging the download button.
  bool dragging_;

  // Whether we are tracking a possible drag.
  bool starting_drag_;

  // Position that a possible drag started at.
  gfx::Point drag_start_point_;

  // For canceling an in progress icon request.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // A model class to control the status text we display.
  DownloadItemModel model_;

  // Hover animations for our body and drop buttons.
  std::unique_ptr<gfx::SlideAnimation> body_hover_animation_;
  std::unique_ptr<gfx::SlideAnimation> drop_hover_animation_;

  // Animation for download complete.
  std::unique_ptr<gfx::SlideAnimation> complete_animation_;

  // Progress animation
  base::RepeatingTimer progress_timer_;

  // Dangerous mode buttons.
  views::LabelButton* save_button_;
  views::LabelButton* discard_button_;

  // Dangerous mode label.
  views::Label* dangerous_download_label_;

  // Whether the dangerous mode label has been sized yet.
  bool dangerous_download_label_sized_;

  // Whether we are currently disabled as part of opening the downloaded file.
  bool disabled_while_opening_;

  // The time at which this view was created.
  base::Time creation_time_;

  // The time at which a dangerous download warning was displayed.
  base::Time time_download_warning_shown_;

  // The currently running download context menu.
  std::unique_ptr<DownloadShelfContextMenuView> context_menu_;

  // The name of this view as reported to assistive technology.
  base::string16 accessible_name_;

  // The icon loaded in the download shelf is based on the file path of the
  // item.  Store the path used, so that we can detect a change in the path
  // and reload the icon.
  base::FilePath last_download_item_path_;

  // ExperienceSampling: This tracks dangerous/malicious downloads warning UI
  // and the user's decisions about it.
  std::unique_ptr<extensions::ExperienceSamplingEvent> sampling_event_;

  // Method factory used to delay reenabling of the item when opening the
  // downloaded file.
  base::WeakPtrFactory<DownloadItemView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_ITEM_VIEW_H__
