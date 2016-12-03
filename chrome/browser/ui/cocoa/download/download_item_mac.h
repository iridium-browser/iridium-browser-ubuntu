// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_ITEM_MAC_H_
#define CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_ITEM_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/icon_manager.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

@class DownloadItemController;

namespace gfx{
class Image;
}

// A class that bridges the visible mac download items to chromium's download
// model. The owning object (DownloadItemController) must explicitly call
// |LoadIcon| if it wants to display the icon associated with this download.

class DownloadItemMac : content::DownloadItem::Observer {
 public:
  DownloadItemMac(content::DownloadItem* download,
                  DownloadItemController* controller);

  // Destructor.
  ~DownloadItemMac() override;

  // content::DownloadItem::Observer implementation
  void OnDownloadUpdated(content::DownloadItem* download) override;
  void OnDownloadOpened(content::DownloadItem* download) override;
  void OnDownloadDestroyed(content::DownloadItem* download) override;

  DownloadItemModel* download_model() { return &download_model_; }

  // Asynchronous icon loading support.
  void LoadIcon();

 private:
  // Callback for asynchronous icon loading.
  void OnExtractIconComplete(gfx::Image* icon_bitmap);

  // The download item model we represent.
  DownloadItemModel download_model_;

  // The objective-c controller object.
  DownloadItemController* item_controller_;  // weak, owns us.

  // For canceling an in progress icon request.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Stores the last known path where the file will be saved.
  base::FilePath lastFilePath_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_DOWNLOAD_DOWNLOAD_ITEM_MAC_H_
