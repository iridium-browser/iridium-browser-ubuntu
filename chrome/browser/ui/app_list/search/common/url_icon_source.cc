// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/url_icon_source.h"

#include <string>
#include <utility>

#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_operations.h"

using content::BrowserThread;

namespace app_list {

UrlIconSource::UrlIconSource(const IconLoadedCallback& icon_loaded_callback,
                             net::URLRequestContextGetter* context_getter,
                             const GURL& icon_url,
                             int icon_size,
                             int default_icon_resource_id)
    : icon_loaded_callback_(icon_loaded_callback),
      context_getter_(context_getter),
      icon_url_(icon_url),
      icon_size_(icon_size),
      default_icon_resource_id_(default_icon_resource_id),
      icon_fetch_attempted_(false) {
  DCHECK(!icon_loaded_callback_.is_null());
}

UrlIconSource::~UrlIconSource() {
}

void UrlIconSource::StartIconFetch() {
  icon_fetch_attempted_ = true;

  icon_fetcher_ =
      net::URLFetcher::Create(icon_url_, net::URLFetcher::GET, this);
  icon_fetcher_->SetRequestContext(context_getter_);
  icon_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  icon_fetcher_->Start();
}

gfx::ImageSkiaRep UrlIconSource::GetImageForScale(float scale) {
  if (!icon_fetch_attempted_)
    StartIconFetch();

  if (!icon_.isNull())
    return icon_.GetRepresentation(scale);

  return ui::ResourceBundle::GetSharedInstance()
      .GetImageSkiaNamed(default_icon_resource_id_)->GetRepresentation(scale);
}

void UrlIconSource::OnURLFetchComplete(
    const net::URLFetcher* source) {
  CHECK_EQ(icon_fetcher_.get(), source);

  std::unique_ptr<net::URLFetcher> fetcher(std::move(icon_fetcher_));

  if (!fetcher->GetStatus().is_success() ||
      fetcher->GetResponseCode() != 200) {
    return;
  }

  std::string unsafe_icon_data;
  fetcher->GetResponseAsString(&unsafe_icon_data);

  ImageDecoder::Start(this, unsafe_icon_data);
}

void UrlIconSource::OnImageDecoded(const SkBitmap& decoded_image) {
  icon_ = gfx::ImageSkiaOperations::CreateResizedImage(
      gfx::ImageSkia::CreateFrom1xBitmap(decoded_image),
      skia::ImageOperations::RESIZE_BEST,
      gfx::Size(icon_size_, icon_size_));

  icon_loaded_callback_.Run();
}

void UrlIconSource::OnDecodeImageFailed() {
  // Failed to decode image. Do nothing and just use the default icon.
}

}  // namespace app_list
