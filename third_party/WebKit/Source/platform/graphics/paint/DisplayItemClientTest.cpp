// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemClient.h"

#include "platform/testing/FakeDisplayItemClient.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

#if DCHECK_IS_ON() && !defined(UNDEFINED_SANITIZER)

TEST(DisplayItemClientTest, IsAlive) {
  EXPECT_FALSE(reinterpret_cast<DisplayItemClient*>(0x12345678)->isAlive());
  FakeDisplayItemClient* testClient = new FakeDisplayItemClient;
  EXPECT_TRUE(testClient->isAlive());
  delete testClient;
  EXPECT_FALSE(testClient->isAlive());
}

#endif

}  // namespace
}  // namespace blink
