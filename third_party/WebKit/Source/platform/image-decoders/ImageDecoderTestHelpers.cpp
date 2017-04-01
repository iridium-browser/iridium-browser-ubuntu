// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/ImageDecoderTestHelpers.h"

#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageFrame.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/StringHasher.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

PassRefPtr<SharedBuffer> readFile(const char* fileName) {
  String filePath = testing::blinkRootDir();
  filePath.append(fileName);
  return testing::readFromFile(filePath);
}

PassRefPtr<SharedBuffer> readFile(const char* dir, const char* fileName) {
  StringBuilder filePath;
  filePath.append(testing::blinkRootDir());
  filePath.append('/');
  filePath.append(dir);
  filePath.append('/');
  filePath.append(fileName);
  return testing::readFromFile(filePath.toString());
}

unsigned hashBitmap(const SkBitmap& bitmap) {
  return StringHasher::hashMemory(bitmap.getPixels(), bitmap.getSize());
}

static unsigned createDecodingBaseline(DecoderCreator createDecoder,
                                       SharedBuffer* data) {
  std::unique_ptr<ImageDecoder> decoder = createDecoder();
  decoder->setData(data, true);
  ImageFrame* frame = decoder->frameBufferAtIndex(0);
  return hashBitmap(frame->bitmap());
}

void createDecodingBaseline(DecoderCreator createDecoder,
                            SharedBuffer* data,
                            Vector<unsigned>* baselineHashes) {
  std::unique_ptr<ImageDecoder> decoder = createDecoder();
  decoder->setData(data, true);
  size_t frameCount = decoder->frameCount();
  for (size_t i = 0; i < frameCount; ++i) {
    ImageFrame* frame = decoder->frameBufferAtIndex(i);
    baselineHashes->push_back(hashBitmap(frame->bitmap()));
  }
}

static void testByteByByteDecode(DecoderCreator createDecoder,
                                 SharedBuffer* data,
                                 size_t expectedFrameCount,
                                 int expectedRepetitionCount) {
  ASSERT_TRUE(data->data());

  Vector<unsigned> baselineHashes;
  createDecodingBaseline(createDecoder, data, &baselineHashes);

  std::unique_ptr<ImageDecoder> decoder = createDecoder();

  size_t frameCount = 0;
  size_t framesDecoded = 0;

  // Pass data to decoder byte by byte.
  RefPtr<SharedBuffer> sourceData[2] = {SharedBuffer::create(),
                                        SharedBuffer::create()};
  const char* source = data->data();

  for (size_t length = 1; length <= data->size() && !decoder->failed();
       ++length) {
    sourceData[0]->append(source, 1u);
    sourceData[1]->append(source++, 1u);
    // Alternate the buffers to cover the JPEGImageDecoder::onSetData restart
    // code.
    decoder->setData(sourceData[length & 1].get(), length == data->size());

    EXPECT_LE(frameCount, decoder->frameCount());
    frameCount = decoder->frameCount();

    if (!decoder->isSizeAvailable())
      continue;

    for (size_t i = framesDecoded; i < frameCount; ++i) {
      // In ICOImageDecoder memory layout could differ from frame order.
      // E.g. memory layout could be |<frame1><frame0>| and frameCount
      // would return 1 until receiving full file.
      // When file is completely received frameCount would return 2 and
      // only then both frames could be completely decoded.
      ImageFrame* frame = decoder->frameBufferAtIndex(i);
      if (frame && frame->getStatus() == ImageFrame::FrameComplete)
        ++framesDecoded;
    }
  }

  EXPECT_FALSE(decoder->failed());
  EXPECT_EQ(expectedFrameCount, decoder->frameCount());
  EXPECT_EQ(expectedFrameCount, framesDecoded);
  EXPECT_EQ(expectedRepetitionCount, decoder->repetitionCount());

  ASSERT_EQ(expectedFrameCount, baselineHashes.size());
  for (size_t i = 0; i < decoder->frameCount(); i++) {
    ImageFrame* frame = decoder->frameBufferAtIndex(i);
    EXPECT_EQ(baselineHashes[i], hashBitmap(frame->bitmap()));
  }
}

