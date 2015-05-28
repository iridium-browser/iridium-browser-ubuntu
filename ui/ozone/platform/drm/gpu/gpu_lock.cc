// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gpu_lock.h"

#include <sys/file.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace ui {

namespace {
const char kGpuLockFile[] = "/run/frecon";
}

GpuLock::GpuLock() {
  fd_ = open(kGpuLockFile, O_RDWR);
  if (fd_ < 0) {
    PLOG(ERROR) << "Failed to open lock file '" << kGpuLockFile << "'";
    return;
  }

  VLOG(1) << "Taking write lock on '" << kGpuLockFile << "'";
  if (HANDLE_EINTR(flock(fd_, LOCK_EX)))
    PLOG(ERROR) << "Error while trying to get lock on '" << kGpuLockFile << "'";

  VLOG(1) << "Done trying to take write lock on '" << kGpuLockFile << "'";
}

GpuLock::~GpuLock() {
  // Failed to open the lock file, so nothing to do here.
  if (fd_ < 0)
    return;

  VLOG(1) << "Releasing write lock on '" << kGpuLockFile << "'";
  close(fd_);
}

}  // namespace ui
