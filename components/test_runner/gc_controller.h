// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_GC_CONTROLLER_H_
#define COMPONENTS_TEST_RUNNER_GC_CONTROLLER_H_

#include "base/macros.h"
#include "gin/wrappable.h"

namespace blink {
class WebFrame;
}

namespace gin {
class Arguments;
}

namespace test_runner {

class GCController : public gin::Wrappable<GCController> {
 public:
  static gin::WrapperInfo kWrapperInfo;
  static void Install(blink::WebFrame* frame);

 private:
  GCController();
  ~GCController() override;

  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  void Collect(const gin::Arguments& args);
  void CollectAll(const gin::Arguments& args);
  void MinorCollect(const gin::Arguments& args);

  DISALLOW_COPY_AND_ASSIGN(GCController);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_GC_CONTROLLER_H_
