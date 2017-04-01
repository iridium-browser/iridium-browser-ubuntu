// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebURLLoaderTestDelegate_h
#define WebURLLoaderTestDelegate_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebURLResponse;
class WebURLLoaderClient;
struct WebURLError;

// Use with WebUnitTestSupport::setLoaderDelegate to intercept calls to a
// WebURLLoaderClient for controlling network responses in a test. Default
// implementations of all methods just call the original method on the
// WebURLLoaderClient.
class BLINK_PLATFORM_EXPORT WebURLLoaderTestDelegate {
 public:
  WebURLLoaderTestDelegate();
  virtual ~WebURLLoaderTestDelegate();

  virtual void didReceiveResponse(WebURLLoaderClient* originalClient,
                                  const WebURLResponse&);
  virtual void didReceiveData(WebURLLoaderClient* originalClient,
                              const char* data,
                              int dataLength);
  virtual void didFail(WebURLLoaderClient* originalClient,
                       const WebURLError&,
                       int64_t totalEncodedDataLength,
                       int64_t totalEncodedBodyLength);
  virtual void didFinishLoading(WebURLLoaderClient* originalClient,
                                double finishTime,
                                int64_t totalEncodedDataLength,
                                int64_t totalEncodedBodyLength);
};

}  // namespace blink

#endif