// This test verifies that calling SharedBuffer::mergeSegmentsIntoBuffer() does
// not break decoding at a critical point: in between a call to decode the size
// (when the decoder stops while it may still have input data to read) and a
// call to do a full decode.
static void testMergeBuffer(DecoderCreator createDecoder, SharedBuffer* data) {
  const unsigned hash = createDecodingBaseline(createDecoder, data);

  // In order to do any verification, this test needs to move the data owned
  // by the SharedBuffer. A way to guarantee that is to create a new one, and
  // then append a string of characters greater than kSegmentSize. This
  // results in writing the data into a segment, skipping the internal
  // contiguous buffer.
  RefPtr<SharedBuffer> segmentedData = SharedBuffer::create();
  segmentedData->append(data->data(), data->size());

  std::unique_ptr<ImageDecoder> decoder = createDecoder();
  decoder->setData(segmentedData.get(), true);

  ASSERT_TRUE(decoder->isSizeAvailable());

  // This will call SharedBuffer::mergeSegmentsIntoBuffer, copying all
  // segments into the contiguous buffer. If the ImageDecoder was pointing to
  // data in a segment, its pointer would no longer be valid.
  segmentedData->data();

  ImageFrame* frame = decoder->frameBufferAtIndex(0);
  ASSERT_FALSE(decoder->failed());
  EXPECT_EQ(frame->getStatus(), ImageFrame::FrameComplete);
  EXPECT_EQ(hashBitmap(frame->bitmap()), hash);
}

static void testRandomFrameDecode(DecoderCreator createDecoder,
                                  SharedBuffer* fullData,
                                  size_t skippingStep) {
  Vector<unsigned> baselineHashes;
  createDecodingBaseline(createDecoder, fullData, &baselineHashes);
  size_t frameCount = baselineHashes.size();

  // Random decoding should get the same results as sequential decoding.
  std::unique_ptr<ImageDecoder> decoder = createDecoder();
  decoder->setData(fullData, true);
  for (size_t i = 0; i < skippingStep; ++i) {
    for (size_t j = i; j < frameCount; j += skippingStep) {
      SCOPED_TRACE(::testing::Message() << "Random i:" << i << " j:" << j);
      ImageFrame* frame = decoder->frameBufferAtIndex(j);
      EXPECT_EQ(baselineHashes[j], hashBitmap(frame->bitmap()));
    }
  }

  // Decoding in reverse order.
  decoder = createDecoder();
  decoder->setData(fullData, true);
  for (size_t i = frameCount; i; --i) {
    SCOPED_TRACE(::testing::Message() << "Reverse i:" << i);
    ImageFrame* frame = decoder->frameBufferAtIndex(i - 1);
    EXPECT_EQ(baselineHashes[i - 1], hashBitmap(frame->bitmap()));
  }
}

static void testRandomDecodeAfterClearFrameBufferCache(
    DecoderCreator createDecoder,
    SharedBuffer* data,
    size_t skippingStep) {
  Vector<unsigned> baselineHashes;
  createDecodingBaseline(createDecoder, data, &baselineHashes);
  size_t frameCount = baselineHashes.size();

  std::unique_ptr<ImageDecoder> decoder = createDecoder();
  decoder->setData(data, true);
  for (size_t clearExceptFrame = 0; clearExceptFrame < frameCount;
       ++clearExceptFrame) {
    decoder->clearCacheExceptFrame(clearExceptFrame);
    for (size_t i = 0; i < skippingStep; ++i) {
      for (size_t j = 0; j < frameCount; j += skippingStep) {
        SCOPED_TRACE(::testing::Message() << "Random i:" << i << " j:" << j);
        ImageFrame* frame = decoder->frameBufferAtIndex(j);
        EXPECT_EQ(baselineHashes[j], hashBitmap(frame->bitmap()));
      }
    }
  }
}

static void testDecodeAfterReallocatingData(DecoderCreator createDecoder,
                                            SharedBuffer* data) {
  std::unique_ptr<ImageDecoder> decoder = createDecoder();

  // Parse from 'data'.
  decoder->setData(data, true);
  size_t frameCount = decoder->frameCount();

  // ... and then decode frames from 'reallocatedData'.
  RefPtr<SharedBuffer> reallocatedData = data->copy();
  ASSERT_TRUE(reallocatedData.get());
  data->clear();
  decoder->setData(reallocatedData.get(), true);

  for (size_t i = 0; i < frameCount; ++i) {
    const ImageFrame* const frame = decoder->frameBufferAtIndex(i);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->getStatus());
  }
}

