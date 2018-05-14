// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class FPDFRenderLoadImageEmbeddertest : public EmbedderTest {};

TEST_F(FPDFRenderLoadImageEmbeddertest, Bug_554151) {
  // Test scanline downsampling with a BitsPerComponent of 4.
  // Should not crash.
  EXPECT_TRUE(OpenDocument("bug_554151.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  std::unique_ptr<void, FPDFBitmapDeleter> bitmap = RenderLoadedPage(page);
  CompareBitmap(bitmap.get(), 612, 792, "a14d7ee573c1b2456d7bf6b7762823cf");
  UnloadPage(page);
}

TEST_F(FPDFRenderLoadImageEmbeddertest, Bug_557223) {
  // Should not crash
  EXPECT_TRUE(OpenDocument("bug_557223.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);
  std::unique_ptr<void, FPDFBitmapDeleter> bitmap = RenderLoadedPage(page);
  CompareBitmap(bitmap.get(), 24, 24, "dc0ea1b743c2edb22c597cadc8537f7b");
  UnloadPage(page);
}

TEST_F(FPDFRenderLoadImageEmbeddertest, Bug_603518) {
  // Should not crash
  EXPECT_TRUE(OpenDocument("bug_603518.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);
  std::unique_ptr<void, FPDFBitmapDeleter> bitmap = RenderLoadedPage(page);
  CompareBitmap(bitmap.get(), 749, 749, "b9e75190cdc5edf0069a408744ca07dc");
  UnloadPage(page);
}
