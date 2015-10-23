// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/image-decoders/ImageDecoderTestHelpers.h"

#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/ImageFrame.h"
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"
#include "wtf/OwnPtr.h"
#include "wtf/StringHasher.h"
#include <gtest/gtest.h>

namespace blink {

PassRefPtr<SharedBuffer> readFile(const char* fileName)
{
    String filePath = Platform::current()->unitTestSupport()->webKitRootDir();
    filePath.append(fileName);
    return Platform::current()->unitTestSupport()->readFromFile(filePath);
}

PassRefPtr<SharedBuffer> readFile(const char* dir, const char* fileName)
{
    String filePath = Platform::current()->unitTestSupport()->webKitRootDir();
    filePath.append("/");
    filePath.append(dir);
    filePath.append("/");
    filePath.append(fileName);

    return Platform::current()->unitTestSupport()->readFromFile(filePath);
}

unsigned hashBitmap(const SkBitmap& bitmap)
{
    return StringHasher::hashMemory(bitmap.getPixels(), bitmap.getSize());
}

void createDecodingBaseline(DecoderCreator createDecoder, SharedBuffer* data, Vector<unsigned>* baselineHashes)
{
    OwnPtr<ImageDecoder> decoder = createDecoder();
    decoder->setData(data, true);
    size_t frameCount = decoder->frameCount();
    for (size_t i = 0; i < frameCount; ++i) {
        ImageFrame* frame = decoder->frameBufferAtIndex(i);
        baselineHashes->append(hashBitmap(frame->getSkBitmap()));
    }
}

void testByteByByteDecode(DecoderCreator createDecoder, const char* file, size_t expectedFrameCount, int expectedRepetitionCount)
{
    RefPtr<SharedBuffer> data = readFile(file);
    ASSERT_TRUE(data.get());

    Vector<unsigned> baselineHashes;
    createDecodingBaseline(createDecoder, data.get(), &baselineHashes);

    OwnPtr<ImageDecoder> decoder = createDecoder();

    size_t frameCount = 0;
    size_t framesDecoded = 0;

    // Pass data to decoder byte by byte.
    for (size_t length = 1; length <= data->size() && !decoder->failed(); ++length) {
        RefPtr<SharedBuffer> tempData = SharedBuffer::create(data->data(), length);
        decoder->setData(tempData.get(), length == data->size());

        EXPECT_LE(frameCount, decoder->frameCount());
        frameCount = decoder->frameCount();

        if (!decoder->isSizeAvailable())
            continue;

        ImageFrame* frame = decoder->frameBufferAtIndex(frameCount - 1);
        if (frame && frame->status() == ImageFrame::FrameComplete && framesDecoded < frameCount)
            ++framesDecoded;
    }

    EXPECT_FALSE(decoder->failed());
    EXPECT_EQ(expectedFrameCount, decoder->frameCount());
    EXPECT_EQ(expectedFrameCount, framesDecoded);
    EXPECT_EQ(expectedRepetitionCount, decoder->repetitionCount());

    ASSERT_EQ(expectedFrameCount, baselineHashes.size());
    for (size_t i = 0; i < decoder->frameCount(); i++) {
        ImageFrame* frame = decoder->frameBufferAtIndex(i);
        EXPECT_EQ(baselineHashes[i], hashBitmap(frame->getSkBitmap()));
    }
}
} // namespace blink
