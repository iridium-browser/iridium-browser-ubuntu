// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_TEST_SUITE_H_
#define CONTENT_TEST_CONTENT_TEST_SUITE_H_

#include "base/compiler_specific.h"
#include "content/public/test/content_test_suite_base.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

#if defined(USE_OZONE)
namespace ui {
class ClientNativePixmapFactory;
}  // namespace ui
#endif

namespace content {

class ContentTestSuite : public ContentTestSuiteBase {
 public:
  ContentTestSuite(int argc, char** argv);
  ~ContentTestSuite() override;

 protected:
  void Initialize() override;

 private:
#if defined(OS_WIN)
  base::win::ScopedCOMInitializer com_initializer_;
#endif
#if defined(USE_OZONE)
  scoped_ptr<ui::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ContentTestSuite);
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_TEST_SUITE_H_
