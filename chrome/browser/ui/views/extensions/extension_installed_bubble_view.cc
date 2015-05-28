// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_installed_bubble_view.h"

#include <algorithm>
#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_with_badge_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"
#include "chrome/common/extensions/sync_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_constants.h"

using extensions::Extension;

namespace {

const int kIconSize = 43;

const int kRightColumnWidth = 285;

// The Bubble uses a BubbleBorder which adds about 6 pixels of whitespace
// around the content view. We compensate by reducing our outer borders by this
// amount + 4px.
const int kOuterMarginInset = 10;
const int kHorizOuterMargin = views::kPanelHorizMargin - kOuterMarginInset;
const int kVertOuterMargin = views::kPanelVertMargin - kOuterMarginInset;

// Interior vertical margin is 8px smaller than standard
const int kVertInnerMargin = views::kPanelVertMargin - 8;

// We want to shift the right column (which contains the header and text) up
// 4px to align with icon.
const int kRightcolumnVerticalShift = -4;

}  // namespace

namespace chrome {

void ShowExtensionInstalledBubble(const Extension* extension,
                                  Browser* browser,
                                  const SkBitmap& icon) {
  ExtensionInstalledBubbleView::Show(extension, browser, icon);
}

}  // namespace chrome

// InstalledBubbleContent is the content view which is placed in the
// ExtensionInstalledBubbleView. It displays the install icon and explanatory
// text about the installed extension.
class InstalledBubbleContent : public views::View,
                               public views::ButtonListener,
                               public views::LinkListener {
 public:
  InstalledBubbleContent(const ExtensionInstalledBubble& bubble,
                         Browser* browser);

  // Overridden from views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overriden from views::LinkListener.
  void LinkClicked(views::Link* source, int event_flags) override;

 private:
  enum Flavors {
    NONE            = 0,
    HOW_TO_USE      = 1 << 0,
    HOW_TO_MANAGE   = 1 << 1,
    SHOW_KEYBINDING = 1 << 2,
    SIGN_IN_PROMO   = 1 << 3,
  };

  // Layout the signin promo at coordinates |offset_x| and |offset_y|. Returns
  // the height (in pixels) of the promo UI.
  int LayoutSigninPromo(int offset_x, int offset_y);

  // Overriden from views::View.
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;

  // The browser we're associated with.
  Browser* browser_;

  // The string that contains the link text at the beginning of the sign-in
  // promo text.
  base::string16 signin_promo_link_text_;
  // The remaining text of the sign-in promo text.
  base::string16 signin_promo_text_;

  // A vector of RenderText objects representing the full sign-in promo
  // paragraph as layed out within the bubble, but has the text of the link
  // whited out so the link can be drawn in its place.
  ScopedVector<gfx::RenderText> sign_in_promo_lines_;

  // A bitmask containing the various flavors of bubble sections to show.
  int flavors_;

  // The height, in pixels, of the sign-in promo.
  size_t height_of_signin_promo_;

  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* how_to_use_;
  views::Link* sign_in_link_;
  views::Label* manage_;
  views::Link* manage_shortcut_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(InstalledBubbleContent);
};