static void testByteByByteSizeAvailable(DecoderCreator createDecoder,
                                        SharedBuffer* data,
                                        size_t frameOffset,
                                        bool hasColorSpace,
                                        int expectedRepetitionCount) {
  std::unique_ptr<ImageDecoder> decoder = createDecoder();
  EXPECT_LT(frameOffset, data->size());

  // Send data to the decoder byte-by-byte and use the provided frame offset in
  // the data to check that isSizeAvailable() changes state only when that
  // offset is reached. Also check other decoder state.
  for (size_t length = 1; length <= frameOffset; ++length) {
    RefPtr<SharedBuffer> tempData = SharedBuffer::create(data->data(), length);
    decoder->setData(tempData.get(), false);

    if (length < frameOffset) {
      EXPECT_FALSE(decoder->isSizeAvailable());
      EXPECT_TRUE(decoder->size().isEmpty());
      EXPECT_FALSE(decoder->hasEmbeddedColorSpace());
      EXPECT_EQ(0u, decoder->frameCount());
      EXPECT_EQ(cAnimationLoopOnce, decoder->repetitionCount());
      EXPECT_FALSE(decoder->frameBufferAtIndex(0));
    } else {
      EXPECT_TRUE(decoder->isSizeAvailable());
      EXPECT_FALSE(decoder->size().isEmpty());
      EXPECT_EQ(decoder->hasEmbeddedColorSpace(), hasColorSpace);
      EXPECT_EQ(1u, decoder->frameCount());
      EXPECT_EQ(expectedRepetitionCount, decoder->repetitionCount());
    }

    ASSERT_FALSE(decoder->failed());
  }
}

static void testProgressiveDecoding(DecoderCreator createDecoder,
                                    SharedBuffer* fullData,
                                    size_t increment) {
  const size_t fullLength = fullData->size();

  std::unique_ptr<ImageDecoder> decoder;

  Vector<unsigned> truncatedHashes;
  Vector<unsigned> progressiveHashes;

  // Compute hashes when the file is truncated.
  for (size_t i = 1; i <= fullLength; i += increment) {
    decoder = createDecoder();
    RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), i);
    decoder->setData(data.get(), i == fullLength);
    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    if (!frame) {
      truncatedHashes.push_back(0);
      continue;
    }
    truncatedHashes.push_back(hashBitmap(frame->bitmap()));
  }

  // Compute hashes when the file is progressively decoded.
  decoder = createDecoder();
  for (size_t i = 1; i <= fullLength; i += increment) {
    RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), i);
    decoder->setData(data.get(), i == fullLength);
    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    if (!frame) {
      progressiveHashes.push_back(0);
      continue;
    }
    progressiveHashes.push_back(hashBitmap(frame->bitmap()));
  }

  for (size_t i = 0; i < truncatedHashes.size(); ++i)
    ASSERT_EQ(truncatedHashes[i], progressiveHashes[i]);
}

void testUpdateRequiredPreviousFrameAfterFirstDecode(
    DecoderCreator createDecoder,
    SharedBuffer* fullData) {
  std::unique_ptr<ImageDecoder> decoder = createDecoder();

  // Give it data that is enough to parse but not decode in order to check the
  // status of requiredPreviousFrameIndex before decoding.
  size_t partialSize = 1;
  do {
    RefPtr<SharedBuffer> data =
        SharedBuffer::create(fullData->data(), partialSize);
    decoder->setData(data.get(), false);
    ++partialSize;
  } while (!decoder->frameCount() ||
           decoder->frameBufferAtIndex(0)->getStatus() ==
               ImageFrame::FrameEmpty);

  EXPECT_EQ(kNotFound,
            decoder->frameBufferAtIndex(0)->requiredPreviousFrameIndex());
  unsigned frameCount = decoder->frameCount();
  for (size_t i = 1; i < frameCount; ++i) {
    EXPECT_EQ(i - 1,
              decoder->frameBufferAtIndex(i)->requiredPreviousFrameIndex());
  }

  decoder->setData(fullData, true);
  for (size_t i = 0; i < frameCount; ++i) {
    EXPECT_EQ(kNotFound,
              decoder->frameBufferAtIndex(i)->requiredPreviousFrameIndex());
  }
}

