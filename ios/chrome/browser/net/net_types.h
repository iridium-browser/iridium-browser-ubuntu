// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_NET_TYPES_H_
#define IOS_CHROME_BROWSER_NET_NET_TYPES_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class URLRequestInterceptor;
}

// A mapping from the scheme name to the protocol handler that services its
// content.
typedef std::map<std::string,
                 linked_ptr<net::URLRequestJobFactory::ProtocolHandler>>
    ProtocolHandlerMap;

// A scoped vector of protocol interceptors.
typedef ScopedVector<net::URLRequestInterceptor>
    URLRequestInterceptorScopedVector;

#endif  // IOS_CHROME_BROWSER_NET_NET_TYPES_H_