InstalledBubbleContent::InstalledBubbleContent(
    const ExtensionInstalledBubble& bubble,
    Browser* browser)
    : browser_(browser),
      flavors_(NONE),
      height_of_signin_promo_(0u),
      how_to_use_(NULL),
      sign_in_link_(NULL),
      manage_(NULL),
      manage_shortcut_(NULL) {
  // The Extension Installed bubble takes on various forms, depending on the
  // type of extension installed. In general, though, they are all similar:
  //
  // -------------------------
  // |      | Heading    [X] |
  // | Icon | Info           |
  // |      | Extra info     |
  // -------------------------
  //
  // Icon and Heading are always shown (as well as the close button).
  // Info is shown for browser actions, page actions and Omnibox keyword
  // extensions and might list keyboard shorcut for the former two types.
  // Extra info is...
  // ... for other types, either a description of how to manage the extension
  //     or a link to configure the keybinding shortcut (if one exists).
  // Extra info can include a promo for signing into sync.

  const Extension* extension = bubble.extension();
  if (extensions::sync_helper::IsSyncableExtension(extension) &&
      SyncPromoUI::ShouldShowSyncPromo(browser->profile()))
    flavors_ |= SIGN_IN_PROMO;

  // Determine the bubble flavor we want, based on the extension type.
  switch (bubble.type()) {
    case ExtensionInstalledBubble::BROWSER_ACTION:
    case ExtensionInstalledBubble::PAGE_ACTION:
      flavors_ |= HOW_TO_USE;
      if (bubble.has_command_keybinding()) {
        flavors_ |= SHOW_KEYBINDING;
      } else {
        // The How-To-Use text makes the bubble seem a little crowded when the
        // extension has a keybinding, so the How-To-Manage text is not shown
        // in those cases.
        flavors_ |= HOW_TO_MANAGE;
      }
      break;
    case ExtensionInstalledBubble::OMNIBOX_KEYWORD:
      flavors_ |= HOW_TO_USE | HOW_TO_MANAGE;
      break;
    case ExtensionInstalledBubble::GENERIC:
      break;
    default:
      // When adding a new bubble type, the flavor needs to be set.
      static_assert(ExtensionInstalledBubble::GENERIC == 3,
          "kBubbleType enum has changed, this switch statement must "
          "be updateed");
      break;
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& font_list =
      rb.GetFontList(ui::ResourceBundle::BaseFont);

  const SkBitmap& icon = bubble.icon();
  // Add the icon (for all flavors).
  // Scale down to 43x43, but allow smaller icons (don't scale up).
  gfx::Size size(icon.width(), icon.height());
  if (size.width() > kIconSize || size.height() > kIconSize)
    size = gfx::Size(kIconSize, kIconSize);
  icon_ = new views::ImageView();
  icon_->SetImageSize(size);
  icon_->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(icon));
  AddChildView(icon_);

  // Add the heading (for all flavors).
  base::string16 extension_name = base::UTF8ToUTF16(extension->name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  heading_ = new views::Label(l10n_util::GetStringFUTF16(
      IDS_EXTENSION_INSTALLED_HEADING, extension_name));
  heading_->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
  heading_->SetMultiLine(true);
  heading_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(heading_);

  if (flavors_ & HOW_TO_USE) {
    how_to_use_ = new views::Label(bubble.GetHowToUseDescription());
    how_to_use_->SetFontList(font_list);
    how_to_use_->SetMultiLine(true);
    how_to_use_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(how_to_use_);
  }

  if (flavors_ & SHOW_KEYBINDING) {
    manage_shortcut_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS));
    manage_shortcut_->set_listener(this);
    AddChildView(manage_shortcut_);
  }

  if (flavors_ & HOW_TO_MANAGE) {
    manage_ = new views::Label(l10n_util::GetStringUTF16(
        IDS_EXTENSION_INSTALLED_MANAGE_INFO));
    manage_->SetFontList(font_list);
    manage_->SetMultiLine(true);
    manage_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(manage_);
  }

  if (flavors_ & SIGN_IN_PROMO) {
    signin_promo_text_ =
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_SIGNIN_PROMO);

    signin_promo_link_text_ =
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_SIGNIN_PROMO_LINK);
    sign_in_link_ = new views::Link(signin_promo_link_text_);
    sign_in_link_->SetFontList(font_list);
    sign_in_link_->set_listener(this);
    AddChildView(sign_in_link_);
  }

  // Add the Close button (for all flavors).
  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::STATE_NORMAL,
      rb.GetImageSkiaNamed(IDR_CLOSE_2));
  close_button_->SetImage(views::CustomButton::STATE_HOVERED,
      rb.GetImageSkiaNamed(IDR_CLOSE_2_H));
  close_button_->SetImage(views::CustomButton::STATE_PRESSED,
      rb.GetImageSkiaNamed(IDR_CLOSE_2_P));
  AddChildView(close_button_);
}

void InstalledBubbleContent::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  DCHECK_EQ(sender, close_button_);
  GetWidget()->Close();
}

void InstalledBubbleContent::LinkClicked(views::Link* source, int event_flags) {
  GetWidget()->Close();

  if (source == sign_in_link_) {
    chrome::ShowBrowserSignin(
        browser_, signin_metrics::SOURCE_EXTENSION_INSTALL_BUBBLE);
    return;
  }

  DCHECK_EQ(manage_shortcut_, source);

  std::string configure_url = chrome::kChromeUIExtensionsURL;
  configure_url += chrome::kExtensionConfigureCommandsSubPage;
  chrome::NavigateParams params(chrome::GetSingletonTabNavigateParams(
      browser_, GURL(configure_url)));
  chrome::Navigate(&params);
}

