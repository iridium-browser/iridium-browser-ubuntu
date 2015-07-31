// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UTILS_H_
#define COMPONENTS_UPDATE_CLIENT_UTILS_H_

#include <string>
#include <base/memory/scoped_ptr.h>

class GURL;

namespace base {
class FilePath;
}

namespace net {
class URLFetcher;
class URLFetcherDelegate;
class URLRequestContextGetter;
}

namespace update_client {

class Configurator;
struct CrxComponent;
struct CrxUpdateItem;

// An update protocol request starts with a common preamble which includes
// version and platform information for Chrome and the operating system,
// followed by a request body, which is the actual payload of the request.
// For example:
//
// <?xml version="1.0" encoding="UTF-8"?>
// <request protocol="3.0" version="chrome-32.0.1.0"  prodversion="32.0.1.0"
//        requestid="{7383396D-B4DD-46E1-9104-AAC6B918E792}"
//        updaterchannel="canary" arch="x86" nacl_arch="x86-64"
//        ADDITIONAL ATTRIBUTES>
//   <hw physmemory="16"/>
//   <os platform="win" version="6.1" arch="x86"/>
//   ... REQUEST BODY ...
// </request>

// Builds a protocol request string by creating the outer envelope for
// the request and including the request body specified as a parameter.
// If specified, |additional_attributes| are appended as attributes of the
// request element. The additional attributes have to be well-formed for
// insertion in the request element.
std::string BuildProtocolRequest(const std::string& browser_version,
                                 const std::string& channel,
                                 const std::string& lang,
                                 const std::string& os_long_name,
                                 const std::string& request_body,
                                 const std::string& additional_attributes);

// Sends a protocol request to the the service endpoint specified by |url|.
// The body of the request is provided by |protocol_request| and it is
// expected to contain XML data. The caller owns the returned object.
scoped_ptr<net::URLFetcher> SendProtocolRequest(
    const GURL& url,
    const std::string& protocol_request,
    net::URLFetcherDelegate* url_fetcher_delegate,
    net::URLRequestContextGetter* url_request_context_getter);

// Returns true if the url request of |fetcher| was succesful.
bool FetchSuccess(const net::URLFetcher& fetcher);

// Returns the error code which occured during the fetch. The function returns 0
// if the fetch was successful. If errors happen, the function could return a
// network error, an http response code, or the status of the fetch, if the
// fetch is pending or canceled.
int GetFetchError(const net::URLFetcher& fetcher);

// Returns true if the |update_item| contains a valid differential update url.
bool HasDiffUpdate(const CrxUpdateItem* update_item);

// Returns true if the |status_code| represents a server error 5xx.
bool IsHttpServerError(int status_code);

// Deletes the file and its directory, if the directory is empty. If the
// parent directory is not empty, the function ignores deleting the directory.
// Returns true if the file and the empty directory are deleted.
bool DeleteFileAndEmptyParentDirectory(const base::FilePath& filepath);

// Returns the component id of the |component|. The component id is in a
// format similar with the format of an extension id.
std::string GetCrxComponentID(const CrxComponent& component);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UTILS_H_
