// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/CrossOriginAccessControl.h"

#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

TEST(CreateAccessControlPreflightRequestTest, LexicographicalOrder) {
  ResourceRequest request;
  request.addHTTPHeaderField("Orange", "Orange");
  request.addHTTPHeaderField("Apple", "Red");
  request.addHTTPHeaderField("Kiwifruit", "Green");
  request.addHTTPHeaderField("Content-Type", "application/octet-stream");
  request.addHTTPHeaderField("Strawberry", "Red");

  ResourceRequest preflight = createAccessControlPreflightRequest(request);

  EXPECT_EQ("apple,content-type,kiwifruit,orange,strawberry",
            preflight.httpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest, ExcludeSimpleHeaders) {
  ResourceRequest request;
  request.addHTTPHeaderField("Accept", "everything");
  request.addHTTPHeaderField("Accept-Language", "everything");
  request.addHTTPHeaderField("Content-Language", "everything");
  request.addHTTPHeaderField("Save-Data", "on");

  ResourceRequest preflight = createAccessControlPreflightRequest(request);

  // Do not emit empty-valued headers; an empty list of non-"CORS safelisted"
  // request headers should cause "Access-Control-Request-Headers:" to be
  // left out in the preflight request.
  EXPECT_EQ(nullAtom,
            preflight.httpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest, ExcludeSimpleContentTypeHeader) {
  ResourceRequest request;
  request.addHTTPHeaderField("Content-Type", "text/plain");

  ResourceRequest preflight = createAccessControlPreflightRequest(request);

  // Empty list also; see comment in test above.
  EXPECT_EQ(nullAtom,
            preflight.httpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest, IncludeNonSimpleHeader) {
  ResourceRequest request;
  request.addHTTPHeaderField("X-Custom-Header", "foobar");

  ResourceRequest preflight = createAccessControlPreflightRequest(request);

  EXPECT_EQ("x-custom-header",
            preflight.httpHeaderField("Access-Control-Request-Headers"));
}

TEST(CreateAccessControlPreflightRequestTest,
     IncludeNonSimpleContentTypeHeader) {
  ResourceRequest request;
  request.addHTTPHeaderField("Content-Type", "application/octet-stream");

  ResourceRequest preflight = createAccessControlPreflightRequest(request);

  EXPECT_EQ("content-type",
            preflight.httpHeaderField("Access-Control-Request-Headers"));
}

}  // namespace

}  // namespace blink