int InstalledBubbleContent::LayoutSigninPromo(int offset_x, int offset_y) {
  sign_in_promo_lines_.clear();
  int height = 0;
  gfx::Rect contents_area = GetContentsBounds();
  if (contents_area.IsEmpty())
    return height;
  contents_area.set_width(kRightColumnWidth);

  base::string16 full_text = signin_promo_link_text_ + signin_promo_text_;

  // The link is the first item in the text.
  const gfx::Size link_size = sign_in_link_->GetPreferredSize();
  sign_in_link_->SetBounds(
      offset_x, offset_y, link_size.width(), link_size.height());

  // Word-wrap the full label text.
  const gfx::FontList font_list;
  std::vector<base::string16> lines;
  gfx::ElideRectangleText(full_text, font_list, contents_area.width(),
                          contents_area.height(), gfx::ELIDE_LONG_WORDS,
                          &lines);

  gfx::Point position = gfx::Point(
      contents_area.origin().x() + offset_x,
      contents_area.origin().y() + offset_y + 1);
  if (base::i18n::IsRTL()) {
    position -= gfx::Vector2d(
        2 * views::kPanelHorizMargin + kHorizOuterMargin, 0);
  }

  // Loop through the lines, creating a renderer for each.
  for (std::vector<base::string16>::const_iterator it = lines.begin();
       it != lines.end(); ++it) {
    gfx::RenderText* line = gfx::RenderText::CreateInstance();
    line->SetDirectionalityMode(gfx::DIRECTIONALITY_FROM_UI);
    line->SetText(*it);
    const gfx::Size size(contents_area.width(),
                         line->GetStringSize().height());
    line->SetDisplayRect(gfx::Rect(position, size));
    position.set_y(position.y() + size.height());
    sign_in_promo_lines_.push_back(line);
    height += size.height();
  }

  // The link is drawn separately; make it transparent here to only draw once.
  // The link always leads other text and is assumed to fit on the first line.
  sign_in_promo_lines_.front()->ApplyColor(SK_ColorTRANSPARENT,
      gfx::Range(0, signin_promo_link_text_.size()));

  return height;
}

gfx::Size InstalledBubbleContent::GetPreferredSize() const {
  int width = kHorizOuterMargin;
  width += kIconSize;
  width += views::kPanelHorizMargin;
  width += kRightColumnWidth;
  width += 2 * views::kPanelHorizMargin;
  width += kHorizOuterMargin;

  int height = kVertOuterMargin;
  height += heading_->GetHeightForWidth(kRightColumnWidth);
  height += kVertInnerMargin;

  if (flavors_ & HOW_TO_USE) {
    height += how_to_use_->GetHeightForWidth(kRightColumnWidth);
    height += kVertInnerMargin;
  }

  if (flavors_ & HOW_TO_MANAGE) {
    height += manage_->GetHeightForWidth(kRightColumnWidth);
    height += kVertInnerMargin;
  }

  if (flavors_ & SIGN_IN_PROMO && height_of_signin_promo_ > 0u) {
    height += height_of_signin_promo_;
    height += kVertInnerMargin;
  }

  if (flavors_ & SHOW_KEYBINDING) {
    height += manage_shortcut_->GetHeightForWidth(kRightColumnWidth);
    height += kVertInnerMargin;
  }

  return gfx::Size(width, std::max(height, kIconSize + 2 * kVertOuterMargin));
}

void InstalledBubbleContent::Layout() {
  int x = kHorizOuterMargin;
  int y = kVertOuterMargin;

  icon_->SetBounds(x, y, kIconSize, kIconSize);
  x += kIconSize;
  x += views::kPanelHorizMargin;

  y += kRightcolumnVerticalShift;
  heading_->SizeToFit(kRightColumnWidth);
  heading_->SetX(x);
  heading_->SetY(y);
  y += heading_->height();
  y += kVertInnerMargin;

  if (flavors_ & HOW_TO_USE) {
    how_to_use_->SizeToFit(kRightColumnWidth);
    how_to_use_->SetX(x);
    how_to_use_->SetY(y);
    y += how_to_use_->height();
    y += kVertInnerMargin;
  }

  if (flavors_ & HOW_TO_MANAGE) {
    manage_->SizeToFit(kRightColumnWidth);
    manage_->SetX(x);
    manage_->SetY(y);
    y += manage_->height();
    y += kVertInnerMargin;
  }

  if (flavors_ & SIGN_IN_PROMO) {
    height_of_signin_promo_ = LayoutSigninPromo(x, y);
    y += height_of_signin_promo_;
    y += kVertInnerMargin;
  }

  if (flavors_ & SHOW_KEYBINDING) {
    gfx::Size sz = manage_shortcut_->GetPreferredSize();
    manage_shortcut_->SetBounds(width() - 2 * kHorizOuterMargin - sz.width(),
                                y,
                                sz.width(),
                                sz.height());
    y += manage_shortcut_->height();
    y += kVertInnerMargin;
  }

  gfx::Size sz;
  x += kRightColumnWidth + 2 * views::kPanelHorizMargin + kHorizOuterMargin -
      close_button_->GetPreferredSize().width();
  y = kVertOuterMargin;
  sz = close_button_->GetPreferredSize();
  // x-1 & y-1 is just slop to get the close button visually aligned with the
  // title text and bubble arrow.
  close_button_->SetBounds(x - 1, y - 1, sz.width(), sz.height());
}