void testResumePartialDecodeAfterClearFrameBufferCache(
    DecoderCreator createDecoder,
    SharedBuffer* fullData) {
  Vector<unsigned> baselineHashes;
  createDecodingBaseline(createDecoder, fullData, &baselineHashes);
  size_t frameCount = baselineHashes.size();

  std::unique_ptr<ImageDecoder> decoder = createDecoder();

  // Let frame 0 be partially decoded.
  size_t partialSize = 1;
  do {
    RefPtr<SharedBuffer> data =
        SharedBuffer::create(fullData->data(), partialSize);
    decoder->setData(data.get(), false);
    ++partialSize;
  } while (!decoder->frameCount() ||
           decoder->frameBufferAtIndex(0)->getStatus() ==
               ImageFrame::FrameEmpty);

  // Skip to the last frame and clear.
  decoder->setData(fullData, true);
  EXPECT_EQ(frameCount, decoder->frameCount());
  ImageFrame* lastFrame = decoder->frameBufferAtIndex(frameCount - 1);
  EXPECT_EQ(baselineHashes[frameCount - 1], hashBitmap(lastFrame->bitmap()));
  decoder->clearCacheExceptFrame(kNotFound);

  // Resume decoding of the first frame.
  ImageFrame* firstFrame = decoder->frameBufferAtIndex(0);
  EXPECT_EQ(ImageFrame::FrameComplete, firstFrame->getStatus());
  EXPECT_EQ(baselineHashes[0], hashBitmap(firstFrame->bitmap()));
}

void testByteByByteDecode(DecoderCreator createDecoder,
                          const char* file,
                          size_t expectedFrameCount,
                          int expectedRepetitionCount) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());
  testByteByByteDecode(createDecoder, data.get(), expectedFrameCount,
                       expectedRepetitionCount);
}
void testByteByByteDecode(DecoderCreator createDecoder,
                          const char* dir,
                          const char* file,
                          size_t expectedFrameCount,
                          int expectedRepetitionCount) {
  RefPtr<SharedBuffer> data = readFile(dir, file);
  ASSERT_TRUE(data.get());
  testByteByByteDecode(createDecoder, data.get(), expectedFrameCount,
                       expectedRepetitionCount);
}

void testMergeBuffer(DecoderCreator createDecoder, const char* file) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());
  testMergeBuffer(createDecoder, data.get());
}

void testMergeBuffer(DecoderCreator createDecoder,
                     const char* dir,
                     const char* file) {
  RefPtr<SharedBuffer> data = readFile(dir, file);
  ASSERT_TRUE(data.get());
  testMergeBuffer(createDecoder, data.get());
}

void testRandomFrameDecode(DecoderCreator createDecoder,
                           const char* file,
                           size_t skippingStep) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());
  SCOPED_TRACE(file);
  testRandomFrameDecode(createDecoder, data.get(), skippingStep);
}
void testRandomFrameDecode(DecoderCreator createDecoder,
                           const char* dir,
                           const char* file,
                           size_t skippingStep) {
  RefPtr<SharedBuffer> data = readFile(dir, file);
  ASSERT_TRUE(data.get());
  SCOPED_TRACE(file);
  testRandomFrameDecode(createDecoder, data.get(), skippingStep);
}

void testRandomDecodeAfterClearFrameBufferCache(DecoderCreator createDecoder,
                                                const char* file,
                                                size_t skippingStep) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());
  SCOPED_TRACE(file);
  testRandomDecodeAfterClearFrameBufferCache(createDecoder, data.get(),
                                             skippingStep);
}

void testRandomDecodeAfterClearFrameBufferCache(DecoderCreator createDecoder,
                                                const char* dir,
                                                const char* file,
                                                size_t skippingStep) {
  RefPtr<SharedBuffer> data = readFile(dir, file);
  ASSERT_TRUE(data.get());
  SCOPED_TRACE(file);
  testRandomDecodeAfterClearFrameBufferCache(createDecoder, data.get(),
                                             skippingStep);
}

void testDecodeAfterReallocatingData(DecoderCreator createDecoder,
                                     const char* file) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());
  testDecodeAfterReallocatingData(createDecoder, data.get());
}

void testDecodeAfterReallocatingData(DecoderCreator createDecoder,
                                     const char* dir,
                                     const char* file) {
  RefPtr<SharedBuffer> data = readFile(dir, file);
  ASSERT_TRUE(data.get());
  testDecodeAfterReallocatingData(createDecoder, data.get());
}

void testByteByByteSizeAvailable(DecoderCreator createDecoder,
                                 const char* file,
                                 size_t frameOffset,
                                 bool hasColorSpace,
                                 int expectedRepetitionCount) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());
  testByteByByteSizeAvailable(createDecoder, data.get(), frameOffset,
                              hasColorSpace, expectedRepetitionCount);
}

void testByteByByteSizeAvailable(DecoderCreator createDecoder,
                                 const char* dir,
                                 const char* file,
                                 size_t frameOffset,
                                 bool hasColorSpace,
                                 int expectedRepetitionCount) {
  RefPtr<SharedBuffer> data = readFile(dir, file);
  ASSERT_TRUE(data.get());
  testByteByByteSizeAvailable(createDecoder, data.get(), frameOffset,
                              hasColorSpace, expectedRepetitionCount);
}

