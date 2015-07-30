// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"

#include "base/i18n/rtl.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_view.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/grit/generated_resources.h"
#include "components/ui/zoom/page_zoom.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

// static
ZoomBubbleView* ZoomBubbleView::zoom_bubble_ = NULL;

// static
void ZoomBubbleView::ShowBubble(content::WebContents* web_contents,
                                bool auto_close) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser && browser->window() &&
         browser->exclusive_access_manager()->fullscreen_controller());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  bool anchor_to_view = !is_fullscreen ||
      browser_view->immersive_mode_controller()->IsRevealed();
  views::View* anchor_view = anchor_to_view ?
      browser_view->GetLocationBarView()->zoom_view() : NULL;

  // Find the extension that initiated the zoom change, if any.
  ui_zoom::ZoomController* zoom_controller =
      ui_zoom::ZoomController::FromWebContents(web_contents);
  const ui_zoom::ZoomRequestClient* client = zoom_controller->last_client();

  // If the bubble is already showing in this window and the zoom change was not
  // initiated by an extension, then the bubble can be reused and only the label
  // text needs to be updated.
  if (zoom_bubble_ && zoom_bubble_->GetAnchorView() == anchor_view && !client) {
    DCHECK_EQ(web_contents, zoom_bubble_->web_contents_);
    zoom_bubble_->Refresh();
    return;
  }

  // If the bubble is already showing but in a different tab, the current
  // bubble must be closed and a new one created.
  CloseBubble();

  zoom_bubble_ = new ZoomBubbleView(anchor_view, web_contents, auto_close,
                                    browser_view->immersive_mode_controller());

  // If the zoom change was initiated by an extension, capture the relevent
  // information from it.
  if (client) {
    zoom_bubble_->SetExtensionInfo(
        static_cast<const extensions::ExtensionZoomRequestClient*>(client)
            ->extension());
  }

  // If we do not have an anchor view, parent the bubble to the content area.
  if (!anchor_to_view)
    zoom_bubble_->set_parent_window(web_contents->GetNativeView());

  views::BubbleDelegateView::CreateBubble(zoom_bubble_);

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen)
    zoom_bubble_->AdjustForFullscreen(browser_view->GetBoundsInScreen());

  if (auto_close)
    zoom_bubble_->GetWidget()->ShowInactive();
  else
    zoom_bubble_->GetWidget()->Show();
}

// static
void ZoomBubbleView::CloseBubble() {
  if (zoom_bubble_)
    zoom_bubble_->Close();
}

// static
ZoomBubbleView* ZoomBubbleView::GetZoomBubble() {
  return zoom_bubble_;
}

ZoomBubbleView::ZoomBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    bool auto_close,
    ImmersiveModeController* immersive_mode_controller)
    : ManagedFullScreenBubbleDelegateView(anchor_view, web_contents),
      image_button_(NULL),
      label_(NULL),
      web_contents_(web_contents),
      auto_close_(auto_close),
      immersive_mode_controller_(immersive_mode_controller) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
  set_notify_enter_exit_on_child(true);
  immersive_mode_controller_->AddObserver(this);
}

ZoomBubbleView::~ZoomBubbleView() {
  if (immersive_mode_controller_)
    immersive_mode_controller_->RemoveObserver(this);
}

void ZoomBubbleView::OnGestureEvent(ui::GestureEvent* event) {
  if (!zoom_bubble_ || !zoom_bubble_->auto_close_ ||
      event->type() != ui::ET_GESTURE_TAP) {
    return;
  }

  auto_close_ = false;
  StopTimer();
  event->SetHandled();
}

void ZoomBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  StopTimer();
}

void ZoomBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  StartTimerIfNecessary();
}

void ZoomBubbleView::Init() {
  // Set up the layout of the zoom bubble. A grid layout is used because
  // sometimes an extension icon is shown next to the zoom label.
  views::GridLayout* grid_layout = new views::GridLayout(this);
  SetLayoutManager(grid_layout);
  views::ColumnSet* columns = grid_layout->AddColumnSet(0);
  // First row.
  if (extension_info_.icon_image) {
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 2,
                       views::GridLayout::USE_PREF, 0, 0);
  }
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  grid_layout->StartRow(0, 0);

  // If this zoom change was initiated by an extension, that extension will be
  // attributed by showing its icon in the zoom bubble.
  if (extension_info_.icon_image) {
    image_button_ = new views::ImageButton(this);
    image_button_->SetTooltipText(
        l10n_util::GetStringFUTF16(IDS_TOOLTIP_ZOOM_EXTENSION_ICON,
                                   base::UTF8ToUTF16(extension_info_.name)));
    image_button_->SetImage(views::Button::STATE_NORMAL,
                            &extension_info_.icon_image->image_skia());
    grid_layout->AddView(image_button_);
  }

  // Add zoom label with the new zoom percent.
  ui_zoom::ZoomController* zoom_controller =
      ui_zoom::ZoomController::FromWebContents(web_contents_);
  int zoom_percent = zoom_controller->GetZoomPercent();
  label_ = new views::Label(
      l10n_util::GetStringFUTF16Int(IDS_TOOLTIP_ZOOM, zoom_percent));
  label_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  grid_layout->AddView(label_);

  // Second row.
  grid_layout->AddPaddingRow(0, 8);
  columns = grid_layout->AddColumnSet(1);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);
  grid_layout->StartRow(0, 1);

  // Add "Reset to Default" button.
  views::LabelButton* set_default_button = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_ZOOM_SET_DEFAULT));
  set_default_button->SetStyle(views::Button::STYLE_BUTTON);
  set_default_button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  grid_layout->AddView(set_default_button);

  StartTimerIfNecessary();
}

