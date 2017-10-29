// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/geometry/DOMPoint.h"

#include "core/geometry/DOMPointInit.h"

namespace blink {

DOMPoint* DOMPoint::Create(double x, double y, double z, double w) {
  return new DOMPoint(x, y, z, w);
}

DOMPoint* DOMPoint::fromPoint(const DOMPointInit& other) {
  return new DOMPoint(other.x(), other.y(), other.z(), other.w());
}

DOMPoint::DOMPoint(double x, double y, double z, double w)
    : DOMPointReadOnly(x, y, z, w) {}

}  // namespace blink
