// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/media/router/create_presentation_connection_request.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

const char kPresentationUrl[] = "http://foo.com";
const char kFrameUrl[] = "http://google.com";
const char kPresentationId[] = "presentationId";
const char kRouteId[] =
    "urn:x-org.chromium:media:route:presentationId/cast-sink1/http://foo.com";

}  // namespace

class CreatePresentationConnectionRequestTest : public ::testing::Test {
 public:
  CreatePresentationConnectionRequestTest()
      : cb_invoked_(false), render_frame_host_id_(1, 2) {}
  ~CreatePresentationConnectionRequestTest() override {}

  void OnSuccess(const content::PresentationSessionInfo& expected_info,
                 const content::PresentationSessionInfo& actual_info,
                 const MediaRoute::Id& route_id) {
    cb_invoked_ = true;
    EXPECT_EQ(expected_info.presentation_url, actual_info.presentation_url);
    EXPECT_EQ(expected_info.presentation_id, actual_info.presentation_id);
  }

  void OnError(const content::PresentationError& expected_error,
               const content::PresentationError& actual_error) {
    cb_invoked_ = true;
    EXPECT_EQ(expected_error.error_type, actual_error.error_type);
    EXPECT_EQ(expected_error.message, actual_error.message);
  }

  void FailOnSuccess(const content::PresentationSessionInfo& info,
                     const MediaRoute::Id& route_id) {
    FAIL() << "Success callback should not have been called.";
  }

  void FailOnError(const content::PresentationError& error) {
    FAIL() << "Error should not have been called.";
  }

  bool cb_invoked_;
  const RenderFrameHostId render_frame_host_id_;
};

// Test that the object's getters match the constructor parameters.
TEST_F(CreatePresentationConnectionRequestTest, Getters) {
  content::PresentationError error(content::PRESENTATION_ERROR_UNKNOWN,
                                   "Unknown error.");
  CreatePresentationConnectionRequest request(
      render_frame_host_id_, kPresentationUrl, GURL(kFrameUrl),
      base::Bind(&CreatePresentationConnectionRequestTest::FailOnSuccess,
                 base::Unretained(this)),
      base::Bind(&CreatePresentationConnectionRequestTest::OnError,
                 base::Unretained(this), error));

  PresentationRequest presentation_request(render_frame_host_id_,
                                           kPresentationUrl, GURL(kFrameUrl));
  EXPECT_TRUE(request.presentation_request().Equals(presentation_request));
  // Since we didn't explicitly call Invoke*, the error callback will be
  // invoked when |request| is destroyed.
}

TEST_F(CreatePresentationConnectionRequestTest, SuccessCallback) {
  content::PresentationSessionInfo session_info(kPresentationUrl,
                                                kPresentationId);
  CreatePresentationConnectionRequest request(
      render_frame_host_id_, kPresentationUrl, GURL(kFrameUrl),
      base::Bind(&CreatePresentationConnectionRequestTest::OnSuccess,
                 base::Unretained(this), session_info),
      base::Bind(&CreatePresentationConnectionRequestTest::FailOnError,
                 base::Unretained(this)));
  request.InvokeSuccessCallback(kPresentationId, kRouteId);
  EXPECT_TRUE(cb_invoked_);
}

TEST_F(CreatePresentationConnectionRequestTest, ErrorCallback) {
  content::PresentationError error(
      content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
      "This is an error message");
  CreatePresentationConnectionRequest request(
      render_frame_host_id_, kPresentationUrl, GURL(kFrameUrl),
      base::Bind(&CreatePresentationConnectionRequestTest::FailOnSuccess,
                 base::Unretained(this)),
      base::Bind(&CreatePresentationConnectionRequestTest::OnError,
                 base::Unretained(this), error));
  request.InvokeErrorCallback(error);
  EXPECT_TRUE(cb_invoked_);
}

}  // namespace media_router
