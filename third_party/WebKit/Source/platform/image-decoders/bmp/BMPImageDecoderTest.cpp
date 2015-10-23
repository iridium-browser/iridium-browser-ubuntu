// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/image-decoders/bmp/BMPImageDecoder.h"

#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include <gtest/gtest.h>

namespace blink {

namespace {

PassOwnPtr<BMPImageDecoder> createDecoder()
{
    return adoptPtr(new BMPImageDecoder(ImageDecoder::AlphaNotPremultiplied, ImageDecoder::GammaAndColorProfileApplied, ImageDecoder::noDecodedImageByteLimit));
}

} // anonymous namespace

TEST(BMPImageDecoderTest, isSizeAvailable)
{
    const char* bmpFile = "/LayoutTests/fast/images/resources/lenna.bmp"; // 256x256
    RefPtr<SharedBuffer> data = readFile(bmpFile);
    ASSERT_TRUE(data.get());

    OwnPtr<BMPImageDecoder> decoder = createDecoder();
    decoder->setData(data.get(), true);
    EXPECT_TRUE(decoder->isSizeAvailable());
    EXPECT_EQ(256, decoder->size().width());
    EXPECT_EQ(256, decoder->size().height());
}

TEST(BMPImageDecoderTest, parseAndDecode)
{
    const char* bmpFile = "/LayoutTests/fast/images/resources/lenna.bmp"; // 256x256
    RefPtr<SharedBuffer> data = readFile(bmpFile);
    ASSERT_TRUE(data.get());

    OwnPtr<BMPImageDecoder> decoder = createDecoder();
    decoder->setData(data.get(), true);

    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->status());
    EXPECT_EQ(256, frame->getSkBitmap().width());
    EXPECT_EQ(256, frame->getSkBitmap().height());
    EXPECT_FALSE(decoder->failed());
}

// Test if a BMP decoder returns a proper error while decoding an empty image.
TEST(BMPImageDecoderTest, emptyImage)
{
    const char* bmpFile = "/LayoutTests/fast/images/resources/0x0.bmp"; // 0x0
    RefPtr<SharedBuffer> data = readFile(bmpFile);
    ASSERT_TRUE(data.get());

    OwnPtr<BMPImageDecoder> decoder = createDecoder();
    decoder->setData(data.get(), true);

    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_EQ(ImageFrame::FrameEmpty, frame->status());
    EXPECT_TRUE(decoder->failed());
}

} // namespace blink
