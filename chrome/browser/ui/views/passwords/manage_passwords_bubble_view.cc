// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/browser/ui/views/passwords/credentials_selection_view.h"
#include "chrome/browser/ui/views/passwords/manage_credential_item_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_view_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

int ManagePasswordsBubbleView::auto_signin_toast_timeout_ = 3;

// Helpers --------------------------------------------------------------------

namespace {

const int kDesiredBubbleWidth = 370;

enum ColumnSetType {
  // | | (FILL, FILL) | |
  // Used for the bubble's header, the credentials list, and for simple
  // messages like "No passwords".
  SINGLE_VIEW_COLUMN_SET,

  // | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the bubble which should nest at the
  // bottom-right corner.
  DOUBLE_BUTTON_COLUMN_SET,

  // | | (LEADING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the bubble which should occupy
  // the corners.
  LINK_BUTTON_COLUMN_SET,

  // | | (TRAILING, CENTER) | |
  // Used when there is only one button which should next at the bottom-right
  // corner.
  SINGLE_BUTTON_COLUMN_SET,

  // | | (LEADING, CENTER) | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
  // Used when there are three buttons.
  TRIPLE_BUTTON_COLUMN_SET,
};

enum TextRowType { ROW_SINGLE, ROW_MULTILINE };

// Construct an appropriate ColumnSet for the given |type|, and add it
// to |layout|.
void BuildColumnSet(views::GridLayout* layout, ColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
  int full_width = kDesiredBubbleWidth - (2 * views::kPanelHorizMargin);
  switch (type) {
    case SINGLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            0,
                            views::GridLayout::FIXED,
                            full_width,
                            0);
      break;
    case DOUBLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case LINK_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case SINGLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case TRIPLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
  }
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
}

// Given a layout and a model, add an appropriate title using a
// SINGLE_VIEW_COLUMN_SET, followed by a spacer row.
void AddTitleRow(views::GridLayout* layout, ManagePasswordsBubbleModel* model) {
  views::Label* title_label = new views::Label(model->title());
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));

  // Add the title to the layout with appropriate padding.
  layout->StartRowWithPadding(
      0, SINGLE_VIEW_COLUMN_SET, 0, views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

}  // namespace


// ManagePasswordsBubbleView::AccountChooserView ------------------------------

// A view offering the user the ability to choose credentials for
// authentication. Contains a list of CredentialsItemView, along with a
// "Cancel" button.
class ManagePasswordsBubbleView::AccountChooserView
    : public views::View,
      public views::ButtonListener {
 public:
  explicit AccountChooserView(ManagePasswordsBubbleView* parent);
  ~AccountChooserView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Adds |password_forms| to the layout remembering their |type|.
  void AddCredentialItemsWithType(
      views::GridLayout* layout,
      const ScopedVector<const autofill::PasswordForm>& password_forms,
      password_manager::CredentialType type);

  ManagePasswordsBubbleView* parent_;
  views::LabelButton* cancel_button_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserView);
};

