/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/testing/URLTestHelpers.h"

#include <string>
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLLoadTiming.h"
#include "public/platform/WebURLLoaderMockFactory.h"

namespace blink {
namespace URLTestHelpers {

WebURL registerMockedURLLoadFromBase(const WebString& baseURL,
                                     const WebString& basePath,
                                     const WebString& fileName,
                                     const WebString& mimeType) {
  // fullURL = baseURL + fileName.
  std::string fullURL =
      std::string(baseURL.utf8().data()) + std::string(fileName.utf8().data());

  // filePath = basePath + ("/" +) fileName.
  base::FilePath filePath =
      WebStringToFilePath(basePath).Append(WebStringToFilePath(fileName));

  KURL url = toKURL(fullURL);
  registerMockedURLLoad(url, FilePathToWebString(filePath), mimeType);
  return WebURL(url);
}

void registerMockedURLLoad(const WebURL& fullURL,
                           const WebString& filePath,
                           const WebString& mimeType) {
  WebURLLoadTiming timing;
  timing.initialize();

  WebURLResponse response(fullURL);
  response.setMIMEType(mimeType);
  response.setHTTPStatusCode(200);
  response.setLoadTiming(timing);

  registerMockedURLLoadWithCustomResponse(fullURL, filePath, response);
}

void registerMockedErrorURLLoad(const WebURL& fullURL) {
  WebURLLoadTiming timing;
  timing.initialize();

  WebURLResponse response;
  response.setMIMEType("image/png");
  response.setHTTPStatusCode(404);
  response.setLoadTiming(timing);

  WebURLError error;
  error.reason = 404;
  Platform::current()->getURLLoaderMockFactory()->registerErrorURL(
      fullURL, response, error);
}

void registerMockedURLLoadWithCustomResponse(const WebURL& fullURL,
                                             const WebString& filePath,
                                             WebURLResponse response) {
  Platform::current()->getURLLoaderMockFactory()->registerURL(fullURL, response,
                                                              filePath);
}

}  // namespace URLTestHelpers
}  // namespace blink
