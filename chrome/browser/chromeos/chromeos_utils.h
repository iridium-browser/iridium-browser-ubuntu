// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROMEOS_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CHROMEOS_UTILS_H_

#include "base/strings/string16.h"

namespace chromeos {

namespace chrome_device_types {

extern const char kChromebox[];
extern const char kChromebase[];
extern const char kChromebook[];

}  // namespace chrome_device_types

// Returns the name of the Chrome device type (e.g. Chromebook, Chromebox).
base::string16 GetChromeDeviceType();

// Returns the string resource ID for the name of the Chrome device type
// (e.g. IDS_CHROMEBOOK, IDS_CHROMEBOX).
int GetChromeDeviceTypeResourceId();

// Returns the name of the Chrome device type to pass to the new Gaia flow param
// 'chrometype' (returns chromebox, chromebase or chromebook).
std::string GetChromeDeviceTypeString();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEOS_UTILS_H_