ManagePasswordsBubbleView::AccountChooserView::AccountChooserView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  cancel_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button_->SetStyle(views::Button::STYLE_BUTTON);
  cancel_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  AddCredentialItemsWithType(
      layout, parent_->model()->local_credentials(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  AddCredentialItemsWithType(
      layout, parent_->model()->federated_credentials(),
      password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED);

  // Button row.
  BuildColumnSet(layout, SINGLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, SINGLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(cancel_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(cancel_button_);
}

ManagePasswordsBubbleView::AccountChooserView::~AccountChooserView() {
}

void ManagePasswordsBubbleView::AccountChooserView::AddCredentialItemsWithType(
    views::GridLayout* layout,
    const ScopedVector<const autofill::PasswordForm>& password_forms,
    password_manager::CredentialType type) {
  net::URLRequestContextGetter* request_context =
      parent_->model()->GetProfile()->GetRequestContext();
  for (const autofill::PasswordForm* form : password_forms) {
    // Add the title to the layout with appropriate padding.
    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(new CredentialsItemView(
        this, form, type, CredentialsItemView::ACCOUNT_CHOOSER,
        request_context));
  }
}

void ManagePasswordsBubbleView::AccountChooserView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  if (sender != cancel_button_) {
    // ManagePasswordsBubbleModel should care about calling a callback in case
    // the bubble is dismissed by any other means.
    CredentialsItemView* view = static_cast<CredentialsItemView*>(sender);
    parent_->model()->OnChooseCredentials(*view->form(),
                                          view->credential_type());
  } else {
    parent_->model()->OnCancelClicked();
  }
  parent_->Close();
}

// ManagePasswordsBubbleView::AutoSigninView ----------------------------------

// A view containing just one credential that was used for for automatic signing
// in.
class ManagePasswordsBubbleView::AutoSigninView
    : public views::View,
      public views::ButtonListener,
      public views::WidgetObserver {
 public:
  explicit AutoSigninView(ManagePasswordsBubbleView* parent);

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::WidgetObserver:
  // Tracks the state of the browser window.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetClosing(views::Widget* widget) override;

  void OnTimer();
  static base::TimeDelta GetTimeout() {
    return base::TimeDelta::FromSeconds(
        ManagePasswordsBubbleView::auto_signin_toast_timeout_);
  }

  base::OneShotTimer<AutoSigninView> timer_;
  ManagePasswordsBubbleView* parent_;
  ScopedObserver<views::Widget, views::WidgetObserver> observed_browser_;

  DISALLOW_COPY_AND_ASSIGN(AutoSigninView);
};

ManagePasswordsBubbleView::AutoSigninView::AutoSigninView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent),
      observed_browser_(this) {
  SetLayoutManager(new views::FillLayout);
  CredentialsItemView* credential = new CredentialsItemView(
      this, &parent_->model()->pending_password(),
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      CredentialsItemView::AUTO_SIGNIN,
      parent_->model()->GetProfile()->GetRequestContext());
  AddChildView(credential);
  parent_->set_initially_focused_view(credential);

  Browser* browser =
      chrome::FindBrowserWithWebContents(parent_->web_contents());
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  observed_browser_.Add(browser_view->GetWidget());

  if (browser_view->IsActive())
    timer_.Start(FROM_HERE, GetTimeout(), this, &AutoSigninView::OnTimer);
}

void ManagePasswordsBubbleView::AutoSigninView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  parent_->model()->OnAutoSignInClicked();
  parent_->Close();
}

void ManagePasswordsBubbleView::AutoSigninView::OnWidgetActivationChanged(
    views::Widget* widget, bool active) {
  if (active && !timer_.IsRunning())
    timer_.Start(FROM_HERE, GetTimeout(), this, &AutoSigninView::OnTimer);
}

void ManagePasswordsBubbleView::AutoSigninView::OnWidgetClosing(
    views::Widget* widget) {
  observed_browser_.RemoveAll();
}

void ManagePasswordsBubbleView::AutoSigninView::OnTimer() {
  parent_->model()->OnAutoSignInToastTimeout();
  parent_->Close();
}

// ManagePasswordsBubbleView::PendingView -------------------------------------

// A view offering the user the ability to save credentials. Contains a
// single ManagePasswordItemsView, along with a "Save Passwords" button
// and a "Never" button.
class ManagePasswordsBubbleView::PendingView
    : public views::View,
      public views::ButtonListener,
      public views::StyledLabelListener {
 public:
  explicit PendingView(ManagePasswordsBubbleView* parent);
  ~PendingView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(const gfx::Range& range,
                              int event_flags) override;

  ManagePasswordsBubbleView* parent_;

  views::BlueButton* save_button_;
  views::LabelButton* never_button_;

  DISALLOW_COPY_AND_ASSIGN(PendingView);
};

