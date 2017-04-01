/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/SharedBuffer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include <algorithm>
#include <cstdlib>
#include <memory>

namespace blink {

TEST(SharedBufferTest, getAsBytes) {
  char testData0[] = "Hello";
  char testData1[] = "World";
  char testData2[] = "Goodbye";

  RefPtr<SharedBuffer> sharedBuffer =
      SharedBuffer::create(testData0, strlen(testData0));
  sharedBuffer->append(testData1, strlen(testData1));
  sharedBuffer->append(testData2, strlen(testData2));

  const size_t size = sharedBuffer->size();
  std::unique_ptr<char[]> data = wrapArrayUnique(new char[size]);
  sharedBuffer->getAsBytes(data.get(), size);

  char expectedConcatenation[] = "HelloWorldGoodbye";
  ASSERT_EQ(strlen(expectedConcatenation), size);
  EXPECT_EQ(0, memcmp(expectedConcatenation, data.get(),
                      strlen(expectedConcatenation)));
}

TEST(SharedBufferTest, getPartAsBytes) {
  char testData0[] = "Hello";
  char testData1[] = "World";
  char testData2[] = "Goodbye";

  RefPtr<SharedBuffer> sharedBuffer =
      SharedBuffer::create(testData0, strlen(testData0));
  sharedBuffer->append(testData1, strlen(testData1));
  sharedBuffer->append(testData2, strlen(testData2));

  struct TestData {
    size_t position;
    size_t size;
    const char* expected;
  } testData[] = {
      {0, 17, "HelloWorldGoodbye"}, {0, 7, "HelloWo"}, {4, 7, "oWorldG"},
  };
  for (TestData& test : testData) {
    std::unique_ptr<char[]> data = wrapArrayUnique(new char[test.size]);
    ASSERT_TRUE(
        sharedBuffer->getPartAsBytes(data.get(), test.position, test.size));
    EXPECT_EQ(0, memcmp(test.expected, data.get(), test.size));
  }
}

TEST(SharedBufferTest, getAsBytesLargeSegments) {
  Vector<char> vector0(0x4000);
  for (size_t i = 0; i < vector0.size(); ++i)
    vector0[i] = 'a';
  Vector<char> vector1(0x4000);
  for (size_t i = 0; i < vector1.size(); ++i)
    vector1[i] = 'b';
  Vector<char> vector2(0x4000);
  for (size_t i = 0; i < vector2.size(); ++i)
    vector2[i] = 'c';

  RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::adoptVector(vector0);
  sharedBuffer->append(vector1);
  sharedBuffer->append(vector2);

  const size_t size = sharedBuffer->size();
  std::unique_ptr<char[]> data = wrapArrayUnique(new char[size]);
  sharedBuffer->getAsBytes(data.get(), size);

  ASSERT_EQ(0x4000U + 0x4000U + 0x4000U, size);
  int position = 0;
  for (int i = 0; i < 0x4000; ++i) {
    EXPECT_EQ('a', data[position]);
    ++position;
  }
  for (int i = 0; i < 0x4000; ++i) {
    EXPECT_EQ('b', data[position]);
    ++position;
  }
  for (int i = 0; i < 0x4000; ++i) {
    EXPECT_EQ('c', data[position]);
    ++position;
  }
}

TEST(SharedBufferTest, copy) {
  Vector<char> testData(10000);
  std::generate(testData.begin(), testData.end(), &std::rand);

  size_t length = testData.size();
  RefPtr<SharedBuffer> sharedBuffer =
      SharedBuffer::create(testData.data(), length);
  sharedBuffer->append(testData.data(), length);
  sharedBuffer->append(testData.data(), length);
  sharedBuffer->append(testData.data(), length);
  // sharedBuffer must contain data more than segmentSize (= 0x1000) to check
  // copy().
  ASSERT_EQ(length * 4, sharedBuffer->size());

  RefPtr<SharedBuffer> clone = sharedBuffer->copy();
  ASSERT_EQ(length * 4, clone->size());
  ASSERT_EQ(0, memcmp(clone->data(), sharedBuffer->data(), clone->size()));

  clone->append(testData.data(), length);
  ASSERT_EQ(length * 5, clone->size());
}

TEST(SharedBufferTest, constructorWithSizeOnly) {
  size_t length = 10000;
  RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(length);
  ASSERT_EQ(length, sharedBuffer->size());

  // The internal flat buffer should have been resized to |length| therefore
  // getSomeData() should directly return the full size.
  const char* data;
  ASSERT_EQ(length, sharedBuffer->getSomeData(data, static_cast<size_t>(0u)));
}

}  // namespace blink
