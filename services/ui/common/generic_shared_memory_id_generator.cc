// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/common/generic_shared_memory_id_generator.h"

#include "base/atomic_sequence_num.h"

namespace ui {
namespace {

// Global atomic to generate gpu memory buffer unique IDs.
base::StaticAtomicSequenceNumber g_next_generic_shared_memory_id;

}  // namespace

gfx::GenericSharedMemoryId GetNextGenericSharedMemoryId() {
  return gfx::GenericSharedMemoryId(g_next_generic_shared_memory_id.GetNext());
}

}  // namespace ui
