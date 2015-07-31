// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_UI_URL_FETCHER_H
#define EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_UI_URL_FETCHER_H

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace net {
class URLFetcher;
}

// WebUIURLFetcher downloads the content of a file by giving its |url| on WebUI.
// Each WebUIURLFetcher is associated with a given |render_process_id,
// render_view_id| pair.
class WebUIURLFetcher : public net::URLFetcherDelegate {
 public:
  // Called when a file URL request is complete.
  // Parameters:
  // - whether the request is success.
  // - If yes, the content of the file.
  using WebUILoadFileCallback = base::Callback<void(bool, const std::string&)>;

  WebUIURLFetcher(content::BrowserContext* context,
                  int render_process_id,
                  int render_view_id,
                  const GURL& url,
                  const WebUILoadFileCallback& callback);
  ~WebUIURLFetcher() override;

  void Start();

 private:
  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  content::BrowserContext* context_;
  int render_process_id_;
  int render_view_id_;
  GURL url_;
  WebUILoadFileCallback callback_;
  scoped_ptr<net::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(WebUIURLFetcher);
};

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WEB_VIEW_WEB_UI_URL_FETCHER_H
