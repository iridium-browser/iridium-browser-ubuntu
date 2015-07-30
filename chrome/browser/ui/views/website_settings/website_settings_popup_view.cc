// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "chrome/browser/ui/website_settings/website_settings_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/user_metrics.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// NOTE(jdonnelly): This use of this process-wide variable assumes that there's
// never more than one website settings popup shown and that it's associated
// with the current window. If this assumption fails in the future, we'll need
// to return a weak pointer from ShowPopup so callers can associate it with the
// current window (or other context) and check if the popup they care about is
// showing.
bool is_popup_showing = false;

// Padding values for sections on the connection tab.
const int kConnectionSectionPaddingBottom = 16;
const int kConnectionSectionPaddingLeft = 18;
const int kConnectionSectionPaddingTop = 16;
const int kConnectionSectionPaddingRight = 18;

// The text color that is used for the site identity status text, if the site's
// identity was sucessfully verified.
const SkColor kIdentityVerifiedTextColor = 0xFF298a27;

// Left icon margin.
const int kIconMarginLeft = 6;

// Margin and padding values for the |PopupHeaderView|.
const int kHeaderMarginBottom = 10;
const int kHeaderPaddingBottom = 12;
const int kHeaderPaddingLeft = 18;
const int kHeaderPaddingRight = 8;
const int kHeaderPaddingTop = 12;

// Spacing between the site identity label and the site identity status text in
// the popup header.
const int kHeaderRowSpacing = 4;

// To make the bubble's arrow point directly at the location icon rather than at
// the Omnibox's edge, inset the bubble's anchor rect by this amount of pixels.
const int kLocationIconVerticalMargin = 5;

// The max possible width of the popup.
const int kMaxPopupWidth = 500;

// The margins between the popup border and the popup content.
const int kPopupMarginTop = 4;
const int kPopupMarginLeft = 0;
const int kPopupMarginBottom = 10;
const int kPopupMarginRight = 0;

// Padding values for sections on the permissions tab.
const int kPermissionsSectionContentMinWidth = 300;
const int kPermissionsSectionPaddingBottom = 6;
const int kPermissionsSectionPaddingLeft = 18;
const int kPermissionsSectionPaddingTop = 16;

// Space between the headline and the content of a section on the permissions
// tab.
const int kPermissionsSectionHeadlineMarginBottom = 10;
// The content of the "Permissions" section and the "Cookies and Site Data"
// section is structured in individual rows. |kPermissionsSectionRowSpacing|
// is the space between these rows.
const int kPermissionsSectionRowSpacing = 2;

const int kSiteDataIconColumnWidth = 20;
const int kSiteDataSectionRowSpacing = 11;

}  // namespace

// |PopupHeaderView| is the UI element (view) that represents the header of the
// |WebsiteSettingsPopupView|. The header shows the status of the site's
// identity check and the name of the site's identity.
class PopupHeaderView : public views::View {
 public:
  explicit PopupHeaderView(views::ButtonListener* close_button_listener);
  ~PopupHeaderView() override;

  // Sets the name of the site's identity.
  void SetIdentityName(const base::string16& name);

  // Sets the |status_text| for the identity check of this site and the
  // |text_color|.
  void SetIdentityStatus(const base::string16& status_text, SkColor text_color);

 private:
  // The label that displays the name of the site's identity.
  views::Label* name_;
  // The label that displays the status of the identity check for this site.
  views::Label* status_;

  DISALLOW_COPY_AND_ASSIGN(PopupHeaderView);
};

// Website Settings are not supported for internal Chrome pages. Instead of the
// |WebsiteSettingsPopupView|, the |InternalPageInfoPopupView| is
// displayed.
class InternalPageInfoPopupView : public views::BubbleDelegateView {
 public:
  explicit InternalPageInfoPopupView(views::View* anchor_view);
  ~InternalPageInfoPopupView() override;

  // views::BubbleDelegateView:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalPageInfoPopupView);
};