ManagePasswordsBubbleView::PendingView::PendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Create the pending credential item, save button and refusal combobox.
  ManagePasswordItemsView* item = nullptr;
  if (!parent->model()->pending_password().username_value.empty()) {
    std::vector<const autofill::PasswordForm*> credentials(
        1, &parent->model()->pending_password());
    item = new ManagePasswordItemsView(parent_->model(), credentials);
  }
  save_button_ = new views::BlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  save_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  never_button_ = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BUBBLE_BLACKLIST_BUTTON1));
  never_button_->SetStyle(views::Button::STYLE_BUTTON);
  never_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));

  // Title row.
  views::StyledLabel* title_label =
      new views::StyledLabel(parent_->model()->title(), this);
  title_label->SetBaseFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::MediumFont));
  if (!parent_->model()->title_brand_link_range().is_empty()) {
    title_label->AddStyleRange(
        parent_->model()->title_brand_link_range(),
        views::StyledLabel::RangeStyleInfo::CreateForLink());
  }
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Credential row.
  if (item) {
    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(item);
  }

  // Button row.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, DOUBLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(save_button_);
  layout->AddView(never_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(save_button_);
}

ManagePasswordsBubbleView::PendingView::~PendingView() {
}

void ManagePasswordsBubbleView::PendingView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  if (sender == save_button_)
    parent_->model()->OnSaveClicked();
  else if (sender == never_button_)
    parent_->model()->OnNeverForThisSiteClicked();
  else
    NOTREACHED();

  parent_->Close();
}

void ManagePasswordsBubbleView::PendingView::StyledLabelLinkClicked(
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(range, parent_->model()->title_brand_link_range());
  parent_->model()->OnBrandLinkClicked();
}

// ManagePasswordsBubbleView::ManageView --------------------------------------

// A view offering the user a list of her currently saved credentials
// for the current page, along with a "Manage passwords" link and a
// "Done" button.
class ManagePasswordsBubbleView::ManageView : public views::View,
                                              public views::ButtonListener,
                                              public views::LinkListener {
 public:
  explicit ManageView(ManagePasswordsBubbleView* parent);
  ~ManageView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  ManagePasswordsBubbleView* parent_;

  views::Link* manage_link_;
  views::LabelButton* done_button_;

  DISALLOW_COPY_AND_ASSIGN(ManageView);
};

ManagePasswordsBubbleView::ManageView::ManageView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Add the title.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  // If we have a list of passwords to store for the current site, display
  // them to the user for management. Otherwise, render a "No passwords for
  // this site" message.
  if (!parent_->model()->local_credentials().empty()) {
    ManagePasswordItemsView* item = new ManagePasswordItemsView(
        parent_->model(), parent_->model()->local_credentials().get());
    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(item);
  } else {
    views::Label* empty_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NO_PASSWORDS));
    empty_label->SetMultiLine(true);
    empty_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    empty_label->SetFontList(
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::SmallFont));

    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(empty_label);
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  }

  // Then add the "manage passwords" link and "Done" button.
  manage_link_ = new views::Link(parent_->model()->manage_link());
  manage_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  manage_link_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  manage_link_->SetUnderline(false);
  manage_link_->set_listener(this);

  done_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
  done_button_->SetStyle(views::Button::STYLE_BUTTON);
  done_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, LINK_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, LINK_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(manage_link_);
  layout->AddView(done_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(done_button_);
}

ManagePasswordsBubbleView::ManageView::~ManageView() {
}

void ManagePasswordsBubbleView::ManageView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == done_button_);
  parent_->model()->OnDoneClicked();
  parent_->Close();
}

void ManagePasswordsBubbleView::ManageView::LinkClicked(views::Link* source,
                                                        int event_flags) {
  DCHECK_EQ(source, manage_link_);
  parent_->model()->OnManageLinkClicked();
  parent_->Close();
}

// ManagePasswordsBubbleView::ManageAccountsView ------------------------------

// A view offering the user a list of his currently saved through the Credential
// Manager API accounts for the current page.
class ManagePasswordsBubbleView::ManageAccountsView
    : public views::View,
      public views::ButtonListener,
      public views::LinkListener {
 public:
  explicit ManageAccountsView(ManagePasswordsBubbleView* parent);

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  ManagePasswordsBubbleView* parent_;

  views::Link* manage_link_;
  views::LabelButton* done_button_;

  DISALLOW_COPY_AND_ASSIGN(ManageAccountsView);
};

