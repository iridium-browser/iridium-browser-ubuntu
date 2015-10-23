// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/surfaces/surfaces_state.h"

namespace surfaces {

SurfacesState::SurfacesState()
    : next_id_namespace_(1u) {
}

SurfacesState::~SurfacesState() {
}

}  // namespace surfaces