////////////////////////////////////////////////////////////////////////////////
// Popup Header
////////////////////////////////////////////////////////////////////////////////

PopupHeaderView::PopupHeaderView(views::ButtonListener* close_button_listener)
    : name_(nullptr), status_(nullptr) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int label_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(label_column);
  column_set->AddPaddingColumn(0, kHeaderPaddingLeft);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(1, 0);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, kHeaderPaddingRight);

  layout->AddPaddingRow(0, kHeaderPaddingTop);

  layout->StartRow(0, label_column);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  name_ = new views::Label(
      base::string16(), rb.GetFontList(ui::ResourceBundle::BoldFont));
  layout->AddView(name_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::TRAILING);
  views::ImageButton* close_button =
      new views::ImageButton(close_button_listener);
  close_button->SetImage(views::CustomButton::STATE_NORMAL,
                         rb.GetImageNamed(IDR_CLOSE_2).ToImageSkia());
  close_button->SetImage(views::CustomButton::STATE_HOVERED,
                         rb.GetImageNamed(IDR_CLOSE_2_H).ToImageSkia());
  close_button->SetImage(views::CustomButton::STATE_PRESSED,
                         rb.GetImageNamed(IDR_CLOSE_2_P).ToImageSkia());
  layout->AddView(close_button, 1, 1, views::GridLayout::TRAILING,
                  views::GridLayout::LEADING);

  layout->AddPaddingRow(0, kHeaderRowSpacing);

  layout->StartRow(1, label_column);
  status_ = new views::Label(base::string16());
  status_->SetMultiLine(true);
  status_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(status_,
                  1,
                  1,
                  views::GridLayout::LEADING,
                  views::GridLayout::LEADING);

  layout->AddPaddingRow(1, kHeaderPaddingBottom);
}

PopupHeaderView::~PopupHeaderView() {
}

void PopupHeaderView::SetIdentityName(const base::string16& name) {
  name_->SetText(name);
}

void PopupHeaderView::SetIdentityStatus(const base::string16& status,
                                        SkColor text_color) {
  status_->SetText(status);
  status_->SetEnabledColor(text_color);
}

////////////////////////////////////////////////////////////////////////////////
// InternalPageInfoPopupView
////////////////////////////////////////////////////////////////////////////////

InternalPageInfoPopupView::InternalPageInfoPopupView(views::View* anchor_view)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(kLocationIconVerticalMargin, 0,
                                     kLocationIconVerticalMargin, 0));

  const int kSpacing = 4;
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal, kSpacing,
                                        kSpacing, kSpacing));
  views::ImageView* icon_view = new views::ImageView();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_view->SetImage(rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_26));
  AddChildView(icon_view);

  views::Label* label =
      new views::Label(l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE));
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);

  views::BubbleDelegateView::CreateBubble(this)->Show();
  SizeToContents();
}

InternalPageInfoPopupView::~InternalPageInfoPopupView() {
}

void InternalPageInfoPopupView::OnWidgetDestroying(views::Widget* widget) {
  is_popup_showing = false;
}

////////////////////////////////////////////////////////////////////////////////
// WebsiteSettingsPopupView
////////////////////////////////////////////////////////////////////////////////

WebsiteSettingsPopupView::~WebsiteSettingsPopupView() {
}

// static
void WebsiteSettingsPopupView::ShowPopup(views::View* anchor_view,
                                         Profile* profile,
                                         content::WebContents* web_contents,
                                         const GURL& url,
                                         const content::SSLStatus& ssl,
                                         Browser* browser) {
  is_popup_showing = true;
  if (InternalChromePage(url)) {
    new InternalPageInfoPopupView(anchor_view);
  } else {
    new WebsiteSettingsPopupView(anchor_view, profile, web_contents, url, ssl,
                                 browser);
  }
}

// static
bool WebsiteSettingsPopupView::IsPopupShowing() {
  return is_popup_showing;
}

