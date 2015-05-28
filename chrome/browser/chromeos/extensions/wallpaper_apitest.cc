// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "net/dns/mock_host_resolver.h"

// Disabled due to flakiness. See http://crbug.com/468632.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Wallpaper) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("wallpaper")) << message_;
}
