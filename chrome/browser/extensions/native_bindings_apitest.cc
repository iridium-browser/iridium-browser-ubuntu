// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/common/switches.h"

namespace extensions {

// And end-to-end test for extension APIs using native bindings.
class NativeBindingsApiTest : public ExtensionApiTest {
 public:
  NativeBindingsApiTest() {}
  ~NativeBindingsApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Note: We don't use a FeatureSwitch::ScopedOverride here because we need
    // the switch to be propogated to the renderer, which doesn't happen with
    // a ScopedOverride.
    command_line->AppendSwitchASCII(switches::kNativeCrxBindings, "1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeBindingsApiTest);
};

IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, SimpleEndToEndTest) {
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("native_bindings")) << message_;
}

}  // namespace extensions