WebsiteSettingsPopupView::WebsiteSettingsPopupView(
    views::View* anchor_view,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const content::SSLStatus& ssl,
    Browser* browser)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      web_contents_(web_contents),
      browser_(browser),
      header_(nullptr),
      tabbed_pane_(nullptr),
      permissions_tab_(nullptr),
      site_data_content_(nullptr),
      cookie_dialog_link_(nullptr),
      permissions_content_(nullptr),
      connection_tab_(nullptr),
      identity_info_content_(nullptr),
      certificate_dialog_link_(nullptr),
      signed_certificate_timestamps_link_(nullptr),
      reset_decisions_button_(nullptr),
      cert_id_(0),
      help_center_link_(nullptr),
      connection_info_content_(nullptr),
      page_info_content_(nullptr),
      weak_factory_(this) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(kLocationIconVerticalMargin, 0,
                                     kLocationIconVerticalMargin, 0));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int content_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  header_ = new PopupHeaderView(this);
  layout->StartRow(1, content_column);
  layout->AddView(header_);

  layout->AddPaddingRow(1, kHeaderMarginBottom);
  tabbed_pane_ = new views::TabbedPane();
  layout->StartRow(1, content_column);
  layout->AddView(tabbed_pane_);

  // Tabs must be added after the tabbed_pane_ was added to the views hierarchy.
  // Adding the |tabbed_pane_| to the views hierarchy triggers the
  // initialization of the native tab UI element. If the native tab UI element
  // is not initalized, adding a tab will result in a NULL pointer exception.
  permissions_tab_ = CreatePermissionsTab();
  tabbed_pane_->AddTabAtIndex(
      TAB_ID_PERMISSIONS,
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TAB_LABEL_PERMISSIONS),
      permissions_tab_);
  connection_tab_ = CreateConnectionTab();
  tabbed_pane_->AddTabAtIndex(
      TAB_ID_CONNECTION,
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TAB_LABEL_CONNECTION),
      connection_tab_);
  DCHECK_EQ(tabbed_pane_->GetTabCount(), NUM_TAB_IDS);
  tabbed_pane_->set_listener(this);

  set_margins(gfx::Insets(kPopupMarginTop, kPopupMarginLeft,
                          kPopupMarginBottom, kPopupMarginRight));

  views::BubbleDelegateView::CreateBubble(this)->Show();
  SizeToContents();

  presenter_.reset(new WebsiteSettings(
      this, profile,
      TabSpecificContentSettings::FromWebContents(web_contents),
      InfoBarService::FromWebContents(web_contents), url, ssl,
      content::CertStore::GetInstance()));
}

void WebsiteSettingsPopupView::OnPermissionChanged(
    const WebsiteSettingsUI::PermissionInfo& permission) {
  presenter_->OnSitePermissionChanged(permission.type, permission.setting);
}

void WebsiteSettingsPopupView::OnWidgetDestroying(views::Widget* widget) {
  is_popup_showing = false;
  presenter_->OnUIClosing();
}

void WebsiteSettingsPopupView::ButtonPressed(views::Button* button,
                                             const ui::Event& event) {
  if (button == reset_decisions_button_)
    presenter_->OnRevokeSSLErrorBypassButtonPressed();
  GetWidget()->Close();
}

void WebsiteSettingsPopupView::LinkClicked(views::Link* source,
                                           int event_flags) {
  // The popup closes automatically when the collected cookies dialog or the
  // certificate viewer opens. So delay handling of the link clicked to avoid
  // a crash in the base class which needs to complete the mouse event handling.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&WebsiteSettingsPopupView::HandleLinkClickedAsync,
                 weak_factory_.GetWeakPtr(), source));
}

