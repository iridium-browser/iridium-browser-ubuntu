// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_edit_model.h"

#include <algorithm>
#include <string>

#include "base/auto_reset.h"
#include "base/format_macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/toolbar/toolbar_model.h"
#include "components/url_formatter/url_fixer.h"
#include "ui/gfx/image/image.h"
#include "url/url_util.h"

using bookmarks::BookmarkModel;
using metrics::OmniboxEventProto;


// Helpers --------------------------------------------------------------------

namespace {

// Histogram name which counts the number of times that the user text is
// cleared.  IME users are sometimes in the situation that IME was
// unintentionally turned on and failed to input latin alphabets (ASCII
// characters) or the opposite case.  In that case, users may delete all
// the text and the user text gets cleared.  We'd like to measure how often
// this scenario happens.
//
// Note that since we don't currently correlate "text cleared" events with
// IME usage, this also captures many other cases where users clear the text;
// though it explicitly doesn't log deleting all the permanent text as
// the first action of an editing sequence (see comments in
// OnAfterPossibleChange()).
const char kOmniboxUserTextClearedHistogram[] = "Omnibox.UserTextCleared";

enum UserTextClearedType {
  OMNIBOX_USER_TEXT_CLEARED_BY_EDITING = 0,
  OMNIBOX_USER_TEXT_CLEARED_WITH_ESCAPE = 1,
  OMNIBOX_USER_TEXT_CLEARED_NUM_OF_ITEMS,
};

// Histogram name which counts the number of times the user enters
// keyword hint mode and via what method.  The possible values are listed
// in the EnteredKeywordModeMethod enum which is defined in the .h file.
const char kEnteredKeywordModeHistogram[] = "Omnibox.EnteredKeywordMode";

// Histogram name which counts the number of milliseconds a user takes
// between focusing and editing the omnibox.
const char kFocusToEditTimeHistogram[] = "Omnibox.FocusToEditTime";

// Histogram name which counts the number of milliseconds a user takes
// between focusing and opening an omnibox match.
const char kFocusToOpenTimeHistogram[] = "Omnibox.FocusToOpenTimeAnyPopupState";

// Split the percentage match histograms into buckets based on the width of the
// omnibox.
const int kPercentageMatchHistogramWidthBuckets[] = { 400, 700, 1200 };

void RecordPercentageMatchHistogram(const base::string16& old_text,
                                    const base::string16& new_text,
                                    bool url_replacement_active,
                                    ui::PageTransition transition,
                                    int omnibox_width) {
  size_t avg_length = (old_text.length() + new_text.length()) / 2;

  int percent = 0;
  if (!old_text.empty() && !new_text.empty()) {
    size_t shorter_length = std::min(old_text.length(), new_text.length());
    base::string16::const_iterator end(old_text.begin() + shorter_length);
    base::string16::const_iterator mismatch(
        std::mismatch(old_text.begin(), end, new_text.begin()).first);
    size_t matching_characters = mismatch - old_text.begin();
    percent = static_cast<float>(matching_characters) / avg_length * 100;
  }

  std::string histogram_name;
  if (url_replacement_active) {
    if (transition == ui::PAGE_TRANSITION_TYPED) {
      histogram_name = "InstantExtended.PercentageMatchV2_QuerytoURL";
      UMA_HISTOGRAM_PERCENTAGE(histogram_name, percent);
    } else {
      histogram_name = "InstantExtended.PercentageMatchV2_QuerytoQuery";
      UMA_HISTOGRAM_PERCENTAGE(histogram_name, percent);
    }
  } else {
    if (transition == ui::PAGE_TRANSITION_TYPED) {
      histogram_name = "InstantExtended.PercentageMatchV2_URLtoURL";
      UMA_HISTOGRAM_PERCENTAGE(histogram_name, percent);
    } else {
      histogram_name = "InstantExtended.PercentageMatchV2_URLtoQuery";
      UMA_HISTOGRAM_PERCENTAGE(histogram_name, percent);
    }
  }

  std::string suffix = "large";
  for (size_t i = 0; i < arraysize(kPercentageMatchHistogramWidthBuckets);
       ++i) {
    if (omnibox_width < kPercentageMatchHistogramWidthBuckets[i]) {
      suffix = base::IntToString(kPercentageMatchHistogramWidthBuckets[i]);
      break;
    }
  }

  // Cannot rely on UMA histograms macro because the name of the histogram is
  // generated dynamically.
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram_name + "_" + suffix, 1, 101, 102,
      base::Histogram::kUmaTargetedHistogramFlag);
  counter->Add(percent);
}

}  // namespace


// OmniboxEditModel::State ----------------------------------------------------

OmniboxEditModel::State::State(bool user_input_in_progress,
                               const base::string16& user_text,
                               const base::string16& gray_text,
                               const base::string16& keyword,
                               bool is_keyword_hint,
                               bool url_replacement_enabled,
                               OmniboxFocusState focus_state,
                               FocusSource focus_source,
                               const AutocompleteInput& autocomplete_input)
    : user_input_in_progress(user_input_in_progress),
      user_text(user_text),
      gray_text(gray_text),
      keyword(keyword),
      is_keyword_hint(is_keyword_hint),
      url_replacement_enabled(url_replacement_enabled),
      focus_state(focus_state),
      focus_source(focus_source),
      autocomplete_input(autocomplete_input) {
}

OmniboxEditModel::State::~State() {
}


// OmniboxEditModel -----------------------------------------------------------

OmniboxEditModel::OmniboxEditModel(OmniboxView* view,
                                   OmniboxEditController* controller,
                                   scoped_ptr<OmniboxClient> client)
    : client_(client.Pass()),
      view_(view),
      controller_(controller),
      focus_state_(OMNIBOX_FOCUS_NONE),
      focus_source_(INVALID),
      user_input_in_progress_(false),
      user_input_since_focus_(true),
      just_deleted_text_(false),
      has_temporary_text_(false),
      paste_state_(NONE),
      control_key_state_(UP),
      is_keyword_hint_(false),
      in_revert_(false),
      allow_exact_keyword_match_(false) {
  omnibox_controller_.reset(new OmniboxController(this, client_.get()));
}

OmniboxEditModel::~OmniboxEditModel() {
}

const OmniboxEditModel::State OmniboxEditModel::GetStateForTabSwitch() {
  // Like typing, switching tabs "accepts" the temporary text as the user
  // text, because it makes little sense to have temporary text when the
  // popup is closed.
  if (user_input_in_progress_) {
    // Weird edge case to match other browsers: if the edit is empty, revert to
    // the permanent text (so the user can get it back easily) but select it (so
    // on switching back, typing will "just work").
    const base::string16 user_text(UserTextFromDisplayText(view_->GetText()));
    if (user_text.empty()) {
      base::AutoReset<bool> tmp(&in_revert_, true);
      view_->RevertAll();
      view_->SelectAll(true);
    } else {
      InternalSetUserText(user_text);
    }
  }

  UMA_HISTOGRAM_BOOLEAN("Omnibox.SaveStateForTabSwitch.UserInputInProgress",
                        user_input_in_progress_);
  return State(
      user_input_in_progress_, user_text_, view_->GetGrayTextAutocompletion(),
      keyword_, is_keyword_hint_,
      controller_->GetToolbarModel()->url_replacement_enabled(),
      focus_state_, focus_source_, input_);
}