ManagePasswordsBubbleView::ManageAccountsView::ManageAccountsView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Add the title.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  if (!parent_->model()->local_credentials().empty()) {
    for (const autofill::PasswordForm* form :
         parent_->model()->local_credentials()) {
      layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
      layout->AddView(new ManageCredentialItemView(parent_->model(), form));
    }
  } else {
    views::Label* empty_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NO_PASSWORDS),
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::SmallFont));
    empty_label->SetMultiLine(true);
    empty_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(empty_label);
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  }

  // Then add the "manage passwords" link and "Done" button.
  manage_link_ = new views::Link(parent_->model()->manage_link());
  manage_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  manage_link_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  manage_link_->SetUnderline(false);
  manage_link_->set_listener(this);

  done_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
  done_button_->SetStyle(views::Button::STYLE_BUTTON);
  done_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, LINK_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, LINK_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(manage_link_);
  layout->AddView(done_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(done_button_);
}

void ManagePasswordsBubbleView::ManageAccountsView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK(sender == done_button_);
  parent_->model()->OnDoneClicked();
  parent_->Close();
}

void ManagePasswordsBubbleView::ManageAccountsView::LinkClicked(
    views::Link* source, int event_flags) {
  DCHECK_EQ(source, manage_link_);
  parent_->model()->OnManageLinkClicked();
  parent_->Close();
}

// ManagePasswordsBubbleView::SaveConfirmationView ----------------------------

// A view confirming to the user that a password was saved and offering a link
// to the Google account manager.
class ManagePasswordsBubbleView::SaveConfirmationView
    : public views::View,
      public views::ButtonListener,
      public views::StyledLabelListener {
 public:
  explicit SaveConfirmationView(ManagePasswordsBubbleView* parent);
  ~SaveConfirmationView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener implementation
  void StyledLabelLinkClicked(const gfx::Range& range,
                              int event_flags) override;

  ManagePasswordsBubbleView* parent_;
  views::LabelButton* ok_button_;

  DISALLOW_COPY_AND_ASSIGN(SaveConfirmationView);
};

ManagePasswordsBubbleView::SaveConfirmationView::SaveConfirmationView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  views::StyledLabel* confirmation =
      new views::StyledLabel(parent_->model()->save_confirmation_text(), this);
  confirmation->SetBaseFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));
  confirmation->AddStyleRange(
      parent_->model()->save_confirmation_link_range(),
      views::StyledLabel::RangeStyleInfo::CreateForLink());

  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(confirmation);

  ok_button_ = new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_OK));
  ok_button_->SetStyle(views::Button::STYLE_BUTTON);
  ok_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, SINGLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, SINGLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(ok_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(ok_button_);
}

ManagePasswordsBubbleView::SaveConfirmationView::~SaveConfirmationView() {
}

void ManagePasswordsBubbleView::SaveConfirmationView::StyledLabelLinkClicked(
    const gfx::Range& range, int event_flags) {
  DCHECK_EQ(range, parent_->model()->save_confirmation_link_range());
  parent_->model()->OnManageLinkClicked();
  parent_->Close();
}

void ManagePasswordsBubbleView::SaveConfirmationView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(sender, ok_button_);
  parent_->model()->OnOKClicked();
  parent_->Close();
}

// ManagePasswordsBubbleView::WebContentMouseHandler --------------------------

// The class listens for WebContentsView events and notifies the bubble if the
// view was clicked on or received keystrokes.
class ManagePasswordsBubbleView::WebContentMouseHandler
    : public ui::EventHandler {
 public:
  explicit WebContentMouseHandler(ManagePasswordsBubbleView* bubble);

  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  ManagePasswordsBubbleView* bubble_;
  scoped_ptr<views::EventMonitor> event_monitor_;

  DISALLOW_COPY_AND_ASSIGN(WebContentMouseHandler);
};

