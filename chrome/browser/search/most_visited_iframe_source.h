// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_MOST_VISITED_IFRAME_SOURCE_H_
#define CHROME_BROWSER_SEARCH_MOST_VISITED_IFRAME_SOURCE_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/search/iframe_source.h"

// Serves HTML for displaying suggestions using iframes, e.g.
// chrome-search://suggestion/loader.html
class MostVisitedIframeSource : public IframeSource {
 public:
  MostVisitedIframeSource();
  ~MostVisitedIframeSource() override;

  // Overridden from IframeSource. Public for testing.
  void StartDataRequest(
      const std::string& path_and_query,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) override;

 protected:
  // Overridden from IframeSource:
  std::string GetSource() const override;

  bool ServesPath(const std::string& path) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MostVisitedIframeSourceTest,
                           LogEndpointIsValidWithProvider);

  DISALLOW_COPY_AND_ASSIGN(MostVisitedIframeSource);
};

#endif  // CHROME_BROWSER_SEARCH_MOST_VISITED_IFRAME_SOURCE_H_
