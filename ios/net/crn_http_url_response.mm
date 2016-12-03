// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/crn_http_url_response.h"

#include "base/mac/scoped_nsobject.h"

@interface CRNHTTPURLResponse () {
  base::scoped_nsobject<NSString> _cr_HTTPVersion;
}
@end

@implementation CRNHTTPURLResponse

- (NSString*)cr_HTTPVersion {
  return _cr_HTTPVersion;
}

- (instancetype)initWithURL:(NSURL*)url
                 statusCode:(NSInteger)statusCode
                HTTPVersion:(NSString*)HTTPVersion
               headerFields:(NSDictionary*)headerFields {
  self = [super initWithURL:url
                 statusCode:statusCode
                HTTPVersion:HTTPVersion
               headerFields:headerFields];
  if (self) {
    _cr_HTTPVersion.reset([HTTPVersion copy]);
  }
  return self;
}

@end