void testProgressiveDecoding(DecoderCreator createDecoder,
                             const char* file,
                             size_t increment) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());
  testProgressiveDecoding(createDecoder, data.get(), increment);
}

void testProgressiveDecoding(DecoderCreator createDecoder,
                             const char* dir,
                             const char* file,
                             size_t increment) {
  RefPtr<SharedBuffer> data = readFile(dir, file);
  ASSERT_TRUE(data.get());
  testProgressiveDecoding(createDecoder, data.get(), increment);
}

void testUpdateRequiredPreviousFrameAfterFirstDecode(
    DecoderCreator createDecoder,
    const char* dir,
    const char* file) {
  RefPtr<SharedBuffer> data = readFile(dir, file);
  ASSERT_TRUE(data.get());
  testUpdateRequiredPreviousFrameAfterFirstDecode(createDecoder, data.get());
}

void testUpdateRequiredPreviousFrameAfterFirstDecode(
    DecoderCreator createDecoder,
    const char* file) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());
  testUpdateRequiredPreviousFrameAfterFirstDecode(createDecoder, data.get());
}

void testResumePartialDecodeAfterClearFrameBufferCache(
    DecoderCreator createDecoder,
    const char* dir,
    const char* file) {
  RefPtr<SharedBuffer> data = readFile(dir, file);
  ASSERT_TRUE(data.get());
  testResumePartialDecodeAfterClearFrameBufferCache(createDecoder, data.get());
}

void testResumePartialDecodeAfterClearFrameBufferCache(
    DecoderCreator createDecoder,
    const char* file) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());
  testResumePartialDecodeAfterClearFrameBufferCache(createDecoder, data.get());
}

static uint32_t premultiplyColor(uint32_t c) {
  return SkPremultiplyARGBInline(SkGetPackedA32(c), SkGetPackedR32(c),
                                 SkGetPackedG32(c), SkGetPackedB32(c));
}

static void verifyFramesMatch(const char* file,
                              const ImageFrame* const a,
                              const ImageFrame* const b) {
  const SkBitmap& bitmapA = a->bitmap();
  const SkBitmap& bitmapB = b->bitmap();
  ASSERT_EQ(bitmapA.width(), bitmapB.width());
  ASSERT_EQ(bitmapA.height(), bitmapB.height());

  int maxDifference = 0;
  for (int y = 0; y < bitmapA.height(); ++y) {
    for (int x = 0; x < bitmapA.width(); ++x) {
      uint32_t colorA = *bitmapA.getAddr32(x, y);
      if (!a->premultiplyAlpha())
        colorA = premultiplyColor(colorA);
      uint32_t colorB = *bitmapB.getAddr32(x, y);
      if (!b->premultiplyAlpha())
        colorB = premultiplyColor(colorB);
      uint8_t* pixelA = reinterpret_cast<uint8_t*>(&colorA);
      uint8_t* pixelB = reinterpret_cast<uint8_t*>(&colorB);
      for (int channel = 0; channel < 4; ++channel) {
        const int difference = abs(pixelA[channel] - pixelB[channel]);
        if (difference > maxDifference)
          maxDifference = difference;
      }
    }
  }

  // Pre-multiplication could round the RGBA channel values. So, we declare
  // that the frames match if the RGBA channel values differ by at most 2.
  EXPECT_GE(2, maxDifference) << file;
}

// Verifies that result of alpha blending is similar for AlphaPremultiplied and
// AlphaNotPremultiplied cases.
void testAlphaBlending(DecoderCreatorWithAlpha createDecoder,
                       const char* file) {
  RefPtr<SharedBuffer> data = readFile(file);
  ASSERT_TRUE(data.get());

  std::unique_ptr<ImageDecoder> decoderA =
      createDecoder(ImageDecoder::AlphaPremultiplied);
  decoderA->setData(data.get(), true);

  std::unique_ptr<ImageDecoder> decoderB =
      createDecoder(ImageDecoder::AlphaNotPremultiplied);
  decoderB->setData(data.get(), true);

  size_t frameCount = decoderA->frameCount();
  ASSERT_EQ(frameCount, decoderB->frameCount());

  for (size_t i = 0; i < frameCount; ++i) {
    verifyFramesMatch(file, decoderA->frameBufferAtIndex(i),
                      decoderB->frameBufferAtIndex(i));
  }
}

}  // namespace blink
