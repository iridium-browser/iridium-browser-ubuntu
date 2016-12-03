// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_IMPL_PROTOCOL_EVENT_BUFFER_H_
#define COMPONENTS_SYNC_CORE_IMPL_PROTOCOL_EVENT_BUFFER_H_

#include <stddef.h>

#include <deque>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"

namespace syncer {

class ProtocolEvent;

// A container for ProtocolEvents.
//
// Stores at most kBufferSize events, then starts dropping the oldest events.
class ProtocolEventBuffer {
 public:
  static const size_t kBufferSize;

  ProtocolEventBuffer();
  ~ProtocolEventBuffer();

  // Records an event.  May cause the oldest event to be dropped.
  void RecordProtocolEvent(const ProtocolEvent& event);

  // Returns the buffered contents.  Will not clear the buffer.
  ScopedVector<ProtocolEvent> GetBufferedProtocolEvents() const;

 private:
  std::deque<ProtocolEvent*> buffer_;
  base::STLElementDeleter<std::deque<ProtocolEvent*>> buffer_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolEventBuffer);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_IMPL_PROTOCOL_EVENT_BUFFER_H_
