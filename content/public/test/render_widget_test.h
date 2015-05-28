// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_RENDER_WIDGET_TEST_H_
#define CONTENT_PUBLIC_TEST_RENDER_WIDGET_TEST_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "content/public/test/render_view_test.h"

class SkBitmap;
struct ViewMsg_Resize_Params;

namespace gfx {
class Size;
}

namespace content {
class RenderWidget;

class RenderWidgetTest : public RenderViewTest {
 public:
  RenderWidgetTest();

 protected:
  RenderWidget* widget();
  void OnResize(const ViewMsg_Resize_Params& params);
  bool next_paint_is_resize_ack();

  static const int kNumBytesPerPixel;
  static const int kLargeWidth;
  static const int kLargeHeight;
  static const int kSmallWidth;
  static const int kSmallHeight;
  static const int kTextPositionX;
  static const int kTextPositionY;
  static const uint32 kRedARGB;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_RENDER_WIDGET_TEST_H_
