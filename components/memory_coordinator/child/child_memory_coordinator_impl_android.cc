// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/child/child_memory_coordinator_impl_android.h"

namespace memory_coordinator {

ChildMemoryCoordinatorImplAndroid::ChildMemoryCoordinatorImplAndroid(
    mojom::MemoryCoordinatorHandlePtr parent,
    ChildMemoryCoordinatorDelegate* delegate)
    : ChildMemoryCoordinatorImpl(std::move(parent), delegate) {}

ChildMemoryCoordinatorImplAndroid::~ChildMemoryCoordinatorImplAndroid() {}

void ChildMemoryCoordinatorImplAndroid::OnTrimMemory(int level) {
  // TODO(bashi): Compare |level| with levels defined in
  // ComponentCallbacks2 when JNI bindings are implemented.
  delegate()->OnTrimMemoryImmediately();
}

std::unique_ptr<ChildMemoryCoordinatorImpl> CreateChildMemoryCoordinator(
    mojom::MemoryCoordinatorHandlePtr parent,
    ChildMemoryCoordinatorDelegate* delegate) {
  return base::WrapUnique(
      new ChildMemoryCoordinatorImplAndroid(std::move(parent), delegate));
}

}  // namespace memory_coordinator
