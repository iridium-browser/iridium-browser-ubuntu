// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/ico/ICOImageDecoder.h"

#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> createDecoder() {
  return WTF::wrapUnique(
      new ICOImageDecoder(ImageDecoder::AlphaNotPremultiplied,
                          ColorBehavior::transformToTargetForTesting(),
                          ImageDecoder::noDecodedImageByteLimit));
}
}

TEST(ICOImageDecoderTests, parseAndDecodeByteByByte) {
  testByteByByteDecode(&createDecoder,
                       "/LayoutTests/images/resources/2entries.ico", 2u,
                       cAnimationNone);
  testByteByByteDecode(&createDecoder,
                       "/LayoutTests/images/resources/greenbox-3frames.cur", 3u,
                       cAnimationNone);
  testByteByByteDecode(
      &createDecoder,
      "/LayoutTests/images/resources/icon-without-and-bitmap.ico", 1u,
      cAnimationNone);
  testByteByByteDecode(&createDecoder, "/LayoutTests/images/resources/1bit.ico",
                       1u, cAnimationNone);
  testByteByByteDecode(&createDecoder,
                       "/LayoutTests/images/resources/bug653075.ico", 2u,
                       cAnimationNone);
}

}  // namespace blink
