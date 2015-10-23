// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_data_fetcher.h"

#include "base/basictypes.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/manifest/manifest_icon_downloader.h"
#include "chrome/browser/manifest/manifest_icon_selector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/web_application_info.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/favicon/core/favicon_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/screen.h"
#include "url/gurl.h"

using content::Manifest;

// Android's preferred icon size in DP is 48, as defined in
// http://developer.android.com/design/style/iconography.html
const int AddToHomescreenDataFetcher::kPreferredIconSizeInDp = 48;

AddToHomescreenDataFetcher::AddToHomescreenDataFetcher(
    content::WebContents* web_contents,
    Observer* observer)
    : WebContentsObserver(web_contents),
      weak_observer_(observer),
      is_waiting_for_web_application_info_(false),
      is_icon_saved_(false),
      is_ready_(false),
      icon_timeout_timer_(false, false),
      shortcut_info_(dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
                     web_contents->GetURL())),
      preferred_icon_size_in_px_(kPreferredIconSizeInDp *
          gfx::Screen::GetScreenFor(web_contents->GetNativeView())->
              GetPrimaryDisplay().device_scale_factor()) {
  // Send a message to the renderer to retrieve information about the page.
  is_waiting_for_web_application_info_ = true;
  Send(new ChromeViewMsg_GetWebApplicationInfo(routing_id()));
}

void AddToHomescreenDataFetcher::OnDidGetWebApplicationInfo(
    const WebApplicationInfo& received_web_app_info) {
  is_waiting_for_web_application_info_ = false;
  if (!web_contents() || !weak_observer_) return;

  // Sanitize received_web_app_info.
  WebApplicationInfo web_app_info = received_web_app_info;
  web_app_info.title =
      web_app_info.title.substr(0, chrome::kMaxMetaTagAttributeLength);
  web_app_info.description =
      web_app_info.description.substr(0, chrome::kMaxMetaTagAttributeLength);

  // Simply set the user-editable title to be the page's title
  shortcut_info_.user_title = web_app_info.title.empty()
                                    ? web_contents()->GetTitle()
                                    : web_app_info.title;
  shortcut_info_.short_name = shortcut_info_.user_title;
  shortcut_info_.name = shortcut_info_.user_title;

  if (web_app_info.mobile_capable == WebApplicationInfo::MOBILE_CAPABLE ||
      web_app_info.mobile_capable == WebApplicationInfo::MOBILE_CAPABLE_APPLE) {
    shortcut_info_.display = blink::WebDisplayModeStandalone;
  }

  // Record what type of shortcut was added by the user.
  switch (web_app_info.mobile_capable) {
    case WebApplicationInfo::MOBILE_CAPABLE:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcut"));
      break;
    case WebApplicationInfo::MOBILE_CAPABLE_APPLE:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcutApple"));
      break;
    case WebApplicationInfo::MOBILE_CAPABLE_UNSPECIFIED:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.Bookmark"));
      break;
  }

  web_contents()->GetManifest(
      base::Bind(&AddToHomescreenDataFetcher::OnDidGetManifest, this));
}

void AddToHomescreenDataFetcher::OnDidGetManifest(
    const content::Manifest& manifest) {
  if (!web_contents() || !weak_observer_) return;

  if (!manifest.IsEmpty()) {
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.Manifest"));
      shortcut_info_.UpdateFromManifest(manifest);
  }

  GURL icon_src = ManifestIconSelector::FindBestMatchingIcon(
      manifest.icons,
      kPreferredIconSizeInDp,
      gfx::Screen::GetScreenFor(web_contents()->GetNativeView()));

  // If fetching the Manifest icon fails, fallback to the best favicon
  // for the page.
  if (!ManifestIconDownloader::Download(
        web_contents(),
        icon_src,
        kPreferredIconSizeInDp,
        base::Bind(&AddToHomescreenDataFetcher::OnManifestIconFetched,
                   this))) {
    FetchFavicon();
  }

  weak_observer_->OnUserTitleAvailable(shortcut_info_.user_title);

  // Kick off a timeout for downloading the icon.  If an icon isn't set within
  // the timeout, fall back to using a dynamically-generated launcher icon.
  icon_timeout_timer_.Start(FROM_HERE,
                            base::TimeDelta::FromMilliseconds(3000),
                            base::Bind(
                                &AddToHomescreenDataFetcher::OnFaviconFetched,
                                this,
                                favicon_base::FaviconRawBitmapResult()));
}

bool AddToHomescreenDataFetcher::OnMessageReceived(
    const IPC::Message& message) {
  if (!is_waiting_for_web_application_info_) return false;

  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(AddToHomescreenDataFetcher, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DidGetWebApplicationInfo,
                        OnDidGetWebApplicationInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

AddToHomescreenDataFetcher::~AddToHomescreenDataFetcher() {
  DCHECK(!weak_observer_);
}

void AddToHomescreenDataFetcher::FetchFavicon() {
  if (!web_contents() || !weak_observer_) return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  // Grab the best, largest icon we can find to represent this bookmark.
  // TODO(dfalcantara): Try combining with the new BookmarksHandler once its
  //                    rewrite is further along.
  std::vector<int> icon_types;
  icon_types.push_back(favicon_base::FAVICON);
  icon_types.push_back(favicon_base::TOUCH_PRECOMPOSED_ICON |
                       favicon_base::TOUCH_ICON);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  // Using favicon if its size is not smaller than platform required size,
  // otherwise using the largest icon among all avaliable icons.
  int threshold_to_get_any_largest_icon = preferred_icon_size_in_px_ - 1;
  favicon_service->GetLargestRawFaviconForPageURL(
      shortcut_info_.url,
      icon_types,
      threshold_to_get_any_largest_icon,
      base::Bind(&AddToHomescreenDataFetcher::OnFaviconFetched, this),
      &favicon_task_tracker_);
}

void AddToHomescreenDataFetcher::OnFaviconFetched(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (!web_contents() || !weak_observer_ || is_icon_saved_)
    return;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&AddToHomescreenDataFetcher::CreateLauncherIcon,
                 this,
                 bitmap_result));
}

void AddToHomescreenDataFetcher::CreateLauncherIcon(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (!web_contents() || !weak_observer_) return;

  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  SkBitmap icon_bitmap;
  if (bitmap_result.is_valid()) {
    gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                          bitmap_result.bitmap_data->size(),
                          &icon_bitmap);
  }

  if (weak_observer_) {
    icon_bitmap = weak_observer_->FinalizeLauncherIcon(icon_bitmap,
                                                       shortcut_info_.url);
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&AddToHomescreenDataFetcher::NotifyObserver,
                 this,
                 icon_bitmap));
}

void AddToHomescreenDataFetcher::OnManifestIconFetched(const SkBitmap& icon) {
  if (icon.drawsNothing()) {
    FetchFavicon();
    return;
  }
  NotifyObserver(icon);
}

void AddToHomescreenDataFetcher::NotifyObserver(const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents() || !weak_observer_ || is_icon_saved_)
    return;

  is_icon_saved_ = true;
  shortcut_icon_ = bitmap;
  is_ready_ = true;
  weak_observer_->OnDataAvailable(shortcut_info_, shortcut_icon_);
}