void OmniboxEditModel::RestoreState(const State* state) {
  // We need to update the permanent text correctly and revert the view
  // regardless of whether there is saved state.
  bool url_replacement_enabled = !state || state->url_replacement_enabled;
  controller_->GetToolbarModel()->set_url_replacement_enabled(
      url_replacement_enabled);
  permanent_text_ = controller_->GetToolbarModel()->GetText();
  // Don't muck with the search term replacement state, as we've just set it
  // correctly.
  view_->RevertWithoutResettingSearchTermReplacement();
  // Restore the autocomplete controller's input, or clear it if this is a new
  // tab.
  input_ = state ? state->autocomplete_input : AutocompleteInput();
  if (!state)
    return;

  SetFocusState(state->focus_state, OMNIBOX_FOCUS_CHANGE_TAB_SWITCH);
  focus_source_ = state->focus_source;
  // Restore any user editing.
  if (state->user_input_in_progress) {
    // NOTE: Be sure and set keyword-related state BEFORE invoking
    // DisplayTextFromUserText(), as its result depends upon this state.
    keyword_ = state->keyword;
    is_keyword_hint_ = state->is_keyword_hint;
    view_->SetUserText(state->user_text,
        DisplayTextFromUserText(state->user_text), false);
    view_->SetGrayTextAutocompletion(state->gray_text);
  }
}

AutocompleteMatch OmniboxEditModel::CurrentMatch(
    GURL* alternate_nav_url) const {
  // If we have a valid match use it. Otherwise get one for the current text.
  AutocompleteMatch match = omnibox_controller_->current_match();

  if (!match.destination_url.is_valid()) {
    GetInfoForCurrentText(&match, alternate_nav_url);
  } else if (alternate_nav_url) {
    *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        input_, match);
  }
  return match;
}

bool OmniboxEditModel::UpdatePermanentText() {
  // When there's new permanent text, and the user isn't interacting with the
  // omnibox, we want to revert the edit to show the new text.  We could simply
  // define "interacting" as "the omnibox has focus", but we still allow updates
  // when the omnibox has focus as long as the user hasn't begun editing, isn't
  // seeing zerosuggestions (because changing this text would require changing
  // or hiding those suggestions), and hasn't toggled on "Show URL" (because
  // this update will re-enable search term replacement, which will be annoying
  // if the user is trying to copy the URL).  When the omnibox doesn't have
  // focus, we assume the user may have abandoned their interaction and it's
  // always safe to change the text; this also prevents someone toggling "Show
  // URL" (which sounds as if it might be persistent) from seeing just that URL
  // forever afterwards.
  //
  // If the page is auto-committing gray text, however, we generally don't want
  // to make any change to the edit.  While auto-commits modify the underlying
  // permanent URL, they're intended to have no effect on the user's editing
  // process -- before and after the auto-commit, the omnibox should show the
  // same user text and the same instant suggestion, even if the auto-commit
  // happens while the edit doesn't have focus.
  base::string16 new_permanent_text = controller_->GetToolbarModel()->GetText();
  base::string16 gray_text = view_->GetGrayTextAutocompletion();
  const bool visibly_changed_permanent_text =
      (permanent_text_ != new_permanent_text) &&
      (!has_focus() ||
       (!user_input_in_progress_ &&
        !(popup_model() && popup_model()->IsOpen()) &&
        controller_->GetToolbarModel()->url_replacement_enabled())) &&
      (gray_text.empty() ||
       new_permanent_text != user_text_ + gray_text);

  permanent_text_ = new_permanent_text;
  return visibly_changed_permanent_text;
}

GURL OmniboxEditModel::PermanentURL() {
  return url_formatter::FixupURL(base::UTF16ToUTF8(permanent_text_),
                                 std::string());
}

void OmniboxEditModel::SetUserText(const base::string16& text) {
  SetInputInProgress(true);
  InternalSetUserText(text);
  omnibox_controller_->InvalidateCurrentMatch();
  paste_state_ = NONE;
  has_temporary_text_ = false;
}

bool OmniboxEditModel::CommitSuggestedText() {
  const base::string16 suggestion = view_->GetGrayTextAutocompletion();
  if (suggestion.empty())
    return false;

  const base::string16 final_text = view_->GetText() + suggestion;
  view_->OnBeforePossibleChange();
  view_->SetWindowTextAndCaretPos(final_text, final_text.length(), false,
      false);
  view_->OnAfterPossibleChange();
  return true;
}

void OmniboxEditModel::OnChanged() {
  // Hide any suggestions we might be showing.
  view_->SetGrayTextAutocompletion(base::string16());

  // Don't call CurrentMatch() when there's no editing, as in this case we'll
  // never actually use it.  This avoids running the autocomplete providers (and
  // any systems they then spin up) during startup.
  const AutocompleteMatch& current_match = user_input_in_progress_ ?
      CurrentMatch(NULL) : AutocompleteMatch();

  client_->OnTextChanged(current_match, user_input_in_progress_, user_text_,
                         result(), popup_model() && popup_model()->IsOpen(),
                         has_focus());
  controller_->OnChanged();
}

void OmniboxEditModel::GetDataForURLExport(GURL* url,
                                           base::string16* title,
                                           gfx::Image* favicon) {
  *url = CurrentMatch(NULL).destination_url;
  if (*url == client_->GetURL()) {
    *title = client_->GetTitle();
    *favicon = client_->GetFavicon();
  }
}

bool OmniboxEditModel::CurrentTextIsURL() const {
  if (controller_->GetToolbarModel()->WouldReplaceURL())
    return false;

  // If current text is not composed of replaced search terms and
  // !user_input_in_progress_, then permanent text is showing and should be a
  // URL, so no further checking is needed.  By avoiding checking in this case,
  // we avoid calling into the autocomplete providers, and thus initializing the
  // history system, as long as possible, which speeds startup.
  if (!user_input_in_progress_)
    return true;

  return !AutocompleteMatch::IsSearchType(CurrentMatch(NULL).type);
}

AutocompleteMatch::Type OmniboxEditModel::CurrentTextType() const {
  return CurrentMatch(NULL).type;
}

void OmniboxEditModel::AdjustTextForCopy(int sel_min,
                                         bool is_all_selected,
                                         base::string16* text,
                                         GURL* url,
                                         bool* write_url) {
  *write_url = false;

  // Do not adjust if selection did not start at the beginning of the field, or
  // if the URL was omitted.
  if ((sel_min != 0) || controller_->GetToolbarModel()->WouldReplaceURL())
    return;

  // Check whether the user is trying to copy the current page's URL by
  // selecting the whole thing without editing it.
  //
  // This is complicated by ZeroSuggest.  When ZeroSuggest is active, the user
  // may be selecting different items and thus changing the address bar text,
  // even though !user_input_in_progress_; and the permanent URL may change
  // without updating the visible text, just like when user input is in
  // progress.  In these cases, we don't want to copy the underlying URL, we
  // want to copy what the user actually sees.  However, if we simply never do
  // this block when !popup_model()->IsOpen(), then just clicking into the
  // address bar and trying to copy will always bypass this block on pages that
  // trigger ZeroSuggest, which is too conservative.  Instead, in the
  // ZeroSuggest case, we check that (a) the user hasn't selected one of the
  // other suggestions, and (b) the selected text is still the same as the
  // permanent text.  ((b) probably implies (a), but it doesn't hurt to be
  // sure.)  If these hold, then it's safe to copy the underlying URL.
  if (!user_input_in_progress_ && is_all_selected &&
      (!popup_model() || !popup_model()->IsOpen() ||
       ((popup_model()->selected_line() == 0) && (*text == permanent_text_)))) {
    // It's safe to copy the underlying URL.  These lines ensure that if the
    // scheme was stripped it's added back, and the URL is unescaped (we escape
    // parts of it for display).
    *url = PermanentURL();
    *text = base::UTF8ToUTF16(url->spec());
    *write_url = true;
    return;
  }

  // We can't use CurrentTextIsURL() or GetDataForURLExport() because right now
  // the user is probably holding down control to cause the copy, which will
  // screw up our calculation of the desired_tld.
  AutocompleteMatch match;
  client_->GetAutocompleteClassifier()->Classify(
      *text, is_keyword_selected(), true, ClassifyPage(), &match, NULL);
  if (AutocompleteMatch::IsSearchType(match.type))
    return;
  *url = match.destination_url;

  // Prefix the text with 'http://' if the text doesn't start with 'http://',
  // the text parses as a url with a scheme of http, the user selected the
  // entire host, and the user hasn't edited the host or manually removed the
  // scheme.
  GURL perm_url(PermanentURL());
  if (perm_url.SchemeIs(url::kHttpScheme) &&
      url->SchemeIs(url::kHttpScheme) && perm_url.host() == url->host()) {
    *write_url = true;
    base::string16 http = base::ASCIIToUTF16(url::kHttpScheme) +
        base::ASCIIToUTF16(url::kStandardSchemeSeparator);
    if (text->compare(0, http.length(), http) != 0)
      *text = http + *text;
  }
}

