// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PLUGIN_PLUGIN_GET_JAVASCRIPT_URL_TEST_H_
#define CONTENT_TEST_PLUGIN_PLUGIN_GET_JAVASCRIPT_URL_TEST_H_

#include "base/compiler_specific.h"
#include "content/test/plugin/plugin_test.h"

namespace NPAPIClient {

// This class tests NPP_GetURLNotify for a javascript URL with _top
// as the target frame.
class ExecuteGetJavascriptUrlTest : public PluginTest {
 public:
  // Constructor.
  ExecuteGetJavascriptUrlTest(NPP id, NPNetscapeFuncs *host_functions);
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
#if defined(OS_WIN)
  static void CALLBACK TimerProc(HWND window, UINT message, UINT_PTR timer_id,
                                 DWORD elapsed_time);
#endif
  bool test_started_;
  // This flag is set to true in the context of the NPN_Evaluate call.
  bool npn_evaluate_context_;
  std::string self_url_;

#if defined(OS_WIN)
  HWND window_;
#endif
};

}  // namespace NPAPIClient

#endif  // CONTENT_TEST_PLUGIN_PLUGIN_GET_JAVASCRIPT_URL_TEST_H_
