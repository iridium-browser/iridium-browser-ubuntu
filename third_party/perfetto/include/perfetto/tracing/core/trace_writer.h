/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_TRACING_CORE_TRACE_WRITER_H_
#define INCLUDE_PERFETTO_TRACING_CORE_TRACE_WRITER_H_

#include "perfetto/protozero/protozero_message_handle.h"

namespace perfetto {

namespace protos {
namespace pbzero {
class TracePacket;
}  // namespace pbzero
}  // namespace protos

// This is a single-thread write interface that allows to write protobufs
// directly into the tracing shared buffer without making any copies.
// It takes care of acquiring and releasing chunks from the
// SharedMemoryArbiter and splitting protos over chunks.
// The idea is that each data source creates one (or more) TraceWriter for each
// thread it wants to write from. Each TraceWriter will get its own dedicated
// chunk and will write into the shared buffer without any locking most of the
// time. Locking will happen only when a chunk is exhausted and a new one is
// acquired from the arbiter.

// TODO: TraceWriter needs to keep the shared memory buffer alive (refcount?).
// Otherwise if the shared memory buffer goes away (e.g. the Service crashes)
// the TraceWriter will keep writing into unmapped memory.

class TraceWriter {
 public:
  using TracePacketHandle =
      protozero::ProtoZeroMessageHandle<protos::pbzero::TracePacket>;

  TraceWriter();
  virtual ~TraceWriter();

  // Returns a handle to the root proto message for the trace. The message will
  // be finalized either by calling directly handle.Finalize() or by letting the
  // handle go out of scope. The returned handle can be std::move()'d but cannot
  // be used after either: (i) the TraceWriter instance is destroyed, (ii) a
  // subsequence NewTracePacket() call is made on the same TraceWriter instance.
  virtual TracePacketHandle NewTracePacket() = 0;
  virtual WriterID writer_id() const = 0;

 private:
  TraceWriter(const TraceWriter&) = delete;
  TraceWriter& operator=(const TraceWriter&) = delete;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_CORE_TRACE_WRITER_H_
