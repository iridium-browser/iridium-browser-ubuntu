// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SHORTCUT_DATA_FETCHER_H_
#define CHROME_BROWSER_ANDROID_SHORTCUT_DATA_FETCHER_H_

#include "base/basictypes.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/common/web_application_info.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/manifest.h"

namespace content {
class WebContents;
}  // namespace content

namespace IPC {
class Message;
}

class GURL;

// Aysnchronously fetches and processes data needed to create a shortcut for an
// Android Home screen launcher.
//
// Because of the various asynchronous calls made by this class, it is
// refcounted to prevent the class from being prematurely deleted.  If the
// pointer to the ShortcutHelper becomes invalid, the pipeline should kill
// itself.
class ShortcutDataFetcher
    : public base::RefCounted<ShortcutDataFetcher>,
      public content::WebContentsObserver {
 public:
  class Observer {
   public:
    // Called when the title of the page is available.
    virtual void OnTitleAvailable(const base::string16& title) = 0;

    // Converts the icon into one that can be used on the Android Home screen.
    virtual SkBitmap FinalizeLauncherIcon(const SkBitmap& icon,
                                          const GURL& url) = 0;

    // Called when all the data needed to create a shortcut is available.
    virtual void OnDataAvailable(const ShortcutInfo& info,
                                 const SkBitmap& icon) = 0;
  };

  // Initialize the fetcher by requesting the information about the page to the
  // renderer process. The initialization is asynchronous and
  // OnDidGetWebApplicationInfo is expected to be called when finished.
  ShortcutDataFetcher(content::WebContents* web_contents, Observer* observer);

  // IPC message received when the initialization is finished.
  void OnDidGetWebApplicationInfo(const WebApplicationInfo& web_app_info);

  // Called when the Manifest has been parsed, or if no Manifest was found.
  void OnDidGetManifest(const content::Manifest& manifest);

  // Accessors, etc.
  void set_weak_observer(Observer* observer) { weak_observer_ = observer; }
  bool is_ready() { return is_ready_; }
  ShortcutInfo& shortcut_info() { return shortcut_info_; }
  const SkBitmap& shortcut_icon() { return shortcut_icon_; }

  // WebContentsObserver
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~ShortcutDataFetcher() override;

  // Grabs the favicon for the current URL.
  void FetchFavicon();
  void OnFaviconFetched(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Creates the launcher icon from the given bitmap.
  void CreateLauncherIcon(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Callback run after an attempt to download manifest icon has been made.  May
  // kick off the download of a favicon if it failed.
  void OnManifestIconFetched(int id,
                             int http_status_code,
                             const GURL& url,
                             const std::vector<SkBitmap>& bitmaps,
                             const std::vector<gfx::Size>& sizes);

  // Notifies the observer that the shortcut data is all available.
  void NotifyObserver(const SkBitmap& icon);

  Observer* weak_observer_;

  bool is_waiting_for_web_application_info_;
  bool is_icon_saved_;
  bool is_ready_;
  base::Timer icon_timeout_timer_;
  ShortcutInfo shortcut_info_;

  // The icon must only be set on the UI thread for thread safety.
  SkBitmap shortcut_icon_;
  base::CancelableTaskTracker favicon_task_tracker_;

  const int preferred_icon_size_in_px_;
  static const int kPreferredIconSizeInDp;

  friend class base::RefCounted<ShortcutDataFetcher>;
  DISALLOW_COPY_AND_ASSIGN(ShortcutDataFetcher);
};

#endif  // CHROME_BROWSER_ANDROID_SHORTCUT_DATA_FETCHER_H_