void InstalledBubbleContent::OnPaint(gfx::Canvas* canvas) {
  for (ScopedVector<gfx::RenderText>::const_iterator it =
           sign_in_promo_lines_.begin();
       it != sign_in_promo_lines_.end(); ++it)
    (*it)->Draw(canvas);

  views::View::OnPaint(canvas);
}

void ExtensionInstalledBubbleView::Show(const Extension* extension,
                                        Browser* browser,
                                        const SkBitmap& icon) {
  new ExtensionInstalledBubbleView(extension, browser, icon);
}

ExtensionInstalledBubbleView::ExtensionInstalledBubbleView(
    const Extension* extension, Browser *browser, const SkBitmap& icon)
    : bubble_(this, extension, browser, icon) {
}

ExtensionInstalledBubbleView::~ExtensionInstalledBubbleView() {}

bool ExtensionInstalledBubbleView::MaybeShowNow() {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(bubble_.browser());

  views::View* reference_view = NULL;
  if (bubble_.type() == bubble_.BROWSER_ACTION ||
      extensions::FeatureSwitch::extension_action_redesign()->IsEnabled()) {
    BrowserActionsContainer* container =
        browser_view->GetToolbarView()->browser_actions();
    if (container->animating())
      return false;

    reference_view = container->GetViewForId(bubble_.extension()->id());
    // If the view is not visible then it is in the chevron, so point the
    // install bubble to the chevron instead. If this is an incognito window,
    // both could be invisible.
    if (!reference_view || !reference_view->visible()) {
      reference_view = container->chevron();
      if (!reference_view || !reference_view->visible())
        reference_view = NULL;  // fall back to app menu below.
    }
  } else if (bubble_.type() == bubble_.PAGE_ACTION) {
    LocationBarView* location_bar_view = browser_view->GetLocationBarView();
    ExtensionAction* page_action =
        extensions::ExtensionActionManager::Get(bubble_.browser()->profile())->
            GetPageAction(*bubble_.extension());
    location_bar_view->SetPreviewEnabledPageAction(page_action,
                                                   true);  // preview_enabled
    reference_view = location_bar_view->GetPageActionView(page_action);
    DCHECK(reference_view);
  } else if (bubble_.type() == bubble_.OMNIBOX_KEYWORD) {
    LocationBarView* location_bar_view = browser_view->GetLocationBarView();
    reference_view = location_bar_view;
    DCHECK(reference_view);
  }

  // Default case.
  if (reference_view == NULL)
    reference_view = browser_view->GetToolbarView()->app_menu();
  SetAnchorView(reference_view);

  set_arrow(bubble_.type() == bubble_.OMNIBOX_KEYWORD ?
            views::BubbleBorder::TOP_LEFT :
            views::BubbleBorder::TOP_RIGHT);
  SetLayoutManager(new views::FillLayout());
  AddChildView(new InstalledBubbleContent(bubble_, bubble_.browser()));

  views::BubbleDelegateView::CreateBubble(this)->Show();

  // The bubble widget is now the parent and owner of |this| and takes care of
  // deletion when the bubble or browser go away.
  bubble_.IgnoreBrowserClosing();

  return true;
}

gfx::Rect ExtensionInstalledBubbleView::GetAnchorRect() const {
  // For omnibox keyword bubbles, move the arrow to point to the left edge
  // of the omnibox, just to the right of the icon.
  if (bubble_.type() == bubble_.OMNIBOX_KEYWORD) {
    const LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(bubble_.browser())->
        GetLocationBarView();
    return gfx::Rect(location_bar_view->GetOmniboxViewOrigin(),
        gfx::Size(0, location_bar_view->omnibox_view()->height()));
  }
  return views::BubbleDelegateView::GetAnchorRect();
}

void ExtensionInstalledBubbleView::WindowClosing() {
  if (bubble_.extension() && bubble_.type() == bubble_.PAGE_ACTION &&
      !extensions::FeatureSwitch::extension_action_redesign()->IsEnabled()) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(bubble_.browser());
    browser_view->GetLocationBarView()->SetPreviewEnabledPageAction(
        extensions::ExtensionActionManager::Get(bubble_.browser()->profile())->
        GetPageAction(*bubble_.extension()),
        false);  // preview_enabled
  }
}
