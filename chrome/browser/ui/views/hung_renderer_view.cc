// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"

#if defined(OS_WIN)
#include "chrome/browser/hang_monitor/hang_crash_dump_win.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using content::WebContents;

HungRendererDialogView* HungRendererDialogView::g_instance_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, public:

HungPagesTableModel::HungPagesTableModel(Delegate* delegate)
    : observer_(NULL),
      delegate_(delegate) {
}

HungPagesTableModel::~HungPagesTableModel() {
}

content::RenderProcessHost* HungPagesTableModel::GetRenderProcessHost() {
  return tab_observers_.empty() ? NULL :
      tab_observers_[0]->web_contents()->GetRenderProcessHost();
}

content::RenderViewHost* HungPagesTableModel::GetRenderViewHost() {
  return tab_observers_.empty() ? NULL :
      tab_observers_[0]->web_contents()->GetRenderViewHost();
}

void HungPagesTableModel::InitForWebContents(WebContents* hung_contents) {
  tab_observers_.clear();
  if (hung_contents) {
    // Force hung_contents to be first.
    if (hung_contents) {
      tab_observers_.push_back(new WebContentsObserverImpl(this,
                                                           hung_contents));
    }
    for (TabContentsIterator it; !it.done(); it.Next()) {
      if (*it != hung_contents &&
          it->GetRenderProcessHost() == hung_contents->GetRenderProcessHost())
        tab_observers_.push_back(new WebContentsObserverImpl(this, *it));
    }
  }
  // The world is different.
  if (observer_)
    observer_->OnModelChanged();
}

///////////////////////////////////////////////////////////////////////////////
// HungPagesTableModel, ui::TableModel implementation:

int HungPagesTableModel::RowCount() {
  return static_cast<int>(tab_observers_.size());
}

base::string16 HungPagesTableModel::GetText(int row, int column_id) {
  DCHECK(row >= 0 && row < RowCount());
  base::string16 title = tab_observers_[row]->web_contents()->GetTitle();
  if (title.empty())
    title = CoreTabHelper::GetDefaultTitle();
  // TODO(xji): Consider adding a special case if the title text is a URL,
  // since those should always have LTR directionality. Please refer to
  // http://crbug.com/6726 for more information.
  base::i18n::AdjustStringForLocaleDirection(&title);
  return title;
}

gfx::ImageSkia HungPagesTableModel::GetIcon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return favicon::ContentFaviconDriver::FromWebContents(
             tab_observers_[row]->web_contents())
      ->GetFavicon()
      .AsImageSkia();
}

void HungPagesTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

void HungPagesTableModel::GetGroupRange(int model_index,
                                        views::GroupRange* range) {
  DCHECK(range);
  range->start = 0;
  range->length = RowCount();
}

void HungPagesTableModel::TabDestroyed(WebContentsObserverImpl* tab) {
  // Clean up tab_observers_ and notify our observer.
  TabObservers::iterator i = std::find(
      tab_observers_.begin(), tab_observers_.end(), tab);
  DCHECK(i != tab_observers_.end());
  int index = static_cast<int>(i - tab_observers_.begin());
  tab_observers_.erase(i);
  if (observer_)
    observer_->OnItemsRemoved(index, 1);

  // Notify the delegate.
  delegate_->TabDestroyed();
  // WARNING: we've likely been deleted.
}

HungPagesTableModel::WebContentsObserverImpl::WebContentsObserverImpl(
    HungPagesTableModel* model, WebContents* tab)
    : content::WebContentsObserver(tab),
      model_(model) {
}

void HungPagesTableModel::WebContentsObserverImpl::RenderProcessGone(
    base::TerminationStatus status) {
  model_->TabDestroyed(this);
}

