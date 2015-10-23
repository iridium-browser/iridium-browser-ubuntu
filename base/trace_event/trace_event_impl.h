// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BASE_TRACE_EVENT_TRACE_EVENT_IMPL_H_
#define BASE_TRACE_EVENT_TRACE_EVENT_IMPL_H_

#include <stack>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event_memory_overhead.h"

namespace base {

class WaitableEvent;
class MessageLoop;

namespace trace_event {

typedef base::Callback<bool(const char* category_group_name,
                            const char* event_name)> ArgumentFilterPredicate;

// For any argument of type TRACE_VALUE_TYPE_CONVERTABLE the provided
// class must implement this interface.
class BASE_EXPORT ConvertableToTraceFormat
    : public RefCounted<ConvertableToTraceFormat> {
 public:
  // Append the class info to the provided |out| string. The appended
  // data must be a valid JSON object. Strings must be properly quoted, and
  // escaped. There is no processing applied to the content after it is
  // appended.
  virtual void AppendAsTraceFormat(std::string* out) const = 0;

  virtual void EstimateTraceMemoryOverhead(TraceEventMemoryOverhead* overhead);

  std::string ToString() const {
    std::string result;
    AppendAsTraceFormat(&result);
    return result;
  }

 protected:
  virtual ~ConvertableToTraceFormat() {}

 private:
  friend class RefCounted<ConvertableToTraceFormat>;
};

const int kTraceMaxNumArgs = 2;

struct TraceEventHandle {
  uint32 chunk_seq;
  // These numbers of bits must be kept consistent with
  // TraceBufferChunk::kMaxTrunkIndex and
  // TraceBufferChunk::kTraceBufferChunkSize (in trace_buffer.h).
  unsigned chunk_index : 26;
  unsigned event_index : 6;
};

class BASE_EXPORT TraceEvent {
 public:
  union TraceValue {
    bool as_bool;
    unsigned long long as_uint;
    long long as_int;
    double as_double;
    const void* as_pointer;
    const char* as_string;
  };

  TraceEvent();
  ~TraceEvent();

  // We don't need to copy TraceEvent except when TraceEventBuffer is cloned.
  // Use explicit copy method to avoid accidentally misuse of copy.
  void CopyFrom(const TraceEvent& other);

  void Initialize(
      int thread_id,
      TraceTicks timestamp,
      ThreadTicks thread_timestamp,
      char phase,
      const unsigned char* category_group_enabled,
      const char* name,
      unsigned long long id,
      unsigned long long context_id,
      unsigned long long bind_id,
      int num_args,
      const char** arg_names,
      const unsigned char* arg_types,
      const unsigned long long* arg_values,
      const scoped_refptr<ConvertableToTraceFormat>* convertable_values,
      unsigned int flags);

  void Reset();

  void UpdateDuration(const TraceTicks& now, const ThreadTicks& thread_now);

  void EstimateTraceMemoryOverhead(TraceEventMemoryOverhead*);

  // Serialize event data to JSON
  void AppendAsJSON(
      std::string* out,
      const ArgumentFilterPredicate& argument_filter_predicate) const;
  void AppendPrettyPrinted(std::ostringstream* out) const;

  static void AppendValueAsJSON(unsigned char type,
                                TraceValue value,
                                std::string* out);

  TraceTicks timestamp() const { return timestamp_; }
  ThreadTicks thread_timestamp() const { return thread_timestamp_; }
  char phase() const { return phase_; }
  int thread_id() const { return thread_id_; }
  TimeDelta duration() const { return duration_; }
  TimeDelta thread_duration() const { return thread_duration_; }
  unsigned long long id() const { return id_; }
  unsigned long long context_id() const { return context_id_; }
  unsigned int flags() const { return flags_; }

  // Exposed for unittesting:

  const base::RefCountedString* parameter_copy_storage() const {
    return parameter_copy_storage_.get();
  }

  const unsigned char* category_group_enabled() const {
    return category_group_enabled_;
  }

  const char* name() const { return name_; }

#if defined(OS_ANDROID)
  void SendToATrace();
#endif

 private:
  // Note: these are ordered by size (largest first) for optimal packing.
  TraceTicks timestamp_;
  ThreadTicks thread_timestamp_;
  TimeDelta duration_;
  TimeDelta thread_duration_;
  // id_ can be used to store phase-specific data.
  unsigned long long id_;
  // context_id_ is used to store context information.
  unsigned long long context_id_;
  TraceValue arg_values_[kTraceMaxNumArgs];
  const char* arg_names_[kTraceMaxNumArgs];
  scoped_refptr<ConvertableToTraceFormat> convertable_values_[kTraceMaxNumArgs];
  const unsigned char* category_group_enabled_;
  const char* name_;
  scoped_refptr<base::RefCountedString> parameter_copy_storage_;
  int thread_id_;
  char phase_;
  unsigned int flags_;
  unsigned long long bind_id_;
  unsigned char arg_types_[kTraceMaxNumArgs];

  DISALLOW_COPY_AND_ASSIGN(TraceEvent);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_EVENT_IMPL_H_