void ZoomBubbleView::WindowClosing() {
  // |zoom_bubble_| can be a new bubble by this point (as Close(); doesn't
  // call this right away). Only set to NULL when it's this bubble.
  if (zoom_bubble_ == this)
    zoom_bubble_ = NULL;
}

void ZoomBubbleView::Close() {
  // Widget's Close() is async, but we don't want to use zoom_bubble_ after
  // this. Additionally web_contents_ may have been destroyed.
  zoom_bubble_ = NULL;
  web_contents_ = NULL;
  ManagedFullScreenBubbleDelegateView::Close();
}

void ZoomBubbleView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (sender == image_button_) {
    DCHECK(extension_info_.icon_image) << "Invalid button press.";
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
    chrome::AddSelectedTabWithURL(
        browser, GURL(base::StringPrintf("chrome://extensions?id=%s",
                                         extension_info_.id.c_str())),
        ui::PAGE_TRANSITION_FROM_API);
  } else {
    ui_zoom::PageZoom::Zoom(web_contents_, content::PAGE_ZOOM_RESET);
  }
}

void ZoomBubbleView::OnImmersiveRevealStarted() {
  CloseBubble();
}

void ZoomBubbleView::OnImmersiveModeControllerDestroyed() {
  immersive_mode_controller_ = NULL;
}

void ZoomBubbleView::OnExtensionIconImageChanged(
    extensions::IconImage* /* image */) {
  image_button_->SetImage(views::Button::STATE_NORMAL,
                          &extension_info_.icon_image->image_skia());
  image_button_->SchedulePaint();
}

void ZoomBubbleView::Refresh() {
  ui_zoom::ZoomController* zoom_controller =
      ui_zoom::ZoomController::FromWebContents(web_contents_);
  int zoom_percent = zoom_controller->GetZoomPercent();
  label_->SetText(
      l10n_util::GetStringFUTF16Int(IDS_TOOLTIP_ZOOM, zoom_percent));
  StartTimerIfNecessary();
}

void ZoomBubbleView::SetExtensionInfo(const extensions::Extension* extension) {
  DCHECK(extension);
  extension_info_.id = extension->id();
  extension_info_.name = extension->name();

  ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia& default_extension_icon_image =
      *rb.GetImageSkiaNamed(IDR_EXTENSIONS_FAVICON);
  int icon_size = gfx::kFaviconSize;

  // We give first preference to an icon from the extension's icon set that
  // matches the size of the default. But not all extensions will declare an
  // icon set, or may not have an icon of the default size (we don't want the
  // bubble to display, for example, a very large icon). In that case, if there
  // is a browser-action icon (size-19) this is an acceptable alternative.
  const ExtensionIconSet& icons = extensions::IconsInfo::GetIcons(extension);
  bool has_default_sized_icon =
      !icons.Get(gfx::kFaviconSize, ExtensionIconSet::MATCH_EXACTLY).empty();
  if (has_default_sized_icon) {
    extension_info_.icon_image.reset(
        new extensions::IconImage(web_contents_->GetBrowserContext(),
                                  extension,
                                  icons,
                                  icon_size,
                                  default_extension_icon_image,
                                  this));
    return;
  }

  const extensions::ActionInfo* browser_action =
      extensions::ActionInfo::GetBrowserActionInfo(extension);
  if (!browser_action || browser_action->default_icon.empty())
    return;

  icon_size = browser_action->default_icon.map().begin()->first;
  extension_info_.icon_image.reset(
      new extensions::IconImage(web_contents_->GetBrowserContext(),
                                extension,
                                browser_action->default_icon,
                                icon_size,
                                default_extension_icon_image,
                                this));
}

void ZoomBubbleView::StartTimerIfNecessary() {
  if (auto_close_) {
    if (timer_.IsRunning()) {
      timer_.Reset();
    } else {
      // The number of milliseconds the bubble should stay on the screen if it
      // will close automatically.
      const int kBubbleCloseDelay = 1500;
      timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kBubbleCloseDelay),
          this,
          &ZoomBubbleView::Close);
    }
  }
}

void ZoomBubbleView::StopTimer() {
  timer_.Stop();
}

ZoomBubbleView::ZoomBubbleExtensionInfo::ZoomBubbleExtensionInfo() {}

ZoomBubbleView::ZoomBubbleExtensionInfo::~ZoomBubbleExtensionInfo() {}
