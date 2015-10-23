// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_sequencer.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/rand_util.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_utils.h"
#include "net/quic/reliable_quic_stream.h"
#include "net/quic/test_tools/quic_stream_sequencer_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::map;
using std::min;
using std::pair;
using std::string;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::CreateFunctor;
using testing::InSequence;
using testing::Return;
using testing::StrEq;

namespace net {
namespace test {

class MockStream : public ReliableQuicStream {
 public:
  MockStream(QuicSession* session, QuicStreamId id)
      : ReliableQuicStream(id, session) {
  }

  MOCK_METHOD0(OnFinRead, void());
  MOCK_METHOD0(OnDataAvailable, void());
  MOCK_METHOD2(CloseConnectionWithDetails, void(QuicErrorCode error,
                                                const string& details));
  MOCK_METHOD1(Reset, void(QuicRstStreamErrorCode error));
  MOCK_METHOD0(OnCanWrite, void());
  QuicPriority EffectivePriority() const override {
    return QuicUtils::HighestPriority();
  }
  virtual bool IsFlowControlEnabled() const {
    return true;
  }
};

namespace {

static const char kPayload[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

class QuicStreamSequencerTest : public ::testing::Test {
 public:
  void ConsumeData(size_t num_bytes) {
    char buffer[1024];
    ASSERT_GT(arraysize(buffer), num_bytes);
    struct iovec iov;
    iov.iov_base = buffer;
    iov.iov_len = num_bytes;
    ASSERT_EQ(static_cast<int>(num_bytes), sequencer_->Readv(&iov, 1));
  }

 protected:
  QuicStreamSequencerTest()
      : connection_(new MockConnection(Perspective::IS_CLIENT)),
        session_(connection_),
        stream_(&session_, 1),
        sequencer_(new QuicStreamSequencer(&stream_)) {}

  bool VerifyReadableRegions(const char** expected, size_t num_expected) {
    iovec iovecs[5];
    size_t num_iovecs =
        sequencer_->GetReadableRegions(iovecs, arraysize(iovecs));
    return VerifyIovecs(iovecs, num_iovecs, expected, num_expected);
  }

  bool VerifyIovecs(iovec* iovecs,
                    size_t num_iovecs,
                    const char** expected,
                    size_t num_expected) {
    if (num_expected != num_iovecs) {
      LOG(ERROR) << "Incorrect number of iovecs.  Expected: " << num_expected
                 << " Actual: " << num_iovecs;
      return false;
    }
    for (size_t i = 0; i < num_expected; ++i) {
      if (!VerifyIovec(iovecs[i], expected[i])) {
        return false;
      }
    }
    return true;
  }

  bool VerifyIovec(const iovec& iovec, StringPiece expected) {
    if (iovec.iov_len != expected.length()) {
      LOG(ERROR) << "Invalid length: " << iovec.iov_len
                 << " vs " << expected.length();
      return false;
    }
    if (memcmp(iovec.iov_base, expected.data(), expected.length()) != 0) {
      LOG(ERROR) << "Invalid data: " << static_cast<char*>(iovec.iov_base)
                 << " vs " << expected;
      return false;
    }
    return true;
  }

  void OnFinFrame(QuicStreamOffset byte_offset, const char* data) {
    QuicStreamFrame frame;
    frame.stream_id = 1;
    frame.offset = byte_offset;
    frame.data = StringPiece(data);
    frame.fin = true;
    sequencer_->OnStreamFrame(frame);
  }

  void OnFrame(QuicStreamOffset byte_offset, const char* data) {
    QuicStreamFrame frame;
    frame.stream_id = 1;
    frame.offset = byte_offset;
    frame.data = StringPiece(data);
    frame.fin = false;
    sequencer_->OnStreamFrame(frame);
  }

  size_t NumBufferedFrames() {
    return QuicStreamSequencerPeer::GetNumBufferedFrames(sequencer_.get());
  }

  bool FrameOverlapsBufferedData(const QuicStreamFrame& frame) {
    return QuicStreamSequencerPeer::FrameOverlapsBufferedData(sequencer_.get(),
                                                              frame);
  }

  MockConnection* connection_;
  MockQuicSpdySession session_;
  testing::StrictMock<MockStream> stream_;
  scoped_ptr<QuicStreamSequencer> sequencer_;
};

// TODO(rch): reorder these tests so they build on each other.

TEST_F(QuicStreamSequencerTest, RejectOldFrame) {
  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 3)));