ManagePasswordsBubbleView::WebContentMouseHandler::WebContentMouseHandler(
    ManagePasswordsBubbleView* bubble)
    : bubble_(bubble) {
  content::WebContents* web_contents = bubble_->web_contents();
  DCHECK(web_contents);
  event_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, web_contents->GetTopLevelNativeWindow());
}

void ManagePasswordsBubbleView::WebContentMouseHandler::OnKeyEvent(
    ui::KeyEvent* event) {
  content::WebContents* web_contents = bubble_->web_contents();
  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  if ((event->key_code() == ui::VKEY_ESCAPE ||
       rvh->IsFocusedElementEditable()) && event->type() == ui::ET_KEY_PRESSED)
    bubble_->Close();
}

void ManagePasswordsBubbleView::WebContentMouseHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    bubble_->Close();
}

void ManagePasswordsBubbleView::WebContentMouseHandler::OnTouchEvent(
    ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    bubble_->Close();
}

// ManagePasswordsBubbleView::UpdatePendingView -------------------------------

// A view offering the user the ability to update credentials. Contains a
// single ManagePasswordItemsView (in case of one credentials) or
// CredentialsSelectionView otherwise, along with a "Update Passwords" button
// and a rejection button.
class ManagePasswordsBubbleView::UpdatePendingView
    : public views::View,
      public views::ButtonListener,
      public views::StyledLabelListener {
 public:
  explicit UpdatePendingView(ManagePasswordsBubbleView* parent);
  ~UpdatePendingView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(const gfx::Range& range,
                              int event_flags) override;

  ManagePasswordsBubbleView* parent_;

  CredentialsSelectionView* selection_view_;

  views::BlueButton* update_button_;

  views::LabelButton* nope_button_;

  DISALLOW_COPY_AND_ASSIGN(UpdatePendingView);
};

ManagePasswordsBubbleView::UpdatePendingView::UpdatePendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent), selection_view_(nullptr) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Create the pending credential item, update button.
  View* item = nullptr;
  if (parent->model()->ShouldShowMultipleAccountUpdateUI()) {
    selection_view_ = new CredentialsSelectionView(
        parent->model(), parent->model()->local_credentials().get(),
        parent->model()->pending_password().username_value);
    item = selection_view_;
  } else {
    std::vector<const autofill::PasswordForm*> forms;
    forms.push_back(&parent->model()->pending_password());
    item = new ManagePasswordItemsView(parent_->model(), forms);
  }
  nope_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));
  nope_button_->SetStyle(views::Button::STYLE_BUTTON);
  nope_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  update_button_ = new views::BlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON));
  update_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));

  // Title row.
  views::StyledLabel* title_label =
      new views::StyledLabel(parent_->model()->title(), this);
  title_label->SetBaseFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::MediumFont));
  if (!parent_->model()->title_brand_link_range().is_empty()) {
    title_label->AddStyleRange(
        parent_->model()->title_brand_link_range(),
        views::StyledLabel::RangeStyleInfo::CreateForLink());
  }
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Credential row.
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(item);

  // Button row.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(0, DOUBLE_BUTTON_COLUMN_SET, 0,
                              views::kUnrelatedControlVerticalSpacing);
  layout->AddView(update_button_);
  layout->AddView(nope_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(update_button_);
}

ManagePasswordsBubbleView::UpdatePendingView::~UpdatePendingView() {}

void ManagePasswordsBubbleView::UpdatePendingView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == update_button_ || sender == nope_button_);
  if (sender == update_button_) {
    if (selection_view_) {
      // Multi account case.
      parent_->model()->OnUpdateClicked(
          *selection_view_->GetSelectedCredentials());
    } else {
      parent_->model()->OnUpdateClicked(parent_->model()->pending_password());
    }
  } else {
    parent_->model()->OnNopeUpdateClicked();
  }
  parent_->Close();
}

void ManagePasswordsBubbleView::UpdatePendingView::StyledLabelLinkClicked(
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(range, parent_->model()->title_brand_link_range());
  parent_->model()->OnBrandLinkClicked();
}

// ManagePasswordsBubbleView --------------------------------------------------

