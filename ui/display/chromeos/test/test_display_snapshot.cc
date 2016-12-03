// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/test/test_display_snapshot.h"

namespace ui {
TestDisplaySnapshot::TestDisplaySnapshot()
    : DisplaySnapshot(0,
                      gfx::Point(0, 0),
                      gfx::Size(0, 0),
                      DISPLAY_CONNECTION_TYPE_UNKNOWN,
                      false,
                      false,
                      false,
                      std::string(),
                      base::FilePath(),
                      std::vector<std::unique_ptr<const DisplayMode>>(),
                      std::vector<uint8_t>(),
                      NULL,
                      NULL) {}

TestDisplaySnapshot::TestDisplaySnapshot(
    int64_t display_id,
    const gfx::Point& origin,
    const gfx::Size& physical_size,
    DisplayConnectionType type,
    bool is_aspect_preserving_scaling,
    int64_t product_id,
    bool has_color_correction_matrix,
    std::vector<std::unique_ptr<const DisplayMode>> modes,
    const DisplayMode* current_mode,
    const DisplayMode* native_mode)
    : DisplaySnapshot(display_id,
                      origin,
                      physical_size,
                      type,
                      is_aspect_preserving_scaling,
                      false,
                      has_color_correction_matrix,
                      std::string(),
                      base::FilePath(),
                      std::move(modes),
                      std::vector<uint8_t>(),
                      current_mode,
                      native_mode) {
  product_id_ = product_id;
}

TestDisplaySnapshot::~TestDisplaySnapshot() {}

std::string TestDisplaySnapshot::ToString() const { return ""; }

}  // namespace ui