  OnFrame(0, "abc");

  EXPECT_EQ(0u, NumBufferedFrames());
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());
  EXPECT_EQ(3u, stream_.flow_controller()->bytes_consumed());
  // Ignore this - it matches a past sequence number and we should not see it
  // again.
  OnFrame(0, "def");
  EXPECT_EQ(0u, NumBufferedFrames());
}

TEST_F(QuicStreamSequencerTest, RejectBufferedFrame) {
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  EXPECT_EQ(1u, NumBufferedFrames());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());

  // Ignore this - it matches a buffered frame.
  // Right now there's no checking that the payload is consistent.
  OnFrame(0, "def");
  EXPECT_EQ(1u, NumBufferedFrames());
}

TEST_F(QuicStreamSequencerTest, FullFrameConsumed) {
  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 3)));

  OnFrame(0, "abc");
  EXPECT_EQ(0u, NumBufferedFrames());
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, BlockedThenFullFrameConsumed) {
  sequencer_->SetBlockedUntilFlush();

  OnFrame(0, "abc");
  EXPECT_EQ(1u, NumBufferedFrames());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());

  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 3)));
  sequencer_->SetUnblocked();
  EXPECT_EQ(0u, NumBufferedFrames());
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());

  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 3)));
  EXPECT_FALSE(sequencer_->IsClosed());
  OnFinFrame(3, "def");
  EXPECT_TRUE(sequencer_->IsClosed());
}

TEST_F(QuicStreamSequencerTest, BlockedThenFullFrameAndFinConsumed) {
  sequencer_->SetBlockedUntilFlush();

  OnFinFrame(0, "abc");
  EXPECT_EQ(1u, NumBufferedFrames());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());

  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 3)));
  EXPECT_FALSE(sequencer_->IsClosed());
  sequencer_->SetUnblocked();
  EXPECT_TRUE(sequencer_->IsClosed());
  EXPECT_EQ(0u, NumBufferedFrames());
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, EmptyFrame) {
  EXPECT_CALL(stream_,
              CloseConnectionWithDetails(QUIC_INVALID_STREAM_FRAME, _));
  OnFrame(0, "");
  EXPECT_EQ(0u, NumBufferedFrames());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, EmptyFinFrame) {
  EXPECT_CALL(stream_, OnDataAvailable());
  OnFinFrame(0, "");
  EXPECT_EQ(0u, NumBufferedFrames());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, PartialFrameConsumed) {
  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 2)));

  OnFrame(0, "abc");
  EXPECT_EQ(1u, NumBufferedFrames());
  EXPECT_EQ(2u, sequencer_->num_bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, NextxFrameNotConsumed) {
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  EXPECT_EQ(1u, NumBufferedFrames());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  EXPECT_EQ(0, sequencer_->num_early_frames_received());
}

TEST_F(QuicStreamSequencerTest, FutureFrameNotProcessed) {
  OnFrame(3, "abc");
  EXPECT_EQ(1u, NumBufferedFrames());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  EXPECT_EQ(1, sequencer_->num_early_frames_received());
}

TEST_F(QuicStreamSequencerTest, OutOfOrderFrameProcessed) {
  // Buffer the first
  OnFrame(6, "ghi");
  EXPECT_EQ(1u, NumBufferedFrames());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  EXPECT_EQ(3u, sequencer_->num_bytes_buffered());
  // Buffer the second
  OnFrame(3, "def");
  EXPECT_EQ(2u, NumBufferedFrames());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  EXPECT_EQ(6u, sequencer_->num_bytes_buffered());

  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 9)));

  // Now process all of them at once.
  OnFrame(0, "abc");
  EXPECT_EQ(9u, sequencer_->num_bytes_consumed());
  EXPECT_EQ(0u, sequencer_->num_bytes_buffered());

  EXPECT_EQ(0u, NumBufferedFrames());
}

