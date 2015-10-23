// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace blink {
class ImageDecoder;
class SharedBuffer;

using DecoderCreator = PassOwnPtr<ImageDecoder>(*)();
PassRefPtr<SharedBuffer> readFile(const char* fileName);
PassRefPtr<SharedBuffer> readFile(const char* dir, const char* fileName);
unsigned hashBitmap(const SkBitmap&);
void createDecodingBaseline(DecoderCreator, SharedBuffer*, Vector<unsigned>* baselineHashes);
void testByteByByteDecode(DecoderCreator createDecoder, const char* file, size_t expectedFrameCount, int expectedRepetitionCount);
}
