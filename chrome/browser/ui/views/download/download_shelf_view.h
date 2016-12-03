// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_

#include <stddef.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/download/download_shelf.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/mouse_watcher.h"

class Browser;
class BrowserView;

namespace content {
class DownloadItem;
class PageNavigator;
}

namespace views {
class ImageButton;
class ImageView;
class MdTextButton;
}

// DownloadShelfView is a view that contains individual views for each download,
// as well as a close button and a link to show all downloads.
//
// DownloadShelfView does not hold an infinite number of download views, rather
// it'll automatically remove views once a certain point is reached.
class DownloadShelfView : public views::AccessiblePaneView,
                          public gfx::AnimationDelegate,
                          public DownloadShelf,
                          public views::ButtonListener,
                          public views::LinkListener,
                          public views::MouseWatcherListener {
 public:
  DownloadShelfView(Browser* browser, BrowserView* parent);
  ~DownloadShelfView() override;

  // Sent from the DownloadItemView when the user opens an item.
  void OpenedDownload();

  // Returns the relevant containing object that can load pages.
  // i.e. the |browser_|.
  content::PageNavigator* GetNavigator();

  // Returns the parent_.
  BrowserView* get_parent() { return parent_; }

  // Implementation of View.
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // Implementation of gfx::AnimationDelegate.
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Implementation of views::LinkListener.
  // Invoked when the user clicks the 'show all downloads' link button.
  void LinkClicked(views::Link* source, int event_flags) override;

  // Implementation of ButtonListener.
  // Invoked when the user clicks the close button. Asks the browser to
  // hide the download shelf.
  void ButtonPressed(views::Button* button, const ui::Event& event) override;

  // Implementation of DownloadShelf.
  bool IsShowing() const override;
  bool IsClosing() const override;
  Browser* browser() const override;

  // Implementation of MouseWatcherListener override.
  void MouseMovedOutOfHost() override;

  // Removes a specified download view. The supplied view is deleted after
  // it's removed.
  void RemoveDownloadView(views::View* view);

 protected:
  // Implementation of DownloadShelf.
  void DoAddDownload(content::DownloadItem* download) override;
  void DoShow() override;
  void DoClose(CloseReason reason) override;

  // From AccessiblePaneView
  views::View* GetDefaultFocusableChild() override;

 private:
  // Adds a View representing a download to this DownloadShelfView.
  // DownloadShelfView takes ownership of the View, and will delete it as
  // necessary.
  void AddDownloadView(views::View* view);

  // Paints the border.
  void OnPaintBorder(gfx::Canvas* canvas) override;

  // Returns true if the shelf is wide enough to show the first download item.
  bool CanFitFirstDownloadItem();

  // Called on theme change.
  void UpdateColorsFromTheme();

  // Overridden from views::View.
  void OnThemeChanged() override;

  // Called when the "close shelf" animation ended.
  void Closed();

  // Returns true if we can auto close. We can auto-close if all the items on
  // the shelf have been opened.
  bool CanAutoClose();

  // Gets the |DownloadItem| for the i^th download view. TODO(estade): this
  // shouldn't be necessary after we only have one type of DownloadItemView.
  content::DownloadItem* GetDownloadItemForView(size_t i);

  // Returns the color of text for the shelf (used for deriving icon color).
  SkColor GetTextColorForIconMd();

  // The browser for this shelf.
  Browser* browser_;

  // The animation for adding new items to the shelf.
  gfx::SlideAnimation new_item_animation_;

  // The show/hide animation for the shelf itself.
  gfx::SlideAnimation shelf_animation_;

  // The download views. These are also child Views, and deleted when
  // the DownloadShelfView is deleted.
  std::vector<views::View*> download_views_;

  // An image displayed on the right of the "Show all downloads..." link.
  // TODO(estade): not shown in MD; remove.
  views::ImageView* arrow_image_;

  // Link for showing all downloads. For MD this is a system style button.
  views::View* show_all_view_;

  // This is the same as |show_all_view_|, but only valid in MD mode.
  views::MdTextButton* show_all_view_md_;

  // Button for closing the downloads. This is contained as a child, and
  // deleted by View.
  views::ImageButton* close_button_;

  // The window this shelf belongs to.
  BrowserView* parent_;

  views::MouseWatcher mouse_watcher_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_VIEW_H_