void HungPagesTableModel::WebContentsObserverImpl::WebContentsDestroyed() {
  model_->TabDestroyed(this);
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView

// static
gfx::ImageSkia* HungRendererDialogView::frozen_icon_ = NULL;

// The dimensions of the hung pages list table view, in pixels.
static const int kTableViewWidth = 300;
static const int kTableViewHeight = 100;

// Padding space in pixels between frozen icon to the info label, hung pages
// list table view and the Kill pages button.
static const int kCentralColumnPadding =
    views::kUnrelatedControlLargeHorizontalSpacing;

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, public:

// static
HungRendererDialogView* HungRendererDialogView::Create(
    gfx::NativeWindow context) {
  if (!g_instance_) {
    g_instance_ = new HungRendererDialogView;
    views::DialogDelegate::CreateDialogWidget(g_instance_, context, NULL);
  }
  return g_instance_;
}

// static
HungRendererDialogView* HungRendererDialogView::GetInstance() {
  return g_instance_;
}

// static
void HungRendererDialogView::Show(WebContents* contents) {
  if (logging::DialogsAreSuppressed())
    return;

  gfx::NativeWindow window =
      platform_util::GetTopLevel(contents->GetNativeView());
#if defined(USE_AURA)
  // Don't show the dialog if there is no root window for the renderer, because
  // it's invisible to the user (happens when the renderer is for prerendering
  // for example).
  if (!window->GetRootWindow())
    return;
#endif
  HungRendererDialogView* view = HungRendererDialogView::Create(window);
  view->ShowForWebContents(contents);
}

// static
void HungRendererDialogView::Hide(WebContents* contents) {
  if (!logging::DialogsAreSuppressed() && HungRendererDialogView::GetInstance())
    HungRendererDialogView::GetInstance()->EndForWebContents(contents);
}

// static
bool HungRendererDialogView::IsFrameActive(WebContents* contents) {
  gfx::NativeWindow window =
      platform_util::GetTopLevel(contents->GetNativeView());
  return platform_util::IsWindowActive(window);
}

HungRendererDialogView::HungRendererDialogView()
    : info_label_(nullptr),
      hung_pages_table_(nullptr),
      kill_button_(nullptr),
      initialized_(false),
      kill_button_clicked_(false) {
  InitClass();
}

HungRendererDialogView::~HungRendererDialogView() {
  hung_pages_table_->SetModel(NULL);
}

void HungRendererDialogView::ShowForWebContents(WebContents* contents) {
  DCHECK(contents && GetWidget());

  // Don't show the warning unless the foreground window is the frame, or this
  // window (but still invisible). If the user has another window or
  // application selected, activating ourselves is rude.
  if (!IsFrameActive(contents) &&
      !platform_util::IsWindowActive(GetWidget()->GetNativeWindow()))
    return;

  if (!GetWidget()->IsActive()) {
    // Place the dialog over content's browser window, similar to modal dialogs.
    Browser* browser = chrome::FindBrowserWithWebContents(contents);
    if (browser) {
      ChromeWebModalDialogManagerDelegate* manager = browser;
      constrained_window::UpdateWidgetModalDialogPosition(
          GetWidget(), manager->GetWebContentsModalDialogHost());
    }

    gfx::NativeWindow window =
        platform_util::GetTopLevel(contents->GetNativeView());
    views::Widget* insert_after =
        views::Widget::GetWidgetForNativeWindow(window);
    if (insert_after)
      GetWidget()->StackAboveWidget(insert_after);

#if defined(OS_WIN)
    // Group the hung renderer dialog with the browsers with the same profile.
    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    ui::win::SetAppIdForWindow(
        shell_integration::win::GetChromiumModelIdForProfile(
            profile->GetPath()),
        views::HWNDForWidget(GetWidget()));
#endif

    // We only do this if the window isn't active (i.e. hasn't been shown yet,
    // or is currently shown but deactivated for another WebContents). This is
    // because this window is a singleton, and it's possible another active
    // renderer may hang while this one is showing, and we don't want to reset
    // the list of hung pages for a potentially unrelated renderer while this
    // one is showing.
    hung_pages_table_model_->InitForWebContents(contents);

    info_label_->SetText(
        l10n_util::GetPluralStringFUTF16(IDS_BROWSER_HANGMONITOR_RENDERER,
                                         hung_pages_table_model_->RowCount()));
    Layout();

    // Make Widget ask for the window title again.
    GetWidget()->UpdateWindowTitle();

    GetWidget()->Show();
  }
}

void HungRendererDialogView::EndForWebContents(WebContents* contents) {
  DCHECK(contents);
  if (hung_pages_table_model_->RowCount() == 0 ||
      hung_pages_table_model_->GetRenderProcessHost() ==
      contents->GetRenderProcessHost()) {
    GetWidget()->Close();
    // Close is async, make sure we drop our references to the tab immediately
    // (it may be going away).
    hung_pages_table_model_->InitForWebContents(NULL);
  }
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, views::DialogDelegate implementation:

base::string16 HungRendererDialogView::GetWindowTitle() const {
  if (!initialized_)
    return base::string16();

  return l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_HANGMONITOR_RENDERER_TITLE,
      hung_pages_table_model_->RowCount());
}

void HungRendererDialogView::WindowClosing() {
  // We are going to be deleted soon, so make sure our instance is destroyed.
  g_instance_ = NULL;
}

int HungRendererDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 HungRendererDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK_EQ(ui::DIALOG_BUTTON_CANCEL, button);
  return l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_WAIT);
}

