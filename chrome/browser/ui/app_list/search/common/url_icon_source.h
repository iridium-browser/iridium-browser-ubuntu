// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_URL_ICON_SOURCE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_URL_ICON_SOURCE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/image_decoder.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace app_list {

// An ImageSkiaSource for icons fetched from a URL. Till the URL icon is
// fetched, the default icon (specified by it's resource id) is shown.
class UrlIconSource : public gfx::ImageSkiaSource,
                      public net::URLFetcherDelegate,
                      public ImageDecoder::ImageRequest {
 public:
  typedef base::Closure IconLoadedCallback;

  // Create a URL Icon source with the given URL. The post_process parameter
  // specifies a function to post-process the result icon before displaying it.
  UrlIconSource(const IconLoadedCallback& icon_loaded_callback,
                net::URLRequestContextGetter* context_getter,
                const GURL& icon_url,
                int icon_size,
                int default_icon_resource_id);
  ~UrlIconSource() override;

 private:
  // Invoked from GetImageForScale to download the app icon when the hosting
  // ImageSkia gets painted on screen.
  void StartIconFetch();

  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  // net::URLFetcherDelegate overrides:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // ImageDecoder::ImageRequest overrides:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  IconLoadedCallback icon_loaded_callback_;
  net::URLRequestContextGetter* context_getter_;
  const GURL icon_url_;
  const int icon_size_;
  const int default_icon_resource_id_;

  bool icon_fetch_attempted_;
  std::unique_ptr<net::URLFetcher> icon_fetcher_;

  gfx::ImageSkia icon_;

  DISALLOW_COPY_AND_ASSIGN(UrlIconSource);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_COMMON_URL_ICON_SOURCE_H_
