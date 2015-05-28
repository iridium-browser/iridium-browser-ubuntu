// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/plugin/plugin_get_javascript_url2_test.h"

#include "base/basictypes.h"

// url for "self".
#define SELF_URL "javascript:window.location+\"\""
// The identifier for the self url stream.
#define SELF_URL_STREAM_ID 1

// The maximum chunk size of stream data.
#define STREAM_CHUNK 197

namespace NPAPIClient {

ExecuteGetJavascriptUrl2Test::ExecuteGetJavascriptUrl2Test(
    NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      test_started_(false) {
}

NPError ExecuteGetJavascriptUrl2Test::SetWindow(NPWindow* pNPWindow) {
#if !defined(OS_MACOSX)
  if (pNPWindow->window == NULL)
    return NPERR_NO_ERROR;
#endif

  if (!test_started_) {
    std::string url = SELF_URL;
    HostFunctions()->geturlnotify(id(), url.c_str(), "_self",
                                  reinterpret_cast<void*>(SELF_URL_STREAM_ID));
    test_started_ = true;
  }
  return NPERR_NO_ERROR;
}

NPError ExecuteGetJavascriptUrl2Test::NewStream(NPMIMEType type, NPStream* stream,
                              NPBool seekable, uint16* stype) {
  if (stream == NULL) {
    SetError("NewStream got null stream");
    return NPERR_INVALID_PARAM;
  }

  static_assert(sizeof(unsigned long) <= sizeof(stream->notifyData),
                "cast validity check");
  unsigned long stream_id = reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

int32 ExecuteGetJavascriptUrl2Test::WriteReady(NPStream *stream) {
  return STREAM_CHUNK;
}

int32 ExecuteGetJavascriptUrl2Test::Write(NPStream *stream, int32 offset, int32 len,
                              void *buffer) {
  if (stream == NULL) {
    SetError("Write got null stream");
    return -1;
  }
  if (len < 0 || len > STREAM_CHUNK) {
    SetError("Write got bogus stream chunk size");
    return -1;
  }

  static_assert(sizeof(unsigned long) <= sizeof(stream->notifyData),
                "cast validity check");
  unsigned long stream_id = reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      self_url_.append(static_cast<char*>(buffer), len);
      break;
    default:
      SetError("Unexpected write callback");
      break;
  }
  // Pretend that we took all the data.
  return len;
}


NPError ExecuteGetJavascriptUrl2Test::DestroyStream(NPStream *stream, NPError reason) {
  if (stream == NULL) {
    SetError("NewStream got null stream");
    return NPERR_INVALID_PARAM;
  }

  static_assert(sizeof(unsigned long) <= sizeof(stream->notifyData),
                "cast validity check");
  unsigned long stream_id = reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      // don't care
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

void ExecuteGetJavascriptUrl2Test::URLNotify(const char* url, NPReason reason, void* data) {
  static_assert(sizeof(unsigned long) <= sizeof(data),
                "cast validity check");

  unsigned long stream_id = reinterpret_cast<unsigned long>(data);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      if (strcmp(url, SELF_URL) != 0)
        SetError("URLNotify reported incorrect url for SELF_URL");
      if (self_url_.empty())
        SetError("Failed to obtain window location.");
      SignalTestCompleted();
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
}

} // namespace NPAPIClient