// static
ManagePasswordsBubbleView* ManagePasswordsBubbleView::manage_passwords_bubble_ =
    NULL;

// static
void ManagePasswordsBubbleView::ShowBubble(content::WebContents* web_contents,
                                           DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(!manage_passwords_bubble_ ||
         !manage_passwords_bubble_->GetWidget()->IsVisible());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  ManagePasswordsIconView* anchor_view =
      is_fullscreen
          ? NULL
          : browser_view->GetLocationBarView()->manage_passwords_icon_view();
  manage_passwords_bubble_ = new ManagePasswordsBubbleView(
      web_contents, anchor_view, reason);

  if (is_fullscreen)
    manage_passwords_bubble_->set_parent_window(web_contents->GetNativeView());

  views::BubbleDelegateView::CreateBubble(manage_passwords_bubble_);

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen) {
    manage_passwords_bubble_->AdjustForFullscreen(
        browser_view->GetBoundsInScreen());
  }
  if (reason == AUTOMATIC)
    manage_passwords_bubble_->GetWidget()->ShowInactive();
  else
    manage_passwords_bubble_->GetWidget()->Show();
}

// static
void ManagePasswordsBubbleView::CloseBubble() {
  if (manage_passwords_bubble_)
    manage_passwords_bubble_->Close();
}

// static
void ManagePasswordsBubbleView::ActivateBubble() {
  DCHECK(manage_passwords_bubble_);
  DCHECK(manage_passwords_bubble_->GetWidget()->IsVisible());
  manage_passwords_bubble_->GetWidget()->Activate();
}

content::WebContents* ManagePasswordsBubbleView::web_contents() const {
  return model()->web_contents();
}

ManagePasswordsBubbleView::ManagePasswordsBubbleView(
    content::WebContents* web_contents,
    ManagePasswordsIconView* anchor_view,
    DisplayReason reason)
    : ManagePasswordsBubble(web_contents, reason),
      ManagedFullScreenBubbleDelegateView(anchor_view, web_contents),
      anchor_view_(anchor_view),
      initially_focused_view_(NULL) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
  if (anchor_view)
    anchor_view->SetActive(true);
  mouse_handler_.reset(new WebContentMouseHandler(this));
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() {
  if (manage_passwords_bubble_ == this)
    manage_passwords_bubble_ = NULL;
}

views::View* ManagePasswordsBubbleView::GetInitiallyFocusedView() {
  return initially_focused_view_;
}

void ManagePasswordsBubbleView::Init() {
  views::FillLayout* layout = new views::FillLayout();
  SetLayoutManager(layout);

  Refresh();
}

void ManagePasswordsBubbleView::Close() {
  mouse_handler_.reset();
  ManagedFullScreenBubbleDelegateView::Close();
}

void ManagePasswordsBubbleView::OnWidgetClosing(views::Widget* /*widget*/) {
  if (anchor_view_)
    anchor_view_->SetActive(false);
}

bool ManagePasswordsBubbleView::ShouldShowCloseButton() const {
  return model()->state() == password_manager::ui::PENDING_PASSWORD_STATE;
}

void ManagePasswordsBubbleView::Refresh() {
  RemoveAllChildViews(true);
  initially_focused_view_ = NULL;
  if (model()->state() == password_manager::ui::PENDING_PASSWORD_STATE) {
    AddChildView(new PendingView(this));
  } else if (model()->state() ==
             password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    AddChildView(new UpdatePendingView(this));
  } else if (model()->state() == password_manager::ui::CONFIRMATION_STATE) {
    AddChildView(new SaveConfirmationView(this));
  } else if (model()->state() ==
                 password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    AddChildView(new AccountChooserView(this));
  } else if (model()->state() == password_manager::ui::AUTO_SIGNIN_STATE) {
    AddChildView(new AutoSigninView(this));
  } else {
    if (model()->IsNewUIActive())
      AddChildView(new ManageAccountsView(this));
    else
      AddChildView(new ManageView(this));
  }
  GetLayoutManager()->Layout(this);
}