void WebsiteSettingsPopupView::TabSelectedAt(int index) {
  switch (index) {
    case TAB_ID_PERMISSIONS:
      presenter_->RecordWebsiteSettingsAction(
          WebsiteSettings::WEBSITE_SETTINGS_PERMISSIONS_TAB_SELECTED);
      break;
    case TAB_ID_CONNECTION:
      // If the Connection tab is selected first, we're still inside the
      // construction of presenter_. In that case, the action is already logged
      // by WEBSITE_SETTINGS_CONNECTION_TAB_SHOWN_IMMEDIATELY.
      if (presenter_) {
        presenter_->RecordWebsiteSettingsAction(
            WebsiteSettings::WEBSITE_SETTINGS_CONNECTION_TAB_SELECTED);
      }
      break;
    default:
      NOTREACHED();
  }
  tabbed_pane_->GetSelectedTab()->Layout();
  SizeToContents();
}

gfx::Size WebsiteSettingsPopupView::GetPreferredSize() const {
  if (header_ == nullptr && tabbed_pane_ == nullptr)
    return views::View::GetPreferredSize();

  int height = 0;
  if (header_)
    height += header_->GetPreferredSize().height();
  if (tabbed_pane_)
    height += tabbed_pane_->GetPreferredSize().height();

  int width = kPermissionsSectionContentMinWidth;
  if (site_data_content_)
    width = std::max(width, site_data_content_->GetPreferredSize().width());
  if (permissions_content_)
    width = std::max(width, permissions_content_->GetPreferredSize().width());
  width += kPermissionsSectionPaddingLeft;
  width = std::min(width, kMaxPopupWidth);

  return gfx::Size(width, height);
}

void WebsiteSettingsPopupView::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  site_data_content_->RemoveAllChildViews(true);

  views::GridLayout* layout = new views::GridLayout(site_data_content_);
  site_data_content_->SetLayoutManager(layout);

  const int site_data_content_column = 0;
  views::ColumnSet* column_set =
      layout->AddColumnSet(site_data_content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::FIXED,
                        kSiteDataIconColumnWidth,
                        0);
  column_set->AddPaddingColumn(0, kIconMarginLeft);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  layout->AddPaddingRow(1, 5);
  for (CookieInfoList::const_iterator i(cookie_info_list.begin());
       i != cookie_info_list.end();
       ++i) {
    base::string16 label_text = l10n_util::GetStringFUTF16(
        IDS_WEBSITE_SETTINGS_SITE_DATA_STATS_LINE,
        base::UTF8ToUTF16(i->cookie_source),
        base::IntToString16(i->allowed),
        base::IntToString16(i->blocked));
    if (i != cookie_info_list.begin())
      layout->AddPaddingRow(1, kSiteDataSectionRowSpacing);
    layout->StartRow(1, site_data_content_column);
    WebsiteSettingsUI::PermissionInfo info;
    info.type = CONTENT_SETTINGS_TYPE_COOKIES;
    info.setting = CONTENT_SETTING_ALLOW;
    views::ImageView* icon = new views::ImageView();
    const gfx::Image& image = WebsiteSettingsUI::GetPermissionIcon(info);
    icon->SetImage(image.ToImageSkia());
    layout->AddView(icon, 1, 1, views::GridLayout::CENTER,
                    views::GridLayout::CENTER);
    layout->AddView(new views::Label(label_text), 1, 1,
                    views::GridLayout::LEADING, views::GridLayout::CENTER);
  }
  layout->AddPaddingRow(1, 6);

  layout->Layout(site_data_content_);
  SizeToContents();
}

