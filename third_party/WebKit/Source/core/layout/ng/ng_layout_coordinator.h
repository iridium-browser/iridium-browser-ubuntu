// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutCoordinator_h
#define NGLayoutCoordinator_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class NGLayoutInputNode;
class NGConstraintSpace;

class CORE_EXPORT NGLayoutCoordinator final
    : public GarbageCollectedFinalized<NGLayoutCoordinator> {
 public:
  NGLayoutCoordinator(NGLayoutInputNode*, NGConstraintSpace*);

  bool Tick(NGPhysicalFragment**);

  DECLARE_TRACE()

  const HeapVector<Member<NGLayoutAlgorithm>>& GetAlgorithmStackForTesting()
      const {
    return layout_algorithms_;
  };

 private:
  HeapVector<Member<NGLayoutAlgorithm>> layout_algorithms_;
  Member<NGPhysicalFragment> fragment_;
};
}

#endif
