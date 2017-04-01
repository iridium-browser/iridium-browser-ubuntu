// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IncrementLoadEventDelayCount_h
#define IncrementLoadEventDelayCount_h

#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class Document;

// A helper class that will increment a document's loadEventDelayCount on
// contruction and decrement it on destruction (semantics similar to RefPtr).
class IncrementLoadEventDelayCount {
  USING_FAST_MALLOC(IncrementLoadEventDelayCount);
  WTF_MAKE_NONCOPYABLE(IncrementLoadEventDelayCount);

 public:
  static std::unique_ptr<IncrementLoadEventDelayCount> create(Document&);
  ~IncrementLoadEventDelayCount();

  // Decrements the loadEventDelayCount and checks load event synchronously,
  // and thus can cause synchronous Document load event/JavaScript execution.
  // Call this only when it is safe, e.g. at the top of an async task.
  // After calling this, |this| no longer blocks document's load event and
  // will not decrement loadEventDelayCount at destruction.
  void clearAndCheckLoadEvent();

  // Increments the new document's count and decrements the old count.
  void documentChanged(Document& newDocument);

 private:
  IncrementLoadEventDelayCount(Document&);
  Persistent<Document> m_document;
};
}  // namespace blink

#endif
