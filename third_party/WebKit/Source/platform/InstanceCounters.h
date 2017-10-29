/*
* Copyright (C) 2012 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef InstanceCounters_h
#define InstanceCounters_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Atomics.h"

namespace blink {

class InstanceCounters {
  STATIC_ONLY(InstanceCounters);

 public:
  enum CounterType {
    kAudioHandlerCounter,
    kDocumentCounter,
    kFrameCounter,
    kJSEventListenerCounter,
    kLayoutObjectCounter,
    kMediaKeySessionCounter,
    kMediaKeysCounter,
    kNodeCounter,
    kResourceCounter,
    kScriptPromiseCounter,
    kSuspendableObjectCounter,
    kV8PerContextDataCounter,
    kWorkerGlobalScopeCounter,

    // This value must be the last.
    kCounterTypeLength,
  };

  static inline void IncrementCounter(CounterType type) {
    DCHECK_NE(type, kNodeCounter);
    AtomicIncrement(&counters_[type]);
  }

  static inline void DecrementCounter(CounterType type) {
    DCHECK_NE(type, kNodeCounter);
    AtomicDecrement(&counters_[type]);
  }

  static inline void IncrementNodeCounter() {
    DCHECK(IsMainThread());
    node_counter_++;
  }

  static inline void DecrementNodeCounter() {
    DCHECK(IsMainThread());
    node_counter_--;
  }

  PLATFORM_EXPORT static int CounterValue(CounterType);

 private:
  PLATFORM_EXPORT static int counters_[kCounterTypeLength];
  PLATFORM_EXPORT static int node_counter_;
};

}  // namespace blink

#endif  // !defined(InstanceCounters_h)
