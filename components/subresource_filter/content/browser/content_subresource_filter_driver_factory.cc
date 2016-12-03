// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"

#include "base/metrics/histogram_macros.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/core/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

namespace subresource_filter {

// static
const char ContentSubresourceFilterDriverFactory::kWebContentsUserDataKey[] =
    "web_contents_subresource_filter_driver_factory";

// static
void ContentSubresourceFilterDriverFactory::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<SubresourceFilterClient> client) {
  if (FromWebContents(web_contents))
    return;
  web_contents->SetUserData(kWebContentsUserDataKey,
                            new ContentSubresourceFilterDriverFactory(
                                web_contents, std::move(client)));
}

// static
ContentSubresourceFilterDriverFactory*
ContentSubresourceFilterDriverFactory::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<ContentSubresourceFilterDriverFactory*>(
      web_contents->GetUserData(kWebContentsUserDataKey));
}

ContentSubresourceFilterDriverFactory::ContentSubresourceFilterDriverFactory(
    content::WebContents* web_contents,
    std::unique_ptr<SubresourceFilterClient> client)
    : content::WebContentsObserver(web_contents),
      client_(std::move(client)),
      activation_state_(ActivationState::DISABLED) {
  content::RenderFrameHost* main_frame_host = web_contents->GetMainFrame();
  if (main_frame_host && main_frame_host->IsRenderFrameLive())
    CreateDriverForFrameHostIfNeeded(main_frame_host);
}

ContentSubresourceFilterDriverFactory::
    ~ContentSubresourceFilterDriverFactory() {}

void ContentSubresourceFilterDriverFactory::CreateDriverForFrameHostIfNeeded(
    content::RenderFrameHost* render_frame_host) {
  auto iterator_and_inserted =
      frame_drivers_.insert(std::make_pair(render_frame_host, nullptr));
  if (iterator_and_inserted.second) {
    iterator_and_inserted.first->second.reset(
        new ContentSubresourceFilterDriver(render_frame_host));
  }
}

void ContentSubresourceFilterDriverFactory::OnFirstSubresourceLoadDisallowed() {
  client_->ToggleNotificationVisibility(activation_state() ==
                                        ActivationState::ENABLED);
}

bool ContentSubresourceFilterDriverFactory::IsWhitelisted(
    const GURL& url) const {
  return whitelisted_set().find(url.host()) != whitelisted_set().end();
}

bool ContentSubresourceFilterDriverFactory::IsBlacklisted(
    const GURL& url) const {
  return activation_set().find(url.host()) != activation_set().end();
}

bool ContentSubresourceFilterDriverFactory::ShouldActivateForURL(
    const GURL& url) const {
  return IsBlacklisted(url) && !IsWhitelisted(url);
}

void ContentSubresourceFilterDriverFactory::
    OnMainResourceMatchedSafeBrowsingBlacklist(
        const GURL& url,
        const std::vector<GURL>& redirect_urls,
        safe_browsing::SBThreatType threat_type,
        safe_browsing::ThreatPatternType threat_type_metadata) {
  bool proceed = false;
  if (GetCurrentActivationList() ==
      ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL) {
    proceed = (threat_type_metadata ==
               safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  } else if (GetCurrentActivationList() ==
             ActivationList::PHISHING_INTERSTITIAL) {
    proceed = (threat_type == safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  }
  if (!proceed)
    return;
  AddHostOfURLToActivationSet(url);
  for (const auto& url : redirect_urls)
    AddHostOfURLToActivationSet(url);
}

void ContentSubresourceFilterDriverFactory::AddHostOfURLToWhitelistSet(
    const GURL& url) {
  if (!url.host().empty() && url.SchemeIsHTTPOrHTTPS())
    whitelisted_hosts_.insert(url.host());
}

void ContentSubresourceFilterDriverFactory::AddHostOfURLToActivationSet(
    const GURL& url) {
  if (!url.host().empty() && url.SchemeIsHTTPOrHTTPS())
    activate_on_hosts_.insert(url.host());
}

void ContentSubresourceFilterDriverFactory::ReadyToCommitMainFrameNavigation(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  if (ShouldActivateForMainFrameURL(url)) {
    set_activation_state(GetMaximumActivationState());
    ActivateForFrameHostIfNeeded(render_frame_host, url);
  }
}

bool ContentSubresourceFilterDriverFactory::ShouldActivateForMainFrameURL(
    const GURL& url) const {
  if (GetCurrentActivationScope() == ActivationScope::ALL_SITES)
    return !IsWhitelisted(url);
  else if (GetCurrentActivationScope() == ActivationScope::ACTIVATION_LIST)
    return ShouldActivateForURL(url);
  return false;
}

void ContentSubresourceFilterDriverFactory::ActivateForFrameHostIfNeeded(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  if (activation_state_ != ActivationState::DISABLED) {
    DriverFromFrameHost(render_frame_host)
        ->ActivateForProvisionalLoad(GetMaximumActivationState());
  }
}

void ContentSubresourceFilterDriverFactory::OnReloadRequested() {
  UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.Prompt.NumReloads", true);
  const GURL whitelist_url(web_contents()->GetLastCommittedURL());
  AddHostOfURLToWhitelistSet(whitelist_url);
  web_contents()->GetController().Reload(true);
}

void ContentSubresourceFilterDriverFactory::SetDriverForFrameHostForTesting(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<ContentSubresourceFilterDriver> driver) {
  auto iterator_and_inserted =
      frame_drivers_.insert(std::make_pair(render_frame_host, nullptr));
  iterator_and_inserted.first->second = std::move(driver);
}

ContentSubresourceFilterDriver*
ContentSubresourceFilterDriverFactory::DriverFromFrameHost(
    content::RenderFrameHost* render_frame_host) {
  auto iterator = frame_drivers_.find(render_frame_host);
  return iterator == frame_drivers_.end() ? nullptr : iterator->second.get();
}

void ContentSubresourceFilterDriverFactory::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  CreateDriverForFrameHostIfNeeded(render_frame_host);
}

void ContentSubresourceFilterDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_drivers_.erase(render_frame_host);
}

void ContentSubresourceFilterDriverFactory::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  if (!render_frame_host->GetParent()) {
    client_->ToggleNotificationVisibility(false);
    set_activation_state(ActivationState::DISABLED);
  } else {
    ActivateForFrameHostIfNeeded(render_frame_host, validated_url);
  }
}

bool ContentSubresourceFilterDriverFactory::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentSubresourceFilterDriverFactory, message)
    IPC_MESSAGE_HANDLER(SubresourceFilterHostMsg_DidDisallowFirstSubresource,
                        OnFirstSubresourceLoadDisallowed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ContentSubresourceFilterDriverFactory::set_activation_state(
    const ActivationState& new_activation_state) {
  activation_state_ = new_activation_state;
}

}  // namespace subresource_filter
