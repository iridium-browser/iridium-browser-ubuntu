// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/render_messages.h"
#include "components/search/search.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"

SearchIPCRouter::SearchIPCRouter(content::WebContents* web_contents,
                                 Delegate* delegate,
                                 std::unique_ptr<Policy> policy)
    : WebContentsObserver(web_contents),
      delegate_(delegate),
      policy_(std::move(policy)),
      commit_counter_(0),
      is_active_tab_(false) {
  DCHECK(web_contents);
  DCHECK(delegate);
  DCHECK(policy_.get());
}

SearchIPCRouter::~SearchIPCRouter() {}

void SearchIPCRouter::OnNavigationEntryCommitted() {
  ++commit_counter_;
  Send(new ChromeViewMsg_SetPageSequenceNumber(routing_id(), commit_counter_));
}

void SearchIPCRouter::DetermineIfPageSupportsInstant() {
  Send(new ChromeViewMsg_DetermineIfPageSupportsInstant(routing_id()));
}

void SearchIPCRouter::SendChromeIdentityCheckResult(
    const base::string16& identity,
    bool identity_match) {
  if (!policy_->ShouldProcessChromeIdentityCheck())
    return;

  Send(new ChromeViewMsg_ChromeIdentityCheckResult(routing_id(), identity,
                                                   identity_match));
}

void SearchIPCRouter::SendHistorySyncCheckResult(bool sync_history) {
  if (!policy_->ShouldProcessHistorySyncCheck())
    return;

  Send(new ChromeViewMsg_HistorySyncCheckResult(routing_id(), sync_history));
}

void SearchIPCRouter::SetSuggestionToPrefetch(
    const InstantSuggestion& suggestion) {
  if (!policy_->ShouldSendSetSuggestionToPrefetch())
    return;

  Send(new ChromeViewMsg_SearchBoxSetSuggestionToPrefetch(routing_id(),
                                                          suggestion));
}

void SearchIPCRouter::SetInputInProgress(bool input_in_progress) {
  if (!policy_->ShouldSendSetInputInProgress(is_active_tab_))
    return;

  Send(new ChromeViewMsg_SearchBoxSetInputInProgress(routing_id(),
                                                     input_in_progress));
}

void SearchIPCRouter::OmniboxFocusChanged(OmniboxFocusState state,
                                          OmniboxFocusChangeReason reason) {
  if (!policy_->ShouldSendOmniboxFocusChanged())
    return;

  Send(new ChromeViewMsg_SearchBoxFocusChanged(routing_id(), state, reason));
}

void SearchIPCRouter::SendMostVisitedItems(
    const std::vector<InstantMostVisitedItem>& items) {
  if (!policy_->ShouldSendMostVisitedItems())
    return;

  Send(new ChromeViewMsg_SearchBoxMostVisitedItemsChanged(routing_id(), items));
}

void SearchIPCRouter::SendThemeBackgroundInfo(
    const ThemeBackgroundInfo& theme_info) {
  if (!policy_->ShouldSendThemeBackgroundInfo())
    return;

  Send(new ChromeViewMsg_SearchBoxThemeChanged(routing_id(), theme_info));
}

void SearchIPCRouter::Submit(const base::string16& text,
                             const EmbeddedSearchRequestParams& params) {
  if (!policy_->ShouldSubmitQuery())
    return;

  Send(new ChromeViewMsg_SearchBoxSubmit(routing_id(), text, params));
}

void SearchIPCRouter::OnTabActivated() {
  is_active_tab_ = true;
}

void SearchIPCRouter::OnTabDeactivated() {
  is_active_tab_ = false;
}

bool SearchIPCRouter::OnMessageReceived(const IPC::Message& message) {
  if (IPC_MESSAGE_CLASS(message) != ChromeMsgStart)
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!search::IsRenderedInInstantProcess(web_contents(), profile))
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchIPCRouter, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantSupportDetermined,
                        OnInstantSupportDetermined)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FocusOmnibox, OnFocusOmnibox);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxDeleteMostVisitedItem,
                        OnDeleteMostVisitedItem);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxUndoMostVisitedDeletion,
                        OnUndoMostVisitedDeletion);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SearchBoxUndoAllMostVisitedDeletions,
                        OnUndoAllMostVisitedDeletions);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_LogEvent, OnLogEvent);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_LogMostVisitedImpression,
                        OnLogMostVisitedImpression);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_LogMostVisitedNavigation,
                        OnLogMostVisitedNavigation);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_PasteAndOpenDropdown,
                        OnPasteAndOpenDropDown);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_HistorySyncCheck,
                        OnHistorySyncCheck);
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ChromeIdentityCheck,
                        OnChromeIdentityCheck);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SearchIPCRouter::OnInstantSupportDetermined(int page_seq_no,
                                                 bool instant_support) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(instant_support);
}

void SearchIPCRouter::OnFocusOmnibox(int page_seq_no,
                                     OmniboxFocusState state) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessFocusOmnibox(is_active_tab_))
    return;

  delegate_->FocusOmnibox(state);
}

void SearchIPCRouter::OnDeleteMostVisitedItem(int page_seq_no,
                                              const GURL& url) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessDeleteMostVisitedItem())
    return;

  delegate_->OnDeleteMostVisitedItem(url);
}

void SearchIPCRouter::OnUndoMostVisitedDeletion(int page_seq_no,
                                                const GURL& url) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessUndoMostVisitedDeletion())
    return;

  delegate_->OnUndoMostVisitedDeletion(url);
}

void SearchIPCRouter::OnUndoAllMostVisitedDeletions(int page_seq_no) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessUndoAllMostVisitedDeletions())
    return;

  delegate_->OnUndoAllMostVisitedDeletions();
}

void SearchIPCRouter::OnLogEvent(int page_seq_no,
                                 NTPLoggingEventType event,
                                 base::TimeDelta time) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogEvent(event, time);
}

void SearchIPCRouter::OnLogMostVisitedImpression(
    int page_seq_no, int position, NTPLoggingTileSource tile_source) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  // Logging impressions is controlled by the same policy as logging events.
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogMostVisitedImpression(position, tile_source);
}

void SearchIPCRouter::OnLogMostVisitedNavigation(
    int page_seq_no, int position, NTPLoggingTileSource tile_source) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  // Logging navigations is controlled by the same policy as logging events.
  if (!policy_->ShouldProcessLogEvent())
    return;

  delegate_->OnLogMostVisitedNavigation(position, tile_source);
}

void SearchIPCRouter::OnPasteAndOpenDropDown(int page_seq_no,
                                             const base::string16& text) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessPasteIntoOmnibox(is_active_tab_))
    return;

  delegate_->PasteIntoOmnibox(text);
}

void SearchIPCRouter::OnChromeIdentityCheck(
    int page_seq_no,
    const base::string16& identity) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessChromeIdentityCheck())
    return;

  delegate_->OnChromeIdentityCheck(identity);
}

void SearchIPCRouter::OnHistorySyncCheck(int page_seq_no) const {
  if (page_seq_no != commit_counter_)
    return;

  delegate_->OnInstantSupportDetermined(true);
  if (!policy_->ShouldProcessHistorySyncCheck())
    return;

  delegate_->OnHistorySyncCheck();
}

void SearchIPCRouter::set_delegate_for_testing(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void SearchIPCRouter::set_policy_for_testing(std::unique_ptr<Policy> policy) {
  DCHECK(policy.get());
  policy_.reset(policy.release());
}
