// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/TraceWrapperMember.h"

#include "core/testing/DeathAwareScriptWrappable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TraceWrapperMemberTest, HeapVectorSwap) {
  typedef TraceWrapperMember<DeathAwareScriptWrappable> Wrapper;

  HeapVector<Wrapper> vector1;
  DeathAwareScriptWrappable* parent1 = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* child1 = DeathAwareScriptWrappable::create();
  vector1.push_back(Wrapper(parent1, child1));

  HeapVector<Wrapper> vector2;
  DeathAwareScriptWrappable* parent2 = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* child2 = DeathAwareScriptWrappable::create();
  vector2.push_back(Wrapper(parent2, child2));

  swap(vector1, vector2, parent1, parent2);
  EXPECT_EQ(parent1, vector1.front().parent());
  EXPECT_EQ(parent2, vector2.front().parent());
}

TEST(TraceWrapperMemberTest, HeapVectorSwap2) {
  typedef TraceWrapperMember<DeathAwareScriptWrappable> Wrapper;

  HeapVector<Wrapper> vector1;
  DeathAwareScriptWrappable* parent1 = DeathAwareScriptWrappable::create();
  DeathAwareScriptWrappable* child1 = DeathAwareScriptWrappable::create();
  vector1.push_back(Wrapper(parent1, child1));

  HeapVector<Member<DeathAwareScriptWrappable>> vector2;
  DeathAwareScriptWrappable* child2 = DeathAwareScriptWrappable::create();
  vector2.push_back(child2);

  swap(vector1, vector2, parent1);
  EXPECT_EQ(1u, vector1.size());
  EXPECT_EQ(child2, vector1.front().get());
  EXPECT_EQ(parent1, vector1.front().parent());
  EXPECT_EQ(1u, vector2.size());
  EXPECT_EQ(child1, vector2.front().get());
}

}  // namespace blink
