// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_E2E_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_E2E_BROWSERTEST_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/test/media_router/media_router_base_browsertest.h"
#include "chrome/test/media_router/test_media_sinks_observer.h"

namespace media_router {

class MediaRouter;

class MediaRouterE2EBrowserTest : public MediaRouterBaseBrowserTest {
 public:
  MediaRouterE2EBrowserTest();
  ~MediaRouterE2EBrowserTest() override;

 protected:
  // InProcessBrowserTest Overrides
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // MediaRouterBaseBrowserTest Overrides
  void ParseCommandLine() override;

  // Callback from MediaRouter when a response to a media route request is
  // received.
  void OnRouteResponseReceived(const MediaRoute* route,
                               const std::string& presentation_id,
                               const std::string& error);

  // Initializes |observer_| to listen for sinks compatible with |source|,
  // finds sink with name matching receiver_, and establishes media
  // route between the source and sink.
  // |observer_| and |route_id_| will be initialized.
  // |origin| is the URL of requestor's page.
  // |tab_id| is the ID of the tab in which the request was made.
  // |origin| and |tab_id| are used for enforcing same-origin and/or same-tab
  // scope for JoinRoute() requests. (e.g., if enforced, the page
  // requesting JoinRoute() must have the same origin as the page that requested
  // CreateRoute()).
  void CreateMediaRoute(const MediaSource& source,
                        const GURL& origin,
                        int tab_id);

  // Stops the established media route and unregisters |observer_|.
  // Note that the route may not be stopped immediately, as it makes an
  // async call to the Media Route Provider.
  // |observer_| and |route_id_| will be reset.
  void StopMediaRoute();

  std::string receiver() const { return receiver_; }

  bool IsSinkDiscovered() const;
  bool IsRouteCreated() const;

 private:
  std::string receiver_;

  MediaRouter* media_router_;
  scoped_ptr<TestMediaSinksObserver> observer_;
  MediaRoute::Id route_id_;
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_E2E_BROWSERTEST_H_
