// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMemoryStatistics_h
#define WebMemoryStatistics_h

#include "public/platform/WebCommon.h"

namespace blink {

struct WebMemoryStatistics {
  size_t partition_alloc_total_allocated_bytes;
  size_t blink_gc_total_allocated_bytes;

  WebMemoryStatistics()
      : partition_alloc_total_allocated_bytes(0),
        blink_gc_total_allocated_bytes(0) {}

  BLINK_EXPORT static WebMemoryStatistics Get();
};

}  // namespace blink

#endif
