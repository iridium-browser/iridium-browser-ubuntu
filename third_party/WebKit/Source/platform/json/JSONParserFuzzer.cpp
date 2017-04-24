// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/json/JSONParser.h"

#include "platform/json/JSONValues.h"
#include "platform/testing/BlinkFuzzerTestSupport.h"
#include "wtf/text/WTFString.h"
#include <stddef.h>
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  blink::parseJSON(WTF::String(data, size), 500);
  return 0;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  blink::InitializeBlinkFuzzTest(argc, argv);
  return 0;
}
