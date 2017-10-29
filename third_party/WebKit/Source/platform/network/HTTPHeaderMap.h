/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTTPHeaderMap_h
#define HTTPHeaderMap_h

#include <memory>
#include <utility>
#include "platform/PlatformExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

typedef Vector<std::pair<String, String>> CrossThreadHTTPHeaderMapData;

// FIXME: Not every header fits into a map. Notably, multiple Set-Cookie header
// fields are needed to set multiple cookies.
class PLATFORM_EXPORT HTTPHeaderMap final {
  DISALLOW_NEW();

 public:
  HTTPHeaderMap();
  ~HTTPHeaderMap();

  // Gets a copy of the data suitable for passing to another thread.
  std::unique_ptr<CrossThreadHTTPHeaderMapData> CopyData() const;

  void Adopt(std::unique_ptr<CrossThreadHTTPHeaderMapData>);

  typedef HashMap<AtomicString, AtomicString, CaseFoldingHash> MapType;
  typedef MapType::AddResult AddResult;
  typedef MapType::const_iterator const_iterator;

  size_t size() const { return headers_.size(); }
  const_iterator begin() const { return headers_.begin(); }
  const_iterator end() const { return headers_.end(); }
  const_iterator Find(const AtomicString& k) const { return headers_.find(k); }
  void Clear() { headers_.clear(); }
  bool Contains(const AtomicString& k) const { return headers_.Contains(k); }
  const AtomicString& Get(const AtomicString& k) const {
    return headers_.at(k);
  }
  AddResult Set(const AtomicString& k, const AtomicString& v) {
    return headers_.Set(k, v);
  }
  AddResult Add(const AtomicString& k, const AtomicString& v) {
    return headers_.insert(k, v);
  }
  void Remove(const AtomicString& k) { headers_.erase(k); }
  bool operator!=(const HTTPHeaderMap& rhs) const {
    return headers_ != rhs.headers_;
  }
  bool operator==(const HTTPHeaderMap& rhs) const {
    return headers_ == rhs.headers_;
  }

 private:
  HashMap<AtomicString, AtomicString, CaseFoldingHash> headers_;
};

}  // namespace blink

#endif  // HTTPHeaderMap_h
