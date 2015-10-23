// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/capture/webm_muxer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::WithArgs;

namespace media {

// Dummy interface class to be able to MOCK its only function below.
class EventHandlerInterface {
 public:
  virtual void WriteCallback(const base::StringPiece& encoded_data) = 0;
  virtual ~EventHandlerInterface() {}
};

class WebmMuxerTest : public testing::Test, public EventHandlerInterface {
 public:
  WebmMuxerTest()
      : webm_muxer_(base::Bind(&WebmMuxerTest::WriteCallback,
                               base::Unretained(this))),
        last_encoded_length_(0),
        accumulated_position_(0) {
    EXPECT_EQ(webm_muxer_.Position(), 0);
    EXPECT_FALSE(webm_muxer_.Seekable());
    EXPECT_EQ(webm_muxer_.segment_.mode(), mkvmuxer::Segment::kLive);
  }

  MOCK_METHOD1(WriteCallback, void(const base::StringPiece&));

  void SaveEncodedDataLen(const base::StringPiece& encoded_data) {
    last_encoded_length_ = encoded_data.size();
    accumulated_position_ += encoded_data.size();
  }

  mkvmuxer::int64 GetWebmMuxerPosition() const {
    return webm_muxer_.Position();
  }

  const mkvmuxer::Segment& GetWebmMuxerSegment() const {
    return webm_muxer_.segment_;
  }

  mkvmuxer::int32 WebmMuxerWrite(const void* buf, mkvmuxer::uint32 len) {
    return webm_muxer_.Write(buf, len);
  }

  WebmMuxer webm_muxer_;

  size_t last_encoded_length_;
  int64_t accumulated_position_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebmMuxerTest);
};

// Checks that AddVideoTrack adds a Track.
TEST_F(WebmMuxerTest, AddVideoTrack) {
  const uint64_t track_number = webm_muxer_.AddVideoTrack(gfx::Size(320, 240),
                                                          30.0f);
  EXPECT_TRUE(GetWebmMuxerSegment().GetTrackByNumber(track_number));
}

// Checks that the WriteCallback is called with appropriate params when
// WebmMuxer::Write() method is called.
TEST_F(WebmMuxerTest, Write) {
  const base::StringPiece encoded_data("abcdefghijklmnopqrstuvwxyz");

  EXPECT_CALL(*this, WriteCallback(encoded_data));
  WebmMuxerWrite(encoded_data.data(), encoded_data.size());

  EXPECT_EQ(GetWebmMuxerPosition(), static_cast<int64_t>(encoded_data.size()));
}

// This test sends two frames and checks that the WriteCallback is called with
// appropriate params in both cases.
TEST_F(WebmMuxerTest, OnEncodedVideoNormalFrames) {
  const base::StringPiece encoded_data("abcdefghijklmnopqrstuvwxyz");
  const uint64_t track_number = webm_muxer_.AddVideoTrack(gfx::Size(320, 240),
                                                          30.0f);

  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(WithArgs<0>(
          Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  webm_muxer_.OnEncodedVideo(track_number,
                             encoded_data,
                             base::TimeDelta::FromMicroseconds(0),
                             false  /* keyframe */);

  // First time around WriteCallback() is pinged a number of times to write the
  // Matroska header, but at the end it dumps |encoded_data|.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  EXPECT_GE(GetWebmMuxerPosition(), static_cast<int64_t>(last_encoded_length_));

  const int64_t begin_of_second_block = accumulated_position_;
  EXPECT_CALL(*this, WriteCallback(_))
      .Times(AtLeast(1))
      .WillRepeatedly(WithArgs<0>(
          Invoke(this, &WebmMuxerTest::SaveEncodedDataLen)));
  webm_muxer_.OnEncodedVideo(track_number,
                             encoded_data,
                             base::TimeDelta::FromMicroseconds(1),
                             false  /* keyframe */);

  // The second time around the callbacks should include a SimpleBlock header,
  // namely the track index, a timestamp and a flags byte, for a total of 6B.
  EXPECT_EQ(last_encoded_length_, encoded_data.size());
  EXPECT_EQ(GetWebmMuxerPosition(), accumulated_position_);
  const uint32_t kSimpleBlockSize = 6u;
  EXPECT_EQ(static_cast<int64_t>(begin_of_second_block + kSimpleBlockSize +
                                 encoded_data.size()),
            accumulated_position_);
}

}  // namespace media
