// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/scattered_stream_writer.h"

#include <string.h>

#include <memory>

#include "components/tracing/test/fake_scattered_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace v2 {
namespace {

const size_t kChunkSize = 8;

TEST(ScatteredStreamWriterTest, ScatteredWrites) {
  FakeScatteredBuffer delegate(kChunkSize);
  ScatteredStreamWriter ssw(&delegate);

  const uint8_t kOneByteBuf[] = {0x40};
  const uint8_t kThreeByteBuf[] = {0x50, 0x51, 0x52};
  const uint8_t kFourByteBuf[] = {0x60, 0x61, 0x62, 0x63};
  uint8_t kTwentyByteBuf[20];
  for (uint8_t i = 0; i < sizeof(kTwentyByteBuf); ++i)
    kTwentyByteBuf[i] = 0xA0 + i;

  // Writing up to the chunk size should cause only the initial extension.
  for (uint8_t i = 0; i < kChunkSize; ++i) {
    ssw.WriteByte(i);
    EXPECT_EQ(kChunkSize - i - 1, ssw.bytes_available());
  }
  EXPECT_EQ(1u, delegate.chunks().size());
  EXPECT_EQ(0u, ssw.bytes_available());

  // This extra write will cause the first extension.
  ssw.WriteBytes(kOneByteBuf, sizeof(kOneByteBuf));
  EXPECT_EQ(2u, delegate.chunks().size());
  EXPECT_EQ(7u, ssw.bytes_available());

  // This starts at offset 1, to make sure we don't hardcode any assumption
  // about alignment.
  ContiguousMemoryRange reserved_range_1 = ssw.ReserveBytes(4);
  EXPECT_EQ(2u, delegate.chunks().size());
  EXPECT_EQ(3u, ssw.bytes_available());

  ssw.WriteByte(0xFF);
  ssw.WriteBytes(kThreeByteBuf, sizeof(kThreeByteBuf));
  EXPECT_EQ(3u, delegate.chunks().size());
  EXPECT_EQ(7u, ssw.bytes_available());

  ContiguousMemoryRange reserved_range_2 = ssw.ReserveBytes(4);
  ssw.WriteBytes(kTwentyByteBuf, sizeof(kTwentyByteBuf));
  EXPECT_EQ(6u, delegate.chunks().size());
  EXPECT_EQ(7u, ssw.bytes_available());

  // Writing reserved bytes should not change the bytes_available().
  memcpy(reserved_range_1.begin, kFourByteBuf, sizeof(kFourByteBuf));
  memcpy(reserved_range_2.begin, kFourByteBuf, sizeof(kFourByteBuf));
  EXPECT_EQ(6u, delegate.chunks().size());
  EXPECT_EQ(7u, ssw.bytes_available());

  // Check that reserving more bytes than what left creates a brand new chunk
  // even if the previous one is not exhausted
  for (uint8_t i = 0; i < 5; ++i)
    ssw.WriteByte(0xFF);
  memcpy(ssw.ReserveBytes(4).begin, kFourByteBuf, sizeof(kFourByteBuf));
  memcpy(ssw.ReserveBytesUnsafe(3), kThreeByteBuf, sizeof(kThreeByteBuf));
  memcpy(ssw.ReserveBytes(3).begin, kThreeByteBuf, sizeof(kThreeByteBuf));
  memcpy(ssw.ReserveBytesUnsafe(1), kOneByteBuf, sizeof(kOneByteBuf));
  memcpy(ssw.ReserveBytes(1).begin, kOneByteBuf, sizeof(kOneByteBuf));

  EXPECT_EQ(8u, delegate.chunks().size());
  EXPECT_EQ(3u, ssw.bytes_available());

  EXPECT_EQ("0001020304050607", delegate.GetChunkAsString(0));
  EXPECT_EQ("4060616263FF5051", delegate.GetChunkAsString(1));
  EXPECT_EQ("5260616263A0A1A2", delegate.GetChunkAsString(2));
  EXPECT_EQ("A3A4A5A6A7A8A9AA", delegate.GetChunkAsString(3));
  EXPECT_EQ("ABACADAEAFB0B1B2", delegate.GetChunkAsString(4));
  EXPECT_EQ("B3FFFFFFFFFF0000", delegate.GetChunkAsString(5));
  EXPECT_EQ("6061626350515200", delegate.GetChunkAsString(6));
  EXPECT_EQ("5051524040000000", delegate.GetChunkAsString(7));

  // Finally reset the writer to a new buffer.
  uint8_t other_buffer[8] = {0};
  ssw.Reset({other_buffer, other_buffer + sizeof(other_buffer)});
  EXPECT_EQ(other_buffer, ssw.write_ptr());
  ssw.WriteByte(1);
  ssw.WriteBytes(kThreeByteBuf, sizeof(kThreeByteBuf));
  EXPECT_EQ(1u, other_buffer[0]);
  EXPECT_EQ(0x52u, other_buffer[3]);
}

}  // namespace
}  // namespace v2
}  // namespace tracing