void WebsiteSettingsPopupView::SetPermissionInfo(
    const PermissionInfoList& permission_info_list) {
  permissions_content_ = new views::View();
  views::GridLayout* layout = new views::GridLayout(permissions_content_);
  permissions_content_->SetLayoutManager(layout);

  base::string16 headline =
      permission_info_list.empty()
          ? base::string16()
          : l10n_util::GetStringUTF16(
                IDS_WEBSITE_SETTINGS_TITLE_SITE_PERMISSIONS);
  views::View* permissions_section =
      CreateSection(headline, permissions_content_, nullptr);
  permissions_tab_->AddChildView(permissions_section);

  const int content_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  for (PermissionInfoList::const_iterator permission =
           permission_info_list.begin();
       permission != permission_info_list.end();
       ++permission) {
    layout->StartRow(1, content_column);
    PermissionSelectorView* selector = new PermissionSelectorView(
        web_contents_ ? web_contents_->GetURL() : GURL::EmptyGURL(),
        *permission);
    selector->AddObserver(this);
    layout->AddView(selector,
                    1,
                    1,
                    views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
    layout->AddPaddingRow(1, kPermissionsSectionRowSpacing);
  }

  layout->Layout(permissions_content_);

  // Add site settings link.
  site_settings_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_SETTINGS_LINK));
  site_settings_link_->set_listener(this);
  views::View* link_section = new views::View();
  const int kLinkMarginTop = 4;
  link_section->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           kConnectionSectionPaddingLeft, kLinkMarginTop, 0));
  link_section->AddChildView(site_settings_link_);
  permissions_tab_->AddChildView(link_section);

  SizeToContents();
}

void WebsiteSettingsPopupView::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  base::string16 identity_status_text = identity_info.GetIdentityStatusText();
  SkColor text_color = SK_ColorBLACK;
  if (identity_info.identity_status ==
          WebsiteSettings::SITE_IDENTITY_STATUS_CERT ||
      identity_info.identity_status ==
          WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT) {
    text_color = kIdentityVerifiedTextColor;
  }
  header_->SetIdentityName(base::UTF8ToUTF16(identity_info.site_identity));
  header_->SetIdentityStatus(identity_status_text, text_color);

  // The headline and the certificate dialog link of the site's identity
  // section is only displayed if the site's identity was verified. If the
  // site's identity was verified, then the headline contains the organization
  // name from the provided certificate. If the organization name is not
  // available than the hostname of the site is used instead.
  base::string16 headline;
  if (identity_info.cert_id) {
    cert_id_ = identity_info.cert_id;
    signed_certificate_timestamp_ids_.assign(
        identity_info.signed_certificate_timestamp_ids.begin(),
        identity_info.signed_certificate_timestamp_ids.end());

    certificate_dialog_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_PAGEINFO_CERT_INFO_BUTTON));
    certificate_dialog_link_->set_listener(this);

    if (!signed_certificate_timestamp_ids_.empty()) {
      signed_certificate_timestamps_link_ =
          new views::Link(l10n_util::GetStringUTF16(
              IDS_PAGEINFO_CERT_TRANSPARENCY_INFO_BUTTON));
      signed_certificate_timestamps_link_->set_listener(this);
    }

    if (identity_info.show_ssl_decision_revoke_button) {
      reset_decisions_button_ = new views::LabelButton(
          this,
          l10n_util::GetStringUTF16(
              IDS_PAGEINFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON));
      reset_decisions_button_->SetStyle(views::Button::STYLE_BUTTON);
    }

    headline = base::UTF8ToUTF16(identity_info.site_identity);
  }
  ResetConnectionSection(
      identity_info_content_,
      WebsiteSettingsUI::GetIdentityIcon(identity_info.identity_status),
      base::string16(),  // The identity section has no headline.
      base::UTF8ToUTF16(identity_info.identity_status_description),
      certificate_dialog_link_,
      signed_certificate_timestamps_link_,
      reset_decisions_button_);

  ResetConnectionSection(
      connection_info_content_,
      WebsiteSettingsUI::GetConnectionIcon(identity_info.connection_status),
      base::string16(),  // The connection section has no headline.
      base::UTF8ToUTF16(identity_info.connection_status_description),
      nullptr,
      nullptr,
      nullptr);

  connection_tab_->InvalidateLayout();
  Layout();
  SizeToContents();
}

