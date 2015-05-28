// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MEDIA_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_MEDIA_DELEGATE_CHROMEOS_H_

#include "ash/media_delegate.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"

class MediaDelegateChromeOS : public ash::MediaDelegate,
                              MediaCaptureDevicesDispatcher::Observer {
 public:
  MediaDelegateChromeOS();
  ~MediaDelegateChromeOS() override;

  // ash::MediaDelegate:
  void HandleMediaNextTrack() override;
  void HandleMediaPlayPause() override;
  void HandleMediaPrevTrack() override;
  ash::MediaCaptureState GetMediaCaptureState(
      content::BrowserContext* context) override;

  // MediaCaptureDevicesDispatcher::Observer:
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       content::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

 private:
  void NotifyMediaCaptureChange();

  base::WeakPtrFactory<MediaDelegateChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaDelegateChromeOS);
};

#endif  // CHROME_BROWSER_UI_ASH_MEDIA_DELEGATE_CHROMEOS_H_
