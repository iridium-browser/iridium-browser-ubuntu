// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_media_sinks_observer.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/mock_screen_availability_listener.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Mock;
using testing::Return;

namespace media_router {

class PresentationMediaSinksObserverTest : public ::testing::Test {
 public:
  PresentationMediaSinksObserverTest()
      : listener_("http://example.com/presentation.html") {}
  ~PresentationMediaSinksObserverTest() override {}

  void SetUp() override {
    EXPECT_CALL(router_, RegisterMediaSinksObserver(_)).WillOnce(Return(true));
    observer_.reset(new PresentationMediaSinksObserver(
        &router_, &listener_,
        MediaSourceForPresentationUrl("http://example.com/presentation.html"),
        GURL("https://google.com")));
    EXPECT_TRUE(observer_->Init());
  }

  void TearDown() override {
    EXPECT_CALL(router_, UnregisterMediaSinksObserver(_));
    observer_.reset();
  }

  MockMediaRouter router_;
  MockScreenAvailabilityListener listener_;
  std::unique_ptr<PresentationMediaSinksObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PresentationMediaSinksObserverTest);
};

TEST_F(PresentationMediaSinksObserverTest, AvailableScreens) {
  std::vector<MediaSink> result;
  result.push_back(MediaSink("sinkId", "Sink", MediaSink::IconType::CAST));

  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(true)).Times(1);
  observer_->OnSinksReceived(result);
}

TEST_F(PresentationMediaSinksObserverTest, NoAvailableScreens) {
  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(false)).Times(1);
  observer_->OnSinksReceived(std::vector<MediaSink>());
}

TEST_F(PresentationMediaSinksObserverTest, ConsecutiveResults) {
  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(false)).Times(1);
  observer_->OnSinksReceived(std::vector<MediaSink>());
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));

  // Does not propagate result to |listener_| since result is same.
  observer_->OnSinksReceived(std::vector<MediaSink>());
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));

  // |listener_| should get result since it changed to true.
  std::vector<MediaSink> result;
  result.push_back(MediaSink("sinkId", "Sink", MediaSink::IconType::CAST));

  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(true)).Times(1);
  observer_->OnSinksReceived(result);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));

  // Does not propagate result to |listener_| since result is same.
  result.push_back(MediaSink("sinkId2", "Sink 2", MediaSink::IconType::CAST));
  observer_->OnSinksReceived(result);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));

  // |listener_| should get result since it changed to false.
  EXPECT_CALL(listener_, OnScreenAvailabilityChanged(false)).Times(1);
  observer_->OnSinksReceived(std::vector<MediaSink>());
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&listener_));
}

}  // namespace media_router