views::View* HungRendererDialogView::CreateExtraView() {
  DCHECK(!kill_button_);
  kill_button_ = views::MdTextButton::CreateSecondaryUiButton(this,
      l10n_util::GetStringUTF16(IDS_BROWSER_HANGMONITOR_RENDERER_END));
  return kill_button_;
}

bool HungRendererDialogView::Cancel() {
  // Start waiting again for responsiveness.
  if (!kill_button_clicked_ &&
      hung_pages_table_model_->GetRenderViewHost()) {
    hung_pages_table_model_->GetRenderViewHost()
        ->GetWidget()
        ->RestartHangMonitorTimeout();
  }
  return true;
}

bool HungRendererDialogView::ShouldUseCustomFrame() const {
#if defined(OS_WIN)
  // Use the old dialog style without Aero glass, otherwise the dialog will be
  // visually constrained to browser window bounds. See http://crbug.com/323278
  return ui::win::IsAeroGlassEnabled();
#else
  return views::DialogDelegateView::ShouldUseCustomFrame();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, views::ButtonListener implementation:

void HungRendererDialogView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(kill_button_, sender);
  kill_button_clicked_ = true;
  content::RenderProcessHost* rph =
      hung_pages_table_model_->GetRenderProcessHost();
  if (!rph)
    return;
#if defined(OS_WIN)
  // Try to generate a crash report for the hung process.
  CrashDumpAndTerminateHungChildProcess(rph->GetHandle());
#else
  rph->Shutdown(content::RESULT_CODE_HUNG, false);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, HungPagesTableModel::Delegate overrides:

void HungRendererDialogView::TabDestroyed() {
  GetWidget()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, views::View overrides:

void HungRendererDialogView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  views::DialogDelegateView::ViewHierarchyChanged(details);
  if (!initialized_ && details.is_add && details.child == this && GetWidget())
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// HungRendererDialogView, private:

void HungRendererDialogView::Init() {
  views::ImageView* frozen_icon_view = new views::ImageView;
  frozen_icon_view->SetImage(frozen_icon_);

  info_label_ = new views::Label();
  info_label_->SetMultiLine(true);
  info_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  hung_pages_table_model_.reset(new HungPagesTableModel(this));
  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn());
  hung_pages_table_ = new views::TableView(
      hung_pages_table_model_.get(), columns, views::ICON_AND_TEXT, true);
  hung_pages_table_->SetGrouper(hung_pages_table_model_.get());

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int double_column_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(double_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::FIXED, frozen_icon_->width(), 0);
  column_set->AddPaddingColumn(0, kCentralColumnPadding);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_set_id);
  layout->AddView(frozen_icon_view, 1, 3);
  // Add the label with a preferred width of 1, this way it doesn't affect the
  // overall preferred size of the dialog.
  layout->AddView(
      info_label_, 1, 1, GridLayout::FILL, GridLayout::LEADING, 1, 0);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, double_column_set_id);
  layout->SkipColumns(1);
  layout->AddView(hung_pages_table_->CreateParentIfNecessary(), 1, 1,
                  views::GridLayout::FILL,
                  views::GridLayout::FILL, kTableViewWidth, kTableViewHeight);

  initialized_ = true;
}

// static
void HungRendererDialogView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    frozen_icon_ = rb.GetImageSkiaNamed(IDR_FROZEN_TAB_ICON);
    initialized = true;
  }
}
