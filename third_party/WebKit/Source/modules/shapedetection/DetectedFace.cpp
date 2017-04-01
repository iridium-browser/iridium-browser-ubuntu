// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/DetectedFace.h"

#include "core/dom/DOMRect.h"

namespace blink {

DetectedFace* DetectedFace::create() {
  return new DetectedFace(DOMRect::create());
}

DetectedFace* DetectedFace::create(DOMRect* boundingBox) {
  return new DetectedFace(boundingBox);
}

DOMRect* DetectedFace::boundingBox() const {
  return m_boundingBox.get();
}

DetectedFace::DetectedFace(DOMRect* boundingBox) : m_boundingBox(boundingBox) {}

DEFINE_TRACE(DetectedFace) {
  visitor->trace(m_boundingBox);
}

}  // namespace blink