void WebsiteSettingsPopupView::SetFirstVisit(
    const base::string16& first_visit) {
  ResetConnectionSection(
      page_info_content_,
      WebsiteSettingsUI::GetFirstVisitIcon(first_visit),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_INFO_TITLE),
      first_visit,
      nullptr,
      nullptr,
      nullptr);
  connection_tab_->InvalidateLayout();
  Layout();
  SizeToContents();
}

void WebsiteSettingsPopupView::SetSelectedTab(TabId tab_id) {
  tabbed_pane_->SelectTabAt(tab_id);
}

views::View* WebsiteSettingsPopupView::CreatePermissionsTab() {
  views::View* pane = new views::View();
  pane->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

  // Add cookies and site data section.
  cookie_dialog_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_SHOW_SITE_DATA));
  cookie_dialog_link_->set_listener(this);
  site_data_content_ = new views::View();
  views::View* site_data_section =
      CreateSection(l10n_util::GetStringUTF16(
                        IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA),
                    site_data_content_,
                    cookie_dialog_link_);
  pane->AddChildView(site_data_section);

  return pane;
}

views::View* WebsiteSettingsPopupView::CreateConnectionTab() {
  views::View* pane = new views::View();
  pane->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
  // Add site identity section.
  identity_info_content_ = new views::View();
  pane->AddChildView(identity_info_content_);

  // Add connection section.
  pane->AddChildView(new views::Separator(views::Separator::HORIZONTAL));
  connection_info_content_ = new views::View();
  pane->AddChildView(connection_info_content_);

  // Add page info section.
  pane->AddChildView(new views::Separator(views::Separator::HORIZONTAL));
  page_info_content_ = new views::View();
  pane->AddChildView(page_info_content_);

  // Add help center link.
  pane->AddChildView(new views::Separator(views::Separator::HORIZONTAL));
  help_center_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_HELP_CENTER_LINK));
  help_center_link_->set_listener(this);
  views::View* link_section = new views::View();
  const int kLinkMarginTop = 4;
  link_section->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           kConnectionSectionPaddingLeft,
                           kLinkMarginTop,
                           0));
  link_section->AddChildView(help_center_link_);
  pane->AddChildView(link_section);
  return pane;
}

views::View* WebsiteSettingsPopupView::CreateSection(
    const base::string16& headline_text,
    views::View* content,
    views::Link* link) {
  views::View* container = new views::View();
  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);
  const int content_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(content_column);
  column_set->AddPaddingColumn(0, kPermissionsSectionPaddingLeft);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  if (headline_text.length() > 0) {
    layout->AddPaddingRow(1, kPermissionsSectionPaddingTop);
    layout->StartRow(1, content_column);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    views::Label* headline = new views::Label(
        headline_text, rb.GetFontList(ui::ResourceBundle::BoldFont));
    layout->AddView(headline, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
  }

  layout->AddPaddingRow(1, kPermissionsSectionHeadlineMarginBottom);
  layout->StartRow(1, content_column);
  layout->AddView(content, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);

  if (link) {
    layout->AddPaddingRow(1, 4);
    layout->StartRow(1, content_column);
    layout->AddView(link, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
  }

  layout->AddPaddingRow(1, kPermissionsSectionPaddingBottom);
  return container;
}

void WebsiteSettingsPopupView::ResetConnectionSection(
    views::View* section_container,
    const gfx::Image& icon,
    const base::string16& headline,
    const base::string16& text,
    views::Link* link,
    views::Link* secondary_link,
    views::LabelButton* reset_decisions_button) {
  section_container->RemoveAllChildViews(true);

  views::GridLayout* layout = new views::GridLayout(section_container);
  section_container->SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(0, kConnectionSectionPaddingLeft);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, kIconMarginLeft);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, kConnectionSectionPaddingRight);


  layout->AddPaddingRow(0, kConnectionSectionPaddingTop);
  layout->StartRow(1, 0);

  // Add status icon.
  views::ImageView* icon_view = new views::ImageView();
  icon_view->SetImage(*icon.ToImageSkia());
  layout->AddView(icon_view, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::LEADING);

  // Add section content.
  views::View* content_pane = new views::View();
  views::GridLayout* content_layout = new views::GridLayout(content_pane);
  content_pane->SetLayoutManager(content_layout);
  views::ColumnSet* content_column_set = content_layout->AddColumnSet(0);
  content_column_set->AddColumn(views::GridLayout::LEADING,
                                views::GridLayout::LEADING,
                                1,
                                views::GridLayout::USE_PREF,
                                0,
                                0);
  if (!headline.empty()) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    views::Label* headline_label = new views::Label(
        headline, rb.GetFontList(ui::ResourceBundle::BoldFont));
    headline_label->SetMultiLine(true);
    headline_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    // Allow linebreaking in the middle of words if necessary, so that extremely
    // long hostnames (longer than one line) will still be completely shown.
    headline_label->SetAllowCharacterBreak(true);
    content_layout->StartRow(1, 0);
    content_layout->AddView(headline_label);
  }

  views::Label* description_label = new views::Label(text);
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description_label->SetAllowCharacterBreak(true);
  content_layout->StartRow(1, 0);
  content_layout->AddView(description_label);

  if (link) {
    content_layout->StartRow(1, 0);
    content_layout->AddView(link);
  }

  if (secondary_link) {
    content_layout->StartRow(1, 0);
    content_layout->AddView(secondary_link);
  }

  if (reset_decisions_button) {
    content_layout->StartRow(1, 0);
    content_layout->AddView(reset_decisions_button);
  }

  layout->AddView(content_pane, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::LEADING);
  layout->AddPaddingRow(0, kConnectionSectionPaddingBottom);
}