void OmniboxEditModel::SetInputInProgress(bool in_progress) {
  if (in_progress && !user_input_since_focus_) {
    base::TimeTicks now = base::TimeTicks::Now();
    DCHECK(last_omnibox_focus_ <= now);
    UMA_HISTOGRAM_TIMES(kFocusToEditTimeHistogram, now - last_omnibox_focus_);
    user_input_since_focus_ = true;
  }

  if (user_input_in_progress_ == in_progress)
    return;

  user_input_in_progress_ = in_progress;
  if (user_input_in_progress_) {
    time_user_first_modified_omnibox_ = base::TimeTicks::Now();
    base::RecordAction(base::UserMetricsAction("OmniboxInputInProgress"));
    autocomplete_controller()->ResetSession();
  }

  controller_->GetToolbarModel()->set_input_in_progress(in_progress);
  controller_->UpdateWithoutTabRestore();

  if (user_input_in_progress_ || !in_revert_)
    client_->OnInputStateChanged();
}

void OmniboxEditModel::Revert() {
  SetInputInProgress(false);
  input_.Clear();
  paste_state_ = NONE;
  InternalSetUserText(base::string16());
  keyword_.clear();
  is_keyword_hint_ = false;
  has_temporary_text_ = false;
  view_->SetWindowTextAndCaretPos(permanent_text_,
                                  has_focus() ? permanent_text_.length() : 0,
                                  false, true);
  client_->OnRevert();
}

void OmniboxEditModel::StartAutocomplete(
    bool has_selected_text,
    bool prevent_inline_autocomplete,
    bool entering_keyword_mode) {
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/440919 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "440919 OmniboxEditModel::StartAutocomplete"));
  size_t cursor_position;
  if (inline_autocomplete_text_.empty()) {
    // Cursor position is equivalent to the current selection's end.
    size_t start;
    view_->GetSelectionBounds(&start, &cursor_position);
    // If we're in keyword mode, we're not displaying the full |user_text_|, so
    // the cursor position we got from the view has to be adjusted later by the
    // length of the undisplayed text.  If we're just entering keyword mode,
    // though, we have to avoid making this adjustment, because we haven't
    // actually hidden any text yet, but the caller has already cleared
    // |is_keyword_hint_|, so DisplayTextFromUserText() will believe we are
    // already in keyword mode, and will thus mis-adjust the cursor position.
    if (!entering_keyword_mode) {
      cursor_position +=
          user_text_.length() - DisplayTextFromUserText(user_text_).length();
    }
  } else {
    // There are some cases where StartAutocomplete() may be called
    // with non-empty |inline_autocomplete_text_|.  In such cases, we cannot
    // use the current selection, because it could result with the cursor
    // position past the last character from the user text.  Instead,
    // we assume that the cursor is simply at the end of input.
    // One example is when user presses Ctrl key while having a highlighted
    // inline autocomplete text.
    // TODO: Rethink how we are going to handle this case to avoid
    // inconsistent behavior when user presses Ctrl key.
    // See http://crbug.com/165961 and http://crbug.com/165968 for more details.
    cursor_position = user_text_.length();
  }

  GURL current_url;
  if (client_->CurrentPageExists())
    current_url = client_->GetURL();
  input_ = AutocompleteInput(
      user_text_, cursor_position, std::string(), current_url, ClassifyPage(),
      prevent_inline_autocomplete || just_deleted_text_ ||
          (has_selected_text && inline_autocomplete_text_.empty()) ||
          (paste_state_ != NONE),
      is_keyword_selected(),
      is_keyword_selected() || allow_exact_keyword_match_, true, false,
      client_->GetSchemeClassifier());

  omnibox_controller_->StartAutocomplete(input_);
}

void OmniboxEditModel::StopAutocomplete() {
  autocomplete_controller()->Stop(true);
}

bool OmniboxEditModel::CanPasteAndGo(const base::string16& text) const {
  if (!client_->IsPasteAndGoEnabled())
    return false;

  AutocompleteMatch match;
  ClassifyStringForPasteAndGo(text, &match, NULL);
  return match.destination_url.is_valid();
}

void OmniboxEditModel::PasteAndGo(const base::string16& text) {
  DCHECK(CanPasteAndGo(text));
  UMA_HISTOGRAM_COUNTS("Omnibox.PasteAndGo", 1);

  view_->RevertAll();
  AutocompleteMatch match;
  GURL alternate_nav_url;
  ClassifyStringForPasteAndGo(text, &match, &alternate_nav_url);
  view_->OpenMatch(match, CURRENT_TAB, alternate_nav_url, text,
                   OmniboxPopupModel::kNoMatch);
}

bool OmniboxEditModel::IsPasteAndSearch(const base::string16& text) const {
  AutocompleteMatch match;
  ClassifyStringForPasteAndGo(text, &match, NULL);
  return AutocompleteMatch::IsSearchType(match.type);
}

