/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/ImageDecodingStore.h"

#include "platform/graphics/ImageFrameGenerator.h"
#include "platform/graphics/test/MockImageDecoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class ImageDecodingStoreTest : public ::testing::Test,
                               public MockImageDecoderClient {
 public:
  void SetUp() override {
    ImageDecodingStore::instance().setCacheLimitInBytes(1024 * 1024);
    m_generator = ImageFrameGenerator::create(SkISize::Make(100, 100), true,
                                              ColorBehavior::ignore());
    m_decodersDestroyed = 0;
  }

  void TearDown() override { ImageDecodingStore::instance().clear(); }

  void decoderBeingDestroyed() override { ++m_decodersDestroyed; }

  void decodeRequested() override {
    // Decoder is never used by ImageDecodingStore.
    ASSERT_TRUE(false);
  }

  ImageFrame::Status status() override { return ImageFrame::FramePartial; }

  size_t frameCount() override { return 1; }
  int repetitionCount() const override { return cAnimationNone; }
  float frameDuration() const override { return 0; }

 protected:
  void evictOneCache() {
    size_t memoryUsageInBytes =
        ImageDecodingStore::instance().memoryUsageInBytes();
    if (memoryUsageInBytes)
      ImageDecodingStore::instance().setCacheLimitInBytes(memoryUsageInBytes -
                                                          1);
    else
      ImageDecodingStore::instance().setCacheLimitInBytes(0);
  }

  RefPtr<ImageFrameGenerator> m_generator;
  int m_decodersDestroyed;
};

TEST_F(ImageDecodingStoreTest, insertDecoder) {
  const SkISize size = SkISize::Make(1, 1);
  std::unique_ptr<ImageDecoder> decoder = MockImageDecoder::create(this);
  decoder->setSize(1, 1);
  const ImageDecoder* refDecoder = decoder.get();
  ImageDecodingStore::instance().insertDecoder(m_generator.get(),
                                               std::move(decoder));
  EXPECT_EQ(1, ImageDecodingStore::instance().cacheEntries());
  EXPECT_EQ(4u, ImageDecodingStore::instance().memoryUsageInBytes());

  ImageDecoder* testDecoder;
  EXPECT_TRUE(ImageDecodingStore::instance().lockDecoder(m_generator.get(),
                                                         size, &testDecoder));
  EXPECT_TRUE(testDecoder);
  EXPECT_EQ(refDecoder, testDecoder);
  ImageDecodingStore::instance().unlockDecoder(m_generator.get(), testDecoder);
  EXPECT_EQ(1, ImageDecodingStore::instance().cacheEntries());
}

TEST_F(ImageDecodingStoreTest, evictDecoder) {
  std::unique_ptr<ImageDecoder> decoder1 = MockImageDecoder::create(this);
  std::unique_ptr<ImageDecoder> decoder2 = MockImageDecoder::create(this);
  std::unique_ptr<ImageDecoder> decoder3 = MockImageDecoder::create(this);
  decoder1->setSize(1, 1);
  decoder2->setSize(2, 2);
  decoder3->setSize(3, 3);
  ImageDecodingStore::instance().insertDecoder(m_generator.get(),
                                               std::move(decoder1));
  ImageDecodingStore::instance().insertDecoder(m_generator.get(),
                                               std::move(decoder2));
  ImageDecodingStore::instance().insertDecoder(m_generator.get(),
                                               std::move(decoder3));
  EXPECT_EQ(3, ImageDecodingStore::instance().cacheEntries());
  EXPECT_EQ(56u, ImageDecodingStore::instance().memoryUsageInBytes());

  evictOneCache();
  EXPECT_EQ(2, ImageDecodingStore::instance().cacheEntries());
  EXPECT_EQ(52u, ImageDecodingStore::instance().memoryUsageInBytes());

  evictOneCache();
  EXPECT_EQ(1, ImageDecodingStore::instance().cacheEntries());
  EXPECT_EQ(36u, ImageDecodingStore::instance().memoryUsageInBytes());

  evictOneCache();
  EXPECT_FALSE(ImageDecodingStore::instance().cacheEntries());
  EXPECT_FALSE(ImageDecodingStore::instance().memoryUsageInBytes());
}

TEST_F(ImageDecodingStoreTest, decoderInUseNotEvicted) {
  std::unique_ptr<ImageDecoder> decoder1 = MockImageDecoder::create(this);
  std::unique_ptr<ImageDecoder> decoder2 = MockImageDecoder::create(this);
  std::unique_ptr<ImageDecoder> decoder3 = MockImageDecoder::create(this);
  decoder1->setSize(1, 1);
  decoder2->setSize(2, 2);
  decoder3->setSize(3, 3);
  ImageDecodingStore::instance().insertDecoder(m_generator.get(),
                                               std::move(decoder1));
  ImageDecodingStore::instance().insertDecoder(m_generator.get(),
                                               std::move(decoder2));
  ImageDecodingStore::instance().insertDecoder(m_generator.get(),
                                               std::move(decoder3));
  EXPECT_EQ(3, ImageDecodingStore::instance().cacheEntries());

  ImageDecoder* testDecoder;
  EXPECT_TRUE(ImageDecodingStore::instance().lockDecoder(
      m_generator.get(), SkISize::Make(2, 2), &testDecoder));

  evictOneCache();
  evictOneCache();
  evictOneCache();
  EXPECT_EQ(1, ImageDecodingStore::instance().cacheEntries());
  EXPECT_EQ(16u, ImageDecodingStore::instance().memoryUsageInBytes());

  ImageDecodingStore::instance().unlockDecoder(m_generator.get(), testDecoder);
  evictOneCache();
  EXPECT_FALSE(ImageDecodingStore::instance().cacheEntries());
  EXPECT_FALSE(ImageDecodingStore::instance().memoryUsageInBytes());
}

TEST_F(ImageDecodingStoreTest, removeDecoder) {
  const SkISize size = SkISize::Make(1, 1);
  std::unique_ptr<ImageDecoder> decoder = MockImageDecoder::create(this);
  decoder->setSize(1, 1);
  const ImageDecoder* refDecoder = decoder.get();
  ImageDecodingStore::instance().insertDecoder(m_generator.get(),
                                               std::move(decoder));
  EXPECT_EQ(1, ImageDecodingStore::instance().cacheEntries());
  EXPECT_EQ(4u, ImageDecodingStore::instance().memoryUsageInBytes());

  ImageDecoder* testDecoder;
  EXPECT_TRUE(ImageDecodingStore::instance().lockDecoder(m_generator.get(),
                                                         size, &testDecoder));
  EXPECT_TRUE(testDecoder);
  EXPECT_EQ(refDecoder, testDecoder);
  ImageDecodingStore::instance().removeDecoder(m_generator.get(), testDecoder);
  EXPECT_FALSE(ImageDecodingStore::instance().cacheEntries());

  EXPECT_FALSE(ImageDecodingStore::instance().lockDecoder(m_generator.get(),
                                                          size, &testDecoder));
}

}  // namespace blink