void WebsiteSettingsPopupView::HandleLinkClickedAsync(views::Link* source) {
  if (source == cookie_dialog_link_) {
    // Count how often the Collected Cookies dialog is opened.
    presenter_->RecordWebsiteSettingsAction(
        WebsiteSettings::WEBSITE_SETTINGS_COOKIES_DIALOG_OPENED);

    if (web_contents_ != NULL)
      new CollectedCookiesViews(web_contents_);
  } else if (source == certificate_dialog_link_) {
    gfx::NativeWindow parent = GetAnchorView() ?
        GetAnchorView()->GetWidget()->GetNativeWindow() : nullptr;
    presenter_->RecordWebsiteSettingsAction(
        WebsiteSettings::WEBSITE_SETTINGS_CERTIFICATE_DIALOG_OPENED);
    ShowCertificateViewerByID(web_contents_, parent, cert_id_);
  } else if (source == signed_certificate_timestamps_link_) {
    chrome::ShowSignedCertificateTimestampsViewer(
        web_contents_, signed_certificate_timestamp_ids_);
    presenter_->RecordWebsiteSettingsAction(
        WebsiteSettings::WEBSITE_SETTINGS_TRANSPARENCY_VIEWER_OPENED);
  } else if (source == help_center_link_) {
    browser_->OpenURL(
        content::OpenURLParams(GURL(chrome::kPageInfoHelpCenterURL),
                               content::Referrer(),
                               NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               false));
    presenter_->RecordWebsiteSettingsAction(
        WebsiteSettings::WEBSITE_SETTINGS_CONNECTION_HELP_OPENED);
  } else if (source == site_settings_link_) {
    // TODO(palmer): This opens the general Content Settings pane, which is OK
    // for now. But on Android, it opens a page specific to a given origin that
    // shows all of the settings for that origin. If/when that's available on
    // desktop we should link to that here, too.
    browser_->OpenURL(content::OpenURLParams(
        GURL(chrome::kChromeUIContentSettingsURL), content::Referrer(),
        NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK, false));
    presenter_->RecordWebsiteSettingsAction(
        WebsiteSettings::WEBSITE_SETTINGS_SITE_SETTINGS_OPENED);
  } else {
    NOTREACHED();
  }
}
