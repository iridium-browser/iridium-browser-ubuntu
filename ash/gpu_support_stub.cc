// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/gpu_support_stub.h"

namespace ash {

GPUSupportStub::GPUSupportStub() {}

GPUSupportStub::~GPUSupportStub() {}

bool GPUSupportStub::IsPanelFittingDisabled() const {
  return false;
}

}  // namespace ash
