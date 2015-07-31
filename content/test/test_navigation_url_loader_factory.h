// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_FACTORY_H_
#define CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/loader/navigation_url_loader_factory.h"

namespace content {

class NavigationURLLoader;

// PlzNavigate
// Manages creation of the NavigationURLLoaders; when registered, all created
// NavigationURLLoaderss will be TestNavigationURLLoaderss. This automatically
// registers itself when it goes in scope, and unregisters itself when it goes
// out of scope. Since you can't have more than one factory registered at a
// time, you can only have one of these objects at a time.
class TestNavigationURLLoaderFactory : public NavigationURLLoaderFactory {
 public:
  TestNavigationURLLoaderFactory();
  ~TestNavigationURLLoaderFactory() override;

  // TestNavigationURLLoaderFactory implementation.
  scoped_ptr<NavigationURLLoader> CreateLoader(
      BrowserContext* browser_context,
      int frame_tree_node_id,
      scoped_ptr<NavigationRequestInfo> request_info,
      NavigationURLLoaderDelegate* delegate) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNavigationURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_FACTORY_H_
