// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
#define EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_

namespace net {
class URLRequest;
}

namespace extensions {
class Extension;
class InfoMap;

// Utilities related to URLRequest jobs for extension resources. See
// chrome/browser/extensions/extension_protocols_unittest.cc for related tests.
namespace url_request_util {

// Sets allowed=true to allow a chrome-extension:// resource request coming from
// renderer A to access a resource in an extension running in renderer B.
// Returns false when it couldn't determine if the resource is allowed or not
bool AllowCrossRendererResourceLoad(net::URLRequest* request,
                                    bool is_incognito,
                                    const Extension* extension,
                                    InfoMap* extension_info_map,
                                    bool* allowed);

// Returns true if |request| corresponds to a resource request from a
// <webview>.
bool IsWebViewRequest(const net::URLRequest* request);

}  // namespace url_request_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
