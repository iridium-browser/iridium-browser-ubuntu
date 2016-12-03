// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace chromeos {
namespace enrollment {

// Returns true if a dialog was successfully created.
bool CreateDialog(const std::string& service_path,
                  gfx::NativeWindow owning_window);

}  // namespace enrollment
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ENROLLMENT_DIALOG_VIEW_H_
