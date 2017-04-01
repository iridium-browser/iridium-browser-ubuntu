// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/device/device_manager_manual.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/location.h"
#include "base/task_scheduler/post_task.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"

namespace ui {

namespace {

void ScanDevicesOnWorkerThread(std::vector<base::FilePath>* result) {
  base::FileEnumerator file_enum(
      base::FilePath(FILE_PATH_LITERAL("/dev/input")), false,
      base::FileEnumerator::FILES, FILE_PATH_LITERAL("event*[0-9]"));
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    result->push_back(path);
  }
}
}

DeviceManagerManual::DeviceManagerManual()
    : have_scanned_devices_(false), weak_ptr_factory_(this) {
}

DeviceManagerManual::~DeviceManagerManual() {
}

void DeviceManagerManual::ScanDevices(DeviceEventObserver* observer) {
  if (have_scanned_devices_) {
    std::vector<base::FilePath>::const_iterator it = devices_.begin();
    for (; it != devices_.end(); ++it) {
      DeviceEvent event(DeviceEvent::INPUT, DeviceEvent::ADD, *it);
      observer->OnDeviceEvent(event);
    }
  } else {
    std::vector<base::FilePath>* result = new std::vector<base::FilePath>();
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, base::TaskTraits()
                       .WithShutdownBehavior(
                           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                       .MayBlock(),
        base::Bind(&ScanDevicesOnWorkerThread, result),
        base::Bind(&DeviceManagerManual::OnDevicesScanned,
                   weak_ptr_factory_.GetWeakPtr(), base::Owned(result)));
    have_scanned_devices_ = true;
  }
}

void DeviceManagerManual::AddObserver(DeviceEventObserver* observer) {
  observers_.AddObserver(observer);
}

void DeviceManagerManual::RemoveObserver(DeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceManagerManual::OnDevicesScanned(
    std::vector<base::FilePath>* result) {
  std::vector<base::FilePath>::const_iterator it = result->begin();
  for (; it != result->end(); ++it) {
    devices_.push_back(*it);
    DeviceEvent event(DeviceEvent::INPUT, DeviceEvent::ADD, *it);
    for (DeviceEventObserver& observer : observers_)
      observer.OnDeviceEvent(event);
  }
}

}  // namespace ui
