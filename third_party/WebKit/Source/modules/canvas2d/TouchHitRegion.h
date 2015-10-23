// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TouchHitRegion_h
#define TouchHitRegion_h

#include "modules/canvas2d/EventHitRegion.h"

namespace blink {

class Touch;

class TouchHitRegion : public EventHitRegion {
public:
    static String region(Touch&);
};

} // namespace blink

#endif
