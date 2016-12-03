// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/nsurlrequest_util.h"

#include "base/strings/stringprintf.h"

namespace net {

std::string FormatUrlRequestForLogging(NSURLRequest* request) {
  NSString* urlAbsoluteString = request.URL.absoluteString;
  NSString* mainDocumentURLAbsoluteString =
      request.mainDocumentURL.absoluteString;
  return base::StringPrintf(
      "request: %s request.mainDocURL: %s",
      urlAbsoluteString ? urlAbsoluteString.UTF8String : "[nil]",
      mainDocumentURLAbsoluteString ?
          mainDocumentURLAbsoluteString.UTF8String : "[nil]");
}

}  // namespace net
