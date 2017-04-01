// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using views::GridLayout;

namespace {

// Fixed width of the column holding the description label of the bubble.
const int kWidthOfDescriptionText = 320;

// Distance between checkbox and the text to the right of it.
const int kCheckboxTextDistance = 4;

// The color of the text of the sub panel to offer UMA opt-in.
const SkColor kTextColor = SkColorSetRGB(102, 102, 102);

enum SessionCrashedBubbleHistogramValue {
  SESSION_CRASHED_BUBBLE_SHOWN,
  SESSION_CRASHED_BUBBLE_ERROR,
  SESSION_CRASHED_BUBBLE_RESTORED,
  SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_HELP,
  SESSION_CRASHED_BUBBLE_IGNORED,
  SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN,
  SESSION_CRASHED_BUBBLE_MAX,
};

void RecordBubbleHistogramValue(SessionCrashedBubbleHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(
      "SessionCrashed.Bubble", value, SESSION_CRASHED_BUBBLE_MAX);
}

// Whether or not the bubble UI should be used.
// TODO(crbug.com/653966): Enable this on all desktop platforms.
bool IsBubbleUIEnabled() {
// Function ChangeMetricsReportingState (called when the user chooses to
// opt-in to UMA) does not support Chrome OS yet, so don't show the bubble on
// Chrome OS.
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

}  // namespace

// A helper class that listens to browser removal event.
class SessionCrashedBubbleView::BrowserRemovalObserver
    : public chrome::BrowserListObserver {
 public:
  explicit BrowserRemovalObserver(Browser* browser) : browser_(browser) {
    DCHECK(browser_);
    BrowserList::AddObserver(this);
  }

  ~BrowserRemovalObserver() override { BrowserList::RemoveObserver(this); }

  // Overridden from chrome::BrowserListObserver.
  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_)
      browser_ = NULL;
  }

  Browser* browser() const { return browser_; }

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRemovalObserver);
};

// static
bool SessionCrashedBubble::Show(Browser* browser) {
  if (!IsBubbleUIEnabled())
    return false;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (browser->profile()->IsOffTheRecord())
    return true;

  // Observes browser removal event and will be deallocated in ShowForReal.
  std::unique_ptr<SessionCrashedBubbleView::BrowserRemovalObserver>
      browser_observer(
          new SessionCrashedBubbleView::BrowserRemovalObserver(browser));

// Stats collection only applies to Google Chrome builds.
#if defined(GOOGLE_CHROME_BUILD)
  // Schedule a task to run GoogleUpdateSettings::GetCollectStatsConsent() on
  // FILE thread, since it does IO. Then, call
  // SessionCrashedBubbleView::ShowForReal with the result.
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&GoogleUpdateSettings::GetCollectStatsConsent),
      base::Bind(&SessionCrashedBubbleView::ShowForReal,
                 base::Passed(&browser_observer)));
#else
  SessionCrashedBubbleView::ShowForReal(std::move(browser_observer), false);
#endif  // defined(GOOGLE_CHROME_BUILD)

  return true;
}

// static
void SessionCrashedBubbleView::ShowForReal(
    std::unique_ptr<BrowserRemovalObserver> browser_observer,
    bool uma_opted_in_already) {
  // Determine whether or not the UMA opt-in option should be offered. It is
  // offered only when it is a Google chrome build, user hasn't opted in yet,
  // and the preference is modifiable by the user.
  bool offer_uma_optin = false;

#if defined(GOOGLE_CHROME_BUILD)
  if (!uma_opted_in_already)
    offer_uma_optin = !IsMetricsReportingPolicyManaged();
#endif  // defined(GOOGLE_CHROME_BUILD)

  Browser* browser = browser_observer->browser();

  if (!browser) {
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ERROR);
    return;
  }

  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar()
                                 ->app_menu_button();
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  if (!web_contents) {
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ERROR);
    return;
  }

  SessionCrashedBubbleView* crash_bubble =
      new SessionCrashedBubbleView(anchor_view, browser, web_contents,
                                   offer_uma_optin);
  views::BubbleDialogDelegateView::CreateBubble(crash_bubble)->Show();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_SHOWN);
  if (uma_opted_in_already)
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN);
}

SessionCrashedBubbleView::SessionCrashedBubbleView(
    views::View* anchor_view,
    Browser* browser,
    content::WebContents* web_contents,
    bool offer_uma_optin)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      content::WebContentsObserver(web_contents),
      browser_(browser),
      web_contents_(web_contents),
      uma_option_(NULL),
      offer_uma_optin_(offer_uma_optin),
      started_navigation_(false),
      restored_(false) {
  set_close_on_deactivate(false);
  registrar_.Add(
      this,
      chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<content::NavigationController>(&(
          web_contents->GetController())));
  browser->tab_strip_model()->AddObserver(this);
}

