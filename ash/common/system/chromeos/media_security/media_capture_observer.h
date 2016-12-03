// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_MEDIA_SECURITY_MEDIA_CAPTURE_OBSERVER_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_MEDIA_SECURITY_MEDIA_CAPTURE_OBSERVER_H_

namespace ash {

class MediaCaptureObserver {
 public:
  // Called when media capture state has changed.
  virtual void OnMediaCaptureChanged() = 0;

 protected:
  virtual ~MediaCaptureObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_MEDIA_SECURITY_MEDIA_CAPTURE_OBSERVER_H_