TEST_F(QuicStreamSequencerTest, BasicHalfCloseOrdered) {
  InSequence s;

  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 3)));
  OnFinFrame(0, "abc");

  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));
}

TEST_F(QuicStreamSequencerTest, BasicHalfCloseUnorderedWithFlush) {
  OnFinFrame(6, "");
  EXPECT_EQ(6u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  OnFrame(3, "def");
  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 6)));
  EXPECT_FALSE(sequencer_->IsClosed());
  OnFrame(0, "abc");
  EXPECT_TRUE(sequencer_->IsClosed());
}

TEST_F(QuicStreamSequencerTest, BasicHalfUnordered) {
  OnFinFrame(3, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  EXPECT_CALL(stream_, OnDataAvailable())
      .WillOnce(testing::Invoke(
          CreateFunctor(this, &QuicStreamSequencerTest::ConsumeData, 3)));
  EXPECT_FALSE(sequencer_->IsClosed());
  OnFrame(0, "abc");
  EXPECT_TRUE(sequencer_->IsClosed());
}

TEST_F(QuicStreamSequencerTest, TerminateWithReadv) {
  char buffer[3];

  OnFinFrame(3, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  EXPECT_FALSE(sequencer_->IsClosed());

  EXPECT_CALL(stream_, OnDataAvailable());
  OnFrame(0, "abc");

  iovec iov = {&buffer[0], 3};
  int bytes_read = sequencer_->Readv(&iov, 1);
  EXPECT_EQ(3, bytes_read);
  EXPECT_TRUE(sequencer_->IsClosed());
}

TEST_F(QuicStreamSequencerTest, MutipleOffsets) {
  OnFinFrame(3, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  EXPECT_CALL(stream_, Reset(QUIC_MULTIPLE_TERMINATION_OFFSETS));
  OnFinFrame(5, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  EXPECT_CALL(stream_, Reset(QUIC_MULTIPLE_TERMINATION_OFFSETS));
  OnFinFrame(1, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));

  OnFinFrame(3, "");
  EXPECT_EQ(3u, QuicStreamSequencerPeer::GetCloseOffset(sequencer_.get()));
}

class QuicSequencerRandomTest : public QuicStreamSequencerTest {
 public:
  typedef pair<int, string> Frame;
  typedef vector<Frame> FrameList;

  void CreateFrames() {
    int payload_size = arraysize(kPayload) - 1;
    int remaining_payload = payload_size;
    while (remaining_payload != 0) {
      int size = min(OneToN(6), remaining_payload);
      int index = payload_size - remaining_payload;
      list_.push_back(std::make_pair(index, string(kPayload + index, size)));
      remaining_payload -= size;
    }
  }

  QuicSequencerRandomTest() {
    CreateFrames();
  }

  int OneToN(int n) {
    return base::RandInt(1, n);
  }

  void ReadAvailableData() {
    // Read all available data
    char output[arraysize(kPayload) + 1];
    iovec iov;
    iov.iov_base = output;
    iov.iov_len = arraysize(output);
    int bytes_read = sequencer_->Readv(&iov, 1);
    EXPECT_NE(0, bytes_read);
    output_.append(output, bytes_read);
  }

  string output_;
  // Data which peek at using GetReadableRegion if we back up.
  string peeked_;
  FrameList list_;
};

// All frames are processed as soon as we have sequential data.
// Infinite buffering, so all frames are acked right away.
TEST_F(QuicSequencerRandomTest, RandomFramesNoDroppingNoBackup) {
  InSequence s;
  EXPECT_CALL(stream_, OnDataAvailable())
      .Times(AnyNumber())
      .WillRepeatedly(
          Invoke(this, &QuicSequencerRandomTest::ReadAvailableData));

  while (!list_.empty()) {
    int index = OneToN(list_.size()) - 1;
    LOG(ERROR) << "Sending index " << index << " " << list_[index].second;
    OnFrame(list_[index].first, list_[index].second.data());

    list_.erase(list_.begin() + index);
  }

  ASSERT_EQ(arraysize(kPayload) - 1, output_.size());
  EXPECT_EQ(kPayload, output_);
}

TEST_F(QuicSequencerRandomTest, RandomFramesNoDroppingBackup) {
  char buffer[10];
  iovec iov[2];
  iov[0].iov_base = &buffer[0];
  iov[0].iov_len = 5;
  iov[1].iov_base = &buffer[5];
  iov[1].iov_len = 5;

  EXPECT_CALL(stream_, OnDataAvailable()).Times(AnyNumber());

  while (output_.size() != arraysize(kPayload) - 1) {
    if (!list_.empty() && (base::RandUint64() % 2 == 0)) {  // Send data
      int index = OneToN(list_.size()) - 1;
      OnFrame(list_[index].first, list_[index].second.data());
      list_.erase(list_.begin() + index);
    } else {  // Read data
      bool has_bytes = sequencer_->HasBytesToRead();
      iovec peek_iov[20];
      int iovs_peeked = sequencer_->GetReadableRegions(peek_iov, 20);
      if (has_bytes) {
        ASSERT_LT(0, iovs_peeked);
      } else {
        ASSERT_EQ(0, iovs_peeked);
      }
      int total_bytes_to_peek = arraysize(buffer);
      for (int i = 0; i < iovs_peeked; ++i) {
        int bytes_to_peek = min<int>(peek_iov[i].iov_len, total_bytes_to_peek);
        peeked_.append(static_cast<char*>(peek_iov[i].iov_base), bytes_to_peek);
        total_bytes_to_peek -= bytes_to_peek;
        if (total_bytes_to_peek == 0) {
          break;
        }
      }
      int bytes_read = sequencer_->Readv(iov, 2);
      output_.append(buffer, bytes_read);
      ASSERT_EQ(output_.size(), peeked_.size());
    }
  }
  EXPECT_EQ(string(kPayload), output_);
  EXPECT_EQ(string(kPayload), peeked_);
}

// Same as above, just using a different method for reading.
TEST_F(QuicStreamSequencerTest, MarkConsumed) {
  InSequence s;
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  OnFrame(3, "def");
  OnFrame(6, "ghi");

  // abcdefghi buffered.
  EXPECT_EQ(9u, sequencer_->num_bytes_buffered());

  // Peek into the data.
  const char* expected[] = {"abc", "def", "ghi"};
  ASSERT_TRUE(VerifyReadableRegions(expected, arraysize(expected)));

  // Consume 1 byte.
  sequencer_->MarkConsumed(1);
  EXPECT_EQ(1u, stream_.flow_controller()->bytes_consumed());
  // Verify data.
  const char* expected2[] = {"bc", "def", "ghi"};
  ASSERT_TRUE(VerifyReadableRegions(expected2, arraysize(expected2)));
  EXPECT_EQ(8u, sequencer_->num_bytes_buffered());

  // Consume 2 bytes.
  sequencer_->MarkConsumed(2);
  EXPECT_EQ(3u, stream_.flow_controller()->bytes_consumed());
  // Verify data.
  const char* expected3[] = {"def", "ghi"};
  ASSERT_TRUE(VerifyReadableRegions(expected3, arraysize(expected3)));
  EXPECT_EQ(6u, sequencer_->num_bytes_buffered());

  // Consume 5 bytes.
  sequencer_->MarkConsumed(5);
  EXPECT_EQ(8u, stream_.flow_controller()->bytes_consumed());
  // Verify data.
  const char* expected4[] = {"i"};
  ASSERT_TRUE(VerifyReadableRegions(expected4, arraysize(expected4)));
  EXPECT_EQ(1u, sequencer_->num_bytes_buffered());
}

TEST_F(QuicStreamSequencerTest, MarkConsumedError) {
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  OnFrame(9, "jklmnopqrstuvwxyz");

  // Peek into the data.  Only the first chunk should be readable because of the
  // missing data.
  const char* expected[] = {"abc"};
  ASSERT_TRUE(VerifyReadableRegions(expected, arraysize(expected)));

  // Now, attempt to mark consumed more data than was readable and expect the
  // stream to be closed.
  EXPECT_CALL(stream_, Reset(QUIC_ERROR_PROCESSING_STREAM));
  EXPECT_DFATAL(sequencer_->MarkConsumed(4),
                "Invalid argument to MarkConsumed.  num_bytes_consumed_: 3 "
                "end_offset: 4 offset: 9 length: 17");
}

TEST_F(QuicStreamSequencerTest, MarkConsumedWithMissingPacket) {
  InSequence s;
  EXPECT_CALL(stream_, OnDataAvailable());

  OnFrame(0, "abc");
  OnFrame(3, "def");
  // Missing packet: 6, ghi.
  OnFrame(9, "jkl");

  const char* expected[] = {"abc", "def"};
  ASSERT_TRUE(VerifyReadableRegions(expected, arraysize(expected)));

  sequencer_->MarkConsumed(6);
}

TEST_F(QuicStreamSequencerTest, FrameOverlapsBufferedData) {
  // Ensure that FrameOverlapsBufferedData returns appropriate responses when
  // there is existing data buffered.
  const int kBufferedOffset = 10;
  const int kBufferedDataLength = 3;
  const int kNewDataLength = 3;
  string data(kNewDataLength, '.');

  // No overlap if no buffered frames.
  EXPECT_EQ(0u, NumBufferedFrames());
  // Add a buffered frame.
  sequencer_->OnStreamFrame(QuicStreamFrame(1, false, kBufferedOffset,
                                            string(kBufferedDataLength, '.')));

  // New byte range partially overlaps with buffered frame, start offset
  // preceding buffered frame.
  EXPECT_TRUE(FrameOverlapsBufferedData(
      QuicStreamFrame(1, false, kBufferedOffset - 1, data)));
  EXPECT_TRUE(FrameOverlapsBufferedData(
      QuicStreamFrame(1, false, kBufferedOffset - kNewDataLength + 1, data)));

  // New byte range partially overlaps with buffered frame, start offset inside
  // existing buffered frame.
  EXPECT_TRUE(FrameOverlapsBufferedData(
      QuicStreamFrame(1, false, kBufferedOffset + 1, data)));
  EXPECT_TRUE(FrameOverlapsBufferedData(QuicStreamFrame(
      1, false, kBufferedOffset + kBufferedDataLength - 1, data)));

  // New byte range entirely outside of buffered frames, start offset preceeding
  // buffered frame.
  EXPECT_FALSE(FrameOverlapsBufferedData(
      QuicStreamFrame(1, false, kBufferedOffset - kNewDataLength, data)));

  // New byte range entirely outside of buffered frames, start offset later than
  // buffered frame.
  EXPECT_FALSE(FrameOverlapsBufferedData(
      QuicStreamFrame(1, false, kBufferedOffset + kBufferedDataLength, data)));
}

TEST_F(QuicStreamSequencerTest, DontAcceptOverlappingFrames) {
  // The peer should never send us non-identical stream frames which contain
  // overlapping byte ranges - if they do, we close the connection.

  QuicStreamFrame frame1(kClientDataStreamId1, false, 1, StringPiece("hello"));
  sequencer_->OnStreamFrame(frame1);

  QuicStreamFrame frame2(kClientDataStreamId1, false, 2, StringPiece("hello"));
  EXPECT_TRUE(FrameOverlapsBufferedData(frame2));
  EXPECT_CALL(stream_, CloseConnectionWithDetails(QUIC_INVALID_STREAM_FRAME, _))
      .Times(1);
  sequencer_->OnStreamFrame(frame2);
}

}  // namespace
}  // namespace test
}  // namespace net