void OmniboxEditModel::AcceptInput(WindowOpenDisposition disposition,
                                   bool for_drop) {
  // Get the URL and transition type for the selected entry.
  GURL alternate_nav_url;
  AutocompleteMatch match = CurrentMatch(&alternate_nav_url);

  // If CTRL is down it means the user wants to append ".com" to the text he
  // typed. If we can successfully generate a URL_WHAT_YOU_TYPED match doing
  // that, then we use this. These matches are marked as generated by the
  // HistoryURLProvider so we only generate them if this provider is present.
  if (control_key_state_ == DOWN_WITHOUT_CHANGE && !is_keyword_selected() &&
      autocomplete_controller()->history_url_provider()) {
    // Generate a new AutocompleteInput, copying the latest one but using "com"
    // as the desired TLD. Then use this autocomplete input to generate a
    // URL_WHAT_YOU_TYPED AutocompleteMatch. Note that using the most recent
    // input instead of the currently visible text means we'll ignore any
    // visible inline autocompletion: if a user types "foo" and is autocompleted
    // to "foodnetwork.com", ctrl-enter will navigate to "foo.com", not
    // "foodnetwork.com".  At the time of writing, this behavior matches
    // Internet Explorer, but not Firefox.
    input_ = AutocompleteInput(
        has_temporary_text_ ? UserTextFromDisplayText(view_->GetText())
                            : input_.text(),
        input_.cursor_position(), "com", GURL(),
        input_.current_page_classification(),
        input_.prevent_inline_autocomplete(), input_.prefer_keyword(),
        input_.allow_exact_keyword_match(), input_.want_asynchronous_matches(),
        input_.from_omnibox_focus(), client_->GetSchemeClassifier());
    AutocompleteMatch url_match(
        autocomplete_controller()->history_url_provider()->SuggestExactInput(
            input_, input_.canonicalized_url(), false));

    if (url_match.destination_url.is_valid()) {
      // We have a valid URL, we use this newly generated AutocompleteMatch.
      match = url_match;
      alternate_nav_url = GURL();
    }
  }

  if (!match.destination_url.is_valid())
    return;

  if ((match.transition == ui::PAGE_TRANSITION_TYPED) &&
      (match.destination_url == PermanentURL())) {
    // When the user hit enter on the existing permanent URL, treat it like a
    // reload for scoring purposes.  We could detect this by just checking
    // user_input_in_progress_, but it seems better to treat "edits" that end
    // up leaving the URL unchanged (e.g. deleting the last character and then
    // retyping it) as reloads too.  We exclude non-TYPED transitions because if
    // the transition is GENERATED, the user input something that looked
    // different from the current URL, even if it wound up at the same place
    // (e.g. manually retyping the same search query), and it seems wrong to
    // treat this as a reload.
    match.transition = ui::PAGE_TRANSITION_RELOAD;
  } else if (for_drop ||
             ((paste_state_ != NONE) &&
              (match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED))) {
    // When the user pasted in a URL and hit enter, score it like a link click
    // rather than a normal typed URL, so it doesn't get inline autocompleted
    // as aggressively later.
    match.transition = ui::PAGE_TRANSITION_LINK;
  }

  client_->OnInputAccepted(match);

  DCHECK(popup_model());
  view_->OpenMatch(match, disposition, alternate_nav_url, base::string16(),
                   popup_model()->selected_line());
}

