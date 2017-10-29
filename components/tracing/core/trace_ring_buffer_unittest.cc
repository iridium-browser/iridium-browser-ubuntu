// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/trace_ring_buffer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace v2 {

namespace {

TEST(TraceRingBufferTest, BasicChunkWrapping) {
  const uint32_t kNumChunks = 5;
  const size_t kBufferSize = kChunkSize * kNumChunks;
  std::unique_ptr<uint8_t[]> storage(new uint8_t[kBufferSize]);
  TraceRingBuffer ring_buffer(storage.get(), kBufferSize);

  EXPECT_EQ(0u, ring_buffer.GetNumChunksTaken());
  uint8_t* last_chunk_end = nullptr;

  // Fill the buffer twice to test wrapping logic.
  for (uint32_t i = 0; i < kNumChunks * 2; ++i) {
    TraceRingBuffer::Chunk* chunk = ring_buffer.TakeChunk(42 /* owner */);
    ASSERT_NE(nullptr, chunk);
    EXPECT_EQ(1u, ring_buffer.GetNumChunksTaken());
    EXPECT_EQ(42u, chunk->owner());
    const uint32_t chunk_idx = i % kNumChunks;
    EXPECT_EQ(chunk_idx == 0 ? storage.get() : last_chunk_end, chunk->begin());
    const uint32_t kPayloadSize = (chunk_idx + 1) * 8;
    memset(chunk->payload(), static_cast<int>(chunk_idx + 1), kPayloadSize);
    last_chunk_end = chunk->end();
    chunk->set_used_size(kPayloadSize);
    ring_buffer.ReturnChunk(chunk);
    EXPECT_EQ(0u, ring_buffer.GetNumChunksTaken());
  }

  // Now scrape the |storage| buffer and check its contents.
  for (uint32_t chunk_idx = 0; chunk_idx < kNumChunks; ++chunk_idx) {
    uint8_t* chunk_start = storage.get() + (chunk_idx * kChunkSize);
    const uint32_t kPayloadSize = (chunk_idx + 1) * 8;
    EXPECT_EQ(kPayloadSize, *reinterpret_cast<uint32_t*>(chunk_start));
    for (uint32_t i = 0; i < kPayloadSize; ++i)
      EXPECT_EQ(chunk_idx + 1, *(chunk_start + sizeof(uint32_t) + i));
  }
}

TEST(TraceRingBufferTest, ChunkBankrupcyDoesNotCrash) {
  const size_t kNumChunks = 2;
  const size_t kBufferSize = kChunkSize * kNumChunks;
  std::unique_ptr<uint8_t[]> storage(new uint8_t[kBufferSize]);
  TraceRingBuffer ring_buffer(storage.get(), kBufferSize);

  TraceRingBuffer::Chunk* chunk1 = ring_buffer.TakeChunk(1);
  ASSERT_NE(nullptr, chunk1);

  TraceRingBuffer::Chunk* chunk2 = ring_buffer.TakeChunk(1);
  ASSERT_NE(nullptr, chunk2);

  EXPECT_EQ(2u, ring_buffer.GetNumChunksTaken());

  for (int i = 0; i < 3; ++i) {
    TraceRingBuffer::Chunk* bankrupcy_chunk = ring_buffer.TakeChunk(1);
    ASSERT_NE(nullptr, bankrupcy_chunk);
    ASSERT_TRUE(ring_buffer.IsBankrupcyChunkForTesting(bankrupcy_chunk));

    // Make sure that the memory of the bankrupty chunk can be dereferenced.
    memset(bankrupcy_chunk->begin(), 0, kChunkSize);
  }
  EXPECT_EQ(2u, ring_buffer.GetNumChunksTaken());

  // Return a chunk and check that the ring buffer is not bankrupt anymore.
  chunk2->set_used_size(42);
  ring_buffer.ReturnChunk(chunk2);
  EXPECT_EQ(1u, ring_buffer.GetNumChunksTaken());
  TraceRingBuffer::Chunk* chunk = ring_buffer.TakeChunk(1);
  ASSERT_NE(nullptr, chunk);
  ASSERT_FALSE(ring_buffer.IsBankrupcyChunkForTesting(chunk));
}

}  // namespace
}  // namespace v2
}  // namespace tracing
