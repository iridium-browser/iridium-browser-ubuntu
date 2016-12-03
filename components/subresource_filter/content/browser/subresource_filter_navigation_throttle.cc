// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_navigation_throttle.h"

#include "components/subresource_filter/content/browser/content_subresource_filter_driver.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/browser/navigation_handle.h"

namespace subresource_filter {

// static
std::unique_ptr<content::NavigationThrottle>
SubresourceFilterNavigationThrottle::Create(content::NavigationHandle* handle) {
  return std::unique_ptr<content::NavigationThrottle>(
      new SubresourceFilterNavigationThrottle(handle));
}

SubresourceFilterNavigationThrottle::SubresourceFilterNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle),
      initial_url_(navigation_handle()->GetURL()) {}

SubresourceFilterNavigationThrottle::~SubresourceFilterNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterNavigationThrottle::WillRedirectRequest() {
  if (!navigation_handle()->GetURL().SchemeIsHTTPOrHTTPS())
    return NavigationThrottle::PROCEED;
  ContentSubresourceFilterDriverFactory* driver_factory =
      ContentSubresourceFilterDriverFactory::FromWebContents(
          navigation_handle()->GetWebContents());
  // Ensure that the activation state of the subresource filter is persisted
  // beyond redirects. In case of the desktop platforms, where Safe Browsing
  // check is performed synchronously it's guaranteed that |driver_factory| has
  // the information about the activation set. But if the Safe Browsing check in
  // asynchronous, then we night miss some redirects.
  if (driver_factory->ShouldActivateForURL(initial_url_)) {
    driver_factory->AddHostOfURLToActivationSet(navigation_handle()->GetURL());
  }

  return NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
SubresourceFilterNavigationThrottle::WillProcessResponse() {
  if (!navigation_handle()->GetURL().SchemeIsHTTPOrHTTPS())
    return NavigationThrottle::PROCEED;

  ContentSubresourceFilterDriverFactory::FromWebContents(
      navigation_handle()->GetWebContents())
      ->ReadyToCommitMainFrameNavigation(
          navigation_handle()->GetRenderFrameHost(), initial_url_);

  return NavigationThrottle::PROCEED;
}

}  // namespace subresource_filter
