// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box_fragment.h"

#include "core/layout/ng/ng_macros.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGMarginStrut NGBoxFragment::MarginStrut() const {
  WRITING_MODE_IGNORED(
      "Accessing the margin strut is fine here. Changing the writing mode"
      "establishes a new formatting context, for which a margin strut is never"
      "set for a fragment.");
  return toNGPhysicalBoxFragment(physical_fragment_)->MarginStrut();
}

}  // namespace blink
