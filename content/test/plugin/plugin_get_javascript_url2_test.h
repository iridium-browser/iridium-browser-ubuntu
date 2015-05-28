// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_GET_JAVASCRIPT_URL2_TEST_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_GET_JAVASCRIPT_URL2_TEST_H_

#include "base/compiler_specific.h"
#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// This class tests NPP_GetURLNotify for a javascript URL with _top
// as the target frame.
class ExecuteGetJavascriptUrl2Test : public PluginTest {
 public:
  // Constructor.
  ExecuteGetJavascriptUrl2Test(NPP id, NPNetscapeFuncs *host_functions);

  //
  // NPAPI functions
  //
  NPError SetWindow(NPWindow* pNPWindow) override;
  NPError NewStream(NPMIMEType type,
                    NPStream* stream,
                    NPBool seekable,
                    uint16* stype) override;
  int32 WriteReady(NPStream* stream) override;
  int32 Write(NPStream* stream, int32 offset, int32 len, void* buffer) override;
  NPError DestroyStream(NPStream* stream, NPError reason) override;
  void URLNotify(const char* url, NPReason reason, void* data) override;

 private:
  bool test_started_;
  std::string self_url_;
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_GET_JAVASCRIPT_URL2_TEST_H_