SessionCrashedBubbleView::~SessionCrashedBubbleView() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

base::string16 SessionCrashedBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_TITLE);
}

bool SessionCrashedBubbleView::ShouldShowWindowTitle() const {
  return true;
}

bool SessionCrashedBubbleView::ShouldShowCloseButton() const {
  return true;
}

void SessionCrashedBubbleView::OnWidgetDestroying(views::Widget* widget) {
  if (!restored_)
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_IGNORED);
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
}

void SessionCrashedBubbleView::Init() {
  SetLayoutManager(new views::FillLayout());

  // Description text label.
  views::Label* text_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_MESSAGE));
  text_label->SetMultiLine(true);
  text_label->SetLineHeight(20);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SizeToFit(kWidthOfDescriptionText);
  AddChildView(text_label);
}

views::View* SessionCrashedBubbleView::CreateFootnoteView() {
  if (!offer_uma_optin_)
    return nullptr;

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN);

  // Checkbox for metric reporting setting.
  // Since the text to the right of the checkbox can't be a simple string (needs
  // a hyperlink in it), this checkbox contains an empty string as its label,
  // and the real text will be added as a separate view.
  uma_option_ = new views::Checkbox(base::string16());
  uma_option_->SetChecked(false);

  // The text to the right of the checkbox.
  size_t offset;
  base::string16 link_text =
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_UMA_LINK_TEXT);
  base::string16 uma_text = l10n_util::GetStringFUTF16(
      IDS_SESSION_CRASHED_VIEW_UMA_OPTIN,
      link_text,
      &offset);
  views::StyledLabel* uma_label = new views::StyledLabel(uma_text, this);
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.font_style = gfx::Font::NORMAL;
  uma_label->AddStyleRange(gfx::Range(offset, offset + link_text.length()),
                           link_style);
  views::StyledLabel::RangeStyleInfo uma_style;
  uma_style.color = kTextColor;
  gfx::Range before_link_range(0, offset);
  if (!before_link_range.is_empty())
    uma_label->AddStyleRange(before_link_range, uma_style);
  gfx::Range after_link_range(offset + link_text.length(), uma_text.length());
  if (!after_link_range.is_empty())
    uma_label->AddStyleRange(after_link_range, uma_style);
  // Shift the text down by 1px to align with the checkbox.
  uma_label->SetBorder(views::CreateEmptyBorder(1, 0, 0, 0));

  // Create a view to hold the checkbox and the text.
  views::View* uma_view = new views::View();
  GridLayout* uma_layout = new GridLayout(uma_view);
  uma_view->SetLayoutManager(uma_layout);

  const int kReportColumnSetId = 0;
  views::ColumnSet* cs = uma_layout->AddColumnSet(kReportColumnSetId);
  cs->AddColumn(GridLayout::CENTER, GridLayout::LEADING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kCheckboxTextDistance);
  cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 1, GridLayout::USE_PREF, 0,
                0);

  uma_layout->StartRow(0, kReportColumnSetId);
  uma_layout->AddView(uma_option_);
  uma_layout->AddView(uma_label);

  return uma_view;
}

bool SessionCrashedBubbleView::Accept() {
  RestorePreviousSession();
  return true;
}

bool SessionCrashedBubbleView::Close() {
  // Don't default to Accept() just because that's the only choice. Instead, do
  // nothing.
  return true;
}

int SessionCrashedBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 SessionCrashedBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON);
}

void SessionCrashedBubbleView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  browser_->OpenURL(content::OpenURLParams(
      GURL("https://support.google.com/chrome/answer/96817"),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_HELP);
}

void SessionCrashedBubbleView::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::ReloadType reload_type) {
  started_navigation_ = true;
}

void SessionCrashedBubbleView::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (started_navigation_)
    CloseBubble();
}

void SessionCrashedBubbleView::WasShown() {
  GetWidget()->Show();
}

void SessionCrashedBubbleView::WasHidden() {
  GetWidget()->Hide();
}

void SessionCrashedBubbleView::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_TAB_CLOSING, type);
  CloseBubble();
}

void SessionCrashedBubbleView::TabDetachedAt(content::WebContents* contents,
                                             int index) {
  if (web_contents_ == contents)
    CloseBubble();
}

void SessionCrashedBubbleView::RestorePreviousSession() {
  SessionRestore::RestoreSessionAfterCrash(browser_);
  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_RESTORED);
  restored_ = true;

  // Record user's choice for opt-in in to UMA.
  // There's no opt-out choice in the crash restore bubble.
  if (uma_option_ && uma_option_->checked()) {
    ChangeMetricsReportingState(true);
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_UMA_OPTIN);
  }
  CloseBubble();
}

void SessionCrashedBubbleView::CloseBubble() {
  GetWidget()->Close();
}
