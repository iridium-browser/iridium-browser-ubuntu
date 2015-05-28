// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_SETUP_TEST_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_SETUP_TEST_H_

#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// A very simple test that just sets up a new plugin.
class PluginSetupTest : public PluginTest {
 public:
  // Constructor.
  PluginSetupTest(NPP id, NPNetscapeFuncs *host_functions);

  // NPAPI SetWindow handler.
  NPError SetWindow(NPWindow* pNPWindow) override;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_SETUP_TEST_H_
