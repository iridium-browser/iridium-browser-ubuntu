// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_

#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "ui/base/layout.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/glow_hover_controller.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/view.h"

class MediaIndicatorButton;
class TabController;

namespace gfx {
class Animation;
class AnimationContainer;
class LinearAnimation;
class MultiAnimation;
class ThrobAnimation;
}
namespace views {
class ImageButton;
class Label;
}

///////////////////////////////////////////////////////////////////////////////
//
//  A View that renders a Tab in a TabStrip.
//
///////////////////////////////////////////////////////////////////////////////
class Tab : public gfx::AnimationDelegate,
            public views::ButtonListener,
            public views::ContextMenuController,
            public views::MaskedTargeterDelegate,
            public views::View {
 public:
  // The Tab's class name.
  static const char kViewClassName[];

  explicit Tab(TabController* controller);
  ~Tab() override;

  TabController* controller() const { return controller_; }

  // Used to set/check whether this Tab is being animated closed.
  void set_closing(bool closing) { closing_ = closing; }
  bool closing() const { return closing_; }

  // See description above field.
  void set_dragging(bool dragging) { dragging_ = dragging; }
  bool dragging() const { return dragging_; }

  // Used to mark the tab as having been detached.  Once this has happened, the
  // tab should be invisibly closed.  This is irreversible.
  void set_detached() { detached_ = true; }
  bool detached() const { return detached_; }

  // Sets the container all animations run from.
  void set_animation_container(gfx::AnimationContainer* container);

  // Returns true if this tab is the active tab.
  bool IsActive() const;

  // Notifies the MediaIndicatorButton that the active state of this tab has
  // changed.
  void ActiveStateChanged();

  // Returns true if the tab is selected.
  bool IsSelected() const;

  // Sets the data this tabs displays. Invokes DataChanged. Should only be
  // called after Tab is added to widget hierarchy.
  void SetData(const TabRendererData& data);
  const TabRendererData& data() const { return data_; }

  // Sets the network state. If the network state changes NetworkStateChanged is
  // invoked.
  void UpdateLoadingAnimation(TabRendererData::NetworkState state);

  // Starts/Stops a pulse animation.
  void StartPulse();
  void StopPulse();

  // Start/stop the pinned tab title animation.
  void StartPinnedTabTitleAnimation();
  void StopPinnedTabTitleAnimation();

  // Set the background offset used to match the image in the inactive tab
  // to the frame image.
  void set_background_offset(const gfx::Point& offset) {
    background_offset_ = offset;
  }

  // Returns true if this tab became the active tab selected in
  // response to the last ui::ET_TAP_DOWN gesture dispatched to
  // this tab. Only used for collecting UMA metrics.
  // See ash/touch/touch_uma.cc.
  bool tab_activated_with_last_tap_down() const {
    return tab_activated_with_last_tap_down_;
  }

  views::GlowHoverController* hover_controller() {
    return &hover_controller_;
  }

  // Returns the width of the largest part of the tab that is available for the
  // user to click to select/activate the tab.
  int GetWidthOfLargestSelectableRegion() const;

  // Returns the inset within the first dragged tab to use when calculating the
  // "drag insertion point".  If we simply used the x-coordinate of the tab,
  // we'd be calculating based on a point well before where the user considers
  // the tab to "be".  The value here is chosen to "feel good" based on the
  // widths of the tab images and the tab overlap.
  //
  // Note that this must return a value smaller than the midpoint of any tab's
  // width, or else the user won't be able to drag a tab to the left of the
  // first tab in the strip.
  static int leading_width_for_drag() { return 16; }

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();
  // Returns the minimum possible size of a selected Tab. Selected tabs must
  // always show a close button and have a larger minimum size than unselected
  // tabs.
  static gfx::Size GetMinimumSelectedSize();
  // Returns the preferred size of a single Tab, assuming space is
  // available.
  static gfx::Size GetStandardSize();

  // Returns the width for touch tabs.
  static int GetTouchWidth();

  // Returns the width for pinned tabs. Pinned tabs always have this width.
  static int GetPinnedWidth();

  // Returns the height for immersive mode tabs.
  static int GetImmersiveHeight();

 private:
  friend class TabTest;
  FRIEND_TEST_ALL_PREFIXES(TabTest, CloseButtonLayout);

  friend class TabStripTest;
  FRIEND_TEST_ALL_PREFIXES(TabStripTest, TabHitTestMaskWhenStacked);
  FRIEND_TEST_ALL_PREFIXES(TabStripTest, TabCloseButtonVisibilityWhenStacked);

  // The animation object used to swap the favicon with the sad tab icon.
  class FaviconCrashAnimation;
  class TabCloseButton;

  // Contains a cached image and the values used to generate it.
  struct ImageCacheEntry {
    ImageCacheEntry();
    ~ImageCacheEntry();

    // ID of the resource used.
    int resource_id;

    // Scale factor we're drawing it.
    ui::ScaleFactor scale_factor;

    // The image.
    gfx::ImageSkia image;
  };

  typedef std::list<ImageCacheEntry> ImageCache;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // views::MaskedTargeterDelegate:
  bool GetHitTestMask(gfx::Path* mask) const override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;
  void OnThemeChanged() override;
  const char* GetClassName() const override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  bool GetTooltipTextOrigin(const gfx::Point& p,
                            gfx::Point* origin) const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleState(ui::AXViewState* state) override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Invoked from Layout to adjust the position of the favicon or media
  // indicator for pinned tabs.
  void MaybeAdjustLeftForPinnedTab(gfx::Rect* bounds) const;

  // Invoked from SetData after |data_| has been updated to the new data.
  void DataChanged(const TabRendererData& old);

  // Paint with the normal tab style.
  void PaintTab(gfx::Canvas* canvas);

  // Paint with the "immersive mode" light-bar style.
  void PaintImmersiveTab(gfx::Canvas* canvas);

  // Paint various portions of the Tab
  void PaintTabBackground(gfx::Canvas* canvas);
  void PaintInactiveTabBackgroundWithTitleChange(gfx::Canvas* canvas);
  void PaintInactiveTabBackground(gfx::Canvas* canvas);
  void PaintInactiveTabBackgroundUsingResourceId(gfx::Canvas* canvas,
                                                 int tab_id);
  void PaintActiveTabBackground(gfx::Canvas* canvas);

  // Paints the favicon, mirrored for RTL if needed.
  void PaintIcon(gfx::Canvas* canvas);

  // Invoked if data_.network_state changes, or the network_state is not none.
  void AdvanceLoadingAnimation(TabRendererData::NetworkState old_state,
                               TabRendererData::NetworkState state);

  // Returns the number of favicon-size elements that can fit in the tab's
  // current size.
  int IconCapacity() const;

  // Returns whether the Tab should display a favicon.
  bool ShouldShowIcon() const;

  // Returns whether the Tab should display the media indicator.
  bool ShouldShowMediaIndicator() const;

  // Returns whether the Tab should display a close button.
  bool ShouldShowCloseBox() const;

  // Gets the throb value for the tab. When a tab is not selected the
  // active background is drawn at |GetThrobValue()|%. This is used for hover,
  // mini tab title change and pulsing.
  double GetThrobValue();

  // Set the temporary offset for the favicon. This is used during the crash
  // animation.
  void SetFaviconHidingOffset(int offset);

  void DisplayCrashedFavicon();
  void ResetCrashedFavicon();

  void StopCrashAnimation();
  void StartCrashAnimation();

  // Returns true if the crash animation is currently running.
  bool IsPerformingCrashAnimation() const;

  // Schedules repaint task for icon.
  void ScheduleIconPaint();

  // Returns the rectangle for the light bar in immersive mode.
  gfx::Rect GetImmersiveBarRect() const;

  // Gets the tab id and frame id.
  void GetTabIdAndFrameId(views::Widget* widget,
                          int* tab_id,
                          int* frame_id) const;

  // Returns |media_indicator_button_|, creating it on-demand.
  MediaIndicatorButton* GetMediaIndicatorButton();

  // Performs a one-time initialization of static resources such as tab images.
  static void InitTabResources();

  // Returns the minimum possible size of a single unselected Tab, not
  // including considering touch mode.
  static gfx::Size GetBasicMinimumUnselectedSize();

  // Loads the images to be used for the tab background.
  static void LoadTabImages();

  // Returns the cached image for the specified arguments, or an empty image if
  // there isn't one cached.
  static gfx::ImageSkia GetCachedImage(int resource_id,
                                       const gfx::Size& size,
                                       ui::ScaleFactor scale_factor);

  // Caches the specified image.
  static void SetCachedImage(int resource_id,
                             ui::ScaleFactor scale_factor,
                             const gfx::ImageSkia& image);

  // The controller, never NULL.
  TabController* const controller_;

  TabRendererData data_;

  // True if the tab is being animated closed.
  bool closing_;

  // True if the tab is being dragged.
  bool dragging_;

  // True if the tab has been detached.
  bool detached_;

  // The offset used to animate the favicon location. This is used when the tab
  // crashes.
  int favicon_hiding_offset_;

  // The point in time when the tab icon was first painted in the waiting state.
  base::TimeTicks waiting_start_time_;

  // The point in time when the tab icon was first painted in the loading state.
  base::TimeTicks loading_start_time_;

  // Paint state for the throbber after the most recent waiting paint.
  gfx::ThrobberWaitingState waiting_state_;

  // Step in the immersive loading progress indicator.
  int immersive_loading_step_;

  bool should_display_crashed_favicon_;

  // Whole-tab throbbing "pulse" animation.
  scoped_ptr<gfx::ThrobAnimation> pulse_animation_;

  scoped_ptr<gfx::MultiAnimation> pinned_title_change_animation_;

  // Crash icon animation (in place of favicon).
  scoped_ptr<gfx::LinearAnimation> crash_icon_animation_;

  scoped_refptr<gfx::AnimationContainer> animation_container_;

  views::ImageButton* close_button_;
  MediaIndicatorButton* media_indicator_button_;  // NULL until first use.
  views::Label* title_;

  bool tab_activated_with_last_tap_down_;

  views::GlowHoverController hover_controller_;

  // The bounds of various sections of the display.
  gfx::Rect favicon_bounds_;

  // The offset used to paint the inactive background image.
  gfx::Point background_offset_;

  struct TabImage {
    gfx::ImageSkia* image_l;
    gfx::ImageSkia* image_c;
    gfx::ImageSkia* image_r;
    int l_width;
    int r_width;
  };
  static TabImage tab_active_;
  static TabImage tab_inactive_;
  static TabImage tab_alpha_;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_;

  // Whether we're showing the media indicator. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_media_indicator_;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_;

  // The current color of the close button.
  SkColor close_button_color_;

  // As the majority of the tabs are inactive, and painting tabs is slowish,
  // we cache a handful of the inactive tab backgrounds here.
  static ImageCache* image_cache_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
