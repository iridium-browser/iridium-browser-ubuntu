// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_page.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"

InstantPage::Delegate::~Delegate() {
}

InstantPage::~InstantPage() {
  if (web_contents()) {
    SearchTabHelper::FromWebContents(web_contents())->model()->RemoveObserver(
        this);
  }
}

bool InstantPage::supports_instant() const {
  return web_contents() &&
      SearchTabHelper::FromWebContents(web_contents())->SupportsInstant();
}

const std::string& InstantPage::instant_url() const {
  return instant_url_;
}

bool InstantPage::IsLocal() const {
  return web_contents() &&
      web_contents()->GetURL() == GURL(chrome::kChromeSearchLocalNtpUrl);
}

InstantPage::InstantPage(Delegate* delegate,
                         const std::string& instant_url,
                         Profile* profile)
    : profile_(profile),
      delegate_(delegate),
      instant_url_(instant_url) {
}

void InstantPage::SetContents(content::WebContents* new_web_contents) {
  ClearContents();

  if (!new_web_contents)
    return;

  Observe(new_web_contents);
  SearchModel* model =
      SearchTabHelper::FromWebContents(web_contents())->model();
  model->AddObserver(this);

  // Already know whether the page supports instant.
  if (model->instant_support() != INSTANT_SUPPORT_UNKNOWN)
    InstantSupportDetermined(model->instant_support() == INSTANT_SUPPORT_YES);
}

bool InstantPage::ShouldProcessAboutToNavigateMainFrame() {
  return false;
}

void InstantPage::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition /* transition_type */) {
  if (!render_frame_host->GetParent() &&
      ShouldProcessAboutToNavigateMainFrame())
    delegate_->InstantPageAboutToNavigateMainFrame(web_contents(), url);
}

void InstantPage::ModelChanged(const SearchModel::State& old_state,
                               const SearchModel::State& new_state) {
  if (old_state.instant_support != new_state.instant_support)
    InstantSupportDetermined(new_state.instant_support == INSTANT_SUPPORT_YES);
}

void InstantPage::InstantSupportDetermined(bool supports_instant) {
  delegate_->InstantSupportDetermined(web_contents(), supports_instant);

  // If the page doesn't support Instant, stop listening to it.
  if (!supports_instant)
    ClearContents();
}

void InstantPage::ClearContents() {
  if (web_contents()) {
    SearchTabHelper::FromWebContents(web_contents())->model()->RemoveObserver(
        this);
  }

  Observe(NULL);
}
