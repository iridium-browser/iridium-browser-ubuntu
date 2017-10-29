// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager_utilities.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"

namespace display {

namespace {

class ScopedSetInternalDisplayId {
 public:
  ScopedSetInternalDisplayId(int64_t);
  ~ScopedSetInternalDisplayId();
};

ScopedSetInternalDisplayId::ScopedSetInternalDisplayId(int64_t id) {
  Display::SetInternalDisplayId(id);
}

ScopedSetInternalDisplayId::~ScopedSetInternalDisplayId() {
  Display::SetInternalDisplayId(kInvalidDisplayId);
}

}  // namespace

TEST(DisplayUtilitiesTest, GenerateDisplayIdList) {
  DisplayIdList list;
  {
    int64_t ids[] = {10, 1};
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(1, list[0]);
    EXPECT_EQ(10, list[1]);

    int64_t three_ids[] = {10, 5, 1};
    list = GenerateDisplayIdList(std::begin(three_ids), std::end(three_ids));
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(1, list[0]);
    EXPECT_EQ(5, list[1]);
    EXPECT_EQ(10, list[2]);
  }
  {
    int64_t ids[] = {10, 100};
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    int64_t three_ids[] = {10, 100, 1000};
    list = GenerateDisplayIdList(std::begin(three_ids), std::end(three_ids));
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);
    EXPECT_EQ(1000, list[2]);
  }
  {
    ScopedSetInternalDisplayId set_internal(100);
    int64_t ids[] = {10, 100};
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);

    std::swap(ids[0], ids[1]);
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);

    int64_t three_ids[] = {10, 100, 1000};
    list = GenerateDisplayIdList(std::begin(three_ids), std::end(three_ids));
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(100, list[0]);
    EXPECT_EQ(10, list[1]);
    EXPECT_EQ(1000, list[2]);
  }
  {
    ScopedSetInternalDisplayId set_internal(10);
    int64_t ids[] = {10, 100};
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    std::swap(ids[0], ids[1]);
    list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);

    int64_t three_ids[] = {10, 100, 1000};
    list = GenerateDisplayIdList(std::begin(three_ids), std::end(three_ids));
    ASSERT_EQ(3u, list.size());
    EXPECT_EQ(10, list[0]);
    EXPECT_EQ(100, list[1]);
    EXPECT_EQ(1000, list[2]);
  }
}

TEST(DisplayUtilitiesTest, DisplayIdListToString) {
  {
    int64_t ids[] = {10, 1, 16};
    DisplayIdList list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ("1,10,16", DisplayIdListToString(list));
  }
  {
    ScopedSetInternalDisplayId set_internal(16);
    int64_t ids[] = {10, 1, 16};
    DisplayIdList list = GenerateDisplayIdList(std::begin(ids), std::end(ids));
    EXPECT_EQ("16,1,10", DisplayIdListToString(list));
  }
}

}  // namespace display
