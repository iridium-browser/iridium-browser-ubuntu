// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubsequenceDisplayItem_h
#define SubsequenceDisplayItem_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Assertions.h"

namespace blink {

class BeginSubsequenceDisplayItem final : public PairedBeginDisplayItem {
 public:
  BeginSubsequenceDisplayItem(const DisplayItemClient& client)
      : PairedBeginDisplayItem(client, kSubsequence, sizeof(*this)) {}
};

class EndSubsequenceDisplayItem final : public PairedEndDisplayItem {
 public:
  EndSubsequenceDisplayItem(const DisplayItemClient& client)
      : PairedEndDisplayItem(client, kEndSubsequence, sizeof(*this)) {}

#if DCHECK_IS_ON()
  bool isEndAndPairedWith(DisplayItem::Type otherType) const final {
    return getType() == kEndSubsequence && otherType == kSubsequence;
  }
#endif
};

}  // namespace blink

#endif  // SubsequenceDisplayItem_h