void OmniboxEditModel::OpenMatch(AutocompleteMatch match,
                                 WindowOpenDisposition disposition,
                                 const GURL& alternate_nav_url,
                                 const base::string16& pasted_text,
                                 size_t index) {
  const base::TimeTicks& now(base::TimeTicks::Now());
  base::TimeDelta elapsed_time_since_user_first_modified_omnibox(
      now - time_user_first_modified_omnibox_);
  autocomplete_controller()->UpdateMatchDestinationURLWithQueryFormulationTime(
      elapsed_time_since_user_first_modified_omnibox, &match);

  base::string16 input_text(pasted_text);
  if (input_text.empty())
      input_text = user_input_in_progress_ ? user_text_ : permanent_text_;
  // Create a dummy AutocompleteInput for use in calling SuggestExactInput()
  // to create an alternate navigational match.
  AutocompleteInput alternate_input(
      input_text, base::string16::npos, std::string(),
      // Somehow we can occasionally get here with no active tab.  It's not
      // clear why this happens.
      client_->CurrentPageExists() ? client_->GetURL() : GURL(), ClassifyPage(),
      false, false, true, true, false, client_->GetSchemeClassifier());
  scoped_ptr<OmniboxNavigationObserver> observer(
      client_->CreateOmniboxNavigationObserver(
          input_text, match,
          autocomplete_controller()->history_url_provider()->SuggestExactInput(
              alternate_input, alternate_nav_url,
              AutocompleteInput::HasHTTPScheme(input_text))));

  base::TimeDelta elapsed_time_since_last_change_to_default_match(
      now - autocomplete_controller()->last_time_default_match_changed());
  DCHECK(match.provider);
  // These elapsed times don't really make sense for ZeroSuggest matches
  // (because the user does not modify the omnibox for ZeroSuggest), so for
  // those we set the elapsed times to something that will be ignored by
  // metrics_log.cc.  They also don't necessarily make sense if the omnibox
  // dropdown is closed or the user used a paste-and-go action.  (In most
  // cases when this happens, the user never modified the omnibox.)
  if ((match.provider->type() == AutocompleteProvider::TYPE_ZERO_SUGGEST) ||
      !popup_model()->IsOpen() || !pasted_text.empty()) {
    const base::TimeDelta default_time_delta =
        base::TimeDelta::FromMilliseconds(-1);
    elapsed_time_since_user_first_modified_omnibox = default_time_delta;
    elapsed_time_since_last_change_to_default_match = default_time_delta;
  }
  // If the popup is closed or this is a paste-and-go action (meaning the
  // contents of the dropdown are ignored regardless), we record for logging
  // purposes a selected_index of 0 and a suggestion list as having a single
  // entry of the match used.
  ACMatches fake_single_entry_matches;
  fake_single_entry_matches.push_back(match);
  AutocompleteResult fake_single_entry_result;
  fake_single_entry_result.AppendMatches(input_, fake_single_entry_matches);
  OmniboxLog log(
      input_text,
      just_deleted_text_,
      input_.type(),
      popup_model()->IsOpen(),
      (!popup_model()->IsOpen() || !pasted_text.empty()) ? 0 : index,
      !pasted_text.empty(),
      -1,  // don't yet know tab ID; set later if appropriate
      ClassifyPage(),
      elapsed_time_since_user_first_modified_omnibox,
      match.allowed_to_be_default_match ? match.inline_autocompletion.length() :
          base::string16::npos,
      elapsed_time_since_last_change_to_default_match,
      (!popup_model()->IsOpen() || !pasted_text.empty()) ?
          fake_single_entry_result : result());
  DCHECK(!popup_model()->IsOpen() || !pasted_text.empty() ||
         (log.elapsed_time_since_user_first_modified_omnibox >=
          log.elapsed_time_since_last_change_to_default_match))
      << "We should've got the notification that the user modified the "
      << "omnibox text at same time or before the most recent time the "
      << "default match changed.";

  if ((disposition == CURRENT_TAB) && client_->CurrentPageExists()) {
    // If we know the destination is being opened in the current tab,
    // we can easily get the tab ID.  (If it's being opened in a new
    // tab, we don't know the tab ID yet.)
    log.tab_id = client_->GetSessionID().id();
  }
  autocomplete_controller()->AddProvidersInfo(&log.providers_info);
  client_->OnURLOpenedFromOmnibox(&log);
  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);
  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);
  DCHECK(!last_omnibox_focus_.is_null())
      << "An omnibox focus should have occurred before opening a match.";
  UMA_HISTOGRAM_TIMES(kFocusToOpenTimeHistogram, now - last_omnibox_focus_);

  TemplateURLService* service = client_->GetTemplateURLService();
  TemplateURL* template_url = match.GetTemplateURL(service, false);
  if (template_url) {
    if (match.transition == ui::PAGE_TRANSITION_KEYWORD) {
      // The user is using a non-substituting keyword or is explicitly in
      // keyword mode.

      // Don't increment usage count for extension keywords.
      if (client_->ProcessExtensionKeyword(template_url, match, disposition,
                                           observer.get())) {
        if (disposition != NEW_BACKGROUND_TAB)
          view_->RevertAll();
        return;
      }

      base::RecordAction(base::UserMetricsAction("AcceptedKeyword"));
      client_->GetTemplateURLService()->IncrementUsageCount(template_url);
    } else {
      DCHECK_EQ(ui::PAGE_TRANSITION_GENERATED, match.transition);
      // NOTE: We purposefully don't increment the usage count of the default
      // search engine here like we do for explicit keywords above; see comments
      // in template_url.h.
    }

    SearchEngineType search_engine_type = match.destination_url.is_valid() ?
        TemplateURLPrepopulateData::GetEngineType(match.destination_url) :
        SEARCH_ENGINE_OTHER;
    UMA_HISTOGRAM_ENUMERATION("Omnibox.SearchEngineType", search_engine_type,
                              SEARCH_ENGINE_MAX);
  }

  // Get the current text before we call RevertAll() which will clear it.
  base::string16 current_text = view_->GetText();

  if (disposition != NEW_BACKGROUND_TAB) {
    base::AutoReset<bool> tmp(&in_revert_, true);
    view_->RevertAll();  // Revert the box to its unedited state.
  }

  RecordPercentageMatchHistogram(
      permanent_text_, current_text,
      controller_->GetToolbarModel()->WouldReplaceURL(),
      match.transition, view_->GetWidth());

  // Track whether the destination URL sends us to a search results page
  // using the default search provider.
  if (client_->GetTemplateURLService()
          ->IsSearchResultsPageFromDefaultSearchProvider(
              match.destination_url)) {
    base::RecordAction(
        base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
  }

  if (match.destination_url.is_valid()) {
    // This calls RevertAll again.
    base::AutoReset<bool> tmp(&in_revert_, true);
    controller_->OnAutocompleteAccept(
        match.destination_url, disposition,
        ui::PageTransitionFromInt(
            match.transition | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
    if (observer && observer->HasSeenPendingLoad())
      ignore_result(observer.release());  // The observer will delete itself.
  }

  BookmarkModel* bookmark_model = client_->GetBookmarkModel();
  if (bookmark_model && bookmark_model->IsBookmarked(match.destination_url))
    client_->OnBookmarkLaunched();
}

bool OmniboxEditModel::AcceptKeyword(EnteredKeywordModeMethod entered_method) {
  DCHECK(is_keyword_hint_ && !keyword_.empty());

  autocomplete_controller()->Stop(false);
  is_keyword_hint_ = false;

  if (popup_model() && popup_model()->IsOpen())
    popup_model()->SetSelectedLineState(OmniboxPopupModel::KEYWORD);
  else
    StartAutocomplete(false, true, true);

  // When entering keyword mode via tab, the new text to show is whatever the
  // newly-selected match in the dropdown is.  When entering via space, however,
  // we should make sure to use the actual |user_text_| as the basis for the new
  // text.  This ensures that if the user types "<keyword><space>" and the
  // default match would have inline autocompleted a further string (e.g.
  // because there's a past multi-word search beginning with this keyword), the
  // inline autocompletion doesn't get filled in as the keyword search query
  // text.
  //
  // We also treat tabbing into keyword mode like tabbing through the popup in
  // that we set |has_temporary_text_|, whereas pressing space is treated like
  // a new keystroke that changes the current match instead of overlaying it
  // with a temporary one.  This is important because rerunning autocomplete
  // after the user pressed space, which will have happened just before reaching
  // here, may have generated a new match, which the user won't actually see and
  // which we don't want to switch back to when existing keyword mode; see
  // comments in ClearKeyword().
  if (entered_method == ENTERED_KEYWORD_MODE_VIA_TAB) {
    // Ensure the current selection is saved before showing keyword mode
    // so that moving to another line and then reverting the text will restore
    // the current state properly.
    bool save_original_selection = !has_temporary_text_;
    has_temporary_text_ = true;
    const AutocompleteMatch& match = CurrentMatch(NULL);
    original_url_ = match.destination_url;
    view_->OnTemporaryTextMaybeChanged(
        DisplayTextFromUserText(match.fill_into_edit),
        save_original_selection, true);
  } else {
    view_->OnTemporaryTextMaybeChanged(DisplayTextFromUserText(user_text_),
                                       false, true);
  }

  base::RecordAction(base::UserMetricsAction("AcceptedKeywordHint"));
  UMA_HISTOGRAM_ENUMERATION(kEnteredKeywordModeHistogram, entered_method,
                            ENTERED_KEYWORD_MODE_NUM_ITEMS);

  return true;
}

void OmniboxEditModel::AcceptTemporaryTextAsUserText() {
  InternalSetUserText(UserTextFromDisplayText(view_->GetText()));
  has_temporary_text_ = false;

  if (user_input_in_progress_ || !in_revert_)
    client_->OnInputStateChanged();
}

void OmniboxEditModel::ClearKeyword() {
  autocomplete_controller()->Stop(false);

  // While we're always in keyword mode upon reaching here, sometimes we've just
  // toggled in via space or tab, and sometimes we're on a non-toggled line
  // (usually because the user has typed a search string).  Keep track of the
  // difference, as we'll need it below.
  bool was_toggled_into_keyword_mode =
    popup_model()->selected_line_state() == OmniboxPopupModel::KEYWORD;

  omnibox_controller_->ClearPopupKeywordMode();

  // There are several possible states we could have been in before the user hit
  // backspace or shift-tab to enter this function:
  // (1) was_toggled_into_keyword_mode == false, has_temporary_text_ == false
  //     The user typed a further key after being in keyword mode already, e.g.
  //     "google.com f".
  // (2) was_toggled_into_keyword_mode == false, has_temporary_text_ == true
  //     The user tabbed away from a dropdown entry in keyword mode, then tabbed
  //     back to it, e.g. "google.com f<tab><shift-tab>".
  // (3) was_toggled_into_keyword_mode == true, has_temporary_text_ == false
  //     The user had just typed space to enter keyword mode, e.g.
  //     "google.com ".
  // (4) was_toggled_into_keyword_mode == true, has_temporary_text_ == true
  //     The user had just typed tab to enter keyword mode, e.g.
  //     "google.com<tab>".
  //
  // For states 1-3, we can safely handle the exit from keyword mode by using
  // OnBefore/AfterPossibleChange() to do a complete state update of all
  // objects.  However, with state 4, if we do this, and if the user had tabbed
  // into keyword mode on a line in the middle of the dropdown instead of the
  // first line, then the state update will rerun autocompletion and reset the
  // whole dropdown, and end up with the first line selected instead, instead of
  // just "undoing" the keyword mode entry on the non-first line.  So in this
  // case we simply reset |is_keyword_hint_| to true and update the window text.
  //
  // You might wonder why we don't simply do this in all cases.  In states 1-2,
  // getting out of keyword mode likely shouldn't put us in keyword hint mode;
  // if the user typed "google.com f" and then put the cursor before 'f' and hit
  // backspace, the resulting text would be "google.comf", which is unlikely to
  // be a keyword.  Unconditionally putting things back in keyword hint mode is
  // going to lead to internally inconsistent state, and possible future
  // crashes.  State 3 is more subtle; here we need to do the full state update
  // because before entering keyword mode to begin with, we will have re-run
  // autocomplete in ways that can produce surprising results if we just switch
  // back out of keyword mode.  For example, if a user has a keyword named "x",
  // an inline-autocompletable history site "xyz.com", and a lower-ranked
  // inline-autocompletable search "x y", then typing "x" will inline-
  // autocomplete to "xyz.com", hitting space will toggle into keyword mode, but
  // then hitting backspace could wind up with the default match as the "x y"
  // search, which feels bizarre.
  const base::string16 window_text(keyword_ + view_->GetText());
  if (was_toggled_into_keyword_mode && has_temporary_text_) {
    // State 4 above.
    is_keyword_hint_ = true;
    view_->SetWindowTextAndCaretPos(window_text.c_str(), keyword_.length(),
        false, true);
  } else {
    // States 1-3 above.
    view_->OnBeforePossibleChange();
    view_->SetWindowTextAndCaretPos(window_text.c_str(), keyword_.length(),
        false, false);
    keyword_.clear();
    is_keyword_hint_ = false;
    view_->OnAfterPossibleChange();
  }
}

void OmniboxEditModel::OnSetFocus(bool control_down) {
  last_omnibox_focus_ = base::TimeTicks::Now();
  user_input_since_focus_ = false;

  // If the omnibox lost focus while the caret was hidden and then regained
  // focus, OnSetFocus() is called and should restore visibility. Note that
  // focus can be regained without an accompanying call to
  // OmniboxView::SetFocus(), e.g. by tabbing in.
  SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  control_key_state_ = control_down ? DOWN_WITHOUT_CHANGE : UP;

  // Try to get ZeroSuggest suggestions if a page is loaded and the user has
  // not been typing in the omnibox.  The |user_input_in_progress_| check is
  // used to detect the case where this function is called after right-clicking
  // in the omnibox and selecting paste in Linux (in which case we actually get
  // the OnSetFocus() call after the process of handling the paste has kicked
  // off).
  // TODO(hfung): Remove this when crbug/271590 is fixed.
  if (client_->CurrentPageExists() && !user_input_in_progress_) {
    // We avoid PermanentURL() here because it's not guaranteed to give us the
    // actual underlying current URL, e.g. if we're on the NTP and the
    // |permanent_text_| is empty.
    input_ =
        AutocompleteInput(permanent_text_, base::string16::npos, std::string(),
                          client_->GetURL(), ClassifyPage(), false, false, true,
                          true, true, client_->GetSchemeClassifier());
    autocomplete_controller()->Start(input_);
  }

  if (user_input_in_progress_ || !in_revert_)
    client_->OnInputStateChanged();
}

void OmniboxEditModel::SetCaretVisibility(bool visible) {
  // Caret visibility only matters if the omnibox has focus.
  if (focus_state_ != OMNIBOX_FOCUS_NONE) {
    SetFocusState(visible ? OMNIBOX_FOCUS_VISIBLE : OMNIBOX_FOCUS_INVISIBLE,
                  OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  }
}

void OmniboxEditModel::OnWillKillFocus() {
  if (user_input_in_progress_ || !in_revert_)
    client_->OnInputStateChanged();
}

void OmniboxEditModel::OnKillFocus() {
  SetFocusState(OMNIBOX_FOCUS_NONE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  focus_source_ = INVALID;
  control_key_state_ = UP;
  paste_state_ = NONE;
}

bool OmniboxEditModel::WillHandleEscapeKey() const {
  return user_input_in_progress_ ||
      (has_temporary_text_ &&
       (CurrentMatch(NULL).destination_url != original_url_));
}

bool OmniboxEditModel::OnEscapeKeyPressed() {
  if (has_temporary_text_ &&
      (CurrentMatch(NULL).destination_url != original_url_)) {
    RevertTemporaryText(true);
    return true;
  }

  // We do not clear the pending entry from the omnibox when a load is first
  // stopped.  If the user presses Escape while stopped, whether editing or not,
  // we clear it.
  if (client_->CurrentPageExists() && !client_->IsLoading()) {
    client_->DiscardNonCommittedNavigations();
    view_->Update();
  }

  if (!user_text_.empty()) {
    UMA_HISTOGRAM_ENUMERATION(kOmniboxUserTextClearedHistogram,
                              OMNIBOX_USER_TEXT_CLEARED_WITH_ESCAPE,
                              OMNIBOX_USER_TEXT_CLEARED_NUM_OF_ITEMS);
  }

  // Unconditionally revert/select all.  This ensures any popup, whether due to
  // normal editing or ZeroSuggest, is closed, and the full text is selected.
  // This in turn allows the user to use escape to quickly select all the text
  // for ease of replacement, and matches other browsers.
  bool user_input_was_in_progress = user_input_in_progress_;
  view_->RevertAll();
  view_->SelectAll(true);

  // If the user was in the midst of editing, don't cancel any underlying page
  // load.  This doesn't match IE or Firefox, but seems more correct.  Note that
  // we do allow the page load to be stopped in the case where ZeroSuggest was
  // visible; this is so that it's still possible to focus the address bar and
  // hit escape once to stop a load even if the address being loaded triggers
  // the ZeroSuggest popup.
  return user_input_was_in_progress;
}

void OmniboxEditModel::OnControlKeyChanged(bool pressed) {
  if (pressed == (control_key_state_ == UP))
    control_key_state_ = pressed ? DOWN_WITHOUT_CHANGE : UP;
}

void OmniboxEditModel::OnPaste() {
  UMA_HISTOGRAM_COUNTS("Omnibox.Paste", 1);
  paste_state_ = PASTING;
}

void OmniboxEditModel::OnUpOrDownKeyPressed(int count) {
  // NOTE: This purposefully doesn't trigger any code that resets paste_state_.
  if (popup_model() && popup_model()->IsOpen()) {
    // The popup is open, so the user should be able to interact with it
    // normally.
    popup_model()->Move(count);
    return;
  }

  if (!query_in_progress()) {
    // The popup is neither open nor working on a query already.  So, start an
    // autocomplete query for the current text.  This also sets
    // user_input_in_progress_ to true, which we want: if the user has started
    // to interact with the popup, changing the permanent_text_ shouldn't change
    // the displayed text.
    // Note: This does not force the popup to open immediately.
    // TODO(pkasting): We should, in fact, force this particular query to open
    // the popup immediately.
    if (!user_input_in_progress_)
      InternalSetUserText(permanent_text_);
    view_->UpdatePopup();
    return;
  }

  // TODO(pkasting): The popup is working on a query but is not open.  We should
  // force it to open immediately.
}

void OmniboxEditModel::OnPopupDataChanged(
    const base::string16& text,
    GURL* destination_for_temporary_text_change,
    const base::string16& keyword,
    bool is_keyword_hint) {
  // The popup changed its data, the match in the controller is no longer valid.
  omnibox_controller_->InvalidateCurrentMatch();

  // Update keyword/hint-related local state.
  bool keyword_state_changed = (keyword_ != keyword) ||
      ((is_keyword_hint_ != is_keyword_hint) && !keyword.empty());
  if (keyword_state_changed) {
    keyword_ = keyword;
    is_keyword_hint_ = is_keyword_hint;

    // |is_keyword_hint_| should always be false if |keyword_| is empty.
    DCHECK(!keyword_.empty() || !is_keyword_hint_);
  }

  // Handle changes to temporary text.
  if (destination_for_temporary_text_change != NULL) {
    const bool save_original_selection = !has_temporary_text_;
    if (save_original_selection) {
      // Save the original selection and URL so it can be reverted later.
      has_temporary_text_ = true;
      original_url_ = *destination_for_temporary_text_change;
      inline_autocomplete_text_.clear();
      view_->OnInlineAutocompleteTextCleared();
    }
    if (control_key_state_ == DOWN_WITHOUT_CHANGE) {
      // Arrowing around the popup cancels control-enter.
      control_key_state_ = DOWN_WITH_CHANGE;
      // Now things are a bit screwy: the desired_tld has changed, but if we
      // update the popup, the new order of entries won't match the old, so the
      // user's selection gets screwy; and if we don't update the popup, and the
      // user reverts, then the selected item will be as if control is still
      // pressed, even though maybe it isn't any more.  There is no obvious
      // right answer here :(
    }
    view_->OnTemporaryTextMaybeChanged(DisplayTextFromUserText(text),
                                       save_original_selection, true);
    return;
  }

  bool call_controller_onchanged = true;
  inline_autocomplete_text_ = text;
  if (inline_autocomplete_text_.empty())
    view_->OnInlineAutocompleteTextCleared();

  const base::string16& user_text =
      user_input_in_progress_ ? user_text_ : permanent_text_;
  if (keyword_state_changed && is_keyword_selected()) {
    // If we reach here, the user most likely entered keyword mode by inserting
    // a space between a keyword name and a search string (as pressing space or
    // tab after the keyword name alone would have been be handled in
    // MaybeAcceptKeywordBySpace() by calling AcceptKeyword(), which won't reach
    // here).  In this case, we don't want to call
    // OnInlineAutocompleteTextMaybeChanged() as normal, because that will
    // correctly change the text (to the search string alone) but move the caret
    // to the end of the string; instead we want the caret at the start of the
    // search string since that's where it was in the original input.  So we set
    // the text and caret position directly.
    //
    // It may also be possible to reach here if we're reverting from having
    // temporary text back to a default match that's a keyword search, but in
    // that case the RevertTemporaryText() call below will reset the caret or
    // selection correctly so the caret positioning we do here won't matter.
    view_->SetWindowTextAndCaretPos(DisplayTextFromUserText(user_text), 0,
                                                            false, false);
  } else if (view_->OnInlineAutocompleteTextMaybeChanged(
             DisplayTextFromUserText(user_text + inline_autocomplete_text_),
             DisplayTextFromUserText(user_text).length())) {
    call_controller_onchanged = false;
  }

  // If |has_temporary_text_| is true, then we previously had a manual selection
  // but now don't (or |destination_for_temporary_text_change| would have been
  // non-NULL). This can happen when deleting the selected item in the popup.
  // In this case, we've already reverted the popup to the default match, so we
  // need to revert ourselves as well.
  if (has_temporary_text_) {
    RevertTemporaryText(false);
    call_controller_onchanged = false;
  }

  // We need to invoke OnChanged in case the destination url changed (as could
  // happen when control is toggled).
  if (call_controller_onchanged)
    OnChanged();
}

bool OmniboxEditModel::OnAfterPossibleChange(const base::string16& old_text,
                                             const base::string16& new_text,
                                             size_t selection_start,
                                             size_t selection_end,
                                             bool selection_differs,
                                             bool text_differs,
                                             bool just_deleted_text,
                                             bool allow_keyword_ui_change) {
  // Update the paste state as appropriate: if we're just finishing a paste
  // that replaced all the text, preserve that information; otherwise, if we've
  // made some other edit, clear paste tracking.
  if (paste_state_ == PASTING)
    paste_state_ = PASTED;
  else if (text_differs)
    paste_state_ = NONE;

  if (text_differs || selection_differs) {
    // Record current focus state for this input if we haven't already.
    if (focus_source_ == INVALID) {
      // We should generally expect the omnibox to have focus at this point, but
      // it doesn't always on Linux. This is because, unlike other platforms,
      // right clicking in the omnibox on Linux doesn't focus it. So pasting via
      // right-click can change the contents without focusing the omnibox.
      // TODO(samarth): fix Linux focus behavior and add a DCHECK here to
      // check that the omnibox does have focus.
      focus_source_ = (focus_state_ == OMNIBOX_FOCUS_INVISIBLE) ?
          FAKEBOX : OMNIBOX;
    }

    // Restore caret visibility whenever the user changes text or selection in
    // the omnibox.
    SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_TYPING);
  }

  // Modifying the selection counts as accepting the autocompleted text.
  const bool user_text_changed =
      text_differs || (selection_differs && !inline_autocomplete_text_.empty());

  // If something has changed while the control key is down, prevent
  // "ctrl-enter" until the control key is released.
  if ((text_differs || selection_differs) &&
      (control_key_state_ == DOWN_WITHOUT_CHANGE))
    control_key_state_ = DOWN_WITH_CHANGE;

  if (!user_text_changed)
    return false;

  // If the user text has not changed, we do not want to change the model's
  // state associated with the text.  Otherwise, we can get surprising behavior
  // where the autocompleted text unexpectedly reappears, e.g. crbug.com/55983
  InternalSetUserText(UserTextFromDisplayText(new_text));
  has_temporary_text_ = false;

  // Track when the user has deleted text so we won't allow inline
  // autocomplete.
  just_deleted_text_ = just_deleted_text;

  if (user_input_in_progress_ && user_text_.empty()) {
    // Log cases where the user started editing and then subsequently cleared
    // all the text.  Note that this explicitly doesn't catch cases like
    // "hit ctrl-l to select whole edit contents, then hit backspace", because
    // in such cases, |user_input_in_progress| won't be true here.
    UMA_HISTOGRAM_ENUMERATION(kOmniboxUserTextClearedHistogram,
                              OMNIBOX_USER_TEXT_CLEARED_BY_EDITING,
                              OMNIBOX_USER_TEXT_CLEARED_NUM_OF_ITEMS);
  }

  const bool no_selection = selection_start == selection_end;

  // Update the popup for the change, in the process changing to keyword mode
  // if the user hit space in mid-string after a keyword.
  // |allow_exact_keyword_match_| will be used by StartAutocomplete() method,
  // which will be called by |view_->UpdatePopup()|; so after that returns we
  // can safely reset this flag.
  allow_exact_keyword_match_ = text_differs && allow_keyword_ui_change &&
      !just_deleted_text && no_selection &&
      CreatedKeywordSearchByInsertingSpaceInMiddle(old_text, user_text_,
                                                   selection_start);
  view_->UpdatePopup();
  if (allow_exact_keyword_match_) {
    UMA_HISTOGRAM_ENUMERATION(kEnteredKeywordModeHistogram,
                              ENTERED_KEYWORD_MODE_VIA_SPACE_IN_MIDDLE,
                              ENTERED_KEYWORD_MODE_NUM_ITEMS);
    allow_exact_keyword_match_ = false;
  }

  // Change to keyword mode if the user is now pressing space after a keyword
  // name.  Note that if this is the case, then even if there was no keyword
  // hint when we entered this function (e.g. if the user has used space to
  // replace some selected text that was adjoined to this keyword), there will
  // be one now because of the call to UpdatePopup() above; so it's safe for
  // MaybeAcceptKeywordBySpace() to look at |keyword_| and |is_keyword_hint_| to
  // determine what keyword, if any, is applicable.
  //
  // If MaybeAcceptKeywordBySpace() accepts the keyword and returns true, that
  // will have updated our state already, so in that case we don't also return
  // true from this function.
  return !(text_differs && allow_keyword_ui_change && !just_deleted_text &&
           no_selection && (selection_start == user_text_.length()) &&
           MaybeAcceptKeywordBySpace(user_text_));
}

// TODO(beaudoin): Merge OnPopupDataChanged with this method once the popup
// handling has completely migrated to omnibox_controller.
void OmniboxEditModel::OnCurrentMatchChanged() {
  has_temporary_text_ = false;

  const AutocompleteMatch& match = omnibox_controller_->current_match();

  client_->OnCurrentMatchChanged(match);

  // We store |keyword| and |is_keyword_hint| in temporary variables since
  // OnPopupDataChanged use their previous state to detect changes.
  base::string16 keyword;
  bool is_keyword_hint;
  TemplateURLService* service = client_->GetTemplateURLService();
  match.GetKeywordUIState(service, &keyword, &is_keyword_hint);
  if (popup_model())
    popup_model()->OnResultChanged();
  // OnPopupDataChanged() resets OmniboxController's |current_match_| early
  // on.  Therefore, copy match.inline_autocompletion to a temp to preserve
  // its value across the entire call.
  const base::string16 inline_autocompletion(match.inline_autocompletion);
  OnPopupDataChanged(inline_autocompletion, NULL, keyword, is_keyword_hint);
}

// static
const char OmniboxEditModel::kCutOrCopyAllTextHistogram[] =
    "Omnibox.CutOrCopyAllText";

bool OmniboxEditModel::query_in_progress() const {
  return !autocomplete_controller()->done();
}

void OmniboxEditModel::InternalSetUserText(const base::string16& text) {
  user_text_ = text;
  just_deleted_text_ = false;
  inline_autocomplete_text_.clear();
  view_->OnInlineAutocompleteTextCleared();
}

void OmniboxEditModel::ClearPopupKeywordMode() const {
  omnibox_controller_->ClearPopupKeywordMode();
}

base::string16 OmniboxEditModel::DisplayTextFromUserText(
    const base::string16& text) const {
  return is_keyword_selected() ?
      KeywordProvider::SplitReplacementStringFromInput(text, false) : text;
}

base::string16 OmniboxEditModel::UserTextFromDisplayText(
    const base::string16& text) const {
  return is_keyword_selected() ? (keyword_ + base::char16(' ') + text) : text;
}

void OmniboxEditModel::GetInfoForCurrentText(AutocompleteMatch* match,
                                             GURL* alternate_nav_url) const {
  DCHECK(match != NULL);

  if (controller_->GetToolbarModel()->WouldPerformSearchTermReplacement(
      false)) {
    // Any time the user hits enter on the unchanged omnibox, we should reload.
    // When we're not extracting search terms, AcceptInput() will take care of
    // this (see code referring to PAGE_TRANSITION_RELOAD there), but when we're
    // extracting search terms, the conditionals there won't fire, so we
    // explicitly set up a match that will reload here.

    // It's important that we fetch the current visible URL to reload instead of
    // just getting a "search what you typed" URL from
    // SearchProvider::CreateSearchSuggestion(), since the user may be in a
    // non-default search mode such as image search.
    match->type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
    match->provider = autocomplete_controller()->search_provider();
    match->destination_url = client_->GetURL();
    match->transition = ui::PAGE_TRANSITION_RELOAD;
  } else if (query_in_progress() ||
             (popup_model() && popup_model()->IsOpen())) {
    if (query_in_progress()) {
      // It's technically possible for |result| to be empty if no provider
      // returns a synchronous result but the query has not completed
      // synchronously; pratically, however, that should never actually happen.
      if (result().empty())
        return;
      // The user cannot have manually selected a match, or the query would have
      // stopped. So the default match must be the desired selection.
      *match = *result().default_match();
    } else {
      // If there are no results, the popup should be closed, so we shouldn't
      // have gotten here.
      CHECK(!result().empty());
      CHECK(popup_model()->selected_line() < result().size());
      const AutocompleteMatch& selected_match =
          result().match_at(popup_model()->selected_line());
      *match =
          (popup_model()->selected_line_state() == OmniboxPopupModel::KEYWORD) ?
              *selected_match.associated_keyword : selected_match;
    }
    if (alternate_nav_url &&
        (!popup_model() || popup_model()->manually_selected_match().empty()))
      *alternate_nav_url = result().alternate_nav_url();
  } else {
    client_->GetAutocompleteClassifier()->Classify(
        UserTextFromDisplayText(view_->GetText()), is_keyword_selected(), true,
        ClassifyPage(), match, alternate_nav_url);
  }
}

void OmniboxEditModel::RevertTemporaryText(bool revert_popup) {
  // The user typed something, then selected a different item.  Restore the
  // text they typed and change back to the default item.
  // NOTE: This purposefully does not reset paste_state_.
  just_deleted_text_ = false;
  has_temporary_text_ = false;

  if (revert_popup && popup_model())
    popup_model()->ResetToDefaultMatch();
  view_->OnRevertTemporaryText();
}

bool OmniboxEditModel::MaybeAcceptKeywordBySpace(
    const base::string16& new_text) {
  size_t keyword_length = new_text.length() - 1;
  return (paste_state_ == NONE) && is_keyword_hint_ &&
      (keyword_.length() == keyword_length) &&
      IsSpaceCharForAcceptingKeyword(new_text[keyword_length]) &&
      !new_text.compare(0, keyword_length, keyword_, 0, keyword_length) &&
      AcceptKeyword(ENTERED_KEYWORD_MODE_VIA_SPACE_AT_END);
}

bool OmniboxEditModel::CreatedKeywordSearchByInsertingSpaceInMiddle(
    const base::string16& old_text,
    const base::string16& new_text,
    size_t caret_position) const {
  DCHECK_GE(new_text.length(), caret_position);

  // Check simple conditions first.
  if ((paste_state_ != NONE) || (caret_position < 2) ||
      (old_text.length() < caret_position) ||
      (new_text.length() == caret_position))
    return false;
  size_t space_position = caret_position - 1;
  if (!IsSpaceCharForAcceptingKeyword(new_text[space_position]) ||
      base::IsUnicodeWhitespace(new_text[space_position - 1]) ||
      new_text.compare(0, space_position, old_text, 0, space_position) ||
      !new_text.compare(space_position, new_text.length() - space_position,
                        old_text, space_position,
                        old_text.length() - space_position)) {
    return false;
  }

  // Then check if the text before the inserted space matches a keyword.
  base::string16 keyword;
  base::TrimWhitespace(new_text.substr(0, space_position), base::TRIM_LEADING,
                       &keyword);
  return !keyword.empty() && !autocomplete_controller()->keyword_provider()->
      GetKeywordForText(keyword).empty();
}

//  static
bool OmniboxEditModel::IsSpaceCharForAcceptingKeyword(wchar_t c) {
  switch (c) {
    case 0x0020:  // Space
    case 0x3000:  // Ideographic Space
      return true;
    default:
      return false;
  }
}

OmniboxEventProto::PageClassification OmniboxEditModel::ClassifyPage() const {
  if (!client_->CurrentPageExists())
    return OmniboxEventProto::OTHER;
  if (client_->IsInstantNTP()) {
    // Note that we treat OMNIBOX as the source if focus_source_ is INVALID,
    // i.e., if input isn't actually in progress.
    return (focus_source_ == FAKEBOX) ?
        OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS :
        OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS;
  }
  const GURL& gurl = client_->GetURL();
  if (!gurl.is_valid())
    return OmniboxEventProto::INVALID_SPEC;
  const std::string& url = gurl.spec();
  if (client_->IsNewTabPage(url))
    return OmniboxEventProto::NTP;
  if (url == url::kAboutBlankURL)
    return OmniboxEventProto::BLANK;
  if (client_->IsHomePage(url))
    return OmniboxEventProto::HOME_PAGE;
  if (controller_->GetToolbarModel()->WouldPerformSearchTermReplacement(true))
    return OmniboxEventProto::SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT;
  if (client_->IsSearchResultsPage())
    return OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT;
  return OmniboxEventProto::OTHER;
}

void OmniboxEditModel::ClassifyStringForPasteAndGo(
    const base::string16& text,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) const {
  DCHECK(match);
  client_->GetAutocompleteClassifier()->Classify(
      text, false, false, ClassifyPage(), match, alternate_nav_url);
}

void OmniboxEditModel::SetFocusState(OmniboxFocusState state,
                                     OmniboxFocusChangeReason reason) {
  if (state == focus_state_)
    return;

  // Update state and notify view if the omnibox has focus and the caret
  // visibility changed.
  const bool was_caret_visible = is_caret_visible();
  focus_state_ = state;
  if (focus_state_ != OMNIBOX_FOCUS_NONE &&
      is_caret_visible() != was_caret_visible)
    view_->ApplyCaretVisibility();

  client_->OnFocusChanged(focus_state_, reason);
}
